// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderRippleSnapshot.h"

#include "engine/ecs/WaterRippleComponents.h"
#include "engine/ecs/WaterRippleSystem.h"
#include "engine/ecs/World.h"
#include "engine/scene/Scene.h"
#include "engine/scene/WaterRipple.h"

#include <cmath>
#include <vector>

namespace {

using Concord::f32;

bool Near(f32 left, f32 right)
{
    return std::abs(left - right) < 0.0001f;
}

/** Spawns one ripple directly on a world, bypassing the scene facade. */
Concord::Entity SpawnRipple(Concord::World& world, const Concord::WaterRipple& ring,
                            f32 lifetime, f32 age)
{
    const Concord::Entity entity = world.Create();
    Concord::WaterRippleComponent component{};
    component.ripple = ring;
    component.lifetime = lifetime;
    component.age = age;
    world.Add<Concord::WaterRippleComponent>(entity, component);
    return entity;
}

/**
 * A ring is drawn at the amplitude it should have, not at the one it was
 * authored with: the fade belongs here so nothing downstream needs to know a
 * ripple is dying.
 */
bool TestStrengthCarriesTheFade()
{
    Concord::World world{};
    Concord::WaterRipple ring{};
    ring.centre = {2.0f, -3.0f};
    ring.strength = 0.8f;
    SpawnRipple(world, ring, 2.0f, 0.0f);
    SpawnRipple(world, ring, 2.0f, 1.0f);
    SpawnRipple(world, ring, 2.0f, 2.0f);

    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, world);
    if (ripples.size() != 2) {
        return false;
    }
    // Halfway through the second ripple's life the curve is at 1 - 0.25.
    return Near(ripples[0].strength, 0.8f) && Near(ripples[1].strength, 0.6f) &&
           Near(ripples[0].centre.x, 2.0f) && Near(ripples[0].centre.y, -3.0f);
}

/** A ripple that has run out, or never had a life, contributes nothing. */
bool TestExpiredAndEmptyRipplesAreDropped()
{
    Concord::World world{};
    Concord::WaterRipple ring{};
    ring.strength = 0.5f;
    SpawnRipple(world, ring, 2.0f, 3.0f);
    SpawnRipple(world, ring, 0.0f, 0.0f);
    Concord::WaterRipple silent{};
    silent.strength = 0.0f;
    SpawnRipple(world, silent, 2.0f, 0.0f);

    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, world);
    return ripples.empty();
}

/**
 * The shader loops over this list per water pixel, so it is bounded rather than
 * allowed to grow with however many splashes a scene happens to have.
 */
bool TestTheListIsBounded()
{
    Concord::World world{};
    Concord::WaterRipple ring{};
    ring.strength = 0.5f;
    for (Concord::u32 index = 0; index < Concord::kMaxRenderRipples + 12u; ++index) {
        SpawnRipple(world, ring, 2.0f, 0.0f);
    }
    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, world);
    return ripples.size() == Concord::kMaxRenderRipples;
}

/** A malformed value must not reach the shader as a NaN wave slope. */
bool TestHostileValuesAreReplaced()
{
    Concord::World world{};
    const f32 notANumber = std::nanf("");
    Concord::WaterRipple ring{};
    ring.centre = {notANumber, 1.0f};
    ring.strength = 0.5f;
    ring.reach = notANumber;
    SpawnRipple(world, ring, 2.0f, 0.0f);

    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, world);
    if (ripples.size() != 1) {
        return false;
    }
    return std::isfinite(ripples[0].centre.x) && Near(ripples[0].centre.x, 0.0f) &&
           std::isfinite(ripples[0].reach) && Near(ripples[0].reach, 24.0f) &&
           Near(ripples[0].centre.y, 1.0f);
}

/**
 * The system has to age ripples and then retire them, and retiring is a
 * structural change: doing it inside the query would invalidate the dense
 * arrays being walked.
 */
bool TestSystemAgesAndRetires()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::WaterRipple>(
        {.ripple = {.strength = 0.6f}, .lifetime = 1.0f});
    Concord::WaterRippleSystem system;
    system.OnUpdate(scene, 0.4f);

    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, scene.GetWorld());
    if (ripples.size() != 1 || !Near(ripples[0].strength, 0.6f * (1.0f - 0.16f))) {
        return false;
    }
    // Past its lifetime it is gone, and the scene can be walked afterwards.
    system.OnUpdate(scene, 0.8f);
    ripples.clear();
    Concord::AppendRippleSnapshots(ripples, scene.GetWorld());
    return ripples.empty();
}

/**
 * A field spawns at its authored rate and no faster, and stops entirely when
 * it is disabled. The rate is what decides whether a body of water reads as
 * calm or as a downpour, so it has to be exactly what was asked for.
 */
bool TestFieldSpawnsAtItsRate()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::WaterRippleField>(
        {.settings = {.rate = 2.0f,
                      .extent = {5.0f, 5.0f},
                      .reach = 6.0f,
                      .lifetime = {100.0f, 100.0f}}});
    Concord::WaterRippleSystem system;
    // Ten tenths of a second at two a second is two rings, and no more.
    for (int step = 0; step < 10; ++step) {
        system.OnUpdate(scene, 0.1f);
    }
    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, scene.GetWorld());
    if (ripples.size() != 2) {
        return false;
    }
    // Every ring lands inside the area, and every one has a shape.
    for (const Concord::RenderRippleSnapshot& ring : ripples) {
        if (std::abs(ring.centre.x) > 5.0f || std::abs(ring.centre.y) > 5.0f) {
            return false;
        }
        if (!Near(ring.reach, 6.0f) || !(ring.wavelength > 0.0f)) {
            return false;
        }
    }
    return true;
}

/** Calm water is a setting, not a missing feature. */
bool TestDisabledFieldSpawnsNothing()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::WaterRippleField>(
        {.settings = {.rate = 40.0f, .lifetime = {100.0f, 100.0f}, .enabled = false}});
    scene.Spawn<Concord::Object::WaterRippleField>(
        {.settings = {.rate = 0.0f, .lifetime = {100.0f, 100.0f}}});
    Concord::WaterRippleSystem system;
    for (int step = 0; step < 20; ++step) {
        system.OnUpdate(scene, 0.1f);
    }
    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, scene.GetWorld());
    return ripples.empty();
}

/** The same seed has to replay the same field, or nothing is reproducible. */
bool TestFieldSeedReplays()
{
    auto collect = []() {
        Concord::Scene scene;
        scene.Spawn<Concord::Object::WaterRippleField>(
            {.settings = {.rate = 20.0f, .lifetime = {100.0f, 100.0f}, .seed = 7u}});
        Concord::WaterRippleSystem system;
        for (int step = 0; step < 10; ++step) {
            system.OnUpdate(scene, 0.1f);
        }
        std::vector<Concord::RenderRippleSnapshot> ripples;
        Concord::AppendRippleSnapshots(ripples, scene.GetWorld());
        return ripples;
    };
    const std::vector<Concord::RenderRippleSnapshot> first = collect();
    const std::vector<Concord::RenderRippleSnapshot> second = collect();
    if (first.size() != second.size() || first.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < first.size(); ++index) {
        if (!Near(first[index].centre.x, second[index].centre.x) ||
            !Near(first[index].centre.y, second[index].centre.y) ||
            !Near(first[index].strength, second[index].strength)) {
            return false;
        }
    }
    return true;
}

/** A zero lifetime would retire a ripple on the frame it appeared on. */
bool TestSpawnClampsLifetime()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::WaterRipple>(
        {.ripple = {.strength = 0.6f}, .lifetime = 0.0f});
    Concord::WaterRippleSystem system;
    system.OnUpdate(scene, 0.0f);
    std::vector<Concord::RenderRippleSnapshot> ripples;
    Concord::AppendRippleSnapshots(ripples, scene.GetWorld());
    return ripples.size() == 1;
}

} // namespace

int main()
{
    return TestStrengthCarriesTheFade() && TestExpiredAndEmptyRipplesAreDropped() &&
                   TestTheListIsBounded() && TestHostileValuesAreReplaced() &&
                   TestSystemAgesAndRetires() && TestSpawnClampsLifetime() &&
                   TestFieldSpawnsAtItsRate() && TestDisabledFieldSpawnsNothing() &&
                   TestFieldSeedReplays()
               ? 0
               : 1;
}
