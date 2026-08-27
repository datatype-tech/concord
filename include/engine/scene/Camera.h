// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_CAMERA_H
#define CONCORD_CAMERA_H

#include "engine/core/Angle.h"
#include "engine/core/Mat4.h"
#include "engine/core/Transform.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"

namespace Concord::Object {

/** Every field a Camera can be spawned from. */
struct CameraDesc {
    /** Camera position in world space. */
    Vec3 position{0.0f, 2.0f, -5.0f};

    /** Point the camera looks at. */
    Vec3 target{0.0f, 0.0f, 0.0f};

    /** World up direction. */
    Vec3 up{0.0f, 1.0f, 0.0f};

    /** Vertical field of view in degrees. */
    f32 fovYDegrees = 60.0f;

    /** Near clip distance; must be greater than zero. */
    f32 nearPlane = 0.1f;

    /** Far clip distance. */
    f32 farPlane = 1000.0f;

    /** The scene renders through the camera with the lowest priority. */
    i32 priority = 0;
};

/**
 * Viewpoint into the scene.
 *
 * Spawning one attaches a Transform (holding the position) and a
 * CameraComponent (holding the lens), so a camera is queryable like anything
 * else rather than being a special case.
 */
struct Camera {
    using Desc = CameraDesc;

    static void Build(World& world, Entity entity, const CameraDesc& desc)
    {
        world.Add<Transform>(entity, Transform{.position = desc.position});
        world.Add<CameraComponent>(entity, CameraComponent{
            .target = desc.target,
            .up = desc.up,
            .fovYDegrees = desc.fovYDegrees,
            .nearPlane = desc.nearPlane,
            .farPlane = desc.farPlane,
            .priority = desc.priority,
        });
    }
};

/** View matrix for a camera placed at `transform`. */
[[nodiscard]] inline Mat4 ViewMatrix(const Transform& transform, const CameraComponent& camera) noexcept
{
    return Mat4::LookAt(transform.position, camera.target, camera.up);
}

/**
 * Projection matrix for a camera.
 *
 * @param aspect Viewport width divided by height.
 */
[[nodiscard]] inline Mat4 ProjectionMatrix(const CameraComponent& camera, f32 aspect) noexcept
{
    return Mat4::Perspective(Radians(camera.fovYDegrees), aspect, camera.nearPlane, camera.farPlane);
}

} // namespace Concord::Object

#endif // CONCORD_CAMERA_H
