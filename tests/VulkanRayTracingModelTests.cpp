// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>

namespace {

bool TestModelAbi()
{
    return alignof(Concord::VulkanRayTracingModelVertex) == 16 &&
           sizeof(Concord::VulkanRayTracingModelVertex) == 48 &&
           offsetof(Concord::VulkanRayTracingModelVertex, position) == 0 &&
           offsetof(Concord::VulkanRayTracingModelVertex, normal) == 16 &&
           offsetof(Concord::VulkanRayTracingModelVertex, texcoord) == 32 &&
           alignof(Concord::VulkanRayTracingModelPrimitiveInfo) == 16 &&
           sizeof(Concord::VulkanRayTracingModelPrimitiveInfo) == 272 &&
           offsetof(Concord::VulkanRayTracingModelPrimitiveInfo, baseColor) == 16 &&
           offsetof(Concord::VulkanRayTracingModelPrimitiveInfo, emissive) == 32 &&
           offsetof(Concord::VulkanRayTracingModelPrimitiveInfo, surface) == 48 &&
           // The water block mirrors the shader's std430 declaration, so its
           // interior offsets are pinned as tightly as its position in the
           // record: an array of vec4 has a sixteen-byte stride and nothing
           // here may introduce padding between them.
           offsetof(Concord::VulkanRayTracingModelPrimitiveInfo, water) == 64 &&
           sizeof(Concord::VulkanWaterMaterial) == 208 &&
           offsetof(Concord::VulkanWaterMaterial, optics) == 0 &&
           offsetof(Concord::VulkanWaterMaterial, absorbance) == 16 &&
           offsetof(Concord::VulkanWaterMaterial, wave) == 32 &&
           offsetof(Concord::VulkanWaterMaterial, flow) == 48 &&
           offsetof(Concord::VulkanWaterMaterial, surface) == 64 &&
           offsetof(Concord::VulkanWaterMaterial, rippleShape) == 80 &&
           offsetof(Concord::VulkanWaterMaterial, rippleMotion) == 144 &&
           std::is_trivially_copyable_v<Concord::VulkanWaterMaterial> &&
           std::is_standard_layout_v<Concord::VulkanRayTracingModelVertex> &&
           std::is_trivially_copyable_v<Concord::VulkanRayTracingModelVertex>;
}

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
    primitive.vertexStride = static_cast<Concord::u32>(sizeof(Concord::ModelVertex));
    primitive.primitiveIndex = 0;
    primitive.meshIndex = 3;
    primitive.materialIndex = 5;
    primitive.metadataIndex = 9;
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
               (Concord::kVulkanRayTracingModelInstanceBit | primitive.metadataIndex) &&
           instance.transform.matrix[0][3] == 2.0f &&
           instance.transform.matrix[1][3] == 3.0f &&
           instance.transform.matrix[2][3] == 4.0f;
}

/**
 * An authored Box material has to reach the hit shader.
 *
 * The hit shader used to name a fixed eight-entry palette with the instance
 * index, so every authored colour in a ray traced scene rendered as one of
 * eight hues: a grey floor came out near-black, and water looking through it
 * had nothing to show. This pins the encoding that replaced it.
 */
bool TestBoxMaterialInstance()
{
    Concord::VulkanRayTracingScene scene{};
    std::array<VkAccelerationStructureInstanceKHR,
               Concord::kVulkanRayTracingMaxInstances> instances{};
    std::array<Concord::VulkanBoxMaterial, Concord::kVulkanRayTracingMaxInstances> materials{};
    scene.instanceBuffer.buffer = reinterpret_cast<VkBuffer>(1);
    scene.instanceBuffer.memory = reinterpret_cast<VkDeviceMemory>(1);
    scene.instanceBuffer.size = sizeof(instances);
    scene.instanceBuffer.allocationSize = sizeof(instances);
    scene.instanceBuffer.memoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    scene.instanceBuffer.mapped = instances.data();
    scene.boxMaterialBuffer.buffer = reinterpret_cast<VkBuffer>(7);
    scene.boxMaterialBuffer.memory = reinterpret_cast<VkDeviceMemory>(7);
    scene.boxMaterialBuffer.size = sizeof(materials);
    scene.boxMaterialBuffer.allocationSize = sizeof(materials);
    scene.boxMaterialBuffer.memoryProperties = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    scene.boxMaterialBuffer.mapped = materials.data();
    scene.bottomLevelAddress = 0x1000;

    Concord::RenderSceneSnapshot snapshot{};
    snapshot.objects.push_back(Concord::RenderObjectSnapshot{
        .model = Concord::Mat4::Translate({1.0f, 2.0f, 3.0f}),
        .shape = Concord::PrimitiveShape::Box,
        .material = Concord::Material{.albedo = COLOR_RGB(255, 0, 0),
                                      .metallic = 0.25f,
                                      .roughness = 0.5f,
                                      .emissive = 2.0f},
    });
    snapshot.objects.push_back(Concord::RenderObjectSnapshot{
        .shape = Concord::PrimitiveShape::Box,
        .material = Concord::Material{.albedo = COLOR_RGB(0, 255, 0), .roughness = 0.9f},
    });
    if (Concord::UploadVulkanRayTracingInstances(scene, &snapshot) != 2) return false;
    const Concord::u32 first = Concord::kVulkanRayTracingBoxMaterialBit | 0u;
    const Concord::u32 second = Concord::kVulkanRayTracingBoxMaterialBit | 1u;
    if (instances[0].instanceCustomIndex != first ||
        instances[1].instanceCustomIndex != second) {
        return false;
    }
    // A model instance is identified by its own, higher bit, so the box bit
    // must never be set on one.
    if ((Concord::kVulkanRayTracingModelInstanceBit & Concord::kVulkanRayTracingBoxMaterialBit) != 0u) {
        return false;
    }
    const Concord::VulkanBoxMaterial& packed = materials[0];
    return std::abs(packed.surface.x - 0.25f) < 0.0001f &&
           std::abs(packed.surface.y - 0.5f) < 0.0001f &&
           std::abs(packed.surface.z - 2.0f) < 0.0001f &&
           packed.albedo.x > 0.99f && packed.albedo.y < 0.01f &&
           std::abs(packed.albedo.w - 1.0f) < 0.0001f &&
           std::abs(materials[1].albedo.y - 1.0f) < 0.0001f;
}

/** A non-finite or out-of-range material must not blank a surface. */
bool TestBoxMaterialSanitizing()
{
    const Concord::VulkanBoxMaterial packed = Concord::MakeVulkanBoxMaterial(
        Concord::Material{.albedo = COLOR_RGB(255, 255, 255),
                          .metallic = 9.0f,
                          .roughness = -4.0f,
                          .emissive = -1.0f});
    const Concord::VulkanBoxMaterial recovered = Concord::MakeVulkanBoxMaterial(
        Concord::Material{.metallic = std::numeric_limits<float>::quiet_NaN()});
    return std::abs(packed.surface.x - 1.0f) < 0.0001f &&
           std::abs(packed.surface.y - 0.04f) < 0.0001f &&
           std::abs(packed.surface.z) < 0.0001f &&
           std::abs(recovered.surface.x) < 0.0001f &&
           std::abs(recovered.surface.y - 0.8f) < 0.0001f;
}

/**
 * A skinned primitive must reach the TLAS through its owning entity rather
 * than through the shared asset, because every animated instance of the same
 * rest mesh deforms differently.
 */
bool TestSkinnedPrimitiveInstance()
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
    primitive.skinned = true;
    primitive.vertexStride =
        static_cast<Concord::u32>(sizeof(Concord::VulkanRayTracingModelVertex));
    primitive.meshIndex = 2;
    primitive.materialIndex = 1;
    primitive.metadataIndex = 4;
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
    primitive.address = 0x5000;
    primitive.indexCount = 3;
    primitive.vertexCount = 3;
    primitive.scratchSize = 256;
    scene.modelPrimitives.push_back(primitive);

    Concord::VulkanRayTracingSkinnedSource source{};
    source.modelPrimitiveIndex = 0;
    source.metadataIndex = 4;
    scene.skinnedSources.push_back(source);

    const Concord::Entity entity{};
    Concord::RenderSceneSnapshot snapshot{};
    snapshot.objects.push_back(Concord::RenderObjectSnapshot{
        .entity = entity,
        .model = Concord::Mat4::Identity(),
        .shape = Concord::PrimitiveShape::Model,
        .modelAsset = asset,
        .modelSkin = 0,
    });
    if (Concord::UploadVulkanRayTracingInstances(scene, &snapshot) != 1) return false;
    if (mapped[0].instanceCustomIndex !=
        (Concord::kVulkanRayTracingModelInstanceBit | primitive.metadataIndex)) {
        return false;
    }
    // A skinned primitive must not be claimed by an unrelated entity.
    snapshot.objects[0].entity = Concord::Entity{};
    snapshot.objects[0].entity.index = 7u;
    if (Concord::UploadVulkanRayTracingInstances(scene, &snapshot) != 0) return false;
    // Nor may a static draw pick up a skinned primitive.
    snapshot.objects[0].modelSkin = -1;
    snapshot.objects[0].entity = entity;
    return Concord::UploadVulkanRayTracingInstances(scene, &snapshot) == 0;
}

} // namespace

int main()
{
    return TestModelAbi() && TestBoxMaterialSanitizing() && TestBoxMaterialInstance() &&
                   TestImportedPrimitiveInstance() && TestSkinnedPrimitiveInstance()
               ? 0
               : 1;
}
