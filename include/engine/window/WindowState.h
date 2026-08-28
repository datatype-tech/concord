// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WINDOWSTATE_H
#define CONCORD_WINDOWSTATE_H

#include "engine/core/Types.h"
#include "engine/window/WindowDesc.h"

struct SDL_Window;

namespace Concord {

/**
 * The live state behind a Window, kept out of the public header so that
 * including `Window.h` never pulls in SDL.
 */
struct WindowState {
    WindowDesc desc{};
    SDL_Window* handle = nullptr;

    /** Whether this Window owns an SDL video-subsystem reference. */
    bool sdlVideoInitialized = false;

    /** Set when the user asked to close, cleared only by reopening. */
    bool shouldClose = false;

    /** Set when the pixel size changed, cleared by the renderer that reads it. */
    bool resized = false;

    u32 pixelWidth = 0;
    u32 pixelHeight = 0;
};

/**
 * Drains the SDL event queue into `state`.
 *
 * Events are polled process-wide, so this filters to the window it owns
 * before recording a close request or a size change.
 */
void PumpWindowEvents(WindowState& state);

} // namespace Concord

#endif // CONCORD_WINDOWSTATE_H
