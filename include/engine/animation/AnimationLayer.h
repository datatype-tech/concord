// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONLAYER_H
#define CONCORD_ANIMATIONLAYER_H

#include "Concord/CExport.h"
#include "engine/animation/AnimationBlend.h"
#include "engine/animation/AnimationStateMachine.h"
#include "engine/animation/JointMask.h"

namespace Concord {

/** How a layer combines its sampled pose with the accumulated pose. */
enum class AnimationBlendMode {
    Override,
    Additive,
};

/** One independently evaluated animation layer. */
struct AnimationLayer {
    const AnimationGraph* graph = nullptr;
    AnimationStateMachineState runtime{};
    JointMask mask{};
    f32 weight = 1.0f;
    AnimationBlendMode mode = AnimationBlendMode::Override;
    bool enabled = false;
};

/** Applies a layer pose to an accumulated pose using its per-joint mask. */
CENGINE_API bool ApplyAnimationLayer(const Skeleton& skeleton,
                                     const AnimationLayer& layer,
                                     const SkeletonPose& layerPose,
                                     SkeletonPose& accumulated) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONLAYER_H
