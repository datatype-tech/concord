// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERPARTICLESNAPSHOT_H
#define CONCORD_RENDERPARTICLESNAPSHOT_H

#include "engine/core/Mat4.h"
#include "engine/core/Types.h"
#include "engine/core/Vec2.h"
#include "engine/core/Vec3.h"
#include "engine/core/Vec4.h"

#include <vector>

namespace Concord {

class World;

/** One corner of a camera-facing particle quad. */
struct RenderParticleVertex {
    /** World-space corner position; the billboard is built facing the camera. */
    Vec3 position{};
    /** Quad corner in [0, 1], driving the analytic sprite falloff. */
    Vec2 texcoord{};
    /** Linear RGB plus intensity in the alpha channel. */
    Vec4 color{};
};

/**
 * Flattened particle geometry for one frame.
 *
 * Emitters are expanded on the CPU into world-space triangles, so the draw
 * needs no instancing and no per-particle shader input beyond the vertex
 * attributes. The array is capped at kMaxParticleVertices; emitters that do
 * not fit are skipped rather than allowed to grow without bound.
 */
struct RenderParticleSnapshot {
    /**
     * Every billboard of the frame, additive emitters first.
     *
     * One array rather than two so a frame still costs a single upload. The
     * split is carried by index instead, because the two kinds of emitter need
     * different blend states and a blend state belongs to a draw call: light
     * that adds and smoke that occludes cannot share one.
     */
    std::vector<RenderParticleVertex> vertices;

    /** Vertices at the front of the array that use additive blending. */
    u32 additiveVertices = 0;

    /** Particles represented by the current vertex array. */
    u32 particleCount = 0;
};

/**
 * Expands every live emitter in `world` into camera-facing billboards.
 *
 * @param view The frame camera view matrix; its rows supply the billboard
 *        right and up axes. A degenerate matrix falls back to world axes.
 */
void AppendParticleSnapshots(RenderParticleSnapshot& particles, const World& world,
                             const Mat4& view);

} // namespace Concord

#endif // CONCORD_RENDERPARTICLESNAPSHOT_H
