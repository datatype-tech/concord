// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PARTICLESYSTEM_H
#define CONCORD_PARTICLESYSTEM_H

#include "Concord/CExport.h"
#include "engine/ecs/System.h"

namespace Concord {

/**
 * Advances every emitter in the scene by the frame delta.
 *
 * Register it with `game.Systems().Add<ParticleSystem>()`. The system only
 * mutates components, so emitters keep simulating correctly whether or not a
 * renderer is attached, and the render side reads them through the usual
 * scene snapshot.
 */
class CENGINE_API ParticleSystem : public ISystem {
public:
    void OnUpdate(Scene& scene, f32 deltaTime) override;
};

} // namespace Concord

#endif // CONCORD_PARTICLESYSTEM_H
