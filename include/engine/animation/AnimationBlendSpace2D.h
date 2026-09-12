// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONBLENDSPACE2D_H
#define CONCORD_ANIMATIONBLENDSPACE2D_H

#include "Concord/CExport.h"
#include "engine/asset/Animation.h"
#include "engine/core/Vec2.h"

#include <array>
#include <span>
#include <vector>

namespace Concord {

/** One clip pinned at a coordinate in the blend plane. */
struct BlendSpace2Sample {
    /** Position on the plane; conventionally (forward speed, strafe speed). */
    Vec2 position{};

    /** Clip played here; the space is invalid without one. */
    const AnimationClip* clip = nullptr;

    /** Playback rate here, letting a short clip cover more of the plane. */
    f32 speed = 1.0f;
};

/** Triangle of sample indices, wound consistently. */
using BlendSpace2Triangle = std::array<u32, 3>;

/** One resolved contribution: which sample, and how much of it to play. */
struct BlendSpace2Weight {
    u32 sample = 0;
    f32 weight = 0.0f;
};

/**
 * A two-dimensional blend space over a triangulated sample set.
 *
 * Locomotion needs this over the 1D form as soon as a character can move in
 * more than one direction: forward speed alone cannot distinguish a walk from
 * a strafe. Blending happens inside a triangle of three clips, which is why
 * the samples must be triangulated rather than merely listed.
 */
struct BlendSpace2D {
    std::vector<BlendSpace2Sample> samples;
    std::vector<BlendSpace2Triangle> triangles;

    /** Whether every sample and triangle is usable. */
    [[nodiscard]] CENGINE_API bool IsValid() const noexcept;

    /**
     * Resolves the contributions for a point on the plane.
     *
     * A point inside a triangle yields its three barycentric weights. A point
     * outside every triangle is pulled to the closest edge: the triangle whose
     * most-negative weight is largest wins, negative weights are dropped, and
     * the rest are renormalised. That keeps a value off the authored hull
     * blending the two nearest clips instead of extrapolating a pose nobody
     * authored.
     *
     * @param out Filled with at most three contributions, weights summing to 1.
     * @return false when the space is invalid or no triangle could be resolved.
     */
    [[nodiscard]] CENGINE_API bool Evaluate(Vec2 value, std::array<BlendSpace2Weight, 3>& out,
                                            u32& count) const noexcept;
};

/** Per-instance playback state for a 2D blend space. */
struct BlendSpace2State {
    /** Normalized playhead in [0, 1), shared by every sample. */
    f32 phase = 0.0f;
};

/** Clears the playhead; fails only when the space cannot be evaluated. */
[[nodiscard]] CENGINE_API bool ResetBlendSpace2D(const BlendSpace2D& space,
                                                 BlendSpace2State& state) noexcept;

/**
 * Advances the playhead and blends the resolved samples into one pose.
 *
 * Every contribution shares a normalized playhead, for the same reason the 1D
 * space does: advancing clips by their own absolute time lets the feet drift
 * apart while they cross-blend. The playhead moves at the weighted average
 * duration and speed of the clips actually contributing.
 *
 * \p scratch absorbs the intermediate samples, up to two, so a per-frame
 * update allocates nothing; callers may reuse the same two poses for every
 * blend space they evaluate.
 */
[[nodiscard]] CENGINE_API bool SampleBlendSpace2D(const BlendSpace2D& space,
                                                  BlendSpace2State& state, Vec2 value,
                                                  f32 deltaSeconds,
                                                  const Skeleton& skeleton,
                                                  SkeletonPose& output,
                                                  std::span<SkeletonPose> scratch) noexcept;

/**
 * Barycentric weights of \p point inside the triangle a, b, c.
 *
 * @return false when the triangle is degenerate, in which case the weights are
 *         left untouched rather than filled with a division by zero.
 */
[[nodiscard]] CENGINE_API bool BlendSpace2DBarycentric(Vec2 point, Vec2 a, Vec2 b, Vec2 c,
                                                       f32 weights[3]) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONBLENDSPACE2D_H
