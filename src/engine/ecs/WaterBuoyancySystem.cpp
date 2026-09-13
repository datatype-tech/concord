// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/WaterBuoyancySystem.h"

#include "engine/core/Transform.h"
#include "engine/ecs/BuoyancyComponent.h"
#include "engine/ecs/PhysicsComponents.h"
#include "engine/ecs/World.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/**
 * Downward acceleration assumed when no physics world is bound.
 *
 * Only reached by a scene that attached Buoyancy without a PhysicsSystem, in
 * which case nothing is integrating anyway and the number never shows.
 */
constexpr f32 kFallbackGravity = 9.81f;

/**
 * Ceiling on the buoyant acceleration, as a multiple of gravity.
 *
 * A body far lighter than water is held under by a force proportional to how
 * far under it is, and nothing in the integrator bounds how far that can be:
 * a beach ball pushed to the bottom of a deep pool is a spring wound to a
 * stiffness the fixed timestep cannot resolve, and it leaves the water at a
 * speed the solver invented. Real water cannot accelerate anything much past
 * this before drag takes over.
 */
constexpr f32 kMaxBuoyantGravities = 6.0f;

/** Half the body's vertical extent, from its collider and transform scale. */
f32 VerticalHalfExtent(World& world, Entity entity, const Transform& transform) noexcept
{
    const Collider* collider = world.Get<Collider>(entity);
    const f32 authored = collider != nullptr ? collider->size.y : 1.0f;
    const f32 scaled = authored * std::abs(transform.scale.y);
    // Floored rather than allowed to reach zero: the submerged fraction below
    // divides by it, and a body with no height is fully in or fully out with
    // nothing in between, which is a step function the drag cannot damp.
    return std::max(scaled * 0.5f, 0.02f);
}

} // namespace

void WaterBuoyancySystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    if (!(deltaTime > 0.0f)) {
        return;
    }
    World& world = scene.GetWorld();
    CollectWaterSurfaces(world, m_surfaces);
    if (m_surfaces.empty()) {
        return;
    }

    const PhysicsWorld* physics = PhysicsWorld::Find(scene);
    const f32 gravity = physics != nullptr ? std::abs(physics->Gravity().y) : kFallbackGravity;
    if (!(gravity > 0.0f)) {
        return;
    }

    world.Query<RigidBody, Buoyancy, Transform>([this, &world, gravity, deltaTime](
                                                    Entity entity, RigidBody& rigidBody,
                                                    const Buoyancy& buoyancy,
                                                    const Transform& transform) {
        if (rigidBody.motion != BodyMotion::Dynamic) {
            return;
        }
        const WaterSurface* surface =
            FindWaterSurface(m_surfaces, transform.position.x, transform.position.z);
        if (surface == nullptr) {
            return;
        }
        const f32 half = VerticalHalfExtent(world, entity, transform);
        const f32 submerged =
            std::clamp((surface->worldY - (transform.position.y - half)) / (2.0f * half), 0.0f,
                       1.0f);
        if (submerged <= 0.0f) {
            return;
        }
        const f32 density = std::max(buoyancy.density, 0.05f);
        // Archimedes, written as an acceleration the solver has not already
        // applied. The solver integrates a full gravity every step, so what is
        // added here is only the displaced water's share of it; the two cancel
        // exactly when the submerged fraction equals the density, which is the
        // waterline the body settles at.
        const f32 buoyantGravities = std::min(submerged / density, kMaxBuoyantGravities);
        rigidBody.linearVelocity.y += gravity * buoyantGravities * deltaTime;

        // Applied as a decay rather than as a subtracted force, so it can never
        // reverse a velocity no matter how long the step: a linear drag large
        // enough to stop a fast entry in one frame overshoots into a bounce,
        // which is the classic way damping turns into the opposite of damping.
        const f32 retained = std::exp(-buoyancy.drag * submerged * deltaTime);
        rigidBody.linearVelocity.x *= retained;
        rigidBody.linearVelocity.y *= retained;
        rigidBody.linearVelocity.z *= retained;
        const f32 spinRetained = std::exp(-buoyancy.angularDrag * submerged * deltaTime);
        rigidBody.angularVelocity.x *= spinRetained;
        rigidBody.angularVelocity.y *= spinRetained;
        rigidBody.angularVelocity.z *= spinRetained;
        rigidBody.applyVelocity = true;
    });
}

} // namespace Concord
