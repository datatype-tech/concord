// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERBUOYANCYSYSTEM_H
#define CONCORD_WATERBUOYANCYSYSTEM_H

#include "Concord/CExport.h"
#include "engine/ecs/System.h"
#include "engine/ecs/WaterSurfaceQuery.h"

#include <vector>

namespace Concord {

/**
 * Floats dynamic bodies that carry `Buoyancy` when they are in the water.
 *
 * Archimedes rather than a spring: the upward push is the weight of the water
 * a body has displaced, which is proportional to how much of the body is under
 * the surface, so a partly submerged crate is pushed up hard and one resting at
 * its own waterline is not pushed at all. That is what makes the resting depth
 * fall out of the density instead of having to be authored, and what makes a
 * body thrown in hard overshoot, bob, and settle.
 *
 * Register it with `game.Systems().Add<WaterBuoyancySystem>()` after
 * `PhysicsSystem`, so it reads velocities the solver has already written back
 * and its own correction is picked up on the next step.
 */
class CENGINE_API WaterBuoyancySystem : public ISystem {
public:
    void OnUpdate(Scene& scene, f32 deltaTime) override;

private:
    /** This frame's water surfaces, rebuilt before the body query runs. */
    std::vector<WaterSurface> m_surfaces;
};

} // namespace Concord

#endif // CONCORD_WATERBUOYANCYSYSTEM_H
