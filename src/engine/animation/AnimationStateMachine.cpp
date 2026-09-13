// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationStateMachine.h"

#include "engine/animation/AnimationBlend.h"
#include "engine/animation/AnimationBlendSpace.h"
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
    runtime.blendRuntime = runtime.nextBlendRuntime;
    runtime.nextState = kInvalidAnimationState;
    runtime.transitionTime = 0.0f;
    runtime.transitionDuration = 0.0f;
    runtime.finished = targetFinished;
}

bool UsesBlendSpace(const AnimationGraph& graph, const AnimationState& state) noexcept
{
    return state.blendSpaceIndex < graph.blendSpaces.size();
}

f32 BlendSpaceDuration(const BlendSpace1D& space, f32 value) noexcept
{
    usize lower = 0;
    usize upper = 0;
    f32 weight = 0.0f;
    if (!space.Evaluate(value, lower, upper, weight) || space.samples.empty()) {
        return 0.0f;
    }
    const f32 from = space.samples[lower].clip != nullptr ? space.samples[lower].clip->duration
                                                          : 0.0f;
    const f32 to = space.samples[upper].clip != nullptr ? space.samples[upper].clip->duration
                                                        : from;
    return from + (to - from) * weight;
}

void AdvanceBlendState(const BlendSpace1D& space, f32 value, f32 delta, f32 speed,
                       BlendSpaceState& runtime) noexcept
{
    const f32 duration = BlendSpaceDuration(space, value);
    const f32 rate = std::isfinite(speed) ? std::max(speed, 0.0f) : 1.0f;
    runtime.phase = AdvanceBlendSpacePhase(runtime.phase, delta, duration, rate);
}

bool SampleGraphState(const ModelAsset& asset, const AnimationGraph& graph,
                      const Skeleton& skeleton, const AnimationState& state, f32 clipTime,
                      f32 blendValue, BlendSpaceState& blendRuntime, SkeletonPose& pose)
{
    if (UsesBlendSpace(graph, state)) {
        SkeletonPose scratch;
        return SampleBlendSpace1D(graph.blendSpaces[state.blendSpaceIndex], blendRuntime,
                                  blendValue, 0.0f, skeleton, pose, scratch);
    }
    if (state.clipIndex >= asset.animations.size()) {
        return false;
    }
    return SampleClipIntoPose(skeleton, asset.animations[state.clipIndex], clipTime,
                              state.loop, pose);
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
        bool currentFinished = false;
        f32 currentNormalized = 0.0f;
        if (UsesBlendSpace(graph, currentState)) {
            AdvanceBlendState(graph.blendSpaces[currentState.blendSpaceIndex],
                              runtime.blendValue, delta, currentState.speed,
                              runtime.blendRuntime);
            currentNormalized = runtime.blendRuntime.phase;
        } else {
            const AnimationClip& currentClip = asset.animations[currentState.clipIndex];
            runtime.currentTime = AdvanceClipTime(runtime.currentTime, delta, currentState,
                                                  currentClip, currentFinished);
            currentNormalized = NormalizedClipTime(currentState, currentClip, runtime.currentTime);
        }

        if (runtime.requestedState != kInvalidAnimationState) {
            const u32 requested = runtime.requestedState;
            runtime.requestedState = kInvalidAnimationState;
            StartStateTransition(graph, requested, runtime);
        }
        if (runtime.nextState == kInvalidAnimationState) {
            if (const AnimationTransition* transition =
                    FindExitTransition(graph, runtime.currentState, currentNormalized)) {
                StartStateTransition(graph, transition->toState, runtime);
            }
        }

        if (runtime.nextState == kInvalidAnimationState) {
            runtime.finished = currentFinished;
            return SampleGraphState(asset, graph, skeleton, currentState, runtime.currentTime,
                                    runtime.blendValue, runtime.blendRuntime, pose);
        }

        const AnimationState& nextState = graph.states[runtime.nextState];
        bool nextFinished = false;
        if (UsesBlendSpace(graph, nextState)) {
            AdvanceBlendState(graph.blendSpaces[nextState.blendSpaceIndex], runtime.blendValue,
                              delta, nextState.speed, runtime.nextBlendRuntime);
        } else {
            const AnimationClip& nextClip = asset.animations[nextState.clipIndex];
            runtime.nextTime = AdvanceClipTime(runtime.nextTime, delta, nextState, nextClip,
                                               nextFinished);
        }
        if (runtime.transitionDuration <= 0.0f) {
            FinishTransition(runtime, nextFinished);
            return SampleGraphState(asset, graph, skeleton, nextState, runtime.currentTime,
                                    runtime.blendValue, runtime.blendRuntime, pose);
        }

        runtime.transitionTime =
            std::min(runtime.transitionTime + delta, runtime.transitionDuration);
        SkeletonPose fromPose;
        SkeletonPose toPose;
        if (!SampleGraphState(asset, graph, skeleton, currentState, runtime.currentTime,
                              runtime.blendValue, runtime.blendRuntime, fromPose) ||
            !SampleGraphState(asset, graph, skeleton, nextState, runtime.nextTime,
                              runtime.blendValue, runtime.nextBlendRuntime, toPose)) {
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
