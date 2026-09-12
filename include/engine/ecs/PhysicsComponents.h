// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PHYSICSCOMPONENTS_H
#define CONCORD_PHYSICSCOMPONENTS_H

#include "engine/core/Vec3.h"
#include "engine/physics/CollisionShape.h"

namespace Concord {

/**
 * Volume Jolt tests for this entity.
 *
 * Size is the same number MeshRenderer uses: full extent along each axis,
 * before Transform::scale. A box of `{1, 2, 1}` with scale 2 is two metres
 * across. Sphere reads radius from `0.5 * size.x`. Capsule reads radius
 * from `0.5 * size.x` and cylinder height from `size.y`.
 */
struct Collider {
    CollisionShape shape = CollisionShape::Box;
    Vec3 size{1.0f, 1.0f, 1.0f};
    /** Local offset of the shape from the transform origin. */
    Vec3 offset{};
    /** True when contacts fire without generating a response. */
    bool isTrigger = false;
};

/**
 * Mass, motion type and the velocities the solver last wrote.
 *
 * Velocities are owned by Jolt for dynamic bodies and copied back each
 * step so a query on the component sees the same motion the body has.
 * Gameplay that wants to throw something writes them before the next
 * PhysicsSystem tick; the system pushes that into Jolt once.
 */
struct RigidBody {
    BodyMotion motion = BodyMotion::Dynamic;
    f32 mass = 1.0f;
    f32 gravityScale = 1.0f;
    f32 linearDamping = 0.05f;
    f32 angularDamping = 0.05f;
    f32 friction = 0.50f;
    f32 restitution = 0.05f;
    Vec3 linearVelocity{};
    Vec3 angularVelocity{};
    /** When true the solver holds the rotation the body was spawned with. */
    bool lockRotation = false;
    /**
     * Set when gameplay changed linearVelocity or angularVelocity this frame.
     *
     * PhysicsSystem clears it after pushing the values into Jolt, so a
     * body that is only being read does not fight the solver.
     */
    bool applyVelocity = false;
};

/**
 * A walking capsule driven by wish velocity, not by the rigid-body solver.
 *
 * FirstPersonController writes `wishVelocity` when fly mode is off.
 * PhysicsSystem steps a Jolt CharacterVirtual and writes the resulting
 * pose back onto Transform, plus whether the capsule has ground under it.
 */
struct CharacterMotor {
    f32 radius = 0.35f;
    f32 height = 1.80f;
    f32 maxSlopeDegrees = 50.0f;
    Vec3 wishVelocity{};
    bool grounded = false;
    bool enabled = true;
};

} // namespace Concord

#endif // CONCORD_PHYSICSCOMPONENTS_H
