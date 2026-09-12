// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_AUDIOSYSTEM_H
#define CONCORD_AUDIOSYSTEM_H

#include "Concord/CExport.h"
#include "engine/audio/AudioWorld.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/System.h"

#include <memory>

namespace Concord {

/**
 * Keeps AudioWorld in lockstep with AudioListener / AudioSource.
 *
 * Register with `game.Systems().Add<AudioSystem>()`. Spatial voices hear
 * from the listener's Transform, or from the main camera when no listener
 * was spawned.
 */
class CENGINE_API AudioSystem : public ISystem {
public:
    explicit AudioSystem(const AudioSettings& settings = {});
    ~AudioSystem() override;

    void OnStart(Scene& scene) override;
    void OnUpdate(Scene& scene, f32 deltaTime) override;
    void OnStop(Scene& scene) override;

    [[nodiscard]] AudioWorld& World() noexcept { return *m_world; }
    [[nodiscard]] const AudioWorld& World() const noexcept { return *m_world; }

    bool Play(Entity entity);
    bool Stop(Entity entity);

private:
    AudioSettings m_settings{};
    std::unique_ptr<AudioWorld> m_world;
};

} // namespace Concord

#endif // CONCORD_AUDIOSYSTEM_H
