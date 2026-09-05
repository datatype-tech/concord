// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "VulkanRayTracingSceneGpuSupport.h"
#include "engine/asset/ModelAsset.h"
#include "engine/render/RenderFrameData.h"
#include "engine/render/vulkan/VulkanFrameDataResources.h"
#include "engine/render/vulkan/VulkanModelAssetCache.h"
#include "engine/render/vulkan/VulkanRayTracingOutput.h"
#include "engine/render/vulkan/VulkanRayTracingPipeline.h"
#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"
#include "engine/render/vulkan/VulkanShaderModule.h"
#include <memory>
#include <vector>
namespace {
std::shared_ptr<Concord::ModelAsset> MakeTriangle()
{
    auto asset = std::make_shared<Concord::ModelAsset>();
    asset->materials.push_back(Concord::ModelMaterial{});
    asset->meshes.push_back(Concord::ModelMesh{.primitives = {
        Concord::ModelPrimitive{
            .vertices = {Concord::ModelVertex{.position = {-1.0f, 0.0f, 0.0f}},
                         Concord::ModelVertex{.position = {1.0f, 0.0f, 0.0f}},
                         Concord::ModelVertex{.position = {0.0f, 1.0f, 0.0f}}},
            .indices = {0, 1, 2},
        }}});
    return asset;
}
} // namespace
int main() {
    if (Concord::ReadVulkanShaderCode("raygen.rgen.spv").empty() || Concord::ReadVulkanShaderCode("raymiss.rmiss.spv").empty() || Concord::ReadVulkanShaderCode("rayhit.rchit.spv").empty()) return 77;
    VkInstance instance = VK_NULL_HANDLE;
    if (!ConcordTest::CreateInstance(instance)) return 77;
    Concord::u32 count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || count == 0) {
        vkDestroyInstance(instance, nullptr); return 77;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    Concord::VulkanRayTracingSupport support{};
    Concord::u32 family = 0xffffffffu;
    for (VkPhysicalDevice candidate : devices) {
        const auto candidateSupport = Concord::QueryVulkanRayTracingSupport(candidate);
        const auto candidateFamily = ConcordTest::FindGraphicsFamily(candidate);
        if (candidateSupport.IsUsable() && candidateFamily != 0xffffffffu) {
            physical = candidate;
            support = candidateSupport;
            family = candidateFamily;
            break;
        }
    }
    if (physical == VK_NULL_HANDLE) { vkDestroyInstance(instance, nullptr); return 77; }
    VkDevice device = VK_NULL_HANDLE;
    if (!ConcordTest::CreateRayTracingDevice(physical, family, device)) {
        vkDestroyInstance(instance, nullptr); return 77;
    }
    Concord::VulkanContext context{.instance = instance, .physicalDevice = physical,
                                   .device = device, .queueFamily = family, .rayTracing = support};
    Concord::VulkanRayTracingScene scene{};
    Concord::VulkanModelAssetCache cache{};
    Concord::VulkanFrameDataResources frameData{};
    Concord::VulkanRayTracingPipeline pipeline{};
    Concord::VulkanRayTracingOutputRing output{};
    const auto asset = MakeTriangle();
    int status = Concord::CreateVulkanRayTracingScene(context, scene) ? 0 : 1;
    if (status == 0) status = cache.Ensure(context, asset) ? 0 : 1;
    Concord::RenderSceneSnapshot snapshot{};
    snapshot.hasCamera = true;
    snapshot.objects.push_back(Concord::RenderObjectSnapshot{
        .model = Concord::Mat4::Translate({0.0f, 0.0f, 1.0f}),
        .shape = Concord::PrimitiveShape::Model,
        .modelAsset = asset,
    });
    if (status == 0 && !Concord::EnsureVulkanRayTracingModelPrimitives(
                           context, scene, snapshot, cache)) {
        status = 1;
    }
    if (status == 0 && (scene.modelPrimitives.size() != 1 ||
                        scene.modelPrimitiveInfos.size() != 1 ||
                        !scene.modelVertexBuffer.IsReady() || !scene.modelIndexBuffer.IsReady() ||
                        scene.modelPrimitiveBuffer.mapped == nullptr ||
                        static_cast<const Concord::VulkanRayTracingModelPrimitiveInfo*>(
                            scene.modelPrimitiveBuffer.mapped)->indexCount != 3)) {
        status = 1;
    }
    if (status == 0 && (!Concord::CreateVulkanFrameDataResources(context, frameData) ||
                        !Concord::UploadVulkanFrameData(frameData, 0, Concord::RenderFrameData{
                            .header = {.cameraValid = 1},
                            .camera = {.view = Concord::Mat4::Identity(),
                                       .projection = Concord::Mat4::Identity()}}))) {
        status = 1;
    }
    if (status == 0 && !Concord::CreateVulkanRayTracingPipeline(
                           context, frameData.layout, scene.descriptorLayout, pipeline)) {
        status = 1;
    }
    if (status == 0 && !Concord::CreateVulkanRayTracingOutputRing(
                           context, pipeline.outputLayout, {16, 16}, output)) {
        status = 1;
    }
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    if (status == 0) {
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = family;
        status = vkCreateCommandPool(device, &poolInfo, nullptr, &pool) == VK_SUCCESS ? 0 : 1;
    }
    if (status == 0) {
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        status = vkAllocateCommandBuffers(device, &allocate, &command) == VK_SUCCESS ? 0 : 1;
    }
    if (status == 0) {
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        status = vkBeginCommandBuffer(command, &begin) == VK_SUCCESS ? 0 : 1;
        if (status == 0 && !Concord::RecordVulkanRayTracingSceneBuild(command, scene, &snapshot)) {
            status = 1;
        }
        if (status == 0) {
            Concord::InsertVulkanRayTracingSceneReadBarrier(
                command, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            Concord::PrepareVulkanRayTracingOutput(command, output.At(0));
            if (!Concord::RecordVulkanRayTracingDispatch(
                    command, pipeline, frameData.sets[0], output.At(0).descriptorSet,
                    scene, output.At(0).extent)) {
                status = 1;
            }
        }
        if (status == 0) status = vkEndCommandBuffer(command) == VK_SUCCESS ? 0 : 1;
    }
    if (status == 0) {
        VkQueue queue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, family, 0, &queue);
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        status = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS ? 0 : 1;
        if (status == 0) status = vkQueueWaitIdle(queue) == VK_SUCCESS ? 0 : 1;
    }
    Concord::DestroyVulkanRayTracingOutputRing(context, output);
    Concord::DestroyVulkanRayTracingPipeline(context, pipeline);
    Concord::DestroyVulkanFrameDataResources(context, frameData);
    Concord::DestroyVulkanRayTracingScene(context, scene); cache.Clear(context);
    if (pool != VK_NULL_HANDLE) vkDestroyCommandPool(device, pool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    return status; }
