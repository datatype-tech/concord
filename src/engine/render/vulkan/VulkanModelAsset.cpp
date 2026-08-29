// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelAsset.h"

#include <limits>

namespace Concord {
namespace {

bool AddCount(usize value, usize& total) noexcept
{
    if (value > std::numeric_limits<usize>::max() - total) {
        return false;
    }
    total += value;
    return total <= std::numeric_limits<u32>::max();
}

bool ReserveCounts(const ModelAsset& asset, usize& vertexCount, usize& indexCount) noexcept
{
    for (const ModelMesh& mesh : asset.meshes) {
        for (const ModelPrimitive& primitive : mesh.primitives) {
            if (!AddCount(primitive.vertices.size(), vertexCount) ||
                !AddCount(primitive.indices.size(), indexCount)) {
                return false;
            }
        }
    }
    return vertexCount != 0 && indexCount != 0;
}

VulkanModelMaterial ConvertMaterial(const ModelMaterial& material) noexcept
{
    const Vec3 baseColor = ToLinear(material.baseColor);
    return VulkanModelMaterial{
        .baseColor = {baseColor.x, baseColor.y, baseColor.z,
                      static_cast<f32>(ColorA(material.baseColor)) / 255.0f},
        .emissive = {material.emissive.x, material.emissive.y, material.emissive.z, 0.0f},
        .surface = {material.metallic, material.roughness, 0.0f, 0.0f},
    };
}

} // namespace

bool BuildVulkanModelUpload(const ModelAsset& asset, VulkanModelUploadData& output)
{
    output = {};
    if (!asset.IsValid()) {
        return false;
    }

    usize vertexCount = 0;
    usize indexCount = 0;
    if (!ReserveCounts(asset, vertexCount, indexCount)) {
        return false;
    }
    try {
        output.vertices.reserve(vertexCount);
        output.indices.reserve(indexCount);
        output.materials.reserve(asset.materials.size());
        output.primitives.reserve(indexCount / 3);
        output.baseColorTextures.reserve(asset.materials.size());
        for (const ModelMaterial& material : asset.materials) {
            output.materials.push_back(ConvertMaterial(material));
            output.baseColorTextures.push_back(material.baseColorTexture);
        }

        for (usize meshIndex = 0; meshIndex < asset.meshes.size(); ++meshIndex) {
            const ModelMesh& mesh = asset.meshes[meshIndex];
            for (const ModelPrimitive& primitive : mesh.primitives) {
                const u32 firstVertex = static_cast<u32>(output.vertices.size());
                const u32 firstIndex = static_cast<u32>(output.indices.size());
                output.vertices.insert(output.vertices.end(), primitive.vertices.begin(),
                                       primitive.vertices.end());
                for (u32 index : primitive.indices) {
                    if (index > std::numeric_limits<u32>::max() - firstVertex) {
                        output = {};
                        return false;
                    }
                    output.indices.push_back(firstVertex + index);
                }
                output.primitives.push_back(VulkanModelPrimitiveRange{
                    .firstVertex = firstVertex,
                    .vertexCount = static_cast<u32>(primitive.vertices.size()),
                    .firstIndex = firstIndex,
                    .indexCount = static_cast<u32>(primitive.indices.size()),
                    .materialIndex = primitive.materialIndex,
                    .meshIndex = static_cast<u32>(meshIndex),
                });
            }
        }
    } catch (...) {
        output = {};
        return false;
    }
    return output.vertices.size() == vertexCount && output.indices.size() == indexCount &&
           output.materials.size() == asset.materials.size();
}

} // namespace Concord
