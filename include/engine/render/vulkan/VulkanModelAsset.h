// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANMODELASSET_H
#define CONCORD_VULKANMODELASSET_H

#include "engine/asset/ModelAsset.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/render/vulkan/VulkanWaterMaterial.h"
#include "engine/core/Vec4.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Concord {

/** GPU-compatible material values produced from one imported material. */
struct VulkanModelMaterial {
    Vec4 baseColor{};
    Vec4 emissive{};
    Vec4 surface{};
    /** Zeroed for a dry material; its emitters' `surface.z` is then also zero. */
    VulkanWaterMaterial water{};
};

static_assert(sizeof(VulkanModelMaterial) == sizeof(Vec4) * 16);
static_assert(offsetof(VulkanModelMaterial, water) == sizeof(Vec4) * 3);

/** Indexed range into the flattened model vertex and index buffers. */
struct VulkanModelPrimitiveRange {
    u32 firstVertex = 0;
    u32 vertexCount = 0;
    u32 firstIndex = 0;
    u32 indexCount = 0;
    u32 materialIndex = 0;
    u32 meshIndex = 0;
};

/** CPU staging representation shared by tests and the Vulkan upload path. */
struct VulkanModelUploadData {
    std::vector<ModelVertex> vertices;
    std::vector<u32> indices;
    std::vector<VulkanModelMaterial> materials;
    std::vector<VulkanModelPrimitiveRange> primitives;
    std::vector<std::string> baseColorTextures;
};

/** Flattens an imported asset without requiring a Vulkan device. */
bool BuildVulkanModelUpload(const ModelAsset& asset,
                            VulkanModelUploadData& output);

/**
 * Directory a model's relative texture URIs resolve against.
 *
 * An in-memory import carries its base directory in sourcePath rather than a
 * file path, so a path without an extension is treated as the directory
 * itself. Every consumer shares this one rule so the raster, skinned and ray
 * tracing paths build identical texture cache keys.
 */
[[nodiscard]] inline std::filesystem::path VulkanModelAssetDirectory(
    const ModelAsset& asset)
{
    if (asset.sourcePath.empty()) {
        return {};
    }
    return asset.sourcePath.has_extension() ? asset.sourcePath.parent_path() : asset.sourcePath;
}

/** Persistent GPU buffers containing one imported model's static data. */
struct VulkanModelAsset {
    VulkanBuffer vertexBuffer{};
    VulkanBuffer indexBuffer{};
    VulkanBuffer materialBuffer{};
    std::vector<VulkanModelMaterial> materials;
    std::vector<VulkanModelPrimitiveRange> primitives;
    std::vector<std::string> baseColorTextures;
    u32 vertexCount = 0;
    u32 indexCount = 0;

    /** Whether all buffers and range metadata can be consumed by a draw. */
    [[nodiscard]] bool IsReady() const noexcept
    {
        // These live in device-local memory, which is never host-mapped, so
        // readiness is storage rather than a mapped pointer.
        return vertexBuffer.HasStorage() && indexBuffer.HasStorage() &&
               materialBuffer.HasStorage() && !primitives.empty() &&
               vertexCount != 0 && indexCount != 0;
    }

    /** Whether the geometry buffers expose the Vulkan AS build-input contract. */
    [[nodiscard]] bool HasRayTracingGeometry() const noexcept
    {
        constexpr VkBufferUsageFlags required =
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        return IsReady() && vertexBuffer.HasDeviceAddress() && indexBuffer.HasDeviceAddress() &&
               (vertexBuffer.usage & required) == required &&
               (indexBuffer.usage & required) == required;
    }
};

/** Creates host-visible vertex, index and material buffers for an asset. */
bool CreateVulkanModelAsset(const VulkanContext& context,
                            const ModelAsset& asset,
                            VulkanModelAsset& output);

/** Releases all buffers and metadata owned by a model resource. */
void DestroyVulkanModelAsset(const VulkanContext& context,
                             VulkanModelAsset& asset) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANMODELASSET_H
