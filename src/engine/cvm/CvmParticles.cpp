// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/core/Types.h"
#include "engine/cvm/CvmSceneAccess.h"
#include "engine/ecs/ParticleComponents.h"
#include "engine/particle/ParticleTypes.h"

#include <cstdint>

/**
 * Particle emitter configuration.
 *
 * An emitter is spawned with defaults and then configured. The alternative --
 * one spawn taking every setting -- is fifteen positional arguments, which is a
 * call nobody can read and where a misplaced value is invisible; a setter that
 * returns 0 because the entity carries no emitter is not.
 */
namespace {

using Concord::Cvm::Channel;

/** Runs \p fn over an entity's particle emitter, returning 1 when it ran. */
template <typename Fn>
std::int64_t EditEmitter(std::int64_t entity, Fn&& fn)
{
    return Concord::Cvm::EditComponent<Concord::ParticleEmitterComponent>(
        entity, [&fn](Concord::ParticleEmitterComponent& emitter) {
            fn(emitter.settings);
            // Re-arming is how a settings change reaches the pool: capacity and
            // the ranges are read when the pool is built, not per particle.
            emitter.restart = true;
        });
}

/** Clamps a shape index onto the enumeration, rejecting anything else. */
bool ToShape(std::int64_t value, Concord::ParticleShape& shape)
{
    if (value < 0 || value > 3) return false;
    shape = static_cast<Concord::ParticleShape>(value);
    return true;
}

} // namespace

extern "C" {

std::int64_t ConcordCvmParticlesSetRate(std::int64_t entity, double rate)
{
    const Concord::f32 value = static_cast<Concord::f32>(rate < 0.0 ? 0.0 : rate);
    return EditEmitter(entity, [value](Concord::ParticleEmitterSettings& settings) {
        settings.emissionRate = value;
    });
}

std::int64_t ConcordCvmParticlesSetSpeed(std::int64_t entity, double low, double high)
{
    const Concord::f32 from = static_cast<Concord::f32>(low);
    const Concord::f32 to = static_cast<Concord::f32>(high < low ? low : high);
    return EditEmitter(entity, [from, to](Concord::ParticleEmitterSettings& settings) {
        settings.speed = {from, to};
    });
}

std::int64_t ConcordCvmParticlesSetLifetime(std::int64_t entity, double low, double high)
{
    const Concord::f32 from = static_cast<Concord::f32>(low);
    const Concord::f32 to = static_cast<Concord::f32>(high < low ? low : high);
    return EditEmitter(entity, [from, to](Concord::ParticleEmitterSettings& settings) {
        settings.lifetime = {from, to};
    });
}

std::int64_t ConcordCvmParticlesSetSize(std::int64_t entity, double start, double end)
{
    const Concord::f32 born = static_cast<Concord::f32>(start);
    const Concord::f32 dies = static_cast<Concord::f32>(end);
    return EditEmitter(entity, [born, dies](Concord::ParticleEmitterSettings& settings) {
        settings.startSize = {born, born};
        settings.endSize = {dies, dies};
    });
}

std::int64_t ConcordCvmParticlesSetShape(std::int64_t entity, std::int64_t shape, double extentX,
                                         double extentY, double extentZ)
{
    Concord::ParticleShape chosen = Concord::ParticleShape::Sphere;
    if (!ToShape(shape, chosen)) return 0;
    const Concord::Vec3 extent =
        Concord::Cvm::ToVec3(extentX, extentY, extentZ);
    return EditEmitter(entity, [chosen, extent](Concord::ParticleEmitterSettings& settings) {
        settings.shape = chosen;
        settings.shapeExtent = extent;
    });
}

std::int64_t ConcordCvmParticlesSetDirection(std::int64_t entity, double x, double y, double z,
                                             double spreadDegrees)
{
    const Concord::Vec3 direction = Concord::Cvm::ToVec3(x, y, z);
    const Concord::f32 spread = static_cast<Concord::f32>(spreadDegrees);
    return EditEmitter(entity, [direction, spread](Concord::ParticleEmitterSettings& settings) {
        settings.direction = direction;
        settings.spreadDegrees = spread;
    });
}

std::int64_t ConcordCvmParticlesSetGravity(std::int64_t entity, double x, double y, double z,
                                           double drag)
{
    const Concord::Vec3 acceleration = Concord::Cvm::ToVec3(x, y, z);
    const Concord::f32 damping = static_cast<Concord::f32>(drag);
    return EditEmitter(entity, [acceleration, damping](
                                   Concord::ParticleEmitterSettings& settings) {
        settings.gravity = acceleration;
        settings.drag = damping;
    });
}

std::int64_t ConcordCvmParticlesSetColor(std::int64_t entity, std::int64_t red,
                                         std::int64_t green, std::int64_t blue,
                                         std::int64_t endRed, std::int64_t endGreen,
                                         std::int64_t endBlue)
{
    // Both ends fade to zero alpha: a particle that vanishes at full opacity
    // pops, and the whole point of a ramp is that it does not.
    const Concord::ColorRGBA start = COLOR_RGBA(Channel(red), Channel(green), Channel(blue), 255);
    const Concord::ColorRGBA end = COLOR_RGBA(Channel(endRed), Channel(endGreen), Channel(endBlue), 0);
    return EditEmitter(entity, [start, end](Concord::ParticleEmitterSettings& settings) {
        settings.startColor = start;
        settings.endColor = end;
    });
}

std::int64_t ConcordCvmParticlesSetBlend(std::int64_t entity, std::int64_t blend)
{
    if (blend < 0 || blend > 1) return 0;
    const auto mode = static_cast<Concord::ParticleBlendMode>(blend);
    return EditEmitter(entity, [mode](Concord::ParticleEmitterSettings& settings) {
        settings.blend = mode;
    });
}

std::int64_t ConcordCvmParticlesSetCapacity(std::int64_t entity, std::int64_t capacity)
{
    if (capacity < 1) return 0;
    if (capacity > 1 << 20) capacity = 1 << 20;
    const Concord::u32 pool = static_cast<Concord::u32>(capacity);
    return EditEmitter(entity, [pool](Concord::ParticleEmitterSettings& settings) {
        settings.capacity = pool;
    });
}

} // extern "C"
