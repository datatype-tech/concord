// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PHYSICSSYSTEM_H
#define CONCORD_PHYSICSSYSTEM_H

#include "Concord/CExport.h"
#include "engine/ecs/System.h"
#include "engine/ecs/Entity.h"
#include "engine/physics/PhysicsQuery.h"
#include "engine/physics/PhysicsWorld.h"

#include <memory>
#include <vector>

namespace Concord {

/**
 * Steps the Jolt world and keeps Transform in lockstep with it.
 *
 * Register with `game.Systems().Add<PhysicsSystem>()`. Dynamic bodies write
 * their solved pose back onto Transform each tick; static and kinematic
 * bodies read Transform and push it into Jolt, which is how a moving
 * platform stays a platform rather than a crate.
 */
class CENGINE_API PhysicsSystem : public ISystem {
public:
    explicit PhysicsSystem(const PhysicsSettings& settings = {});
    ~PhysicsSystem() override;

    void OnStart(Scene& scene) override;
    void OnUpdate(Scene& scene, f32 deltaTime) override;
    void OnStop(Scene& scene) override;

    /** The simulation this system owns. */
    [[nodiscard]] PhysicsWorld& World() noexcept { return *m_world; }
    [[nodiscard]] const PhysicsWorld& World() const noexcept { return *m_world; }

    /** Forwards to the bound world; returns false when physics is not running. */
    [[nodiscard]] bool Raycast(Vec3 origin, Vec3 direction, f32 maxDistance,
                               PhysicsRayHit& hit) const;

    bool SetLinearVelocity(Entity entity, Vec3 velocity);
    [[nodiscard]] Vec3 LinearVelocity(Entity entity) const;
    bool ApplyImpulse(Entity entity, Vec3 impulse);
    [[nodiscard]] std::vector<Entity> OverlapSphere(Vec3 centre, f32 radius) const;

private:
    PhysicsSettings m_settings{};
    std::unique_ptr<PhysicsWorld> m_world;
};

} // namespace Concord

#endif // CONCORD_PHYSICSSYSTEM_H
