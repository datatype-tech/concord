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

/** Maps SDL scancodes to the stable public Key index. */
bool* KeySlot(WindowState& state, SDL_Scancode scancode) noexcept
{
    switch (scancode) {
    case SDL_SCANCODE_W: return &state.keyDown[static_cast<u32>(Key::W)];
    case SDL_SCANCODE_A: return &state.keyDown[static_cast<u32>(Key::A)];
    case SDL_SCANCODE_S: return &state.keyDown[static_cast<u32>(Key::S)];
    case SDL_SCANCODE_D: return &state.keyDown[static_cast<u32>(Key::D)];
    case SDL_SCANCODE_Q: return &state.keyDown[static_cast<u32>(Key::Q)];
    case SDL_SCANCODE_E: return &state.keyDown[static_cast<u32>(Key::E)];
    case SDL_SCANCODE_SPACE: return &state.keyDown[static_cast<u32>(Key::Space)];
    case SDL_SCANCODE_LSHIFT: return &state.keyDown[static_cast<u32>(Key::Shift)];
    case SDL_SCANCODE_LCTRL: return &state.keyDown[static_cast<u32>(Key::Control)];
    default: return nullptr;
    }
}

} // namespace

void PumpWindowEvents(WindowState& state)
{
    state.mouseDelta = {};
    for (bool& pressed : state.mouseButtonPressed) {
        pressed = false;
    }

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
            if (!BelongsTo(state, event.key.windowID)) {
                break;
            }
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                if (state.mouseCaptured) {
                    SDL_SetWindowRelativeMouseMode(state.handle, false);
                    state.mouseCaptured = false;
                }
                break;
            }
            if (bool* key = KeySlot(state, event.key.scancode)) {
                *key = true;
            }
            break;

        case SDL_EVENT_KEY_UP:
            if (!BelongsTo(state, event.key.windowID)) {
                break;
            }
            if (bool* key = KeySlot(state, event.key.scancode)) {
                *key = false;
            }
            break;

        case SDL_EVENT_MOUSE_MOTION:
            if (BelongsTo(state, event.motion.windowID) && state.mouseCaptured) {
                state.mouseDelta.x += event.motion.xrel;
                state.mouseDelta.y += event.motion.yrel;
            }
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (BelongsTo(state, event.button.windowID)) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    state.mouseButtonPressed[static_cast<u32>(MouseButton::Left)] = true;
                } else if (event.button.button == SDL_BUTTON_RIGHT) {
                    state.mouseButtonPressed[static_cast<u32>(MouseButton::Right)] = true;
                }
            }
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (BelongsTo(state, event.window.windowID)) {
                if (state.mouseCaptured) {
                    SDL_SetWindowRelativeMouseMode(state.handle, false);
                }
                state.mouseCaptured = false;
                state.mouseDelta = {};
                for (bool& key : state.keyDown) {
                    key = false;
                }
                for (bool& pressed : state.mouseButtonPressed) {
                    pressed = false;
                }
            }
            break;

        default:
            break;
        }
    }
}

} // namespace Concord
