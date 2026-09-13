// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CParticle.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using Concord::Mat4;
using Concord::ParticleEmitterSettings;
using Concord::ParticleEmitterState;
using Concord::ParticleStepResult;
using Concord::Vec3;
using Concord::Vec4;

constexpr Concord::f32 kTolerance = 0.001f;

bool Near(Concord::f32 a, Concord::f32 b) { return std::abs(a - b) < kTolerance; }
bool Near(Vec3 a, Vec3 b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

/** Steps an emitter when only the side effects matter. */
void Step(const ParticleEmitterSettings& settings, ParticleEmitterState& state,
          const Mat4& transform, Concord::f32 deltaTime)
{
    (void)Concord::StepParticleEmitter(settings, state, transform, deltaTime);
}

/** Deterministic emitter: fixed lifetime, no motion, no randomness of note. */
ParticleEmitterSettings FixedSettings()
{
    ParticleEmitterSettings settings;
    settings.shape = Concord::ParticleShape::Point;
    settings.emissionRate = 0.0f;
    settings.burstCount = 0;
    settings.lifetime = {2.0f, 2.0f};
    settings.speed = {0.0f, 0.0f};
    settings.startSize = {0.1f, 0.1f};
    settings.endSize = {0.1f, 0.1f};
    settings.gravity = {0.0f, 0.0f, 0.0f};
    settings.drag = 0.0f;
    settings.spreadDegrees = 0.0f;
    settings.direction = {0.0f, 1.0f, 0.0f};
    settings.capacity = 32;
    return settings;
}

bool TestResetAllocatesPool()
{
    const ParticleEmitterSettings settings = FixedSettings();
    ParticleEmitterState state;
    if (!Concord::ResetParticleEmitter(settings, state)) return false;
    return state.IsReady() && state.particles.size() == 32 && state.liveCount == 0 &&
           state.started;
}

bool TestZeroCapacityIsRejected()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.capacity = 0;
    ParticleEmitterState state;
    return !Concord::ResetParticleEmitter(settings, state) && !state.started;
}

bool TestCapacityClampsToCeiling()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.capacity = Concord::kMaxParticlesPerEmitter + 4096;
    ParticleEmitterState state;
    if (!Concord::ResetParticleEmitter(settings, state)) return false;
    return state.particles.size() == Concord::kMaxParticlesPerEmitter;
}

bool TestBurstSpawnsRequestedCount()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 5;
    settings.capacity = 8;
    ParticleEmitterState state;
    const ParticleStepResult result =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.016f);
    return result.emitted == 5 && result.live == 5 && state.totalEmitted == 5;
}

bool TestBurstIsBoundedByCapacity()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 100;
    settings.capacity = 7;
    ParticleEmitterState state;
    const ParticleStepResult result =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.016f);
    return result.emitted == 7 && result.live == 7;
}

bool TestBurstRunsOnlyOnce()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 5;
    settings.capacity = 32;
    ParticleEmitterState state;
    const ParticleStepResult first =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.05f);
    const ParticleStepResult second =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.05f);
    return first.emitted == 5 && second.emitted == 0 && state.totalEmitted == 5;
}

bool TestEmissionRateAccumulates()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.emissionRate = 10.0f;
    settings.capacity = 64;
    ParticleEmitterState state;
    Concord::u32 total = 0;
    for (int step = 0; step < 10; ++step) {
        total += Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.1f).emitted;
    }
    return total == 10 && state.liveCount == 10;
}

bool TestParticlesExpireAfterLifetime()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 4;
    settings.lifetime = {0.3f, 0.3f};
    settings.capacity = 8;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Identity(), 0.1f);
    Step(settings, state, Mat4::Identity(), 0.1f);
    if (state.liveCount != 4) return false;
    const ParticleStepResult result =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.1f);
    return result.expired == 4 && state.liveCount == 0;
}

bool TestExpiredSlotsAreRecycled()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 6;
    settings.lifetime = {0.2f, 0.2f};
    settings.capacity = 6;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Identity(), 0.1f);
    Step(settings, state, Mat4::Identity(), 0.1f);
    if (state.liveCount != 0) return false;
    state.elapsed = 0.0f;
    const ParticleStepResult second =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.05f);
    return second.emitted == 6 && second.live == 6;
}

bool TestStepIsClampedToTheMaximum()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 1;
    settings.capacity = 4;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Identity(), 0.01f);
    if (state.liveCount != 1) return false;
    // A hitch must not teleport particles through their whole lifetime.
    Step(settings, state, Mat4::Identity(), 5.0f);
    return Near(state.particles[0].age, 0.01f + Concord::kMaxParticleStepSeconds) &&
           Near(state.elapsed, 0.01f + Concord::kMaxParticleStepSeconds);
}

bool TestSameSeedReplaysIdentically()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.shape = Concord::ParticleShape::Sphere;
    settings.shapeExtent = {2.0f, 2.0f, 2.0f};
    settings.emissionRate = 200.0f;
    settings.speed = {1.0f, 4.0f};
    settings.seed = 42u;
    settings.capacity = 64;
    ParticleEmitterState first;
    ParticleEmitterState second;
    for (int step = 0; step < 8; ++step) {
        Step(settings, first, Mat4::Identity(), 0.016f);
        Step(settings, second, Mat4::Identity(), 0.016f);
    }
    if (first.liveCount == 0 || first.liveCount != second.liveCount) return false;
    for (Concord::u32 index = 0; index < first.liveCount; ++index) {
        if (!Near(first.particles[index].position, second.particles[index].position) ||
            !Near(first.particles[index].velocity, second.particles[index].velocity)) {
            return false;
        }
    }
    return true;
}

bool TestDifferentSeedsDiverge()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.shape = Concord::ParticleShape::Sphere;
    settings.emissionRate = 200.0f;
    settings.speed = {1.0f, 4.0f};
    settings.capacity = 64;
    ParticleEmitterState first;
    ParticleEmitterState second;
    settings.seed = 1u;
    Step(settings, first, Mat4::Identity(), 0.05f);
    settings.seed = 999u;
    Step(settings, second, Mat4::Identity(), 0.05f);
    if (first.liveCount == 0 || second.liveCount == 0) return false;
    return !Near(first.particles[0].position, second.particles[0].position);
}

bool TestGravityIntegratesVelocity()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 1;
    settings.gravity = {0.0f, -10.0f, 0.0f};
    settings.capacity = 4;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Identity(), 0.1f);
    if (state.liveCount != 1) return false;
    if (!Near(state.particles[0].velocity.y, -1.0f)) return false;
    Step(settings, state, Mat4::Identity(), 0.1f);
    return Near(state.particles[0].velocity.y, -2.0f) &&
           Near(state.particles[0].position.y, -0.3f);
}

bool TestDragReducesSpeed()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.burstCount = 1;
    settings.speed = {10.0f, 10.0f};
    settings.drag = 1.0f;
    settings.capacity = 4;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Identity(), 0.1f);
    if (state.liveCount != 1) return false;
    // One step of dt applies 1 / (1 + drag * dt).
    return Near(state.particles[0].velocity.y, 10.0f / (1.0f + 1.0f * 0.1f));
}

bool TestWorldSpaceEmittersUseTheTransform()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.localSpace = false;
    settings.burstCount = 1;
    settings.capacity = 4;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Translate({5.0f, 0.0f, -3.0f}), 0.01f);
    if (state.liveCount != 1) return false;
    return Near(state.particles[0].position, {5.0f, 0.0f, -3.0f});
}

bool TestLocalSpaceEmittersIgnoreTranslation()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.localSpace = true;
    settings.burstCount = 1;
    settings.capacity = 4;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Translate({5.0f, 0.0f, -3.0f}), 0.01f);
    if (state.liveCount != 1) return false;
    return Near(state.particles[0].position, {0.0f, 0.0f, 0.0f});
}

bool TestLocalSpaceGravityFollowsTheEmitter()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.localSpace = true;
    settings.burstCount = 1;
    settings.gravity = {0.0f, -10.0f, 0.0f};
    settings.capacity = 4;
    ParticleEmitterState state;
    // Gravity is authored in world space. Rolling the emitter 90 degrees about
    // Z maps local -X onto world -Y, so world down becomes local -X and the
    // particle must still fall the way the world says it does.
    Step(settings, state, Mat4::Rotate(1.5707963f, {0.0f, 0.0f, 1.0f}), 0.1f);
    if (state.liveCount != 1) return false;
    return Near(state.particles[0].velocity.x, -1.0f) &&
           Near(state.particles[0].velocity.y, 0.0f);
}

bool TestNonLoopingEmitterStops()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.looping = false;
    settings.emissionRate = 100.0f;
    settings.lifetime = {0.2f, 0.2f};
    settings.capacity = 256;
    ParticleEmitterState state;
    for (int step = 0; step < 6; ++step) {
        Step(settings, state, Mat4::Identity(), 0.1f);
    }
    const Concord::u64 emittedWhenStopped = state.totalEmitted;
    if (emittedWhenStopped == 0) return false;
    for (int step = 0; step < 5; ++step) {
        Step(settings, state, Mat4::Identity(), 0.1f);
    }
    return state.totalEmitted == emittedWhenStopped;
}

bool TestNonPositiveStepIsIgnored()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.emissionRate = 100.0f;
    ParticleEmitterState state;
    Step(settings, state, Mat4::Identity(), 0.1f);
    const Concord::u32 before = state.liveCount;
    const Concord::u64 emittedBefore = state.totalEmitted;
    const ParticleStepResult zero =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), 0.0f);
    const ParticleStepResult negative =
        Concord::StepParticleEmitter(settings, state, Mat4::Identity(), -1.0f);
    const ParticleStepResult nan = Concord::StepParticleEmitter(
        settings, state, Mat4::Identity(), std::numeric_limits<Concord::f32>::quiet_NaN());
    return before == state.liveCount && emittedBefore == state.totalEmitted &&
           zero.live == before && negative.live == before && nan.live == before;
}

bool TestColorRampEndpoints()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.startColor = COLOR_RGBA(255, 128, 0, 255);
    settings.endColor = COLOR_RGBA(0, 0, 255, 0);
    const Vec4 start = Concord::ParticleColorAt(settings, 0.0f);
    const Vec4 end = Concord::ParticleColorAt(settings, 1.0f);
    const Vec4 middle = Concord::ParticleColorAt(settings, 0.5f);
    const Vec3 startLinear = Concord::ToLinear(settings.startColor);
    return Near(start.x, startLinear.x) && Near(start.w, 1.0f) && Near(end.z, 1.0f) &&
           Near(end.w, 0.0f) && Near(middle.w, 0.5f) && Near(middle.z, 0.5f);
}

bool TestSizeRampAndLifeFraction()
{
    Concord::Particle particle;
    particle.lifetime = 2.0f;
    particle.age = 0.5f;
    particle.startSize = 0.4f;
    particle.endSize = 0.0f;
    const Concord::f32 fraction = Concord::ParticleLifeFraction(particle);
    return Near(fraction, 0.25f) && Near(Concord::ParticleSizeAt(particle, fraction), 0.3f) &&
           Near(Concord::ParticleSizeAt(particle, -5.0f), 0.4f) &&
           Near(Concord::ParticleSizeAt(particle, 5.0f), 0.0f);
}

bool TestSpawnVolumeStaysInsideTheShape()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.shape = Concord::ParticleShape::Box;
    settings.shapeExtent = {1.0f, 2.0f, 3.0f};
    settings.emissionRate = 4000.0f;
    settings.capacity = 512;
    ParticleEmitterState state;
    for (int step = 0; step < 5; ++step) {
        Step(settings, state, Mat4::Identity(), 0.02f);
    }
    if (state.liveCount == 0) return false;
    for (Concord::u32 index = 0; index < state.liveCount; ++index) {
        const Vec3 position = state.particles[index].position;
        if (std::abs(position.x) > 1.0001f || std::abs(position.y) > 2.0001f ||
            std::abs(position.z) > 3.0001f) {
            return false;
        }
    }
    return true;
}

bool TestSphereSpawnStaysInsideRadius()
{
    ParticleEmitterSettings settings = FixedSettings();
    settings.shape = Concord::ParticleShape::Sphere;
    settings.shapeExtent = {1.5f, 1.5f, 1.5f};
    settings.emissionRate = 4000.0f;
    settings.capacity = 512;
    ParticleEmitterState state;
    for (int step = 0; step < 5; ++step) {
        Step(settings, state, Mat4::Identity(), 0.02f);
    }
    if (state.liveCount == 0) return false;
    for (Concord::u32 index = 0; index < state.liveCount; ++index) {
        if (Concord::Length(state.particles[index].position) > 1.5001f) return false;
    }
    return true;
}

struct Case {
    const char* name;
    bool (*run)();
};

} // namespace

int main()
{
    const Case cases[] = {
        {"reset allocates the pool", TestResetAllocatesPool},
        {"zero capacity is rejected", TestZeroCapacityIsRejected},
        {"capacity clamps to the ceiling", TestCapacityClampsToCeiling},
        {"burst spawns the requested count", TestBurstSpawnsRequestedCount},
        {"burst is bounded by capacity", TestBurstIsBoundedByCapacity},
        {"burst runs only once", TestBurstRunsOnlyOnce},
        {"emission rate accumulates", TestEmissionRateAccumulates},
        {"particles expire after their lifetime", TestParticlesExpireAfterLifetime},
        {"expired slots are recycled", TestExpiredSlotsAreRecycled},
        {"step is clamped to the maximum", TestStepIsClampedToTheMaximum},
        {"same seed replays identically", TestSameSeedReplaysIdentically},
        {"different seeds diverge", TestDifferentSeedsDiverge},
        {"gravity integrates velocity", TestGravityIntegratesVelocity},
        {"drag reduces speed", TestDragReducesSpeed},
        {"world-space emitters use the transform", TestWorldSpaceEmittersUseTheTransform},
        {"local-space emitters ignore translation", TestLocalSpaceEmittersIgnoreTranslation},
        {"local-space gravity follows the emitter", TestLocalSpaceGravityFollowsTheEmitter},
        {"non-looping emitter stops", TestNonLoopingEmitterStops},
        {"non-positive step is ignored", TestNonPositiveStepIsIgnored},
        {"colour ramp endpoints", TestColorRampEndpoints},
        {"size ramp and life fraction", TestSizeRampAndLifeFraction},
        {"spawn volume stays inside the box", TestSpawnVolumeStaysInsideTheShape},
        {"sphere spawn stays inside the radius", TestSphereSpawnStaysInsideRadius},
    };
    Concord::u32 failures = 0;
    for (const Case& test : cases) {
        if (!test.run()) {
            std::cerr << "FAILED: " << test.name << '\n';
            ++failures;
        }
    }
    const Concord::u32 total = static_cast<Concord::u32>(sizeof(cases) / sizeof(cases[0]));
    std::cout << (total - failures) << '/' << total << " particle cases passed\n";
    return failures == 0 ? 0 : 1;
}
