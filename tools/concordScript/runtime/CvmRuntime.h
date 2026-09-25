// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORDCVM_RUNTIME_H
#define CONCORDCVM_RUNTIME_H

/**
 * C ABI of the Concord Visual Machine standard library.
 *
 * ConcordScript's \c @cvm blocks lower to LLVM IR, which can only call
 * functions with a stable C ABI. This header is that boundary: every entry
 * point below is a plain C function taking and returning \c int64_t, so the
 * JIT can bind it as an absolute symbol and an AOT object can reference it by
 * name.
 *
 * Conventions every function follows:
 *
 * - Everything is \c int64_t. CVM has one value type, so a handle, a length,
 *   a payload and a status are all the same width; the signature carries the
 *   meaning.
 * - Container, thread, mutex and counter values are opaque handles, never
 *   pointers. A handle is validated on every use, so a stale or forged handle
 *   degrades to the documented failure value instead of dereferencing garbage.
 * - No function ever aborts on a bad handle. Host errors are reported through
 *   the return value.
 *
 * The matching CX-visible names are declared in CvmHostRegistry.cpp; that
 * table is the single place where a CX name is bound to a symbol here.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Prints one CVM value as a decimal line. Returns the value unchanged. */
int64_t cvm_print(int64_t value);

/**
 * Link alias of \c cvm_print under the unprefixed name.
 *
 * This is a deliberate exception to the \c cvm_ prefix. A @cvm body has always
 * written \c print(...), so the emitted LLVM IR has always referenced the
 * external symbol \c print, and an AOT object built before this library
 * existed still resolves against that name. New host code should call
 * \c cvm_print.
 */
int64_t print(int64_t value);

/**
 * Prints one CVM f64 as a decimal line with up to six significant digits, and
 * returns it unchanged.
 *
 * Separate from \c cvm_print because the two print differently: \c cvm_print
 * has always written an exact integer, and a float that happens to hold a whole
 * number should not start sprouting a decimal point in existing output.
 */
double cvm_print_f64(double value);

/*
 * Strings.
 *
 * A CVM string is a NUL-terminated byte buffer preceded by an ownership word,
 * so cvm_str_free is safe on a literal as well as on a value these functions
 * allocated. Only the functions below and the compiler's own literals may
 * produce one; passing a pointer that did not come from either is undefined.
 */

/** Returns the byte length of \p text. */
int64_t cvm_str_len(const char* text);

/** Returns 1 when the two strings hold the same bytes. */
int64_t cvm_str_eq(const char* left, const char* right);

/** Returns a newly allocated concatenation, or NULL on failure. */
char* cvm_str_concat(const char* left, const char* right);

/** Releases an owned string. Returns 1 when it freed, 0 for a literal. */
int64_t cvm_str_free(char* text);

/** Returns the byte offset of \p needle in \p haystack, or -1. */
int64_t cvm_str_find(const char* haystack, const char* needle);

/** Returns a newly allocated slice of \p count bytes from \p start. */
char* cvm_str_sub(const char* text, int64_t start, int64_t count);

/** Returns a newly allocated decimal rendering of \p value. */
char* cvm_str_from_int(int64_t value);

/** Returns a newly allocated rendering of \p value, as cvm_print_f64 writes it. */
char* cvm_str_from_float(double value);

/** Parses a leading integer, returning 0 when there is none. */
int64_t cvm_str_to_int(const char* text);

/** Parses a leading number, returning 0.0 when there is none. */
double cvm_str_to_float(const char* text);

/** Returns the byte at \p index, or -1 when out of range. */
int64_t cvm_str_char_at(const char* text, int64_t index);

/** Prints \p text as a line and returns it unchanged. */
const char* cvm_print_str(const char* text);

/*
 * Text, measured the way text is written rather than the way it is stored.
 *
 * These count and index Unicode characters in a UTF-8 string, which is what an
 * author means by the length of one. They are separate from cvm_str_len and
 * cvm_str_char_at, which stay byte-oriented: bytes are what is actually stored,
 * and a substring by byte offset is a legitimate thing to want.
 */

/** Returns the number of UTF-8 characters, or 0 for a null pointer. */
int64_t cvm_str_len_utf8(const char* text);

/** Returns the code point at character \p index, or -1 when out of range. */
int64_t cvm_str_at_utf8(const char* text, int64_t index);

/** Returns a newly allocated string holding code point \p code, or NULL. */
char* cvm_str_from_code(int64_t code);

/** Returns a newly allocated copy with ASCII letters folded to upper case. */
char* cvm_str_upper(const char* text);

/** Returns a newly allocated copy with ASCII letters folded to lower case. */
char* cvm_str_lower(const char* text);

/*
 * Lists of strings.
 *
 * A pointer is 64 bits, so these store into the same container the i64 list
 * uses; new, free, size and clear name the i64 symbols directly. Nothing is
 * copied -- the list holds the pointer it was given, so an entry must be a
 * literal or outlive the list, exactly as in C.
 */

int64_t cvm_slist_push(int64_t list, const char* text);
const char* cvm_slist_get(int64_t list, int64_t index);
const char* cvm_slist_set(int64_t list, int64_t index, const char* text);
const char* cvm_slist_pop(int64_t list);
int64_t cvm_slist_insert(int64_t list, int64_t index, const char* text);
const char* cvm_slist_remove(int64_t list, int64_t index);
int64_t cvm_slist_index_of(int64_t list, const char* text);
int64_t cvm_slist_contains(int64_t list, const char* text);

/** Returns every entry joined by \p separator as one newly allocated string. */
char* cvm_slist_join(int64_t list, const char* separator);

/*
 * Containers whose slots are 4 bytes.
 *
 * These do not share the 64-bit container, because a slot being four bytes is
 * the entire point of them. They exist for data that has to match the engine's
 * own precision -- vertex positions, per-instance parameters, anything destined
 * for a GPU buffer. Arithmetic in a script should still be f64.
 */

int64_t cvm_list32_new(void);
int64_t cvm_list32_free(int64_t list);
int64_t cvm_list32_size(int64_t list);
int64_t cvm_list32_clear(int64_t list);
int64_t cvm_list32_push(int64_t list, double value);
double cvm_list32_pop(int64_t list);
double cvm_list32_get(int64_t list, int64_t index);
double cvm_list32_set(int64_t list, int64_t index, double value);
int64_t cvm_list32_insert(int64_t list, int64_t index, double value);
double cvm_list32_remove(int64_t list, int64_t index);
int64_t cvm_list32_index_of(int64_t list, double value);
int64_t cvm_list32_contains(int64_t list, double value);

int64_t cvm_array32_new(int64_t length);
int64_t cvm_array32_free(int64_t array);
int64_t cvm_array32_size(int64_t array);
double cvm_array32_get(int64_t array, int64_t index);
double cvm_array32_set(int64_t array, int64_t index, double value);
int64_t cvm_array32_fill(int64_t array, double value);
double cvm_array32_sum(int64_t array);
int64_t cvm_array32_copy(int64_t array);
int64_t cvm_array32_reverse(int64_t array);

/** Stores \p value and returns the value it replaced. */
int64_t cvm_counter_set(int64_t counter, int64_t value);

/** Creates an empty growable list. Returns a handle, or 0 on failure. */
int64_t cvm_list_new(void);

/** Releases a list and its storage. Returns 1 on success, 0 otherwise. */
int64_t cvm_list_free(int64_t list);

/** Returns the element count, or 0 for an invalid handle. */
int64_t cvm_list_size(int64_t list);

/** Removes every element, keeping the allocation. Returns the new size. */
int64_t cvm_list_clear(int64_t list);

/** Appends one value. Returns the new element count. */
int64_t cvm_list_push(int64_t list, int64_t value);

/** Removes and returns the last value, or 0 when the list is empty. */
int64_t cvm_list_pop(int64_t list);

/** Returns the value at \p index, or 0 when out of range. */
int64_t cvm_list_get(int64_t list, int64_t index);

/** Overwrites \p index. Returns the stored value, or 0 when out of range. */
int64_t cvm_list_set(int64_t list, int64_t index, int64_t value);

/** Inserts before \p index; an index at or past the end appends. */
int64_t cvm_list_insert(int64_t list, int64_t index, int64_t value);

/** Removes \p index and returns the value that was there, or 0. */
int64_t cvm_list_remove(int64_t list, int64_t index);

/** Returns 1 when \p value occurs in the list, 0 otherwise. */
int64_t cvm_list_contains(int64_t list, int64_t value);

/** Returns the first index of \p value, or -1 when it does not occur. */
int64_t cvm_list_index_of(int64_t list, int64_t value);

/*
 * The f64 view of the containers.
 *
 * A list slot is 64 bits and an f64 is 64 bits, so these share one container
 * with the i64 accessors rather than duplicating it: a list and an flist are
 * the same object. The accessors that need no reinterpretation -- new, free,
 * size, clear, copy, reverse -- are registered against the i64 symbols and
 * have no separate entry point here.
 */

int64_t cvm_flist_push(int64_t list, double value);
double cvm_flist_pop(int64_t list);
double cvm_flist_get(int64_t list, int64_t index);
double cvm_flist_set(int64_t list, int64_t index, double value);
int64_t cvm_flist_insert(int64_t list, int64_t index, double value);
double cvm_flist_remove(int64_t list, int64_t index);
int64_t cvm_flist_contains(int64_t list, double value);
int64_t cvm_flist_index_of(int64_t list, double value);
double cvm_farray_get(int64_t array, int64_t index);
double cvm_farray_set(int64_t array, int64_t index, double value);
int64_t cvm_farray_fill(int64_t array, double value);
double cvm_farray_sum(int64_t array);

/** Creates a zero-filled array of \p length elements, or 0 on failure. */
int64_t cvm_array_new(int64_t length);

/** Releases an array. Returns 1 on success, 0 otherwise. */
int64_t cvm_array_free(int64_t array);

/** Returns the element count, or 0 for an invalid handle. */
int64_t cvm_array_size(int64_t array);

/** Returns the value at \p index, or 0 when out of range. */
int64_t cvm_array_get(int64_t array, int64_t index);

/** Overwrites \p index. Returns the stored value, or 0 when out of range. */
int64_t cvm_array_set(int64_t array, int64_t index, int64_t value);

/** Sets every element to \p value. Returns the element count. */
int64_t cvm_array_fill(int64_t array, int64_t value);

/** Returns the sum of all elements, or 0 for an invalid handle. */
int64_t cvm_array_sum(int64_t array);

/** Returns a new array holding a copy, or 0 on failure. */
int64_t cvm_array_copy(int64_t array);

/** Reverses the array in place. Returns the element count. */
int64_t cvm_array_reverse(int64_t array);

/**
 * Starts \p entry on a new thread.
 *
 * \p entry is the address of a no-argument CVM function returning \c int64_t,
 * which is exactly the shape every \c @cvm block already has. Returns a thread
 * handle for \c cvm_thread_join, or 0 when the thread could not start.
 */
int64_t cvm_thread_spawn(int64_t entry);

/**
 * Starts \p entry on a new thread, passing it \p arg.
 *
 * \p entry is the address of an i64(i64) CVM function -- a @cvm function with
 * one i64 parameter. This is how two threads reach the same data: there are no
 * closures and no shared globals, so anything they are to share has to be
 * handed over, and a handle is the only thing that can be.
 */
int64_t cvm_thread_spawn_arg(int64_t entry, int64_t arg);

/** Waits for a thread and returns the value its entry function returned. */
int64_t cvm_thread_join(int64_t thread);

/** Offers the rest of the current time slice to another runnable thread. */
int64_t cvm_thread_yield(void);

/** Returns the number of logical processors available to the process. */
int64_t cvm_thread_count(void);

/** Creates an unlocked recursive-free mutex. Returns a handle, or 0. */
int64_t cvm_mutex_new(void);

/** Releases a mutex. Returns 1 on success, 0 otherwise. */
int64_t cvm_mutex_free(int64_t mutex);

/** Blocks until the mutex is held. Returns 1 on success, 0 otherwise. */
int64_t cvm_mutex_lock(int64_t mutex);

/** Releases the mutex. Returns 1 when it was held by this thread. */
int64_t cvm_mutex_unlock(int64_t mutex);

/** Creates an atomic counter starting at \p initial, or 0 on failure. */
int64_t cvm_counter_new(int64_t initial);

/** Releases a counter. Returns 1 on success, 0 otherwise. */
int64_t cvm_counter_free(int64_t counter);

/** Adds \p delta atomically and returns the new value. */
int64_t cvm_counter_add(int64_t counter, int64_t delta);

/** Returns the current value without modifying it. */
int64_t cvm_counter_get(int64_t counter);

/** Returns monotonic milliseconds since an unspecified epoch. */
int64_t cvm_sys_time_ms(void);

/** Suspends the calling thread for \p milliseconds. Returns the argument. */
int64_t cvm_sys_sleep_ms(int64_t milliseconds);

/** Returns the number of logical processors on the machine. */
int64_t cvm_sys_cpu_count(void);

/** Seeds the process-wide pseudo-random generator. Returns the seed used. */
int64_t cvm_sys_random_seed(int64_t seed);

/** Returns the next pseudo-random 63-bit value. */
int64_t cvm_sys_random_next(void);

/** Returns a pseudo-random value in [low, high); returns low when empty. */
int64_t cvm_sys_random_range(int64_t low, int64_t high);

/** Returns a uniform draw in [0, 1). */
double cvm_sys_random_float(void);

/** Terminates the process with \p code. Never returns. */
int64_t cvm_sys_exit(int64_t code);

/*
 * Text files. Paths are bytes as the host fopen sees them; contents are CVM
 * strings, so a NUL in the file truncates what a script can later read.
 * file_read returns NULL when the file cannot be opened. Writes replace the
 * whole file.
 */

/** Reads the whole file as a newly allocated string, or NULL on failure. */
char* cvm_file_read(const char* path);

/** Writes \p text over \p path. Returns 1 on success. */
int64_t cvm_file_write(const char* path, const char* text);

/** Returns 1 when \p path can be opened for reading. */
int64_t cvm_file_exists(const char* path);

/*
 * Scalar maths. Angles are radians; convert with radians(...) / degrees(...).
 */

double cvm_pi(void);
double cvm_tau(void);
double cvm_sin(double x);
double cvm_cos(double x);
double cvm_tan(double x);
double cvm_asin(double x);
double cvm_acos(double x);
double cvm_atan(double x);
double cvm_atan2(double y, double x);
double cvm_sqrt(double x);
double cvm_pow(double base, double exponent);
double cvm_hypot(double x, double y);
double cvm_floor(double x);
double cvm_ceil(double x);
double cvm_round(double x);
double cvm_abs(double x);
double cvm_min(double left, double right);
double cvm_max(double left, double right);
double cvm_sign(double x);
double cvm_fract(double x);
double cvm_radians(double degrees);
double cvm_degrees(double radians);
double cvm_clamp(double value, double low, double high);
double cvm_lerp(double start, double end, double t);
int64_t cvm_iabs(int64_t value);
int64_t cvm_imin(int64_t left, int64_t right);
int64_t cvm_imax(int64_t left, int64_t right);

/**
 * Creates an empty i64-to-i64 map. Returns a handle, or 0 on allocation
 * failure.
 */
int64_t cvm_map_new(void);

/** Releases a map. Returns 1 on success, 0 if the handle is not a map. */
int64_t cvm_map_free(int64_t map);

/** Number of stored keys, or -1 if the handle is not a map. */
int64_t cvm_map_len(int64_t map);

/**
 * Inserts or overwrites \p key. Returns 1 on success, 0 if the handle is
 * not a map.
 */
int64_t cvm_map_set(int64_t map, int64_t key, int64_t value);

/**
 * Returns the value bound to \p key, or 0 if the key is absent or the
 * handle is not a map. Call cvm_map_has to tell those two cases apart.
 */
int64_t cvm_map_get(int64_t map, int64_t key);

/** Returns 1 if \p key is present, 0 otherwise. */
int64_t cvm_map_has(int64_t map, int64_t key);

/** Removes \p key. Returns 1 if it was present, 0 otherwise. */
int64_t cvm_map_remove(int64_t map, int64_t key);

/**
 * Creates an empty string-to-i64 map. Returns a handle, or 0 on allocation
 * failure.
 */
int64_t cvm_smap_new(void);

/** Releases a string map. Returns 1 on success, 0 if the handle is not one. */
int64_t cvm_smap_free(int64_t map);

/** Number of stored keys, or -1 if the handle is not a string map. */
int64_t cvm_smap_len(int64_t map);

/**
 * Inserts or overwrites \p key. The map copies the bytes; the caller may
 * free the original. Returns 1 on success, 0 if the handle is not a map
 * or \p key is null.
 */
int64_t cvm_smap_set(int64_t map, const char* key, int64_t value);

/**
 * Returns the value bound to \p key, or 0 if the key is absent, null, or
 * the handle is not a string map. Call cvm_smap_has to tell those cases
 * apart.
 */
int64_t cvm_smap_get(int64_t map, const char* key);

/** Returns 1 if \p key is present, 0 otherwise. */
int64_t cvm_smap_has(int64_t map, const char* key);

/** Removes \p key. Returns 1 if it was present, 0 otherwise. */
int64_t cvm_smap_remove(int64_t map, const char* key);

#ifdef __cplusplus
}
#endif

#endif // CONCORDCVM_RUNTIME_H
