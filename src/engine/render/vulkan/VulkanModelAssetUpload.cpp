// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelAsset.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <limits>
#include <span>
#include <utility>

namespace Concord {
namespace {

/**
 * Copies host bytes into a device-local buffer through one transient submit.
 *
 * Model assets are created once per unique file, so a blocking upload here
 * costs a single round trip at load time and buys device-local reads for
 * every later draw. Keeping the vertex and index data in host-visible memory
 * instead would make each frame read them back across the bus.
 */
bool UploadToDeviceLocal(const VulkanContext& context, VulkanBuffer& destination,
                         std::span<const std::byte> bytes) noexcept
{
    if (bytes.empty() || bytes.size() > destination.size ||
        context.graphicsQueue == VK_NULL_HANDLE) {
        return false;
    }
    VulkanBuffer staging{};
    if (!CreateVulkanHostBuffer(context, static_cast<VkDeviceSize>(bytes.size()),
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging) ||
        !UploadVulkanBuffer(staging, bytes)) {
        DestroyVulkanBuffer(context, staging);
        return false;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = context.queueFamily;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(context.device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        DestroyVulkanBuffer(context, staging);
        return false;
    }

    VkCommandBufferAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandPool = pool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocation.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    bool ok =
        vkAllocateCommandBuffers(context.device, &allocation, &commandBuffer) == VK_SUCCESS;
    if (ok) {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        ok = vkBeginCommandBuffer(commandBuffer, &begin) == VK_SUCCESS;
    }
    if (ok) {
        VkBufferCopy region{};
        region.size = static_cast<VkDeviceSize>(bytes.size());
        vkCmdCopyBuffer(commandBuffer, staging.buffer, destination.buffer, 1, &region);
        ok = vkEndCommandBuffer(commandBuffer) == VK_SUCCESS;
    }
    if (ok) {
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &commandBuffer;
        ok = vkQueueSubmit(context.graphicsQueue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS;
    }
    if (ok) {
        // The staging buffer is released below, so the copy has to be done.
        ok = vkQueueWaitIdle(context.graphicsQueue) == VK_SUCCESS;
        if (!ok) {
            VulkanFailed("vkQueueWaitIdle(model upload)", VK_ERROR_UNKNOWN);
        }
    }
    vkDestroyCommandPool(context.device, pool, nullptr);
    DestroyVulkanBuffer(context, staging);
    return ok;
}

/** Creates a device-local buffer filled from a host vector. */
template <typename T>
bool CreateDeviceVectorBuffer(const VulkanContext& context, const std::vector<T>& values,
                              VkBufferUsageFlags usage, bool deviceAddress, VulkanBuffer& buffer)
{
    if (values.empty() || values.size() > std::numeric_limits<VkDeviceSize>::max() / sizeof(T)) {
        return false;
    }
    VulkanBufferCreateInfo info{};
    info.size = static_cast<VkDeviceSize>(values.size() * sizeof(T));
    info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.deviceAddress = deviceAddress;
    if (!CreateVulkanBuffer(context, info, buffer)) {
        return false;
    }
    const std::span<const T> view(values.data(), values.size());
    return UploadToDeviceLocal(context, buffer, std::as_bytes(view));
}

} // namespace

bool CreateVulkanModelAsset(const VulkanContext& context, const ModelAsset& asset,
                            VulkanModelAsset& output)
{
    DestroyVulkanModelAsset(context, output);
    VulkanModelUploadData data{};
    if (!BuildVulkanModelUpload(asset, data)) {
        return false;
    }
    const bool rayGeometry = context.rayTracing.bufferDeviceAddress &&
                             context.rayTracing.accelerationStructure;
    const VkBufferUsageFlags vertexUsage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        (rayGeometry ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0);
    const VkBufferUsageFlags indexUsage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        (rayGeometry ? VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0);
    if (!CreateDeviceVectorBuffer(context, data.vertices, vertexUsage, rayGeometry,
                                  output.vertexBuffer) ||
        !CreateDeviceVectorBuffer(context, data.indices, indexUsage, rayGeometry,
                                  output.indexBuffer) ||
        !CreateDeviceVectorBuffer(context, data.materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                  false, output.materialBuffer)) {
        DestroyVulkanModelAsset(context, output);
        return false;
    }
    output.vertexCount = static_cast<u32>(data.vertices.size());
    output.indexCount = static_cast<u32>(data.indices.size());
    output.materials = std::move(data.materials);
    output.primitives = std::move(data.primitives);
    output.baseColorTextures = std::move(data.baseColorTextures);
    return output.IsReady();
}

void DestroyVulkanModelAsset(const VulkanContext& context, VulkanModelAsset& asset) noexcept
{
    DestroyVulkanBuffer(context, asset.materialBuffer);
    DestroyVulkanBuffer(context, asset.indexBuffer);
    DestroyVulkanBuffer(context, asset.vertexBuffer);
    asset.primitives.clear();
    asset.materials.clear();
    asset.baseColorTextures.clear();
    asset.vertexCount = 0;
    asset.indexCount = 0;
}

} // namespace Concord
