// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "engine/render/vulkan/VulkanUiToolkit.h"
#include "Concord/CUiToolkit.h"
#include <backends/imgui_impl_vulkan.h>

#include <utility>

namespace Concord {
bool CreateVulkanUiToolkit(const VulkanContext& context, VkFormat format, UiToolkit& ui, VulkanUiToolkit& toolkit)
{
    ui.Activate(); toolkit.ui=&ui; toolkit.format=format;
    ImGui_ImplVulkan_InitInfo init{};
    init.ApiVersion=VK_API_VERSION_1_3; init.Instance=context.instance;
    init.PhysicalDevice=context.physicalDevice; init.Device=context.device;
    init.QueueFamily=context.queueFamily; init.Queue=context.graphicsQueue;
    init.MinImageCount=kMaxFramesInFlight; init.ImageCount=kMaxFramesInFlight;
    init.DescriptorPoolSize=128; init.MSAASamples=VK_SAMPLE_COUNT_1_BIT;
    init.UseDynamicRendering=true;
    init.PipelineRenderingCreateInfo.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    init.PipelineRenderingCreateInfo.colorAttachmentCount=1;
    init.PipelineRenderingCreateInfo.pColorAttachmentFormats=&toolkit.format;
    if (!ImGui_ImplVulkan_Init(&init)) return false;
    toolkit.initialized=true;
    if (!CreateVulkanUiPipeline(context,format,toolkit.pipeline)) return false;
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter=VK_FILTER_LINEAR; sampler.minFilter=VK_FILTER_LINEAR;
    sampler.addressModeU=sampler.addressModeV=sampler.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    return vkCreateSampler(context.device,&sampler,nullptr,&toolkit.sampler)==VK_SUCCESS &&
           ImGui_ImplVulkan_CreateFontsTexture();
}
bool RefreshVulkanUiFormat(const VulkanContext& context, VkFormat format, VulkanUiToolkit& toolkit)
{
    if (!toolkit.initialized) return false;
    if (toolkit.format == format) return true;

    VulkanUiPipeline replacement{};
    if (!CreateVulkanUiPipeline(context,format,replacement)) return false;
    if (vkDeviceWaitIdle(context.device) != VK_SUCCESS) {
        DestroyVulkanUiPipeline(context,replacement);
        return false;
    }

    // Both frame slots may reference the previous attachment format. This rare
    // display-format transition retires them together, before ImGui NewFrame;
    // ordinary viewport-size changes still wait only their own frame fence.
    toolkit.ui->Activate();
    toolkit.ui->SetSceneTexture(0);
    for (auto& viewport : toolkit.viewports) DestroyVulkanUiViewport(context,viewport);
    DestroyVulkanUiPipeline(context,toolkit.pipeline);
    toolkit.pipeline=std::exchange(replacement,{});
    toolkit.format=format;
    return true;
}
void DestroyVulkanUiToolkit(const VulkanContext& context, VulkanUiToolkit& toolkit)
{
    if (!toolkit.initialized) { toolkit={}; return; }
    toolkit.ui->Activate();
    for (auto& viewport : toolkit.viewports) DestroyVulkanUiViewport(context,viewport);
    for (auto& image : toolkit.images) DestroyVulkanUiImage(context,image);
    if (toolkit.sampler) vkDestroySampler(context.device,toolkit.sampler,nullptr);
    DestroyVulkanUiPipeline(context,toolkit.pipeline);
    ImGui_ImplVulkan_Shutdown(); toolkit={};
}
void RecordVulkanUiToolkit(VkCommandBuffer command, VulkanUiToolkit& toolkit, u32 slot,
                           VkImageView target, VkExtent2D extent)
{
    toolkit.ui->Activate();
    auto& viewport=toolkit.viewports[slot];
    if (viewport.layout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout=viewport.layout; barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
        barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        barrier.image=viewport.image; barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,0,nullptr,0,nullptr,1,&barrier);
        viewport.layout=barrier.newLayout;
    }
    VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    attachment.imageView=target; attachment.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.color={{0.03f,0.04f,0.055f,1.0f}};
    VkRenderingInfo render{VK_STRUCTURE_TYPE_RENDERING_INFO};
    render.renderArea.extent=extent; render.layerCount=1; render.colorAttachmentCount=1; render.pColorAttachments=&attachment;
    vkCmdBeginRendering(command,&render);
    auto* data=static_cast<ImDrawData*>(toolkit.ui->DrawData());
    if (data) ImGui_ImplVulkan_RenderDrawData(data,command,toolkit.pipeline.pipeline);
    vkCmdEndRendering(command);
}
}
