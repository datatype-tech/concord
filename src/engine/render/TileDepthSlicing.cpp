// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/TileDepthSlicing.h"

#include <algorithm>
#include <cmath>

namespace Concord {

u32 DepthSliceFor(f32 viewDepth, f32 nearPlane, f32 farPlane, u32 sliceCount) noexcept
{
    if (sliceCount == 0) {
        return 0;
    }
    const u32 lastSlice = sliceCount - 1;
    // NaN has no position on the range, so it takes the safest slice. An
    // infinite depth is genuinely beyond the far plane and lands with it.
    if (std::isnan(viewDepth) || viewDepth <= nearPlane) {
        return 0;
    }
    if (!std::isfinite(farPlane) || farPlane <= nearPlane) {
        return lastSlice;
    }
    if (viewDepth >= farPlane) {
        return lastSlice;
    }
    // A degenerate range would divide by a zero span; treat it as one slice.
    const f32 span = std::log(farPlane / nearPlane);
    if (!(span > 0.0f)) {
        return lastSlice;
    }
    const f32 ratio = std::log(viewDepth / nearPlane) / span;
    const f32 scaled = ratio * static_cast<f32>(sliceCount);
    if (!std::isfinite(scaled) || scaled <= 0.0f) {
        return 0;
    }
    const u32 slice = static_cast<u32>(scaled);
    return std::min(slice, lastSlice);
}

u32 TileClusterIndex(u32 tileX, u32 tileY, u32 slice, u32 tileColumns, u32 tileRows,
                     u32 sliceCount) noexcept
{
    if (tileColumns == 0 || tileRows == 0 || sliceCount == 0) {
        return 0;
    }
    const u32 x = std::min(tileX, tileColumns - 1);
    const u32 y = std::min(tileY, tileRows - 1);
    const u32 z = std::min(slice, sliceCount - 1);
    return (z * tileRows + y) * tileColumns + x;
}

} // namespace Concord
