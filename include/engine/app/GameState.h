// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_GAMESTATE_H
#define CONCORD_GAMESTATE_H

#include "engine/app/Game.h"
#include "engine/render/IRenderBackend.h"
#include "engine/scene/Scene.h"

#include <exception>
#include <functional>
#include <memory>

namespace Concord {

/** Runtime-owned state shared by the Game lifecycle translation units. */
struct Game::Impl {
    GameConfig config{};
    Window* window = nullptr;
    Scene* scene = nullptr;
    Scene* pendingScene = nullptr;
    std::unique_ptr<IRenderBackend> renderer;
    std::function<void(f32)> onUpdate;
    SystemSchedule systems;
    bool systemsStarted = false;
    bool running = false;
    bool quitRequested = false;
    f32 deltaTime = 0.0f;
    u64 frameCount = 0;

    /** Empty scene rendered when the caller never called LoadScene. */
    Scene fallbackScene;

    /** Stops the active schedule before changing the active scene pointer. */
    void ActivateScene(Scene* nextScene)
    {
        if (scene == nextScene) {
            return;
        }
        if (systemsStarted) {
            systemsStarted = false;
            if (scene) {
                systems.Stop(*scene);
            }
        }
        scene = nextScene;
    }

    /** Applies a scene request deferred from an active frame callback. */
    void ApplyPendingScene()
    {
        if (pendingScene) {
            Scene* nextScene = pendingScene;
            pendingScene = nullptr;
            ActivateScene(nextScene);
        }
    }

    /** Starts the active scene's schedule while preserving cleanup on failure. */
    void StartSystems()
    {
        if (!scene || systemsStarted) {
            return;
        }
        systems.Start(*scene);
        systemsStarted = true;
    }

    /** Stops the active schedule and clears its state before invoking callbacks. */
    void StopSystems()
    {
        if (!systemsStarted) {
            return;
        }
        systemsStarted = false;
        if (scene) {
            systems.Stop(*scene);
        }
    }

    /** Waits for rendering and stops systems, preserving the first cleanup failure. */
    void FinishRun()
    {
        running = false;
        std::exception_ptr failure;
        try {
            if (renderer) {
                renderer->WaitIdle();
            }
        } catch (...) {
            failure = std::current_exception();
        }
        try {
            StopSystems();
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
        if (failure) {
            std::rethrow_exception(failure);
        }
    }

    /** Best-effort cleanup used while an unrelated frame exception is active. */
    void AbortRun() noexcept
    {
        if (!running && !systemsStarted) {
            return;
        }
        try {
            FinishRun();
        } catch (...) {
        }
    }
};

} // namespace Concord

#endif // CONCORD_GAMESTATE_H
