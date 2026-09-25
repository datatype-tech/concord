// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHandles.h"
#include "CvmRuntime.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * CVM strings.
 *
 * A CVM string is a pointer to the first character of a buffer that is
 * preceded by an ownership word:
 *
 *     [ int64 owned ][ bytes... ][ NUL ]
 *                      ^ the pointer a script holds
 *
 * The header exists so \c cvm_str_free is always safe. The compiler emits
 * literals as static data with owned = 0, and every allocating function here
 * returns owned = 1, which means freeing a literal is a no-op rather than a
 * crash -- the one mistake a script holding strings is most likely to make.
 *
 * Strings are NUL-terminated bytes, not Unicode: CVM has no character type and
 * no encoding, and pretending otherwise would be a promise this layer cannot
 * keep. Lengths are byte counts.
 */
typedef struct CvmStringHeader {
    int64_t owned;
} CvmStringHeader;

/** Returns the header of a CVM string, or NULL for a null pointer. */
static CvmStringHeader* CvmStringOwner(const char* text)
{
    return text == NULL ? NULL : ((CvmStringHeader*)text) - 1;
}

/** Allocates an owned string of \p length bytes, or NULL on failure. */
static char* CvmStringAllocate(size_t length);

/** The allocator above, exposed to the other string units. */
char* cvm_str_alloc(size_t length)
{
    return CvmStringAllocate(length);
}

static char* CvmStringAllocate(size_t length)
{
    CvmStringHeader* header = (CvmStringHeader*)malloc(sizeof(CvmStringHeader) + length + 1);
    if (header == NULL) return NULL;
    header->owned = 1;
    char* text = (char*)(header + 1);
    text[length] = '\0';
    return text;
}

int64_t cvm_str_len(const char* text)
{
    return text == NULL ? 0 : (int64_t)strlen(text);
}

int64_t cvm_str_eq(const char* left, const char* right)
{
    if (left == right) return 1;
    if (left == NULL || right == NULL) return 0;
    return strcmp(left, right) == 0 ? 1 : 0;
}

char* cvm_str_concat(const char* left, const char* right)
{
    const size_t leftLength = left == NULL ? 0 : strlen(left);
    const size_t rightLength = right == NULL ? 0 : strlen(right);
    char* text = CvmStringAllocate(leftLength + rightLength);
    if (text == NULL) return NULL;
    if (leftLength > 0) memcpy(text, left, leftLength);
    if (rightLength > 0) memcpy(text + leftLength, right, rightLength);
    return text;
}

int64_t cvm_str_free(char* text)
{
    CvmStringHeader* header = CvmStringOwner(text);
    if (header == NULL) return 0;
    if (header->owned == 0) return 0;
    free(header);
    return 1;
}

int64_t cvm_str_find(const char* haystack, const char* needle)
{
    if (haystack == NULL || needle == NULL) return -1;
    const char* found = strstr(haystack, needle);
    return found == NULL ? -1 : (int64_t)(found - haystack);
}

char* cvm_str_sub(const char* text, int64_t start, int64_t count)
{
    if (text == NULL || count < 0) return NULL;
    const int64_t length = (int64_t)strlen(text);
    if (start < 0) start = 0;
    if (start > length) start = length;
    if (count > length - start) count = length - start;
    char* piece = CvmStringAllocate((size_t)count);
    if (piece == NULL) return NULL;
    memcpy(piece, text + start, (size_t)count);
    return piece;
}

char* cvm_str_from_int(int64_t value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%" PRId64, value);
    const size_t length = strlen(buffer);
    char* text = CvmStringAllocate(length);
    if (text == NULL) return NULL;
    memcpy(text, buffer, length);
    return text;
}

char* cvm_str_from_float(double value)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%g", value);
    const size_t length = strlen(buffer);
    char* text = CvmStringAllocate(length);
    if (text == NULL) return NULL;
    memcpy(text, buffer, length);
    return text;
}

int64_t cvm_str_to_int(const char* text)
{
    if (text == NULL) return 0;
    return (int64_t)strtoll(text, NULL, 10);
}

double cvm_str_to_float(const char* text)
{
    if (text == NULL) return 0.0;
    return strtod(text, NULL);
}

int64_t cvm_str_char_at(const char* text, int64_t index)
{
    if (text == NULL || index < 0) return -1;
    const int64_t length = (int64_t)strlen(text);
    if (index >= length) return -1;
    return (int64_t)(unsigned char)text[index];
}

const char* cvm_print_str(const char* text)
{
    printf("cvm: %s\n", text == NULL ? "(null)" : text);
    fflush(stdout);
    return text;
}

int64_t cvm_str_len_utf8(const char* text)
{
    if (text == NULL) return 0;
    int64_t count = 0;
    for (const unsigned char* at = (const unsigned char*)text; *at != 0; ++at) {
        // A continuation byte is never the start of a character, so counting
        // the bytes that are not one counts the characters.
        if ((*at & 0xC0) != 0x80) ++count;
    }
    return count;
}

int64_t cvm_str_at_utf8(const char* text, int64_t index)
{
    if (text == NULL || index < 0) return -1;
    const unsigned char* at = (const unsigned char*)text;
    int64_t seen = 0;
    while (*at != 0) {
        if ((*at & 0xC0) != 0x80) {
            if (seen == index) {
                // Decode the sequence that starts here.
                int64_t code = 0;
                int extra = 0;
                if (*at < 0x80) {
                    code = *at;
                } else if ((*at & 0xE0) == 0xC0) {
                    code = *at & 0x1F;
                    extra = 1;
                } else if ((*at & 0xF0) == 0xE0) {
                    code = *at & 0x0F;
                    extra = 2;
                } else if ((*at & 0xF8) == 0xF0) {
                    code = *at & 0x07;
                    extra = 3;
                } else {
                    return -1;  // Not a lead byte this decoder recognises.
                }
                for (int step = 0; step < extra; ++step) {
                    const unsigned char next = at[step + 1];
                    if ((next & 0xC0) != 0x80) return -1;
                    code = (code << 6) | (next & 0x3F);
                }
                return code;
            }
            ++seen;
        }
        ++at;
    }
    return -1;
}

char* cvm_str_from_code(int64_t code)
{
    if (code < 0 || code > 0x10FFFF) return NULL;
    char buffer[4];
    size_t length = 0;
    if (code < 0x80) {
        buffer[length++] = (char)code;
    } else if (code < 0x800) {
        buffer[length++] = (char)(0xC0 | (code >> 6));
        buffer[length++] = (char)(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
        buffer[length++] = (char)(0xE0 | (code >> 12));
        buffer[length++] = (char)(0x80 | ((code >> 6) & 0x3F));
        buffer[length++] = (char)(0x80 | (code & 0x3F));
    } else {
        buffer[length++] = (char)(0xF0 | (code >> 18));
        buffer[length++] = (char)(0x80 | ((code >> 12) & 0x3F));
        buffer[length++] = (char)(0x80 | ((code >> 6) & 0x3F));
        buffer[length++] = (char)(0x80 | (code & 0x3F));
    }
    char* text = CvmStringAllocate(length);
    if (text == NULL) return NULL;
    memcpy(text, buffer, length);
    return text;
}

/** Case folding that only touches ASCII, because that is all that is defined here. */
static char* CvmFoldCase(const char* text, int upper)
{
    if (text == NULL) return NULL;
    const size_t length = strlen(text);
    char* folded = CvmStringAllocate(length);
    if (folded == NULL) return NULL;
    for (size_t index = 0; index < length; ++index) {
        char character = text[index];
        // Bytes at or above 0x80 are part of a multi-byte sequence and are left
        // alone: folding them needs a Unicode table this layer does not have,
        // and a wrong answer would be worse than no answer.
        if (upper && character >= 'a' && character <= 'z') {
            character = (char)(character - 'a' + 'A');
        } else if (!upper && character >= 'A' && character <= 'Z') {
            character = (char)(character - 'A' + 'a');
        }
        folded[index] = character;
    }
    return folded;
}

char* cvm_str_upper(const char* text)
{
    return CvmFoldCase(text, 1);
}

char* cvm_str_lower(const char* text)
{
    return CvmFoldCase(text, 0);
}
