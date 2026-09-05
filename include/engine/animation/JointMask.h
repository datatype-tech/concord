// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_JOINTMASK_H
#define CONCORD_JOINTMASK_H

#include "Concord/CExport.h"
#include "engine/asset/Skeleton.h"

#include <span>
#include <string_view>
#include <vector>

namespace Concord {

/** Per-joint influence weights used by animation layers. */
struct CENGINE_API JointMask {
    std::vector<f32> weights;

    /** Resizes the mask to the skeleton with a uniform default weight. */
    void Reset(const Skeleton& skeleton, f32 weight = 1.0f);
    /** Returns a clamped weight, or zero for an out-of-range joint. */
    [[nodiscard]] f32 Weight(u32 joint) const noexcept;
    /** Sets one joint's weight, ignoring an out-of-range index. */
    void Set(u32 joint, f32 weight) noexcept;
};

/** Builds a mask containing one joint and all of its descendants. */
CENGINE_API JointMask MaskSubtree(const Skeleton& skeleton, u32 rootJoint,
                                  f32 weight = 1.0f);

/** Builds a mask by matching joint names exactly. */
CENGINE_API JointMask MaskJoints(const Skeleton& skeleton,
                                std::span<const std::string_view> names,
                                f32 weight = 1.0f);

} // namespace Concord

#endif // CONCORD_JOINTMASK_H
