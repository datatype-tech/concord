// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationController.h"

#include "engine/animation/AnimationStateMachine.h"
#include "engine/asset/Animation.h"
#include "engine/asset/ModelAsset.h"

#include <utility>

namespace Concord {
namespace {

void ClearPose(SkinningPoseComponent& pose) noexcept
{
    pose.skeletonIndex = kInvalidAnimationIndex;
    pose.sourceAsset = nullptr;
    pose.local.clear();
    pose.jointMatrices.clear();
}

bool PreparePose(const ModelAsset& asset, u32 skeletonIndex, SkinningPoseComponent& pose)
{
    if (skeletonIndex >= asset.skeletons.size() || !asset.skeletons[skeletonIndex].IsValid()) {
        ClearPose(pose);
        return false;
    }
    const Skeleton& skeleton = asset.skeletons[skeletonIndex];
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

} // namespace

usize UpdateAnimationControllers(World& world, f32 deltaTime) noexcept
{
    usize updated = 0;
    try {
        world.Query<AnimationControllerComponent, SkinningPoseComponent>(
            [&](Entity, AnimationControllerComponent& controller, SkinningPoseComponent& pose) {
                if (!controller.asset || !controller.graph ||
                    !PreparePose(*controller.asset, controller.skeletonIndex, pose)) {
                    ClearPose(pose);
                    return;
                }
                const Skeleton& skeleton = controller.asset->skeletons[controller.skeletonIndex];
                SkeletonPose accumulated;
                if (!EvaluateAnimationStateMachine(*controller.asset, *controller.graph,
                                                   controller.skeletonIndex, deltaTime,
                                                   controller.runtime, accumulated)) {
                    return;
                }
                for (AnimationLayer& layer : controller.layers) {
                    if (!layer.enabled || !layer.graph) continue;
                    SkeletonPose layerPose;
                    if (!EvaluateAnimationStateMachine(*controller.asset, *layer.graph,
                                                       controller.skeletonIndex, deltaTime,
                                                       layer.runtime, layerPose)) {
                        continue;
                    }
                    ApplyAnimationLayer(skeleton, layer, layerPose, accumulated);
                }
                pose.local = std::move(accumulated.local);
                pose.jointMatrices = std::move(accumulated.jointMatrices);
                ++updated;
            });
    } catch (...) {
        return updated;
    }
    return updated;
}

} // namespace Concord
