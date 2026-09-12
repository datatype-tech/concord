// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PHYSICSWORLD_H
#define CONCORD_PHYSICSWORLD_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/ecs/Entity.h"
#include "engine/physics/CollisionShape.h"
#include "engine/physics/PhysicsQuery.h"

#include <memory>
#include <vector>

namespace Concord {

class Scene;

/**
 * One Jolt simulation, hidden behind pimpl so public headers never name it.
 *
 * A Scene does not own this: PhysicsSystem does, and binds the pair for the
 * duration of the run so a raycast from CVM can find the same world the
 * crates are falling in.
 */
class CENGINE_API PhysicsWorld {
public:
    explicit PhysicsWorld(const PhysicsSettings& settings = {});
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    /** Replaces gravity. Dynamic bodies feel it on the next step. */
    void SetGravity(Vec3 gravity);

    /** The gravity the world is currently integrating. */
    [[nodiscard]] Vec3 Gravity() const;

    /**
     * Creates, updates and destroys Jolt bodies to match the scene, then
     * advances the solver and writes dynamic poses back onto Transform.
     */
    void Step(Scene& scene, f32 deltaSeconds);

    /**
     * Casts a ray against every body in this world.
     *
     * @return True when something was hit; `hit` is left untouched on a miss.
     */
    [[nodiscard]] bool Raycast(Vec3 origin, Vec3 direction, f32 maxDistance,
                               PhysicsRayHit& hit) const;

    /**
     * Instant change of linear velocity on a dynamic or kinematic body.
     *
     * Writes the component and, when the Jolt body already exists, the solver
     * as well, so a throw issued mid-frame does not wait for the next step.
     */
    bool SetLinearVelocity(Entity entity, Vec3 velocity);

    /** The velocity Jolt last solved, or the authored value before the first step. */
    [[nodiscard]] Vec3 LinearVelocity(Entity entity) const;

    /** Instant change of angular velocity, same timing as SetLinearVelocity. */
    bool SetAngularVelocity(Entity entity, Vec3 velocity);

    /** Angular velocity Jolt last solved, or the authored value before the first step. */
    [[nodiscard]] Vec3 AngularVelocity(Entity entity) const;

    /**
     * Adds an impulse at the centre of mass.
     *
     * @return False when the entity has no body yet (call Step once first).
     */
    bool ApplyImpulse(Entity entity, Vec3 impulse);

    /** Every body whose broad-phase volume meets a sphere. */
    [[nodiscard]] std::vector<Entity> OverlapSphere(Vec3 centre, f32 radius) const;

    /** Remembers which scene this world is simulating, for Find(). */
    void BindScene(Scene& scene);

    /** Clears the Find() registration. */
    void UnbindScene();

    /** The world bound to `scene`, or nullptr when physics is not running. */
    [[nodiscard]] static PhysicsWorld* Find(const Scene& scene);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Concord

#endif // CONCORD_PHYSICSWORLD_H
