// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowMath.h"

#include "engine/scene/SunLight.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Returns a finite vector suitable for view-matrix construction. */
Vec3 SafeVector(Vec3 value) noexcept
{
    const auto finite = [](f32 component) { return std::isfinite(component) ? component : 0.0f; };
    return {finite(value.x), finite(value.y), finite(value.z)};
}

/** Clamps authored sun angles before deriving a direction vector. */
f32 SafeAngle(f32 value, f32 fallback, f32 minimum, f32 maximum) noexcept
{
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

/** Builds an orthographic projection using the engine's Vulkan clip convention. */
Mat4 Orthographic(f32 radius, f32 nearPlane, f32 farPlane) noexcept
{
    const f32 diameter = std::max(radius * 2.0f, 0.001f);
    Mat4 result{};
    result.col[0].x = 2.0f / diameter;
    result.col[1].y = -2.0f / diameter;
    result.col[2].z = 1.0f / (nearPlane - farPlane);
    result.col[3].z = nearPlane / (nearPlane - farPlane);
    result.col[3].w = 1.0f;
    return result;
}

/** Estimates a conservative radius around the camera's shadow focus point. */
f32 SceneRadius(const RenderSceneSnapshot& snapshot, Vec3 center) noexcept
{
    f32 radius = 12.0f;
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        const Vec3 objectCenter = SafeVector({object.model.col[3].x, object.model.col[3].y,
                                              object.model.col[3].z});
        const f32 x = Length({object.model.col[0].x, object.model.col[0].y,
                              object.model.col[0].z});
        const f32 y = Length({object.model.col[1].x, object.model.col[1].y,
                              object.model.col[1].z});
        const f32 z = Length({object.model.col[2].x, object.model.col[2].y,
                              object.model.col[2].z});
        const f32 extent = 0.5f * std::sqrt(x * x + y * y + z * z);
        radius = std::max(radius, Length(objectCenter - center) + extent);
    }
    return std::clamp(radius * 1.25f, 12.0f, 160.0f);
}

} // namespace

VulkanDirectionalShadowState BuildVulkanDirectionalShadowState(
    const RenderSceneSnapshot& snapshot) noexcept
{
    VulkanDirectionalShadowState result{};
    if (!snapshot.hasCamera) {
        return result;
    }
    const RenderLightSnapshot* light = nullptr;
    for (usize i = 0; i < snapshot.lights.size(); ++i) {
        const RenderLightSnapshot& candidate = snapshot.lights[i];
        if (candidate.light.type == LightType::Directional && candidate.light.castShadow &&
            std::isfinite(candidate.light.intensity) && candidate.light.intensity > 0.0f) {
            light = &candidate;
            result.lightIndex = i;
            break;
        }
    }
    if (!light) {
        return result;
    }
    LightComponent safeLight = light->light;
    safeLight.elevationDegrees = SafeAngle(safeLight.elevationDegrees, 45.0f, -90.0f, 90.0f);
    safeLight.azimuthDegrees = SafeAngle(safeLight.azimuthDegrees, 200.0f, -360.0f, 360.0f);
    const Vec3 direction = Normalize(SafeVector(Object::LightDirection(safeLight)));
    if (Length(direction) < 0.001f) {
        return result;
    }
    const Vec3 center = SafeVector(snapshot.camera.target);
    const f32 radius = SceneRadius(snapshot, center);
    const Vec3 eye = center - direction * (radius * 2.0f);
    const Vec3 up = std::abs(Dot(direction, {0.0f, 1.0f, 0.0f})) > 0.95f
                        ? Vec3{0.0f, 0.0f, 1.0f}
                        : Vec3{0.0f, 1.0f, 0.0f};
    result.viewProjection = Orthographic(radius, 0.1f, radius * 4.0f) *
                            Mat4::LookAt(eye, center, up);
    result.enabled = true;
    return result;
}

} // namespace Concord
