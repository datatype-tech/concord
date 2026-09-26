// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "engine/render/vulkan/VulkanUiToolkit.h"
#include "Concord/CUiToolkit.h"
#include <backends/imgui_impl_vulkan.h>
#include <limits>

namespace Concord {
void DestroyVulkanUiViewport(const VulkanContext& context, VulkanUiViewport& viewport)
{
    if (viewport.texture) ImGui_ImplVulkan_RemoveTexture(viewport.texture);
    DestroyVulkanDepthBuffer(context, viewport.depth);
    if (viewport.view) vkDestroyImageView(context.device, viewport.view, nullptr);
    if (viewport.image) vkDestroyImage(context.device, viewport.image, nullptr);
    if (viewport.memory) vkFreeMemory(context.device, viewport.memory, nullptr);
    viewport = {};
}
bool EnsureVulkanUiViewport(const VulkanContext& context, VulkanUiToolkit& toolkit, u32 slot)
{
    auto& viewport = toolkit.viewports[slot];
    const VkExtent2D extent{toolkit.ui->ViewportWidth(), toolkit.ui->ViewportHeight()};
    if (viewport.texture && viewport.extent.width == extent.width && viewport.extent.height == extent.height) return true;
    // The owning frame fence has completed. No device-wide idle is needed on resize.
    DestroyVulkanUiViewport(context, viewport);
    viewport.extent = extent;
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image.imageType = VK_IMAGE_TYPE_2D; image.format = toolkit.format;
    image.extent = {extent.width,extent.height,1}; image.mipLevels = 1; image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT; image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (vkCreateImage(context.device,&image,nullptr,&viewport.image) != VK_SUCCESS) return false;
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context.device,viewport.image,&requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(context.physicalDevice,&properties);
    u32 memoryType = std::numeric_limits<u32>::max();
    for (u32 i=0;i<properties.memoryTypeCount;++i) {
        if ((requirements.memoryTypeBits & (1u<<i)) &&
            (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) { memoryType=i; break; }
    }
    if (memoryType == std::numeric_limits<u32>::max()) return false;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = memoryType;
    if (vkAllocateMemory(context.device,&allocation,nullptr,&viewport.memory) != VK_SUCCESS ||
        vkBindImageMemory(context.device,viewport.image,viewport.memory,0) != VK_SUCCESS) return false;
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image=viewport.image; view.viewType=VK_IMAGE_VIEW_TYPE_2D; view.format=toolkit.format;
    view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    if (vkCreateImageView(context.device,&view,nullptr,&viewport.view) != VK_SUCCESS ||
        !CreateVulkanDepthBuffer(context,viewport.depth,extent)) return false;
    viewport.texture = ImGui_ImplVulkan_AddTexture(toolkit.sampler,viewport.view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    return viewport.texture != VK_NULL_HANDLE;
}
}
