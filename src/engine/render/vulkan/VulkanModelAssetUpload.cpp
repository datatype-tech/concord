// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelAsset.h"

#include <limits>
#include <span>
#include <utility>

namespace Concord {
namespace {

template <typename T>
bool UploadVector(VulkanBuffer& buffer, const std::vector<T>& values) noexcept
{
    if (values.empty() || values.size() > std::numeric_limits<VkDeviceSize>::max() / sizeof(T)) {
        return false;
    }
    const std::span<const T> view(values.data(), values.size());
    return UploadVulkanBuffer(buffer, std::as_bytes(view));
}

template <typename T>
bool CreateVectorBuffer(const VulkanContext& context, const std::vector<T>& values,
                       VkBufferUsageFlags usage, bool deviceAddress, VulkanBuffer& buffer)
{
    if (values.empty() || values.size() > std::numeric_limits<VkDeviceSize>::max() / sizeof(T)) {
        return false;
    }
    const VkDeviceSize size = static_cast<VkDeviceSize>(values.size() * sizeof(T));
    return CreateVulkanHostBuffer(context, size, usage, buffer, deviceAddress) &&
           UploadVector(buffer, values);
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
    if (!CreateVectorBuffer(context, data.vertices, vertexUsage, rayGeometry,
                            output.vertexBuffer) ||
        !CreateVectorBuffer(context, data.indices, indexUsage, rayGeometry,
                            output.indexBuffer) ||
        !CreateVectorBuffer(context, data.materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, false,
                            output.materialBuffer)) {
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
