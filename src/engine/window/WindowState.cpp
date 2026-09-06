// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/window/WindowState.h"

#include "engine/input/SdlInputCodes.h"

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
    BeginInputFrame(state.input);

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
            if (!BelongsTo(state, event.key.windowID)) break;
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                if (state.mouseCaptured) {
                    SDL_SetWindowRelativeMouseMode(state.handle, false);
                    state.mouseCaptured = false;
                }
                break;
            }
            SetKeyState(state.input, KeyCodeFromSdlScanCode(event.key.scancode), true);
            break;

        case SDL_EVENT_KEY_UP:
            if (BelongsTo(state, event.key.windowID)) {
                SetKeyState(state.input, KeyCodeFromSdlScanCode(event.key.scancode), false);
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (!BelongsTo(state, event.motion.windowID)) break;
            if (state.mouseCaptured) {
                state.input.mouseDelta.x += event.motion.xrel;
                state.input.mouseDelta.y += event.motion.yrel;
            }
            state.input.mousePosition = {event.motion.x, event.motion.y};
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (BelongsTo(state, event.button.windowID)) {
                MouseButton button{};
                if (MouseButtonFromSdlButton(event.button.button, button)) {
                    SetButtonState(state.input, button, true);
                }
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (BelongsTo(state, event.button.windowID)) {
                MouseButton button{};
                if (MouseButtonFromSdlButton(event.button.button, button)) {
                    SetButtonState(state.input, button, false);
                }
            }
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if (BelongsTo(state, event.wheel.windowID)) {
                f32 x = event.wheel.x;
                f32 y = event.wheel.y;
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                    x = -x;
                    y = -y;
                }
                state.input.wheelDelta += Vec2{x, y};
            }
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (BelongsTo(state, event.window.windowID)) {
                if (state.mouseCaptured) {
                    SDL_SetWindowRelativeMouseMode(state.handle, false);
                }
                state.mouseCaptured = false;
                ReleaseAllInput(state.input);
            }
            break;

        default:
            break;
        }
    }
}

} // namespace Concord
