// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationBlendSpace.h"

#include "engine/animation/AnimationBlend.h"

#include <cmath>

namespace Concord {
namespace {

/** Linear interpolation that keeps both endpoints exact. */
f32 Lerp(f32 from, f32 to, f32 weight) noexcept
{
    return from + (to - from) * weight;
}

/** Wraps a playhead into [0, 1), treating a non-finite value as the start. */
f32 WrapPhase(f32 phase) noexcept
{
    if (!std::isfinite(phase)) {
        return 0.0f;
    }
    const f32 wrapped = phase - std::floor(phase);
    return std::isfinite(wrapped) ? wrapped : 0.0f;
}

} // namespace

bool BlendSpace1D::IsValid() const noexcept
{
    if (samples.empty()) {
        return false;
    }
    for (usize index = 0; index < samples.size(); ++index) {
        if (samples[index].clip == nullptr ||
            !std::isfinite(samples[index].position) ||
            !std::isfinite(samples[index].speed) || samples[index].speed <= 0.0f) {
            return false;
        }
        // Written as a negated comparison so a NaN position also fails.
        if (index != 0 && !(samples[index - 1].position < samples[index].position)) {
            return false;
        }
    }
    return true;
}

bool BlendSpace1D::Evaluate(f32 value, usize& lower, usize& upper, f32& weight) const noexcept
{
    lower = 0;
    upper = 0;
    weight = 0.0f;
    if (!IsValid()) {
        return false;
    }
    const usize last = samples.size() - 1;
    // A NaN parameter clamps low rather than picking an arbitrary pair.
    if (!(value > samples.front().position)) {
        return true;
    }
    if (value >= samples[last].position) {
        lower = last;
        upper = last;
        return true;
    }
    for (usize index = 1; index <= last; ++index) {
        if (value < samples[index].position) {
            lower = index - 1;
            upper = index;
            const f32 span = samples[index].position - samples[index - 1].position;
            weight = span > 0.0f ? (value - samples[index - 1].position) / span : 0.0f;
            // Landing exactly on a sample needs no blend, which lets the
            // sampler skip the second clip and the pose interpolation.
            if (!(weight > 0.0f)) {
                upper = lower;
            }
            return true;
        }
    }
    lower = last;
    upper = last;
    return true;
}

bool ResetBlendSpace(const BlendSpace1D& space, BlendSpaceState& state) noexcept
{
    state.phase = 0.0f;
    return space.IsValid();
}

f32 AdvanceBlendSpacePhase(f32 phase, f32 deltaSeconds, f32 duration, f32 speed) noexcept
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f || !(duration > 0.0001f) ||
        !(speed > 0.0f)) {
        return WrapPhase(phase);
    }
    return WrapPhase(phase + deltaSeconds * speed / duration);
}

bool SampleBlendSpace1D(const BlendSpace1D& space, BlendSpaceState& state, f32 value,
                        f32 deltaSeconds, const Skeleton& skeleton, SkeletonPose& output,
                        SkeletonPose& scratch) noexcept
{
    usize lower = 0;
    usize upper = 0;
    f32 weight = 0.0f;
    if (!space.Evaluate(value, lower, upper, weight)) {
        return false;
    }
    const BlendSpaceSample& low = space.samples[lower];
    const BlendSpaceSample& high = space.samples[upper];
    const f32 duration = Lerp(low.clip->duration, high.clip->duration, weight);
    state.phase = AdvanceBlendSpacePhase(state.phase, deltaSeconds, duration,
                                         Lerp(low.speed, high.speed, weight));
    if (lower == upper) {
        return SampleAnimation(skeleton, *low.clip, state.phase * low.clip->duration, output, true);
    }
    if (!SampleAnimation(skeleton, *low.clip, state.phase * low.clip->duration, scratch, true)) {
        return false;
    }
    if (!SampleAnimation(skeleton, *high.clip, state.phase * high.clip->duration, output, true)) {
        return false;
    }
    return BlendPoses(skeleton, scratch, output, weight, output);
}

} // namespace Concord
