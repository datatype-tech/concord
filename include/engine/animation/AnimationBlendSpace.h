// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONBLENDSPACE_H
#define CONCORD_ANIMATIONBLENDSPACE_H

#include "Concord/CExport.h"
#include "engine/asset/Animation.h"
#include "engine/asset/Skeleton.h"

#include <vector>

namespace Concord {

/** One clip pinned at a coordinate on the blend axis. */
struct BlendSpaceSample {
    /** Position on the blend axis, for example a speed in metres per second. */
    f32 position = 0.0f;

    /** Clip played at this position; the space is invalid without one. */
    const AnimationClip* clip = nullptr;

    /** Playback rate here, letting a short clip cover more of the axis. */
    f32 speed = 1.0f;
};

/**
 * A one-dimensional blend space.
 *
 * Samples are authored in strictly ascending position order. A value between
 * two samples blends them; a value outside the authored range clamps to the
 * nearest sample rather than extrapolating, so a locomotion space can never
 * invent a pose no animator posed.
 */
struct BlendSpace1D {
    std::vector<BlendSpaceSample> samples;

    /** Whether every sample is usable and the axis is strictly ascending. */
    [[nodiscard]] CENGINE_API bool IsValid() const noexcept;

    /**
     * Resolves the pair of samples bracketing \p value.
     *
     * @param lower Index of the sample at or below the value.
     * @param upper Index of the sample above it, equal to \p lower when the
     *        value lands on a sample or the space holds a single entry.
     * @param weight Blend amount toward \p upper, in [0, 1].
     */
    [[nodiscard]] CENGINE_API bool Evaluate(f32 value, usize& lower, usize& upper,
                                            f32& weight) const noexcept;
};

/** Per-instance playback state for a blend space. */
struct BlendSpaceState {
    /** Normalized playhead in [0, 1), shared by every sample. */
    f32 phase = 0.0f;
};

/** Clears the playhead; fails only when the space has no usable sample. */
[[nodiscard]] CENGINE_API bool ResetBlendSpace(const BlendSpace1D& space,
                                               BlendSpaceState& state) noexcept;

/**
 * Advances a normalized playhead for one step, wrapped into [0, 1).
 *
 * Shared phase is what keeps a slow walk and a fast run landing on the same
 * beat while they cross-blend; advancing each clip by its own absolute time
 * would let the feet drift apart mid-blend.
 */
[[nodiscard]] CENGINE_API f32 AdvanceBlendSpacePhase(f32 phase, f32 deltaSeconds,
                                                     f32 duration, f32 speed) noexcept;

/**
 * Advances the playhead and samples the space into a blended local pose.
 *
 * \p scratch holds the second sample so a per-frame update allocates nothing;
 * callers may reuse one scratch pose across every blend space they evaluate.
 */
[[nodiscard]] CENGINE_API bool SampleBlendSpace1D(const BlendSpace1D& space,
                                                  BlendSpaceState& state, f32 value,
                                                  f32 deltaSeconds, const Skeleton& skeleton,
                                                  SkeletonPose& output,
                                                  SkeletonPose& scratch) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONBLENDSPACE_H
