// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONSTATEMACHINEINTERNAL_H
#define CONCORD_ANIMATIONSTATEMACHINEINTERNAL_H

#include "engine/animation/AnimationGraph.h"
#include "engine/animation/AnimationStateMachine.h"
#include "engine/asset/Animation.h"

namespace Concord {

/** Clamps a delta time to the finite, non-negative range the evaluator accepts. */
[[nodiscard]] f32 AnimationSafeDelta(f32 value) noexcept;

/** Advances one state's clip clock, honoring loop, speed and one-shot completion. */
f32 AdvanceClipTime(f32 time, f32 delta, const AnimationState& state,
                    const AnimationClip& clip, bool& finished) noexcept;

/** Maps a clip clock to its normalized [0, 1] progress. */
[[nodiscard]] f32 NormalizedClipTime(const AnimationState& state, const AnimationClip& clip,
                                     f32 time) noexcept;

/** Returns the first exit-time transition leaving `state` that has fired. */
[[nodiscard]] const AnimationTransition* FindExitTransition(const AnimationGraph& graph,
                                                             u32 state,
                                                             f32 normalizedTime) noexcept;

/** Begins a directed transition from the current state when the graph allows it. */
bool StartStateTransition(const AnimationGraph& graph, u32 target,
                          AnimationStateMachineState& runtime) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONSTATEMACHINEINTERNAL_H
