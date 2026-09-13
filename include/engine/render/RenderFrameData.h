// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERFRAMEDATA_H
#define CONCORD_RENDERFRAMEDATA_H

#include "engine/core/Mat4.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/core/Vec4.h"
#include "engine/render/RenderSceneSnapshot.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>

namespace Concord {

/** Maximum number of lights copied into one frame's GPU-visible block. */
inline constexpr u32 kMaxRenderLights = 64;

/** Bit flags stored in RenderFrameHeaderData::reserved. */
inline constexpr u32 kRenderFrameFlagTileLights = 1u << 0;
inline constexpr u32 kRenderFrameFlagDirectionalShadow = 1u << 1;
inline constexpr u32 kRenderFrameShadowLightShift = 8u;
inline constexpr u32 kRenderFrameShadowLightMask = 0xffu << kRenderFrameShadowLightShift;

/** Counts and validity flags at the start of the frame block. */
struct alignas(16) RenderFrameHeaderData {
    u32 cameraValid = 0;
    u32 lightCount = 0;
    u32 droppedLightCount = 0;
    /** Feature flags encoded in the ABI's fourth word for backwards layout compatibility. */
    u32 reserved = 0;
};

/** Camera matrices shared by every draw in a frame. */
struct alignas(16) RenderFrameCameraData {
    Mat4 view{};
    Mat4 projection{};
};

/** One light in the fixed-size frame light array. */
struct alignas(16) RenderFrameLightData {
    /** xyz world position; w is the LightType ordinal (0/1/2). */
    Vec4 positionType{};
    /** xyz incident/spot direction; w is the non-negative range. */
    Vec4 directionRange{};
    /** rgb linear color; w is the non-negative intensity. */
    Vec4 colorIntensity{};
    /** x is cos(spot half-angle); y is 1 when shadow casting is enabled. */
    Vec4 spotShadow{};
};

/** CPU representation of the per-frame uniform block uploaded to Vulkan.
 *
 * Lights are copied in snapshot order; when the fixed capacity is exceeded,
 * the first `kMaxRenderLights` survive and `droppedLightCount` reports the
 * number omitted. The whole object is trivially copyable for one upload.
 */
struct alignas(16) RenderFrameData {
    RenderFrameHeaderData header{};
    RenderFrameCameraData camera{};
    Vec4 ambientColorIntensity{};
    std::array<RenderFrameLightData, kMaxRenderLights> lights{};
    /** World-to-light clip transform used by the optional directional shadow pass. */
    Mat4 shadowViewProjection{};
    /**
     * x is seconds since the backend started; the rest is reserved.
     *
     * Appended last on purpose: the shaders declare only the prefix they read,
     * so growing the block this way leaves every existing offset untouched.
     */
    Vec4 frameTime{};
    /**
     * x exposure, y contrast, z saturation, w vignette.
     *
     * Appended after `frameTime` for the same reason it was: only the shaders
     * that read the grade declare this far into the block, so every plain
     * forward-shading shader keeps its existing offsets.
     */
    Vec4 grade{};
    /** x bloom threshold, y bloom intensity, z bloom radius, w chromatic aberration. */
    Vec4 postFx{};
    /**
     * Per-frame context for shading a water surface.
     *
     * x is the live disturbance count, y is 1 when the camera sits below a
     * water surface, z is the angular size of one pixel in radians, w reserved.
     * Carried as floats so the block needs no second integer vector, and read
     * back with a cast where a count is wanted.
     */
    Vec4 surfaceInfo{};
    /** x density, y height falloff, z base height, w anisotropy. */
    Vec4 fog{};
    /** x sun scattering, y ambient scattering, z march steps, w march distance. */
    Vec4 fogLight{};
    /** rgb linear sky overhead; w the clock in hours, which clouds drift on. */
    Vec4 zenithColor{};
    /** rgb linear sky at the horizon. */
    Vec4 horizonColor{};
    /** x coverage, y density, z floor altitude, w weather cell size. */
    Vec4 cloud{};
    /** x fine march steps, y drift speed, z seconds since start, w light gain. */
    Vec4 cloudDetail{};
    /**
     * Shape of the layer itself.
     *
     * x thickness, y detail cell size, z detail strength, w cloud type. Appended
     * after cloudDetail rather than merged into it, because every offset before
     * this one is pinned by the shaders that declare less than the whole block:
     * a stage reading up to c cloudDetail cannot be moved by a field that was
     * added later.
     */
    Vec4 cloudShape{};
    /** x coarse steps, y light steps, z ambient gain, w reserved. */
    Vec4 cloudMarch{};
    /** xyz unit direction toward the moon; w how brightly it lights the night. */
    Vec4 celestial{};
    /**
     * Water surfaces a dry fragment shades itself against for the "wet near
     * water" look, each packed as xy centre, z world-space surface height, w
     * radius (see RenderWaterBodySnapshot). A radius of zero is an empty slot.
     *
     * Appended last, after celestial, for the same append-only reason as
     * everything above it: only rayhit.rchit declares this far into the
     * block, so every plain forward-shading shader keeps its existing offsets.
     */
    std::array<Vec4, kMaxWetnessBodies> wetnessBodies{};
};

static_assert(sizeof(RenderFrameHeaderData) == 16);
static_assert(sizeof(RenderFrameCameraData) == 128);
static_assert(sizeof(RenderFrameLightData) == 64);
static_assert(std::is_standard_layout_v<RenderFrameData>);
static_assert(std::is_trivially_copyable_v<RenderFrameData>);
static_assert(offsetof(RenderFrameData, camera) == 16);
static_assert(offsetof(RenderFrameData, ambientColorIntensity) == 144);
static_assert(offsetof(RenderFrameData, lights) == 160);
static_assert(offsetof(RenderFrameData, shadowViewProjection) == 160 + kMaxRenderLights * 64);
static_assert(offsetof(RenderFrameData, frameTime) == 160 + kMaxRenderLights * 64 + 64);
static_assert(offsetof(RenderFrameData, grade) == 160 + kMaxRenderLights * 64 + 64 + 16);
static_assert(offsetof(RenderFrameData, postFx) == 160 + kMaxRenderLights * 64 + 64 + 32);
static_assert(offsetof(RenderFrameData, surfaceInfo) ==
              160 + kMaxRenderLights * 64 + 64 + 48);
static_assert(offsetof(RenderFrameData, fog) == 160 + kMaxRenderLights * 64 + 64 + 64);
static_assert(offsetof(RenderFrameData, cloud) == 160 + kMaxRenderLights * 64 + 64 + 128);
static_assert(offsetof(RenderFrameData, cloudShape) == 160 + kMaxRenderLights * 64 + 64 + 160);
static_assert(offsetof(RenderFrameData, cloudMarch) == 160 + kMaxRenderLights * 64 + 64 + 176);
static_assert(offsetof(RenderFrameData, celestial) == 160 + kMaxRenderLights * 64 + 64 + 192);
static_assert(offsetof(RenderFrameData, wetnessBodies) == 160 + kMaxRenderLights * 64 + 64 + 208);
static_assert(sizeof(RenderFrameData) ==
              160 + kMaxRenderLights * 64 + 64 + 208 + kMaxWetnessBodies * 16);
static_assert(alignof(RenderFrameData) == 16);

/**
 * Builds a bounded, GPU-layout-compatible block from a render snapshot.
 *
 * @param timeSeconds Seconds since start, driving time-based shading such as
 *        the analytic water surface. Defaulted so callers that do not animate
 *        anything need not thread a clock through.
 */
RenderFrameData BuildRenderFrameData(const RenderSceneSnapshot& snapshot,
                                     f32 timeSeconds = 0.0f);

/** Returns a borrowed byte view suitable for the current frame-buffer upload. */
[[nodiscard]] std::span<const std::byte> RenderFrameDataBytes(
    const RenderFrameData& data) noexcept;

} // namespace Concord

#endif // CONCORD_RENDERFRAMEDATA_H
