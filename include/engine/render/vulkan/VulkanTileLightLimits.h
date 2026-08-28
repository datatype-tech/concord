// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANTILELIGHTLIMITS_H
#define CONCORD_VULKANTILELIGHTLIMITS_H

#include "engine/core/Types.h"

namespace Concord {

/** Pixel width and height of one forward-lighting tile. */
inline constexpr u32 kTileSizePixels = 16;

/** Fixed grid dimensions kept stable while a swapchain is being resized. */
inline constexpr u32 kMaxTileColumns = 128;
inline constexpr u32 kMaxTileRows = 128;
inline constexpr u32 kMaxTileCount = kMaxTileColumns * kMaxTileRows;

/** Maximum number of light indices stored by one tile. */
inline constexpr u32 kMaxLightsPerTile = 64;

/** Number of bytes occupied by one std430 tile header. */
inline constexpr usize kTileHeaderBytes = 16;

/** Minimum storage buffer kept bound when full tile culling is unavailable. */
inline constexpr usize kTileFallbackBufferBytes = kTileHeaderBytes;

/** Returns the fixed storage-buffer size for one frame slot. */
[[nodiscard]] constexpr usize TileLightBufferBytes() noexcept
{
    return kMaxTileCount * (kTileHeaderBytes + kMaxLightsPerTile * sizeof(u32));
}

} // namespace Concord

#endif // CONCORD_VULKANTILELIGHTLIMITS_H
