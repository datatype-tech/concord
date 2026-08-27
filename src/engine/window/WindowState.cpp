// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/window/WindowState.h"

#include <SDL3/SDL.h>

namespace Concord {

namespace {

/** Whether an event's window id names the window this state owns. */
bool BelongsTo(const WindowState& state, SDL_WindowID id)
{
    return state.handle != nullptr && id == SDL_GetWindowID(state.handle);
}

} // namespace

void PumpWindowEvents(WindowState& state)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            state.shouldClose = true;
            break;

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (BelongsTo(state, event.window.windowID)) {
                state.shouldClose = true;
            }
            break;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (BelongsTo(state, event.window.windowID)) {
                state.pixelWidth = static_cast<u32>(event.window.data1);
                state.pixelHeight = static_cast<u32>(event.window.data2);
                state.resized = true;
            }
            break;

        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                state.shouldClose = true;
            }
            break;

        default:
            break;
        }
    }
}

} // namespace Concord
