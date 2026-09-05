// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ANIMATIONSAMPLING_H
#define CONCORD_ANIMATIONSAMPLING_H

#include "Concord/CExport.h"
#include "engine/asset/Animation.h"

namespace Concord {

/** Samples one clip into a local pose and rebuilds its skin matrices. */
[[nodiscard]] CENGINE_API bool SampleClipIntoPose(const Skeleton& skeleton,
                                                  const AnimationClip& clip,
                                                  f32 time,
                                                  bool loop,
                                                  SkeletonPose& pose) noexcept;

} // namespace Concord

#endif // CONCORD_ANIMATIONSAMPLING_H
