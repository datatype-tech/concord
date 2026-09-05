// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONBLEND_H
#define CONCORD_ANIMATIONBLEND_H

#include "Concord/CExport.h"
#include "engine/asset/Skeleton.h"

namespace Concord {

/** Blends two local bone transforms with shortest-path rotation interpolation. */
[[nodiscard]] CENGINE_API BoneTransform BlendBoneTransform(const BoneTransform& from,
                                                           const BoneTransform& to,
                                                           f32 weight) noexcept;

/** Blends two poses into `out`, using the skeleton bind pose for missing joints. */
[[nodiscard]] CENGINE_API bool BlendPoses(const Skeleton& skeleton,
                                          const SkeletonPose& from,
                                          const SkeletonPose& to,
                                          f32 weight,
                                          SkeletonPose& out) noexcept;

/** Applies an additive pose relative to the skeleton bind pose. */
[[nodiscard]] CENGINE_API bool AdditiveBlendPose(const Skeleton& skeleton,
                                                 const SkeletonPose& base,
                                                 const SkeletonPose& additive,
                                                 f32 weight,
                                                 SkeletonPose& out) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONBLEND_H
