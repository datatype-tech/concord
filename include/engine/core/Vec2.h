// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CORE_VEC2_H
#define CONCORD_CORE_VEC2_H

#include "engine/core/Types.h"

namespace Concord {

/** Two-component floating-point vector used by imported texture coordinates. */
struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;

    friend Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
    friend Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
    friend Vec2 operator*(Vec2 v, f32 s) noexcept { return {v.x * s, v.y * s}; }
    friend Vec2 operator/(Vec2 v, f32 s) noexcept { return {v.x / s, v.y / s}; }
    Vec2& operator+=(Vec2 value) noexcept { return *this = *this + value; }
};

} // namespace Concord

#endif // CONCORD_CORE_VEC2_H
