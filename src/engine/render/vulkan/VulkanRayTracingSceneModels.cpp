// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

namespace Concord {
namespace {

VulkanRayTracingModelPrimitive* FindPrimitive(VulkanRayTracingScene& scene,
                                               const ModelAsset* source,
                                               u32 primitiveIndex) noexcept
{
    for (VulkanRayTracingModelPrimitive& primitive : scene.modelPrimitives) {
        if (primitive.source == source && primitive.primitiveIndex == primitiveIndex) {
            return &primitive;
        }
    }
    return nullptr;
}

} // namespace

bool EnsureVulkanRayTracingModelPrimitives(
    const VulkanContext& context, VulkanRayTracingScene& scene,
    const RenderSceneSnapshot& snapshot, const VulkanModelAssetCache& modelAssets)
{
    if (context.device == VK_NULL_HANDLE || !scene.dispatch.IsReady()) return false;
    const usize primitiveStart = scene.modelPrimitives.size();
    try {
        for (const RenderObjectSnapshot& object : snapshot.objects) {
            if (object.shape != PrimitiveShape::Model || !object.modelAsset ||
                object.modelSkin >= 0 || object.skinningRange.jointCount != 0) {
                continue;
            }
            const VulkanModelAsset* gpu = modelAssets.Find(object.modelAsset.get());
            if (gpu == nullptr || !gpu->HasRayTracingGeometry()) return false;
            for (u32 rangeIndex = 0; rangeIndex < gpu->primitives.size(); ++rangeIndex) {
                const VulkanModelPrimitiveRange& range = gpu->primitives[rangeIndex];
                if (object.modelMesh != kAllModelMeshes && range.meshIndex != object.modelMesh) {
                    continue;
                }
                if (FindPrimitive(scene, object.modelAsset.get(), rangeIndex) != nullptr) {
                    continue;
                }
                if (scene.modelPrimitives.size() >= kVulkanRayTracingMaxInstances) {
                    return false;
                }
                scene.modelPrimitives.emplace_back();
                VulkanRayTracingModelPrimitive& primitive = scene.modelPrimitives.back();
                if (!AppendVulkanRayTracingModelData(
                        scene, object.modelAsset.get(), rangeIndex, *gpu, range, primitive) ||
                    !CreateVulkanRayTracingModelPrimitive(
                        context, scene, object.modelAsset.get(), rangeIndex, *gpu, range,
                        primitive)) {
                    scene.modelPrimitives.pop_back();
                    DestroyVulkanRayTracingModelPrimitives(context, scene);
                    return false;
                }
            }
        }
        if (scene.modelPrimitives.size() != primitiveStart &&
            !RebuildVulkanRayTracingModelBuffers(context, scene)) {
            DestroyVulkanRayTracingModelPrimitives(context, scene);
            return false;
        }
    } catch (...) {
        DestroyVulkanRayTracingModelPrimitives(context, scene);
        return false;
    }
    return true;
}

} // namespace Concord
