// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationRetarget.h"

#include "engine/animation/AnimationRetargetInternal.h"
#include "engine/asset/ModelAsset.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Concord {
namespace {

/** Resolves the source joint a channel addresses, or kInvalidJoint. */
u32 ResolveChannelJoint(const Skeleton& skeleton, const AnimationChannel& channel) noexcept
{
    if (channel.sourceNode != kInvalidJoint) return skeleton.FindJoint(channel.sourceNode);
    return channel.joint < skeleton.joints.size() ? channel.joint : kInvalidJoint;
}

/** Normalized target joint names for the name-based fallback mapping. */
std::unordered_map<std::string, u32> TargetJointsByName(const Skeleton& skeleton)
{
    std::unordered_map<std::string, u32> byName;
    byName.reserve(skeleton.joints.size());
    for (u32 joint = 0; joint < skeleton.joints.size(); ++joint) {
        byName.emplace(NormalizeJointName(skeleton.joints[joint].name), joint);
    }
    return byName;
}

} // namespace

bool RetargetClip(const HumanoidSkeleton& source, const AnimationClip& clip,
                  const HumanoidSkeleton& target, const RetargetOptions& options,
                  RetargetResult& out)
{
    try {
        if (!source.IsValid() || !target.IsValid()) return false;
        const Skeleton& sourceSkeleton = *source.skeleton;
        const Skeleton& targetSkeleton = *target.skeleton;

        std::vector<u32> slotOfJoint(sourceSkeleton.joints.size(), kInvalidJoint);
        for (u32 slot = 0; slot < kHumanoidBoneCount; ++slot) {
            const u32 joint = source.Bone(slot);
            if (joint < slotOfJoint.size()) slotOfJoint[joint] = slot;
        }
        const std::unordered_map<std::string, u32> targetByName =
            options.mapByName ? TargetJointsByName(targetSkeleton)
                              : std::unordered_map<std::string, u32>{};

        std::vector<Mat4> sourceGlobals;
        std::vector<Mat4> targetGlobals;
        CollectBindGlobals(sourceSkeleton, sourceGlobals);
        CollectBindGlobals(targetSkeleton, targetGlobals);
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
            if (sourceJoint >= slotOfJoint.size()) continue;

            u32 targetJoint = kInvalidJoint;
            const u32 slot = slotOfJoint[sourceJoint];
            if (slot != kInvalidJoint) {
                targetJoint = target.Bone(slot);
            } else if (options.mapByName) {
                const auto it = targetByName.find(
                    NormalizeJointName(sourceSkeleton.joints[sourceJoint].name));
                if (it != targetByName.end()) targetJoint = it->second;
            }
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

usize RetargetAssetAnimations(const HumanoidSkeleton& source, const ModelAsset& sourceAsset,
                              const HumanoidSkeleton& target, ModelAsset& targetAsset,
                              const RetargetOptions& options)
{
    if (!source.IsValid() || !target.IsValid()) return 0;
    usize appended = 0;
    try {
        for (const AnimationClip& clip : sourceAsset.animations) {
            RetargetResult result;
            if (!RetargetClip(source, clip, target, options, result)) return appended;
            targetAsset.animations.push_back(std::move(result.clip));
            ++appended;
            if (result.hasRootMotion) {
                targetAsset.animations.push_back(std::move(result.rootMotion));
            }
        }
    } catch (...) {
        return appended;
    }
    return appended;
}

} // namespace Concord
