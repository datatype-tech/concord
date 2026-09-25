// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHostRegistry.h"

#include "CvmRuntime.h"

#include <unordered_map>

/**
 * The registered host surface.
 *
 * The parameter arrays are declared once per distinct arity so a row reads as
 * a signature rather than as pointer arithmetic. Rows are grouped by the
 * runtime unit that implements them and otherwise kept in the order a reader
 * would expect to learn them: console, list, array, thread, synchronisation,
 * system.
 */
namespace {

using ConcordScript::CvmCallbackSlot;
using ConcordScript::CvmHostSignature;
using ConcordScript::CvmHostType;

constexpr CvmHostType kI64[] = {CvmHostType::I64};
constexpr CvmHostType kF64[] = {CvmHostType::F64};
constexpr CvmHostType kStr[] = {CvmHostType::Str};
constexpr CvmHostType kStrStr[] = {CvmHostType::Str, CvmHostType::Str};
constexpr CvmHostType kStrI64[] = {CvmHostType::Str, CvmHostType::I64};
constexpr CvmHostType kStrI64I64[] = {CvmHostType::Str, CvmHostType::I64, CvmHostType::I64};
constexpr CvmHostType kI64F64[] = {CvmHostType::I64, CvmHostType::F64};
constexpr CvmHostType kI64F64F64[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kI64I64F64[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::F64};
constexpr CvmHostType kI64Str[] = {CvmHostType::I64, CvmHostType::Str};
constexpr CvmHostType kI64StrI64[] = {CvmHostType::I64, CvmHostType::Str, CvmHostType::I64};
constexpr CvmHostType kI64I64Str[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::Str};
constexpr CvmHostType kF64x3[] = {CvmHostType::F64, CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kI64I64[] = {CvmHostType::I64, CvmHostType::I64};
constexpr CvmHostType kI64I64I64[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::I64};
constexpr CvmHostType kF64F64[] = {CvmHostType::F64, CvmHostType::F64};

/**
 * reinterpret_cast from a function pointer needs this to stay well-formed.
 *
 * It is not a constant expression, which is why the table below is const
 * rather than constexpr and is initialized at load time.
 */
template <typename Function>
void* HostAddress(Function function)
{
    return reinterpret_cast<void*>(function);
}

const CvmHostSignature kHostFunctions[] = {
    // Keeps the unprefixed link name on purpose: the emitted IR and every AOT
    // object built so far reference the external symbol "print" (see the
    // runtime header). The CX-visible name and the symbol coincide here.
    {"print", "print", CvmHostType::I64, kI64, 1, HostAddress(&print)},
    {"print_float", "cvm_print_f64", CvmHostType::F64, kF64, 1, HostAddress(&cvm_print_f64)},
    {"print_str", "cvm_print_str", CvmHostType::Str, kStr, 1, HostAddress(&cvm_print_str)},

    {"str_len", "cvm_str_len", CvmHostType::I64, kStr, 1, HostAddress(&cvm_str_len)},
    {"str_eq", "cvm_str_eq", CvmHostType::I64, kStrStr, 2, HostAddress(&cvm_str_eq)},
    {"str_concat", "cvm_str_concat", CvmHostType::Str, kStrStr, 2, HostAddress(&cvm_str_concat)},
    {"str_free", "cvm_str_free", CvmHostType::I64, kStr, 1, HostAddress(&cvm_str_free)},
    {"str_find", "cvm_str_find", CvmHostType::I64, kStrStr, 2, HostAddress(&cvm_str_find)},
    {"str_sub", "cvm_str_sub", CvmHostType::Str, kStrI64I64, 3, HostAddress(&cvm_str_sub)},
    {"str_from_int", "cvm_str_from_int", CvmHostType::Str, kI64, 1, HostAddress(&cvm_str_from_int)},
    {"str_from_float", "cvm_str_from_float", CvmHostType::Str, kF64, 1,
     HostAddress(&cvm_str_from_float)},
    {"str_to_int", "cvm_str_to_int", CvmHostType::I64, kStr, 1, HostAddress(&cvm_str_to_int)},
    {"str_to_float", "cvm_str_to_float", CvmHostType::F64, kStr, 1,
     HostAddress(&cvm_str_to_float)},
    {"str_char_at", "cvm_str_char_at", CvmHostType::I64, kStrI64, 2, HostAddress(&cvm_str_char_at)},

    {"list_new", "cvm_list_new", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_list_new)},
    {"list_free", "cvm_list_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_free)},
    {"list_size", "cvm_list_size", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_size)},
    {"list_clear", "cvm_list_clear", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_clear)},
    {"list_push", "cvm_list_push", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_list_push)},
    {"list_pop", "cvm_list_pop", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_pop)},
    {"list_get", "cvm_list_get", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_list_get)},
    {"list_set", "cvm_list_set", CvmHostType::I64, kI64I64I64, 3, HostAddress(&cvm_list_set)},
    {"list_insert", "cvm_list_insert", CvmHostType::I64, kI64I64I64, 3, HostAddress(&cvm_list_insert)},
    {"list_remove", "cvm_list_remove", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_list_remove)},
    {"list_contains", "cvm_list_contains", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_list_contains)},
    {"list_index_of", "cvm_list_index_of", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_list_index_of)},

    {"array_new", "cvm_array_new", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_new)},
    {"array_free", "cvm_array_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_free)},
    {"array_size", "cvm_array_size", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_size)},
    {"array_get", "cvm_array_get", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_array_get)},
    {"array_set", "cvm_array_set", CvmHostType::I64, kI64I64I64, 3, HostAddress(&cvm_array_set)},
    {"array_fill", "cvm_array_fill", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_array_fill)},
    {"array_sum", "cvm_array_sum", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_sum)},
    {"array_copy", "cvm_array_copy", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_copy)},
    {"array_reverse", "cvm_array_reverse", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_reverse)},

    // The f64 view. A slot is 64 bits either way, so the container is shared and
    // the aliases below name the same symbol as their i64 counterpart.
    {"flist_new", "cvm_list_new", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_list_new)},
    {"flist_free", "cvm_list_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_free)},
    {"flist_size", "cvm_list_size", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_size)},
    {"flist_clear", "cvm_list_clear", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_clear)},
    {"flist_push", "cvm_flist_push", CvmHostType::I64, kI64F64, 2, HostAddress(&cvm_flist_push)},
    {"flist_pop", "cvm_flist_pop", CvmHostType::F64, kI64, 1, HostAddress(&cvm_flist_pop)},
    {"flist_get", "cvm_flist_get", CvmHostType::F64, kI64I64, 2, HostAddress(&cvm_flist_get)},
    {"flist_set", "cvm_flist_set", CvmHostType::F64, kI64I64F64, 3, HostAddress(&cvm_flist_set)},
    {"flist_insert", "cvm_flist_insert", CvmHostType::I64, kI64I64F64, 3,
     HostAddress(&cvm_flist_insert)},
    {"flist_remove", "cvm_flist_remove", CvmHostType::F64, kI64I64, 2,
     HostAddress(&cvm_flist_remove)},
    {"flist_contains", "cvm_flist_contains", CvmHostType::I64, kI64F64, 2,
     HostAddress(&cvm_flist_contains)},
    {"flist_index_of", "cvm_flist_index_of", CvmHostType::I64, kI64F64, 2,
     HostAddress(&cvm_flist_index_of)},

    {"farray_new", "cvm_array_new", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_new)},
    {"farray_free", "cvm_array_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_free)},
    {"farray_size", "cvm_array_size", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_size)},
    {"farray_get", "cvm_farray_get", CvmHostType::F64, kI64I64, 2, HostAddress(&cvm_farray_get)},
    {"farray_set", "cvm_farray_set", CvmHostType::F64, kI64I64F64, 3, HostAddress(&cvm_farray_set)},
    {"farray_fill", "cvm_farray_fill", CvmHostType::I64, kI64F64, 2, HostAddress(&cvm_farray_fill)},
    {"farray_sum", "cvm_farray_sum", CvmHostType::F64, kI64, 1, HostAddress(&cvm_farray_sum)},
    {"farray_copy", "cvm_array_copy", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array_copy)},
    {"farray_reverse", "cvm_array_reverse", CvmHostType::I64, kI64, 1,
     HostAddress(&cvm_array_reverse)},

    {"thread_spawn", "cvm_thread_spawn", CvmHostType::I64, kI64, 1, HostAddress(&cvm_thread_spawn)},
    {"thread_spawn_arg", "cvm_thread_spawn_arg", CvmHostType::I64, kI64I64, 2,
     HostAddress(&cvm_thread_spawn_arg)},
    {"thread_join", "cvm_thread_join", CvmHostType::I64, kI64, 1, HostAddress(&cvm_thread_join)},
    {"thread_yield", "cvm_thread_yield", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_thread_yield)},
    {"thread_count", "cvm_thread_count", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_thread_count)},

    {"mutex_new", "cvm_mutex_new", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_mutex_new)},
    {"mutex_free", "cvm_mutex_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_mutex_free)},
    {"mutex_lock", "cvm_mutex_lock", CvmHostType::I64, kI64, 1, HostAddress(&cvm_mutex_lock)},
    {"mutex_unlock", "cvm_mutex_unlock", CvmHostType::I64, kI64, 1, HostAddress(&cvm_mutex_unlock)},
    {"counter_new", "cvm_counter_new", CvmHostType::I64, kI64, 1, HostAddress(&cvm_counter_new)},
    {"counter_free", "cvm_counter_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_counter_free)},
    {"counter_add", "cvm_counter_add", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_counter_add)},
    {"counter_get", "cvm_counter_get", CvmHostType::I64, kI64, 1, HostAddress(&cvm_counter_get)},

    {"sys_time_ms", "cvm_sys_time_ms", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_sys_time_ms)},
    {"sys_sleep_ms", "cvm_sys_sleep_ms", CvmHostType::I64, kI64, 1, HostAddress(&cvm_sys_sleep_ms)},
    {"sys_cpu_count", "cvm_sys_cpu_count", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_sys_cpu_count)},
    {"sys_random_seed", "cvm_sys_random_seed", CvmHostType::I64, kI64, 1, HostAddress(&cvm_sys_random_seed)},
    {"sys_random_next", "cvm_sys_random_next", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_sys_random_next)},
    {"sys_random_range", "cvm_sys_random_range", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_sys_random_range)},
    {"sys_random_float", "cvm_sys_random_float", CvmHostType::F64, nullptr, 0,
     HostAddress(&cvm_sys_random_float)},
    {"sys_exit", "cvm_sys_exit", CvmHostType::I64, kI64, 1, HostAddress(&cvm_sys_exit)},

    {"file_read", "cvm_file_read", CvmHostType::Str, kStr, 1, HostAddress(&cvm_file_read)},
    {"file_write", "cvm_file_write", CvmHostType::I64, kStrStr, 2, HostAddress(&cvm_file_write)},
    {"file_exists", "cvm_file_exists", CvmHostType::I64, kStr, 1, HostAddress(&cvm_file_exists)},

    {"pi", "cvm_pi", CvmHostType::F64, nullptr, 0, HostAddress(&cvm_pi)},
    {"tau", "cvm_tau", CvmHostType::F64, nullptr, 0, HostAddress(&cvm_tau)},
    {"sin", "cvm_sin", CvmHostType::F64, kF64, 1, HostAddress(&cvm_sin)},
    {"cos", "cvm_cos", CvmHostType::F64, kF64, 1, HostAddress(&cvm_cos)},
    {"tan", "cvm_tan", CvmHostType::F64, kF64, 1, HostAddress(&cvm_tan)},
    {"asin", "cvm_asin", CvmHostType::F64, kF64, 1, HostAddress(&cvm_asin)},
    {"acos", "cvm_acos", CvmHostType::F64, kF64, 1, HostAddress(&cvm_acos)},
    {"atan", "cvm_atan", CvmHostType::F64, kF64, 1, HostAddress(&cvm_atan)},
    {"atan2", "cvm_atan2", CvmHostType::F64, kF64F64, 2, HostAddress(&cvm_atan2)},
    {"sqrt", "cvm_sqrt", CvmHostType::F64, kF64, 1, HostAddress(&cvm_sqrt)},
    {"pow", "cvm_pow", CvmHostType::F64, kF64F64, 2, HostAddress(&cvm_pow)},
    {"hypot", "cvm_hypot", CvmHostType::F64, kF64F64, 2, HostAddress(&cvm_hypot)},
    {"floor", "cvm_floor", CvmHostType::F64, kF64, 1, HostAddress(&cvm_floor)},
    {"ceil", "cvm_ceil", CvmHostType::F64, kF64, 1, HostAddress(&cvm_ceil)},
    {"round", "cvm_round", CvmHostType::F64, kF64, 1, HostAddress(&cvm_round)},
    {"abs", "cvm_abs", CvmHostType::F64, kF64, 1, HostAddress(&cvm_abs)},
    {"min", "cvm_min", CvmHostType::F64, kF64F64, 2, HostAddress(&cvm_min)},
    {"max", "cvm_max", CvmHostType::F64, kF64F64, 2, HostAddress(&cvm_max)},
    {"sign", "cvm_sign", CvmHostType::F64, kF64, 1, HostAddress(&cvm_sign)},
    {"fract", "cvm_fract", CvmHostType::F64, kF64, 1, HostAddress(&cvm_fract)},
    {"radians", "cvm_radians", CvmHostType::F64, kF64, 1, HostAddress(&cvm_radians)},
    {"degrees", "cvm_degrees", CvmHostType::F64, kF64, 1, HostAddress(&cvm_degrees)},
    {"clamp", "cvm_clamp", CvmHostType::F64, kF64x3, 3, HostAddress(&cvm_clamp)},
    {"lerp", "cvm_lerp", CvmHostType::F64, kF64x3, 3, HostAddress(&cvm_lerp)},
    {"iabs", "cvm_iabs", CvmHostType::I64, kI64, 1, HostAddress(&cvm_iabs)},
    {"imin", "cvm_imin", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_imin)},
    {"imax", "cvm_imax", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_imax)},

    // Text as it is written, rather than as it is stored.
    {"str_len_utf8", "cvm_str_len_utf8", CvmHostType::I64, kStr, 1,
     HostAddress(&cvm_str_len_utf8)},
    {"str_at_utf8", "cvm_str_at_utf8", CvmHostType::I64, kStrI64, 2,
     HostAddress(&cvm_str_at_utf8)},
    {"str_from_code", "cvm_str_from_code", CvmHostType::Str, kI64, 1,
     HostAddress(&cvm_str_from_code)},
    {"str_upper", "cvm_str_upper", CvmHostType::Str, kStr, 1, HostAddress(&cvm_str_upper)},
    {"str_lower", "cvm_str_lower", CvmHostType::Str, kStr, 1, HostAddress(&cvm_str_lower)},

    // Lists of strings. A pointer is 64 bits, so the container aliases are the
    // i64 ones; only the accessors that know the slots hold text are separate.
    {"slist_new", "cvm_list_new", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_list_new)},
    {"slist_free", "cvm_list_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_free)},
    {"slist_size", "cvm_list_size", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_size)},
    {"slist_clear", "cvm_list_clear", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list_clear)},
    {"slist_push", "cvm_slist_push", CvmHostType::I64, kI64Str, 2, HostAddress(&cvm_slist_push)},
    {"slist_get", "cvm_slist_get", CvmHostType::Str, kI64I64, 2, HostAddress(&cvm_slist_get)},
    {"slist_set", "cvm_slist_set", CvmHostType::Str, kI64I64Str, 3, HostAddress(&cvm_slist_set)},
    {"slist_pop", "cvm_slist_pop", CvmHostType::Str, kI64, 1, HostAddress(&cvm_slist_pop)},
    {"slist_insert", "cvm_slist_insert", CvmHostType::I64, kI64I64Str, 3,
     HostAddress(&cvm_slist_insert)},
    {"slist_remove", "cvm_slist_remove", CvmHostType::Str, kI64I64, 2,
     HostAddress(&cvm_slist_remove)},
    {"slist_index_of", "cvm_slist_index_of", CvmHostType::I64, kI64Str, 2,
     HostAddress(&cvm_slist_index_of)},
    {"slist_contains", "cvm_slist_contains", CvmHostType::I64, kI64Str, 2,
     HostAddress(&cvm_slist_contains)},
    {"slist_join", "cvm_slist_join", CvmHostType::Str, kI64Str, 2,
     HostAddress(&cvm_slist_join)},

    // Containers whose slots are four bytes, for data that has to match the
    // engine's own precision.
    {"list32_new", "cvm_list32_new", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_list32_new)},
    {"list32_free", "cvm_list32_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list32_free)},
    {"list32_size", "cvm_list32_size", CvmHostType::I64, kI64, 1, HostAddress(&cvm_list32_size)},
    {"list32_clear", "cvm_list32_clear", CvmHostType::I64, kI64, 1,
     HostAddress(&cvm_list32_clear)},
    {"list32_push", "cvm_list32_push", CvmHostType::I64, kI64F64, 2,
     HostAddress(&cvm_list32_push)},
    {"list32_pop", "cvm_list32_pop", CvmHostType::F64, kI64, 1, HostAddress(&cvm_list32_pop)},
    {"list32_get", "cvm_list32_get", CvmHostType::F64, kI64I64, 2, HostAddress(&cvm_list32_get)},
    {"list32_set", "cvm_list32_set", CvmHostType::F64, kI64I64F64, 3,
     HostAddress(&cvm_list32_set)},
    {"list32_insert", "cvm_list32_insert", CvmHostType::I64, kI64I64F64, 3,
     HostAddress(&cvm_list32_insert)},
    {"list32_remove", "cvm_list32_remove", CvmHostType::F64, kI64I64, 2,
     HostAddress(&cvm_list32_remove)},
    {"list32_index_of", "cvm_list32_index_of", CvmHostType::I64, kI64F64, 2,
     HostAddress(&cvm_list32_index_of)},
    {"list32_contains", "cvm_list32_contains", CvmHostType::I64, kI64F64, 2,
     HostAddress(&cvm_list32_contains)},

    {"array32_new", "cvm_array32_new", CvmHostType::I64, kI64, 1, HostAddress(&cvm_array32_new)},
    {"array32_free", "cvm_array32_free", CvmHostType::I64, kI64, 1,
     HostAddress(&cvm_array32_free)},
    {"array32_size", "cvm_array32_size", CvmHostType::I64, kI64, 1,
     HostAddress(&cvm_array32_size)},
    {"array32_get", "cvm_array32_get", CvmHostType::F64, kI64I64, 2,
     HostAddress(&cvm_array32_get)},
    {"array32_set", "cvm_array32_set", CvmHostType::F64, kI64I64F64, 3,
     HostAddress(&cvm_array32_set)},
    {"array32_fill", "cvm_array32_fill", CvmHostType::I64, kI64F64, 2,
     HostAddress(&cvm_array32_fill)},
    {"array32_sum", "cvm_array32_sum", CvmHostType::F64, kI64, 1,
     HostAddress(&cvm_array32_sum)},
    {"array32_copy", "cvm_array32_copy", CvmHostType::I64, kI64, 1,
     HostAddress(&cvm_array32_copy)},
    {"array32_reverse", "cvm_array32_reverse", CvmHostType::I64, kI64, 1,
     HostAddress(&cvm_array32_reverse)},

    {"counter_set", "cvm_counter_set", CvmHostType::I64, kI64I64, 2,
     HostAddress(&cvm_counter_set)},

    {"map_new", "cvm_map_new", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_map_new)},
    {"map_free", "cvm_map_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_map_free)},
    {"map_len", "cvm_map_len", CvmHostType::I64, kI64, 1, HostAddress(&cvm_map_len)},
    {"map_set", "cvm_map_set", CvmHostType::I64, kI64I64I64, 3, HostAddress(&cvm_map_set)},
    {"map_get", "cvm_map_get", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_map_get)},
    {"map_has", "cvm_map_has", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_map_has)},
    {"map_remove", "cvm_map_remove", CvmHostType::I64, kI64I64, 2, HostAddress(&cvm_map_remove)},

    {"smap_new", "cvm_smap_new", CvmHostType::I64, nullptr, 0, HostAddress(&cvm_smap_new)},
    {"smap_free", "cvm_smap_free", CvmHostType::I64, kI64, 1, HostAddress(&cvm_smap_free)},
    {"smap_len", "cvm_smap_len", CvmHostType::I64, kI64, 1, HostAddress(&cvm_smap_len)},
    {"smap_set", "cvm_smap_set", CvmHostType::I64, kI64StrI64, 3, HostAddress(&cvm_smap_set)},
    {"smap_get", "cvm_smap_get", CvmHostType::I64, kI64Str, 2, HostAddress(&cvm_smap_get)},
    {"smap_has", "cvm_smap_has", CvmHostType::I64, kI64Str, 2, HostAddress(&cvm_smap_has)},
    {"smap_remove", "cvm_smap_remove", CvmHostType::I64, kI64Str, 2,
     HostAddress(&cvm_smap_remove)},
};

// Mixed signatures: a handle, a key code, an axis and a colour channel are
// integers, while anything physical is a double.
constexpr CvmHostType kSpawnCamera[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                        CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                        CvmHostType::F64};
constexpr CvmHostType kSpawnSun[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                     CvmHostType::F64};
// A box now takes an extent per axis rather than one cube size.
constexpr CvmHostType kSpawnBox[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                     CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                     CvmHostType::F64, CvmHostType::I64, CvmHostType::I64,
                                     CvmHostType::I64};
constexpr CvmHostType kSpawnStaticBody[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                            CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                            CvmHostType::F64};
constexpr CvmHostType kSpawnDynamicBox[] = {
    CvmHostType::I64, CvmHostType::F64, CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
    CvmHostType::F64, CvmHostType::F64, CvmHostType::I64, CvmHostType::I64, CvmHostType::I64,
    CvmHostType::F64};
constexpr CvmHostType kSpawnDynamicSphere[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                               CvmHostType::F64, CvmHostType::F64, CvmHostType::I64,
                                               CvmHostType::I64, CvmHostType::I64, CvmHostType::F64};
constexpr CvmHostType kOverlapSphere[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                          CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSpawnSphere[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                        CvmHostType::F64, CvmHostType::F64, CvmHostType::I64,
                                        CvmHostType::I64, CvmHostType::I64};
constexpr CvmHostType kSpawnPlane[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                       CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                       CvmHostType::I64, CvmHostType::I64, CvmHostType::I64};
constexpr CvmHostType kSpawnPointLight[] = {CvmHostType::I64, CvmHostType::F64,
                                            CvmHostType::F64, CvmHostType::F64,
                                            CvmHostType::I64, CvmHostType::I64,
                                            CvmHostType::I64, CvmHostType::F64,
                                            CvmHostType::F64};
// An entity handle first, then the three components it is being moved by.
constexpr CvmHostType kMoveEntity[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                       CvmHostType::F64};
constexpr CvmHostType kEditColour[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::I64,
                                       CvmHostType::I64};
constexpr CvmHostType kEditMaterial[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                         CvmHostType::F64};
constexpr CvmHostType kEditFov[] = {CvmHostType::I64, CvmHostType::F64};
constexpr CvmHostType kSetAmbient[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::I64,
                                       CvmHostType::I64, CvmHostType::F64};
constexpr CvmHostType kSetGrade[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                     CvmHostType::F64, CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSetFog[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                   CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSetClouds[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                      CvmHostType::F64};
constexpr CvmHostType kRunScene[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::I64,
                                     CvmHostType::Str};
constexpr CvmHostType kStrF64[] = {CvmHostType::Str, CvmHostType::F64};
constexpr CvmHostType kI64I64F64x3[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::F64,
                                        CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kEntityTwoF64[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kEntityFourF64[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                          CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kParticleColor[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::I64,
                                          CvmHostType::I64, CvmHostType::I64, CvmHostType::I64,
                                          CvmHostType::I64};
constexpr CvmHostType kFiveF64[] = {CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                    CvmHostType::F64, CvmHostType::F64};
// scene, model, position(3), scale, colour(3), opacity, absorption, amplitude, speed
constexpr CvmHostType kEntityColor[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                        CvmHostType::F64, CvmHostType::I64, CvmHostType::I64,
                                        CvmHostType::I64};
constexpr CvmHostType kShapeExtent[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::F64,
                                        CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSpawnWater[] = {
    CvmHostType::I64, CvmHostType::I64, CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
    CvmHostType::F64, CvmHostType::I64, CvmHostType::I64, CvmHostType::I64, CvmHostType::F64,
    CvmHostType::F64, CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSpawnSpotLight[] = {
    CvmHostType::I64, CvmHostType::F64, CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
    CvmHostType::F64, CvmHostType::I64, CvmHostType::I64, CvmHostType::I64, CvmHostType::F64,
    CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSpawnRipple[] = {
    CvmHostType::I64, CvmHostType::F64, CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
    CvmHostType::F64, CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSpawnRippleField[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                            CvmHostType::F64};
constexpr CvmHostType kPlayAnimation[] = {CvmHostType::I64, CvmHostType::I64, CvmHostType::F64,
                                         CvmHostType::I64};
constexpr CvmHostType kF64x4[] = {CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                  CvmHostType::F64};
constexpr CvmHostType kUiLabel[] = {CvmHostType::F64, CvmHostType::F64, CvmHostType::Str};
constexpr CvmHostType kUiButton[] = {CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                     CvmHostType::F64, CvmHostType::Str};
constexpr CvmHostType kRaycast[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                    CvmHostType::F64, CvmHostType::F64, CvmHostType::F64,
                                    CvmHostType::F64, CvmHostType::F64};
constexpr CvmHostType kSpawnListener[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                          CvmHostType::F64};
constexpr CvmHostType kSpawnSound[] = {CvmHostType::I64, CvmHostType::F64, CvmHostType::F64,
                                       CvmHostType::F64, CvmHostType::Str, CvmHostType::F64,
                                       CvmHostType::I64, CvmHostType::I64};

/**
 * The Concord engine binding.
 *
 * Addresses are null on purpose: nothing here is linked into the compiler. The
 * symbol names are the engine C ABI declared by
 * concord/include/engine/cvm/CvmApi.h. Lengths are metres and angles are
 * degrees; colour channels stay 0..255 integers.
 */
const CvmHostSignature kHostModuleFunctions[] = {
    {"concord_version", "ConcordCvmVersion", CvmHostType::I64, nullptr, 0, nullptr},

    {"concord_scene_new", "ConcordCvmSceneCreate", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_scene_free", "ConcordCvmSceneDestroy", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_scene_entity_count", "ConcordCvmSceneEntityCount", CvmHostType::I64, kI64, 1,
     nullptr},
    {"concord_scene_has_camera", "ConcordCvmSceneHasMainCamera", CvmHostType::I64, kI64, 1,
     nullptr},

    {"concord_spawn_camera", "ConcordCvmSceneSpawnCamera", CvmHostType::I64, kSpawnCamera, 7,
     nullptr},
    {"concord_spawn_sun", "ConcordCvmSceneSpawnSun", CvmHostType::I64, kSpawnSun, 4, nullptr},
    {"concord_spawn_box", "ConcordCvmSceneSpawnBox", CvmHostType::I64, kSpawnBox, 10, nullptr},
    {"concord_spawn_sphere", "ConcordCvmSceneSpawnSphere", CvmHostType::I64, kSpawnSphere, 8,
     nullptr},
    {"concord_spawn_plane", "ConcordCvmSceneSpawnPlane", CvmHostType::I64, kSpawnPlane, 9,
     nullptr},
    {"concord_spawn_static_body", "ConcordCvmSceneSpawnStaticBody", CvmHostType::I64,
     kSpawnStaticBody, 7, nullptr},
    {"concord_spawn_dynamic_box", "ConcordCvmSceneSpawnDynamicBox", CvmHostType::I64,
     kSpawnDynamicBox, 11, nullptr},
    {"concord_spawn_dynamic_sphere", "ConcordCvmSceneSpawnDynamicSphere", CvmHostType::I64,
     kSpawnDynamicSphere, 9, nullptr},
    {"concord_spawn_point_light", "ConcordCvmSceneSpawnPointLight", CvmHostType::I64,
     kSpawnPointLight, 9, nullptr},
    {"concord_spawn_spot_light", "ConcordCvmSceneSpawnSpotLight", CvmHostType::I64,
     kSpawnSpotLight, 12, nullptr},

    {"concord_entity_set_position", "ConcordCvmEntitySetPosition", CvmHostType::I64, kMoveEntity, 4,
     nullptr},
    {"concord_entity_set_rotation", "ConcordCvmEntitySetRotation", CvmHostType::I64, kMoveEntity, 4,
     nullptr},
    {"concord_entity_translate", "ConcordCvmEntityTranslate", CvmHostType::I64, kMoveEntity, 4,
     nullptr},
    {"concord_entity_position", "ConcordCvmEntityPosition", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_entity_rotation", "ConcordCvmEntityRotation", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_entity_set_scale", "ConcordCvmEntitySetScale", CvmHostType::I64, kMoveEntity, 4,
     nullptr},
    {"concord_entity_scale", "ConcordCvmEntityScale", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_entity_set_visible", "ConcordCvmEntitySetVisible", CvmHostType::I64, kI64I64, 2,
     nullptr},
    {"concord_entity_set_shadow", "ConcordCvmEntitySetShadow", CvmHostType::I64, kI64I64, 2,
     nullptr},
    {"concord_entity_set_color", "ConcordCvmEntitySetColor", CvmHostType::I64, kEditColour, 4,
     nullptr},
    {"concord_entity_set_material", "ConcordCvmEntitySetMaterial", CvmHostType::I64,
     kEditMaterial, 4, nullptr},
    {"concord_entity_set_fov", "ConcordCvmEntitySetFov", CvmHostType::I64, kEditFov, 2, nullptr},
    {"concord_entity_destroy", "ConcordCvmEntityDestroy", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_entity_alive", "ConcordCvmEntityAlive", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_entity_look_at", "ConcordCvmEntityLookAt", CvmHostType::I64, kMoveEntity, 4,
     nullptr},
    {"concord_entity_forward", "ConcordCvmEntityForward", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_entity_set_size", "ConcordCvmEntitySetSize", CvmHostType::I64, kMoveEntity, 4,
     nullptr},
    {"concord_entity_size", "ConcordCvmEntitySize", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_entity_overlaps", "ConcordCvmEntityOverlaps", CvmHostType::I64, kI64I64, 2,
     nullptr},
    {"concord_entity_distance", "ConcordCvmEntityDistance", CvmHostType::F64, kI64I64, 2,
     nullptr},
    {"concord_scene_camera", "ConcordCvmSceneCamera", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_light_set_intensity", "ConcordCvmLightSetIntensity", CvmHostType::I64, kI64F64, 2,
     nullptr},
    {"concord_light_set_color", "ConcordCvmLightSetColor", CvmHostType::I64, kEditColour, 4,
     nullptr},
    {"concord_light_set_angles", "ConcordCvmLightSetAngles", CvmHostType::I64, kEntityTwoF64,
     3, nullptr},
    {"concord_entity_color", "ConcordCvmEntityColor", CvmHostType::I64, kI64I64, 2, nullptr},
    {"concord_entity_material", "ConcordCvmEntityMaterial", CvmHostType::F64, kI64I64, 2,
     nullptr},
    {"concord_entity_fov", "ConcordCvmEntityFov", CvmHostType::F64, kI64, 1, nullptr},
    {"concord_entity_visible", "ConcordCvmEntityVisible", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_entity_shadow", "ConcordCvmEntityShadow", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_light_intensity", "ConcordCvmLightIntensity", CvmHostType::F64, kI64, 1, nullptr},
    {"concord_light_color", "ConcordCvmLightColor", CvmHostType::I64, kI64I64, 2, nullptr},
    {"concord_light_angle", "ConcordCvmLightAngle", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_scene_snapshot", "ConcordCvmSceneSnapshot", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_scene_snapshot_at", "ConcordCvmSceneSnapshotAt", CvmHostType::I64, kI64, 1,
     nullptr},
    {"concord_entity_kind", "ConcordCvmEntityKind", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_entity_set_name", "ConcordCvmEntitySetName", CvmHostType::I64, kI64Str, 2, nullptr},
    {"concord_entity_name", "ConcordCvmEntityName", CvmHostType::Str, kI64, 1, nullptr},
    {"concord_raycast", "ConcordCvmRaycast", CvmHostType::I64, kRaycast, 8, nullptr},
    {"concord_raycast_distance", "ConcordCvmRaycastDistance", CvmHostType::F64, nullptr, 0,
     nullptr},
    {"concord_raycast_point", "ConcordCvmRaycastPoint", CvmHostType::F64, kI64, 1, nullptr},

    {"concord_scene_set_sky", "ConcordCvmSceneSetSky", CvmHostType::I64, kEditColour, 4, nullptr},
    {"concord_scene_set_ambient", "ConcordCvmSceneSetAmbient", CvmHostType::I64, kSetAmbient, 5,
     nullptr},
    {"concord_scene_set_grade", "ConcordCvmSceneSetGrade", CvmHostType::I64, kSetGrade, 6,
     nullptr},
    {"concord_scene_set_fog", "ConcordCvmSceneSetFog", CvmHostType::I64, kSetFog, 5, nullptr},
    {"concord_scene_set_clouds", "ConcordCvmSceneSetClouds", CvmHostType::I64, kSetClouds, 4,
     nullptr},

    // Imported models.
    {"concord_model_load", "ConcordCvmModelLoad", CvmHostType::I64, kStrF64, 2, nullptr},
    {"concord_model_free", "ConcordCvmModelFree", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_last_error", "ConcordCvmLastError", CvmHostType::Str, nullptr, 0, nullptr},
    {"concord_spawn_model", "ConcordCvmSceneSpawnModel", CvmHostType::I64, kI64I64F64x3, 5,
     nullptr},
    {"concord_model_material_count", "ConcordCvmModelMaterialCount", CvmHostType::I64, kI64, 1,
     nullptr},
    {"concord_model_clip_count", "ConcordCvmModelClipCount", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_model_skeleton_count", "ConcordCvmModelSkeletonCount", CvmHostType::I64, kI64, 1,
     nullptr},
    {"concord_model_clip_name", "ConcordCvmModelClipName", CvmHostType::Str, kI64I64, 2, nullptr},
    {"concord_model_clip_duration", "ConcordCvmModelClipDuration", CvmHostType::F64, kI64I64, 2,
     nullptr},
    {"concord_model_find_clip", "ConcordCvmModelFindClip", CvmHostType::I64, kI64Str, 2, nullptr},
    {"concord_model_set_albedo_texture", "ConcordCvmModelSetAlbedoTexture", CvmHostType::I64,
     kI64I64Str, 3, nullptr},
    {"concord_entity_play_animation", "ConcordCvmEntityPlayAnimation", CvmHostType::I64,
     kPlayAnimation, 4, nullptr},
    {"concord_entity_stop_animation", "ConcordCvmEntityStopAnimation", CvmHostType::I64, kI64, 1,
     nullptr},
    {"concord_entity_set_animation_speed", "ConcordCvmEntitySetAnimationSpeed", CvmHostType::I64,
     kI64F64, 2, nullptr},
    {"concord_entity_animation_time", "ConcordCvmEntityAnimationTime", CvmHostType::F64, kI64, 1,
     nullptr},

    // Particles: spawned with defaults, then configured.
    {"concord_spawn_particles", "ConcordCvmSceneSpawnParticles", CvmHostType::I64,
     kEntityColor, 7, nullptr},
    {"concord_particles_set_rate", "ConcordCvmParticlesSetRate", CvmHostType::I64, kI64F64, 2,
     nullptr},
    {"concord_particles_set_speed", "ConcordCvmParticlesSetSpeed", CvmHostType::I64,
     kEntityTwoF64, 3, nullptr},
    {"concord_particles_set_lifetime", "ConcordCvmParticlesSetLifetime", CvmHostType::I64,
     kEntityTwoF64, 3, nullptr},
    {"concord_particles_set_size", "ConcordCvmParticlesSetSize", CvmHostType::I64,
     kEntityTwoF64, 3, nullptr},
    {"concord_particles_set_shape", "ConcordCvmParticlesSetShape", CvmHostType::I64,
     kShapeExtent, 5, nullptr},
    {"concord_particles_set_direction", "ConcordCvmParticlesSetDirection", CvmHostType::I64,
     kEntityFourF64, 5, nullptr},
    {"concord_particles_set_gravity", "ConcordCvmParticlesSetGravity", CvmHostType::I64,
     kEntityFourF64, 5, nullptr},
    {"concord_particles_set_color", "ConcordCvmParticlesSetColor", CvmHostType::I64,
     kParticleColor, 7, nullptr},
    {"concord_particles_set_blend", "ConcordCvmParticlesSetBlend", CvmHostType::I64, kI64I64, 2,
     nullptr},
    {"concord_particles_set_capacity", "ConcordCvmParticlesSetCapacity", CvmHostType::I64,
     kI64I64, 2, nullptr},

    // Water takes its whole surface at spawn: the engine stamps it onto the
    // asset's meshes, so there is no per-entity copy to edit afterwards.
    {"concord_spawn_water", "ConcordCvmSceneSpawnWater", CvmHostType::I64, kSpawnWater, 13,
     nullptr},
    {"concord_spawn_ripple", "ConcordCvmSceneSpawnRipple", CvmHostType::I64, kSpawnRipple, 8,
     nullptr},
    {"concord_spawn_ripple_field", "ConcordCvmSceneSpawnRippleField", CvmHostType::I64,
     kSpawnRippleField, 4, nullptr},

    // Engine systems, recorded here and registered when the game starts.
    {"concord_add_particle_system", "ConcordCvmAddParticleSystem", CvmHostType::I64, nullptr, 0,
     nullptr},
    {"concord_add_water_ripple_system", "ConcordCvmAddWaterRippleSystem", CvmHostType::I64,
     nullptr, 0, nullptr},
    {"concord_add_animation_system", "ConcordCvmAddAnimationSystem", CvmHostType::I64, nullptr, 0,
     nullptr},
    {"concord_add_day_cycle", "ConcordCvmAddDayCycle", CvmHostType::I64, kFiveF64, 5, nullptr},
    {"concord_add_first_person", "ConcordCvmAddFirstPerson", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_add_physics_system", "ConcordCvmAddPhysicsSystem", CvmHostType::I64, nullptr, 0,
     nullptr},
    {"concord_entity_add_body", "ConcordCvmEntityAddBody", CvmHostType::I64, kI64I64F64, 3,
     nullptr},
    {"concord_entity_set_velocity", "ConcordCvmEntitySetVelocity", CvmHostType::I64, kMoveEntity,
     4, nullptr},
    {"concord_entity_velocity", "ConcordCvmEntityVelocity", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_entity_apply_impulse", "ConcordCvmEntityApplyImpulse", CvmHostType::I64,
     kMoveEntity, 4, nullptr},
    {"concord_entity_grounded", "ConcordCvmEntityGrounded", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_overlap_sphere", "ConcordCvmOverlapSphere", CvmHostType::I64, kOverlapSphere, 5,
     nullptr},
    {"concord_physics_step", "ConcordCvmPhysicsStep", CvmHostType::I64, kI64F64, 2, nullptr},
    {"concord_set_gravity", "ConcordCvmSetGravity", CvmHostType::I64, kMoveEntity, 4, nullptr},
    {"concord_gravity", "ConcordCvmGravity", CvmHostType::F64, kI64I64, 2, nullptr},
    {"concord_entity_add_motor", "ConcordCvmEntityAddMotor", CvmHostType::I64, kI64F64F64, 3,
     nullptr},
    {"concord_entity_set_wish_velocity", "ConcordCvmEntitySetWishVelocity", CvmHostType::I64,
     kMoveEntity, 4, nullptr},
    {"concord_entity_wish_velocity", "ConcordCvmEntityWishVelocity", CvmHostType::F64, kI64I64, 2,
     nullptr},
    {"concord_entity_set_angular_velocity", "ConcordCvmEntitySetAngularVelocity", CvmHostType::I64,
     kMoveEntity, 4, nullptr},
    {"concord_entity_angular_velocity", "ConcordCvmEntityAngularVelocity", CvmHostType::F64,
     kI64I64, 2, nullptr},
    {"concord_set_fly_mode", "ConcordCvmSetFlyMode", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_add_audio_system", "ConcordCvmAddAudioSystem", CvmHostType::I64, nullptr, 0,
     nullptr},
    {"concord_spawn_listener", "ConcordCvmSceneSpawnListener", CvmHostType::I64, kSpawnListener, 4,
     nullptr},
    {"concord_entity_add_listener", "ConcordCvmEntityAddListener", CvmHostType::I64, kI64F64, 2,
     nullptr},
    {"concord_spawn_sound", "ConcordCvmSceneSpawnSound", CvmHostType::I64, kSpawnSound, 8,
     nullptr},
    {"concord_entity_play", "ConcordCvmEntityPlay", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_entity_stop", "ConcordCvmEntityStop", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_entity_set_volume", "ConcordCvmEntitySetVolume", CvmHostType::I64, kI64F64, 2,
     nullptr},
    {"concord_entity_volume", "ConcordCvmEntityVolume", CvmHostType::F64, kI64, 1, nullptr},
    {"concord_entity_set_pitch", "ConcordCvmEntitySetPitch", CvmHostType::I64, kI64F64, 2,
     nullptr},
    {"concord_entity_pitch", "ConcordCvmEntityPitch", CvmHostType::F64, kI64, 1, nullptr},
    {"concord_entity_set_range", "ConcordCvmEntitySetRange", CvmHostType::I64, kEntityTwoF64, 3,
     nullptr},

    // The argument is the address of an i64(f64) function; the emitter cannot
    // see through an address, so the contract lives in CvmApi.h.
    {"concord_set_update", "ConcordCvmSetUpdate", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_quit", "ConcordCvmQuit", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_run_scene", "ConcordCvmRunScene", CvmHostType::I64, kRunScene, 4, nullptr},

    {"concord_key_down", "ConcordCvmKeyDown", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_key_pressed", "ConcordCvmKeyPressed", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_key_released", "ConcordCvmKeyReleased", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_mouse_delta", "ConcordCvmMouseDelta", CvmHostType::F64, kI64, 1, nullptr},
    {"concord_mouse_down", "ConcordCvmMouseDown", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_mouse_pressed", "ConcordCvmMousePressed", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_mouse_released", "ConcordCvmMouseReleased", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_mouse_position", "ConcordCvmMousePosition", CvmHostType::F64, kI64, 1, nullptr},
    {"concord_mouse_wheel", "ConcordCvmMouseWheel", CvmHostType::F64, kI64, 1, nullptr},
    {"concord_set_mouse_captured", "ConcordCvmSetMouseCaptured", CvmHostType::I64, kI64, 1,
     nullptr},
    {"concord_mouse_captured", "ConcordCvmMouseCaptured", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_window_width", "ConcordCvmWindowWidth", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_window_height", "ConcordCvmWindowHeight", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_set_overlay", "ConcordCvmSetOverlay", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_overlay", "ConcordCvmOverlay", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_set_title", "ConcordCvmSetTitle", CvmHostType::I64, kStr, 1, nullptr},
    {"concord_set_window_mode", "ConcordCvmSetWindowMode", CvmHostType::I64, kI64, 1, nullptr},
    {"concord_window_mode", "ConcordCvmWindowMode", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_frame_count", "ConcordCvmFrameCount", CvmHostType::I64, nullptr, 0, nullptr},
    {"concord_delta_time", "ConcordCvmDeltaTime", CvmHostType::F64, nullptr, 0, nullptr},
    {"concord_time", "ConcordCvmTime", CvmHostType::F64, nullptr, 0, nullptr},
    {"concord_ui_panel", "ConcordCvmUiPanel", CvmHostType::I64, kF64x4, 4, nullptr},
    {"concord_ui_label", "ConcordCvmUiLabel", CvmHostType::I64, kUiLabel, 3, nullptr},
    {"concord_ui_button", "ConcordCvmUiButton", CvmHostType::I64, kUiButton, 5, nullptr},
};

constexpr CvmHostType kCallbackI64[] = {CvmHostType::I64};
constexpr CvmHostType kCallbackF64[] = {CvmHostType::F64};

/**
 * The callbacks that call back into CVM.
 *
 * Every one returns i64, because that is the only shape the host can call it
 * through: each has a fixed function-pointer type on the other side, and a
 * mismatch is an ABI error the callee cannot survive.
 */
const CvmCallbackSlot kCallbackSlots[] = {
    {"cvm_thread_spawn", 0, nullptr, 0, CvmHostType::I64},
    {"cvm_thread_spawn_arg", 0, kCallbackI64, 1, CvmHostType::I64},
    {"ConcordCvmSetUpdate", 0, kCallbackF64, 1, CvmHostType::I64},
};

/** Built once so lookup is a hash probe rather than a scan per call site. */
const std::unordered_map<std::string_view, const CvmHostSignature*>& HostIndex()
{
    static const std::unordered_map<std::string_view, const CvmHostSignature*> index = [] {
        std::unordered_map<std::string_view, const CvmHostSignature*> built;
        for (const CvmHostSignature& signature : kHostFunctions) {
            built.emplace(signature.name, &signature);
        }
        for (const CvmHostSignature& signature : kHostModuleFunctions) {
            built.emplace(signature.name, &signature);
        }
        return built;
    }();
    return index;
}

const char* TypeName(ConcordScript::CvmHostType type)
{
    switch (type) {
        case CvmHostType::F64: return "f64";
        case CvmHostType::Str: return "str";
        case CvmHostType::Void: return "void";
        case CvmHostType::I64: break;
    }
    return "i64";
}

} // namespace

namespace ConcordScript {

std::span<const CvmCallbackSlot> CallbackSlots()
{
    return kCallbackSlots;
}

const CvmCallbackSlot* FindCallbackSlot(std::string_view symbol, std::size_t parameter)
{
    for (const CvmCallbackSlot& slot : kCallbackSlots) {
        if (slot.symbol == symbol && slot.parameter == parameter) return &slot;
    }
    return nullptr;
}

std::span<const CvmHostSignature> HostFunctions()
{
    return kHostFunctions;
}

std::span<const CvmHostSignature> HostModuleFunctions()
{
    return kHostModuleFunctions;
}

const CvmHostSignature* FindHostFunction(std::string_view name)
{
    const auto found = HostIndex().find(name);
    return found == HostIndex().end() ? nullptr : found->second;
}

std::string DescribeHostSignature(const CvmHostSignature& signature)
{
    std::string text = signature.name;
    text += "(";
    for (std::size_t index = 0; index < signature.paramCount; ++index) {
        if (index != 0) text += ", ";
        text += TypeName(signature.params[index]);
    }
    text += ") -> ";
    text += TypeName(signature.result);
    return text;
}

} // namespace ConcordScript
