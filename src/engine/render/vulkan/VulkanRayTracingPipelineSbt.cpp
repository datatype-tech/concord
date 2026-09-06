// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingPipelineInternal.h"

#include <cstddef>
#include <cstring>
#include <vector>

namespace Concord {

bool CreateVulkanRayTracingPipelineSbt(const VulkanContext& context,
                                       VulkanRayTracingPipeline& pipeline)
{
    // Raygen, sky miss + shadow miss, one hit group.
    pipeline.sbtLayout = BuildVulkanRayTracingSbtLayout(pipeline.support, 1, 2, 1);
    if (!pipeline.sbtLayout.IsReady() || pipeline.pipeline == VK_NULL_HANDLE ||
        pipeline.getShaderGroupHandles == nullptr) {
        return false;
    }
    constexpr u32 groupCount = 4;
    const usize handleSize = pipeline.support.shaderGroupHandleSize;
    const usize handleBytes = handleSize * groupCount;
    std::vector<std::byte> handles(handleBytes);
    if (pipeline.getShaderGroupHandles(pipeline.device, pipeline.pipeline, 0, groupCount,
                                       handles.size(), handles.data()) != VK_SUCCESS) {
        return false;
    }
    std::vector<std::byte> records(static_cast<usize>(pipeline.sbtLayout.totalSize));
    const VkDeviceSize offsets[] = {pipeline.sbtLayout.raygen.offset,
                                    pipeline.sbtLayout.miss.offset,
                                    pipeline.sbtLayout.miss.offset +
                                        static_cast<VkDeviceSize>(pipeline.sbtLayout.miss.stride),
                                    pipeline.sbtLayout.hit.offset};
    for (u32 index = 0; index < groupCount; ++index) {
        std::memcpy(records.data() + static_cast<usize>(offsets[index]),
                    handles.data() + index * handleSize, handleSize);
    }
    VulkanBufferCreateInfo info{};
    info.size = pipeline.sbtLayout.totalSize;
    info.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
    info.requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    info.preferredMemoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    info.persistentMap = true;
    info.deviceAddress = true;
    if (!CreateVulkanBuffer(context, info, pipeline.sbt) ||
        !UploadVulkanBuffer(pipeline.sbt, records)) {
        return false;
    }
    return BuildVulkanRayTracingSbtRegions(pipeline.sbt.GetDeviceAddress(), pipeline.sbtLayout,
                                           pipeline.raygenRegion, pipeline.missRegion,
                                           pipeline.hitRegion);
}

} // namespace Concord
