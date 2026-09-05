// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"
#include "Concord/CScene.h"

#include <cmath>
#include <memory>

namespace {

std::shared_ptr<Concord::ModelAsset> MakeAsset()
{
    auto asset = std::make_shared<Concord::ModelAsset>();
    Concord::Skeleton skeleton{};
    skeleton.joints.push_back(Concord::Joint{.name = "root", .parent = -1});
    skeleton.nodeIndices = {0};
    skeleton.root = 0;
    asset->skeletons.push_back(skeleton);
    asset->nodes.push_back(Concord::ModelNode{.name = "root"});
    Concord::AnimationClip clip{};
    clip.name = "slide";
    clip.duration = 1.0f;
    Concord::AnimationChannel channel{};
    channel.sourceNode = 0;
    channel.path = Concord::AnimationPath::Translation;
    channel.vec3Keys = {
        {.time = 0.0f, .value = {0.0f, 0.0f, 0.0f}},
        {.time = 1.0f, .value = {2.0f, 0.0f, 0.0f}},
    };
    clip.channels.push_back(channel);
    asset->animations.push_back(clip);
    return asset;
}

bool TestPlayback()
{
    const auto asset = MakeAsset();
    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .asset = asset.get(), .skeletonIndex = 0, .clipIndex = 0})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();
    if (Concord::UpdateAnimationComponents(scene.GetWorld(), 0.5f) != 1) return false;
    const auto* pose = scene.GetWorld().Get<Concord::SkinningPoseComponent>(entity);
    return pose != nullptr && pose->local.size() == 1 &&
           std::fabs(pose->local[0].translation.x - 1.0f) < 0.001f &&
           pose->jointMatrices.size() == 1;
}

bool TestInvalidAssetIsSkipped()
{
    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .skeletonIndex = 0, .clipIndex = 0})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();
    return Concord::UpdateAnimationComponents(scene.GetWorld(), 0.25f) == 0 &&
           scene.GetWorld().Get<Concord::SkinningPoseComponent>(entity)->local.empty();
}

bool TestReverseNonLoopStopsAtStart()
{
    const auto asset = MakeAsset();
    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .asset = asset.get(), .skeletonIndex = 0, .clipIndex = 0,
            .time = 0.25f, .speed = -1.0f, .loop = false})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();
    if (Concord::UpdateAnimationComponents(scene.GetWorld(), 0.5f) != 1) return false;
    const auto* animation = scene.GetWorld().Get<Concord::AnimationComponent>(entity);
    return animation != nullptr && animation->time == 0.0f && !animation->playing;
}

bool TestInvalidClipResetsPoseWithinAsset()
{
    const auto asset = MakeAsset();
    asset->skeletons[0].joints[0].local.translation.x = 7.0f;
    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .asset = asset.get(), .skeletonIndex = 0, .clipIndex = 0})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();
    if (Concord::UpdateAnimationComponents(scene.GetWorld(), 0.5f) != 1) return false;
    auto* animation = scene.GetWorld().Get<Concord::AnimationComponent>(entity);
    if (animation == nullptr) return false;
    animation->clipIndex = Concord::kInvalidAnimationIndex;
    if (Concord::UpdateAnimationComponents(scene.GetWorld(), 0.0f) != 0) return false;
    const auto* pose = scene.GetWorld().Get<Concord::SkinningPoseComponent>(entity);
    return pose != nullptr && pose->sourceAsset == asset.get() && pose->local.size() == 1 &&
           std::fabs(pose->local[0].translation.x - 7.0f) < 0.001f;
}

bool TestNonFiniteInputsAreContained()
{
    const auto asset = MakeAsset();
    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .asset = asset.get(), .skeletonIndex = 0, .clipIndex = 0,
            .time = NAN, .speed = NAN})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();
    if (Concord::UpdateAnimationComponents(scene.GetWorld(), NAN) != 1) return false;
    const auto* animation = scene.GetWorld().Get<Concord::AnimationComponent>(entity);
    const auto* pose = scene.GetWorld().Get<Concord::SkinningPoseComponent>(entity);
    return animation != nullptr && pose != nullptr && std::isfinite(animation->time) &&
           animation->time == 0.0f && pose->local.size() == 1 &&
           std::fabs(pose->local[0].translation.x) < 0.001f;
}

bool TestAssetSwitchResetsPose()
{
    const auto firstAsset = MakeAsset();
    const auto secondAsset = MakeAsset();
    secondAsset->skeletons[0].joints[0].local.translation.x = 7.0f;
    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .asset = firstAsset.get(), .skeletonIndex = 0, .clipIndex = 0, .playing = false})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();
    if (Concord::UpdateAnimationComponents(scene.GetWorld(), 0.0f) != 1) return false;
    auto* animation = scene.GetWorld().Get<Concord::AnimationComponent>(entity);
    if (animation == nullptr) return false;
    animation->asset = secondAsset.get();
    animation->clipIndex = Concord::kInvalidAnimationIndex;
    if (Concord::UpdateAnimationComponents(scene.GetWorld(), 0.0f) != 0) return false;
    const auto* pose = scene.GetWorld().Get<Concord::SkinningPoseComponent>(entity);
    return pose != nullptr && pose->sourceAsset == secondAsset.get() && pose->local.size() == 1 &&
           std::fabs(pose->local[0].translation.x - 7.0f) < 0.001f;
}

} // namespace

int main()
{
    return TestPlayback() && TestInvalidAssetIsSkipped() &&
                   TestReverseNonLoopStopsAtStart() && TestInvalidClipResetsPoseWithinAsset() &&
                   TestNonFiniteInputsAreContained() && TestAssetSwitchResetsPose()
               ? 0
               : 1;
}
