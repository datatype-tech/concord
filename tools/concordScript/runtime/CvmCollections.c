// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHandles.h"
#include "CvmRuntime.h"

#include <stdlib.h>
#include <string.h>

/**
 * Growable and fixed-size int64 containers.
 *
 * Element access is bounds-checked against the container's own length rather
 * than trusted, because a CVM program can hand any integer to any function.
 * The handle lookup and the element access are separately locked: the lookup
 * proves the handle is live, and the caller is responsible for not mutating
 * one container from two threads without a mutex of its own.
 */
typedef struct CvmList {
    int64_t* elements;
    int64_t size;
    int64_t capacity;
} CvmList;

typedef struct CvmArray {
    int64_t* elements;
    int64_t size;
} CvmArray;

/** Doubles a list's storage, or reports that it cannot. */
static int CvmListReserve(CvmList* list, int64_t needed)
{
    if (needed <= list->capacity) return 1;
    int64_t capacity = list->capacity == 0 ? 8 : list->capacity;
    while (capacity < needed) capacity *= 2;
    int64_t* grown = (int64_t*)realloc(list->elements, (size_t)capacity * sizeof(int64_t));
    if (grown == NULL) return 0;
    list->elements = grown;
    list->capacity = capacity;
    return 1;
}

/** Shifts elements right, making room for one value at `index`. */
static void CvmListShiftUp(CvmList* list, int64_t index)
{
    memmove(&list->elements[index + 1], &list->elements[index],
            (size_t)(list->size - index) * sizeof(int64_t));
    ++list->size;
}

/** Shifts elements left, closing the gap at `index`. */
static void CvmListShiftDown(CvmList* list, int64_t index)
{
    memmove(&list->elements[index], &list->elements[index + 1],
            (size_t)(list->size - index - 1) * sizeof(int64_t));
    --list->size;
}

/** Clamps a possibly negative index into [0, size] for insertion. */
static int64_t CvmListClampInsert(CvmList* list, int64_t index)
{
    if (index < 0) return 0;
    if (index > list->size) return list->size;
    return index;
}

int64_t cvm_list_new(void)
{
    CvmList* list = (CvmList*)calloc(1, sizeof(CvmList));
    if (list == NULL) return 0;
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_LIST, list);
    if (handle == 0) free(list);
    return handle;
}

int64_t cvm_list_free(int64_t list)
{
    CvmList* released = (CvmList*)CvmHandleRetire(list, CVM_HANDLE_LIST);
    if (released == NULL) return 0;
    free(released->elements);
    free(released);
    return 1;
}

int64_t cvm_list_size(int64_t list)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    return found == NULL ? 0 : found->size;
}

int64_t cvm_list_clear(int64_t list)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL) return 0;
    found->size = 0;
    return 0;
}

int64_t cvm_list_push(int64_t list, int64_t value)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL) return 0;
    if (!CvmListReserve(found, found->size + 1)) return found->size;
    found->elements[found->size++] = value;
    return found->size;
}

int64_t cvm_list_pop(int64_t list)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL || found->size == 0) return 0;
    return found->elements[--found->size];
}

int64_t cvm_list_get(int64_t list, int64_t index)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL || index < 0 || index >= found->size) return 0;
    return found->elements[index];
}

int64_t cvm_list_set(int64_t list, int64_t index, int64_t value)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL || index < 0 || index >= found->size) return 0;
    found->elements[index] = value;
    return value;
}

int64_t cvm_list_insert(int64_t list, int64_t index, int64_t value)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL) return 0;
    const int64_t at = CvmListClampInsert(found, index);
    if (!CvmListReserve(found, found->size + 1)) return found->size;
    CvmListShiftUp(found, at);
    found->elements[at] = value;
    return found->size;
}

int64_t cvm_list_remove(int64_t list, int64_t index)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL || index < 0 || index >= found->size) return 0;
    const int64_t removed = found->elements[index];
    CvmListShiftDown(found, index);
    return removed;
}

int64_t cvm_list_contains(int64_t list, int64_t value)
{
    return cvm_list_index_of(list, value) >= 0 ? 1 : 0;
}

int64_t cvm_list_index_of(int64_t list, int64_t value)
{
    CvmList* found = (CvmList*)CvmHandleAcquire(list, CVM_HANDLE_LIST);
    if (found == NULL) return -1;
    for (int64_t index = 0; index < found->size; ++index) {
        if (found->elements[index] == value) return index;
    }
    return -1;
}

int64_t cvm_array_new(int64_t length)
{
    if (length < 0) return 0;
    CvmArray* array = (CvmArray*)calloc(1, sizeof(CvmArray));
    if (array == NULL) return 0;
    if (length > 0) {
        array->elements = (int64_t*)calloc((size_t)length, sizeof(int64_t));
        if (array->elements == NULL) {
            free(array);
            return 0;
        }
    }
    array->size = length;
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_ARRAY, array);
    if (handle == 0) {
        free(array->elements);
        free(array);
    }
    return handle;
}

int64_t cvm_array_free(int64_t array)
{
    CvmArray* released = (CvmArray*)CvmHandleRetire(array, CVM_HANDLE_ARRAY);
    if (released == NULL) return 0;
    free(released->elements);
    free(released);
    return 1;
}

int64_t cvm_array_size(int64_t array)
{
    CvmArray* found = (CvmArray*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY);
    return found == NULL ? 0 : found->size;
}

int64_t cvm_array_get(int64_t array, int64_t index)
{
    CvmArray* found = (CvmArray*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY);
    if (found == NULL || index < 0 || index >= found->size) return 0;
    return found->elements[index];
}

int64_t cvm_array_set(int64_t array, int64_t index, int64_t value)
{
    CvmArray* found = (CvmArray*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY);
    if (found == NULL || index < 0 || index >= found->size) return 0;
    found->elements[index] = value;
    return value;
}

int64_t cvm_array_fill(int64_t array, int64_t value)
{
    CvmArray* found = (CvmArray*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY);
    if (found == NULL) return 0;
    for (int64_t index = 0; index < found->size; ++index) found->elements[index] = value;
    return found->size;
}

int64_t cvm_array_sum(int64_t array)
{
    CvmArray* found = (CvmArray*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY);
    if (found == NULL) return 0;
    int64_t total = 0;
    for (int64_t index = 0; index < found->size; ++index) total += found->elements[index];
    return total;
}

int64_t cvm_array_copy(int64_t array)
{
    CvmArray* found = (CvmArray*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY);
    if (found == NULL) return 0;
    const int64_t handle = cvm_array_new(found->size);
    if (handle == 0) return 0;
    CvmArray* copy = (CvmArray*)CvmHandleAcquire(handle, CVM_HANDLE_ARRAY);
    if (copy != NULL && found->size > 0) {
        memcpy(copy->elements, found->elements, (size_t)found->size * sizeof(int64_t));
    }
    return handle;
}

int64_t cvm_array_reverse(int64_t array)
{
    CvmArray* found = (CvmArray*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY);
    if (found == NULL) return 0;
    for (int64_t left = 0, right = found->size - 1; left < right; ++left, --right) {
        const int64_t swap = found->elements[left];
        found->elements[left] = found->elements[right];
        found->elements[right] = swap;
    }
    return found->size;
}
