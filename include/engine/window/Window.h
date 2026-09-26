// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WINDOW_H
#define CONCORD_WINDOW_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"
#include "engine/core/Vec2.h"
#include "engine/input/InputSnapshot.h"
#include "engine/window/WindowDesc.h"

#include <functional>
#include <memory>
#include <string>

namespace Concord {

class Game;
struct WindowAccess;

/**
 * A window the application asks the engine to open.
 *
 * Constructing one creates no OS window: it is only a specification, built
 * from a WindowDesc so a caller can name just the fields it cares about
 * (`Window({.title = "My Game"})`). The real platform window appears once
 * the spec is handed to Game::AttachWindow, after which this same object
 * doubles as a live handle whose Set() calls reach the open window. A Window
 * has stable identity and is intentionally non-movable, so an attached Game
 * never retains a pointer to a moved-from handle.
 *
 * SDL lives entirely behind the pimpl, so including this header never drags
 * `SDL3/SDL.h` into application code.
 */
class CENGINE_API Window {
public:
    explicit Window(WindowDesc desc = {});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    /** The full current description. */
    [[nodiscard]] const WindowDesc& Desc() const noexcept;

    [[nodiscard]] const std::string& Title() const noexcept;

    /** Client width in pixels; the live size once open, else the requested one. */
    [[nodiscard]] u32 Width() const noexcept;

    /** Client height in pixels; the live size once open, else the requested one. */
    [[nodiscard]] u32 Height() const noexcept;

    [[nodiscard]] WindowMode Mode() const noexcept;
    [[nodiscard]] bool Vsync() const noexcept;

    /** Whether a real OS window is currently open. */
    [[nodiscard]] bool IsOpen() const noexcept;

    /** Whether closure has been accepted; remains true until the window reopens. */
    [[nodiscard]] bool ShouldClose() const noexcept;

    /**
     * Replaces this window's description wholesale and, when open, pushes
     * the change to the live OS window. Fields left unnamed in `desc` take
     * WindowDesc's defaults, not the window's current values.
     */
    void Set(WindowDesc desc);

    /** Changes only the caption. */
    void SetTitle(std::string title);

    /** Changes only the presentation mode. */
    void SetMode(WindowMode mode);

    /** Shows or hides the window. */
    void SetVisible(bool visible);

    /** Enables or removes the native frame without changing the fullscreen state. */
    void SetDecorated(bool decorated);
    /** Sets the draggable rectangle in client coordinates; zero size disables it.
     * Keep interactive title-bar buttons outside this rectangle. Borderless resize
     * edges are handled by the engine whenever resizing is enabled. */
    void SetDragRegion(Vec2 position, Vec2 size) noexcept;
    /** Minimizes the attached native window. */
    void Minimize() noexcept;
    /** Maximizes to the monitor work area, preserving the taskbar. */
    void Maximize() noexcept;
    /** Restores a minimized or maximized window. */
    void Restore() noexcept;
    /** Toggles fullscreen and returns to the preceding windowed mode. */
    void ToggleFullscreen();
    [[nodiscard]] bool IsMinimized() const noexcept;
    [[nodiscard]] bool IsMaximized() const noexcept;
    /**
     * Sets a handler for native close requests, including Alt+F4 and system quit.
     * Return false to keep the window open, for example while confirming unsaved
     * edits. An empty handler restores automatic acceptance. Invoked on the
     * event-pump thread; captured objects must outlive the installed handler.
     */
    void SetCloseRequestHandler(std::function<bool()> handler);
    /** Confirms closure and requests a loop exit without invoking the handler. */
    void RequestClose() noexcept;

    [[nodiscard]] bool IsKeyDown(KeyCode key) const noexcept;
    [[nodiscard]] bool WasKeyPressed(KeyCode key) const noexcept;
    [[nodiscard]] bool WasKeyReleased(KeyCode key) const noexcept;
    [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const noexcept;
    [[nodiscard]] bool WasMouseButtonPressed(MouseButton button) const noexcept;
    [[nodiscard]] bool WasMouseButtonReleased(MouseButton button) const noexcept;
    [[nodiscard]] Vec2 MousePosition() const noexcept;
    [[nodiscard]] Vec2 MouseDelta() const noexcept;
    [[nodiscard]] Vec2 MouseWheelDelta() const noexcept;
    void SetMouseCaptured(bool captured) noexcept;
    [[nodiscard]] bool IsMouseCaptured() const noexcept;
    /** The raw per-pump keyboard and mouse state feeding all queries above. */
    [[nodiscard]] const InputSnapshot& Input() const noexcept;

private:
    friend class Game;
    friend struct WindowAccess;

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    /** Creates the OS window, optionally with Vulkan surface support. */
    bool Open(bool enableVulkan);

    /** Destroys the OS window, leaving the description intact. */
    void Close();

    /** Drains the event queue, refreshing the close flag and size cache. */
    void PumpEvents();

    /** Native SDL window pointer, for Vulkan surface creation. */
    [[nodiscard]] void* NativeHandle() const noexcept;

    /** Reads and clears the pending-resize flag the swapchain watches. */
    [[nodiscard]] bool ConsumeResizeFlag() noexcept;
};

} // namespace Concord

#endif // CONCORD_WINDOW_H
