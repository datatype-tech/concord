// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PARTICLESIMULATION_H
#define CONCORD_PARTICLESIMULATION_H

#include "Concord/CExport.h"
#include "engine/core/Mat4.h"
#include "engine/core/Vec4.h"
#include "engine/particle/ParticleTypes.h"

namespace Concord {

/** Allocates the pool and arms the emitter; safe to call on a live emitter. */
[[nodiscard]] CENGINE_API bool ResetParticleEmitter(
    const ParticleEmitterSettings& settings, ParticleEmitterState& state) noexcept;

/**
 * Advances one emitter by deltaTime.
 *
 * Spawns what the emission rate owes, integrates gravity and drag, then
 * retires the particles that reached their lifetime. Every count is bounded
 * by the pool capacity, and a non-positive or non-finite step is ignored.
 *
 * The transform places world-space emitters. A local-space emitter ignores
 * the translation and rotates gravity into its own frame, so a tilted
 * emitter still pulls particles toward its own floor.
 */
[[nodiscard]] CENGINE_API ParticleStepResult StepParticleEmitter(
    const ParticleEmitterSettings& settings, ParticleEmitterState& state,
    const Mat4& emitterTransform, f32 deltaTime) noexcept;

/** Normalized life fraction: 0 at birth, 1 at death. */
[[nodiscard]] CENGINE_API f32 ParticleLifeFraction(const Particle& particle) noexcept;

/** Samples the emitter colour ramp at a life fraction, alpha included. */
[[nodiscard]] CENGINE_API Vec4 ParticleColorAt(const ParticleEmitterSettings& settings,
                                               f32 fraction) noexcept;

/** Samples one particle size ramp at a life fraction. */
[[nodiscard]] CENGINE_API f32 ParticleSizeAt(const Particle& particle, f32 fraction) noexcept;

} // namespace Concord

#endif // CONCORD_PARTICLESIMULATION_H
