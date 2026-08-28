// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanTileLightCulling.h"

#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanPhysicalDevice.h"
#include "engine/render/vulkan/VulkanShaderModule.h"

#include <vector>

namespace Concord {
namespace {

struct TileDispatchPushConstants {
    u32 viewportWidth = 0;
    u32 viewportHeight = 0;
    u32 tileColumns = 0;
    u32 tileRows = 0;
};

static_assert(sizeof(TileDispatchPushConstants) == 16);

/** Creates the set-0 pipeline layout and compute pipeline. */
bool CreatePipeline(const VulkanContext& context, VkDescriptorSetLayout frameDataLayout,
                    const std::vector<u32>& code, VulkanTileLightCulling& culling)
{
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.size = sizeof(TileDispatchPushConstants);
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &frameDataLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &culling.layout) != VK_SUCCESS) {
        return false;
    }

    const VkShaderModule shader = CreateVulkanShaderModule(context, code);
    if (shader == VK_NULL_HANDLE) {
        return false;
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader;
    stage.pName = "main";
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stage;
    pipelineInfo.layout = culling.layout;
    const VkResult result = vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1,
                                                     &pipelineInfo, nullptr, &culling.pipeline);
    vkDestroyShaderModule(context.device, shader, nullptr);
    if (result != VK_SUCCESS) {
        VulkanFailed("vkCreateComputePipelines", result);
        return false;
    }
    return true;
}

} // namespace

bool CreateVulkanTileLightCulling(const VulkanContext& context,
                                  VkDescriptorSetLayout frameDataLayout,
                                  VulkanTileLightCulling& culling)
{
    DestroyVulkanTileLightCulling(context, culling);
    if (context.device == VK_NULL_HANDLE || frameDataLayout == VK_NULL_HANDLE ||
        !QueueFamilySupportsCompute(context.physicalDevice, context.queueFamily)) {
        return false;
    }
    const std::vector<u32> code = ReadVulkanShaderCode("tile_cull.comp.spv");
    if (code.empty() || !CreatePipeline(context, frameDataLayout, code, culling)) {
        DestroyVulkanTileLightCulling(context, culling);
        return false;
    }
    return true;
}

void DestroyVulkanTileLightCulling(const VulkanContext& context,
                                   VulkanTileLightCulling& culling) noexcept
{
    if (context.device != VK_NULL_HANDLE) {
        if (culling.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(context.device, culling.pipeline, nullptr);
        }
        if (culling.layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(context.device, culling.layout, nullptr);
        }
    }
    culling = {};
}

void RecordVulkanTileLightCulling(VkCommandBuffer commandBuffer, VkExtent2D extent,
                                  const VulkanTileLightCulling& culling,
                                  VkDescriptorSet frameDataSet)
{
    if (!culling.IsReady() || commandBuffer == VK_NULL_HANDLE || frameDataSet == VK_NULL_HANDLE ||
        extent.width == 0 || extent.height == 0) {
        return;
    }
    const u32 columns = (extent.width + kTileSizePixels - 1) / kTileSizePixels;
    const u32 rows = (extent.height + kTileSizePixels - 1) / kTileSizePixels;
    if (columns > kMaxTileColumns || rows > kMaxTileRows) {
        return;
    }
    const TileDispatchPushConstants push{extent.width, extent.height, columns, rows};
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, culling.pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, culling.layout, 0, 1,
                            &frameDataSet, 0, nullptr);
    vkCmdPushConstants(commandBuffer, culling.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(push), &push);
    vkCmdDispatch(commandBuffer, columns, rows, 1);
}

void InsertVulkanTileLightBarrier(VkCommandBuffer commandBuffer, VkBuffer tileBuffer)
{
    if (commandBuffer == VK_NULL_HANDLE || tileBuffer == VK_NULL_HANDLE) {
        return;
    }
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = tileBuffer;
    barrier.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0,
                         nullptr);
}

} // namespace Concord
