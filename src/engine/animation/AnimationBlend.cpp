// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationBlend.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

f32 SafeWeight(f32 value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

BoneTransform BindAt(const Skeleton& skeleton, usize index) noexcept
{
    return index < skeleton.joints.size() ? skeleton.joints[index].local : BoneTransform{};
}

BoneTransform PoseAt(const Skeleton& skeleton, const SkeletonPose& pose, usize index) noexcept
{
    return index < pose.local.size() ? pose.local[index] : BindAt(skeleton, index);
}

} // namespace

BoneTransform BlendBoneTransform(const BoneTransform& from, const BoneTransform& to,
                                f32 weight) noexcept
{
    return BoneTransform::Interpolate(from, to, SafeWeight(weight));
}

bool BlendPoses(const Skeleton& skeleton, const SkeletonPose& from, const SkeletonPose& to,
                f32 weight, SkeletonPose& out) noexcept
{
    try {
        if (!skeleton.IsValid()) return false;
        const f32 amount = SafeWeight(weight);
        out.local.resize(skeleton.joints.size());
        out.jointMatrices.resize(skeleton.joints.size());
        for (usize index = 0; index < out.local.size(); ++index) {
            out.local[index] = BlendBoneTransform(PoseAt(skeleton, from, index),
                                                  PoseAt(skeleton, to, index), amount);
        }
        skeleton.BuildJointMatrices(out.local, out.jointMatrices);
        return true;
    } catch (...) {
        return false;
    }
}

bool AdditiveBlendPose(const Skeleton& skeleton, const SkeletonPose& base,
                       const SkeletonPose& additive, f32 weight, SkeletonPose& out) noexcept
{
    try {
        if (!skeleton.IsValid()) return false;
        const f32 amount = SafeWeight(weight);
        out.local.resize(skeleton.joints.size());
        out.jointMatrices.resize(skeleton.joints.size());
        for (usize index = 0; index < out.local.size(); ++index) {
            const BoneTransform bind = BindAt(skeleton, index);
            const BoneTransform baseTransform = PoseAt(skeleton, base, index);
            const BoneTransform additiveTransform = PoseAt(skeleton, additive, index);
            const Quat deltaRotation = bind.rotation.Normalized().Conjugated() *
                                        additiveTransform.rotation.Normalized();
            out.local[index] = baseTransform;
            out.local[index].translation +=
                (additiveTransform.translation - bind.translation) * amount;
            out.local[index].scale += (additiveTransform.scale - bind.scale) * amount;
            out.local[index].rotation =
                (baseTransform.rotation * Slerp(Quat::Identity(), deltaRotation, amount))
                    .Normalized();
        }
        skeleton.BuildJointMatrices(out.local, out.jointMatrices);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Concord
