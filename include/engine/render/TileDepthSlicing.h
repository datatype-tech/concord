// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_TILEDEPTHSLICING_H
#define CONCORD_TILEDEPTHSLICING_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

namespace Concord {

/** Depth slices each tile column is divided into. */
inline constexpr u32 kTileDepthSlices = 4;

/**
 * Maps a positive view-space depth to a slice in [0, sliceCount).
 *
 * Slices are spaced logarithmically rather than linearly. Perspective depth
 * precision collapses toward the near plane, so equal linear bands would
 * spend most of their slices on the far distance that needs them least and
 * leave the near field — where lights are both denser and more visible —
 * sharing a single slice.
 *
 * Geometry at or in front of the near plane lands in slice 0 and anything at
 * or beyond the far plane in the last slice, so a fragment outside the
 * authored range still resolves to a slice that exists rather than to an
 * index the light list does not cover.
 *
 * The compute pass culls per slice and the fragment pass looks the slice up
 * again, so both must reach the same answer for the same depth.
 */
[[nodiscard]] CRENDER_API u32 DepthSliceFor(f32 viewDepth, f32 nearPlane, f32 farPlane,
                                            u32 sliceCount) noexcept;

/**
 * Linear index of one cluster in a (tileX, tileY, slice) grid.
 *
 * Tiles vary fastest so a fragment's slice neighbours in depth sit one
 * row-stride apart, which keeps a single tile's slices contiguous. Every
 * coordinate is clamped, so a shader that computes a slightly out-of-grid
 * coordinate resolves to a real cluster instead of reading past the list.
 */
[[nodiscard]] CRENDER_API u32 TileClusterIndex(u32 tileX, u32 tileY, u32 slice,
                                               u32 tileColumns, u32 tileRows,
                                               u32 sliceCount) noexcept;

} // namespace Concord

#endif // CONCORD_TILEDEPTHSLICING_H
