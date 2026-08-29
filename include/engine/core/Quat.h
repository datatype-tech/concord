// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CORE_QUAT_H
#define CONCORD_CORE_QUAT_H

#include "engine/core/Mat4.h"
#include "engine/core/Types.h"

#include <algorithm>
#include <cmath>

namespace Concord {

/** Unit quaternion used for imported node and bone rotations. */
struct Quat {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 1.0f;

    /** Returns the identity rotation. */
    [[nodiscard]] static constexpr Quat Identity() noexcept { return {}; }

    /** Normalizes malformed input, falling back to identity. */
    [[nodiscard]] Quat Normalized() const noexcept
    {
        const f32 length = std::sqrt(x * x + y * y + z * z + w * w);
        if (!std::isfinite(length) || length < 0.000001f) {
            return Identity();
        }
        return {x / length, y / length, z / length, w / length};
    }

    /** Converts the quaternion to a column-major affine matrix. */
    [[nodiscard]] Mat4 ToMatrix() const noexcept
    {
        const Quat q = Normalized();
        const f32 xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
        const f32 xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
        const f32 wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
        Mat4 result = Mat4::Identity();
        result.col[0] = {1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),
                         2.0f * (xz - wy), 0.0f};
        result.col[1] = {2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz),
                         2.0f * (yz + wx), 0.0f};
        result.col[2] = {2.0f * (xz + wy), 2.0f * (yz - wx),
                         1.0f - 2.0f * (xx + yy), 0.0f};
        return result;
    }
};

/** Spherical interpolation with a normalized shortest-path result. */
[[nodiscard]] inline Quat Slerp(Quat a, Quat b, f32 amount) noexcept
{
    a = a.Normalized();
    b = b.Normalized();
    f32 dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f) {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    amount = std::isfinite(amount) ? std::clamp(amount, 0.0f, 1.0f) : 0.0f;
    if (dot > 0.9995f) {
        return Quat{a.x + amount * (b.x - a.x), a.y + amount * (b.y - a.y),
                    a.z + amount * (b.z - a.z), a.w + amount * (b.w - a.w)}
            .Normalized();
    }
    const f32 angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const f32 denominator = std::sin(angle);
    if (std::abs(denominator) < 0.000001f) {
        return a;
    }
    const f32 first = std::sin((1.0f - amount) * angle) / denominator;
    const f32 second = std::sin(amount * angle) / denominator;
    return {a.x * first + b.x * second, a.y * first + b.y * second,
            a.z * first + b.z * second, a.w * first + b.w * second};
}

} // namespace Concord

#endif // CONCORD_CORE_QUAT_H
