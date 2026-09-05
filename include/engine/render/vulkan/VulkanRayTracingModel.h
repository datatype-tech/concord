// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGMODEL_H
#define CONCORD_VULKANRAYTRACINGMODEL_H

#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/core/Vec4.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <type_traits>

namespace Concord {

struct ModelAsset;

/** Marks a TLAS instance whose BLAS comes from an imported model. */
inline constexpr u32 kVulkanRayTracingModelInstanceBit = 1u << 23;
inline constexpr u32 kVulkanRayTracingModelInstanceMask =
    kVulkanRayTracingModelInstanceBit - 1u;
inline constexpr VkDeviceSize kVulkanRayTracingModelAddressAlignment = 4;
inline constexpr u32 kVulkanRayTracingModelMetadataCapacity = 256;

/** std430-compatible vertex payload consumed by the model closest-hit shader. */
struct alignas(16) VulkanRayTracingModelVertex {
    Vec4 position{};
    Vec4 normal{};
    Vec4 texcoord{};
};

/** std430-compatible range and material payload for one model primitive. */
struct alignas(16) VulkanRayTracingModelPrimitiveInfo {
    u32 firstVertex = 0;
    u32 firstIndex = 0;
    u32 indexCount = 0;
    u32 materialIndex = 0;
    Vec4 baseColor{};
    Vec4 emissive{};
    Vec4 surface{};
};

static_assert(sizeof(VulkanRayTracingModelVertex) == sizeof(Vec4) * 3);
static_assert(alignof(VulkanRayTracingModelVertex) == 16);
static_assert(std::is_standard_layout_v<VulkanRayTracingModelVertex> &&
              std::is_trivially_copyable_v<VulkanRayTracingModelVertex>);
static_assert(offsetof(VulkanRayTracingModelVertex, position) == 0);
static_assert(offsetof(VulkanRayTracingModelVertex, normal) == sizeof(Vec4));
static_assert(offsetof(VulkanRayTracingModelVertex, texcoord) == sizeof(Vec4) * 2);
static_assert(sizeof(VulkanRayTracingModelPrimitiveInfo) == sizeof(Vec4) * 4);
static_assert(alignof(VulkanRayTracingModelPrimitiveInfo) == 16);
static_assert(std::is_standard_layout_v<VulkanRayTracingModelPrimitiveInfo> &&
              std::is_trivially_copyable_v<VulkanRayTracingModelPrimitiveInfo>);
static_assert(offsetof(VulkanRayTracingModelPrimitiveInfo, firstVertex) == 0);
static_assert(offsetof(VulkanRayTracingModelPrimitiveInfo, firstIndex) == sizeof(u32));
static_assert(offsetof(VulkanRayTracingModelPrimitiveInfo, indexCount) == sizeof(u32) * 2);
static_assert(offsetof(VulkanRayTracingModelPrimitiveInfo, materialIndex) == sizeof(u32) * 3);
static_assert(offsetof(VulkanRayTracingModelPrimitiveInfo, baseColor) == sizeof(Vec4));
static_assert(offsetof(VulkanRayTracingModelPrimitiveInfo, emissive) == sizeof(Vec4) * 2);
static_assert(offsetof(VulkanRayTracingModelPrimitiveInfo, surface) == sizeof(Vec4) * 3);

/** Device-side BLAS metadata for one static imported-model primitive. */
struct VulkanRayTracingModelPrimitive {
    const ModelAsset* source = nullptr;
    u32 primitiveIndex = 0;
    u32 meshIndex = 0;
    u32 materialIndex = 0;
    u32 firstVertex = 0;
    u32 firstIndex = 0;
    u32 indexCount = 0;
    u32 vertexCount = 0;
    u32 metadataIndex = 0;
    u32 metadataFirstVertex = 0;
    u32 metadataFirstIndex = 0;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceAddress vertexAddress = 0;
    VkDeviceAddress indexAddress = 0;
    VkAccelerationStructureKHR accelerationStructure = VK_NULL_HANDLE;
    VulkanBuffer storage{};
    VulkanBuffer scratch{};
    VkDeviceAddress address = 0;
    VkDeviceSize scratchSize = 0;

    /** Whether the BLAS and its source geometry are ready for a TLAS instance. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return source != nullptr && vertexBuffer != VK_NULL_HANDLE &&
               indexBuffer != VK_NULL_HANDLE && vertexAddress != 0 && indexAddress != 0 &&
               vertexAddress % kVulkanRayTracingModelAddressAlignment == 0 &&
               indexAddress % kVulkanRayTracingModelAddressAlignment == 0 &&
               indexCount >= 3 && indexCount % 3 == 0 && vertexCount != 0 &&
               metadataIndex < kVulkanRayTracingModelMetadataCapacity &&
               metadataFirstVertex <= 0xffffffffu - vertexCount &&
               metadataFirstIndex <= 0xffffffffu - indexCount &&
               materialIndex < kVulkanRayTracingModelInstanceBit &&
               accelerationStructure != VK_NULL_HANDLE && storage.IsBound() &&
               scratch.HasDeviceAddress() && address != 0 && scratchSize != 0;
    }
};

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGMODEL_H
