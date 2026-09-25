// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDCVM_HANDLES_H
#define CONCORDCVM_HANDLES_H

/**
 * Internal handle table shared by the Concord Visual Machine runtime units.
 *
 * CVM never sees a host pointer. A container, thread, mutex or counter is
 * identified by a slot index encoded as \c index + 1, so zero is always an
 * invalid handle and a value released on one thread cannot later be mistaken
 * for a live object of another kind: every acquisition re-checks both the
 * slot's liveness and its kind.
 */

#include <stdint.h>

/** Identifies which runtime unit owns a slot's object. */
typedef enum CvmHandleKind {
    CVM_HANDLE_LIST = 1,
    CVM_HANDLE_ARRAY = 2,
    CVM_HANDLE_THREAD = 3,
    CVM_HANDLE_MUTEX = 4,
    CVM_HANDLE_COUNTER = 5,

    /** A list whose slots are 4 bytes rather than 8. */
    CVM_HANDLE_LIST32 = 6,

    /** An array whose slots are 4 bytes rather than 8. */
    CVM_HANDLE_ARRAY32 = 7,

    /** An i64-to-i64 associative map. */
    CVM_HANDLE_MAP = 8,

    /** A string-to-i64 associative map. */
    CVM_HANDLE_SMAP = 9
} CvmHandleKind;

/** Stores \p object and returns its handle, or 0 when allocation fails. */
int64_t CvmHandleCreate(CvmHandleKind kind, void* object);

/** Returns the object for \p handle, or NULL when it is not a live \p kind. */
void* CvmHandleAcquire(int64_t handle, CvmHandleKind kind);

/** Retires \p handle and returns its object, or NULL when it is not live. */
void* CvmHandleRetire(int64_t handle, CvmHandleKind kind);

/** Acquires the runtime-wide lock guarding the handle table and shared state. */
void CvmRuntimeLock(void);

/** Releases the runtime-wide lock. */
void CvmRuntimeUnlock(void);

#endif // CONCORDCVM_HANDLES_H
