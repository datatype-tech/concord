// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderWaterBodySnapshot.h"

#include "engine/core/Transform.h"
#include "engine/ecs/WaterBodyComponent.h"
#include "engine/ecs/World.h"

#include <cmath>
#include <vector>

namespace {

using Concord::f32;

bool Near(f32 left, f32 right)
{
    return std::abs(left - right) < 0.0001f;
}

Concord::Entity SpawnBody(Concord::World& world, Concord::Transform transform,
                          Concord::Vec2 extent)
{
    const Concord::Entity entity = world.Create();
    world.Add<Concord::Transform>(entity, transform);
    world.Add<Concord::WaterBodyComponent>(entity, Concord::WaterBodyComponent{.extent = extent});
    return entity;
}

/** A footprint reports its circle as the longer of its two half extents. */
bool TestRadiusCoversTheLongerAxis()
{
    Concord::World world{};
    SpawnBody(world, Concord::Transform{.position = {1.0f, 2.0f, 3.0f}}, {5.0f, 9.0f});
    std::vector<Concord::RenderWaterBodySnapshot> bodies;
    Concord::AppendWaterBodySnapshots(bodies, world);
    if (bodies.size() != 1) {
        return false;
    }
    return Near(bodies[0].centre.x, 1.0f) && Near(bodies[0].centre.y, 3.0f) &&
           Near(bodies[0].worldY, 2.0f) && Near(bodies[0].radius, 9.0f);
}

/** Extent is in local space, so a scaled transform has to widen the footprint. */
bool TestExtentScalesWithTransform()
{
    Concord::World world{};
    SpawnBody(world, Concord::Transform{.scale = {2.0f, 1.0f, 3.0f}}, {4.0f, 4.0f});
    std::vector<Concord::RenderWaterBodySnapshot> bodies;
    Concord::AppendWaterBodySnapshots(bodies, world);
    // 4 * 2 = 8 on X, 4 * 3 = 12 on Z; the circle has to cover the wider one.
    return bodies.size() == 1 && Near(bodies[0].radius, 12.0f);
}

/** The zero-extent sentinel a vertical waterfall sheet is spawned with opts out entirely. */
bool TestZeroExtentIsSkipped()
{
    Concord::World world{};
    SpawnBody(world, Concord::Transform{}, {0.0f, 0.0f});
    SpawnBody(world, Concord::Transform{}, {5.0f, 0.0f});
    SpawnBody(world, Concord::Transform{}, {0.0f, 5.0f});
    std::vector<Concord::RenderWaterBodySnapshot> bodies;
    Concord::AppendWaterBodySnapshots(bodies, world);
    return bodies.empty();
}

/** The shader loops over this list per dry fragment, so it is bounded. */
bool TestTheListIsBounded()
{
    Concord::World world{};
    for (Concord::u32 index = 0; index < Concord::kMaxWetnessBodies + 5u; ++index) {
        SpawnBody(world, Concord::Transform{}, {3.0f, 3.0f});
    }
    std::vector<Concord::RenderWaterBodySnapshot> bodies;
    Concord::AppendWaterBodySnapshots(bodies, world);
    return bodies.size() == Concord::kMaxWetnessBodies;
}

/** A malformed transform must not reach the shader as a NaN centre or radius. */
bool TestHostileValuesAreReplaced()
{
    Concord::World world{};
    const f32 notANumber = std::nanf("");
    SpawnBody(world, Concord::Transform{.position = {notANumber, 1.0f, 2.0f},
                                        .scale = {notANumber, 1.0f, 1.0f}},
             {4.0f, 4.0f});
    std::vector<Concord::RenderWaterBodySnapshot> bodies;
    Concord::AppendWaterBodySnapshots(bodies, world);
    if (bodies.size() != 1) {
        return false;
    }
    return std::isfinite(bodies[0].centre.x) && Near(bodies[0].centre.x, 0.0f) &&
           std::isfinite(bodies[0].radius) && Near(bodies[0].worldY, 1.0f);
}

} // namespace

int main()
{
    return TestRadiusCoversTheLongerAxis() && TestExtentScalesWithTransform() &&
                   TestZeroExtentIsSkipped() && TestTheListIsBounded() &&
                   TestHostileValuesAreReplaced()
               ? 0
               : 1;
}
