// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CParticle.h"
#include "Concord/CScene.h"
#include "engine/render/RenderParticleLimits.h"
#include "engine/render/RenderParticleSnapshot.h"

#include <cmath>
#include <iostream>

namespace {

using Concord::Entity;
using Concord::Mat4;
using Concord::ParticleEmitterComponent;
using Concord::ParticleEmitterSettings;
using Concord::RenderParticleSnapshot;
using Concord::Transform;
using Concord::Vec3;
using Concord::World;

constexpr Concord::f32 kTolerance = 0.0005f;

bool Near(Concord::f32 a, Concord::f32 b) { return std::abs(a - b) < kTolerance; }
bool Near(Vec3 a, Vec3 b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

/** Deterministic emitter that spawns a known number of stationary particles. */
ParticleEmitterSettings FixedSettings(Concord::u32 burst, Concord::f32 size)
{
    ParticleEmitterSettings settings;
    settings.shape = Concord::ParticleShape::Point;
    settings.emissionRate = 0.0f;
    settings.burstCount = burst;
    settings.lifetime = {10.0f, 10.0f};
    settings.speed = {0.0f, 0.0f};
    settings.startSize = {size, size};
    settings.endSize = {size, size};
    settings.gravity = {0.0f, 0.0f, 0.0f};
    settings.drag = 0.0f;
    settings.startColor = COLOR_RGBA(255, 255, 255, 255);
    settings.endColor = COLOR_RGBA(255, 255, 255, 255);
    settings.capacity = 64;
    return settings;
}

Entity SpawnEmitter(World& world, const ParticleEmitterSettings& settings,
                    const Transform& transform)
{
    const Entity entity = world.Create();
    world.Add<ParticleEmitterComponent>(entity, ParticleEmitterComponent{.settings = settings});
    world.Add<Transform>(entity, transform);
    return entity;
}

/** Advances the emitter stored on `entity` by one small step. */
void StepEmitter(World& world, Entity entity, Concord::f32 deltaTime)
{
    ParticleEmitterComponent* emitter = world.Get<ParticleEmitterComponent>(entity);
    const Transform* transform = world.Get<Transform>(entity);
    if (emitter == nullptr || transform == nullptr) return;
    (void)Concord::StepParticleEmitter(emitter->settings, emitter->state, transform->ToMatrix(),
                                       deltaTime);
}

bool TestEmptyWorldProducesNoGeometry()
{
    World world;
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    return particles.vertices.empty() && particles.particleCount == 0;
}

bool TestOneParticleProducesOneQuad()
{
    World world;
    const Entity entity = SpawnEmitter(world, FixedSettings(1, 0.4f), Transform{});
    StepEmitter(world, entity, 0.01f);
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    return particles.particleCount == 1 &&
           particles.vertices.size() == Concord::kParticleIndicesPerParticle;
}

bool TestQuadCornersSpanTheSize()
{
    World world;
    const Entity entity =
        SpawnEmitter(world, FixedSettings(1, 0.4f), Transform{.position = {2.0f, 3.0f, -1.0f}});
    StepEmitter(world, entity, 0.01f);
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    if (particles.vertices.size() != 6) return false;
    Concord::f32 minX = particles.vertices[0].position.x;
    Concord::f32 maxX = minX;
    Concord::f32 minY = particles.vertices[0].position.y;
    Concord::f32 maxY = minY;
    for (const Concord::RenderParticleVertex& vertex : particles.vertices) {
        minX = std::min(minX, vertex.position.x);
        maxX = std::max(maxX, vertex.position.x);
        minY = std::min(minY, vertex.position.y);
        maxY = std::max(maxY, vertex.position.y);
    }
    // An identity view matrix puts the camera right on +X and up on +Y, so a
    // 0.4-unit particle spans 0.4 units on both axes around its centre.
    return Near(maxX - minX, 0.4f) && Near(maxY - minY, 0.4f) && Near((minX + maxX) * 0.5f, 2.0f) &&
           Near((minY + maxY) * 0.5f, 3.0f);
}

bool TestEmitterTransformPlacesLocalSpaceParticles()
{
    World world;
    ParticleEmitterSettings settings = FixedSettings(1, 0.2f);
    settings.localSpace = true;
    const Entity entity =
        SpawnEmitter(world, settings, Transform{.position = {-4.0f, 1.0f, 7.0f}});
    StepEmitter(world, entity, 0.01f);
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    if (particles.vertices.empty()) return false;
    const Vec3 first = particles.vertices[0].position;
    return std::abs(first.x + 4.0f) < 0.5f && std::abs(first.y - 1.0f) < 0.5f &&
           std::abs(first.z - 7.0f) < 0.5f;
}

/** Centre of the quad built by the most recent snapshot call. */
bool SnapshotCentre(World& world, Vec3& centre)
{
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    if (particles.vertices.size() != 6) return false;
    centre = {};
    for (const Concord::RenderParticleVertex& vertex : particles.vertices) {
        centre += vertex.position;
    }
    centre = centre / 6.0f;
    return true;
}

bool TestLocalSpaceParticlesFollowAMovedEmitter()
{
    World world;
    ParticleEmitterSettings settings = FixedSettings(1, 0.2f);
    settings.localSpace = true;
    const Entity entity =
        SpawnEmitter(world, settings, Transform{.position = {-4.0f, 1.0f, 7.0f}});
    StepEmitter(world, entity, 0.01f);
    // Moving the emitter after the spawn must carry its particles with it.
    world.Get<Transform>(entity)->position = {1.0f, 2.0f, 3.0f};
    Vec3 centre{};
    return SnapshotCentre(world, centre) && Near(centre, Vec3{1.0f, 2.0f, 3.0f});
}

bool TestWorldSpaceParticlesStayWhereTheySpawned()
{
    World world;
    ParticleEmitterSettings settings = FixedSettings(1, 0.2f);
    settings.localSpace = false;
    const Entity entity =
        SpawnEmitter(world, settings, Transform{.position = {-4.0f, 1.0f, 7.0f}});
    StepEmitter(world, entity, 0.01f);
    // A world-space particle bakes the emitter pose at spawn, so moving the
    // emitter afterwards must leave it where it was born.
    world.Get<Transform>(entity)->position = {1.0f, 2.0f, 3.0f};
    Vec3 centre{};
    return SnapshotCentre(world, centre) && Near(centre, Vec3{-4.0f, 1.0f, 7.0f});
}

bool TestInvisibleParticlesAreSkipped()
{
    World world;
    ParticleEmitterSettings settings = FixedSettings(4, 0.2f);
    settings.startColor = COLOR_RGBA(255, 255, 255, 0);
    settings.endColor = COLOR_RGBA(255, 255, 255, 0);
    const Entity entity = SpawnEmitter(world, settings, Transform{});
    StepEmitter(world, entity, 0.01f);
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    return particles.vertices.empty() && particles.particleCount == 0;
}

bool TestManyEmittersAccumulate()
{
    World world;
    const Entity first = SpawnEmitter(world, FixedSettings(3, 0.1f), Transform{});
    const Entity second = SpawnEmitter(world, FixedSettings(2, 0.1f), Transform{});
    StepEmitter(world, first, 0.01f);
    StepEmitter(world, second, 0.01f);
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    return particles.particleCount == 5 &&
           particles.vertices.size() == 5 * Concord::kParticleIndicesPerParticle;
}

bool TestVertexCapIsRespected()
{
    World world;
    ParticleEmitterSettings settings = FixedSettings(Concord::kMaxRenderedParticles + 512, 0.1f);
    settings.capacity = Concord::kMaxParticlesPerEmitter;
    const Entity entity = SpawnEmitter(world, settings, Transform{});
    StepEmitter(world, entity, 0.01f);
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    return particles.vertices.size() <= Concord::kMaxParticleVertices &&
           particles.vertices.size() == Concord::kMaxParticleVertices;
}

bool TestColorsComeFromTheRamp()
{
    World world;
    ParticleEmitterSettings settings = FixedSettings(1, 0.2f);
    settings.startColor = COLOR_RGBA(255, 0, 0, 255);
    const Entity entity = SpawnEmitter(world, settings, Transform{});
    StepEmitter(world, entity, 0.01f);
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, world, Mat4::Identity());
    if (particles.vertices.empty()) return false;
    const Concord::Vec4 color = particles.vertices[0].color;
    return Near(color.w, 1.0f) && Near(color.x, Concord::ToLinear(settings.startColor).x);
}

bool TestSystemAdvancesTheSceneEmitter()
{
    Concord::Scene scene;
    ParticleEmitterSettings settings = FixedSettings(0, 0.2f);
    settings.emissionRate = 20.0f;
    scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {}, .settings = settings});
    Concord::ParticleSystem system;
    // Twenty particles per second over five tenths of a second is ten.
    for (int step = 0; step < 5; ++step) {
        system.OnUpdate(scene, 0.1f);
    }
    RenderParticleSnapshot particles;
    Concord::AppendParticleSnapshots(particles, scene.GetWorld(), Mat4::Identity());
    return particles.particleCount == 10 &&
           particles.vertices.size() == 10 * Concord::kParticleIndicesPerParticle;
}

bool TestSystemHonoursTheRestartFlag()
{
    Concord::Scene scene;
    ParticleEmitterSettings settings = FixedSettings(6, 0.2f);
    settings.capacity = 4;
    const Concord::EntityHandle handle = scene.Spawn<Concord::Object::ParticleEmitter>(
        {.transform = {}, .settings = settings});
    Concord::ParticleSystem system;
    system.OnUpdate(scene, 0.01f);
    ParticleEmitterComponent* emitter =
        scene.GetWorld().Get<ParticleEmitterComponent>(handle.Id());
    if (emitter == nullptr || emitter->state.liveCount != 4) return false;
    emitter->restart = true;
    system.OnUpdate(scene, 0.01f);
    return !emitter->restart && emitter->state.liveCount == 4 &&
           emitter->state.totalEmitted == 4;
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"empty world produces no geometry", TestEmptyWorldProducesNoGeometry},
        {"one particle produces one quad", TestOneParticleProducesOneQuad},
        {"quad corners span the size", TestQuadCornersSpanTheSize},
        {"local-space emitter uses the transform", TestEmitterTransformPlacesLocalSpaceParticles},
        {"local-space particles follow a moved emitter",
     TestLocalSpaceParticlesFollowAMovedEmitter},
    {"world-space particles stay where they spawned",
     TestWorldSpaceParticlesStayWhereTheySpawned},
        {"invisible particles are skipped", TestInvisibleParticlesAreSkipped},
        {"many emitters accumulate", TestManyEmittersAccumulate},
        {"vertex cap is respected", TestVertexCapIsRespected},
        {"colours come from the ramp", TestColorsComeFromTheRamp},
        {"system advances the scene emitter", TestSystemAdvancesTheSceneEmitter},
        {"system honours the restart flag", TestSystemHonoursTheRestartFlag},
    };
    Concord::u32 failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    const Concord::u32 total = static_cast<Concord::u32>(sizeof(cases) / sizeof(cases[0]));
    std::cout << (total - failures) << '/' << total << " particle render cases passed\n";
    return failures == 0 ? 0 : 1;
}
