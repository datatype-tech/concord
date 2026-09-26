// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WINDOWSTATE_H
#define CONCORD_WINDOWSTATE_H

#include "engine/core/Types.h"
#include "engine/input/InputSnapshot.h"

#include "engine/window/WindowDesc.h"
#include <array>
#include <functional>

struct SDL_Window;

namespace Concord {

/** A client-space rectangle used by native hit testing. */
struct WindowRegion {
    Vec2 position{};
    Vec2 size{};
};

/**
 * The live state behind a Window, kept out of the public header so that
 * including `Window.h` never pulls in SDL.
 *
 * Keyboard, mouse and wheel input is collected into one InputSnapshot that
 * the event pump refreshes per frame.
 */
struct WindowState {
    WindowDesc desc{};
    SDL_Window* handle = nullptr;

    /** Whether this Window owns an SDL video-subsystem reference. */
    bool sdlVideoInitialized = false;

    /** Set when closure is accepted, cleared only by reopening. */
    bool shouldClose = false;
    /** Optional native-close gate; false defers closure until explicitly confirmed. */
    std::function<bool()> closeRequestHandler;

    /** Set when the pixel size changed, cleared by the renderer that reads it. */
    bool resized = false;

    u32 pixelWidth = 0;
    u32 pixelHeight = 0;
    bool mouseCaptured = false;
    Vec2 dragPosition{}, dragSize{};
    /** Interactive holes in the drag region; fixed capacity keeps hit testing allocation-free. */
    std::array<WindowRegion, 64> dragExclusions{};
    u32 dragExclusionCount = 0;
    WindowMode restoreMode = WindowMode::Windowed;
    InputSnapshot input{};
    /** Optional platform UI observer; never owns or retains the event. */
    std::function<void(const void*)> eventObserver;
};

/**
 * Drains the SDL event queue into `state`.
 *
 * Events are polled process-wide, so this filters to the window it owns
 * before recording a close request, a size change, or the input snapshot's
 * per-frame edges.
 */
void PumpWindowEvents(WindowState& state);
/** Installs native hit testing without exposing SDL in the public API. */
void InstallWindowHitTest(WindowState& state);
/** Applies Windows compositor chrome when the platform exposes it. */
void ApplyWindowsChrome(SDL_Window* window);

} // namespace Concord

#endif // CONCORD_WINDOWSTATE_H
