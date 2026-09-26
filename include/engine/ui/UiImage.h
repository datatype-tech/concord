// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef CONCORD_UIIMAGE_H
#define CONCORD_UIIMAGE_H
#include "engine/core/Types.h"
#include <vector>

namespace Concord {
/** Straight-alpha RGBA8 pixels waiting for the renderer to create a UI texture. */
struct UiImagePixels {
    u32 id = 0;
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> rgba;
};
}
#endif
