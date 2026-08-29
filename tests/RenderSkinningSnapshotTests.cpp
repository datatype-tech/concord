// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CAnimation.h"
#include "Concord/CObject.h"
#include "Concord/CScene.h"
#include "engine/render/RenderSceneSnapshot.h"

#include <cmath>
#include <memory>

namespace {

std::shared_ptr<Concord::ModelAsset> MakeAsset()
{
    auto asset = std::make_shared<Concord::ModelAsset>();
    asset->materials.push_back(Concord::ModelMaterial{});
    Concord::ModelPrimitive primitive{};
    primitive.vertices = {
        Concord::ModelVertex{.position = {0.0f, 0.0f, 0.0f}},
        Concord::ModelVertex{.position = {1.0f, 0.0f, 0.0f}},
        Concord::ModelVertex{.position = {0.0f, 1.0f, 0.0f}},
    };
    primitive.indices = {0, 1, 2};
    asset->meshes.push_back(Concord::ModelMesh{.primitives = {primitive}});
    Concord::Skeleton skeleton{};
    skeleton.joints.push_back(Concord::Joint{.name = "root", .parent = -1});
    skeleton.nodeIndices = {0};
    skeleton.root = 0;
    asset->skeletons.push_back(skeleton);
    asset->nodes.push_back(Concord::ModelNode{.name = "root", .mesh = 0, .skin = 0});
    return asset;
}

} // namespace

int main()
{
    const auto asset = MakeAsset();
    Concord::Scene scene;
    const Concord::Entity entity = scene.Spawn<Concord::Object::Model>({.asset = asset})
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .asset = asset.get(), .skeletonIndex = 0, .playing = false})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{
            .skeletonIndex = 0, .jointMatrices = {Concord::Mat4::Translate({2.0f, 3.0f, 4.0f})},
            .sourceAsset = asset.get()})
        .Id();
    const Concord::RenderSceneSnapshot snapshot = Concord::ExtractRenderScene(scene, 1.0f);
    if (snapshot.objects.size() != 1 || snapshot.skinningPalette.jointMatrices.size() != 1) {
        return 1;
    }
    const auto& object = snapshot.objects.front();
    const auto& matrix = snapshot.skinningPalette.jointMatrices.front();
    const bool validPose = object.entity == entity && object.modelSkin == 0 &&
                           object.skinningRange.firstJoint == 0 &&
                   object.skinningRange.jointCount == 1 && object.skinningRange.flags == 0 &&
                   std::fabs(matrix.col[3].x - 2.0f) < 0.0001f &&
                   std::fabs(matrix.col[3].y - 3.0f) < 0.0001f;
    if (!validPose) return 1;

    auto* animation = scene.GetWorld().Get<Concord::AnimationComponent>(entity);
    auto* pose = scene.GetWorld().Get<Concord::SkinningPoseComponent>(entity);
    if (animation == nullptr || pose == nullptr) return 1;
    animation->asset = nullptr;
    const Concord::RenderSceneSnapshot stale = Concord::ExtractRenderScene(scene, 1.0f);
    return stale.skinningPalette.jointMatrices.empty() &&
                   stale.objects.front().skinningRange.jointCount == 0
               ? 0
               : 1;
}
