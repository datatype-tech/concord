// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/cvm/CvmPhysicsRuntime.h"
#include "engine/cvm/CvmSceneAccess.h"
#include "engine/ecs/PhysicsComponents.h"
#include "engine/ecs/PhysicsSystem.h"
#include "engine/physics/PhysicsWorld.h"
#include "engine/scene/PhysicsBody.h"
#include "engine/scene/Scene.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {

using Concord::Cvm::Colour;
using Concord::Cvm::Record;
using Concord::Cvm::ResolveEntity;
using Concord::Cvm::ToVec3;

Concord::BodyMotion MotionFromAbi(std::int64_t motion) noexcept
{
    switch (motion) {
    case 1:
        return Concord::BodyMotion::Kinematic;
    case 2:
        return Concord::BodyMotion::Dynamic;
    default:
        return Concord::BodyMotion::Static;
    }
}

double AxisOf(const Concord::Vec3& value, std::int64_t axis) noexcept
{
    if (axis == 0) {
        return value.x;
    }
    if (axis == 1) {
        return value.y;
    }
    return value.z;
}

std::mutex g_simulationMutex;
std::unordered_map<Concord::Scene*, std::unique_ptr<Concord::PhysicsSystem>> g_simulations;

/**
 * The world currently bound to \p scene, creating a CVM-owned solver when
 * the Game has not registered one yet.
 *
 * Headless tests call concord_physics_step without RunScene. A running game
 * already has PhysicsSystem; Find() returns that world and we must not
 * create a second one.
 */
Concord::PhysicsWorld* EnsureWorld(Concord::Scene& scene)
{
    if (Concord::PhysicsWorld* existing = Concord::PhysicsWorld::Find(scene)) {
        return existing;
    }
    std::lock_guard<std::mutex> guard(g_simulationMutex);
    std::unique_ptr<Concord::PhysicsSystem>& slot = g_simulations[&scene];
    if (!slot) {
        slot = std::make_unique<Concord::PhysicsSystem>();
        slot->OnStart(scene);
    }
    return &slot->World();
}

} // namespace

namespace Concord::Cvm {

void ReleaseSimulation(Scene& scene)
{
    std::lock_guard<std::mutex> guard(g_simulationMutex);
    const auto found = g_simulations.find(&scene);
    if (found == g_simulations.end()) {
        return;
    }
    found->second->OnStop(scene);
    g_simulations.erase(found);
}

} // namespace Concord::Cvm

extern "C" {

std::int64_t ConcordCvmSceneSpawnStaticBody(std::int64_t scene, double positionX, double positionY,
                                            double positionZ, double sizeX, double sizeY,
                                            double sizeZ)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) {
        return 0;
    }
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::StaticBody>({
        .transform = {.position = ToVec3(positionX, positionY, positionZ)},
        .size = ToVec3(sizeX, sizeY, sizeZ),
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnDynamicBox(std::int64_t scene, double positionX, double positionY,
                                            double positionZ, double sizeX, double sizeY,
                                            double sizeZ, std::int64_t red, std::int64_t green,
                                            std::int64_t blue, double mass)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) {
        return 0;
    }
    const Concord::f32 kilograms =
        mass > 0.0 && std::isfinite(mass) ? static_cast<Concord::f32>(mass) : 1.0f;
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::DynamicBox>({
        .transform = {.position = ToVec3(positionX, positionY, positionZ)},
        .size = ToVec3(sizeX, sizeY, sizeZ),
        .material = {.albedo = Colour(red, green, blue)},
        .mass = kilograms,
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmSceneSpawnDynamicSphere(std::int64_t scene, double positionX,
                                               double positionY, double positionZ,
                                               double diameter, std::int64_t red,
                                               std::int64_t green, std::int64_t blue, double mass)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) {
        return 0;
    }
    const Concord::f32 kilograms =
        mass > 0.0 && std::isfinite(mass) ? static_cast<Concord::f32>(mass) : 1.0f;
    const Concord::f32 extent =
        diameter > 0.0 && std::isfinite(diameter) ? static_cast<Concord::f32>(diameter) : 1.0f;
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::DynamicSphere>({
        .transform = {.position = ToVec3(positionX, positionY, positionZ)},
        .diameter = extent,
        .material = {.albedo = Colour(red, green, blue)},
        .mass = kilograms,
    });
    return Record(scene, *found, handle);
}

std::int64_t ConcordCvmEntityAddBody(std::int64_t entity, std::int64_t motion, double mass)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id)) {
        return 0;
    }
    Concord::World& world = scene->GetWorld();
    if (world.Get<Concord::Transform>(id) == nullptr) {
        return 0;
    }
    Concord::Vec3 size{1.0f, 1.0f, 1.0f};
    Concord::CollisionShape shape = Concord::CollisionShape::Box;
    if (const Concord::MeshRenderer* mesh = world.Get<Concord::MeshRenderer>(id)) {
        size = mesh->size;
        if (mesh->shape == Concord::PrimitiveShape::Sphere) {
            shape = Concord::CollisionShape::Sphere;
        }
    }
    const Concord::f32 kilograms =
        mass > 0.0 && std::isfinite(mass) ? static_cast<Concord::f32>(mass) : 1.0f;
    world.Add<Concord::Collider>(id, Concord::Collider{.shape = shape, .size = size});
    world.Add<Concord::RigidBody>(id, Concord::RigidBody{.motion = MotionFromAbi(motion),
                                                         .mass = kilograms});
    return 1;
}

std::int64_t ConcordCvmEntitySetVelocity(std::int64_t entity, double x, double y, double z)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id)) {
        return 0;
    }
    if (Concord::PhysicsWorld* physics = Concord::PhysicsWorld::Find(*scene)) {
        return physics->SetLinearVelocity(id, ToVec3(x, y, z)) ? 1 : 0;
    }
    Concord::RigidBody* body = scene->GetWorld().Get<Concord::RigidBody>(id);
    if (body == nullptr) {
        return 0;
    }
    body->linearVelocity = ToVec3(x, y, z);
    body->applyVelocity = true;
    return 1;
}

double ConcordCvmEntityVelocity(std::int64_t entity, std::int64_t axis)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id) || axis < 0 || axis > 2) {
        return 0.0;
    }
    Concord::Vec3 velocity{};
    if (Concord::PhysicsWorld* physics = Concord::PhysicsWorld::Find(*scene)) {
        velocity = physics->LinearVelocity(id);
    } else if (const Concord::RigidBody* body = scene->GetWorld().Get<Concord::RigidBody>(id)) {
        velocity = body->linearVelocity;
    } else {
        return 0.0;
    }
    return AxisOf(velocity, axis);
}

std::int64_t ConcordCvmEntityApplyImpulse(std::int64_t entity, double x, double y, double z)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id)) {
        return 0;
    }
    Concord::PhysicsWorld* physics = Concord::PhysicsWorld::Find(*scene);
    if (physics == nullptr) {
        return 0;
    }
    return physics->ApplyImpulse(id, ToVec3(x, y, z)) ? 1 : 0;
}

std::int64_t ConcordCvmEntityGrounded(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id)) {
        return 0;
    }
    const Concord::CharacterMotor* motor = scene->GetWorld().Get<Concord::CharacterMotor>(id);
    return motor != nullptr && motor->grounded ? 1 : 0;
}

std::int64_t ConcordCvmOverlapSphere(std::int64_t scene, double x, double y, double z,
                                     double radius)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) {
        return 0;
    }
    Concord::PhysicsWorld* physics = Concord::PhysicsWorld::Find(*found);
    if (physics == nullptr) {
        return 0;
    }
    const std::vector<Concord::Entity> hits =
        physics->OverlapSphere(ToVec3(x, y, z), static_cast<Concord::f32>(radius));
    if (hits.empty()) {
        return 0;
    }
    return Concord::Cvm::FindOrCreateEntity(scene, *found, hits.front());
}

std::int64_t ConcordCvmPhysicsStep(std::int64_t scene, double deltaSeconds)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr || !(deltaSeconds > 0.0) || !std::isfinite(deltaSeconds)) {
        return 0;
    }
    Concord::PhysicsWorld* world = Concord::PhysicsWorld::Find(*found);
    {
        std::lock_guard<std::mutex> guard(g_simulationMutex);
        const bool ownedHere = g_simulations.contains(found);
        if (world != nullptr && !ownedHere) {
            // The Game's PhysicsSystem already integrates this scene each frame.
            return 1;
        }
    }
    world = EnsureWorld(*found);
    world->Step(*found, static_cast<Concord::f32>(deltaSeconds));
    return 1;
}

std::int64_t ConcordCvmSetGravity(std::int64_t scene, double x, double y, double z)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) {
        return 0;
    }
    EnsureWorld(*found)->SetGravity(ToVec3(x, y, z));
    return 1;
}

double ConcordCvmGravity(std::int64_t scene, std::int64_t axis)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr || axis < 0 || axis > 2) {
        return 0.0;
    }
    Concord::PhysicsWorld* world = Concord::PhysicsWorld::Find(*found);
    if (world == nullptr) {
        return axis == 1 ? -9.81 : 0.0;
    }
    return AxisOf(world->Gravity(), axis);
}

std::int64_t ConcordCvmEntityAddMotor(std::int64_t entity, double radius, double height)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id)) {
        return 0;
    }
    if (scene->GetWorld().Get<Concord::Transform>(id) == nullptr) {
        return 0;
    }
    const Concord::f32 capsuleRadius =
        radius > 0.0 && std::isfinite(radius) ? static_cast<Concord::f32>(radius) : 0.35f;
    const Concord::f32 capsuleHeight =
        height > 0.0 && std::isfinite(height) ? static_cast<Concord::f32>(height) : 1.80f;
    scene->GetWorld().Add<Concord::CharacterMotor>(
        id, Concord::CharacterMotor{.radius = capsuleRadius, .height = capsuleHeight});
    return 1;
}

std::int64_t ConcordCvmEntitySetWishVelocity(std::int64_t entity, double x, double y, double z)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id)) {
        return 0;
    }
    Concord::CharacterMotor* motor = scene->GetWorld().Get<Concord::CharacterMotor>(id);
    if (motor == nullptr) {
        return 0;
    }
    motor->wishVelocity = ToVec3(x, y, z);
    motor->enabled = true;
    return 1;
}

double ConcordCvmEntityWishVelocity(std::int64_t entity, std::int64_t axis)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id) || axis < 0 || axis > 2) {
        return 0.0;
    }
    const Concord::CharacterMotor* motor = scene->GetWorld().Get<Concord::CharacterMotor>(id);
    if (motor == nullptr) {
        return 0.0;
    }
    return AxisOf(motor->wishVelocity, axis);
}

std::int64_t ConcordCvmEntitySetAngularVelocity(std::int64_t entity, double x, double y, double z)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id)) {
        return 0;
    }
    if (Concord::PhysicsWorld* physics = Concord::PhysicsWorld::Find(*scene)) {
        return physics->SetAngularVelocity(id, ToVec3(x, y, z)) ? 1 : 0;
    }
    Concord::RigidBody* body = scene->GetWorld().Get<Concord::RigidBody>(id);
    if (body == nullptr) {
        return 0;
    }
    body->angularVelocity = ToVec3(x, y, z);
    body->applyVelocity = true;
    return 1;
}

double ConcordCvmEntityAngularVelocity(std::int64_t entity, std::int64_t axis)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!ResolveEntity(entity, scene, id) || axis < 0 || axis > 2) {
        return 0.0;
    }
    Concord::Vec3 velocity{};
    if (Concord::PhysicsWorld* physics = Concord::PhysicsWorld::Find(*scene)) {
        velocity = physics->AngularVelocity(id);
    } else if (const Concord::RigidBody* body = scene->GetWorld().Get<Concord::RigidBody>(id)) {
        velocity = body->angularVelocity;
    } else {
        return 0.0;
    }
    return AxisOf(velocity, axis);
}

} // extern "C"
