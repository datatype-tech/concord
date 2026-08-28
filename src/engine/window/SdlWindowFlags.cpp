// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/window/SdlWindowFlags.h"

namespace Concord {

SDL_WindowFlags ToSdlWindowFlags(const WindowDesc& desc, bool enableVulkan)
{
    SDL_WindowFlags flags = 0;
    if (enableVulkan) {
        flags |= SDL_WINDOW_VULKAN;
    }

    if (desc.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (!desc.visible) {
        flags |= SDL_WINDOW_HIDDEN;
    }

    switch (desc.mode) {
    case WindowMode::Fullscreen:
        flags |= SDL_WINDOW_FULLSCREEN;
        break;
    case WindowMode::Borderless:
        flags |= SDL_WINDOW_BORDERLESS;
        break;
    case WindowMode::Windowed:
        break;
    }
    return flags;
}

void ApplySdlWindowMode(SDL_Window* handle, WindowMode mode)
{
    switch (mode) {
    case WindowMode::Fullscreen:
        SDL_SetWindowBordered(handle, true);
        SDL_SetWindowFullscreen(handle, true);
        break;
    case WindowMode::Borderless:
        SDL_SetWindowFullscreen(handle, false);
        SDL_SetWindowBordered(handle, false);
        break;
    case WindowMode::Windowed:
        SDL_SetWindowFullscreen(handle, false);
        SDL_SetWindowBordered(handle, true);
        break;
    }
}

} // namespace Concord
