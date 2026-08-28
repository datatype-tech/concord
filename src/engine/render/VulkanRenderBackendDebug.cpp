// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendDebug.h"

namespace Concord {

void BeginVulkanDebugLabel(const VulkanContext& context, VkCommandBuffer commandBuffer,
                           const char* name, Vec3 color) noexcept
{
    if (!context.validationEnabled || context.device == VK_NULL_HANDLE) {
        return;
    }
    const auto begin = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(context.device, "vkCmdBeginDebugUtilsLabelEXT"));
    if (!begin) {
        return;
    }
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = color.x;
    label.color[1] = color.y;
    label.color[2] = color.z;
    label.color[3] = 1.0f;
    begin(commandBuffer, &label);
}

void EndVulkanDebugLabel(const VulkanContext& context, VkCommandBuffer commandBuffer) noexcept
{
    if (!context.validationEnabled || context.device == VK_NULL_HANDLE) {
        return;
    }
    const auto end = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetDeviceProcAddr(context.device, "vkCmdEndDebugUtilsLabelEXT"));
    if (end) {
        end(commandBuffer);
    }
}

} // namespace Concord
