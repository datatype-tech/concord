// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelPipelineInternal.h"

#include "engine/core/Color.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace Concord {
namespace {

f32 SafeEmissive(Vec4 value) noexcept
{
    const f32 luminance = value.x * 0.2126f + value.y * 0.7152f + value.z * 0.0722f;
    return std::isfinite(luminance) ? std::max(luminance, 0.0f) : 0.0f;
}
VulkanModelPushConstants MakePush(const RenderObjectSnapshot& object,
                                  const VulkanModelMaterial* material) noexcept
{
    VulkanModelPushConstants push{};
    push.model = object.model;
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
std::filesystem::path AssetDirectory(const ModelAsset& asset)
{
    if (asset.sourcePath.empty()) return {};
    return asset.sourcePath.has_extension() ? asset.sourcePath.parent_path() : asset.sourcePath;
}
const VulkanTexture* ResolveTexture(const VulkanTextureCache& textureCache,
                                   const VulkanModelAsset& asset,
                                   const ModelAsset& source,
                                   const VulkanModelPrimitiveRange& range)
{
    if (range.materialIndex >= asset.baseColorTextures.size()) {
        return textureCache.Fallback();
    }
    return textureCache.Find(asset.baseColorTextures[range.materialIndex], AssetDirectory(source));
}

void DrawObject(VkCommandBuffer commandBuffer, const VulkanModelPipeline& pipeline,
                const RenderObjectSnapshot& object, const VulkanModelAssetCache& cache,
                const VulkanTextureCache& textureCache)
{
    if (!object.modelAsset || object.shape != PrimitiveShape::Model ||
        object.skinningRange.jointCount != 0) return;
    const VulkanModelAsset* asset = cache.Find(object.modelAsset.get());
    if (asset == nullptr || !asset->IsReady()) return;
    const VkBuffer vertexBuffers[] = {asset->vertexBuffer.buffer};
    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, asset->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    for (const VulkanModelPrimitiveRange& range : asset->primitives) {
        if (object.modelMesh != kAllModelMeshes && range.meshIndex != object.modelMesh) continue;
        const VulkanModelMaterial* material = range.materialIndex < asset->materials.size()
                                                  ? &asset->materials[range.materialIndex]
                                                  : nullptr;
        const VulkanTexture* texture = ResolveTexture(textureCache, *asset, *object.modelAsset,
                                                      range);
        if (pipeline.textureLayout != VK_NULL_HANDLE &&
            (texture == nullptr || texture->descriptorSet == VK_NULL_HANDLE)) {
            continue;
        }
        if (pipeline.textureLayout != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.layout, 1, 1, &texture->descriptorSet, 0, nullptr);
        }
        const VulkanModelPushConstants push = MakePush(object, material);
        vkCmdPushConstants(commandBuffer, pipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(push), &push);
        if (range.indexCount != 0) {
            vkCmdDrawIndexed(commandBuffer, range.indexCount, 1, range.firstIndex, 0, 0);
        }
    }
}

} // namespace
void SetVulkanModelViewport(VkCommandBuffer commandBuffer, VkExtent2D extent)
{
    VkViewport viewport{};
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}
void RecordVulkanModelDraws(VkCommandBuffer commandBuffer,
                            const VulkanModelPipeline& pipeline,
                            const RenderSceneSnapshot& snapshot,
                            VkDescriptorSet frameDataSet,
                            const VulkanModelAssetCache& cache,
                            const VulkanTextureCache& textureCache)
{
    if (pipeline.layout == VK_NULL_HANDLE || !snapshot.hasCamera ||
        frameDataSet == VK_NULL_HANDLE) return;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0,
                            1, &frameDataSet, 0, nullptr);
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        DrawObject(commandBuffer, pipeline, object, cache, textureCache);
    }
}
void InsertVulkanModelInputBarrier(VkCommandBuffer commandBuffer,
                                   const RenderSceneSnapshot& snapshot,
                                   const VulkanModelAssetCache& cache)
{
    if (commandBuffer == VK_NULL_HANDLE) return;
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        if (object.shape != PrimitiveShape::Model || !object.modelAsset) continue;
        const VulkanModelAsset* asset = cache.Find(object.modelAsset.get());
        if (asset == nullptr || !asset->IsReady()) continue;
        VkBufferMemoryBarrier barriers[3]{};
        const VulkanBuffer* buffers[] = {&asset->vertexBuffer, &asset->indexBuffer,
                                         &asset->materialBuffer};
        for (u32 index = 0; index < 3; ++index) {
            barriers[index].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barriers[index].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            barriers[index].dstAccessMask = index == 2
                                                ? VK_ACCESS_SHADER_READ_BIT
                                                : (VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                                                   VK_ACCESS_INDEX_READ_BIT);
            barriers[index].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[index].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barriers[index].buffer = buffers[index]->buffer;
            barriers[index].size = VK_WHOLE_SIZE;
        }
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
                                 VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 3, barriers, 0, nullptr);
    }
}

} // namespace Concord
