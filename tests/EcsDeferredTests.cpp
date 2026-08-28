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

bool TestDeferredMutationOrder()
{
    Concord::World world;
    const Concord::Entity first = world.Create();
    const Concord::Entity second = world.Create();
    world.Add<Position>(first, Position{.x = 1.0f});
    world.Add<Marker>(first, Marker{.value = 3});
    world.Add<Position>(second, Position{.x = 2.0f});

    Concord::usize visited = 0;
    bool flushRejected = false;
    world.Query<Position>([&](Concord::Entity current, Position&) {
        ++visited;
        if (current != first) {
            return;
        }
        world.DeferRemove<Marker>(current);
        world.DeferAdd<Marker>(current, Marker{.value = 9});
        world.DeferDestroy(second);
        try {
            world.FlushDeferred();
        } catch (const std::logic_error&) {
            flushRejected = true;
        }
    });

    const bool stable = visited == 2 && flushRejected && world.DeferredCount() == 3 &&
                        world.Has<Marker>(first) && world.IsAlive(second);
    const Concord::usize applied = world.FlushDeferred();
    const Marker* marker = world.Get<Marker>(first);
    bool outsideRejected = false;
    try {
        world.DeferDestroy(first);
    } catch (const std::logic_error&) {
        outsideRejected = true;
    }
    return Check(stable && applied == 3 && marker && marker->value == 9 &&
                 !world.IsAlive(second) && world.DeferredCount() == 0 && outsideRejected);
}

bool TestDeferredFailureIsAtMostOnce()
{
    Concord::World world;
    const Concord::Entity entity = world.Create();
    world.Add<Position>(entity, Position{.x = 4.0f});
    int attempts = 0;
    world.Query<Position>([&](Concord::Entity, Position&) {
        world.Defer([&] {
            ++attempts;
            throw std::runtime_error("deferred command failed");
        });
        world.DeferRemove<Position>(entity);
    });

    bool threw = false;
    try {
        world.FlushDeferred();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    if (!Check(threw && attempts == 1 && world.DeferredCount() == 1 &&
               world.Has<Position>(entity))) {
        return false;
    }
    return Check(world.FlushDeferred() == 1 && !world.Has<Position>(entity) &&
                 world.DeferredCount() == 0);
}

} // namespace

namespace ConcordTests {

bool RunDeferredEcsTests()
{
    return TestDeferredMutationOrder() && TestDeferredFailureIsAtMostOnce();
}

} // namespace ConcordTests
