// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_OVERLAYDRAWBATCH_H
#define CONCORD_OVERLAYDRAWBATCH_H

#include "engine/core/Types.h"

#include <array>
#include <vector>

namespace Concord {

/** A contiguous range of overlay vertices sharing one push-constant color. */
struct OverlayDraw {
    u32 first = 0;
    u32 count = 0;
    std::array<f32, 4> color = {1.0f, 1.0f, 1.0f, 1.0f};
};

/** Preserves painter order while merging adjacent, identically colored geometry. */
inline void AppendOverlayDraw(std::vector<OverlayDraw>& draws, u32 first, u32 count,
                              const std::array<f32, 4>& color)
{
    if (count == 0) return;
    if (!draws.empty()) {
        OverlayDraw& previous = draws.back();
        if (previous.first + previous.count == first && previous.color == color) {
            previous.count += count;
            return;
        }
    }
    draws.push_back({first, count, color});
}

} // namespace Concord

#endif // CONCORD_OVERLAYDRAWBATCH_H
