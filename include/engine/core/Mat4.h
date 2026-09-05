// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_MAT4_H
#define CONCORD_MAT4_H

#include "engine/core/Angle.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/core/Vec4.h"

#include <cmath>

namespace Concord {

/**
 * Column-major 4x4 matrix, laid out so it can be uploaded to Vulkan verbatim.
 *
 * Projection matrices follow the Vulkan clip convention rather than the
 * OpenGL one: depth maps to [0, 1] and the Y axis points down. Baking the Y
 * flip into the projection keeps shaders free of correction factors and
 * avoids a negative-height viewport.
 */
struct Mat4 {
    Vec4 col[4]{};

    [[nodiscard]] static Mat4 Identity() noexcept
    {
        Mat4 m{};
        m.col[0].x = 1.0f;
        m.col[1].y = 1.0f;
        m.col[2].z = 1.0f;
        m.col[3].w = 1.0f;
        return m;
    }

    [[nodiscard]] Mat4 operator*(const Mat4& rhs) const noexcept
    {
        Mat4 out{};
        for (u32 c = 0; c < 4; ++c) {
            for (u32 r = 0; r < 4; ++r) {
                f32 sum = 0.0f;
                for (u32 k = 0; k < 4; ++k) {
                    sum += col[k][r] * rhs.col[c][k];
                }
                out.col[c][r] = sum;
            }
        }
        return out;
    }

    [[nodiscard]] Vec4 operator*(const Vec4& v) const noexcept
    {
        Vec4 out{};
        for (u32 r = 0; r < 4; ++r) {
            out[r] = col[0][r] * v.x + col[1][r] * v.y + col[2][r] * v.z + col[3][r] * v.w;
        }
        return out;
    }

    [[nodiscard]] static Mat4 Translate(Vec3 t) noexcept
    {
        Mat4 m = Identity();
        m.col[3] = {t.x, t.y, t.z, 1.0f};
        return m;
    }

    [[nodiscard]] static Mat4 Scale(Vec3 s) noexcept
    {
        Mat4 m = Identity();
        m.col[0].x = s.x;
        m.col[1].y = s.y;
        m.col[2].z = s.z;
        return m;
    }

    /**
     * Rotation about an arbitrary axis.
     *
     * @param radians Rotation angle in radians.
     * @param axis    Rotation axis; normalized internally, so it need not be unit length.
     */
    [[nodiscard]] static Mat4 Rotate(f32 radians, Vec3 axis) noexcept
    {
        const Vec3 a = Normalize(axis);
        const f32 c = std::cos(radians);
        const f32 s = std::sin(radians);
        const f32 ic = 1.0f - c;

        Mat4 m = Identity();
        m.col[0] = {c + a.x * a.x * ic,       a.y * a.x * ic + a.z * s, a.z * a.x * ic - a.y * s, 0.0f};
        m.col[1] = {a.x * a.y * ic - a.z * s, c + a.y * a.y * ic,       a.z * a.y * ic + a.x * s, 0.0f};
        m.col[2] = {a.x * a.z * ic + a.y * s, a.y * a.z * ic - a.x * s, c + a.z * a.z * ic,       0.0f};
        return m;
    }

    /**
     * Right-handed view matrix.
     *
     * @param eye    Camera position in world space.
     * @param target Point the camera looks at.
     * @param up     World up direction, normally {0, 1, 0}.
     */
    [[nodiscard]] static Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up) noexcept
    {
        const auto finite = [](Vec3 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        };
        const auto validDirection = [&](Vec3 value) noexcept {
            return finite(value) && Dot(value, value) > 1.0e-12f;
        };

        if (!finite(eye)) eye = {};
        Vec3 forward = target - eye;
        if (!validDirection(forward)) forward = {0.0f, 0.0f, -1.0f};
        forward = Normalize(forward);
        if (!validDirection(up)) up = {0.0f, 1.0f, 0.0f};
        if (std::fabs(Dot(forward, Normalize(up))) > 0.999f) {
            up = std::fabs(forward.y) < 0.9f ? Vec3{0.0f, 1.0f, 0.0f}
                                            : Vec3{0.0f, 0.0f, 1.0f};
        }
        Vec3 side = Normalize(Cross(forward, up));
        if (!validDirection(side)) side = {1.0f, 0.0f, 0.0f};
        const Vec3 correctedUp = Cross(side, forward);

        Mat4 m = Identity();
        m.col[0] = {side.x, correctedUp.x, -forward.x, 0.0f};
        m.col[1] = {side.y, correctedUp.y, -forward.y, 0.0f};
        m.col[2] = {side.z, correctedUp.z, -forward.z, 0.0f};
        m.col[3] = {-Dot(side, eye), -Dot(correctedUp, eye), Dot(forward, eye), 1.0f};
        for (const Vec4& column : m.col) {
            if (!finite({column.x, column.y, column.z})) return Identity();
        }
        return m;
    }

    /**
     * Perspective projection using the Vulkan clip convention.
     *
     * @param fovYRadians Vertical field of view in radians.
     * @param aspect      Viewport width divided by height; callers should use
     *                    the current viewport ratio unless overriding it.
     */
    [[nodiscard]] static Mat4 Perspective(f32 fovYRadians, f32 aspect, f32 nearPlane, f32 farPlane) noexcept
    {
        if (!std::isfinite(fovYRadians) || !std::isfinite(aspect) || !std::isfinite(nearPlane) ||
            !std::isfinite(farPlane) || fovYRadians <= 0.0f || fovYRadians >= 3.1415927f ||
            aspect <= 0.0f || nearPlane <= 0.0f || farPlane <= nearPlane) {
            return Identity();
        }
        const f64 tangent = 1.0 / std::tan(static_cast<f64>(fovYRadians) * 0.5);
        const f64 denominator = static_cast<f64>(nearPlane) - static_cast<f64>(farPlane);
        const f64 values[4] = {
            tangent / static_cast<f64>(aspect),
            -tangent,
            static_cast<f64>(farPlane) / denominator,
            (static_cast<f64>(nearPlane) * static_cast<f64>(farPlane)) / denominator,
        };
        for (const f64 value : values) {
            if (!std::isfinite(value) || !std::isfinite(static_cast<f32>(value))) return Identity();
        }
        Mat4 m{};
        m.col[0].x = static_cast<f32>(values[0]);
        m.col[1].y = static_cast<f32>(values[1]);
        m.col[2].z = static_cast<f32>(values[2]);
        m.col[2].w = -1.0f;
        m.col[3].z = static_cast<f32>(values[3]);
        return m;
    }
    /**
     * Orthographic projection using the Vulkan clip convention.
     *
     * Depth maps to [0, 1] and the Y axis points down, matching Perspective,
     * so both projection kinds share one shader path.
     */
    [[nodiscard]] static Mat4 Orthographic(f32 left, f32 right, f32 bottom, f32 top,
                                           f32 nearPlane, f32 farPlane) noexcept
    {
        if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(bottom) ||
            !std::isfinite(top) || !std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
            left >= right || bottom >= top || nearPlane >= farPlane) {
            return Identity();
        }
        Mat4 m{};
        m.col[0].x = 2.0f / (right - left);
        m.col[1].y = -2.0f / (top - bottom);
        m.col[2].z = 1.0f / (nearPlane - farPlane);
        m.col[3].x = -(right + left) / (right - left);
        m.col[3].y = -(top + bottom) / (top - bottom);
        m.col[3].z = nearPlane / (nearPlane - farPlane);
        m.col[3].w = 1.0f;
        return m;
    }

    /**
     * Inverts a rigid transform (orthonormal rotation plus translation).
     *
     * Camera view matrices are built this way: transposing the rotation part
     * is exact, so no general-purpose inverse is required. Returns Identity
     * when any spatial component is not finite.
     */
    [[nodiscard]] Mat4 InvertRigid() const noexcept
    {
        for (const Vec4& column : col) {
            if (!std::isfinite(column.x) || !std::isfinite(column.y) || !std::isfinite(column.z)) {
                return Identity();
            }
        }
        const f32 tx = col[3].x;
        const f32 ty = col[3].y;
        const f32 tz = col[3].z;
        Mat4 inverse{};
        inverse.col[0] = {col[0].x, col[1].x, col[2].x, 0.0f};
        inverse.col[1] = {col[0].y, col[1].y, col[2].y, 0.0f};
        inverse.col[2] = {col[0].z, col[1].z, col[2].z, 0.0f};
        inverse.col[3] = {-(col[0].x * tx + col[0].y * ty + col[0].z * tz),
                          -(col[1].x * tx + col[1].y * ty + col[1].z * tz),
                          -(col[2].x * tx + col[2].y * ty + col[2].z * tz), 1.0f};
        return inverse;
    }
};

} // namespace Concord

#endif // CONCORD_MAT4_H
