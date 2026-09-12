// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PARTICLETYPES_H
#define CONCORD_PARTICLETYPES_H

#include "Concord/CExport.h"
#include "engine/core/Color.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"

#include <vector>

namespace Concord {

/** Volume new particles are spawned inside. */
/** How a particle emitter's billboards combine with the frame behind them. */
enum class ParticleBlendMode {
    /** Adds light; for fire, sparks and glows. */
    Additive,
    /** Occludes what is behind it and scatters the frame's light; for smoke. */
    Scatter,
};

/** Radial distribution particles are spawned over. */
enum class ParticleShape {
    /** Every particle starts at the emitter origin. */
    Point,
    /** Uniformly distributed inside a sphere of radius shapeExtent.x. */
    Sphere,
    /** Uniformly distributed inside a box of the given half extents. */
    Box,
    /** Inside a cone that opens along local +Y from the emitter origin. */
    Cone,
};

/** Inclusive range a per-particle value is drawn from. */
struct ParticleRange {
    f32 min = 0.0f;
    f32 max = 0.0f;

    /** Interpolates between the bounds using a normalized sample. */
    [[nodiscard]] f32 Sample(f32 unit) const noexcept
    {
        return min + (max - min) * unit;
    }
};

/** Hard ceiling on one emitter pool, keeping an allocation bounded. */
inline constexpr u32 kMaxParticlesPerEmitter = 65536;

/** Pool size an emitter allocates when the descriptor does not ask for one. */
inline constexpr u32 kDefaultParticleCapacity = 1024;

/** Largest simulation step accepted in one call, in seconds. */
inline constexpr f32 kMaxParticleStepSeconds = 0.1f;

/**
 * Everything an emitter needs to spawn and integrate particles.
 *
 * Every field carries a default, so a Desc names only what it changes. The
 * colour ramp runs from startColor to endColor over each particle lifetime,
 * including the alpha channel, so fading out is authored as an end alpha.
 */
struct ParticleEmitterSettings {
    /** Spawn volume. */
    ParticleShape shape = ParticleShape::Sphere;

    /** Sphere radius, box half extents, or cone radius (x) and height (y). */
    Vec3 shapeExtent{0.5f, 0.5f, 0.5f};

    /** Particles spawned per second; zero disables continuous emission. */
    f32 emissionRate = 120.0f;

    /** Particles spawned once when the emitter starts. */
    u32 burstCount = 0;

    /** Seconds each particle lives. */
    ParticleRange lifetime{0.8f, 1.6f};

    /** Initial speed along the sampled direction, in units per second. */
    ParticleRange speed{1.0f, 3.0f};

    /** Billboard size at birth, in world units. */
    ParticleRange startSize{0.05f, 0.12f};

    /** Billboard size at death, in world units. */
    ParticleRange endSize{0.0f, 0.02f};

    /** Central emission direction in emitter-local space. */
    Vec3 direction{0.0f, 1.0f, 0.0f};

    /** Half-angle of the cone directions are sampled from, in degrees. */
    f32 spreadDegrees = 180.0f;

    /** Constant acceleration, in units per second squared. */
    Vec3 gravity{0.0f, -4.0f, 0.0f};

    /** Velocity damping per second; zero keeps particles ballistic. */
    f32 drag = 0.35f;

    /** Colour and alpha at birth. */
    ColorRGBA startColor = COLOR_RGB(255, 214, 120);

    /** Colour and alpha at death. */
    ColorRGBA endColor = COLOR_RGBA(255, 72, 16, 0);

    /** Emitter seed; the same seed and step sequence replays identically. */
    u32 seed = 1u;

    /** Whether an exhausted emitter keeps emitting. */
    bool looping = true;

    /** Whether particles follow the emitter transform instead of the world. */
    bool localSpace = true;

    /** Pool capacity, clamped to kMaxParticlesPerEmitter. */
    u32 capacity = kDefaultParticleCapacity;

    /**
     * How this emitter's billboards combine with what is behind them.
     *
     * Fire, sparks and glows emit: adding light is what makes them read as
     * brighter than their surroundings, and it also makes the draw order
     * irrelevant. Smoke occludes: it has to take light away from what is behind
     * it, or a plume over a bright background turns into a glow instead of a
     * shadow, which is the clearest tell that smoke is being drawn as fire.
     *
     * Declared last so every existing designated initialiser still names its
     * fields in declaration order.
     */
    ParticleBlendMode blend = ParticleBlendMode::Additive;
};

/** One live particle. Age and lifetime drive the colour and size ramps. */
struct Particle {
    Vec3 position{};
    Vec3 velocity{};
    f32 age = 0.0f;
    f32 lifetime = 1.0f;
    f32 startSize = 0.0f;
    f32 endSize = 0.0f;
};

/**
 * Mutable emitter state.
 *
 * The pool is compacted, so only the first liveCount entries of particles are
 * meaningful; dead particles are removed by swapping the last live one into
 * their slot, which keeps the live range contiguous for rendering.
 */
struct ParticleEmitterState {
    std::vector<Particle> particles;
    u32 liveCount = 0;
    f32 emissionCarry = 0.0f;
    f32 elapsed = 0.0f;
    u32 randomState = 1u;
    u64 totalEmitted = 0;
    bool started = false;

    /** Whether the pool has been allocated. */
    [[nodiscard]] bool IsReady() const noexcept { return !particles.empty(); }
};

/** Counters reported by one simulation step. */
struct ParticleStepResult {
    u32 emitted = 0;
    u32 expired = 0;
    u32 live = 0;
};

} // namespace Concord

#endif // CONCORD_PARTICLETYPES_H
