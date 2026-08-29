// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANMODELASSET_H
#define CONCORD_VULKANMODELASSET_H

#include "engine/asset/ModelAsset.h"
#include "engine/render/vulkan/VulkanBuffer.h"
#include "engine/core/Vec4.h"

#include <string>
#include <vector>

namespace Concord {

/** GPU-compatible material values produced from one imported material. */
struct VulkanModelMaterial {
    Vec4 baseColor{};
    Vec4 emissive{};
    Vec4 surface{};
};

static_assert(sizeof(VulkanModelMaterial) == sizeof(Vec4) * 3);

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
        return vertexBuffer.IsReady() && indexBuffer.IsReady() &&
               materialBuffer.IsReady() && !primitives.empty() &&
               vertexCount != 0 && indexCount != 0;
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
