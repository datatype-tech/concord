// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderModelSnapshot.h"

#include <cmath>
#include <vector>

namespace Concord {
namespace {

bool Finite(const Mat4& matrix) noexcept
{
    for (const Vec4& column : matrix.col) {
        for (u32 row = 0; row < 4; ++row) {
            if (!std::isfinite(column[row])) return false;
        }
    }
    return true;
}

Mat4 NodeModel(const ModelAsset& asset, u32 index, std::vector<Mat4>& cache,
               std::vector<u8>& state) noexcept
{
    if (index >= asset.nodes.size() || state[index] == 1) return Mat4::Identity();
    if (state[index] == 2) return cache[index];
    state[index] = 1;
    const ModelNode& node = asset.nodes[index];
    const Mat4 parent = node.parent >= 0 && static_cast<usize>(node.parent) < asset.nodes.size()
                            ? NodeModel(asset, static_cast<u32>(node.parent), cache, state)
                            : Mat4::Identity();
    const Mat4 result = parent * node.local.ToMatrix();
    cache[index] = Finite(result) ? result : Mat4::Identity();
    state[index] = 2;
    return cache[index];
}

void AppendOne(RenderSceneSnapshot& snapshot, Entity entity,
               const ModelRenderer& model, Mat4 instanceModel, u32 mesh,
               u32 node, i32 skin)
{
    snapshot.objects.push_back(RenderObjectSnapshot{
        .entity = entity, .model = instanceModel, .shape = PrimitiveShape::Model,
        .size = {1.0f, 1.0f, 1.0f}, .material = {}, .castShadow = model.castShadow,
        .modelAsset = model.asset, .modelMesh = mesh, .modelNode = node, .modelSkin = skin,
    });
}

} // namespace

void AppendModelSnapshots(RenderSceneSnapshot& snapshot, Entity entity,
                          const ModelRenderer& model, Mat4 instanceModel)
{
    if (!model.asset) return;
    const ModelAsset& asset = *model.asset;
    std::vector<Mat4> cache(asset.nodes.size(), Mat4::Identity());
    std::vector<u8> state(asset.nodes.size(), 0);
    bool appended = false;
    for (u32 nodeIndex = 0; nodeIndex < asset.nodes.size(); ++nodeIndex) {
        const ModelNode& node = asset.nodes[nodeIndex];
        if (node.mesh < 0 || static_cast<usize>(node.mesh) >= asset.meshes.size() ||
            (model.meshIndex != kAllModelMeshes && model.meshIndex != static_cast<u32>(node.mesh))) {
            continue;
        }
        const Mat4 nodeModel = instanceModel * NodeModel(asset, nodeIndex, cache, state);
        AppendOne(snapshot, entity, model, nodeModel,
                  static_cast<u32>(node.mesh), nodeIndex, node.skin);
        appended = true;
    }
    if (appended) return;
    if (model.meshIndex == kAllModelMeshes || model.meshIndex < asset.meshes.size()) {
        AppendOne(snapshot, entity, model, instanceModel, model.meshIndex,
                  kInvalidRenderNode, -1);
    }
}

} // namespace Concord
