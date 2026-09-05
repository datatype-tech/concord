// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationStateMachineInternal.h"

#include <algorithm>
#include <cmath>

namespace Concord {

f32 AnimationSafeDelta(f32 value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

f32 AdvanceClipTime(f32 time, f32 delta, const AnimationState& state,
                    const AnimationClip& clip, bool& finished) noexcept
{
    finished = false;
    const f32 duration = std::isfinite(clip.duration) && clip.duration > 0.0f ? clip.duration : 0.0f;
    time += delta * (std::isfinite(state.speed) ? state.speed : 0.0f);
    if (!std::isfinite(time)) {
        time = state.speed < 0.0f ? 0.0f : duration;
    }
    if (state.loop && duration > 0.0f) {
        time = std::fmod(time, duration);
        return time < 0.0f ? time + duration : time;
    }
    if (duration > 0.0f) {
        if (time >= duration) {
            finished = true;
            return duration;
        }
        return std::max(time, 0.0f);
    }
    finished = true;
    return 0.0f;
}

f32 NormalizedClipTime(const AnimationState& state, const AnimationClip& clip, f32 time) noexcept
{
    if (clip.duration > 0.0f && std::isfinite(clip.duration)) {
        return std::clamp(time / clip.duration, 0.0f, 1.0f);
    }
    return 1.0f;
}

const AnimationTransition* FindExitTransition(const AnimationGraph& graph, u32 state,
                                               f32 normalizedTime) noexcept
{
    for (const AnimationTransition& transition : graph.transitions) {
        if (transition.fromState == state && transition.hasExitTime &&
            normalizedTime >= transition.exitTime) {
            return &transition;
        }
    }
    return nullptr;
}

bool StartStateTransition(const AnimationGraph& graph, u32 target,
                          AnimationStateMachineState& runtime) noexcept
{
    if (target >= graph.states.size() || target == runtime.currentState) {
        return false;
    }
    const AnimationTransition* transition = graph.FindTransition(runtime.currentState, target);
    if (!transition) {
        return false;
    }
    if (runtime.nextState != kInvalidAnimationState && !transition->canInterrupt) {
        return false;
    }
    runtime.nextState = target;
    runtime.nextTime = 0.0f;
    runtime.transitionTime = 0.0f;
    runtime.transitionDuration = std::max(transition->duration, 0.0f);
    return true;
}

} // namespace Concord
