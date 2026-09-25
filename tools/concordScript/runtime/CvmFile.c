// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmRuntime.h"

#include <stdio.h>
#include <string.h>

/** The allocator CvmString.c exposes for owned CVM strings. */
char* cvm_str_alloc(size_t length);

/** Upper bound so a missing path cannot be used to allocate the heap. */
enum { kCvmFileMaxBytes = 16 * 1024 * 1024 };

char* cvm_file_read(const char* path)
{
    if (path == NULL || path[0] == '\0') return NULL;
    FILE* file = fopen(path, "rb");
    if (file == NULL) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    const long size = ftell(file);
    if (size < 0 || size > kCvmFileMaxBytes) {
        fclose(file);
        return NULL;
    }
    rewind(file);
    char* text = cvm_str_alloc((size_t)size);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    if (size > 0 && fread(text, 1, (size_t)size, file) != (size_t)size) {
        cvm_str_free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    return text;
}

int64_t cvm_file_write(const char* path, const char* text)
{
    if (path == NULL || path[0] == '\0') return 0;
    FILE* file = fopen(path, "wb");
    if (file == NULL) return 0;
    const char* bytes = text == NULL ? "" : text;
    const size_t length = strlen(bytes);
    const int ok = fwrite(bytes, 1, length, file) == length && fflush(file) == 0;
    fclose(file);
    return ok ? 1 : 0;
}

int64_t cvm_file_exists(const char* path)
{
    if (path == NULL || path[0] == '\0') return 0;
    FILE* file = fopen(path, "rb");
    if (file == NULL) return 0;
    fclose(file);
    return 1;
}
