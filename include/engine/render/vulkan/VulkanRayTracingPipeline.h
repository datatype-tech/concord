// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRAYTRACINGPIPELINE_H
#define CONCORD_VULKANRAYTRACINGPIPELINE_H

#include "engine/core/Types.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanRayTracingSupport.h"

#include <vulkan/vulkan.h>

namespace Concord {

struct VulkanRayTracingScene;

/** One aligned region in a ray-tracing shader binding table allocation. */
struct VulkanRayTracingSbtRegion {
    VkDeviceSize offset = 0;
    VkDeviceSize size = 0;
    VkDeviceSize stride = 0;

    /** Whether the region contains at least one shader-group record. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return size != 0 && stride != 0 && size >= stride;
    }
};

/** A validated, allocation-relative SBT layout for one ray-tracing pipeline. */
struct VulkanRayTracingSbtLayout {
    VulkanRayTracingSbtRegion raygen{};
    VulkanRayTracingSbtRegion miss{};
    VulkanRayTracingSbtRegion hit{};
    VkDeviceSize totalSize = 0;
    VkDeviceSize baseAlignment = 0;

    /** Whether all required regions and the complete allocation are valid. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return raygen.IsReady() && miss.IsReady() && hit.IsReady() && totalSize != 0 &&
               baseAlignment != 0;
    }
};

/** Computes overflow-safe offsets for raygen, miss, and hit SBT records. */
[[nodiscard]] VulkanRayTracingSbtLayout BuildVulkanRayTracingSbtLayout(
    const VulkanRayTracingSupport& support, u32 raygenRecordCount = 1,
    u32 missRecordCount = 1, u32 hitRecordCount = 1) noexcept;

/** Converts an allocation base address and layout into Vulkan trace regions. */
bool BuildVulkanRayTracingSbtRegions(
    VkDeviceAddress baseAddress, const VulkanRayTracingSbtLayout& layout,
    VkStridedDeviceAddressRegionKHR& raygen, VkStridedDeviceAddressRegionKHR& miss,
    VkStridedDeviceAddressRegionKHR& hit) noexcept;

/** Device entry points and resources for an optional KHR RT pipeline. */
struct VulkanRayTracingPipeline {
    VkDevice device = VK_NULL_HANDLE;
    VulkanRayTracingSupport support{};
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout outputLayout = VK_NULL_HANDLE;
    VulkanBuffer sbt{};
    VulkanRayTracingSbtLayout sbtLayout{};
    VkStridedDeviceAddressRegionKHR raygenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR hitRegion{};
    PFN_vkCreateRayTracingPipelinesKHR createPipelines = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR getShaderGroupHandles = nullptr;
    PFN_vkCmdTraceRaysKHR cmdTraceRays = nullptr;

    /** Whether pipeline, SBT and all required extension entry points exist. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        return device != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE &&
               layout != VK_NULL_HANDLE && outputLayout != VK_NULL_HANDLE && sbt.IsReady() &&
               sbtLayout.IsReady() && raygenRegion.size != 0 && missRegion.size != 0 &&
               hitRegion.size != 0 && createPipelines != nullptr &&
               getShaderGroupHandles != nullptr && cmdTraceRays != nullptr;
    }
};

/** Creates the optional three-stage ray-generation pipeline and SBT. */
bool CreateVulkanRayTracingPipeline(const VulkanContext& context,
                                    VkDescriptorSetLayout frameDataLayout,
                                    VkDescriptorSetLayout sceneLayout,
                                    VulkanRayTracingPipeline& pipeline);

/** Releases the pipeline layout, shader binding table and descriptor layout. */
void DestroyVulkanRayTracingPipeline(const VulkanContext& context,
                                     VulkanRayTracingPipeline& pipeline) noexcept;

/** Records one ray dispatch against a frame descriptor, output image and TLAS. */
bool RecordVulkanRayTracingDispatch(VkCommandBuffer commandBuffer,
                                    const VulkanRayTracingPipeline& pipeline,
                                    VkDescriptorSet frameDataSet,
                                    VkDescriptorSet outputSet,
                                    const VulkanRayTracingScene& scene,
                                    VkExtent2D extent) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANRAYTRACINGPIPELINE_H
