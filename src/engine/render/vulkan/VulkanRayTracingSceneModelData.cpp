// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/asset/ModelAsset.h"

#include <limits>
#include <span>

namespace Concord {
namespace {

const ModelPrimitive* FindSourcePrimitive(const ModelAsset& asset,
                                          u32 primitiveIndex) noexcept
{
    u32 index = 0;
    for (const ModelMesh& mesh : asset.meshes) {
        for (const ModelPrimitive& primitive : mesh.primitives) {
            if (index++ == primitiveIndex) return &primitive;
        }
    }
    return nullptr;
}

bool FitsU32(usize size) noexcept
{
    return size <= std::numeric_limits<u32>::max();
}

} // namespace

bool AppendVulkanRayTracingModelData(
    VulkanRayTracingScene& scene, const ModelAsset* source, u32 primitiveIndex,
    const VulkanModelAsset& gpu, const VulkanModelPrimitiveRange& range,
    VulkanRayTracingModelPrimitive& output)
{
    if (source == nullptr || range.materialIndex >= gpu.materials.size() ||
        range.indexCount < 3 || range.indexCount % 3 != 0 || range.vertexCount == 0 ||
        !FitsU32(scene.modelVertices.size()) || !FitsU32(scene.modelIndices.size()) ||
        scene.modelPrimitiveInfos.size() >= kVulkanRayTracingModelMetadataCapacity) {
        return false;
    }
    const ModelPrimitive* primitive = FindSourcePrimitive(*source, primitiveIndex);
    if (primitive == nullptr || primitive->vertices.size() != range.vertexCount ||
        primitive->indices.size() != range.indexCount) {
        return false;
    }
    const usize vertexStart = scene.modelVertices.size();
    const usize indexStart = scene.modelIndices.size();
    const usize infoStart = scene.modelPrimitiveInfos.size();
    try {
        if (primitive->vertices.size() > std::numeric_limits<usize>::max() - vertexStart ||
            primitive->indices.size() > std::numeric_limits<usize>::max() - indexStart ||
            !FitsU32(vertexStart + primitive->vertices.size()) ||
            !FitsU32(indexStart + primitive->indices.size())) {
            return false;
        }
        scene.modelVertices.reserve(vertexStart + primitive->vertices.size());
        scene.modelIndices.reserve(indexStart + primitive->indices.size());
        for (const ModelVertex& vertex : primitive->vertices) {
            scene.modelVertices.push_back({
                .position = {vertex.position.x, vertex.position.y, vertex.position.z, 1.0f},
                .normal = {vertex.normal.x, vertex.normal.y, vertex.normal.z, 0.0f},
                .texcoord = {vertex.texcoord.x, vertex.texcoord.y, 0.0f, 0.0f},
            });
        }
        for (u32 index : primitive->indices) {
            if (index >= primitive->vertices.size()) {
                scene.modelVertices.resize(vertexStart);
                scene.modelIndices.resize(indexStart);
                scene.modelPrimitiveInfos.resize(infoStart);
                return false;
            }
            scene.modelIndices.push_back(index);
        }
        const VulkanModelMaterial& material = gpu.materials[range.materialIndex];
        scene.modelPrimitiveInfos.push_back({
            .firstVertex = static_cast<u32>(vertexStart),
            .firstIndex = static_cast<u32>(indexStart),
            .indexCount = range.indexCount,
            .materialIndex = range.materialIndex,
            .baseColor = material.baseColor,
            .emissive = material.emissive,
            .surface = material.surface,
        });
    } catch (...) {
        scene.modelVertices.resize(vertexStart);
        scene.modelIndices.resize(indexStart);
        scene.modelPrimitiveInfos.resize(infoStart);
        return false;
    }
    output.metadataIndex = static_cast<u32>(infoStart);
    output.metadataFirstVertex = static_cast<u32>(vertexStart);
    output.metadataFirstIndex = static_cast<u32>(indexStart);
    return true;
}

} // namespace Concord
