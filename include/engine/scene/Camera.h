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

#include <algorithm>
#include <cmath>
#include <limits>

namespace Concord::Object {

/** Every field a Camera can be spawned from. */
struct CameraDesc {
    /** Camera position in world space. */
    Vec3 position{0.0f, 2.0f, -5.0f};

    /** Euler angles in degrees (Y-X-Z); used as-is when `target` is degenerate. */
    Vec3 rotation{};

    /**
     * Optional world-space look-at point. A finite value enables one-time
     * target-to-rotation conversion; the default leaves `rotation` in charge.
     */
    Vec3 target{std::numeric_limits<f32>::quiet_NaN(),
                std::numeric_limits<f32>::quiet_NaN(),
                std::numeric_limits<f32>::quiet_NaN()};

    /** Vertical field of view in degrees, perspective projection only. */
    f32 fovYDegrees = 60.0f;

    /** Projection the lens uses. */
    CameraProjection projection = CameraProjection::Perspective;

    /** Vertical half-extent of the frustum in world units, orthographic only. */
    f32 orthographicSize = 5.0f;

    /** Near clip distance; must be greater than zero. */
    f32 nearPlane = 0.1f;

    /** Far clip distance. */
    f32 farPlane = 1000.0f;

    /** Width-over-height ratio forced on the projection; 0 uses the window. */
    f32 aspectOverride = 0.0f;

    /** The scene renders through the enabled camera with the lowest priority. */
    i32 priority = 0;

    /** Disabled cameras are skipped by MainCamera and by rendering. */
    bool enabled = true;
};

/** Euler rotation in degrees that looks from `position` toward `target`. */
[[nodiscard]] inline Vec3 LookAtRotation(Vec3 position, Vec3 target) noexcept
{
    const Vec3 direction = target - position;
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
        !std::isfinite(direction.z)) {
        return {};
    }
    const f32 horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (horizontal < 1.0e-6f && std::fabs(direction.y) < 1.0e-6f) {
        return {};
    }
    const f32 pitch = std::atan2(direction.y, horizontal);
    const f32 yaw = std::atan2(-direction.x, -direction.z);
    return {Degrees(pitch), Degrees(yaw), 0.0f};
}

/**
 * Viewpoint into the scene.
 *
 * Spawning one attaches a Transform (position and orientation) and a
 * CameraComponent (the lens), so a camera is queryable like anything else
 * rather than being a special case. The view direction is the Transform's
 * rotation: the camera looks down its local -Z axis, and controllers aim it
 * by rotating the Transform.
 */
struct Camera {
    using Desc = CameraDesc;

    /** Attaches the components that constitute a camera. */
    static void Build(World& world, Entity entity, const CameraDesc& desc)
    {
        Vec3 rotation = desc.rotation;
        const Vec3 direction = desc.target - desc.position;
        if (std::isfinite(direction.x) && std::isfinite(direction.y) &&
            std::isfinite(direction.z) && Dot(direction, direction) > 1.0e-12f) {
            rotation = LookAtRotation(desc.position, desc.target);
        }
        world.Add<Transform>(entity,
                             Transform{.position = desc.position, .rotation = rotation});
        world.Add<CameraComponent>(entity, CameraComponent{
            .projection = desc.projection,
            .fovYDegrees = desc.fovYDegrees,
            .orthographicSize = desc.orthographicSize,
            .nearPlane = desc.nearPlane,
            .farPlane = desc.farPlane,
            .aspectOverride = desc.aspectOverride,
            .priority = desc.priority,
            .enabled = desc.enabled,
        });
    }
};

/** World-space direction a camera faces: its Transform's local -Z axis. */
[[nodiscard]] inline Vec3 ForwardVector(const Transform& transform) noexcept
{
    const Vec3 rotation = transform.rotation;
    if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y)) {
        return {0.0f, 0.0f, -1.0f};
    }
    const f32 pitch = Radians(rotation.x);
    const f32 yaw = Radians(rotation.y);
    return {-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}

/**
 * View matrix for a camera oriented by `transform`'s rotation.
 *
 * Built as the exact rigid inverse of the camera's world transform, so it
 * always agrees with the matrices that place scene objects.
 */
[[nodiscard]] inline Mat4 ViewMatrix(const Transform& transform) noexcept
{
    const Vec3 rotation = transform.rotation;
    const Vec3 position = transform.position;
    const bool finite = std::isfinite(rotation.x) && std::isfinite(rotation.y) &&
                        std::isfinite(rotation.z) && std::isfinite(position.x) &&
                        std::isfinite(position.y) && std::isfinite(position.z);
    if (!finite) {
        return Mat4::Identity();
    }
    const Mat4 rigid =
        Mat4::Translate(position) * Mat4::Rotate(Radians(rotation.y), {0.0f, 1.0f, 0.0f}) *
        Mat4::Rotate(Radians(rotation.x), {1.0f, 0.0f, 0.0f}) *
        Mat4::Rotate(Radians(rotation.z), {0.0f, 0.0f, 1.0f});
    return rigid.InvertRigid();
}

/**
 * Projection matrix for a camera.
 *
 * @param aspect Viewport width divided by height. Ignored while the camera
 *        carries an aspect override; otherwise the renderer supplies the
 *        current window ratio by default.
 */
[[nodiscard]] inline Mat4 ProjectionMatrix(const CameraComponent& camera, f32 aspect) noexcept
{
    const f32 safeAspect = std::isfinite(camera.aspectOverride) && camera.aspectOverride > 0.0f
                               ? camera.aspectOverride
                               : (std::isfinite(aspect) && aspect > 0.0f ? aspect : 1.0f);
    const f32 nearPlane =
        std::isfinite(camera.nearPlane) && camera.nearPlane > 0.0f ? camera.nearPlane : 0.1f;
    const f32 farPlane = std::isfinite(camera.farPlane) && camera.farPlane > nearPlane
                             ? camera.farPlane
                             : nearPlane + 1000.0f;
    if (camera.projection == CameraProjection::Orthographic) {
        const f32 size = std::isfinite(camera.orthographicSize) && camera.orthographicSize > 0.0f
                             ? camera.orthographicSize
                             : 5.0f;
        return Mat4::Orthographic(-size * safeAspect, size * safeAspect, -size, size, nearPlane,
                                  farPlane);
    }
    const f32 fov =
        std::isfinite(camera.fovYDegrees) ? std::clamp(camera.fovYDegrees, 1.0f, 179.0f) : 60.0f;
    return Mat4::Perspective(Radians(fov), safeAspect, nearPlane, farPlane);
}

} // namespace Concord::Object

#endif // CONCORD_CAMERA_H
