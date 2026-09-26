// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SDLWINDOWFLAGS_H
#define CONCORD_SDLWINDOWFLAGS_H

#include "engine/window/WindowDesc.h"

#include <SDL3/SDL.h>

namespace Concord {

/**
 * Maps a window description and renderer request onto SDL creation flags.
 *
 * Module-private: it names SDL types, so it lives beside the sources that
 * already include SDL and never reaches a public header.
 */
SDL_WindowFlags ToSdlWindowFlags(const WindowDesc& desc, bool enableVulkan);

/**
 * Applies the presentation mode to an already-open SDL window. An undecorated
 * window never gains a native frame on the way in or out of fullscreen, which
 * would otherwise flash a system caption and shift the client area.
 */
void ApplySdlWindowMode(SDL_Window* handle, WindowMode mode, bool decorated);

} // namespace Concord

#endif // CONCORD_SDLWINDOWFLAGS_H
