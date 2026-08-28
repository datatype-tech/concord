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
        const Vec3 f = Normalize(target - eye);
        const Vec3 s = Normalize(Cross(f, up));
        const Vec3 u = Cross(s, f);

        Mat4 m = Identity();
        m.col[0] = {s.x, u.x, -f.x, 0.0f};
        m.col[1] = {s.y, u.y, -f.y, 0.0f};
        m.col[2] = {s.z, u.z, -f.z, 0.0f};
        m.col[3] = {-Dot(s, eye), -Dot(u, eye), Dot(f, eye), 1.0f};
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
        const f32 t = 1.0f / std::tan(fovYRadians * 0.5f);
        Mat4 m{};
        m.col[0].x = t / aspect;
        m.col[1].y = -t;
        m.col[2].z = farPlane / (nearPlane - farPlane);
        m.col[2].w = -1.0f;
        m.col[3].z = (nearPlane * farPlane) / (nearPlane - farPlane);
        return m;
    }
};

} // namespace Concord

#endif // CONCORD_MAT4_H
