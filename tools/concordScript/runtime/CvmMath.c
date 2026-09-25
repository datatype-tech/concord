// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "CvmRuntime.h"

#include <math.h>
#include <stdint.h>

/**
 * Scalar maths for gameplay: camera orbits, projectiles, easing.
 *
 * These wrap libm rather than reimplementing it. Angles are radians, matching
 * the C library, so a script that thinks in degrees writes radians(yaw).
 */
static const double kPi = 3.14159265358979323846;
static const double kTau = 6.28318530717958647692;

double cvm_pi(void)
{
    return kPi;
}

double cvm_tau(void)
{
    return kTau;
}

double cvm_sin(double x) { return sin(x); }
double cvm_cos(double x) { return cos(x); }
double cvm_tan(double x) { return tan(x); }
double cvm_asin(double x) { return asin(x); }
double cvm_acos(double x) { return acos(x); }
double cvm_atan(double x) { return atan(x); }
double cvm_atan2(double y, double x) { return atan2(y, x); }
double cvm_sqrt(double x) { return sqrt(x); }
double cvm_pow(double base, double exponent) { return pow(base, exponent); }
double cvm_hypot(double x, double y) { return hypot(x, y); }
double cvm_floor(double x) { return floor(x); }
double cvm_ceil(double x) { return ceil(x); }
double cvm_round(double x) { return round(x); }
double cvm_abs(double x) { return fabs(x); }
double cvm_min(double left, double right) { return fmin(left, right); }
double cvm_max(double left, double right) { return fmax(left, right); }

double cvm_sign(double x)
{
    if (x > 0.0) return 1.0;
    if (x < 0.0) return -1.0;
    return x;
}

double cvm_fract(double x)
{
    return x - floor(x);
}

double cvm_radians(double degrees)
{
    return degrees * (kPi / 180.0);
}

double cvm_degrees(double radians)
{
    return radians * (180.0 / kPi);
}

double cvm_clamp(double value, double low, double high)
{
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

double cvm_lerp(double start, double end, double t)
{
    return start + (end - start) * t;
}

int64_t cvm_iabs(int64_t value)
{
    if (value >= 0) return value;
    if (value == INT64_MIN) return INT64_MAX;
    return -value;
}

int64_t cvm_imin(int64_t left, int64_t right)
{
    return left < right ? left : right;
}

int64_t cvm_imax(int64_t left, int64_t right)
{
    return left > right ? left : right;
}
