// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanPresent.h"

namespace Concord {

bool SubmitFrame(const VulkanContext& context, const VulkanSwapchain& swapchain,
                 VulkanFrame& frame, u32 imageIndex)
{
    if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS) {
        return false;
    }

    VkSemaphore signal = swapchain.renderFinished[imageIndex];
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frame.imageAvailable;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.commandBuffer;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &signal;

    return vkQueueSubmit(context.graphicsQueue, 1, &submit, frame.inFlight) == VK_SUCCESS;
}

bool PresentFrame(const VulkanContext& context, const VulkanSwapchain& swapchain, u32 imageIndex)
{
    VkSemaphore wait = swapchain.renderFinished[imageIndex];

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &wait;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain.handle;
    present.pImageIndices = &imageIndex;

    const VkResult result = vkQueuePresentKHR(context.graphicsQueue, &present);
    return result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR;
}

} // namespace Concord
