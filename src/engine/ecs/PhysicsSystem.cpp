// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/PhysicsSystem.h"

#include "engine/scene/Scene.h"

namespace Concord {

PhysicsSystem::PhysicsSystem(const PhysicsSettings& settings) : m_settings(settings)
{
    m_world = std::make_unique<PhysicsWorld>(m_settings);
}

PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::OnStart(Scene& scene)
{
    m_world->BindScene(scene);
}

void PhysicsSystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    m_world->Step(scene, deltaTime);
}

void PhysicsSystem::OnStop(Scene&)
{
    m_world->UnbindScene();
}

bool PhysicsSystem::Raycast(Vec3 origin, Vec3 direction, f32 maxDistance, PhysicsRayHit& hit) const
{
    return m_world->Raycast(origin, direction, maxDistance, hit);
}

bool PhysicsSystem::SetLinearVelocity(Entity entity, Vec3 velocity)
{
    return m_world->SetLinearVelocity(entity, velocity);
}

Vec3 PhysicsSystem::LinearVelocity(Entity entity) const
{
    return m_world->LinearVelocity(entity);
}

bool PhysicsSystem::ApplyImpulse(Entity entity, Vec3 impulse)
{
    return m_world->ApplyImpulse(entity, impulse);
}

std::vector<Entity> PhysicsSystem::OverlapSphere(Vec3 centre, f32 radius) const
{
    return m_world->OverlapSphere(centre, radius);
}

} // namespace Concord
