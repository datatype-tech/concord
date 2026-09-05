// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationStateMachine.h"

#include "engine/animation/AnimationBlend.h"
#include "engine/animation/AnimationSampling.h"
#include "engine/animation/AnimationStateMachineInternal.h"
#include "engine/asset/ModelAsset.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Completes an in-flight transition, promoting the target state to current. */
void FinishTransition(AnimationStateMachineState& runtime, bool targetFinished) noexcept
{
    runtime.currentState = runtime.nextState;
    runtime.currentTime = runtime.nextTime;
    runtime.nextState = kInvalidAnimationState;
    runtime.transitionTime = 0.0f;
    runtime.transitionDuration = 0.0f;
    runtime.finished = targetFinished;
}

} // namespace

void ResetAnimationState(AnimationStateMachineState& state) noexcept
{
    state = AnimationStateMachineState{};
    state.currentState = kInvalidAnimationState;
    state.nextState = kInvalidAnimationState;
    state.requestedState = kInvalidAnimationState;
}

bool RequestAnimationTransition(const AnimationGraph& graph, u32 state,
                                AnimationStateMachineState& runtime) noexcept
{
    if (state >= graph.states.size()) return false;
    runtime.requestedState = state;
    return true;
}

bool RequestAnimationTransition(const AnimationGraph& graph, std::string_view state,
                                AnimationStateMachineState& runtime) noexcept
{
    return RequestAnimationTransition(graph, graph.FindState(state), runtime);
}

bool EvaluateAnimationStateMachine(const ModelAsset& asset, const AnimationGraph& graph,
                                   u32 skeletonIndex, f32 deltaTime,
                                   AnimationStateMachineState& runtime,
                                   SkeletonPose& pose) noexcept
{
    try {
        if (!graph.IsValid(asset) || skeletonIndex >= asset.skeletons.size() ||
            !asset.skeletons[skeletonIndex].IsValid()) {
            return false;
        }
        const Skeleton& skeleton = asset.skeletons[skeletonIndex];

        if (!runtime.started || runtime.currentState >= graph.states.size()) {
            ResetAnimationState(runtime);
            runtime.currentState = graph.initialState;
            runtime.started = true;
        }

        const f32 delta = AnimationSafeDelta(deltaTime);
        const AnimationState& currentState = graph.states[runtime.currentState];
        const AnimationClip& currentClip = asset.animations[currentState.clipIndex];
        bool currentFinished = false;
        runtime.currentTime = AdvanceClipTime(runtime.currentTime, delta, currentState,
                                              currentClip, currentFinished);

        if (runtime.requestedState != kInvalidAnimationState) {
            const u32 requested = runtime.requestedState;
            runtime.requestedState = kInvalidAnimationState;
            StartStateTransition(graph, requested, runtime);
        }
        if (runtime.nextState == kInvalidAnimationState && currentClip.duration > 0.0f) {
            const f32 normalized = NormalizedClipTime(currentState, currentClip,
                                                      runtime.currentTime);
            if (const AnimationTransition* transition =
                    FindExitTransition(graph, runtime.currentState, normalized)) {
                StartStateTransition(graph, transition->toState, runtime);
            }
        }

        if (runtime.nextState == kInvalidAnimationState) {
            runtime.finished = currentFinished;
            return SampleClipIntoPose(skeleton, currentClip, runtime.currentTime,
                                      currentState.loop, pose);
        }

        const AnimationState& nextState = graph.states[runtime.nextState];
        const AnimationClip& nextClip = asset.animations[nextState.clipIndex];
        bool nextFinished = false;
        runtime.nextTime = AdvanceClipTime(runtime.nextTime, delta, nextState, nextClip,
                                           nextFinished);
        if (runtime.transitionDuration <= 0.0f) {
            FinishTransition(runtime, nextFinished);
            return SampleClipIntoPose(skeleton, nextClip, runtime.currentTime, nextState.loop,
                                      pose);
        }

        runtime.transitionTime =
            std::min(runtime.transitionTime + delta, runtime.transitionDuration);
        SkeletonPose fromPose;
        SkeletonPose toPose;
        if (!SampleClipIntoPose(skeleton, currentClip, runtime.currentTime, currentState.loop,
                                fromPose) ||
            !SampleClipIntoPose(skeleton, nextClip, runtime.nextTime, nextState.loop, toPose)) {
            return false;
        }
        if (!BlendPoses(skeleton, fromPose, toPose,
                        runtime.transitionTime / runtime.transitionDuration, pose)) {
            return false;
        }
        if (runtime.transitionTime >= runtime.transitionDuration) {
            FinishTransition(runtime, nextFinished);
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Concord
