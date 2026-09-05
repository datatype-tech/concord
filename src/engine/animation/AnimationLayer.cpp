// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationLayer.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

f32 SafeWeight(f32 value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

BoneTransform PoseAt(const Skeleton& skeleton, const SkeletonPose& pose, usize index) noexcept
{
    return index < pose.local.size() ? pose.local[index] : skeleton.joints[index].local;
}

} // namespace

bool ApplyAnimationLayer(const Skeleton& skeleton, const AnimationLayer& layer,
                         const SkeletonPose& layerPose, SkeletonPose& accumulated) noexcept
{
    try {
        if (!skeleton.IsValid() || !layer.enabled) return true;
        const f32 layerWeight = SafeWeight(layer.weight);
        if (layerWeight <= 0.0f) return true;
        if (accumulated.local.size() != skeleton.joints.size()) {
            accumulated.Reset(skeleton);
        }
        for (usize index = 0; index < accumulated.local.size(); ++index) {
            const f32 amount = layerWeight * layer.mask.Weight(static_cast<u32>(index));
            if (amount <= 0.0f) continue;
            const BoneTransform source = PoseAt(skeleton, layerPose, index);
            if (layer.mode == AnimationBlendMode::Override) {
                accumulated.local[index] =
                    BlendBoneTransform(accumulated.local[index], source, amount);
                continue;
            }
            const BoneTransform bind = skeleton.joints[index].local;
            const Quat delta = bind.rotation.Normalized().Conjugated() * source.rotation.Normalized();
            BoneTransform& destination = accumulated.local[index];
            destination.translation += (source.translation - bind.translation) * amount;
            destination.scale += (source.scale - bind.scale) * amount;
            destination.rotation =
                (destination.rotation * Slerp(Quat::Identity(), delta, amount)).Normalized();
        }
        accumulated.jointMatrices.resize(skeleton.joints.size());
        skeleton.BuildJointMatrices(accumulated.local, accumulated.jointMatrices);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Concord
