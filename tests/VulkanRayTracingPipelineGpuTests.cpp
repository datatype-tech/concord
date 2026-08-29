// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "VulkanRayTracingSceneGpuSupport.h"

#include "engine/render/vulkan/VulkanRayTracingOutput.h"
#include "engine/render/vulkan/VulkanRayTracingPipeline.h"
#include "engine/render/vulkan/VulkanRayTracingScene.h"
#include "engine/render/vulkan/VulkanRayTracingSupport.h"
#include "engine/render/vulkan/VulkanShaderModule.h"

#include <vector>

namespace {

/** Creates the minimal frame-data layout consumed by the raygen shader. */
VkDescriptorSetLayout CreateFrameLayout(VkDevice device)
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                         VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = &binding;
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    return vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) == VK_SUCCESS
               ? layout
               : VK_NULL_HANDLE;
}

/** Skips the optional test when bundled RT shader artifacts were not staged. */
bool ShaderArtifactsAvailable()
{
    return !Concord::ReadVulkanShaderCode("raygen.rgen.spv").empty() &&
           !Concord::ReadVulkanShaderCode("raymiss.rmiss.spv").empty() &&
           !Concord::ReadVulkanShaderCode("rayhit.rchit.spv").empty();
}

/** Skips devices that cannot expose the storage and transfer features used by RT output. */
bool SupportsOutputFormat(VkPhysicalDevice device)
{
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device, Concord::kVulkanRayTracingOutputFormat,
                                        &properties);
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                                               VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
                                               VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                                               VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                                               VK_FORMAT_FEATURE_BLIT_DST_BIT;
    return (properties.optimalTilingFeatures & required) == required;
}

} // namespace

int main()
{
    if (!ShaderArtifactsAvailable()) {
        return 77;
    }
    VkInstance instance = VK_NULL_HANDLE;
    if (!ConcordTest::CreateInstance(instance)) {
        return 77;
    }
    Concord::u32 count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    Concord::VulkanRayTracingSupport support{};
    Concord::u32 queueFamily = 0xffffffffu;
    for (VkPhysicalDevice candidate : devices) {
        const auto candidateSupport = Concord::QueryVulkanRayTracingSupport(candidate);
        const Concord::u32 family = ConcordTest::FindGraphicsFamily(candidate);
        if (candidateSupport.IsUsable() && family != 0xffffffffu) {
            if (!SupportsOutputFormat(candidate)) {
                continue;
            }
            physicalDevice = candidate;
            support = candidateSupport;
            queueFamily = family;
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    VkDevice device = VK_NULL_HANDLE;
    if (!ConcordTest::CreateRayTracingDevice(physicalDevice, queueFamily, device)) {
        vkDestroyInstance(instance, nullptr);
        return 77;
    }
    Concord::VulkanContext context{.instance = instance,
                                   .physicalDevice = physicalDevice,
                                   .device = device,
                                   .queueFamily = queueFamily,
                                   .rayTracing = support};
    VkDescriptorSetLayout frameLayout = CreateFrameLayout(device);
    Concord::VulkanRayTracingScene scene{};
    Concord::VulkanRayTracingPipeline pipeline{};
    Concord::VulkanRayTracingOutputRing output{};
    int status = frameLayout == VK_NULL_HANDLE ? 1 : 0;
    if (status == 0) {
        status = Concord::CreateVulkanRayTracingScene(context, scene) ? 0 : 1;
    }
    if (status == 0) {
        status = Concord::CreateVulkanRayTracingPipeline(
                     context, frameLayout, scene.descriptorLayout, pipeline)
                     ? 0
                     : 1;
    }
    if (status == 0) {
        status = Concord::CreateVulkanRayTracingOutputRing(
                     context, pipeline.outputLayout, {64, 64}, output)
                     ? 0
                     : 1;
    }
    if (status == 0 && (!pipeline.IsReady() || !output.IsReady())) {
        status = 1;
    }
    if (status == 0 && !Concord::SupportsVulkanRayTracingComposite(
                           context, Concord::kVulkanRayTracingOutputFormat)) {
        status = 1;
    }
    Concord::DestroyVulkanRayTracingOutputRing(context, output);
    Concord::DestroyVulkanRayTracingPipeline(context, pipeline);
    Concord::DestroyVulkanRayTracingScene(context, scene);
    if (frameLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, frameLayout, nullptr);
    }
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    return status;
}
