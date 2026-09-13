// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/TileDepthSlicing.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using Concord::f32;
using Concord::u32;

constexpr f32 kNear = 0.1f;
constexpr f32 kFar = 1000.0f;
constexpr u32 kSlices = Concord::kTileDepthSlices;

bool TestRangeEndpoints()
{
    if (Concord::DepthSliceFor(kNear, kNear, kFar, kSlices) != 0) return false;
    if (Concord::DepthSliceFor(0.0f, kNear, kFar, kSlices) != 0) return false;
    if (Concord::DepthSliceFor(-5.0f, kNear, kFar, kSlices) != 0) return false;
    if (Concord::DepthSliceFor(kFar, kNear, kFar, kSlices) != kSlices - 1) return false;
    return Concord::DepthSliceFor(5000.0f, kNear, kFar, kSlices) == kSlices - 1;
}

bool TestMalformedInputResolves()
{
    const f32 nan = std::numeric_limits<f32>::quiet_NaN();
    const f32 inf = std::numeric_limits<f32>::infinity();
    if (Concord::DepthSliceFor(nan, kNear, kFar, kSlices) != 0) return false;
    if (Concord::DepthSliceFor(inf, kNear, kFar, kSlices) != kSlices - 1) return false;
    // A degenerate range must not divide by a zero log span. A depth beyond
    // its single value still resolves; one at the near plane is slice 0.
    if (Concord::DepthSliceFor(5.0f, 1.0f, 1.0f, kSlices) != kSlices - 1) return false;
    if (Concord::DepthSliceFor(1.0f, 1.0f, 1.0f, kSlices) != 0) return false;
    if (Concord::DepthSliceFor(1.0f, kNear, kNear, kSlices) != kSlices - 1) return false;
    // No slices means there is nothing to index.
    return Concord::DepthSliceFor(1.0f, kNear, kFar, 0) == 0;
}

bool TestSliceNeverDecreasesWithDepth()
{
    u32 previous = 0;
    for (u32 step = 0; step <= 400; ++step) {
        // Sweep geometrically so the near field, where slices are dense, gets
        // as many samples as the far field.
        const f32 depth = kNear * std::pow(kFar / kNear, static_cast<f32>(step) / 400.0f);
        const u32 slice = Concord::DepthSliceFor(depth, kNear, kFar, kSlices);
        if (slice < previous || slice >= kSlices) return false;
        previous = slice;
    }
    return true;
}

bool TestSlicesAreLogarithmicNotLinear()
{
    // Bands span a constant ratio, so the first one closes at near * band.
    const f32 band = std::pow(kFar / kNear, 1.0f / static_cast<f32>(kSlices));
    // That boundary must sit far below a linear quarter of the range, which is
    // the whole point of spacing the slices logarithmically: a linear split
    // would put it exactly at the quarter and starve the near field.
    const f32 linearQuarter = kNear + (kFar - kNear) * 0.25f;
    if (!(kNear * band < linearQuarter * 0.01f)) return false;
    for (u32 slice = 1; slice < kSlices; ++slice) {
        const f32 lower = kNear * std::pow(band, static_cast<f32>(slice));
        if (Concord::DepthSliceFor(lower * 1.001f, kNear, kFar, kSlices) != slice) return false;
    }
    return true;
}

bool TestClusterIndexLayout()
{
    const u32 columns = 128;
    const u32 rows = 128;
    if (Concord::TileClusterIndex(0, 0, 0, columns, rows, kSlices) != 0) return false;
    // Tiles vary fastest within a slice.
    if (Concord::TileClusterIndex(1, 0, 0, columns, rows, kSlices) != 1) return false;
    if (Concord::TileClusterIndex(0, 1, 0, columns, rows, kSlices) != columns) return false;
    // Each slice is a contiguous block of tile rows.
    const u32 sliceStride = columns * rows;
    return Concord::TileClusterIndex(0, 0, 1, columns, rows, kSlices) == sliceStride &&
           Concord::TileClusterIndex(5, 7, 3, columns, rows, kSlices) ==
               3 * sliceStride + 7 * columns + 5;
}

bool TestClusterIndexClamps()
{
    const u32 columns = 16;
    const u32 rows = 8;
    const u32 last = Concord::TileClusterIndex(columns - 1, rows - 1, kSlices - 1, columns, rows,
                                               kSlices);
    // Out-of-grid coordinates must land on a real cluster, never past the end.
    if (Concord::TileClusterIndex(999, 999, 999, columns, rows, kSlices) != last) return false;
    if (Concord::TileClusterIndex(0, 0, 0, 0, rows, kSlices) != 0) return false;
    if (Concord::TileClusterIndex(0, 0, 0, columns, 0, kSlices) != 0) return false;
    return Concord::TileClusterIndex(0, 0, 0, columns, rows, 0) == 0;
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"range endpoints", TestRangeEndpoints},
        {"malformed input resolves", TestMalformedInputResolves},
        {"slice never decreases with depth", TestSliceNeverDecreasesWithDepth},
        {"slices are logarithmic not linear", TestSlicesAreLogarithmicNotLinear},
        {"cluster index layout", TestClusterIndexLayout},
        {"cluster index clamps", TestClusterIndexClamps},
    };
    Concord::u32 failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    const Concord::u32 total = static_cast<Concord::u32>(sizeof(cases) / sizeof(cases[0]));
    std::cout << (total - failures) << '/' << total << " depth slicing cases passed\n";
    return failures == 0 ? 0 : 1;
}
