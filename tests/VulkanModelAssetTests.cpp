// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelAsset.h"

#include <cmath>

namespace {

Concord::ModelPrimitive Triangle(float z, Concord::u32 material)
{
    return Concord::ModelPrimitive{
        .vertices = {
            Concord::ModelVertex{.position = {0.0f, 0.0f, z}},
            Concord::ModelVertex{.position = {1.0f, 0.0f, z}},
            Concord::ModelVertex{.position = {0.0f, 1.0f, z}},
        },
        .indices = {0, 1, 2},
        .materialIndex = material,
    };
}

bool TestFlattening()
{
    Concord::ModelAsset asset{};
    asset.materials.push_back(Concord::ModelMaterial{
        .name = "red", .baseColor = COLOR_RGBA(128, 64, 32, 200),
        .metallic = 0.25f, .roughness = 0.5f, .baseColorTexture = "red.png"});
    asset.materials.push_back(Concord::ModelMaterial{
        .name = "blue", .baseColor = COLOR_RGB(0, 0, 255), .roughness = 1.0f});
    asset.meshes.push_back(Concord::ModelMesh{.name = "first", .primitives = {Triangle(0.0f, 0)}});
    asset.meshes.push_back(Concord::ModelMesh{.name = "second", .primitives = {Triangle(1.0f, 1)}});

    Concord::VulkanModelUploadData upload{};
    if (!Concord::BuildVulkanModelUpload(asset, upload) || upload.vertices.size() != 6 ||
        upload.indices != std::vector<Concord::u32>{0, 1, 2, 3, 4, 5} ||
        upload.primitives.size() != 2 || upload.primitives[1].firstVertex != 3 ||
        upload.primitives[1].firstIndex != 3 || upload.primitives[1].meshIndex != 1 ||
        upload.baseColorTextures[0] != "red.png" || upload.baseColorTextures[1] != "") {
        return false;
    }
    const auto& material = upload.materials[0];
    return std::abs(material.baseColor.x - 0.251964f) < 0.001f &&
           std::abs(material.baseColor.w - 200.0f / 255.0f) < 0.001f &&
           std::abs(material.surface.x - 0.25f) < 0.001f;
}

bool TestRejectsInvalidAsset()
{
    Concord::ModelAsset asset{};
    Concord::VulkanModelUploadData upload{};
    return !Concord::BuildVulkanModelUpload(asset, upload) && upload.vertices.empty() &&
           upload.indices.empty() && upload.materials.empty();
}

} // namespace

int main()
{
    return TestFlattening() && TestRejectsInvalidAsset() ? 0 : 1;
}
