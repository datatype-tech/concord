// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WINDOWDESC_H
#define CONCORD_WINDOWDESC_H

#include "engine/window/Resolution.h"
#include "engine/window/WindowMode.h"

#include <string>

namespace Concord {

/**
 * Every field a Window can be constructed or Set() from.
 *
 * A plain aggregate, so a caller names only the fields it cares about with
 * a designated initializer: `Window({.title = "My Game"})`. Every field
 * carries its own default, so the result is always fully formed. Set()
 * replaces a Window's description wholesale — a field left unnamed falls
 * back to *this type's* default, not to the Window's current value.
 */
struct WindowDesc {
    /** Caption shown in the title bar. */
    std::string title = "Concord Flash";

    /** Client-area size. Ignored while `mode` is Fullscreen. */
    Resolution resolution{};

    /** How the window is presented. */
    WindowMode mode = WindowMode::Windowed;

    /**
     * Whether the user can resize the window by dragging its edges. When on,
     * the renderer rebuilds the swapchain to match each new size.
     */
    bool resizable = true;

    /** Whether the OS window is shown once attached. */
    bool visible = true;

    /**
     * Whether presentation waits for the display's vertical refresh. On
     * selects a FIFO present mode (no tearing, frame rate capped to the
     * refresh rate); off prefers mailbox or immediate.
     */
    bool vsync = true;
};

} // namespace Concord

#endif // CONCORD_WINDOWDESC_H
