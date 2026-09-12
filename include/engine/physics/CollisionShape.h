// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_COLLISIONSHAPE_H
#define CONCORD_COLLISIONSHAPE_H

#include "engine/core/Types.h"
#include "engine/core/Vec3.h"

namespace Concord {

/**
 * Collision primitives the physics world can build a body from.
 *
 * MeshRenderer stays a drawing hint. A collider is the volume Jolt actually
 * tests, so a decorative mesh and the body that stops a crate can disagree
 * on purpose.
 */
enum class CollisionShape : u32 {
    Box,
    Sphere,
    Capsule,
};

/**
 * How a body is allowed to move.
 *
 * Static never integrates. Kinematic is pushed by gameplay. Dynamic is
 * what the solver owns — gravity, contacts, and the velocities a crate
 * picks up when it lands.
 */
enum class BodyMotion : u32 {
    Static,
    Kinematic,
    Dynamic,
};

/**
 * Tunables for one physics world.
 *
 * Authored in world units and seconds so a scene that already thinks in
 * metres does not grow a second scale just to fall.
 */
struct PhysicsSettings {
    /** Acceleration applied to every dynamic body, world units per second². */
    Vec3 gravity{0.0f, -9.81f, 0.0f};

    /**
     * Fixed step the solver runs at.
     *
     * A frame longer than this is split so a hitch cannot tunnel a crate
     * through a floor. Zero disables the split and steps the raw delta.
     */
    f32 fixedDeltaSeconds = 1.0f / 60.0f;

    /** Collision iterations Jolt spends per fixed step. */
    u32 collisionSteps = 1;
};

} // namespace Concord

#endif // CONCORD_COLLISIONSHAPE_H
