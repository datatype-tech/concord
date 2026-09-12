// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/particle/ParticleSimulation.h"

#include "engine/particle/ParticleSampling.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Clamps a life fraction into [0, 1], treating NaN as the end of life. */
f32 SafeFraction(f32 fraction) noexcept
{
    return std::isfinite(fraction) ? std::clamp(fraction, 0.0f, 1.0f) : 1.0f;
}

/** Transforms an affine point, ignoring the homogeneous divide. */
Vec3 TransformPoint(const Mat4& matrix, Vec3 point) noexcept
{
    const Vec4 result = matrix * Vec4{point.x, point.y, point.z, 1.0f};
    return {result.x, result.y, result.z};
}

/** Places the emitter gravity in the frame the particles are integrated in. */
Vec3 ResolveGravity(const ParticleEmitterSettings& settings, const Mat4& emitterTransform) noexcept
{
    if (!settings.localSpace) {
        return settings.gravity;
    }
    const Vec3 local = TransformParticleDirection(emitterTransform.InvertRigid(), settings.gravity);
    return std::isfinite(local.x) && std::isfinite(local.y) && std::isfinite(local.z)
               ? local
               : settings.gravity;
}

/** Appends one particle to the compacted live range. */
void SpawnParticle(const ParticleEmitterSettings& settings, ParticleEmitterState& state,
                   const Mat4& emitterTransform) noexcept
{
    if (state.liveCount >= state.particles.size()) {
        return;
    }
    const Vec3 localPosition = SampleParticleSpawn(settings, state.randomState);
    const Vec3 localVelocity =
        SampleParticleDirection(settings, state.randomState) *
        settings.speed.Sample(SampleParticleUnit(state.randomState));
    Particle particle{};
    if (settings.localSpace) {
        particle.position = localPosition;
        particle.velocity = localVelocity;
    } else {
        particle.position = TransformPoint(emitterTransform, localPosition);
        particle.velocity = TransformParticleDirection(emitterTransform, localVelocity);
    }
    particle.lifetime =
        std::max(settings.lifetime.Sample(SampleParticleUnit(state.randomState)), 0.0001f);
    particle.startSize =
        std::max(settings.startSize.Sample(SampleParticleUnit(state.randomState)), 0.0f);
    particle.endSize =
        std::max(settings.endSize.Sample(SampleParticleUnit(state.randomState)), 0.0f);
    state.particles[state.liveCount++] = particle;
    ++state.totalEmitted;
}

} // namespace

bool ResetParticleEmitter(const ParticleEmitterSettings& settings,
                          ParticleEmitterState& state) noexcept
{
    const u32 capacity = std::min(settings.capacity, kMaxParticlesPerEmitter);
    if (capacity == 0) {
        return false;
    }
    try {
        state.particles.assign(capacity, Particle{});
    } catch (...) {
        state.particles.clear();
        state.liveCount = 0;
        state.started = false;
        return false;
    }
    state.liveCount = 0;
    state.emissionCarry = 0.0f;
    state.elapsed = 0.0f;
    state.totalEmitted = 0;
    state.randomState = settings.seed == 0u ? 1u : settings.seed;
    state.started = true;
    return true;
}

ParticleStepResult StepParticleEmitter(const ParticleEmitterSettings& settings,
                                       ParticleEmitterState& state,
                                       const Mat4& emitterTransform, f32 deltaTime) noexcept
{
    ParticleStepResult result{};
    if (!state.started && !ResetParticleEmitter(settings, state)) {
        return result;
    }
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
        result.live = state.liveCount;
        return result;
    }
    const f32 step = std::min(deltaTime, kMaxParticleStepSeconds);
    const u32 capacity = static_cast<u32>(state.particles.size());

    if (settings.burstCount != 0 && state.elapsed <= 0.0f) {
        const u32 burst = std::min(settings.burstCount, capacity - state.liveCount);
        for (u32 index = 0; index < burst; ++index) {
            SpawnParticle(settings, state, emitterTransform);
            ++result.emitted;
        }
    }

    const bool emitting = settings.emissionRate > 0.0f &&
                          (settings.looping || state.elapsed < settings.lifetime.max);
    if (emitting) {
        state.emissionCarry += settings.emissionRate * step;
        u32 owed = static_cast<u32>(std::floor(state.emissionCarry));
        // A long hitch must not queue an unbounded backlog of spawns.
        owed = std::min(owed, static_cast<u32>(settings.emissionRate) + 1u);
        state.emissionCarry -= static_cast<f32>(owed);
        const u32 spawn = std::min(owed, capacity - state.liveCount);
        for (u32 index = 0; index < spawn; ++index) {
            SpawnParticle(settings, state, emitterTransform);
            ++result.emitted;
        }
        if (spawn < owed) {
            state.emissionCarry = 0.0f;
        }
    }

    const Vec3 gravity = ResolveGravity(settings, emitterTransform);
    const f32 drag = std::max(settings.drag, 0.0f);
    const f32 damping = drag > 0.0f ? 1.0f / (1.0f + drag * step) : 1.0f;
    u32 index = 0;
    while (index < state.liveCount) {
        Particle& particle = state.particles[index];
        particle.velocity = particle.velocity * damping + gravity * step;
        particle.position += particle.velocity * step;
        particle.age += step;
        if (particle.age >= particle.lifetime) {
            state.particles[index] = state.particles[state.liveCount - 1];
            --state.liveCount;
            ++result.expired;
            continue;
        }
        ++index;
    }
    state.elapsed += step;
    result.live = state.liveCount;
    return result;
}

f32 ParticleLifeFraction(const Particle& particle) noexcept
{
    return SafeFraction(particle.age / particle.lifetime);
}

Vec4 ParticleColorAt(const ParticleEmitterSettings& settings, f32 fraction) noexcept
{
    const f32 t = SafeFraction(fraction);
    const Vec3 start = ToLinear(settings.startColor);
    const Vec3 end = ToLinear(settings.endColor);
    const Vec3 mixed = start + (end - start) * t;
    const f32 startAlpha = static_cast<f32>(ColorA(settings.startColor)) / 255.0f;
    const f32 endAlpha = static_cast<f32>(ColorA(settings.endColor)) / 255.0f;
    return {mixed.x, mixed.y, mixed.z, startAlpha + (endAlpha - startAlpha) * t};
}

f32 ParticleSizeAt(const Particle& particle, f32 fraction) noexcept
{
    const f32 t = SafeFraction(fraction);
    return particle.startSize + (particle.endSize - particle.startSize) * t;
}

} // namespace Concord
