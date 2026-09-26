// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "engine/render/vulkan/VulkanUiToolkit.h"
#include "Concord/CUiToolkit.h"
#include <backends/imgui_impl_vulkan.h>

#include <cstring>
#include <limits>

namespace Concord {
namespace {
u32 FindMemoryType(const VulkanContext& context, u32 typeBits, VkMemoryPropertyFlags required)
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice, &properties);
    for (u32 index = 0; index < properties.memoryTypeCount; ++index)
        if ((typeBits & (1u << index)) && (properties.memoryTypes[index].propertyFlags & required) == required) return index;
    return std::numeric_limits<u32>::max();
}

/** Host-visible source of one copy; destroyed as soon as the queue has drained. */
struct StagingBuffer {
    const VulkanContext& context;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    ~StagingBuffer()
    {
        if (buffer) vkDestroyBuffer(context.device, buffer, nullptr);
        if (memory) vkFreeMemory(context.device, memory, nullptr);
    }
    bool Create(const UiImagePixels& pixels)
    {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = pixels.rgba.size(); info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(context.device, &info, nullptr, &buffer) != VK_SUCCESS) return false;
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(context.device, buffer, &requirements);
        const u32 type = FindMemoryType(context, requirements.memoryTypeBits,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == std::numeric_limits<u32>::max()) return false;
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = type;
        if (vkAllocateMemory(context.device, &allocation, nullptr, &memory) != VK_SUCCESS ||
            vkBindBufferMemory(context.device, buffer, memory, 0) != VK_SUCCESS) return false;
        void* mapped = nullptr;
        if (vkMapMemory(context.device, memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) return false;
        std::memcpy(mapped, pixels.rgba.data(), pixels.rgba.size());
        vkUnmapMemory(context.device, memory);
        return true;
    }
};

bool CreateImage(const VulkanContext& context, const UiImagePixels& pixels, VulkanUiImage& image)
{
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D; info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {pixels.width, pixels.height, 1}; info.mipLevels = 1; info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT; info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE; info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(context.device, &info, nullptr, &image.image) != VK_SUCCESS) return false;
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device, image.image, &requirements);
    const u32 type = FindMemoryType(context, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (type == std::numeric_limits<u32>::max()) return false;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = type;
    if (vkAllocateMemory(context.device, &allocation, nullptr, &image.memory) != VK_SUCCESS ||
        vkBindImageMemory(context.device, image.image, image.memory, 0) != VK_SUCCESS) return false;
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = image.image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = info.format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return vkCreateImageView(context.device, &view, nullptr, &image.view) == VK_SUCCESS;
}

void RecordUpload(VkCommandBuffer command, VkBuffer source, const UiImagePixels& pixels, VkImage target)
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = target; barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {pixels.width, pixels.height, 1};
    vkCmdCopyBufferToImage(command, source, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool SubmitUpload(const VulkanContext& context, VkBuffer source, const UiImagePixels& pixels, VkImage target)
{
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; poolInfo.queueFamilyIndex = context.queueFamily;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &pool) != VK_SUCCESS) return false;
    VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = pool; allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocate.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    bool submitted = false;
    if (vkAllocateCommandBuffers(context.device, &allocate, &command) == VK_SUCCESS) {
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(command, &begin) == VK_SUCCESS) {
            RecordUpload(command, source, pixels, target);
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
            submitted = vkEndCommandBuffer(command) == VK_SUCCESS &&
                        vkQueueSubmit(context.graphicsQueue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
                        vkQueueWaitIdle(context.graphicsQueue) == VK_SUCCESS;
        }
    }
    vkDestroyCommandPool(context.device, pool, nullptr);
    return submitted;
}
}

void DestroyVulkanUiImage(const VulkanContext& context, VulkanUiImage& image)
{
    if (image.texture) ImGui_ImplVulkan_RemoveTexture(image.texture);
    if (image.view) vkDestroyImageView(context.device, image.view, nullptr);
    if (image.image) vkDestroyImage(context.device, image.image, nullptr);
    if (image.memory) vkFreeMemory(context.device, image.memory, nullptr);
    image = {};
}

bool UploadVulkanUiImages(const VulkanContext& context, VulkanUiToolkit& toolkit)
{
    auto pending = toolkit.ui->TakePendingImages();
    bool succeeded = true;
    for (const auto& pixels : pending) {
        if (pixels.rgba.size() != static_cast<usize>(pixels.width) * pixels.height * 4) { succeeded = false; continue; }
        StagingBuffer staging{context};
        VulkanUiImage image{};
        if (!staging.Create(pixels) || !CreateImage(context, pixels, image) ||
            !SubmitUpload(context, staging.buffer, pixels, image.image)) {
            DestroyVulkanUiImage(context, image);
            succeeded = false;
            continue;
        }
        image.texture = ImGui_ImplVulkan_AddTexture(toolkit.sampler, image.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (!image.texture) { DestroyVulkanUiImage(context, image); succeeded = false; continue; }
        toolkit.ui->SetImageTexture(pixels.id, reinterpret_cast<u64>(image.texture));
        toolkit.images.push_back(image);
    }
    return succeeded;
}
}
