// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHandles.h"

#include <windows.h>

#include <stdlib.h>

/**
 * Slot storage for the runtime handle table.
 *
 * The array only ever grows: a retired slot is pushed on a free stack and
 * reused by the next allocation, so a handle never changes meaning while it
 * is outstanding. Allocation and lookup share one critical section, which
 * also serialises the pseudo-random generator in CvmSystem.c.
 */
typedef struct CvmHandleSlot {
    void* object;
    int32_t kind;
    int32_t alive;
} CvmHandleSlot;

static CRITICAL_SECTION g_lock;
static INIT_ONCE g_lockOnce = INIT_ONCE_STATIC_INIT;
static CvmHandleSlot* g_slots = NULL;
static int64_t* g_freeSlots = NULL;
static int64_t g_slotCount = 0;
static int64_t g_slotCapacity = 0;
static int64_t g_freeCount = 0;
static int64_t g_freeCapacity = 0;

static BOOL CALLBACK CvmInitLock(PINIT_ONCE once, PVOID parameter, PVOID* context)
{
    (void)once;
    (void)parameter;
    (void)context;
    InitializeCriticalSection(&g_lock);
    return TRUE;
}

void CvmRuntimeLock(void)
{
    InitOnceExecuteOnce(&g_lockOnce, CvmInitLock, NULL, NULL);
    EnterCriticalSection(&g_lock);
}

void CvmRuntimeUnlock(void)
{
    LeaveCriticalSection(&g_lock);
}

/** Grows the slot array; handles stay valid because indices do not move. */
static int CvmGrowSlots(void)
{
    const int64_t capacity = g_slotCapacity == 0 ? 64 : g_slotCapacity * 2;
    CvmHandleSlot* grown =
        (CvmHandleSlot*)realloc(g_slots, (size_t)capacity * sizeof(CvmHandleSlot));
    if (grown == NULL) return 0;
    for (int64_t index = g_slotCapacity; index < capacity; ++index) {
        grown[index].object = NULL;
        grown[index].kind = 0;
        grown[index].alive = 0;
    }
    g_slots = grown;
    g_slotCapacity = capacity;
    return 1;
}

/** Records a retired slot index for reuse; drops it when it cannot grow. */
static void CvmPushFreeSlot(int64_t index)
{
    if (g_freeCount == g_freeCapacity) {
        const int64_t capacity = g_freeCapacity == 0 ? 64 : g_freeCapacity * 2;
        int64_t* grown = (int64_t*)realloc(g_freeSlots, (size_t)capacity * sizeof(int64_t));
        if (grown == NULL) return;
        g_freeSlots = grown;
        g_freeCapacity = capacity;
    }
    g_freeSlots[g_freeCount++] = index;
}

int64_t CvmHandleCreate(CvmHandleKind kind, void* object)
{
    if (object == NULL) return 0;
    CvmRuntimeLock();
    int64_t index = -1;
    if (g_freeCount > 0) {
        index = g_freeSlots[--g_freeCount];
    } else {
        if (g_slotCount == g_slotCapacity && !CvmGrowSlots()) {
            CvmRuntimeUnlock();
            return 0;
        }
        index = g_slotCount++;
    }
    g_slots[index].object = object;
    g_slots[index].kind = (int32_t)kind;
    g_slots[index].alive = 1;
    CvmRuntimeUnlock();
    return index + 1;
}

/** Resolves a handle while the caller already holds the runtime lock. */
static CvmHandleSlot* CvmLookupLocked(int64_t handle, CvmHandleKind kind)
{
    if (handle <= 0) return NULL;
    const int64_t index = handle - 1;
    if (index >= g_slotCount) return NULL;
    CvmHandleSlot* slot = &g_slots[index];
    if (!slot->alive || slot->kind != (int32_t)kind) return NULL;
    return slot;
}

void* CvmHandleAcquire(int64_t handle, CvmHandleKind kind)
{
    CvmRuntimeLock();
    CvmHandleSlot* slot = CvmLookupLocked(handle, kind);
    void* object = slot == NULL ? NULL : slot->object;
    CvmRuntimeUnlock();
    return object;
}

void* CvmHandleRetire(int64_t handle, CvmHandleKind kind)
{
    CvmRuntimeLock();
    CvmHandleSlot* slot = CvmLookupLocked(handle, kind);
    void* object = NULL;
    if (slot != NULL) {
        object = slot->object;
        slot->object = NULL;
        slot->alive = 0;
        slot->kind = 0;
        CvmPushFreeSlot(handle - 1);
    }
    CvmRuntimeUnlock();
    return object;
}
