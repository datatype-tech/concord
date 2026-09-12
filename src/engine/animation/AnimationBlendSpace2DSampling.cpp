// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationBlendSpace2D.h"

#include "engine/animation/AnimationBlend.h"
#include "engine/animation/AnimationBlendSpace.h"

#include <cmath>

namespace Concord {
namespace {

/** Weighted average of a per-sample scalar over the resolved contributions. */
f32 WeightedAverage(const BlendSpace2D& space, const std::array<BlendSpace2Weight, 3>& out,
                    u32 count, bool useDuration) noexcept
{
    f32 total = 0.0f;
    for (u32 index = 0; index < count; ++index) {
        const BlendSpace2Sample& sample = space.samples[out[index].sample];
        const f32 value = useDuration ? sample.clip->duration : sample.speed;
        total += out[index].weight * value;
    }
    return total;
}

/** Samples one contribution straight into a destination pose. */
bool SampleContribution(const BlendSpace2D& space, const Skeleton& skeleton, u32 sampleIndex,
                        f32 phase, SkeletonPose& destination) noexcept
{
    const AnimationClip& clip = *space.samples[sampleIndex].clip;
    return SampleAnimation(skeleton, clip, phase * clip.duration, destination, true);
}

} // namespace

bool ResetBlendSpace2D(const BlendSpace2D& space, BlendSpace2State& state) noexcept
{
    state.phase = 0.0f;
    std::array<BlendSpace2Weight, 3> out{};
    u32 count = 0;
    return space.Evaluate({0.0f, 0.0f}, out, count);
}

bool SampleBlendSpace2D(const BlendSpace2D& space, BlendSpace2State& state, Vec2 value,
                        f32 deltaSeconds, const Skeleton& skeleton, SkeletonPose& output,
                        std::span<SkeletonPose> scratch) noexcept
{
    std::array<BlendSpace2Weight, 3> out{};
    u32 count = 0;
    if (!space.Evaluate(value, out, count)) {
        return false;
    }
    if (count > 1 && scratch.size() < count - 1) {
        return false;
    }
    const f32 duration = WeightedAverage(space, out, count, true);
    const f32 speed = WeightedAverage(space, out, count, false);
    state.phase = AdvanceBlendSpacePhase(state.phase, deltaSeconds, duration, speed);

    if (!SampleContribution(space, skeleton, out[0].sample, state.phase, output)) {
        return false;
    }
    // Fold in the remaining contributions by progressive renormalisation, so
    // three clips blend in the proportion their weights asked for.
    f32 accumulated = out[0].weight;
    for (u32 index = 1; index < count; ++index) {
        if (!SampleContribution(space, skeleton, out[index].sample, state.phase,
                                scratch[index - 1])) {
            return false;
        }
        accumulated += out[index].weight;
        if (!(accumulated > 0.0f)) {
            return false;
        }
        if (!BlendPoses(skeleton, output, scratch[index - 1], out[index].weight / accumulated,
                        output)) {
            return false;
        }
    }
    return true;
}

} // namespace Concord
