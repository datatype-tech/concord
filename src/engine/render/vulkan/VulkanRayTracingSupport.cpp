// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSupport.h"

#include <cstring>
#include <vector>

namespace Concord {
namespace {

/** Returns whether an enumerated extension list contains `name`. */
bool HasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name) noexcept
{
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

/** Enumerates device extensions without exposing allocation failures to callers. */
bool EnumerateExtensions(VkPhysicalDevice device,
                         std::vector<VkExtensionProperties>& extensions) noexcept
{
    u32 count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }
    try {
        extensions.resize(count);
    } catch (...) {
        return false;
    }
    return vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) ==
           VK_SUCCESS;
}

/** Links only feature structs backed by extensions advertised by the device. */
void LinkFeatureChain(bool hasAcceleration, bool hasPipeline, bool hasRayQuery,
                      VkPhysicalDeviceFeatures2& features,
                      VkPhysicalDeviceBufferDeviceAddressFeatures& address,
                      VkPhysicalDeviceAccelerationStructureFeaturesKHR& acceleration,
                      VkPhysicalDeviceRayTracingPipelineFeaturesKHR& pipeline,
                      VkPhysicalDeviceRayQueryFeaturesKHR& rayQuery) noexcept
{
    void* chain = nullptr;
    if (hasRayQuery) {
        rayQuery.pNext = chain;
        chain = &rayQuery;
    }
    if (hasPipeline) {
        pipeline.pNext = chain;
        chain = &pipeline;
    }
    if (hasAcceleration) {
        acceleration.pNext = chain;
        chain = &acceleration;
    }
    address.pNext = chain;
    features.pNext = &address;
}

} // namespace

VulkanRayTracingSupport QueryVulkanRayTracingSupport(VkPhysicalDevice device) noexcept
{
    VulkanRayTracingSupport result{};
    if (device == VK_NULL_HANDLE) {
        return result;
    }
    std::vector<VkExtensionProperties> extensions;
    if (!EnumerateExtensions(device, extensions)) {
        return result;
    }
    const bool hasAcceleration =
        HasExtension(extensions, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    const bool hasPipeline =
        hasAcceleration && HasExtension(extensions, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    const bool hasDeferred =
        HasExtension(extensions, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    VkPhysicalDeviceRayQueryFeaturesKHR rayQuery{};
    rayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    const bool hasRayQuery = hasAcceleration &&
                             HasExtension(extensions, VK_KHR_RAY_QUERY_EXTENSION_NAME);
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR pipeline{};
    pipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration{};
    acceleration.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    VkPhysicalDeviceBufferDeviceAddressFeatures address{};
    address.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    LinkFeatureChain(hasAcceleration, hasPipeline, hasRayQuery, features, address, acceleration,
                     pipeline, rayQuery);
    vkGetPhysicalDeviceFeatures2(device, &features);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR pipelineProperties{};
    pipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationProperties{};
    accelerationProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    pipelineProperties.pNext = &accelerationProperties;
    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    if (hasPipeline) {
        properties.pNext = &pipelineProperties;
        vkGetPhysicalDeviceProperties2(device, &properties);
    }

    result.bufferDeviceAddress = address.bufferDeviceAddress == VK_TRUE;
    result.accelerationStructure = acceleration.accelerationStructure == VK_TRUE;
    result.rayTracingPipeline = pipeline.rayTracingPipeline == VK_TRUE;
    result.deferredHostOperations = hasDeferred;
    result.rayQuery = hasRayQuery && rayQuery.rayQuery == VK_TRUE;
    result.shaderGroupHandleSize = pipelineProperties.shaderGroupHandleSize;
    result.shaderGroupHandleAlignment = pipelineProperties.shaderGroupHandleAlignment;
    result.shaderGroupBaseAlignment = pipelineProperties.shaderGroupBaseAlignment;
    result.maxShaderGroupStride = pipelineProperties.maxShaderGroupStride;
    result.maxRayRecursionDepth = pipelineProperties.maxRayRecursionDepth;
    result.maxRayHitAttributeSize = pipelineProperties.maxRayHitAttributeSize;
    result.supported = result.bufferDeviceAddress && result.accelerationStructure &&
                       result.rayTracingPipeline && result.deferredHostOperations &&
                       result.shaderGroupHandleSize != 0 &&
                       result.shaderGroupHandleAlignment != 0 &&
                       result.shaderGroupBaseAlignment != 0 &&
                       result.maxShaderGroupStride != 0 &&
                       result.maxRayRecursionDepth != 0;
    return result;
}

} // namespace Concord
