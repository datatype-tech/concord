// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/app/Game.h"
#include "engine/core/Types.h"
#include "engine/core/Vec2.h"
#include "engine/cvm/CvmSceneRegistry.h"
#include "engine/cvm/CvmPhysicsRuntime.h"
#include "engine/cvm/CvmSystems.h"
#include "engine/debug/DebugOverlay.h"
#include "engine/input/InputSnapshot.h"
#include "engine/input/KeyCode.h"
#include "engine/scene/Scene.h"
#include "engine/ui/UiCanvas.h"
#include "engine/window/Resolution.h"
#include "engine/window/Window.h"
#include "engine/window/WindowMode.h"

#include <cstdint>
#include <string>

/**
 * The lifecycle, frame-callback and input half of the CVM C ABI.
 *
 * The callback and the window are process-wide rather than per-scene because a
 * CVM program has exactly one loop: a script declares globals and functions,
 * and exactly one of them runs the scene.
 */
namespace {

/** The revision of the CVM C ABI this module implements. */
constexpr std::int64_t kCvmApiVersion = 12;

/** Address of the script's frame callback, or 0 when none is registered. */
std::int64_t g_updateCallback = 0;

/** The window whose input a script may query; set only while a scene runs. */
Concord::Window* g_activeWindow = nullptr;

/** The game currently inside Run(); needed so a callback can quit. */
Concord::Game* g_activeGame = nullptr;

/** Seconds accumulated inside the current Run(), advanced in InvokeUpdate. */
double g_gameTime = 0.0;

/** Clears the active window however Run() exits, including by exception. */
struct WindowScope {
    explicit WindowScope(Concord::Window* window, Concord::Game* game) noexcept
    {
        g_activeWindow = window;
        g_activeGame = game;
    }
    ~WindowScope()
    {
        g_activeWindow = nullptr;
        g_activeGame = nullptr;
    }
    WindowScope(const WindowScope&) = delete;
    WindowScope& operator=(const WindowScope&) = delete;
};

/** Clamps a requested pixel dimension into a size a window can actually use. */
Concord::u32 Pixels(std::int64_t value)
{
    if (value < 1) return 1;
    if (value > 16384) return 16384;
    return static_cast<Concord::u32>(value);
}

/** Maps an ABI integer onto a real key, rejecting values outside the enum. */
bool ToKeyCode(std::int64_t value, Concord::KeyCode& key)
{
    if (value <= 0) return false;
    if (value >= static_cast<std::int64_t>(Concord::kKeyCodeCount)) return false;
    key = static_cast<Concord::KeyCode>(static_cast<Concord::u16>(value));
    return true;
}

/** Maps an ABI integer onto a mouse button, rejecting values outside the enum. */
bool ToMouseButton(std::int64_t value, Concord::MouseButton& button)
{
    if (value < 0) return false;
    if (value >= static_cast<std::int64_t>(Concord::kMouseButtonCount)) return false;
    button = static_cast<Concord::MouseButton>(static_cast<Concord::u8>(value));
    return true;
}

/**
 * Runs the script's frame callback, if it registered one.
 *
 * Delta time crosses as a double, in seconds. It has to be a double: a frame is
 * about 0.016 seconds, and an integer second count would truncate every frame
 * to zero. The script's callback is therefore i64(f64) -- an i64 result because
 * the engine has nothing to do with what it returns except a negative value,
 * which means "leave the loop after this frame" — the documented way a script
 * binds Escape to quit.
 */
void InvokeUpdate(Concord::f32 deltaSeconds)
{
    if (g_updateCallback == 0) return;
    g_gameTime += static_cast<double>(deltaSeconds);
    using UpdateFn = std::int64_t (*)(double);
    const auto update = reinterpret_cast<UpdateFn>(g_updateCallback);
    const std::int64_t result = update(static_cast<double>(deltaSeconds));
    if (result < 0 && g_activeGame != nullptr) g_activeGame->Quit();
}

} // namespace

extern "C" {

std::int64_t ConcordCvmVersion(void)
{
    return kCvmApiVersion;
}

std::int64_t ConcordCvmSetUpdate(std::int64_t update)
{
    const std::int64_t previous = g_updateCallback;
    g_updateCallback = update;
    return previous;
}

std::int64_t ConcordCvmRunScene(std::int64_t scene, std::int64_t width, std::int64_t height,
                               const char* title)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) return 0;

    // The Window must outlive the Game: AttachWindow borrows it, so both live
    // here for as long as Run() blocks. The Scene outlives both, because the
    // handle table still owns it. A null or empty title is not an error: a
    // script that does not care about the caption should not have to invent one.
    const bool hasTitle = title != nullptr && title[0] != '\0';
    Concord::Window window({.title = hasTitle ? std::string(title) : std::string("ConcordScript"),
                            .resolution = {.width = Pixels(width), .height = Pixels(height)}});
    Concord::Game game;
    game.AttachWindow(window);
    game.LoadScene(*found);
    // A headless concord_physics_step may have bound a CVM-owned solver.
    // Drop it before PhysicsSystem registers, so one Scene never has two worlds.
    Concord::Cvm::ReleaseSimulation(*found);
    // Systems a script asked for are registered here, because this is the first
    // moment a Game exists.
    Concord::Cvm::ApplyRequestedSystems(game, window);
    game.OnUpdate([](Concord::f32 deltaTime) { InvokeUpdate(deltaTime); });
    g_gameTime = 0.0;
    WindowScope scope(&window, &game);
    game.Run();
    Concord::Cvm::ClearLiveSystems();
    return 1;
}

std::int64_t ConcordCvmQuit(void)
{
    if (g_activeGame == nullptr) return 0;
    g_activeGame->Quit();
    return 1;
}

std::int64_t ConcordCvmSetMouseCaptured(std::int64_t captured)
{
    if (g_activeWindow == nullptr) return 0;
    g_activeWindow->SetMouseCaptured(captured != 0);
    return 1;
}

std::int64_t ConcordCvmMouseCaptured(void)
{
    if (g_activeWindow == nullptr) return 0;
    return g_activeWindow->IsMouseCaptured() ? 1 : 0;
}

std::int64_t ConcordCvmWindowWidth(void)
{
    if (g_activeWindow == nullptr) return 0;
    return static_cast<std::int64_t>(g_activeWindow->Width());
}

std::int64_t ConcordCvmWindowHeight(void)
{
    if (g_activeWindow == nullptr) return 0;
    return static_cast<std::int64_t>(g_activeWindow->Height());
}

std::int64_t ConcordCvmSetOverlay(std::int64_t visible)
{
    if (g_activeGame == nullptr) return 0;
    g_activeGame->Overlay().showDebugInfo = visible != 0;
    return 1;
}

std::int64_t ConcordCvmOverlay(void)
{
    if (g_activeGame == nullptr) return 0;
    return g_activeGame->Overlay().showDebugInfo ? 1 : 0;
}

std::int64_t ConcordCvmSetTitle(const char* title)
{
    if (g_activeWindow == nullptr) return 0;
    g_activeWindow->SetTitle(title == nullptr ? std::string() : std::string(title));
    return 1;
}

std::int64_t ConcordCvmSetWindowMode(std::int64_t mode)
{
    if (g_activeWindow == nullptr) return 0;
    if (mode < 0 || mode > 2) return 0;
    g_activeWindow->SetMode(static_cast<Concord::WindowMode>(mode));
    return 1;
}

std::int64_t ConcordCvmWindowMode(void)
{
    if (g_activeWindow == nullptr) return 0;
    return static_cast<std::int64_t>(g_activeWindow->Mode());
}

std::int64_t ConcordCvmFrameCount(void)
{
    if (g_activeGame == nullptr) return 0;
    return static_cast<std::int64_t>(g_activeGame->FrameCount());
}

double ConcordCvmDeltaTime(void)
{
    if (g_activeGame == nullptr) return 0.0;
    return static_cast<double>(g_activeGame->DeltaTime());
}

double ConcordCvmTime(void)
{
    if (g_activeGame == nullptr) return 0.0;
    return g_gameTime;
}

std::int64_t ConcordCvmUiPanel(double x, double y, double width, double height)
{
    if (g_activeGame == nullptr) return 0;
    g_activeGame->Ui().Panel(static_cast<Concord::f32>(x), static_cast<Concord::f32>(y),
                             static_cast<Concord::f32>(width), static_cast<Concord::f32>(height));
    return 1;
}

std::int64_t ConcordCvmUiLabel(double x, double y, const char* text)
{
    if (g_activeGame == nullptr) return 0;
    g_activeGame->Ui().Label(static_cast<Concord::f32>(x), static_cast<Concord::f32>(y),
                             text == nullptr ? "" : text);
    return 1;
}

std::int64_t ConcordCvmUiButton(double x, double y, double width, double height, const char* text)
{
    if (g_activeGame == nullptr) return 0;
    return g_activeGame->Ui().Button(static_cast<Concord::f32>(x), static_cast<Concord::f32>(y),
                                     static_cast<Concord::f32>(width),
                                     static_cast<Concord::f32>(height),
                                     text == nullptr ? "" : text)
               ? 1
               : 0;
}

std::int64_t ConcordCvmKeyDown(std::int64_t key)
{
    Concord::KeyCode code;
    if (g_activeWindow == nullptr || !ToKeyCode(key, code)) return 0;
    return g_activeWindow->IsKeyDown(code) ? 1 : 0;
}

std::int64_t ConcordCvmKeyPressed(std::int64_t key)
{
    Concord::KeyCode code;
    if (g_activeWindow == nullptr || !ToKeyCode(key, code)) return 0;
    return g_activeWindow->WasKeyPressed(code) ? 1 : 0;
}

std::int64_t ConcordCvmKeyReleased(std::int64_t key)
{
    Concord::KeyCode code;
    if (g_activeWindow == nullptr || !ToKeyCode(key, code)) return 0;
    return g_activeWindow->WasKeyReleased(code) ? 1 : 0;
}

double ConcordCvmMouseDelta(std::int64_t axis)
{
    if (g_activeWindow == nullptr) return 0.0;
    if (axis != 0 && axis != 1) return 0.0;
    const Concord::Vec2 delta = g_activeWindow->MouseDelta();
    return static_cast<double>(axis == 0 ? delta.x : delta.y);
}

std::int64_t ConcordCvmMouseDown(std::int64_t button)
{
    Concord::MouseButton code;
    if (g_activeWindow == nullptr || !ToMouseButton(button, code)) return 0;
    return g_activeWindow->IsMouseButtonDown(code) ? 1 : 0;
}

std::int64_t ConcordCvmMousePressed(std::int64_t button)
{
    Concord::MouseButton code;
    if (g_activeWindow == nullptr || !ToMouseButton(button, code)) return 0;
    return g_activeWindow->WasMouseButtonPressed(code) ? 1 : 0;
}

std::int64_t ConcordCvmMouseReleased(std::int64_t button)
{
    Concord::MouseButton code;
    if (g_activeWindow == nullptr || !ToMouseButton(button, code)) return 0;
    return g_activeWindow->WasMouseButtonReleased(code) ? 1 : 0;
}

double ConcordCvmMousePosition(std::int64_t axis)
{
    if (g_activeWindow == nullptr) return 0.0;
    if (axis != 0 && axis != 1) return 0.0;
    const Concord::Vec2 position = g_activeWindow->MousePosition();
    return static_cast<double>(axis == 0 ? position.x : position.y);
}

double ConcordCvmMouseWheel(std::int64_t axis)
{
    if (g_activeWindow == nullptr) return 0.0;
    if (axis != 0 && axis != 1) return 0.0;
    const Concord::Vec2 delta = g_activeWindow->MouseWheelDelta();
    return static_cast<double>(axis == 0 ? delta.x : delta.y);
}

} // extern "C"
