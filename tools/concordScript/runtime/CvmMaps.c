// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHandles.h"
#include "CvmRuntime.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Associative maps for the CVM stdlib.
 *
 * Linear pair lists keep the implementation small and the ABI identical to
 * lists: a handle, integer keys or copied strings, and no iterator object.
 * A lookup that misses returns 0, same as list_get, so map_has / smap_has
 * exist to tell "stored zero" from "not present".
 */

typedef struct {
    int64_t key;
    int64_t value;
} MapPair;

typedef struct {
    MapPair* pairs;
    int64_t len;
    int64_t cap;
} Map;

typedef struct {
    char* key;
    int64_t value;
} SMapPair;

typedef struct {
    SMapPair* pairs;
    int64_t len;
    int64_t cap;
} SMap;

static int GrowCap(int64_t current, int64_t* outCap)
{
    int64_t cap = current < 4 ? 4 : current * 2;
    if (cap <= current) return 0;
    *outCap = cap;
    return 1;
}

static Map* AsMap(int64_t handle)
{
    return (Map*)CvmHandleAcquire(handle, CVM_HANDLE_MAP);
}

static SMap* AsSMap(int64_t handle)
{
    return (SMap*)CvmHandleAcquire(handle, CVM_HANDLE_SMAP);
}

static int64_t FindKey(const Map* map, int64_t key)
{
    for (int64_t i = 0; i < map->len; ++i) {
        if (map->pairs[i].key == key) return i;
    }
    return -1;
}

static int64_t FindSKey(const SMap* map, const char* key)
{
    if (key == NULL) return -1;
    for (int64_t i = 0; i < map->len; ++i) {
        if (strcmp(map->pairs[i].key, key) == 0) return i;
    }
    return -1;
}

int64_t cvm_map_new(void)
{
    Map* map = (Map*)calloc(1, sizeof(Map));
    if (map == NULL) return 0;
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_MAP, map);
    if (handle == 0) free(map);
    return handle;
}

int64_t cvm_map_free(int64_t map)
{
    Map* payload = (Map*)CvmHandleRetire(map, CVM_HANDLE_MAP);
    if (payload == NULL) return 0;
    free(payload->pairs);
    free(payload);
    return 1;
}

int64_t cvm_map_len(int64_t map)
{
    const Map* payload = AsMap(map);
    return payload == NULL ? -1 : payload->len;
}

int64_t cvm_map_set(int64_t map, int64_t key, int64_t value)
{
    Map* payload = AsMap(map);
    if (payload == NULL) return 0;
    const int64_t found = FindKey(payload, key);
    if (found >= 0) {
        payload->pairs[found].value = value;
        return 1;
    }
    if (payload->len == payload->cap) {
        int64_t cap = 0;
        if (!GrowCap(payload->cap, &cap)) return 0;
        MapPair* pairs = (MapPair*)realloc(payload->pairs, (size_t)cap * sizeof(MapPair));
        if (pairs == NULL) return 0;
        payload->pairs = pairs;
        payload->cap = cap;
    }
    payload->pairs[payload->len].key = key;
    payload->pairs[payload->len].value = value;
    payload->len += 1;
    return 1;
}

int64_t cvm_map_get(int64_t map, int64_t key)
{
    const Map* payload = AsMap(map);
    if (payload == NULL) return 0;
    const int64_t found = FindKey(payload, key);
    return found < 0 ? 0 : payload->pairs[found].value;
}

int64_t cvm_map_has(int64_t map, int64_t key)
{
    const Map* payload = AsMap(map);
    return payload == NULL ? 0 : (FindKey(payload, key) >= 0 ? 1 : 0);
}

int64_t cvm_map_remove(int64_t map, int64_t key)
{
    Map* payload = AsMap(map);
    if (payload == NULL) return 0;
    const int64_t found = FindKey(payload, key);
    if (found < 0) return 0;
    payload->len -= 1;
    payload->pairs[found] = payload->pairs[payload->len];
    return 1;
}

int64_t cvm_smap_new(void)
{
    SMap* map = (SMap*)calloc(1, sizeof(SMap));
    if (map == NULL) return 0;
    const int64_t handle = CvmHandleCreate(CVM_HANDLE_SMAP, map);
    if (handle == 0) free(map);
    return handle;
}

int64_t cvm_smap_free(int64_t map)
{
    SMap* payload = (SMap*)CvmHandleRetire(map, CVM_HANDLE_SMAP);
    if (payload == NULL) return 0;
    for (int64_t i = 0; i < payload->len; ++i) {
        free(payload->pairs[i].key);
    }
    free(payload->pairs);
    free(payload);
    return 1;
}

int64_t cvm_smap_len(int64_t map)
{
    const SMap* payload = AsSMap(map);
    return payload == NULL ? -1 : payload->len;
}

int64_t cvm_smap_set(int64_t map, const char* key, int64_t value)
{
    SMap* payload = AsSMap(map);
    if (payload == NULL || key == NULL) return 0;
    const int64_t found = FindSKey(payload, key);
    if (found >= 0) {
        payload->pairs[found].value = value;
        return 1;
    }
    const size_t bytes = strlen(key) + 1;
    char* copy = (char*)malloc(bytes);
    if (copy == NULL) return 0;
    memcpy(copy, key, bytes);
    if (payload->len == payload->cap) {
        int64_t cap = 0;
        if (!GrowCap(payload->cap, &cap)) {
            free(copy);
            return 0;
        }
        SMapPair* pairs = (SMapPair*)realloc(payload->pairs, (size_t)cap * sizeof(SMapPair));
        if (pairs == NULL) {
            free(copy);
            return 0;
        }
        payload->pairs = pairs;
        payload->cap = cap;
    }
    payload->pairs[payload->len].key = copy;
    payload->pairs[payload->len].value = value;
    payload->len += 1;
    return 1;
}

int64_t cvm_smap_get(int64_t map, const char* key)
{
    const SMap* payload = AsSMap(map);
    if (payload == NULL) return 0;
    const int64_t found = FindSKey(payload, key);
    return found < 0 ? 0 : payload->pairs[found].value;
}

int64_t cvm_smap_has(int64_t map, const char* key)
{
    const SMap* payload = AsSMap(map);
    return payload == NULL ? 0 : (FindSKey(payload, key) >= 0 ? 1 : 0);
}

int64_t cvm_smap_remove(int64_t map, const char* key)
{
    SMap* payload = AsSMap(map);
    if (payload == NULL) return 0;
    const int64_t found = FindSKey(payload, key);
    if (found < 0) return 0;
    free(payload->pairs[found].key);
    payload->len -= 1;
    payload->pairs[found] = payload->pairs[payload->len];
    payload->pairs[payload->len].key = NULL;
    return 1;
}
