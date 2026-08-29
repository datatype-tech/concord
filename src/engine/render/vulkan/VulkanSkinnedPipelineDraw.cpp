// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanSkinnedPipelineInternal.h"
#include "engine/render/vulkan/VulkanModelPipelineInternal.h"

#include "engine/core/Color.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

f32 SafeEmissive(Vec4 value) noexcept
{
    const f32 luminance = value.x * 0.2126f + value.y * 0.7152f + value.z * 0.0722f;
    return std::isfinite(luminance) ? std::max(luminance, 0.0f) : 0.0f;
}

SkinningObjectPushConstants MakePush(const RenderObjectSnapshot& object,
                                     const VulkanModelMaterial* material) noexcept
{
    SkinningObjectPushConstants push{};
    push.model = object.model;
    push.palette = object.skinningRange;
    if (material != nullptr) {
        push.albedo = material->baseColor;
        push.material = {material->surface.x, material->surface.y,
                         SafeEmissive(material->emissive), 0.0f};
    } else {
        const Vec3 albedo = ToLinear(object.material.albedo);
        push.albedo = {albedo.x, albedo.y, albedo.z,
                       static_cast<f32>(ColorA(object.material.albedo)) / 255.0f};
        push.material = {object.material.metallic, object.material.roughness,
                         std::max(object.material.emissive, 0.0f), 0.0f};
    }
    return push;
}

void DrawObject(VkCommandBuffer commandBuffer, const VulkanSkinnedPipeline& pipeline,
                const RenderObjectSnapshot& object, const VulkanModelAssetCache& cache)
{
    if (object.shape != PrimitiveShape::Model || !object.modelAsset ||
        object.skinningRange.jointCount == 0) return;
    const VulkanModelAsset* asset = cache.Find(object.modelAsset.get());
    if (asset == nullptr || !asset->IsReady()) return;
    const VkBuffer vertex = asset->vertexBuffer.buffer;
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex, &offset);
    vkCmdBindIndexBuffer(commandBuffer, asset->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    for (const VulkanModelPrimitiveRange& range : asset->primitives) {
        if (object.modelMesh != kAllModelMeshes && range.meshIndex != object.modelMesh) continue;
        const VulkanModelMaterial* material = range.materialIndex < asset->materials.size()
                                                  ? &asset->materials[range.materialIndex]
                                                  : nullptr;
        const SkinningObjectPushConstants push = MakePush(object, material);
        vkCmdPushConstants(commandBuffer, pipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(push), &push);
        if (range.indexCount != 0) vkCmdDrawIndexed(commandBuffer, range.indexCount, 1,
                                                    range.firstIndex, 0, 0);
    }
}

} // namespace

void SetVulkanSkinnedViewport(VkCommandBuffer commandBuffer, VkExtent2D extent)
{
    VkViewport viewport{};
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void RecordVulkanSkinnedDraws(VkCommandBuffer commandBuffer,
                              const VulkanSkinnedPipeline& pipeline,
                              const RenderSceneSnapshot& snapshot,
                              VkDescriptorSet frameDataSet,
                              const VulkanSkinningResources& resources, u32 frameIndex,
                              const VulkanModelAssetCache& cache)
{
    if (pipeline.layout == VK_NULL_HANDLE || !snapshot.hasCamera ||
        frameDataSet == VK_NULL_HANDLE || !BindVulkanSkinningPalette(
            commandBuffer, pipeline.layout, resources, frameIndex)) return;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0,
                            1, &frameDataSet, 0, nullptr);
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        DrawObject(commandBuffer, pipeline, object, cache);
    }
}

void InsertVulkanSkinnedInputBarrier(VkCommandBuffer commandBuffer,
                                      const RenderSceneSnapshot& snapshot,
                                      const VulkanSkinningResources& resources,
                                      u32 frameIndex, const VulkanModelAssetCache& cache)
{
    InsertVulkanModelInputBarrier(commandBuffer, snapshot, cache);
    InsertVulkanSkinningPaletteBarrier(commandBuffer, resources, frameIndex);
}

} // namespace Concord
