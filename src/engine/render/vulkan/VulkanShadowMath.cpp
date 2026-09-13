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

/** Axis-aligned world bounds of everything the shadow pass draws. */
struct ShadowBounds {
    Vec3 center{};
    f32 radius = 12.0f;
};

/**
 * Fits the shadow volume to the scene instead of to the camera.
 *
 * A shadow is a world-space projection: it must land in the same place, at
 * the same size, no matter where the viewer stands. Sizing the orthographic
 * box from a camera-relative focus point makes its centre slide and its
 * extent rescale every time the camera turns, so the same object appears to
 * cast a differently shaped shadow from a different angle.
 */
ShadowBounds ComputeShadowBounds(const RenderSceneSnapshot& snapshot) noexcept
{
    ShadowBounds bounds{};
    bool started = false;
    Vec3 minimum{};
    Vec3 maximum{};
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        const Vec3 center = SafeVector({object.model.col[3].x, object.model.col[3].y,
                                        object.model.col[3].z});
        const f32 x = Length({object.model.col[0].x, object.model.col[0].y,
                              object.model.col[0].z});
        const f32 y = Length({object.model.col[1].x, object.model.col[1].y,
                              object.model.col[1].z});
        const f32 z = Length({object.model.col[2].x, object.model.col[2].y,
                              object.model.col[2].z});
        const f32 extent = 0.5f * std::sqrt(x * x + y * y + z * z);
        const Vec3 low = center - Vec3{extent, extent, extent};
        const Vec3 high = center + Vec3{extent, extent, extent};
        if (!started) {
            minimum = low;
            maximum = high;
            started = true;
            continue;
        }
        minimum = {std::min(minimum.x, low.x), std::min(minimum.y, low.y),
                   std::min(minimum.z, low.z)};
        maximum = {std::max(maximum.x, high.x), std::max(maximum.y, high.y),
                   std::max(maximum.z, high.z)};
    }
    if (!started) {
        return bounds;
    }
    bounds.center = (minimum + maximum) * 0.5f;
    bounds.radius = std::clamp(Length((maximum - minimum) * 0.5f) * 1.15f, 12.0f, 200.0f);
    return bounds;
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
    const ShadowBounds bounds = ComputeShadowBounds(snapshot);
    const Vec3 center = bounds.center;
    const f32 radius = bounds.radius;
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
