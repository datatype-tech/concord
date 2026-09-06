// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanDebugOverlay.h"

#include "engine/render/vulkan/VulkanDebugOverlayInternal.h"
#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanShaderModule.h"

#include <cstddef>
#include <cstring>
#include <span>

namespace Concord {
namespace {

/** Host-visible quad budget per frame slot, generous over the worst case. */
constexpr VkDeviceSize kOverlayVertexBytes = 128 * 1024;

/** Finds a memory type usable for `typeBits` with the wanted properties. */
u32 FindMemoryType(const VulkanContext& context, u32 typeBits, VkMemoryPropertyFlags wanted)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &properties);
    for (u32 index = 0; index < properties.memoryTypeCount; ++index) {
        const bool accepted = (typeBits & (1u << index)) != 0;
        const bool fits = (properties.memoryTypes[index].propertyFlags & wanted) == wanted;
        if (accepted && fits) {
            return index;
        }
    }
    return UINT32_MAX;
}

/** Whether the device can sample and fill an R8_UNORM optimal-tiled image. */
bool R8SampledSupported(VkPhysicalDevice physicalDevice)
{
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice, VK_FORMAT_R8_UNORM, &properties);
    const VkFormatFeatureFlags required =
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    return (properties.optimalTilingFeatures & required) == required;
}

/** Allocates and binds the device-local atlas image; layout stays UNDEFINED. */
bool CreateAtlasImage(const VulkanContext& context, VulkanDebugOverlay& overlay,
                      VkExtent2D atlasExtent)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.extent = {atlasExtent.width, atlasExtent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult result = vkCreateImage(context.device, &imageInfo, nullptr, &overlay.atlasImage);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateImage(debug overlay atlas)", result);
    }
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device, overlay.atlasImage, &requirements);
    const u32 memoryType =
        FindMemoryType(context, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryType == UINT32_MAX) {
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = memoryType;
    result = vkAllocateMemory(context.device, &allocation, nullptr, &overlay.atlasMemory);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkAllocateMemory(debug overlay atlas)", result);
    }
    result = vkBindImageMemory(context.device, overlay.atlasImage, overlay.atlasMemory, 0);
    return result == VK_SUCCESS || VulkanFailed("vkBindImageMemory(debug overlay atlas)", result);
}

/** Uploads the packed texels through a transient one-shot command buffer. */
bool UploadAtlas(const VulkanContext& context, VulkanDebugOverlay& overlay,
                 const VulkanBuffer& staging)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = context.queueFamily;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        return false;
    }
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = pool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    const bool allocated =
        vkAllocateCommandBuffers(context.device, &allocateInfo, &commandBuffer) == VK_SUCCESS;
    if (!allocated) {
        vkDestroyCommandPool(context.device, pool, nullptr);
        return false;
    }
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = overlay.atlasImage;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toTransfer);
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {overlay.font.width, overlay.font.height, 1};
    vkCmdCopyBufferToImage(commandBuffer, staging.buffer, overlay.atlasImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    VkImageMemoryBarrier toShaderRead = toTransfer;
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &toShaderRead);
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(context.graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(context.graphicsQueue);
    vkDestroyCommandPool(context.device, pool, nullptr);
    return true;
}

} // namespace

bool CreateVulkanDebugOverlay(const VulkanContext& context, VkFormat colorFormat,
                              VulkanDebugOverlay& overlay)
{
    if (context.device == VK_NULL_HANDLE || colorFormat == VK_FORMAT_UNDEFINED ||
        !R8SampledSupported(context.physicalDevice)) {
        return false;
    }
    overlay.device = context.device;

    overlay.font = BakeDebugFont(kOverlayFontPixelHeight);
    const std::size_t texelCount =
        static_cast<std::size_t>(overlay.font.width) * overlay.font.height;

    VulkanBuffer staging{};
    if (!CreateVulkanHostBuffer(context, static_cast<VkDeviceSize>(texelCount),
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging)) {
        DestroyVulkanDebugOverlay(context, overlay);
        return false;
    }
    const bool uploaded =
        UploadVulkanBuffer(
            staging,
            std::as_bytes(std::span{overlay.font.texels.data(), texelCount})) &&
        CreateAtlasImage(context, overlay, {overlay.font.width, overlay.font.height}) &&
        UploadAtlas(context, overlay, staging);
    DestroyVulkanBuffer(context, staging);
    overlay.font.texels.clear();
    overlay.font.texels.shrink_to_fit();
    if (!uploaded) {
        DestroyVulkanDebugOverlay(context, overlay);
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = overlay.atlasImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VkResult result = vkCreateImageView(context.device, &viewInfo, nullptr, &overlay.atlasView);
    if (result != VK_SUCCESS) {
        DestroyVulkanDebugOverlay(context, overlay);
        return VulkanFailed("vkCreateImageView(debug overlay atlas)", result);
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    const VkFilter filter = overlay.font.smooth ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    result = vkCreateSampler(context.device, &samplerInfo, nullptr, &overlay.sampler);
    if (result != VK_SUCCESS) {
        DestroyVulkanDebugOverlay(context, overlay);
        return VulkanFailed("vkCreateSampler(debug overlay)", result);
    }

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr,
                                         &overlay.descriptorLayout);
    if (result != VK_SUCCESS) {
        DestroyVulkanDebugOverlay(context, overlay);
        return VulkanFailed("vkCreateDescriptorSetLayout(debug overlay)", result);
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    result = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &overlay.descriptorPool);
    if (result != VK_SUCCESS) {
        DestroyVulkanDebugOverlay(context, overlay);
        return VulkanFailed("vkCreateDescriptorPool(debug overlay)", result);
    }
    VkDescriptorSetAllocateInfo setDescription{};
    setDescription.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setDescription.descriptorPool = overlay.descriptorPool;
    setDescription.descriptorSetCount = 1;
    setDescription.pSetLayouts = &overlay.descriptorLayout;
    result = vkAllocateDescriptorSets(context.device, &setDescription, &overlay.descriptorSet);
    if (result != VK_SUCCESS) {
        DestroyVulkanDebugOverlay(context, overlay);
        return VulkanFailed("vkAllocateDescriptorSets(debug overlay)", result);
    }
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = overlay.sampler;
    imageInfo.imageView = overlay.atlasView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = overlay.descriptorSet;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);

    VkPushConstantRange pushRange{};
    pushRange.offset = 0;
    pushRange.size = sizeof(OverlayPushConstants);
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &overlay.descriptorLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    result = vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &overlay.layout);
    if (result != VK_SUCCESS) {
        DestroyVulkanDebugOverlay(context, overlay);
        return VulkanFailed("vkCreatePipelineLayout(debug overlay)", result);
    }

    const std::vector<u32> vertexCode = ReadVulkanShaderCode("debug_overlay.vert.spv");
    const std::vector<u32> fragmentCode = ReadVulkanShaderCode("debug_overlay.frag.spv");
    VkShaderModule vertex = CreateVulkanShaderModule(context, vertexCode);
    VkShaderModule fragment = CreateVulkanShaderModule(context, fragmentCode);
    if (vertex == VK_NULL_HANDLE || fragment == VK_NULL_HANDLE) {
        if (vertex != VK_NULL_HANDLE) {
            vkDestroyShaderModule(context.device, vertex, nullptr);
        }
        if (fragment != VK_NULL_HANDLE) {
            vkDestroyShaderModule(context.device, fragment, nullptr);
        }
        DestroyVulkanDebugOverlay(context, overlay);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex;
    stages[0].pName = "main";
    stages[1] = stages[0];
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment;

    VkVertexInputBindingDescription vertexBinding{};
    vertexBinding.binding = 0;
    vertexBinding.stride = sizeof(OverlayVertex);
    vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription vertexAttributes[2]{};
    vertexAttributes[0].location = 0;
    vertexAttributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[0].offset = offsetof(OverlayVertex, x);
    vertexAttributes[1].location = 1;
    vertexAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexAttributes[1].offset = offsetof(OverlayVertex, u);
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &vertexBinding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = vertexAttributes;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &colorFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &rendering;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &assembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depth;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = overlay.layout;
    result = vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &overlay.pipeline);
    vkDestroyShaderModule(context.device, vertex, nullptr);
    vkDestroyShaderModule(context.device, fragment, nullptr);
    if (result != VK_SUCCESS) {
        DestroyVulkanDebugOverlay(context, overlay);
        return VulkanFailed("vkCreateGraphicsPipelines(debug overlay)", result);
    }

    for (VulkanBuffer& buffer : overlay.vertices) {
        if (!CreateVulkanHostBuffer(context, kOverlayVertexBytes,
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, buffer)) {
            DestroyVulkanDebugOverlay(context, overlay);
            return false;
        }
    }
    return true;
}

} // namespace Concord
