// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/app/Game.h"

#include "engine/app/GameState.h"
#include "engine/core/Vec2.h"
#include "engine/ui/UiCanvas.h"
#include "engine/window/Window.h"

#include <chrono>
#include <thread>
#include <utility>

namespace Concord {

Game::Game(GameConfig config) : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
}

Game::~Game()
{
    m_impl->AbortRun();
    try {
        DetachWindow();
    } catch (...) {
    }
}

void Game::LoadScene(Scene& scene)
{
    if (m_impl->running) {
        m_impl->pendingScene = &scene;
        return;
    }
    m_impl->ActivateScene(&scene);
}

SystemSchedule& Game::Systems() noexcept { return m_impl->systems; }

DebugOverlay& Game::Overlay() noexcept { return m_impl->debugOverlay; }

UiCanvas& Game::Ui() noexcept { return m_impl->ui; }

void Game::OnUpdate(std::function<void(f32 deltaTime)> onUpdate)
{
    m_impl->onUpdate = std::move(onUpdate);
}

void Game::Run()
{
    Impl& impl = *m_impl;
    if (!impl.window || impl.running) {
        return;
    }

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();

    impl.running = true;
    try {
        impl.quitRequested = false;
        impl.frameCount = 0;
        impl.ApplyPendingScene();
        impl.StartSystems();

        while (!impl.quitRequested && impl.window && !impl.window->ShouldClose()) {
            impl.window->PumpEvents();

            const auto now = Clock::now();
            impl.deltaTime = std::chrono::duration<f32>(now - previous).count();
            previous = now;
            // A stall anywhere outside the loop (present block, resize storm,
            // OS hitch) must not become simulation time: feeding a 200 ms step
            // to controllers and integrators teleports the camera, tunnels
            // bodies and boils the particles, and the correction cascade reads
            // as a second, longer hitch. Fifty milliseconds is the whole debt
            // one frame may carry.
            impl.deltaTime = std::min(impl.deltaTime, 0.05f);

            if (impl.window) {
                const Vec2 mouse = impl.window->MousePosition();
                impl.ui.Begin(mouse.x, mouse.y,
                              impl.window->WasMouseButtonPressed(MouseButton::Left),
                              static_cast<f32>(impl.window->Width()),
                              static_cast<f32>(impl.window->Height()));
            } else {
                impl.ui.Begin(0.0f, 0.0f, false, 0.0f, 0.0f);
            }
            if (impl.onUpdate) {
                impl.onUpdate(impl.deltaTime);
            }
            impl.ui.End();

            if (!impl.window) {
                break;
            }
            impl.ApplyPendingScene();
            impl.StartSystems();

            Scene& scene = impl.scene ? *impl.scene : impl.fallbackScene;
            impl.systems.Update(scene, impl.deltaTime);
            scene.FlushDeferred();
            impl.ApplyPendingScene();
            impl.StartSystems();

            Scene& renderScene = impl.scene ? *impl.scene : impl.fallbackScene;
            if (impl.renderer) {
                // Submit the text built last iteration: this frame's cost is
                // not known until this frame has finished.
                impl.renderer->SetDebugOverlay(impl.debugOverlay.showDebugInfo
                                                   ? &impl.debugOverlay.Frame()
                                                   : nullptr);
                impl.renderer->SetUi(impl.ui.DrawList().commands.empty()
                                         ? nullptr
                                         : &impl.ui.DrawList());
            }
            f32 cpuSeconds = 0.0f;
            if (impl.renderer && impl.renderer->BeginFrame()) {
                impl.renderer->DrawScene(renderScene);
                // Stop the clock before presenting. A vsync-locked swapchain
                // blocks inside EndFrame until the next vblank, so measuring
                // past it would report the refresh rate back as the engine's
                // cost and hide every regression the number exists to show.
                cpuSeconds = std::chrono::duration<f32>(Clock::now() - now).count();
                impl.renderer->EndFrame();
            }
            if (impl.renderer) {
                const f32 frameSeconds = std::chrono::duration<f32>(Clock::now() - now).count();
                impl.debugOverlay.Update(frameSeconds, cpuSeconds, renderScene.EntityCount(),
                                         impl.renderer->LastFrameStats());
            }

            ++impl.frameCount;

            if (impl.config.frameRateLimit > 0) {
                const auto budget =
                    std::chrono::duration<f32>(1.0f / static_cast<f32>(impl.config.frameRateLimit));
                const auto spent = Clock::now() - now;
                if (spent < budget) {
                    std::this_thread::sleep_for(budget - spent);
                }
            }
        }
    } catch (...) {
        impl.AbortRun();
        throw;
    }
    impl.FinishRun();
}

void Game::Quit() noexcept { m_impl->quitRequested = true; }

f32 Game::DeltaTime() const noexcept { return m_impl->deltaTime; }

u64 Game::FrameCount() const noexcept { return m_impl->frameCount; }

} // namespace Concord
