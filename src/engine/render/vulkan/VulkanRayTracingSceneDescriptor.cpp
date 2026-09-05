// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <array>

namespace Concord {

bool CreateVulkanRayTracingSceneDescriptor(const VulkanContext& context,
                                          VulkanRayTracingScene& scene)
{
    std::array<VkDescriptorSetLayoutBinding, 4> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    const VkShaderStageFlags rayStages = context.rayTracing.IsUsable()
                                             ? VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                                   VK_SHADER_STAGE_MISS_BIT_KHR |
                                                   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
                                             : 0;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | rayStages;
    const VkShaderStageFlags modelStages = VK_SHADER_STAGE_FRAGMENT_BIT | rayStages;
    for (u32 index = 1; index < bindings.size(); ++index) {
        bindings[index].binding = index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = modelStages;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    VkResult result = vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr,
                                                   &scene.descriptorLayout);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateDescriptorSetLayout(ray query)", result);
    }
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<u32>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;
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
    return UpdateVulkanRayTracingSceneModelDescriptors(context, scene);
}

bool UpdateVulkanRayTracingSceneModelDescriptors(const VulkanContext& context,
                                                 VulkanRayTracingScene& scene)
{
    if (context.device == VK_NULL_HANDLE || scene.descriptorSet == VK_NULL_HANDLE ||
        scene.modelPrimitiveBuffer.buffer == VK_NULL_HANDLE) {
        return false;
    }
    const VkBuffer vertexBuffer = scene.modelVertexBuffer.buffer != VK_NULL_HANDLE
                                      ? scene.modelVertexBuffer.buffer
                                      : scene.vertexBuffer.buffer;
    const VkBuffer indexBuffer = scene.modelIndexBuffer.buffer != VK_NULL_HANDLE
                                     ? scene.modelIndexBuffer.buffer
                                     : scene.indexBuffer.buffer;
    if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) return false;
    VkDescriptorBufferInfo buffers[3]{};
    buffers[0] = {vertexBuffer, 0, VK_WHOLE_SIZE};
    buffers[1] = {indexBuffer, 0, VK_WHOLE_SIZE};
    buffers[2] = {scene.modelPrimitiveBuffer.buffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet writes[3]{};
    for (u32 index = 0; index < 3; ++index) {
        writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[index].dstSet = scene.descriptorSet;
        writes[index].dstBinding = index + 1;
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[index].pBufferInfo = &buffers[index];
    }
    vkUpdateDescriptorSets(context.device, 3, writes, 0, nullptr);
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
