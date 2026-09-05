// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationGraph.h"

#include "engine/asset/ModelAsset.h"

#include <cmath>

namespace Concord {

bool AnimationGraph::IsValid(const ModelAsset& asset) const noexcept
{
    if (states.empty() || initialState >= states.size()) return false;
    for (usize index = 0; index < states.size(); ++index) {
        const AnimationState& state = states[index];
        if (state.name.empty() || state.clipIndex >= asset.animations.size() ||
            !std::isfinite(state.speed)) {
            return false;
        }
        for (usize other = 0; other < index; ++other) {
            if (states[other].name == state.name) return false;
        }
    }
    for (const AnimationTransition& transition : transitions) {
        if (transition.fromState >= states.size() || transition.toState >= states.size() ||
            transition.fromState == transition.toState || !std::isfinite(transition.duration) ||
            transition.duration < 0.0f ||
            (transition.hasExitTime &&
             (!std::isfinite(transition.exitTime) || transition.exitTime < 0.0f ||
              transition.exitTime > 1.0f))) {
            return false;
        }
    }
    return true;
}

u32 AnimationGraph::FindState(std::string_view name) const noexcept
{
    for (usize index = 0; index < states.size(); ++index) {
        if (states[index].name == name) return static_cast<u32>(index);
    }
    return kInvalidAnimationState;
}

const AnimationTransition* AnimationGraph::FindTransition(u32 fromState,
                                                           u32 toState) const noexcept
{
    for (const AnimationTransition& transition : transitions) {
        if (transition.fromState == fromState && transition.toState == toState) {
            return &transition;
        }
    }
    return nullptr;
}

} // namespace Concord
