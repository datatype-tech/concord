// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/asset/ModelAsset.h"

#include <limits>
#include <utility>

namespace Concord {
namespace {

/** Finds the snapshot object carrying this entity's animated pose. */
const RenderObjectSnapshot* FindPosedObject(const RenderSceneSnapshot& snapshot,
                                            Entity entity) noexcept
{
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        if (object.entity == entity && object.skinningRange.jointCount != 0) {
            return &object;
        }
    }
    return nullptr;
}

/** Finds one flattened primitive inside an asset by its import order index. */
const ModelPrimitive* FindSourcePrimitive(const ModelAsset& asset, u32 primitiveIndex) noexcept
{
    u32 index = 0;
    for (const ModelMesh& mesh : asset.meshes) {
        for (const ModelPrimitive& primitive : mesh.primitives) {
            if (index++ == primitiveIndex) {
                return &primitive;
            }
        }
    }
    return nullptr;
}

/** Whether a source already owns this entity's geometry. */
bool HasSource(const VulkanRayTracingScene& scene, Entity entity) noexcept
{
    for (const VulkanRayTracingSkinnedSource& source : scene.skinnedSources) {
        if (source.entity == entity) {
            return true;
        }
    }
    return false;
}

/**
 * Publishes one rest primitive into the packed SSBOs the closest-hit shader
 * reads, and registers the BLAS entry that will be refit every frame.
 */
bool AppendSource(VulkanRayTracingScene& scene, const RenderObjectSnapshot& object,
                  const VulkanModelAsset& gpu, const VulkanModelPrimitiveRange& range,
                  u32 primitiveIndex, const ModelPrimitive& rest)
{
    if (rest.vertices.empty() || rest.indices.size() < 3 || rest.indices.size() % 3 != 0 ||
        range.materialIndex >= gpu.materials.size() ||
        scene.modelPrimitiveInfos.size() >= kVulkanRayTracingModelMetadataCapacity ||
        scene.modelVertices.size() > std::numeric_limits<u32>::max() - rest.vertices.size() ||
        scene.modelIndices.size() > std::numeric_limits<u32>::max() - rest.indices.size()) {
        return false;
    }
    const u32 firstVertex = static_cast<u32>(scene.modelVertices.size());
    const u32 firstIndex = static_cast<u32>(scene.modelIndices.size());
    const u32 metadataIndex = static_cast<u32>(scene.modelPrimitiveInfos.size());
    try {
        for (const ModelVertex& vertex : rest.vertices) {
            scene.modelVertices.push_back({
                .position = {vertex.position.x, vertex.position.y, vertex.position.z, 1.0f},
                .normal = {vertex.normal.x, vertex.normal.y, vertex.normal.z, 0.0f},
                .texcoord = {vertex.texcoord.x, vertex.texcoord.y, 0.0f, 0.0f},
            });
        }
        for (const u32 index : rest.indices) {
            if (index >= rest.vertices.size()) {
                return false;
            }
            scene.modelIndices.push_back(index);
        }
        const VulkanModelMaterial& material = gpu.materials[range.materialIndex];
        scene.modelPrimitiveInfos.push_back({
            .firstVertex = firstVertex,
            .firstIndex = firstIndex,
            .indexCount = static_cast<u32>(rest.indices.size()),
            // The hit shader reads this as a sampler slot, not a material id.
            .materialIndex = AcquireRayTracingTextureSlot(scene.textureSlots, *object.modelAsset,
                                                          gpu, range.materialIndex),
            .baseColor = material.baseColor,
            .emissive = material.emissive,
            .surface = material.surface,
        });

        scene.modelPrimitives.emplace_back();
        VulkanRayTracingModelPrimitive& primitive = scene.modelPrimitives.back();
        primitive.source = object.modelAsset.get();
        primitive.skinned = true;
        primitive.vertexStride = static_cast<u32>(sizeof(VulkanRayTracingModelVertex));
        primitive.primitiveIndex = primitiveIndex;
        primitive.meshIndex = range.meshIndex;
        primitive.materialIndex = range.materialIndex;
        // The BLAS input addresses are already offset to this primitive, so
        // the build descriptor addresses vertices from zero.
        primitive.firstVertex = 0;
        primitive.firstIndex = 0;
        primitive.vertexCount = static_cast<u32>(rest.vertices.size());
        primitive.indexCount = static_cast<u32>(rest.indices.size());
        primitive.metadataIndex = metadataIndex;
        primitive.metadataFirstVertex = firstVertex;
        primitive.metadataFirstIndex = firstIndex;

        VulkanRayTracingSkinnedSource source;
        source.rest = rest;
        source.entity = object.entity;
        source.modelPrimitiveIndex = static_cast<u32>(scene.modelPrimitives.size() - 1);
        source.metadataIndex = metadataIndex;
        source.metadataFirstVertex = firstVertex;
        source.vertexCount = primitive.vertexCount;
        source.scratch.resize(source.vertexCount);
        scene.skinnedSources.push_back(std::move(source));
    } catch (...) {
        scene.modelPrimitives.pop_back();
        return false;
    }
    return true;
}

} // namespace

bool AppendVulkanRayTracingSkinnedSources(VulkanRayTracingScene& scene,
                                          const RenderSceneSnapshot& snapshot,
                                          const VulkanModelAssetCache& modelAssets)
{
    try {
        for (const RenderObjectSnapshot& object : snapshot.objects) {
            if (object.shape != PrimitiveShape::Model || !object.modelAsset ||
                HasSource(scene, object.entity)) {
                continue;
            }
            const RenderObjectSnapshot* posed = FindPosedObject(snapshot, object.entity);
            if (posed == nullptr) {
                continue;
            }
            const VulkanModelAsset* gpu = modelAssets.Find(object.modelAsset.get());
            if (gpu == nullptr || !gpu->IsReady()) {
                continue;
            }
            for (u32 rangeIndex = 0; rangeIndex < gpu->primitives.size(); ++rangeIndex) {
                const VulkanModelPrimitiveRange& range = gpu->primitives[rangeIndex];
                if (object.modelMesh != kAllModelMeshes && range.meshIndex != object.modelMesh) {
                    continue;
                }
                const ModelPrimitive* rest = FindSourcePrimitive(*object.modelAsset, rangeIndex);
                if (rest == nullptr || rest->vertices.size() != range.vertexCount ||
                    rest->indices.size() != range.indexCount) {
                    continue;
                }
                if (!AppendSource(scene, object, *gpu, range, rangeIndex, *rest)) {
                    return false;
                }
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace Concord
