// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmRuntime.h"

#include <string.h>

/**
 * The f64 view of the containers.
 *
 * A CVM list already holds 64-bit slots, and an f64 is 64 bits, so the two
 * families share one container rather than duplicating the growth, bounds and
 * handle logic. The i64 accessors move a value through unchanged; these move
 * its bit pattern and reinterpret on the way back out.
 *
 * The consequence is worth stating plainly: a list and an flist are the same
 * object, and a handle from one works in the other. That is a feature here --
 * it is the same eight bytes either way.
 *
 * The aliases that need no reinterpretation at all (new, free, size, clear,
 * copy, reverse) are registered against the i64 symbols directly and have no
 * wrapper in this file.
 */
static int64_t CvmBits(double value)
{
    int64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double CvmValue(int64_t bits)
{
    double value = 0.0;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int64_t cvm_flist_push(int64_t list, double value)
{
    return cvm_list_push(list, CvmBits(value));
}

double cvm_flist_pop(int64_t list)
{
    return CvmValue(cvm_list_pop(list));
}

double cvm_flist_get(int64_t list, int64_t index)
{
    return CvmValue(cvm_list_get(list, index));
}

double cvm_flist_set(int64_t list, int64_t index, double value)
{
    return CvmValue(cvm_list_set(list, index, CvmBits(value)));
}

int64_t cvm_flist_insert(int64_t list, int64_t index, double value)
{
    return cvm_list_insert(list, index, CvmBits(value));
}

double cvm_flist_remove(int64_t list, int64_t index)
{
    return CvmValue(cvm_list_remove(list, index));
}

/**
 * Compares by value rather than by bit pattern.
 *
 * -0.0 and 0.0 are equal to an author even though their bits differ, so a
 * search that compared bits would answer a question nobody asked.
 */
int64_t cvm_flist_index_of(int64_t list, double value)
{
    const int64_t count = cvm_list_size(list);
    for (int64_t index = 0; index < count; ++index) {
        if (cvm_flist_get(list, index) == value) return index;
    }
    return -1;
}

int64_t cvm_flist_contains(int64_t list, double value)
{
    return cvm_flist_index_of(list, value) >= 0 ? 1 : 0;
}

double cvm_farray_get(int64_t array, int64_t index)
{
    return CvmValue(cvm_array_get(array, index));
}

double cvm_farray_set(int64_t array, int64_t index, double value)
{
    return CvmValue(cvm_array_set(array, index, CvmBits(value)));
}

int64_t cvm_farray_fill(int64_t array, double value)
{
    return cvm_array_fill(array, CvmBits(value));
}

double cvm_farray_sum(int64_t array)
{
    const int64_t count = cvm_array_size(array);
    double total = 0.0;
    for (int64_t index = 0; index < count; ++index) {
        total += cvm_farray_get(array, index);
    }
    return total;
}
