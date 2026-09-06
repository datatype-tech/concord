// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONRETARGETINTERNAL_H
#define CONCORD_ANIMATIONRETARGETINTERNAL_H

#include "engine/animation/AnimationRetarget.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Concord {

/** Bind-pose global matrices per joint, mirroring BuildJointMatrices' walk. */
void CollectBindGlobals(const Skeleton& skeleton, std::vector<Mat4>& globals);

/** Averages the target-over-source ratios of the retarget reference chains. */
[[nodiscard]] f32 ComputeMotionScale(const HumanoidSkeleton& source,
                                     const HumanoidSkeleton& target,
                                     const std::vector<Mat4>& sourceGlobals,
                                     const std::vector<Mat4>& targetGlobals);

/** Scales a translation channel's keys and tangents by a uniform factor. */
void ScaleTranslationChannel(AnimationChannel& channel, f32 scale) noexcept;

/** Zeroes the horizontal components of a translation channel. */
void StripHorizontalTranslation(AnimationChannel& channel) noexcept;

/** Keeps only the scaled horizontal components of a translation channel. */
void KeepHorizontalTranslation(AnimationChannel& channel, f32 scale) noexcept;

/** Resolves the source joint a channel addresses, or kInvalidJoint. */
[[nodiscard]] u32 ResolveChannelJoint(const Skeleton& skeleton,
                                      const AnimationChannel& channel) noexcept;

/** Normalized target joint names for the name-based fallback mapping. */
[[nodiscard]] std::unordered_map<std::string, u32> TargetJointsByName(
    const Skeleton& skeleton);

} // namespace Concord

#endif // CONCORD_ANIMATIONRETARGETINTERNAL_H
