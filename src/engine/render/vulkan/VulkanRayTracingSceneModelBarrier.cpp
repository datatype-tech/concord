// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <vector>

namespace Concord {
namespace {

void AppendBarrier(std::vector<VkBufferMemoryBarrier>& barriers,
                   VkBuffer buffer) noexcept
{
    if (buffer == VK_NULL_HANDLE) return;
    for (const VkBufferMemoryBarrier& existing : barriers) {
        if (existing.buffer == buffer) return;
    }
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = buffer;
    barrier.size = VK_WHOLE_SIZE;
    barriers.push_back(barrier);
}

} // namespace

bool InsertVulkanRayTracingModelInputBarrier(
    VkCommandBuffer commandBuffer, const VulkanRayTracingScene& scene) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE) return false;
    std::vector<VkBufferMemoryBarrier> barriers;
    try {
        barriers.reserve(scene.modelPrimitives.size() * 2);
        for (const VulkanRayTracingModelPrimitive& primitive : scene.modelPrimitives) {
            if (!primitive.IsReady()) return false;
            AppendBarrier(barriers, primitive.vertexBuffer);
            AppendBarrier(barriers, primitive.indexBuffer);
        }
    } catch (...) {
        return false;
    }
    if (barriers.empty()) return true;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0,
                         nullptr, static_cast<u32>(barriers.size()), barriers.data(), 0, nullptr);
    return true;
}

} // namespace Concord
