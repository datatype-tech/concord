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

/** Rig-level data prepared once and shared by every clip of a batch. */
struct RetargetContext {
    const HumanoidSkeleton* target = nullptr;
    std::vector<u32> slotOfJoint;
    std::unordered_map<std::string, u32> targetByName;
    f32 motionScale = 1.0f;
    u32 targetHips = kInvalidJoint;
    bool mapByName = true;
};

RetargetContext PrepareRetarget(const HumanoidSkeleton& source,
                                const HumanoidSkeleton& target,
                                const RetargetOptions& options)
{
    RetargetContext context;
    context.target = &target;
    context.mapByName = options.mapByName;
    context.slotOfJoint.assign(source.skeleton->joints.size(), kInvalidJoint);
    for (u32 slot = 0; slot < kHumanoidBoneCount; ++slot) {
        const u32 joint = source.Bone(slot);
        if (joint < context.slotOfJoint.size()) context.slotOfJoint[joint] = slot;
    }
    context.targetByName =
        options.mapByName ? TargetJointsByName(*target.skeleton)
                          : std::unordered_map<std::string, u32>{};
    std::vector<Mat4> sourceGlobals;
    std::vector<Mat4> targetGlobals;
    CollectBindGlobals(*source.skeleton, sourceGlobals);
    CollectBindGlobals(*target.skeleton, targetGlobals);
    context.motionScale = ComputeMotionScale(source, target, sourceGlobals, targetGlobals);
    context.targetHips = target.Bone(HumanoidBone::Hips);
    return context;
}

bool RetargetClipPrepared(const RetargetContext& context, const Skeleton& sourceSkeleton,
                          const AnimationClip& clip, const RetargetOptions& options,
                          RetargetResult& out)
{
    const bool stripHorizontal = options.rootMotion != RootMotionMode::KeepBaked;
    const bool extractRoot = options.rootMotion == RootMotionMode::ExtractRootMotion;

    out = RetargetResult{};
    out.clip.name = clip.name;
    out.clip.duration = clip.duration;
    out.rootMotion.name = clip.name + "_RootMotion";
    out.rootMotion.duration = clip.duration;

    for (const AnimationChannel& channel : clip.channels) {
        const u32 sourceJoint = ResolveChannelJoint(sourceSkeleton, channel);
        if (sourceJoint >= context.slotOfJoint.size()) continue;

        u32 targetJoint = kInvalidJoint;
        const u32 slot = context.slotOfJoint[sourceJoint];
        if (slot != kInvalidJoint) {
            targetJoint = context.target->Bone(slot);
        } else if (context.mapByName) {
            const auto it = context.targetByName.find(
                NormalizeJointName(sourceSkeleton.joints[sourceJoint].name));
            if (it != context.targetByName.end()) targetJoint = it->second;
        }
        if (targetJoint == kInvalidJoint) continue;

        AnimationChannel retargeted = channel;
        retargeted.joint = targetJoint;
        retargeted.sourceNode = kInvalidJoint;
        const bool isHips = targetJoint == context.targetHips;
        if (isHips && channel.path == AnimationPath::Translation) {
            ScaleTranslationChannel(retargeted, context.motionScale);
            if (stripHorizontal) StripHorizontalTranslation(retargeted);
            if (extractRoot) {
                AnimationChannel root = channel;
                root.joint = targetJoint;
                root.sourceNode = kInvalidJoint;
                KeepHorizontalTranslation(root, context.motionScale);
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
}

} // namespace

bool RetargetClip(const HumanoidSkeleton& source, const AnimationClip& clip,
                  const HumanoidSkeleton& target, const RetargetOptions& options,
                  RetargetResult& out)
{
    try {
        if (!source.IsValid() || !target.IsValid()) return false;
        const RetargetContext context = PrepareRetarget(source, target, options);
        return RetargetClipPrepared(context, *source.skeleton, clip, options, out);
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
        const RetargetContext context = PrepareRetarget(source, target, options);
        for (const AnimationClip& clip : sourceAsset.animations) {
            RetargetResult result;
            if (!RetargetClipPrepared(context, *source.skeleton, clip, options, result)) {
                return appended;
            }
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
