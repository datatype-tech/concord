// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHandles.h"
#include "CvmRuntime.h"

#include <stdlib.h>
#include <string.h>

/**
 * The f32 containers.
 *
 * Unlike the f64 family, these do not share the 64-bit container: the whole
 * reason to have them is that a slot is four bytes. They are a separate store
 * with the same growth and bounds discipline, reached through their own handle
 * kinds, so an f32 list and an i64 list are different objects and a handle from
 * one does not resolve in the other.
 *
 * f32 exists for data that has to match the engine's own precision -- vertex
 * positions, per-instance parameters, anything destined for a GPU buffer.
 * Arithmetic in a script should still be f64; these are for storage.
 */
typedef struct CvmList32 {
    float* elements;
    int64_t size;
    int64_t capacity;
} CvmList32;

typedef struct CvmArray32 {
    float* elements;
    int64_t size;
} CvmArray32;

static int CvmList32Reserve(CvmList32* list, int64_t needed)
{
    if (needed <= list->capacity) return 1;
    int64_t capacity = list->capacity == 0 ? 8 : list->capacity;
    while (capacity < needed) capacity *= 2;
    float* grown = (float*)realloc(list->elements, (size_t)capacity * sizeof(float));
    if (grown == NULL) return 0;
    list->elements = grown;
    list->capacity = capacity;
    return 1;
}

int64_t cvm_list32_new(void)
{
    CvmList32* list = (CvmList32*)calloc(1, sizeof(CvmList32));
    if (list == NULL) return 0;
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_LIST32, list);
    if (handle == 0) free(list);
    return handle;
}

int64_t cvm_list32_free(int64_t list)
{
    CvmList32* released = (CvmList32*)CvmHandleRetire(list, CVM_HANDLE_LIST32);
    if (released == NULL) return 0;
    free(released->elements);
    free(released);
    return 1;
}

int64_t cvm_list32_size(int64_t list)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    return found == NULL ? 0 : found->size;
}

int64_t cvm_list32_clear(int64_t list)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL) return 0;
    found->size = 0;
    return 0;
}

int64_t cvm_list32_push(int64_t list, double value)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL) return 0;
    if (!CvmList32Reserve(found, found->size + 1)) return found->size;
    found->elements[found->size++] = (float)value;
    return found->size;
}

double cvm_list32_pop(int64_t list)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL || found->size == 0) return 0.0;
    return (double)found->elements[--found->size];
}

double cvm_list32_get(int64_t list, int64_t index)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL || index < 0 || index >= found->size) return 0.0;
    return (double)found->elements[index];
}

double cvm_list32_set(int64_t list, int64_t index, double value)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL || index < 0 || index >= found->size) return 0.0;
    found->elements[index] = (float)value;
    return (double)found->elements[index];
}

int64_t cvm_list32_insert(int64_t list, int64_t index, double value)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL) return 0;
    int64_t at = index;
    if (at < 0) at = 0;
    if (at > found->size) at = found->size;
    if (!CvmList32Reserve(found, found->size + 1)) return found->size;
    memmove(&found->elements[at + 1], &found->elements[at],
            (size_t)(found->size - at) * sizeof(float));
    found->elements[at] = (float)value;
    return ++found->size;
}

double cvm_list32_remove(int64_t list, int64_t index)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL || index < 0 || index >= found->size) return 0.0;
    const double removed = (double)found->elements[index];
    memmove(&found->elements[index], &found->elements[index + 1],
            (size_t)(found->size - index - 1) * sizeof(float));
    --found->size;
    return removed;
}

int64_t cvm_list32_index_of(int64_t list, double value)
{
    CvmList32* found = (CvmList32*)CvmHandleAcquire(list, CVM_HANDLE_LIST32);
    if (found == NULL) return -1;
    const float wanted = (float)value;
    for (int64_t index = 0; index < found->size; ++index) {
        if (found->elements[index] == wanted) return index;
    }
    return -1;
}

int64_t cvm_list32_contains(int64_t list, double value)
{
    return cvm_list32_index_of(list, value) >= 0 ? 1 : 0;
}

int64_t cvm_array32_new(int64_t length)
{
    if (length < 0) return 0;
    CvmArray32* array = (CvmArray32*)calloc(1, sizeof(CvmArray32));
    if (array == NULL) return 0;
    if (length > 0) {
        array->elements = (float*)calloc((size_t)length, sizeof(float));
        if (array->elements == NULL) {
            free(array);
            return 0;
        }
    }
    array->size = length;
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_ARRAY32, array);
    if (handle == 0) {
        free(array->elements);
        free(array);
    }
    return handle;
}

int64_t cvm_array32_free(int64_t array)
{
    CvmArray32* released = (CvmArray32*)CvmHandleRetire(array, CVM_HANDLE_ARRAY32);
    if (released == NULL) return 0;
    free(released->elements);
    free(released);
    return 1;
}

int64_t cvm_array32_size(int64_t array)
{
    CvmArray32* found = (CvmArray32*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY32);
    return found == NULL ? 0 : found->size;
}

double cvm_array32_get(int64_t array, int64_t index)
{
    CvmArray32* found = (CvmArray32*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY32);
    if (found == NULL || index < 0 || index >= found->size) return 0.0;
    return (double)found->elements[index];
}

double cvm_array32_set(int64_t array, int64_t index, double value)
{
    CvmArray32* found = (CvmArray32*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY32);
    if (found == NULL || index < 0 || index >= found->size) return 0.0;
    found->elements[index] = (float)value;
    return (double)found->elements[index];
}

int64_t cvm_array32_fill(int64_t array, double value)
{
    CvmArray32* found = (CvmArray32*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY32);
    if (found == NULL) return 0;
    for (int64_t index = 0; index < found->size; ++index) {
        found->elements[index] = (float)value;
    }
    return found->size;
}

double cvm_array32_sum(int64_t array)
{
    CvmArray32* found = (CvmArray32*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY32);
    if (found == NULL) return 0.0;
    double total = 0.0;
    for (int64_t index = 0; index < found->size; ++index) {
        total += (double)found->elements[index];
    }
    return total;
}

int64_t cvm_array32_copy(int64_t array)
{
    CvmArray32* found = (CvmArray32*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY32);
    if (found == NULL) return 0;
    const int64_t handle = cvm_array32_new(found->size);
    if (handle == 0) return 0;
    CvmArray32* copy = (CvmArray32*)CvmHandleAcquire(handle, CVM_HANDLE_ARRAY32);
    if (copy != NULL && found->size > 0) {
        memcpy(copy->elements, found->elements, (size_t)found->size * sizeof(float));
    }
    return handle;
}

int64_t cvm_array32_reverse(int64_t array)
{
    CvmArray32* found = (CvmArray32*)CvmHandleAcquire(array, CVM_HANDLE_ARRAY32);
    if (found == NULL) return 0;
    for (int64_t left = 0, right = found->size - 1; left < right; ++left, --right) {
        const float swap = found->elements[left];
        found->elements[left] = found->elements[right];
        found->elements[right] = swap;
    }
    return found->size;
}
