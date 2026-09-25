// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmRuntime.h"

#include <stdlib.h>
#include <string.h>

/** The owned-string allocator, defined in CvmString.c and internal to the library. */
char* cvm_str_alloc(size_t length);

/**
 * Lists of strings.
 *
 * A string is a pointer and a pointer is 64 bits, so this needs no container of
 * its own: it stores the address in the same slot an i64 list uses, and the
 * aliases that do no reinterpretation (new, free, size, clear) point straight
 * at the i64 symbols. What is here is only what has to know the slots hold
 * text.
 *
 * Nothing is copied. A list holds the pointer it was given, so a string that
 * was freed -- or one that came from a helper whose result nobody kept -- is a
 * dangling entry the same way it would be in C. Strings handed to a list are
 * expected to be literals or to outlive the list.
 */

int64_t cvm_slist_push(int64_t list, const char* text)
{
    return cvm_list_push(list, (int64_t)(intptr_t)text);
}

const char* cvm_slist_get(int64_t list, int64_t index)
{
    return (const char*)(intptr_t)cvm_list_get(list, index);
}

const char* cvm_slist_set(int64_t list, int64_t index, const char* text)
{
    cvm_list_set(list, index, (int64_t)(intptr_t)text);
    return text;
}

const char* cvm_slist_pop(int64_t list)
{
    return (const char*)(intptr_t)cvm_list_pop(list);
}

int64_t cvm_slist_insert(int64_t list, int64_t index, const char* text)
{
    return cvm_list_insert(list, index, (int64_t)(intptr_t)text);
}

const char* cvm_slist_remove(int64_t list, int64_t index)
{
    return (const char*)(intptr_t)cvm_list_remove(list, index);
}

int64_t cvm_slist_index_of(int64_t list, const char* text)
{
    const int64_t count = cvm_list_size(list);
    for (int64_t index = 0; index < count; ++index) {
        if (cvm_str_eq(cvm_slist_get(list, index), text) == 1) return index;
    }
    return -1;
}

int64_t cvm_slist_contains(int64_t list, const char* text)
{
    return cvm_slist_index_of(list, text) >= 0 ? 1 : 0;
}

/**
 * Joins every entry with \p separator into one newly allocated string.
 *
 * The reason this exists rather than leaving it to a loop in the script: a
 * script building a string by repeated concatenation allocates a new string per
 * step and has to free each one, and getting that exactly right by hand is the
 * kind of thing a library should own.
 */
char* cvm_slist_join(int64_t list, const char* separator)
{
    const int64_t count = cvm_list_size(list);
    const size_t separatorLength = separator == NULL ? 0 : strlen(separator);

    size_t total = 0;
    for (int64_t index = 0; index < count; ++index) {
        const char* entry = cvm_slist_get(list, index);
        total += entry == NULL ? 0 : strlen(entry);
        if (index + 1 < count) total += separatorLength;
    }

    char* joined = cvm_str_alloc(total);
    if (joined == NULL) return NULL;
    size_t at = 0;
    for (int64_t index = 0; index < count; ++index) {
        const char* entry = cvm_slist_get(list, index);
        if (entry != NULL) {
            const size_t length = strlen(entry);
            memcpy(joined + at, entry, length);
            at += length;
        }
        if (index + 1 < count && separatorLength > 0) {
            memcpy(joined + at, separator, separatorLength);
            at += separatorLength;
        }
    }
    joined[at] = '\0';
    return joined;
}
