// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONGRAPH_H
#define CONCORD_ANIMATIONGRAPH_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace Concord {

struct ModelAsset;

/** One named state that samples an animation clip. */
struct AnimationState {
    std::string name;
    u32 clipIndex = 0xFFFFFFFFu;
    f32 speed = 1.0f;
    bool loop = true;
};

/** A directed state transition with optional normalized exit-time gating. */
struct AnimationTransition {
    u32 fromState = 0xFFFFFFFFu;
    u32 toState = 0xFFFFFFFFu;
    f32 duration = 0.2f;
    bool hasExitTime = false;
    f32 exitTime = 1.0f;
    bool canInterrupt = true;
};

/** Immutable state-machine description shared by controller instances. */
struct CENGINE_API AnimationGraph {
    std::vector<AnimationState> states;
    std::vector<AnimationTransition> transitions;
    u32 initialState = 0;

    /** Validates state indices, transition ranges and referenced clips. */
    [[nodiscard]] bool IsValid(const ModelAsset& asset) const noexcept;
    /** Finds a state by exact name, or returns the invalid state sentinel. */
    [[nodiscard]] u32 FindState(std::string_view name) const noexcept;
    /** Finds a directed transition, or returns nullptr when none exists. */
    [[nodiscard]] const AnimationTransition* FindTransition(u32 fromState,
                                                             u32 toState) const noexcept;
};

inline constexpr u32 kInvalidAnimationState = 0xFFFFFFFFu;

} // namespace Concord

#endif // CONCORD_ANIMATIONGRAPH_H
