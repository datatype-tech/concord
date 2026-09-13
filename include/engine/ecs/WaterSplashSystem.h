// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERSPLASHSYSTEM_H
#define CONCORD_WATERSPLASHSYSTEM_H

#include "Concord/CExport.h"
#include "engine/core/Vec3.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/System.h"

#include <unordered_map>
#include <utility>
#include <vector>

namespace Concord {

/**
 * Turns a dynamic body's entry into a `WaterBodyComponent` surface into a splash.
 *
 * Water carries no Jolt collider (see `WaterBodyComponent`), so this is the
 * only place "something fell into the water" is ever noticed: each frame it
 * snapshots every water surface, then walks every dynamic `RigidBody` looking
 * for one whose height just crossed from above a surface's Y to at or below
 * it, inside that surface's footprint, while still falling. A crossing spawns
 * a `WaterRipple` ring at the impact point plus a one-shot droplet burst, both
 * scaled by how fast the body was falling, and retires the burst emitter
 * itself once its particles have run their course -- the same way
 * `WaterRippleSystem` owns a ring's whole life rather than splitting birth
 * from retirement across two systems.
 *
 * Register it with `game.Systems().Add<WaterSplashSystem>()`, after
 * `PhysicsSystem` so this frame's `linearVelocity` has already been copied
 * back from Jolt.
 */
class CENGINE_API WaterSplashSystem : public ISystem {
public:
    void OnUpdate(Scene& scene, f32 deltaTime) override;

private:
    /** One water surface's world-space footprint, snapshotted for this frame. */
    struct Surface {
        f32 worldY = 0.0f;
        f32 centreX = 0.0f;
        f32 centreZ = 0.0f;
        f32 halfExtentX = 0.0f;
        f32 halfExtentZ = 0.0f;
    };

    /** A crossing found this frame, acted on once the body query has finished. */
    struct Impact {
        Vec3 position{};
        f32 fallSpeed = 0.0f;
    };

    /** This frame's water surfaces, rebuilt before the body query runs. */
    std::vector<Surface> m_surfaces;

    /** Crossings found this frame; cleared and refilled every update. */
    std::vector<Impact> m_impacts;

    /**
     * Whether each dynamic body was at or below some surface last frame.
     *
     * Keyed by the entity's packed slot and generation rather than by Entity
     * itself, so the map compares correctly across a query without pulling in
     * Entity's equality operator; a destroyed body's stale entry simply reads
     * as "was not submerged" if its slot is ever reused.
     */
    std::unordered_map<u64, bool> m_submerged;

    /** Splash burst emitters this system owns, aged and destroyed once spent. */
    std::vector<std::pair<Entity, f32>> m_bursts;
};

} // namespace Concord

#endif // CONCORD_WATERSPLASHSYSTEM_H
