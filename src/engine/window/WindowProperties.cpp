// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/window/Window.h"

#include "engine/window/WindowImpl.h"
#include "engine/window/SdlWindowFlags.h"

#include <SDL3/SDL.h>

#include <utility>

namespace Concord {

void Window::Set(WindowDesc desc)
{
    WindowState& state = m_impl->state;
    state.desc = std::move(desc);
    if (!state.handle) {
        return;
    }

    SDL_SetWindowTitle(state.handle, state.desc.title.c_str());
    SDL_SetWindowResizable(state.handle, state.desc.resizable);
    SDL_SetWindowSize(state.handle, static_cast<int>(state.desc.resolution.width),
                      static_cast<int>(state.desc.resolution.height));
    ApplySdlWindowMode(state.handle, state.desc.mode);
    SetVisible(state.desc.visible);
    SDL_SyncWindow(state.handle);

    int width = 0;
    int height = 0;
    if (SDL_GetWindowSizeInPixels(state.handle, &width, &height)) {
        state.pixelWidth = static_cast<u32>(width);
        state.pixelHeight = static_cast<u32>(height);
    }
    state.resized = true;
}

void Window::SetTitle(std::string title)
{
    WindowState& state = m_impl->state;
    state.desc.title = std::move(title);
    if (state.handle) {
        SDL_SetWindowTitle(state.handle, state.desc.title.c_str());
    }
}

void Window::SetMode(WindowMode mode)
{
    WindowDesc desc = m_impl->state.desc;
    desc.mode = mode;
    Set(std::move(desc));
}

void Window::SetVisible(bool visible)
{
    WindowState& state = m_impl->state;
    state.desc.visible = visible;
    if (!state.handle) {
        return;
    }
    if (visible) {
        SDL_ShowWindow(state.handle);
    } else {
        SDL_HideWindow(state.handle);
    }
}

} // namespace Concord
