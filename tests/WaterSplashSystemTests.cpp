// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/WaterSplashSystem.h"

#include "engine/ecs/ParticleComponents.h"
#include "engine/ecs/PhysicsComponents.h"
#include "engine/ecs/WaterRippleComponents.h"
#include "engine/scene/Scene.h"
#include "engine/scene/Water.h"

namespace {

using Concord::f32;

/** A water surface five units wide on each side, centred at the origin. */
Concord::Entity SpawnBasin(Concord::Scene& scene, f32 worldY = 0.0f)
{
    return scene.Spawn<Concord::Object::Water>(
        {.transform = {.position = {0.0f, worldY, 0.0f}}, .splashExtent = {5.0f, 5.0f}});
}

/** A falling dynamic body, bypassing PhysicsSystem so the test controls velocity directly. */
Concord::Entity SpawnFaller(Concord::Scene& scene, Concord::Vec3 position, f32 downwardSpeed)
{
    return scene.CreateEntity()
        .Add<Concord::Transform>(Concord::Transform{.position = position})
        .Add<Concord::RigidBody>(
            Concord::RigidBody{.linearVelocity = {0.0f, -downwardSpeed, 0.0f}});
}

/** A basin opted out of splash detection -- the default splashExtent. */
bool TestUnextendedSurfaceNeverSplashes()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Water>({.transform = {.position = {0.0f, 0.0f, 0.0f}}});
    Concord::Entity faller = SpawnFaller(scene, {0.0f, 1.0f, 0.0f}, 5.0f);
    Concord::WaterSplashSystem system;
    system.OnUpdate(scene, 0.1f);
    scene.GetWorld().Get<Concord::Transform>(faller)->position.y = -0.1f;
    system.OnUpdate(scene, 0.1f);
    return scene.GetWorld().ComponentCount<Concord::WaterRippleComponent>() == 0;
}

/** Falling fast enough, from above the surface to at or below it, splashes once. */
bool TestFastFallerSplashesOnEntry()
{
    Concord::Scene scene;
    SpawnBasin(scene);
    Concord::Entity faller = SpawnFaller(scene, {1.0f, 1.0f, 0.5f}, 5.0f);
    Concord::WaterSplashSystem system;
    system.OnUpdate(scene, 0.1f);
    if (scene.GetWorld().ComponentCount<Concord::WaterRippleComponent>() != 0) {
        return false;
    }
    scene.GetWorld().Get<Concord::Transform>(faller)->position.y = -0.1f;
    system.OnUpdate(scene, 0.1f);
    return scene.GetWorld().ComponentCount<Concord::WaterRippleComponent>() == 1 &&
           scene.GetWorld().ComponentCount<Concord::ParticleEmitterComponent>() == 1;
}

/** A gentle settle -- below the minimum fall speed -- does not read as a splash. */
bool TestSlowFallerDoesNotSplash()
{
    Concord::Scene scene;
    SpawnBasin(scene);
    Concord::Entity faller = SpawnFaller(scene, {0.0f, 1.0f, 0.0f}, 0.05f);
    Concord::WaterSplashSystem system;
    system.OnUpdate(scene, 0.1f);
    scene.GetWorld().Get<Concord::Transform>(faller)->position.y = -0.05f;
    system.OnUpdate(scene, 0.1f);
    return scene.GetWorld().ComponentCount<Concord::WaterRippleComponent>() == 0;
}

/** Only Dynamic bodies can be "falling" in the sense this system cares about. */
bool TestStaticBodyNeverSplashes()
{
    Concord::Scene scene;
    SpawnBasin(scene);
    Concord::Entity faller = scene.CreateEntity()
                                 .Add<Concord::Transform>(Concord::Transform{.position = {0.0f, 1.0f, 0.0f}})
                                 .Add<Concord::RigidBody>(Concord::RigidBody{
                                     .motion = Concord::BodyMotion::Static,
                                     .linearVelocity = {0.0f, -5.0f, 0.0f}});
    Concord::WaterSplashSystem system;
    system.OnUpdate(scene, 0.1f);
    scene.GetWorld().Get<Concord::Transform>(faller)->position.y = -0.1f;
    system.OnUpdate(scene, 0.1f);
    return scene.GetWorld().ComponentCount<Concord::WaterRippleComponent>() == 0;
}

/** A body already resting on the bottom does not splash again every frame. */
bool TestSubmergedBodyDoesNotRetrigger()
{
    Concord::Scene scene;
    SpawnBasin(scene);
    Concord::Entity faller = SpawnFaller(scene, {0.0f, 1.0f, 0.0f}, 5.0f);
    Concord::WaterSplashSystem system;
    system.OnUpdate(scene, 0.1f);
    scene.GetWorld().Get<Concord::Transform>(faller)->position.y = -0.1f;
    system.OnUpdate(scene, 0.1f);
    // Still below, still falling fast on paper -- but it never left the water,
    // so a second, third, fourth frame down there must not be new impacts.
    system.OnUpdate(scene, 0.1f);
    system.OnUpdate(scene, 0.1f);
    return scene.GetWorld().ComponentCount<Concord::WaterRippleComponent>() == 1;
}

/** Outside the surface's footprint, a body can fall through the same Y for free. */
bool TestOutsideFootprintNeverSplashes()
{
    Concord::Scene scene;
    SpawnBasin(scene);
    Concord::Entity faller = SpawnFaller(scene, {40.0f, 1.0f, 0.0f}, 5.0f);
    Concord::WaterSplashSystem system;
    system.OnUpdate(scene, 0.1f);
    scene.GetWorld().Get<Concord::Transform>(faller)->position.y = -0.1f;
    system.OnUpdate(scene, 0.1f);
    return scene.GetWorld().ComponentCount<Concord::WaterRippleComponent>() == 0;
}

/** The burst emitter a splash spawns is retired once its particles are spent. */
bool TestBurstEmitterIsRetiredAfterItsLifetime()
{
    Concord::Scene scene;
    SpawnBasin(scene);
    Concord::Entity faller = SpawnFaller(scene, {0.0f, 1.0f, 0.0f}, 5.0f);
    Concord::WaterSplashSystem system;
    system.OnUpdate(scene, 0.1f);
    scene.GetWorld().Get<Concord::Transform>(faller)->position.y = -0.1f;
    system.OnUpdate(scene, 0.1f);
    if (scene.GetWorld().ComponentCount<Concord::ParticleEmitterComponent>() != 1) {
        return false;
    }
    // Longer than the burst's own particle lifetime plus the buffer the
    // system keeps it alive for -- the emitter has to be gone by then.
    for (int step = 0; step < 20; ++step) {
        system.OnUpdate(scene, 0.1f);
    }
    return scene.GetWorld().ComponentCount<Concord::ParticleEmitterComponent>() == 0;
}

} // namespace

int main()
{
    return TestUnextendedSurfaceNeverSplashes() && TestFastFallerSplashesOnEntry() &&
                   TestSlowFallerDoesNotSplash() && TestStaticBodyNeverSplashes() &&
                   TestSubmergedBodyDoesNotRetrigger() && TestOutsideFootprintNeverSplashes() &&
                   TestBurstEmitterIsRetiredAfterItsLifetime()
               ? 0
               : 1;
}
