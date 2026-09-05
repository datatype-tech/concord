// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONSTATEMACHINE_H
#define CONCORD_ANIMATIONSTATEMACHINE_H

#include "Concord/CExport.h"
#include "engine/animation/AnimationGraph.h"
#include "engine/asset/Skeleton.h"

#include <string_view>

namespace Concord {

/** Mutable playback state for one AnimationGraph instance. */
struct AnimationStateMachineState {
    u32 currentState = kInvalidAnimationState;
    u32 nextState = kInvalidAnimationState;
    u32 requestedState = kInvalidAnimationState;
    f32 currentTime = 0.0f;
    f32 nextTime = 0.0f;
    f32 transitionTime = 0.0f;
    f32 transitionDuration = 0.0f;
    bool started = false;
    bool finished = false;
};

/** Resets a graph instance to its unstarted state. */
CENGINE_API void ResetAnimationState(AnimationStateMachineState& state) noexcept;

/** Requests a named state transition on the next graph evaluation. */
CENGINE_API bool RequestAnimationTransition(const AnimationGraph& graph,
                                            u32 state,
                                            AnimationStateMachineState& runtime) noexcept;

/** Requests a named state transition on the next graph evaluation. */
CENGINE_API bool RequestAnimationTransition(const AnimationGraph& graph,
                                            std::string_view state,
                                            AnimationStateMachineState& runtime) noexcept;

/** Evaluates a graph instance, including crossfade and exit-time behavior. */
CENGINE_API bool EvaluateAnimationStateMachine(const ModelAsset& asset,
                                              const AnimationGraph& graph,
                                              u32 skeletonIndex,
                                              f32 deltaTime,
                                              AnimationStateMachineState& runtime,
                                              SkeletonPose& pose) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONSTATEMACHINE_H
