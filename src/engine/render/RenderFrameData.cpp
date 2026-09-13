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

/** Keeps a scalar world coordinate finite without flooring it at zero. */
f32 SafeCoordinate(f32 value) noexcept
{
    return std::isfinite(value) ? value : 0.0f;
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

/**
 * Packs one snapshot light into the stable four-vec ABI.
 *
 * @param environment Sun multiplier from the scene; applied to directional
 *        lights alone, because the control it comes from is the sun's.
 */
RenderFrameLightData PackLight(const RenderLightSnapshot& source, f32 sunIntensity)
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
    const f32 scale = type == LightType::Directional ? std::max(sunIntensity, 0.0f) : 1.0f;
    return RenderFrameLightData{
        .positionType = {position.x, position.y, position.z, static_cast<f32>(type)},
        .directionRange = {direction.x, direction.y, direction.z, SafeNonNegative(light.range)},
        .colorIntensity = {color.x, color.y, color.z,
                           SafeNonNegative(light.intensity) * scale},
        .spotShadow = {std::cos(Radians(angle)), light.castShadow ? 1.0f : 0.0f, 0.0f, 0.0f},
    };
}

/** Copies lights in scene order until the fixed ABI capacity is reached. */
usize CopyLights(RenderFrameData& result, const RenderSceneSnapshot& snapshot)
{
    const f32 sunIntensity =
        std::isfinite(snapshot.environment.sunIntensity)
            ? std::max(snapshot.environment.sunIntensity, 0.0f)
            : 1.0f;
    usize copied = 0;
    for (const RenderLightSnapshot& light : snapshot.lights) {
        if (copied == kMaxRenderLights) {
            break;
        }
        result.lights[copied++] = PackLight(light, sunIntensity);
    }
    return copied;
}

} // namespace

RenderFrameData BuildRenderFrameData(const RenderSceneSnapshot& snapshot, f32 timeSeconds)
{
    RenderFrameData result{};
    result.shadowViewProjection = Mat4::Identity();
    result.frameTime = {std::isfinite(timeSeconds) ? timeSeconds : 0.0f, 0.0f, 0.0f, 0.0f};
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
    // A negative exposure would invert the image and a negative vignette would
    // brighten the corners, so both are clamped rather than merely finite.
    result.grade = {SafeNonNegative(snapshot.environment.exposure),
                    std::clamp(SafeNonNegative(snapshot.environment.contrast), 0.0f, 1.0f),
                    SafeNonNegative(snapshot.environment.saturation),
                    std::clamp(SafeNonNegative(snapshot.environment.vignette), 0.0f, 1.0f)};
    // A bloom radius beyond a fraction of the frame is a full-screen wash
    // rather than a glow, so it is bounded rather than merely finite.
    result.postFx = {SafeNonNegative(snapshot.environment.bloomThreshold),
                     SafeNonNegative(snapshot.environment.bloomIntensity),
                     std::clamp(SafeNonNegative(snapshot.environment.bloomRadius), 0.0f, 512.0f),
                     std::clamp(SafeNonNegative(snapshot.environment.chromaticAberration),
                                0.0f, 0.05f)};
    result.surfaceInfo = {
        static_cast<f32>(std::min<usize>(snapshot.ripples.size(), kMaxRenderRipples)), 0.0f,
        0.0f, 0.0f};
    // A negative height falloff would make the medium denser without limit as
    // it rises, and an anisotropy outside -1..1 has no phase function at all,
    // so both are clamped rather than merely checked for finiteness.
    result.fog = {SafeNonNegative(snapshot.environment.fogDensity),
                  SafeNonNegative(snapshot.environment.fogHeightFalloff),
                  SafeColor(snapshot.environment.fogBaseHeight),
                  std::clamp(SafeColor(snapshot.environment.fogAnisotropy + 1.0f), 0.0f, 2.0f) -
                      1.0f};
    result.zenithColor = {SafeColor(snapshot.environment.zenithColor.x),
                          SafeColor(snapshot.environment.zenithColor.y),
                          SafeColor(snapshot.environment.zenithColor.z), 0.0f};
    result.horizonColor = {SafeColor(snapshot.environment.horizonColor.x),
                           SafeColor(snapshot.environment.horizonColor.y),
                           SafeColor(snapshot.environment.horizonColor.z), 0.0f};
    result.cloud = {std::clamp(SafeNonNegative(snapshot.environment.cloudCoverage), 0.0f, 1.0f),
                    SafeNonNegative(snapshot.environment.cloudDensity),
                    SafeNonNegative(snapshot.environment.cloudAltitude),
                    std::clamp(SafeNonNegative(snapshot.environment.cloudWeatherScale), 1.0f,
                               1000000.0f)};
    result.cloudDetail = {
        static_cast<f32>(std::min<u32>(snapshot.environment.cloudSteps, 96u)),
        SafeNonNegative(snapshot.environment.cloudDriftSpeed),
        std::isfinite(timeSeconds) ? timeSeconds : 0.0f,
        SafeNonNegative(snapshot.environment.cloudLightGain)};
    // A thickness of zero collapses the layer to a plane with no interval left
    // to march, and a detail cell of zero is a division by it, so both are
    // floored rather than merely made finite.
    result.cloudShape = {
        std::clamp(SafeNonNegative(snapshot.environment.cloudThickness), 1.0f, 1000000.0f),
        std::clamp(SafeNonNegative(snapshot.environment.cloudDetailScale), 1.0f, 1000000.0f),
        std::clamp(SafeNonNegative(snapshot.environment.cloudDetailStrength), 0.0f, 1.0f),
        std::clamp(SafeNonNegative(snapshot.environment.cloudType), 0.0f, 1.0f)};
    result.cloudMarch = {
        static_cast<f32>(std::min<u32>(snapshot.environment.cloudCoarseSteps, 64u)),
        static_cast<f32>(std::min<u32>(snapshot.environment.cloudLightSteps, 32u)),
        SafeNonNegative(snapshot.environment.cloudAmbientGain),
        0.0f};
    const Vec3 moon = snapshot.environment.moonDirection;
    const f32 moonLength =
        std::sqrt(moon.x * moon.x + moon.y * moon.y + moon.z * moon.z);
    result.celestial = moonLength > 0.0001f
                           ? Vec4{moon.x / moonLength, moon.y / moonLength, moon.z / moonLength,
                                  std::clamp(SafeNonNegative(snapshot.environment.moonIntensity),
                                             0.0f, 1.0f)}
                           : Vec4{0.0f, 1.0f, 0.0f,
                                  std::clamp(SafeNonNegative(snapshot.environment.moonIntensity),
                                             0.0f, 1.0f)};
    result.fogLight = {SafeNonNegative(snapshot.environment.fogSunScattering),
                       SafeNonNegative(snapshot.environment.fogAmbientScattering),
                       static_cast<f32>(std::min<u32>(snapshot.environment.fogSteps, 8u)),
                       std::clamp(SafeNonNegative(snapshot.environment.fogStepDistance), 0.0f,
                                  4096.0f)};

    // Left as zero past the live count: a radius of zero is the empty-slot
    // sentinel the shader's own loop bails out on, so an unused tail cannot be
    // read as a body sitting at the world origin.
    const usize wetCount = std::min<usize>(snapshot.waterBodies.size(), kMaxWetnessBodies);
    for (usize index = 0; index < wetCount; ++index) {
        const RenderWaterBodySnapshot& body = snapshot.waterBodies[index];
        result.wetnessBodies[index] = {SafeCoordinate(body.centre.x), SafeCoordinate(body.worldY),
                                       SafeCoordinate(body.centre.y),
                                       SafeNonNegative(body.radius)};
    }

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
