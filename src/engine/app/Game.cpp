// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/app/Game.h"

#include "engine/app/GameState.h"
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

            if (impl.onUpdate) {
                impl.onUpdate(impl.deltaTime);
            }

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
                impl.debugOverlay.Update(impl.deltaTime, renderScene.EntityCount(),
                                         impl.renderer->LastFrameStats());
                impl.renderer->SetDebugOverlay(impl.debugOverlay.showDebugInfo
                                                   ? &impl.debugOverlay.Frame()
                                                   : nullptr);
            }
            if (impl.renderer && impl.renderer->BeginFrame()) {
                impl.renderer->DrawScene(renderScene);
                impl.renderer->EndFrame();
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
