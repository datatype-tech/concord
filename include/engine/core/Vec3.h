// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VEC3_H
#define CONCORD_VEC3_H

#include "engine/core/Types.h"

#include <cmath>

namespace Concord {

/**
 * Three-component vector.
 *
 * Deliberately a plain aggregate with no user-declared constructors: it is
 * the type application code writes most often inside a Desc
 * (`.position = {0.0f, 2.0f, -5.0f}`), and any constructor would defeat
 * brace initialization there.
 */
struct Vec3 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    friend Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    friend Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    friend Vec3 operator*(Vec3 v, f32 s) noexcept { return {v.x * s, v.y * s, v.z * s}; }
    friend Vec3 operator*(f32 s, Vec3 v) noexcept { return v * s; }
    friend Vec3 operator/(Vec3 v, f32 s) noexcept { return {v.x / s, v.y / s, v.z / s}; }
    friend Vec3 operator-(Vec3 v) noexcept { return {-v.x, -v.y, -v.z}; }

    Vec3& operator+=(Vec3 o) noexcept { return *this = *this + o; }
    Vec3& operator-=(Vec3 o) noexcept { return *this = *this - o; }
    Vec3& operator*=(f32 s) noexcept { return *this = *this * s; }
};

[[nodiscard]] inline f32 Dot(Vec3 a, Vec3 b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

[[nodiscard]] inline Vec3 Cross(Vec3 a, Vec3 b) noexcept
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

[[nodiscard]] inline f32 Length(Vec3 v) noexcept { return std::sqrt(Dot(v, v)); }

/** Returns `v` scaled to unit length, or `v` unchanged when it is degenerate. */
[[nodiscard]] inline Vec3 Normalize(Vec3 v) noexcept
{
    const f32 len = Length(v);
    return len > 0.0f ? v / len : v;
}

} // namespace Concord

#endif // CONCORD_VEC3_H
