// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowPipelineInternal.h"

#include "engine/render/vulkan/VulkanModelAssetCache.h"
#include "engine/render/vulkan/VulkanModelPipelineInternal.h"
#include "engine/render/vulkan/VulkanSkinningResources.h"

namespace Concord {
namespace {

/** Returns whether a primitive range stays inside the uploaded index buffer. */
bool ValidRange(const VulkanModelAsset& asset,
                const VulkanModelPrimitiveRange& range) noexcept
{
    return range.vertexCount != 0 && range.firstVertex <= asset.vertexCount &&
           range.vertexCount <= asset.vertexCount - range.firstVertex &&
           range.indexCount >= 3 && (range.indexCount % 3) == 0 &&
           range.firstIndex <= asset.indexCount &&
           range.indexCount <= asset.indexCount - range.firstIndex;
}

/** Records one animated model's indexed ranges in the shadow depth target. */
void DrawSkinnedModel(VkCommandBuffer commandBuffer, const VulkanShadowPipeline& pipeline,
                      const RenderObjectSnapshot& object, const Mat4& lightViewProjection,
                      const VulkanModelAssetCache& modelAssets) noexcept
{
    if (object.shape != PrimitiveShape::Model || !object.castShadow || object.modelSkin < 0 ||
        object.skinningRange.jointCount == 0 || object.skinningRange.flags != 0 ||
        !object.modelAsset) {
        return;
    }
    const VulkanModelAsset* asset = modelAssets.Find(object.modelAsset.get());
    if (asset == nullptr || !asset->IsReady()) return;
    const VkBuffer vertex = asset->vertexBuffer.buffer;
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, &offset);
    vkCmdBindIndexBuffer(commandBuffer, asset->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    VulkanSkinnedShadowPushConstants push{};
    push.model = lightViewProjection * object.model;
    push.palette = object.skinningRange;
    vkCmdPushConstants(commandBuffer, pipeline.skinnedLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(push), &push);
    for (const VulkanModelPrimitiveRange& range : asset->primitives) {
        if (object.modelMesh != kAllModelMeshes && range.meshIndex != object.modelMesh) continue;
        if (ValidRange(*asset, range)) {
            vkCmdDrawIndexed(commandBuffer, range.indexCount, 1, range.firstIndex, 0, 0);
        }
    }
}

} // namespace

void RecordVulkanSkinnedShadowCasters(
    VkCommandBuffer commandBuffer, const VulkanShadowPipeline& pipeline,
    const RenderSceneSnapshot& snapshot, const Mat4& lightViewProjection,
    const VulkanSkinningResources& skinningResources, u32 frameIndex,
    const VulkanModelAssetCache& modelAssets) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !pipeline.HasSkinned() ||
        !skinningResources.IsReady() || frameIndex >= kMaxFramesInFlight) {
        return;
    }
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline.skinnedLayout, 0, 1,
                            &skinningResources.sets[frameIndex], 0, nullptr);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.skinnedDepth);
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        DrawSkinnedModel(commandBuffer, pipeline, object, lightViewProjection, modelAssets);
    }
}

} // namespace Concord
