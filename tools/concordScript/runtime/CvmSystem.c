// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmHandles.h"
#include "CvmRuntime.h"

#include <windows.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Console output, clocks and the process-wide pseudo-random generator.
 *
 * The generator is an xorshift64* sequence rather than a library rand(): it is
 * defined here, so a CVM program produces the same numbers on every toolchain
 * and a seeded run is reproducible across the JIT and an AOT object. Its state
 * is advanced under the runtime lock, which makes concurrent draws safe and
 * keeps the sequence a function of the seed alone.
 */
static uint64_t g_randomState = 0x9E3779B97F4A7C15ull;

/** Advances the generator and returns one 64-bit draw. */
static uint64_t CvmRandomStep(void)
{
    uint64_t state = g_randomState;
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    g_randomState = state;
    return state * 0x2545F4914F6CDD1Dull;
}

int64_t cvm_print(int64_t value)
{
    printf("cvm: %" PRId64 "\n", value);
    fflush(stdout);
    return value;
}

int64_t print(int64_t value)
{
    return cvm_print(value);
}

double cvm_print_f64(double value)
{
    printf("cvm: %g\n", value);
    fflush(stdout);
    return value;
}

int64_t cvm_sys_time_ms(void)
{
    return (int64_t)GetTickCount64();
}

int64_t cvm_sys_sleep_ms(int64_t milliseconds)
{
    if (milliseconds > 0) Sleep((DWORD)milliseconds);
    return milliseconds;
}

int64_t cvm_sys_cpu_count(void)
{
    SYSTEM_INFO information;
    GetSystemInfo(&information);
    return (int64_t)information.dwNumberOfProcessors;
}

int64_t cvm_sys_random_seed(int64_t seed)
{
    CvmRuntimeLock();
    g_randomState = seed == 0 ? (uint64_t)GetTickCount64() | 1u : (uint64_t)seed;
    const int64_t used = (int64_t)(g_randomState >> 1);
    CvmRuntimeUnlock();
    return used;
}

int64_t cvm_sys_random_next(void)
{
    CvmRuntimeLock();
    const uint64_t draw = CvmRandomStep();
    CvmRuntimeUnlock();
    return (int64_t)(draw >> 1);
}

int64_t cvm_sys_random_range(int64_t low, int64_t high)
{
    if (high <= low) return low;
    CvmRuntimeLock();
    const uint64_t draw = CvmRandomStep();
    CvmRuntimeUnlock();
    return low + (int64_t)(draw % (uint64_t)(high - low));
}

double cvm_sys_random_float(void)
{
    CvmRuntimeLock();
    const uint64_t draw = CvmRandomStep();
    CvmRuntimeUnlock();
    return (double)(draw >> 11) * (1.0 / 9007199254740992.0);
}

int64_t cvm_sys_exit(int64_t code)
{
    exit((int)code);
    return code;
}
