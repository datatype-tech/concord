// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/PhysicsComponents.h"
#include "engine/ecs/PhysicsSystem.h"
#include "engine/scene/PhysicsBody.h"
#include "engine/scene/Scene.h"

#include <cmath>
#include <vector>

namespace {

bool Near(float left, float right, float tolerance = 0.08f)
{
    return std::fabs(left - right) < tolerance;
}

void Step(Concord::PhysicsSystem& physics, Concord::Scene& scene, int frames)
{
    for (int index = 0; index < frames; ++index) {
        physics.OnUpdate(scene, 1.0f / 60.0f);
    }
}

bool TestDynamicBoxRestsOnFloor()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::StaticBody>(
        {.transform = {.position = {0.0f, -0.5f, 0.0f}}, .size = {8.0f, 1.0f, 8.0f}});
    const Concord::EntityHandle box = scene.Spawn<Concord::Object::DynamicBox>(
        {.transform = {.position = {0.0f, 4.0f, 0.0f}},
         .size = {0.5f, 0.5f, 0.5f},
         .mass = 1.0f,
         .restitution = 0.0f});
    Concord::PhysicsSystem physics;
    physics.OnStart(scene);
    Step(physics, scene, 180);
    const Concord::Transform* transform = box.Get<Concord::Transform>();
    const Concord::RigidBody* body = box.Get<Concord::RigidBody>();
    physics.OnStop(scene);
    return transform != nullptr && body != nullptr && transform->position.y > 0.1f &&
           transform->position.y < 0.55f && std::fabs(body->linearVelocity.y) < 0.35f;
}

bool TestStaticBodyDoesNotFall()
{
    Concord::Scene scene;
    const Concord::EntityHandle volume = scene.Spawn<Concord::Object::StaticBody>(
        {.transform = {.position = {0.0f, 3.0f, 0.0f}}, .size = {1.0f, 1.0f, 1.0f}});
    Concord::PhysicsSystem physics;
    physics.OnStart(scene);
    Step(physics, scene, 60);
    const Concord::Transform* transform = volume.Get<Concord::Transform>();
    physics.OnStop(scene);
    return transform != nullptr && Near(transform->position.y, 3.0f, 0.001f);
}

bool TestRaycastHitsFloor()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::StaticBody>(
        {.transform = {.position = {0.0f, 0.0f, 0.0f}}, .size = {4.0f, 0.4f, 4.0f}});
    Concord::PhysicsSystem physics;
    physics.OnStart(scene);
    physics.OnUpdate(scene, 1.0f / 60.0f);
    Concord::PhysicsRayHit hit{};
    const bool found =
        physics.Raycast({0.0f, 3.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, 10.0f, hit);
    physics.OnStop(scene);
    return found && hit.entity.IsValid() && hit.distance > 2.6f && hit.distance < 3.1f &&
           hit.normal.y > 0.7f;
}

bool TestFindReturnsBoundWorld()
{
    Concord::Scene scene;
    Concord::PhysicsSystem physics;
    physics.OnStart(scene);
    Concord::PhysicsWorld* found = Concord::PhysicsWorld::Find(scene);
    physics.OnStop(scene);
    return found == &physics.World() && Concord::PhysicsWorld::Find(scene) == nullptr;
}

bool TestImpulseMovesBox()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::StaticBody>(
        {.transform = {.position = {0.0f, -0.5f, 0.0f}}, .size = {12.0f, 1.0f, 12.0f}});
    const Concord::EntityHandle box = scene.Spawn<Concord::Object::DynamicBox>(
        {.transform = {.position = {0.0f, 0.4f, 0.0f}},
         .size = {0.5f, 0.5f, 0.5f},
         .mass = 1.0f,
         .restitution = 0.0f,
         .friction = 0.05f});
    Concord::PhysicsSystem physics;
    physics.OnStart(scene);
    Step(physics, scene, 90);
    const bool pushed = physics.ApplyImpulse(box.Id(), {4.0f, 0.0f, 0.0f});
    Step(physics, scene, 45);
    const Concord::Transform* transform = box.Get<Concord::Transform>();
    physics.OnStop(scene);
    return pushed && transform != nullptr && transform->position.x > 0.25f;
}

bool TestOverlapSphereFindsFloor()
{
    Concord::Scene scene;
    const Concord::EntityHandle floor = scene.Spawn<Concord::Object::StaticBody>(
        {.transform = {.position = {0.0f, 0.0f, 0.0f}}, .size = {4.0f, 0.4f, 4.0f}});
    Concord::PhysicsSystem physics;
    physics.OnStart(scene);
    physics.OnUpdate(scene, 1.0f / 60.0f);
    const std::vector<Concord::Entity> hits = physics.OverlapSphere({0.0f, 0.0f, 0.0f}, 2.0f);
    physics.OnStop(scene);
    for (Concord::Entity entity : hits) {
        if (entity == floor.Id()) {
            return true;
        }
    }
    return false;
}

bool TestSetVelocityLiftsBox()
{
    Concord::Scene scene;
    const Concord::EntityHandle box = scene.Spawn<Concord::Object::DynamicBox>(
        {.transform = {.position = {0.0f, 2.0f, 0.0f}},
         .size = {0.4f, 0.4f, 0.4f},
         .mass = 1.0f});
    Concord::PhysicsSystem physics;
    physics.OnStart(scene);
    physics.OnUpdate(scene, 1.0f / 60.0f);
    const bool set = physics.SetLinearVelocity(box.Id(), {0.0f, 6.0f, 0.0f});
    physics.OnUpdate(scene, 1.0f / 60.0f);
    const Concord::Transform* transform = box.Get<Concord::Transform>();
    physics.OnStop(scene);
    return set && transform != nullptr && transform->position.y > 2.05f;
}

} // namespace

int main()
{
    return TestDynamicBoxRestsOnFloor() && TestStaticBodyDoesNotFall() && TestRaycastHitsFloor() &&
                   TestFindReturnsBoundWorld() && TestImpulseMovesBox() &&
                   TestOverlapSphereFindsFloor() && TestSetVelocityLiftsBox()
               ? 0
               : 1;
}
