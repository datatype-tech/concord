// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHandles.h"
#include "CvmRuntime.h"

#include <windows.h>

#include <stdlib.h>

/**
 * Threads, mutexes and atomic counters.
 *
 * A CVM function is already \c i64() — the exact shape a thread entry point
 * needs — so \c thread_spawn takes the function's address rather than any kind
 * of closure. Sharing data between threads goes through a mutex or an atomic
 * counter; a container mutated from two threads without one is a data race,
 * exactly as it would be in C.
 */
typedef struct CvmThread {
    HANDLE handle;

    /**
     * The entry point as an address.
     *
     * Stored as an integer rather than a typed pointer because CVM has two
     * entry shapes: thread_spawn takes i64(), thread_spawn_arg takes i64(i64).
     * Which one applies is recorded alongside it.
     */
    intptr_t entry;

    /** Passed to an i64(i64) entry; ignored by an i64() entry. */
    int64_t argument;

    int64_t result;

    /** Non-zero when entry is i64(i64). */
    int32_t takesArgument;
} CvmThread;

typedef struct CvmMutex {
    CRITICAL_SECTION section;
    DWORD owner;
    int32_t held;
} CvmMutex;

typedef struct CvmCounter {
    int64_t value;
} CvmCounter;

/** Runs a spawned entry point and records what it returned. */
static DWORD WINAPI CvmThreadTrampoline(LPVOID parameter)
{
    CvmThread* thread = (CvmThread*)parameter;
    if (thread->takesArgument) {
        thread->result = ((int64_t (*)(int64_t))thread->entry)(thread->argument);
    } else {
        thread->result = ((int64_t (*)(void))thread->entry)();
    }
    return 0;
}

/** Starts \p entry on a new thread, remembering how it must be called. */
static int64_t StartThread(intptr_t entry, int64_t argument, int32_t takesArgument)
{
    if (entry == 0) return 0;
    CvmThread* thread = (CvmThread*)calloc(1, sizeof(CvmThread));
    if (thread == NULL) return 0;
    thread->entry = entry;
    thread->argument = argument;
    thread->takesArgument = takesArgument;
    thread->handle = CreateThread(NULL, 0, CvmThreadTrampoline, thread, 0, NULL);
    if (thread->handle == NULL) {
        free(thread);
        return 0;
    }
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_THREAD, thread);
    if (handle == 0) {
        WaitForSingleObject(thread->handle, INFINITE);
        CloseHandle(thread->handle);
        free(thread);
    }
    return handle;
}

int64_t cvm_thread_spawn(int64_t entry)
{
    return StartThread((intptr_t)entry, 0, 0);
}

int64_t cvm_thread_spawn_arg(int64_t entry, int64_t arg)
{
    return StartThread((intptr_t)entry, arg, 1);
}

int64_t cvm_thread_join(int64_t thread)
{
    CvmThread* found = (CvmThread*)CvmHandleAcquire(thread, CVM_HANDLE_THREAD);
    if (found == NULL) return 0;
    WaitForSingleObject(found->handle, INFINITE);
    const int64_t result = found->result;
    CvmThread* released = (CvmThread*)CvmHandleRetire(thread, CVM_HANDLE_THREAD);
    if (released != NULL) {
        CloseHandle(released->handle);
        free(released);
    }
    return result;
}

int64_t cvm_thread_yield(void)
{
    SwitchToThread();
    return 0;
}

int64_t cvm_thread_count(void)
{
    SYSTEM_INFO information;
    GetSystemInfo(&information);
    return (int64_t)information.dwNumberOfProcessors;
}

int64_t cvm_mutex_new(void)
{
    CvmMutex* mutex = (CvmMutex*)calloc(1, sizeof(CvmMutex));
    if (mutex == NULL) return 0;
    InitializeCriticalSection(&mutex->section);
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_MUTEX, mutex);
    if (handle == 0) {
        DeleteCriticalSection(&mutex->section);
        free(mutex);
    }
    return handle;
}

int64_t cvm_mutex_free(int64_t mutex)
{
    CvmMutex* released = (CvmMutex*)CvmHandleRetire(mutex, CVM_HANDLE_MUTEX);
    if (released == NULL) return 0;
    DeleteCriticalSection(&released->section);
    free(released);
    return 1;
}

int64_t cvm_mutex_lock(int64_t mutex)
{
    CvmMutex* found = (CvmMutex*)CvmHandleAcquire(mutex, CVM_HANDLE_MUTEX);
    if (found == NULL) return 0;
    EnterCriticalSection(&found->section);
    found->owner = GetCurrentThreadId();
    found->held = 1;
    return 1;
}

int64_t cvm_mutex_unlock(int64_t mutex)
{
    CvmMutex* found = (CvmMutex*)CvmHandleAcquire(mutex, CVM_HANDLE_MUTEX);
    if (found == NULL || !found->held || found->owner != GetCurrentThreadId()) return 0;
    found->held = 0;
    LeaveCriticalSection(&found->section);
    return 1;
}

int64_t cvm_counter_new(int64_t initial)
{
    CvmCounter* counter = (CvmCounter*)calloc(1, sizeof(CvmCounter));
    if (counter == NULL) return 0;
    counter->value = initial;
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_COUNTER, counter);
    if (handle == 0) free(counter);
    return handle;
}

int64_t cvm_counter_free(int64_t counter)
{
    CvmCounter* released = (CvmCounter*)CvmHandleRetire(counter, CVM_HANDLE_COUNTER);
    if (released == NULL) return 0;
    free(released);
    return 1;
}

int64_t cvm_counter_add(int64_t counter, int64_t delta)
{
    CvmCounter* found = (CvmCounter*)CvmHandleAcquire(counter, CVM_HANDLE_COUNTER);
    if (found == NULL) return 0;
    return InterlockedExchangeAdd64((volatile LONG64*)&found->value, (LONG64)delta) + delta;
}

int64_t cvm_counter_set(int64_t counter, int64_t value)
{
    CvmCounter* found = (CvmCounter*)CvmHandleAcquire(counter, CVM_HANDLE_COUNTER);
    if (found == NULL) return 0;
    return InterlockedExchange64((volatile LONG64*)&found->value, (LONG64)value);
}

int64_t cvm_counter_get(int64_t counter)
{
    CvmCounter* found = (CvmCounter*)CvmHandleAcquire(counter, CVM_HANDLE_COUNTER);
    if (found == NULL) return 0;
    return InterlockedCompareExchange64((volatile LONG64*)&found->value, 0, 0);
}
