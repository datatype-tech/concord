// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "EcsTestCases.h"

#include "engine/scene/Scene.h"

#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

struct ScenePosition {
    float x = 0.0f;
};

struct FailingDesc {
    float x = 0.0f;
};

struct FailingArchetype {
    using Desc = FailingDesc;

    static void Build(Concord::World& world, Concord::Entity entity, const Desc& desc)
    {
        world.Add<ScenePosition>(entity, ScenePosition{.x = desc.x});
        throw std::runtime_error("archetype build failed");
    }
};

bool Check(bool condition)
{
    return condition;
}

bool TestSpawnRollback()
{
    Concord::Scene scene;
    bool threw = false;
    try {
        scene.Spawn<FailingArchetype>({.x = 9.0f});
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "archetype build failed";
    }
    if (!Check(threw && scene.EntityCount() == 0 &&
               scene.GetWorld().ComponentCount<ScenePosition>() == 0)) {
        return false;
    }

    const Concord::EntityHandle replacement =
        scene.CreateEntity().Add<ScenePosition>({.x = 2.0f});
    return Check(replacement.IsAlive() && replacement.Get<ScenePosition>() != nullptr &&
                 replacement.Get<ScenePosition>()->x == 2.0f && scene.EntityCount() == 1);
}

bool TestHandlesAndCamera()
{
    Concord::Scene scene;
    Concord::EntityHandle stale = scene.CreateEntity();
    stale.Destroy();
    stale.Add<ScenePosition>(ScenePosition{.x = 4.0f});
    Concord::EntityHandle invalid;
    invalid.Add<ScenePosition>(ScenePosition{.x = 5.0f});
    if (!Check(scene.EntityCount() == 0)) {
        return false;
    }

    scene.CreateEntity().Add<ScenePosition>({.x = 6.0f});
    const Concord::EntityHandle constHandle =
        scene.CreateEntity().Add<ScenePosition>(ScenePosition{.x = 7.0f});
    static_assert(std::is_same_v<decltype(constHandle.Get<ScenePosition>()), const ScenePosition*>);
    if (!Check(constHandle.Get<ScenePosition>() != nullptr && scene.EntityCount() == 2)) {
        return false;
    }

    scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{})
        .Add<Concord::CameraComponent>(Concord::CameraComponent{.priority = 4});
    const Concord::Entity mainCamera =
        scene.CreateEntity()
            .Add<Concord::Transform>(Concord::Transform{})
            .Add<Concord::CameraComponent>(Concord::CameraComponent{.priority = -2})
            .Id();
    scene.CreateEntity().Add<Concord::CameraComponent>(
        Concord::CameraComponent{.priority = -10});
    return Check(scene.MainCamera() == mainCamera);
}

} // namespace

namespace ConcordTests {

bool RunSceneEcsTests()
{
    static_assert(!std::is_move_constructible_v<Concord::Scene>);
    static_assert(!std::is_move_assignable_v<Concord::Scene>);
    return TestSpawnRollback() && TestHandlesAndCamera();
}

} // namespace ConcordTests
