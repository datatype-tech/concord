// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/ParticleSystem.h"

#include "engine/core/Transform.h"
#include "engine/ecs/ParticleComponents.h"
#include "engine/ecs/World.h"
#include "engine/particle/ParticleSimulation.h"
#include "engine/scene/Scene.h"

namespace Concord {

void ParticleSystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    World& world = scene.GetWorld();
    world.Query<ParticleEmitterComponent, Transform>(
        [deltaTime](Entity, ParticleEmitterComponent& emitter, const Transform& transform) {
            if (emitter.restart) {
                emitter.restart = false;
                if (!ResetParticleEmitter(emitter.settings, emitter.state)) {
                    emitter.lastStep = {};
                    return;
                }
            }
            emitter.lastStep = StepParticleEmitter(emitter.settings, emitter.state,
                                                   transform.ToMatrix(), deltaTime);
        });
}

} // namespace Concord
