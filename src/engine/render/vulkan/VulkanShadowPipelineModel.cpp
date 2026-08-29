// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowPipelineInternal.h"

#include "engine/render/vulkan/VulkanModelPipelineInternal.h"

namespace Concord {
namespace {

/** Returns whether a flattened primitive range can be indexed safely. */
bool ValidRange(const VulkanModelAsset& asset, const VulkanModelPrimitiveRange& range) noexcept
{
    return range.indexCount != 0 && range.firstIndex <= asset.indexCount &&
           range.indexCount <= asset.indexCount - range.firstIndex;
}

/** Records one static model instance into the directional depth target. */
void DrawModel(VkCommandBuffer commandBuffer, const VulkanShadowPipeline& pipeline,
               const RenderObjectSnapshot& object, const Mat4& lightViewProjection,
               const VulkanModelAssetCache& modelAssets) noexcept
{
    if (object.shape != PrimitiveShape::Model || !object.castShadow ||
        object.modelSkin >= 0 || !object.modelAsset) {
        return;
    }
    const VulkanModelAsset* asset = modelAssets.Find(object.modelAsset.get());
    if (asset == nullptr || !asset->IsReady()) return;
    const VkBuffer vertex = asset->vertexBuffer.buffer;
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, &offset);
    vkCmdBindIndexBuffer(commandBuffer, asset->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    for (const VulkanModelPrimitiveRange& range : asset->primitives) {
        if (object.modelMesh != kAllModelMeshes && range.meshIndex != object.modelMesh) {
            continue;
        }
        if (!ValidRange(*asset, range)) continue;
        const VulkanModelShadowPushConstants push{lightViewProjection, object.model};
        vkCmdPushConstants(commandBuffer, pipeline.modelLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(push), &push);
        vkCmdDrawIndexed(commandBuffer, range.indexCount, 1, range.firstIndex, 0, 0);
    }
}

} // namespace

void InsertVulkanModelShadowInputBarrier(VkCommandBuffer commandBuffer,
                                         const RenderSceneSnapshot& snapshot,
                                         const VulkanModelAssetCache& modelAssets) noexcept
{
    InsertVulkanModelInputBarrier(commandBuffer, snapshot, modelAssets);
}

void RecordVulkanModelShadowCasters(
    VkCommandBuffer commandBuffer, const VulkanShadowPipeline& pipeline,
    const RenderSceneSnapshot& snapshot, const Mat4& lightViewProjection,
    const VulkanModelAssetCache& modelAssets) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || !pipeline.HasModel()) return;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.modelDepth);
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        DrawModel(commandBuffer, pipeline, object, lightViewProjection, modelAssets);
    }
}

} // namespace Concord
