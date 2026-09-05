// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/AnimationSystem.h"

#include "engine/animation/AnimationController.h"
#include "engine/asset/Animation.h"
#include "engine/asset/ModelAsset.h"
#include "engine/ecs/AnimationComponents.h"
#include "engine/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Concord {
namespace {

f32 SafeDelta(f32 value) noexcept
{
    return std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0.0f;
}

bool PreparePose(const ModelAsset& asset, u32 skeletonIndex,
                 SkinningPoseComponent& pose)
{
    if (skeletonIndex >= asset.skeletons.size()) return false;
    const Skeleton& skeleton = asset.skeletons[skeletonIndex];
    if (!skeleton.IsValid()) {
        pose.local.clear();
        pose.jointMatrices.clear();
        pose.skeletonIndex = kInvalidAnimationIndex;
        pose.sourceAsset = nullptr;
        return false;
    }
    if (pose.sourceAsset != &asset || pose.skeletonIndex != skeletonIndex ||
        pose.local.size() != skeleton.joints.size() ||
        pose.jointMatrices.size() != skeleton.joints.size()) {
        SkeletonPose bind = skeleton.CreateBindPose();
        pose.local = std::move(bind.local);
        pose.jointMatrices = std::move(bind.jointMatrices);
    }
    pose.skeletonIndex = skeletonIndex;
    pose.sourceAsset = &asset;
    return true;
}

void ClearPose(SkinningPoseComponent& pose) noexcept
{
    pose.skeletonIndex = kInvalidAnimationIndex;
    pose.sourceAsset = nullptr;
    pose.local.clear();
    pose.jointMatrices.clear();
}

bool UpdateOne(AnimationComponent& animation, SkinningPoseComponent& pose,
               f32 deltaTime) noexcept
{
    try {
        if (!animation.asset || animation.skeletonIndex >= animation.asset->skeletons.size()) {
            ClearPose(pose);
            return false;
        }
        const Skeleton& skeleton = animation.asset->skeletons[animation.skeletonIndex];
        if (!PreparePose(*animation.asset, animation.skeletonIndex, pose)) return false;
        if (animation.clipIndex >= animation.asset->animations.size()) {
            SkeletonPose bind = skeleton.CreateBindPose();
            pose.local = std::move(bind.local);
            pose.jointMatrices = std::move(bind.jointMatrices);
            return false;
        }
        const AnimationClip& clip = animation.asset->animations[animation.clipIndex];
        const f32 speed = std::isfinite(animation.speed)
                              ? std::clamp(animation.speed, -32.0f, 32.0f)
                              : 0.0f;
        if (!std::isfinite(animation.time)) {
            animation.time = 0.0f;
        }
        if (!animation.loop && clip.duration > 0.0f) {
            animation.time = std::clamp(animation.time, 0.0f, clip.duration);
        }
        if (animation.playing) {
            animation.time += SafeDelta(deltaTime) * speed;
            if (!std::isfinite(animation.time)) {
                animation.time = speed < 0.0f ? 0.0f : clip.duration;
            }
        }
        if (animation.loop && clip.duration > 0.0f) {
            animation.time = std::fmod(animation.time, clip.duration);
            if (animation.time < 0.0f) animation.time += clip.duration;
        } else if (clip.duration > 0.0f) {
            if (animation.time >= clip.duration && speed >= 0.0f) {
                animation.time = clip.duration;
                animation.playing = false;
            } else if (animation.time <= 0.0f && speed <= 0.0f) {
                animation.time = 0.0f;
                animation.playing = false;
            }
        }
        SkeletonPose sampled{.local = pose.local, .jointMatrices = pose.jointMatrices};
        if (!SampleAnimation(skeleton, clip, animation.time, sampled, animation.loop)) {
            skeleton.BuildJointMatrices(pose.local, pose.jointMatrices);
            return false;
        }
        pose.local = std::move(sampled.local);
        pose.jointMatrices = std::move(sampled.jointMatrices);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

usize UpdateAnimationComponents(World& world, f32 deltaTime) noexcept
{
    try {
        usize updated = 0;
        world.Query<AnimationComponent, SkinningPoseComponent>(
            [&](Entity entity, AnimationComponent& animation, SkinningPoseComponent& pose) {
                if (world.Has<AnimationControllerComponent>(entity)) {
                    return;
                }
                if (UpdateOne(animation, pose, deltaTime)) ++updated;
            });
        updated += UpdateAnimationControllers(world, deltaTime);
        return updated;
    } catch (...) {
        return 0;
    }
}

void AnimationSystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    UpdateAnimationComponents(scene.GetWorld(), deltaTime);
}

} // namespace Concord
