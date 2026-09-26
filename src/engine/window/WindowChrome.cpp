// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "Concord/CWindow.h"
#include "engine/window/WindowImpl.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <utility>

#if defined(_WIN32)
#include <dwmapi.h>
#include <windows.h>
#endif

namespace Concord {
void ApplyWindowsChrome(SDL_Window* window)
{
#if defined(_WIN32)
    const auto properties = SDL_GetWindowProperties(window);
    const auto hwnd = static_cast<HWND>(SDL_GetPointerProperty(
        properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) {
        return;
    }

    const DWORD cornerPreference = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &cornerPreference, sizeof(cornerPreference));
    const BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &darkMode, sizeof(darkMode));
#else
    (void)window;
#endif
}

namespace {
SDL_HitTestResult SDLCALL HitTest(SDL_Window* window, const SDL_Point* point, void* data)
{
    const auto& state = *static_cast<const WindowState*>(data);
    const auto flags = SDL_GetWindowFlags(window);
    if (!(flags & SDL_WINDOW_BORDERLESS) || (flags & SDL_WINDOW_FULLSCREEN)) return SDL_HITTEST_NORMAL;
    if (state.desc.resizable && !(flags & SDL_WINDOW_MAXIMIZED)) {
        int width=0,height=0; SDL_GetWindowSize(window,&width,&height);
        const int edge=std::max(5,static_cast<int>(6*SDL_GetWindowDisplayScale(window)));
        const bool left=point->x<edge,right=point->x>=width-edge,top=point->y<edge,bottom=point->y>=height-edge;
        if(top && left)return SDL_HITTEST_RESIZE_TOPLEFT;
        if(top && right)return SDL_HITTEST_RESIZE_TOPRIGHT;
        if(bottom && left)return SDL_HITTEST_RESIZE_BOTTOMLEFT;
        if(bottom && right)return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
        if(left)return SDL_HITTEST_RESIZE_LEFT;
        if(right)return SDL_HITTEST_RESIZE_RIGHT;
        if(top)return SDL_HITTEST_RESIZE_TOP;
        if(bottom)return SDL_HITTEST_RESIZE_BOTTOM;
    }
    const auto inside=[point](Vec2 position,Vec2 size) {
        return point->x>=position.x && point->y>=position.y && point->x<position.x+size.x && point->y<position.y+size.y;
    };
    if(!inside(state.dragPosition,state.dragSize))return SDL_HITTEST_NORMAL;
    for(u32 index=0;index<state.dragExclusionCount;++index)
        if(inside(state.dragExclusions[index].position,state.dragExclusions[index].size))return SDL_HITTEST_NORMAL;
    return SDL_HITTEST_DRAGGABLE;
}
bool FiniteRegion(Vec2 position,Vec2 size)
{
    return std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(size.x) && std::isfinite(size.y);
}
}
void InstallWindowHitTest(WindowState& state) {SDL_SetWindowHitTest(state.handle,HitTest,&state);}
void Window::SetDecorated(bool decorated)
{
    auto& state=m_impl->state;state.desc.decorated=decorated;
    if(state.handle)SDL_SetWindowBordered(state.handle,decorated && state.desc.mode==WindowMode::Windowed);
}
void Window::SetDragRegion(Vec2 position,Vec2 size) noexcept
{
    auto& state=m_impl->state;
    state.dragExclusionCount=0;
    if(!FiniteRegion(position,size)) {state.dragSize={};return;}
    state.dragPosition=position;state.dragSize={std::max(0.0f,size.x),std::max(0.0f,size.y)};
}
void Window::ExcludeFromDragRegion(Vec2 position,Vec2 size) noexcept
{
    auto& state=m_impl->state;
    if(!FiniteRegion(position,size) || size.x<=0 || size.y<=0 || state.dragExclusionCount>=state.dragExclusions.size())return;
    state.dragExclusions[state.dragExclusionCount++]={position,size};
}
void Window::Minimize() noexcept {if(m_impl->state.handle)SDL_MinimizeWindow(m_impl->state.handle);}
void Window::Maximize() noexcept {if(m_impl->state.handle)SDL_MaximizeWindow(m_impl->state.handle);}
void Window::Restore() noexcept {if(m_impl->state.handle)SDL_RestoreWindow(m_impl->state.handle);}
bool Window::IsMinimized() const noexcept {return m_impl->state.handle && (SDL_GetWindowFlags(m_impl->state.handle)&SDL_WINDOW_MINIMIZED);}
bool Window::IsMaximized() const noexcept {return m_impl->state.handle && (SDL_GetWindowFlags(m_impl->state.handle)&SDL_WINDOW_MAXIMIZED);}
void Window::ToggleFullscreen() {SetMode(Mode()==WindowMode::Fullscreen?m_impl->state.restoreMode:WindowMode::Fullscreen);}
void Window::SetCloseRequestHandler(std::function<bool()> handler)
{
    m_impl->state.closeRequestHandler = std::move(handler);
}
void Window::RequestClose() noexcept {m_impl->state.shouldClose=true;}
}
