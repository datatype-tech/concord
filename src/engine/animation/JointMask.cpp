// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/JointMask.h"

#include <algorithm>
#include <cmath>

namespace Concord {

void JointMask::Reset(const Skeleton& skeleton, f32 weight)
{
    const f32 safe = std::isfinite(weight) ? std::clamp(weight, 0.0f, 1.0f) : 0.0f;
    weights.assign(skeleton.joints.size(), safe);
}

f32 JointMask::Weight(u32 joint) const noexcept
{
    if (weights.empty()) return 1.0f;
    if (joint >= weights.size()) return 0.0f;
    return std::isfinite(weights[joint]) ? std::clamp(weights[joint], 0.0f, 1.0f) : 0.0f;
}

void JointMask::Set(u32 joint, f32 weight) noexcept
{
    if (joint >= weights.size()) return;
    weights[joint] = std::isfinite(weight) ? std::clamp(weight, 0.0f, 1.0f) : 0.0f;
}

JointMask MaskSubtree(const Skeleton& skeleton, u32 rootJoint, f32 weight)
{
    JointMask result;
    result.Reset(skeleton, 0.0f);
    if (rootJoint >= skeleton.joints.size()) return result;
    for (u32 joint = rootJoint; joint < skeleton.joints.size(); ++joint) {
        i32 parent = static_cast<i32>(joint);
        while (parent >= 0) {
            if (static_cast<u32>(parent) == rootJoint) {
                result.Set(joint, weight);
                break;
            }
            parent = skeleton.joints[static_cast<usize>(parent)].parent;
        }
    }
    return result;
}

JointMask MaskJoints(const Skeleton& skeleton, std::span<const std::string_view> names,
                     f32 weight)
{
    JointMask result;
    result.Reset(skeleton, 0.0f);
    for (u32 joint = 0; joint < skeleton.joints.size(); ++joint) {
        for (const std::string_view name : names) {
            if (skeleton.joints[joint].name == name) {
                result.Set(joint, weight);
                break;
            }
        }
    }
    return result;
}

} // namespace Concord
