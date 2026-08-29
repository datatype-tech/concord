// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONCOMPONENTS_H
#define CONCORD_ANIMATIONCOMPONENTS_H

#include "engine/asset/Skeleton.h"
#include "engine/core/Mat4.h"
#include "engine/core/Types.h"

#include <vector>

namespace Concord {

struct ModelAsset;

/** Sentinel used when an animation asset index has not been resolved. */
inline constexpr u32 kInvalidAnimationIndex = 0xFFFFFFFFu;

/** Selects animation data through a non-owning immutable asset reference. */
struct AnimationComponent {
    /** The asset must outlive this component and remain immutable while used. */
    const ModelAsset* asset = nullptr;
    /** Index of the skeleton in `asset->skeletons`. */
    u32 skeletonIndex = kInvalidAnimationIndex;
    /** Index of the clip in `asset->animations`. */
    u32 clipIndex = kInvalidAnimationIndex;
    /** Current clip position in seconds. */
    f32 time = 0.0f;
    /** Playback multiplier; negative values play backwards. */
    f32 speed = 1.0f;
    /** Wraps time at the clip duration when true. */
    bool loop = true;
    /** Advances time during `AnimationSystem` updates when true. */
    bool playing = true;
};

/** CPU pose cache consumed by palette upload and retained by an entity. */
struct SkinningPoseComponent {
    /** Must match the skeleton selected by AnimationComponent. */
    u32 skeletonIndex = kInvalidAnimationIndex;
    /** Local transforms in the selected skeleton's joint order. */
    std::vector<BoneTransform> local;
    /** Derived global-times-inverse-bind matrices in the same order. */
    std::vector<Mat4> jointMatrices;
    /** Non-owning identity of the asset that produced the cached pose; required for rendering. */
    const ModelAsset* sourceAsset = nullptr;
};

} // namespace Concord

#endif // CONCORD_ANIMATIONCOMPONENTS_H
