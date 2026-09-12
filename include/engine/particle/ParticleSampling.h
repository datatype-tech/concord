// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PARTICLESAMPLING_H
#define CONCORD_PARTICLESAMPLING_H

#include "engine/core/Mat4.h"
#include "engine/core/Vec3.h"
#include "engine/particle/ParticleTypes.h"

namespace Concord {

/** Advances a deterministic xorshift generator and returns the next word. */
u32 NextParticleRandom(u32& state) noexcept;

/** Returns a sample in [0, 1). */
f32 SampleParticleUnit(u32& state) noexcept;

/** Samples a point inside the emitter spawn volume, in emitter-local space. */
Vec3 SampleParticleSpawn(const ParticleEmitterSettings& settings, u32& state) noexcept;

/** Samples a unit direction inside the emitter spread cone, in local space. */
Vec3 SampleParticleDirection(const ParticleEmitterSettings& settings, u32& state) noexcept;

/** Rotates a direction by the linear part of a matrix, ignoring translation. */
Vec3 TransformParticleDirection(const Mat4& matrix, Vec3 direction) noexcept;

/** Builds an orthonormal basis whose third axis is the normalized input. */
void BuildParticleBasis(Vec3 axis, Vec3& tangent, Vec3& bitangent) noexcept;

} // namespace Concord

#endif // CONCORD_PARTICLESAMPLING_H
