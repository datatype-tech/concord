// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/OverlayDrawBatch.h"

int main()
{
    const std::array<float, 4> cyan{0.25f, 1.0f, 0.88f, 1.0f};
    const std::array<float, 4> white{1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<Concord::OverlayDraw> draws;
    // Pixel titles must not consume one draw per lit pixel.
    for (Concord::u32 i = 0; i < 300; ++i) {
        Concord::AppendOverlayDraw(draws, i * 6, 6, cyan);
    }
    if (draws.size() != 1 || draws[0].count != 1800) return 1;
    // Differently colored commands retain order even beyond the former 64-draw cap.
    for (Concord::u32 i = 0; i < 130; ++i) {
        Concord::AppendOverlayDraw(draws, 1800 + i * 6, 6, i % 2 ? cyan : white);
    }
    if (draws.size() != 131) return 2;
    for (Concord::u32 i = 0; i < 130; ++i) {
        const auto& draw = draws[i + 1];
        if (draw.first != 1800 + i * 6 || draw.count != 6 ||
            draw.color != (i % 2 ? cyan : white)) return 3;
    }
    Concord::AppendOverlayDraw(draws, 3000, 0, cyan);
    if (draws.size() != 131) return 4;
    Concord::AppendOverlayDraw(draws, 3000, 6, cyan);
    if (draws.size() != 132 || draws.back().first != 3000) return 5;
    draws.clear();
    Concord::AppendOverlayDraw(draws, 0, 6, white);
    return draws.size() == 1 && draws.front().first == 0 ? 0 : 6;
}
