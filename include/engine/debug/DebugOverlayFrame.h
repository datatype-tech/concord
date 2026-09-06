// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_DEBUGOVERLAYFRAME_H
#define CONCORD_DEBUGOVERLAYFRAME_H

#include "engine/core/Types.h"

#include <cstddef>

namespace Concord {

/** Characters rendered per overlay line, including the terminator. */
inline constexpr std::size_t kDebugOverlayLineLength = 80;
/** Lines the overlay can carry; additional updates replace earlier lines. */
inline constexpr std::size_t kDebugOverlayMaxLines = 12;

/** One fixed-size text line of the on-screen debug overlay. */
struct DebugOverlayLine {
    char text[kDebugOverlayLineLength] = {};
};

/**
 * The overlay text submitted to the render backend for one frame.
 *
 * Plain aggregate with fixed capacity so the frame path performs no
 * allocation and the pointer handed to the backend stays valid until the
 * next Update. Rendering it is the backend's business; this type carries
 * no Vulkan or SDL knowledge.
 */
struct DebugOverlayFrame {
    bool visible = false;
    u32 lineCount = 0;
    DebugOverlayLine lines[kDebugOverlayMaxLines] = {};
};

} // namespace Concord

#endif // CONCORD_DEBUGOVERLAYFRAME_H
