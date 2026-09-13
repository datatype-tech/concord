// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERPARTICLELIMITS_H
#define CONCORD_RENDERPARTICLELIMITS_H

#include "engine/core/Types.h"

namespace Concord {

/** Triangles per billboard: two, sharing the quad diagonal. */
inline constexpr u32 kParticleIndicesPerParticle = 6;

/** Particles the renderer will draw in one frame; the rest are dropped. */
inline constexpr u32 kMaxRenderedParticles = 8192;

/** Vertices one frame of particle geometry may occupy. */
inline constexpr u32 kMaxParticleVertices = kMaxRenderedParticles * kParticleIndicesPerParticle;

} // namespace Concord

#endif // CONCORD_RENDERPARTICLELIMITS_H
