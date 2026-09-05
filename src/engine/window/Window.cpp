// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/window/Window.h"

#include "engine/window/WindowImpl.h"
#include "engine/window/SdlWindowFlags.h"
#include "engine/window/WindowState.h"

#include <SDL3/SDL.h>

#include <utility>

namespace Concord {

Window::Window(WindowDesc desc) : m_impl(std::make_unique<Impl>())
{
    m_impl->state.desc = std::move(desc);
}

Window::~Window()
{
    if (m_impl) {
        Close();
    }
}

const WindowDesc& Window::Desc() const noexcept { return m_impl->state.desc; }
const std::string& Window::Title() const noexcept { return m_impl->state.desc.title; }
WindowMode Window::Mode() const noexcept { return m_impl->state.desc.mode; }
bool Window::Vsync() const noexcept { return m_impl->state.desc.vsync; }
bool Window::IsOpen() const noexcept { return m_impl->state.handle != nullptr; }
bool Window::ShouldClose() const noexcept { return m_impl->state.shouldClose; }

bool Window::IsKeyDown(Key key) const noexcept
{
    const u32 index = static_cast<u32>(key);
    return index < kKeyCount && m_impl->state.keyDown[index];
}

bool Window::WasMouseButtonPressed(MouseButton button) const noexcept
{
    const u32 index = static_cast<u32>(button);
    return index < kMouseButtonCount && m_impl->state.mouseButtonPressed[index];
}

Vec2 Window::MouseDelta() const noexcept { return m_impl->state.mouseDelta; }

void Window::SetMouseCaptured(bool captured) noexcept
{
    WindowState& state = m_impl->state;
    if (!state.handle) return;
    if (SDL_SetWindowRelativeMouseMode(state.handle, captured)) {
        state.mouseCaptured = captured;
        state.mouseDelta = {};
    }
}

bool Window::IsMouseCaptured() const noexcept { return m_impl->state.mouseCaptured; }

u32 Window::Width() const noexcept
{
    const WindowState& state = m_impl->state;
    return state.handle ? state.pixelWidth : state.desc.resolution.width;
}

u32 Window::Height() const noexcept
{
    const WindowState& state = m_impl->state;
    return state.handle ? state.pixelHeight : state.desc.resolution.height;
}

bool Window::Open(bool enableVulkan)
{
    WindowState& state = m_impl->state;
    if (state.handle) {
        return true;
    }
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        return false;
    }
    state.sdlVideoInitialized = true;

    state.handle = SDL_CreateWindow(state.desc.title.c_str(),
                                    static_cast<int>(state.desc.resolution.width),
                                    static_cast<int>(state.desc.resolution.height),
                                    ToSdlWindowFlags(state.desc, enableVulkan));
    if (!state.handle) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        state.sdlVideoInitialized = false;
        return false;
    }

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(state.handle, &width, &height);
    state.pixelWidth = static_cast<u32>(width);
    state.pixelHeight = static_cast<u32>(height);
    state.shouldClose = false;
    state.resized = false;
    return true;
}

void Window::Close()
{
    WindowState& state = m_impl->state;
    if (state.handle) {
        if (state.mouseCaptured) {
            SDL_SetWindowRelativeMouseMode(state.handle, false);
        }
        state.mouseCaptured = false;
        SDL_DestroyWindow(state.handle);
        state.handle = nullptr;
    }
    if (state.sdlVideoInitialized) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        state.sdlVideoInitialized = false;
    }
}

void Window::PumpEvents() { PumpWindowEvents(m_impl->state); }

void* Window::NativeHandle() const noexcept { return m_impl->state.handle; }

bool Window::ConsumeResizeFlag() noexcept
{
    const bool was = m_impl->state.resized;
    m_impl->state.resized = false;
    return was;
}

} // namespace Concord
