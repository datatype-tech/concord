// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationSampling.h"

namespace Concord {

bool SampleClipIntoPose(const Skeleton& skeleton, const AnimationClip& clip, f32 time, bool loop,
                        SkeletonPose& pose) noexcept
{
    return SampleAnimation(skeleton, clip, time, pose, loop);
}

} // namespace Concord
