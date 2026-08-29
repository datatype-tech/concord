// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <array>
#include <memory>

namespace {

bool TestImportedPrimitiveInstance()
{
    auto asset = std::make_shared<Concord::ModelAsset>();
    Concord::VulkanRayTracingScene scene{};
    std::array<VkAccelerationStructureInstanceKHR,
               Concord::kVulkanRayTracingMaxInstances> mapped{};
    scene.instanceBuffer.buffer = reinterpret_cast<VkBuffer>(1);
    scene.instanceBuffer.memory = reinterpret_cast<VkDeviceMemory>(1);
    scene.instanceBuffer.size = sizeof(mapped);
    scene.instanceBuffer.allocationSize = sizeof(mapped);
    scene.instanceBuffer.memoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    scene.instanceBuffer.mapped = mapped.data();
    scene.bottomLevelAddress = 0x1000;
    Concord::VulkanRayTracingModelPrimitive primitive{};
    primitive.source = asset.get();
    primitive.primitiveIndex = 0;
    primitive.meshIndex = 3;
    primitive.materialIndex = 5;
    primitive.vertexBuffer = reinterpret_cast<VkBuffer>(2);
    primitive.indexBuffer = reinterpret_cast<VkBuffer>(3);
    primitive.vertexAddress = 0x2000;
    primitive.indexAddress = 0x3000;
    primitive.accelerationStructure = reinterpret_cast<VkAccelerationStructureKHR>(4);
    primitive.storage.buffer = reinterpret_cast<VkBuffer>(5);
    primitive.storage.memory = reinterpret_cast<VkDeviceMemory>(5);
    primitive.scratch.buffer = reinterpret_cast<VkBuffer>(6);
    primitive.scratch.memory = reinterpret_cast<VkDeviceMemory>(6);
    primitive.scratch.deviceAddress = 0x4000;
    primitive.indexCount = 3;
    primitive.vertexCount = 3;
    primitive.address = 0x5000;
    primitive.scratchSize = 256;
    scene.modelPrimitives.push_back(primitive);
    Concord::RenderSceneSnapshot snapshot{};
    snapshot.objects.push_back(Concord::RenderObjectSnapshot{
        .model = Concord::Mat4::Translate({2.0f, 3.0f, 4.0f}),
        .shape = Concord::PrimitiveShape::Model,
        .modelAsset = asset,
    });
    if (Concord::UploadVulkanRayTracingInstances(scene, &snapshot) != 1) return false;
    const auto& instance = mapped[0];
    return instance.accelerationStructureReference == primitive.address &&
           instance.instanceCustomIndex ==
               (Concord::kVulkanRayTracingModelInstanceBit | primitive.materialIndex) &&
           instance.transform.matrix[0][3] == 2.0f &&
           instance.transform.matrix[1][3] == 3.0f &&
           instance.transform.matrix[2][3] == 4.0f;
}

} // namespace

int main()
{
    return TestImportedPrimitiveInstance() ? 0 : 1;
}
