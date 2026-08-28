// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "EcsTestCases.h"

#include "engine/ecs/EntityRegistry.h"
#include "engine/ecs/World.h"

#include <stdexcept>
#include <type_traits>

namespace {

struct Position {
    float x = 0.0f;
};

struct Throwing {
    static inline bool fail = false;

    explicit Throwing(int)
    {
        if (fail) {
            throw std::runtime_error("component construction failed");
        }
    }
};

bool Check(bool condition)
{
    return condition;
}

bool TestEntityLifetime()
{
    Concord::World world;
    const Concord::Entity first = world.Create();
    world.Add<Position>(first, Position{.x = 1.0f});
    if (!Check(world.Get<Position>(first)->x == 1.0f)) {
        return false;
    }
    if (!Check(world.Destroy(first) && !world.Destroy(first))) {
        return false;
    }

    const Concord::Entity replacement = world.Create();
    if (!Check(replacement.index == first.index && replacement != first)) {
        return false;
    }
    bool addThrew = false;
    try {
        world.Add<Position>(first, Position{.x = 2.0f});
    } catch (const std::invalid_argument&) {
        addThrew = true;
    }
    world.Add<Position>(replacement, Position{.x = 3.0f});
    world.Remove<Position>(first);
    return Check(addThrew && world.ComponentCount<Position>() == 1 &&
                 world.Get<Position>(first) == nullptr &&
                 world.Get<Position>(replacement)->x == 3.0f);
}

bool TestForeignAndConstructionFailures()
{
    Concord::World world;
    const Concord::Entity entity = world.Create();
    world.Add<Position>(entity, Position{.x = 3.0f});

    Concord::World otherWorld;
    const Concord::Entity foreign = otherWorld.Create();
    bool foreignThrew = false;
    try {
        world.Add<Position>(foreign, Position{.x = 8.0f});
    } catch (const std::invalid_argument&) {
        foreignThrew = true;
    }
    const bool foreignSafe = !world.IsAlive(foreign) && world.Get<Position>(foreign) == nullptr &&
                             !world.Has<Position>(foreign) && !world.Destroy(foreign);

    Throwing::fail = true;
    bool constructionThrew = false;
    try {
        world.Add<Throwing>(entity, 1);
    } catch (const std::runtime_error&) {
        constructionThrew = true;
    }
    Throwing::fail = false;
    return Check(foreignThrew && foreignSafe && constructionThrew &&
                 world.Get<Position>(entity)->x == 3.0f &&
                 world.ComponentCount<Throwing>() == 0);
}

} // namespace

namespace ConcordTests {

bool RunWorldEcsTests()
{
    static_assert(!std::is_move_constructible_v<Concord::EntityRegistry>);
    static_assert(!std::is_move_assignable_v<Concord::EntityRegistry>);
    static_assert(!std::is_move_constructible_v<Concord::World>);
    static_assert(!std::is_move_assignable_v<Concord::World>);
    return TestEntityLifetime() && TestForeignAndConstructionFailures();
}

} // namespace ConcordTests
