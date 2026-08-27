// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/app/Game.h"

#include "engine/render/RenderBackendFactory.h"
#include "engine/scene/Scene.h"
#include "engine/window/Window.h"

#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

namespace Concord {

struct Game::Impl {
    GameConfig config{};
    Window* window = nullptr;
    Scene* scene = nullptr;
    std::unique_ptr<IRenderBackend> renderer;
    std::function<void(f32)> onUpdate;
    SystemSchedule systems;
    bool systemsStarted = false;

    bool quitRequested = false;
    f32 deltaTime = 0.0f;
    u64 frameCount = 0;

    /** Empty scene rendered when the caller never called LoadScene. */
    Scene fallbackScene;
};

Game::Game(GameConfig config) : m_impl(std::make_unique<Impl>())
{
    m_impl->config = config;
}

Game::~Game()
{
    if (m_impl->renderer) {
        m_impl->renderer->Shutdown();
        m_impl->renderer.reset();
    }
    if (m_impl->window) {
        m_impl->window->Close();
        m_impl->window = nullptr;
    }
}

void Game::AttachWindow(Window& window)
{
    DetachWindow();

    if (!window.Open()) {
        std::fprintf(stderr, "[Concord] failed to open window\n");
        return;
    }
    m_impl->window = &window;

    if (!m_impl->config.enableRendering) {
        return;
    }

    std::unique_ptr<IRenderBackend> backend = CreateRenderBackend();
    if (!backend) {
        std::fprintf(stderr, "[Concord] no render backend is registered; "
                              "link the engine's render DLL to enable rendering\n");
        return;
    }

#if defined(NDEBUG)
    const bool validation = false;
#else
    const bool validation = m_impl->config.enableValidation;
#endif

    if (!backend->Init(window, validation)) {
        std::fprintf(stderr, "[Concord] renderer initialization failed\n");
        return;
    }
    m_impl->renderer = std::move(backend);
}

void Game::DetachWindow()
{
    if (m_impl->renderer) {
        m_impl->renderer->Shutdown();
        m_impl->renderer.reset();
    }
    if (m_impl->window) {
        m_impl->window->Close();
        m_impl->window = nullptr;
    }
}

void Game::LoadScene(Scene& scene)
{
    if (m_impl->systemsStarted && m_impl->scene) {
        m_impl->systems.Stop(*m_impl->scene);
        m_impl->systemsStarted = false;
    }
    m_impl->scene = &scene;
}

SystemSchedule& Game::Systems() noexcept { return m_impl->systems; }

void Game::OnUpdate(std::function<void(f32 deltaTime)> onUpdate)
{
    m_impl->onUpdate = std::move(onUpdate);
}

void Game::Run()
{
    Impl& impl = *m_impl;
    if (!impl.window) {
        return;
    }

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();

    impl.quitRequested = false;

    if (impl.scene && !impl.systemsStarted) {
        impl.systems.Start(*impl.scene);
        impl.systemsStarted = true;
    }

    while (!impl.quitRequested && !impl.window->ShouldClose()) {
        impl.window->PumpEvents();

        const auto now = Clock::now();
        impl.deltaTime = std::chrono::duration<f32>(now - previous).count();
        previous = now;

        if (impl.onUpdate) {
            impl.onUpdate(impl.deltaTime);
        }

        Scene& scene = impl.scene ? *impl.scene : impl.fallbackScene;
        impl.systems.Update(scene, impl.deltaTime);

        if (impl.renderer && impl.renderer->BeginFrame()) {
            impl.renderer->DrawScene(scene);
            impl.renderer->EndFrame();
        }

        ++impl.frameCount;

        if (impl.config.frameRateLimit > 0) {
            const auto budget = std::chrono::duration<f32>(1.0f / static_cast<f32>(impl.config.frameRateLimit));
            const auto spent = Clock::now() - now;
            if (spent < budget) {
                std::this_thread::sleep_for(budget - spent);
            }
        }
    }

    if (impl.renderer) {
        impl.renderer->WaitIdle();
    }

    if (impl.systemsStarted && impl.scene) {
        impl.systems.Stop(*impl.scene);
        impl.systemsStarted = false;
    }
}

void Game::Quit() noexcept { m_impl->quitRequested = true; }

f32 Game::DeltaTime() const noexcept { return m_impl->deltaTime; }

u64 Game::FrameCount() const noexcept { return m_impl->frameCount; }

} // namespace Concord