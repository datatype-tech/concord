// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationRetarget.h"

#include "engine/animation/AnimationRetargetInternal.h"

#include <cmath>
#include <vector>

namespace Concord {
namespace {

void ScaleTranslationChannel(AnimationChannel& channel, f32 scale) noexcept
{
    for (AnimationVec3Key& key : channel.vec3Keys) {
        key.value *= scale;
        key.inTangent *= scale;
        key.outTangent *= scale;
    }
}

void StripHorizontalTranslation(AnimationChannel& channel) noexcept
{
    for (AnimationVec3Key& key : channel.vec3Keys) {
        key.value.x = 0.0f;
        key.value.z = 0.0f;
        key.inTangent.x = 0.0f;
        key.inTangent.z = 0.0f;
        key.outTangent.x = 0.0f;
        key.outTangent.z = 0.0f;
    }
}

void KeepHorizontalTranslation(AnimationChannel& channel, f32 scale) noexcept
{
    for (AnimationVec3Key& key : channel.vec3Keys) {
        key.value = {key.value.x * scale, 0.0f, key.value.z * scale};
        key.inTangent = {key.inTangent.x * scale, 0.0f, key.inTangent.z * scale};
        key.outTangent = {key.outTangent.x * scale, 0.0f, key.outTangent.z * scale};
    }
}

/** Resolves the source joint a channel addresses, or kInvalidJoint. */
u32 ResolveChannelJoint(const Skeleton& skeleton, const AnimationChannel& channel) noexcept
{
    if (channel.sourceNode != kInvalidJoint) return skeleton.FindJoint(channel.sourceNode);
    return channel.joint < skeleton.joints.size() ? channel.joint : kInvalidJoint;
}

} // namespace

bool RetargetClip(const HumanoidSkeleton& source, const AnimationClip& clip,
                  const HumanoidSkeleton& target, const RetargetOptions& options,
                  RetargetResult& out)
{
    try {
        if (!source.IsValid() || !target.IsValid()) return false;
        const Skeleton& sourceSkeleton = *source.skeleton;

        std::vector<u32> slotOfJoint(sourceSkeleton.joints.size(), kInvalidJoint);
        for (u32 slot = 0; slot < kHumanoidBoneCount; ++slot) {
            const u32 joint = source.Bone(slot);
            if (joint < slotOfJoint.size()) slotOfJoint[joint] = slot;
        }

        std::vector<Mat4> sourceGlobals;
        std::vector<Mat4> targetGlobals;
        CollectBindGlobals(sourceSkeleton, sourceGlobals);
        CollectBindGlobals(*target.skeleton, targetGlobals);
        const f32 motionScale = ComputeMotionScale(source, target, sourceGlobals, targetGlobals);
        const bool stripHorizontal = options.rootMotion != RootMotionMode::KeepBaked;
        const bool extractRoot = options.rootMotion == RootMotionMode::ExtractRootMotion;

        out = RetargetResult{};
        out.clip.name = clip.name;
        out.clip.duration = clip.duration;
        out.rootMotion.name = clip.name + "_RootMotion";
        out.rootMotion.duration = clip.duration;

        for (const AnimationChannel& channel : clip.channels) {
            const u32 sourceJoint = ResolveChannelJoint(sourceSkeleton, channel);
            if (sourceJoint >= slotOfJoint.size() || slotOfJoint[sourceJoint] == kInvalidJoint) {
                continue;
            }
            const u32 slot = slotOfJoint[sourceJoint];
            const u32 targetJoint = target.Bone(slot);
            if (targetJoint == kInvalidJoint) continue;

            AnimationChannel retargeted = channel;
            retargeted.joint = targetJoint;
            retargeted.sourceNode = kInvalidJoint;
            const bool isHips = slot == static_cast<u32>(HumanoidBone::Hips);
            if (isHips && channel.path == AnimationPath::Translation) {
                ScaleTranslationChannel(retargeted, motionScale);
                if (stripHorizontal) StripHorizontalTranslation(retargeted);
                if (extractRoot) {
                    AnimationChannel root = channel;
                    root.joint = targetJoint;
                    root.sourceNode = kInvalidJoint;
                    KeepHorizontalTranslation(root, motionScale);
                    out.rootMotion.channels.push_back(std::move(root));
                    out.hasRootMotion = true;
                }
            }
            if (extractRoot && isHips && channel.path == AnimationPath::Rotation) {
                AnimationChannel rootRotation = channel;
                rootRotation.joint = targetJoint;
                rootRotation.sourceNode = kInvalidJoint;
                out.rootMotion.channels.push_back(std::move(rootRotation));
            }
            out.clip.channels.push_back(std::move(retargeted));
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace Concord
