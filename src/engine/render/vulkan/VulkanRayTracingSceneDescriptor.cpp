// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/render/vulkan/VulkanResult.h"

namespace Concord {

bool CreateVulkanRayTracingSceneDescriptor(const VulkanContext& context,
                                          VulkanRayTracingScene& scene)
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    if (context.rayTracing.IsUsable()) {
        binding.stageFlags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                              VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    VkResult result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr,
                                                   &scene.descriptorLayout);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateDescriptorSetLayout(ray query)", result);
    }
    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    result = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &scene.descriptorPool);
    if (result != VK_SUCCESS) {
        DestroyVulkanRayTracingSceneDescriptor(context, scene);
        return VulkanFailed("vkCreateDescriptorPool(ray query)", result);
    }
    VkDescriptorSetAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocateInfo.descriptorPool = scene.descriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &scene.descriptorLayout;
    result = vkAllocateDescriptorSets(context.device, &allocateInfo, &scene.descriptorSet);
    if (result != VK_SUCCESS) {
        DestroyVulkanRayTracingSceneDescriptor(context, scene);
        return VulkanFailed("vkAllocateDescriptorSets(ray query)", result);
    }
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &scene.topLevel;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = scene.descriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    write.pNext = &asInfo;
    vkUpdateDescriptorSets(context.device, 1, &write, 0, nullptr);
    return true;
}

void DestroyVulkanRayTracingSceneDescriptor(const VulkanContext& context,
                                            VulkanRayTracingScene& scene) noexcept
{
    const VkDevice device = scene.device != VK_NULL_HANDLE ? scene.device : context.device;
    if (device != VK_NULL_HANDLE && scene.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, scene.descriptorPool, nullptr);
    }
    if (device != VK_NULL_HANDLE && scene.descriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, scene.descriptorLayout, nullptr);
    }
    scene.descriptorPool = VK_NULL_HANDLE;
    scene.descriptorLayout = VK_NULL_HANDLE;
    scene.descriptorSet = VK_NULL_HANDLE;
}

bool BindVulkanRayTracingScene(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                               const VulkanRayTracingScene& scene,
                               VkPipelineBindPoint bindPoint) noexcept
{
    if (commandBuffer == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE ||
        scene.descriptorSet == VK_NULL_HANDLE) {
        return false;
    }
    vkCmdBindDescriptorSets(commandBuffer, bindPoint, pipelineLayout, kVulkanRayTracingDescriptorSet,
                            1, &scene.descriptorSet, 0, nullptr);
    return true;
}

} // namespace Concord
