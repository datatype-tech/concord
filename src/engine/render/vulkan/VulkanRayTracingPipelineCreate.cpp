// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingPipeline.h"

#include "engine/render/vulkan/VulkanRayTracingPipelineInternal.h"
#include "engine/render/vulkan/VulkanResult.h"
#include "engine/render/vulkan/VulkanShaderModule.h"
namespace Concord {
namespace {
template <typename Function>
Function LoadFunction(VkDevice device, const char* name) noexcept
{
    return reinterpret_cast<Function>(vkGetDeviceProcAddr(device, name));
}

/** Creates the storage-image descriptor layout consumed by raygen. */
VkDescriptorSetLayout CreateOutputLayout(VkDevice device)
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    return vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) == VK_SUCCESS
               ? layout
               : VK_NULL_HANDLE;
}
/** Creates a three-set layout: frame data, output image, and TLAS. */
VkPipelineLayout CreatePipelineLayout(VkDevice device, VkDescriptorSetLayout frameData,
                                      VkDescriptorSetLayout output, VkDescriptorSetLayout scene)
{
    VkDescriptorSetLayout layouts[] = {frameData, output, scene};
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 3;
    info.pSetLayouts = layouts;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    return vkCreatePipelineLayout(device, &info, nullptr, &layout) == VK_SUCCESS
               ? layout
               : VK_NULL_HANDLE;
}
/** Creates raygen, miss, and closest-hit shader modules from staged assets. */
bool LoadModules(const VulkanContext& context, VkShaderModule modules[3])
{
    constexpr const char* names[] = {"raygen.rgen.spv", "raymiss.rmiss.spv",
                                     "rayhit.rchit.spv"};
    for (u32 index = 0; index < 3; ++index) {
        modules[index] = CreateVulkanShaderModule(context, ReadVulkanShaderCode(names[index]));
        if (modules[index] == VK_NULL_HANDLE) {
            for (u32 cleanup = 0; cleanup < 3; ++cleanup) {
                if (modules[cleanup] != VK_NULL_HANDLE) {
                    vkDestroyShaderModule(context.device, modules[cleanup], nullptr);
                    modules[cleanup] = VK_NULL_HANDLE;
                }
            }
            return false;
        }
    }
    return true;
}
/** Builds the three shader groups and creates the KHR ray-tracing pipeline. */
bool CreatePipeline(VulkanRayTracingPipeline& pipeline, VkShaderModule modules[3])
{
    VkPipelineShaderStageCreateInfo stages[3]{};
    const VkShaderStageFlagBits stageKinds[] = {VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                                                 VK_SHADER_STAGE_MISS_BIT_KHR,
                                                 VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR};
    for (u32 index = 0; index < 3; ++index) {
        stages[index].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[index].stage = stageKinds[index];
        stages[index].module = modules[index];
        stages[index].pName = "main";
    }
    VkRayTracingShaderGroupCreateInfoKHR groups[3]{};
    groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    groups[0].generalShader = 0;
    groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;
    groups[1] = groups[0];
    groups[1].generalShader = 1;
    groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    groups[2].generalShader = VK_SHADER_UNUSED_KHR;
    groups[2].closestHitShader = 2;
    groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
    groups[2].intersectionShader = VK_SHADER_UNUSED_KHR;
    VkRayTracingPipelineCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    info.stageCount = 3;
    info.pStages = stages;
    info.groupCount = 3;
    info.pGroups = groups;
    info.maxPipelineRayRecursionDepth = 1;
    info.layout = pipeline.layout;
    return pipeline.createPipelines(pipeline.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &info,
                                    nullptr, &pipeline.pipeline) == VK_SUCCESS;
}
} // namespace
bool CreateVulkanRayTracingPipeline(const VulkanContext& context,
                                    VkDescriptorSetLayout frameDataLayout,
                                    VkDescriptorSetLayout sceneLayout,
                                    VulkanRayTracingPipeline& pipeline)
{
    DestroyVulkanRayTracingPipeline(context, pipeline);
    if (context.device == VK_NULL_HANDLE || frameDataLayout == VK_NULL_HANDLE ||
        sceneLayout == VK_NULL_HANDLE || !context.rayTracing.IsUsable()) {
        return false;
    }
    pipeline.device = context.device;
    pipeline.createPipelines = LoadFunction<PFN_vkCreateRayTracingPipelinesKHR>(
        context.device, "vkCreateRayTracingPipelinesKHR");
    pipeline.getShaderGroupHandles = LoadFunction<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        context.device, "vkGetRayTracingShaderGroupHandlesKHR");
    pipeline.cmdTraceRays = LoadFunction<PFN_vkCmdTraceRaysKHR>(context.device, "vkCmdTraceRaysKHR");
    pipeline.support = context.rayTracing;
    if (pipeline.createPipelines == nullptr || pipeline.getShaderGroupHandles == nullptr ||
        pipeline.cmdTraceRays == nullptr) {
        DestroyVulkanRayTracingPipeline(context, pipeline);
        return false;
    }
    pipeline.outputLayout = CreateOutputLayout(context.device);
    pipeline.layout = pipeline.outputLayout == VK_NULL_HANDLE
                          ? VK_NULL_HANDLE
                          : CreatePipelineLayout(context.device, frameDataLayout,
                                                 pipeline.outputLayout, sceneLayout);
    VkShaderModule modules[3]{};
    const bool ready = pipeline.layout != VK_NULL_HANDLE && LoadModules(context, modules) &&
                       CreatePipeline(pipeline, modules) &&
                       CreateVulkanRayTracingPipelineSbt(context, pipeline);
    for (VkShaderModule module : modules) {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(context.device, module, nullptr);
        }
    }
    if (!ready) {
        DestroyVulkanRayTracingPipeline(context, pipeline);
        return false;
    }
    return pipeline.IsReady();
}
} // namespace Concord
