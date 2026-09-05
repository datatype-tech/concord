// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONCONTROLLER_H
#define CONCORD_ANIMATIONCONTROLLER_H

#include "Concord/CExport.h"
#include "engine/animation/AnimationLayer.h"
#include "engine/ecs/AnimationComponents.h"
#include "engine/ecs/World.h"

#include <array>

namespace Concord {

inline constexpr u32 kMaxAnimationLayers = 4;

/** ECS component that evaluates one graph and a bounded set of animation layers. */
struct CENGINE_API AnimationControllerComponent {
    /** Immutable asset containing the selected skeleton and graph clips. */
    const ModelAsset* asset = nullptr;
    /** Skeleton index used by the base graph and every layer. */
    u32 skeletonIndex = kInvalidAnimationIndex;
    /** Base locomotion/state graph. */
    const AnimationGraph* graph = nullptr;
    /** Runtime state for the base graph. */
    AnimationStateMachineState runtime{};
    /** Optional upper-body or additive layers, evaluated in array order. */
    std::array<AnimationLayer, kMaxAnimationLayers> layers{};
};

/** Advances controller components and writes their final skinning poses. */
CENGINE_API usize UpdateAnimationControllers(World& world, f32 deltaTime) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONCONTROLLER_H
