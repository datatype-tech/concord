// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_GAME_H
#define CONCORD_GAME_H

#include "Concord/CExport.h"
#include "engine/app/GameConfig.h"
#include "engine/core/Types.h"
#include "engine/ecs/SystemSchedule.h"

#include <functional>
#include <memory>

namespace Concord {

class Scene;
class Window;

/**
 * Entry point for the engine's lifecycle.
 *
 * A Game owns at most one window and one active scene, and drives the frame
 * loop that ties them together. The renderer it owns is hidden behind the
 * pimpl, so this header stays free of Vulkan and SDL declarations.
 */
class CENGINE_API Game {
public:
    /**
     * @param config Subsystem switches; defaults bring up rendering with
     *        validation enabled in Debug builds.
     */
    explicit Game(GameConfig config = {});
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    /**
     * Opens `window` and binds it to this Game's renderer.
     *
     * Takes a non-const reference because attaching also makes `window` a
     * live handle: later Set() calls on it reach the open OS window. The
     * window must outlive the Game, or be detached first. A Game owns at
     * most one window, so calling this again replaces the previous one.
     */
    void AttachWindow(Window& window);

    /** Closes the attached window ahead of destruction, if any. */
    void DetachWindow();

    /**
     * Makes `scene` the active scene: its entities start being rendered.
     *
     * A Game holds at most one active scene, so this deactivates whichever
     * was loaded before. Calls made during Run() are applied at the next safe
     * frame boundary. `scene` must outlive the Game or the next LoadScene
     * call, whichever comes first.
     */
    void LoadScene(Scene& scene);

    /**
     * Registers a callback invoked once per frame with the elapsed time in
     * seconds. Passing an empty std::function stops the ticking.
     */
    void OnUpdate(std::function<void(f32 deltaTime)> onUpdate);

    /**
     * The systems ticked each frame, before rendering.
     *
     * Systems are the data-oriented counterpart to OnUpdate: register one
     * with `game.Systems().Add<MySystem>()` to run logic over components in
     * bulk rather than object by object.
     */
    [[nodiscard]] SystemSchedule& Systems() noexcept;

    /**
     * Runs the frame loop until the attached window is closed.
     *
     * Returns immediately when no window is attached, so a headless Game
     * does not spin.
     */
    void Run();

    /** Requests that the loop started by Run() finish after the current frame. */
    void Quit() noexcept;

    /** Seconds elapsed between the two most recently processed frames. */
    [[nodiscard]] f32 DeltaTime() const noexcept;

    /** Frame-loop iterations processed since Run() started. */
    [[nodiscard]] u64 FrameCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Concord

#endif // CONCORD_GAME_H
