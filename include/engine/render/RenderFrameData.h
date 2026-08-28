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
static_assert(sizeof(RenderFrameData) == 160 + kMaxRenderLights * 64 + 64);
static_assert(alignof(RenderFrameData) == 16);

/** Builds a bounded, GPU-layout-compatible block from a render snapshot. */
RenderFrameData BuildRenderFrameData(const RenderSceneSnapshot& snapshot);

/** Returns a borrowed byte view suitable for the current frame-buffer upload. */
[[nodiscard]] std::span<const std::byte> RenderFrameDataBytes(
    const RenderFrameData& data) noexcept;

} // namespace Concord

#endif // CONCORD_RENDERFRAMEDATA_H
