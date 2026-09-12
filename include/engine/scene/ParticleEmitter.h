// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SCENE_PARTICLEEMITTER_H
#define CONCORD_SCENE_PARTICLEEMITTER_H

#include "engine/core/Transform.h"
#include "engine/ecs/ParticleComponents.h"
#include "engine/ecs/World.h"

namespace Concord {
namespace Object {

/** Placement and behaviour of one particle emitter. */
struct ParticleEmitterDesc {
    /** Where the emitter sits and which way its emission cone points. */
    Transform transform{};

    /** Spawn volume, rate, ranges, forces and colour ramp. */
    ParticleEmitterSettings settings{};
};

/**
 * Spawnable archetype for a particle emitter.
 *
 * Stores no state: Build attaches the Transform that places it and the
 * component holding its settings and pool. Register a ParticleSystem on the
 * Game for the pool to advance.
 */
struct ParticleEmitter {
    using Desc = ParticleEmitterDesc;

    static void Build(World& world, Entity entity, const ParticleEmitterDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<ParticleEmitterComponent>(
            entity, ParticleEmitterComponent{.settings = desc.settings});
    }
};

} // namespace Object
} // namespace Concord

#endif // CONCORD_SCENE_PARTICLEEMITTER_H
