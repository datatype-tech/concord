// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "EcsTestCases.h"

#include "engine/ecs/World.h"

#include <stdexcept>

namespace {

struct Position {
    float x = 0.0f;
};

struct Marker {
    int value = 0;
};

bool Check(bool condition)
{
    return condition;
}

bool TestMutationGuard()
{
    Concord::World world;
    const Concord::Entity entity = world.Create();
    world.Add<Position>(entity, Position{.x = 4.0f});
    bool createRejected = false;
    bool destroyRejected = false;
    bool addRejected = false;
    bool removeRejected = false;
    world.Query<Position>([&](Concord::Entity current, Position&) {
        try {
            world.Create();
        } catch (const std::logic_error&) {
            createRejected = true;
        }
        try {
            world.Destroy(current);
        } catch (const std::logic_error&) {
            destroyRejected = true;
        }
        try {
            world.Add<Marker>(current, Marker{});
        } catch (const std::logic_error&) {
            addRejected = true;
        }
        try {
            world.Remove<Position>(current);
        } catch (const std::logic_error&) {
            removeRejected = true;
        }
    });
    return Check(createRejected && destroyRejected && addRejected && removeRejected &&
                 world.IsAlive(entity) && world.Has<Position>(entity));
}

bool TestCallbackExceptionRestoresGuard()
{
    Concord::World world;
    const Concord::Entity entity = world.Create();
    world.Add<Position>(entity, Position{.x = 5.0f});
    bool callbackThrew = false;
    try {
        world.Query<Position>([&](Concord::Entity current, Position&) {
            world.DeferRemove<Position>(current);
            throw std::runtime_error("query callback failed");
        });
    } catch (const std::runtime_error&) {
        callbackThrew = true;
    }
    const Concord::Entity after = world.Create();
    const bool queueSurvived = world.DeferredCount() == 1 && world.Has<Position>(entity);
    const Concord::usize applied = world.FlushDeferred();
    return Check(callbackThrew && queueSurvived && applied == 1 && world.IsAlive(after) &&
                 !world.Has<Position>(entity));
}

bool TestConstQuery()
{
    Concord::World world;
    const Concord::Entity entity = world.Create();
    world.Add<Position>(entity, Position{.x = 6.0f});
    const Concord::World& readOnly = world;
    Concord::usize matches = 0;
    readOnly.Query<Position>([&](Concord::Entity, const Position& position) {
        if (position.x == 6.0f) {
            ++matches;
        }
    });
    return Check(matches == 1);
}

} // namespace

namespace ConcordTests {

bool RunQueryEcsTests()
{
    return TestMutationGuard() && TestCallbackExceptionRestoresGuard() && TestConstQuery();
}

} // namespace ConcordTests
