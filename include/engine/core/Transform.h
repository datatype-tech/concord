// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_TRANSFORM_H
#define CONCORD_TRANSFORM_H

#include "engine/core/Angle.h"
#include "engine/core/Mat4.h"
#include "engine/core/Vec3.h"

namespace Concord {

/**
 * Position, rotation and scale of a scene node.
 *
 * Rotation is stored as Euler angles in degrees with a Y-X-Z (yaw-pitch-roll)
 * order, because that is what reads clearly in a hand-written Desc; the
 * conversion to a matrix happens here rather than at every call site.
 */
struct Transform {
    /** World-space position. */
    Vec3 position{0.0f, 0.0f, 0.0f};

    /** Euler angles in degrees, applied in Y-X-Z order. */
    Vec3 rotation{0.0f, 0.0f, 0.0f};

    /** Per-axis scale. */
    Vec3 scale{1.0f, 1.0f, 1.0f};

    /** Composes the model matrix: scale first, then rotation, then translation. */
    [[nodiscard]] Mat4 ToMatrix() const noexcept
    {
        const Mat4 rotY = Mat4::Rotate(Radians(rotation.y), {0.0f, 1.0f, 0.0f});
        const Mat4 rotX = Mat4::Rotate(Radians(rotation.x), {1.0f, 0.0f, 0.0f});
        const Mat4 rotZ = Mat4::Rotate(Radians(rotation.z), {0.0f, 0.0f, 1.0f});
        return Mat4::Translate(position) * (rotY * rotX * rotZ) * Mat4::Scale(scale);
    }
};

} // namespace Concord

#endif // CONCORD_TRANSFORM_H
