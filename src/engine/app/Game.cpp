// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/app/Game.h"

#include "engine/app/GameState.h"
#include "engine/core/Vec2.h"
#include "engine/ui/UiCanvas.h"
#include "engine/window/Window.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>

namespace Concord {
namespace {

/**
 * Loop period, in milliseconds, above which a frame is reported as a stall.
 *
 * Read from `CONCORD_HITCH_MS` once; zero, which is the default, disables the
 * check entirely. A hitch is the one performance defect an average cannot
 * show -- a single 90 ms frame inside a second of 8 ms ones moves the mean by
 * a tenth of a millisecond and is the only thing anybody actually notices --
 * so it needs its own measurement rather than a smaller number in the same
 * counter.
 */
f32 HitchThresholdSeconds() noexcept
{
    static const f32 threshold = [] {
        const char* value = std::getenv("CONCORD_HITCH_MS");
        if (value == nullptr || *value == '\0') {
            return 0.0f;
        }
        const double parsed = std::strtod(value, nullptr);
        return parsed > 0.0 ? static_cast<f32>(parsed / 1000.0) : 0.0f;
    }();
    return threshold;
}

} // namespace

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
                const f32 hitch = HitchThresholdSeconds();
                if (hitch > 0.0f && frameSeconds > hitch) {
                    // Both halves, because they point at different culprits.
                    // Time inside the simulation and the command recording is
                    // the engine's own; time only the full period saw is the
                    // present call, which means the swapchain or the driver.
                    std::fprintf(stderr,
                                 "[hitch] frame %llu  total %.1f ms  cpu %.1f ms  present %.1f ms"
                                 "  entities %llu  particles %u\n",
                                 static_cast<unsigned long long>(impl.frameCount),
                                 static_cast<double>(frameSeconds) * 1000.0,
                                 static_cast<double>(cpuSeconds) * 1000.0,
                                 static_cast<double>(frameSeconds - cpuSeconds) * 1000.0,
                                 static_cast<unsigned long long>(renderScene.EntityCount()),
                                 impl.renderer->LastFrameStats().particles);
                }
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

bool Game::RenderStill(const char* path, const StillRenderDesc& desc)
{
    Impl& impl = *m_impl;
    if (path == nullptr || *path == '\0' || !impl.window || !impl.renderer || impl.running) {
        return false;
    }

    impl.running = true;
    bool captured = false;
    try {
        impl.ApplyPendingScene();
        impl.StartSystems();
        impl.deltaTime = desc.deltaTime;

        // One pass more than the warm-up asks for: the last one is the frame
        // that gets kept, and it is stepped like every other so that nothing in
        // the picture is a half-updated version of what the warm-up settled.
        for (u32 index = 0; index <= desc.warmupFrames; ++index) {
            const bool keeping = index == desc.warmupFrames;
            // Pumped even though nothing is watching: a window that never
            // services its queue is a window the OS reports as hung, and a long
            // offline render is exactly long enough for that to happen.
            impl.window->PumpEvents();

            Scene& scene = impl.scene ? *impl.scene : impl.fallbackScene;
            impl.systems.Update(scene, desc.deltaTime);
            scene.FlushDeferred();
            impl.ApplyPendingScene();
            impl.StartSystems();

            Scene& renderScene = impl.scene ? *impl.scene : impl.fallbackScene;
            impl.renderer->SetDebugOverlay(
                desc.includeOverlay && impl.debugOverlay.showDebugInfo ? &impl.debugOverlay.Frame()
                                                                       : nullptr);
            impl.renderer->SetUi(nullptr);
            // Armed before the frame rather than after it, because the copy is
            // recorded inside that frame's own command buffer.
            bool armed = keeping && impl.renderer->CaptureStill(path);
            // A refused BeginFrame is a dropped frame in a live loop and the
            // entire result here, so the kept frame is retried. The request
            // survives a frame that never recorded it, so retrying is just
            // another attempt at the same frame rather than a second request.
            for (u32 attempt = 0; attempt < (keeping ? 8u : 1u); ++attempt) {
                if (impl.renderer->BeginFrame()) {
                    impl.renderer->DrawScene(renderScene);
                    impl.renderer->EndFrame();
                    captured = captured || armed;
                    break;
                }
                if (!keeping) {
                    break;
                }
                impl.window->PumpEvents();
                // The swapchain may have been rebuilt at a different size while
                // this was failing, which retires a request armed against the
                // old one.
                armed = impl.renderer->CaptureStill(path);
            }
            ++impl.frameCount;
        }
    } catch (...) {
        impl.AbortRun();
        throw;
    }
    impl.FinishRun();
    return captured;
}

void Game::Quit() noexcept { m_impl->quitRequested = true; }

f32 Game::DeltaTime() const noexcept { return m_impl->deltaTime; }

u64 Game::FrameCount() const noexcept { return m_impl->frameCount; }

} // namespace Concord
