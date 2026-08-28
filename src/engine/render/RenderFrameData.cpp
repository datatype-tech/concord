// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderFrameData.h"

#include "engine/core/Color.h"
#include "engine/scene/SunLight.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Concord {
namespace {

/** Keeps scalar values finite and non-negative before they cross the ABI. */
f32 SafeNonNegative(f32 value) noexcept
{
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

/** Keeps a color channel finite and non-negative without discarding HDR values. */
f32 SafeColor(f32 value) noexcept
{
    return std::isfinite(value) ? std::max(value, 0.0f) : 0.0f;
}

/** Replaces malformed vector components before they reach GPU memory. */
Vec3 SafeVector(Vec3 value) noexcept
{
    const auto Finite = [](f32 component) {
        return std::isfinite(component) ? component : 0.0f;
    };
    return {Finite(value.x), Finite(value.y), Finite(value.z)};
}

/** Clamps an authored angle while providing a finite fallback. */
f32 SafeAngle(f32 value, f32 fallback, f32 minimum, f32 maximum) noexcept
{
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

/** Converts a spot transform to a world-space forward direction. */
Vec3 SpotDirection(const Transform& transform) noexcept
{
    const Mat4 matrix = transform.ToMatrix();
    return Normalize(SafeVector({-matrix.col[2].x, -matrix.col[2].y, -matrix.col[2].z}));
}

/** Packs one snapshot light into the stable four-vec ABI. */
RenderFrameLightData PackLight(const RenderLightSnapshot& source)
{
    const LightComponent& light = source.light;
    const u32 typeValue = static_cast<u32>(light.type);
    const LightType type = typeValue <= static_cast<u32>(LightType::Spot)
                               ? light.type
                               : LightType::Point;
    Vec3 direction{};
    if (type == LightType::Directional) {
        LightComponent safeLight = light;
        safeLight.elevationDegrees = SafeAngle(light.elevationDegrees, 45.0f, -90.0f, 90.0f);
        safeLight.azimuthDegrees = SafeAngle(light.azimuthDegrees, 200.0f, -360.0f, 360.0f);
        direction = Object::LightDirection(safeLight);
    } else if (type == LightType::Spot) {
        direction = SpotDirection(source.transform);
    }
    const Vec3 position = SafeVector(source.transform.position);
    direction = SafeVector(direction);
    const Vec3 color = ToLinear(light.color);
    const f32 angle = SafeAngle(light.spotAngleDegrees, 35.0f, 0.0f, 179.0f);
    return RenderFrameLightData{
        .positionType = {position.x, position.y, position.z, static_cast<f32>(type)},
        .directionRange = {direction.x, direction.y, direction.z, SafeNonNegative(light.range)},
        .colorIntensity = {color.x, color.y, color.z, SafeNonNegative(light.intensity)},
        .spotShadow = {std::cos(Radians(angle)), light.castShadow ? 1.0f : 0.0f, 0.0f, 0.0f},
    };
}

/** Copies lights in scene order until the fixed ABI capacity is reached. */
usize CopyLights(RenderFrameData& result, const RenderSceneSnapshot& snapshot)
{
    usize copied = 0;
    for (const RenderLightSnapshot& light : snapshot.lights) {
        if (copied == kMaxRenderLights) {
            break;
        }
        result.lights[copied++] = PackLight(light);
    }
    return copied;
}

} // namespace

RenderFrameData BuildRenderFrameData(const RenderSceneSnapshot& snapshot)
{
    RenderFrameData result{};
    result.shadowViewProjection = Mat4::Identity();
    result.header.cameraValid = snapshot.hasCamera ? 1u : 0u;
    if (snapshot.hasCamera) {
        result.camera.view = snapshot.camera.view;
        result.camera.projection = snapshot.camera.projection;
    }
    const Vec3 ambientColor = ToLinear(snapshot.environment.ambientColor);
    const Vec3 linearAmbient = {SafeColor(ambientColor.x), SafeColor(ambientColor.y),
                                SafeColor(ambientColor.z)};
    result.ambientColorIntensity = {linearAmbient.x, linearAmbient.y, linearAmbient.z,
                                     SafeNonNegative(snapshot.environment.ambientIntensity)};

    const usize copied = CopyLights(result, snapshot);
    result.header.lightCount = static_cast<u32>(copied);
    const usize dropped = snapshot.lights.size() - copied;
    result.header.droppedLightCount = static_cast<u32>(
        std::min(dropped, static_cast<usize>(std::numeric_limits<u32>::max())));
    return result;
}

std::span<const std::byte> RenderFrameDataBytes(const RenderFrameData& data) noexcept
{
    return std::as_bytes(std::span<const RenderFrameData>(&data, 1));
}

} // namespace Concord
