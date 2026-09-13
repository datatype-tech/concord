// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanModelAsset.h"

#include <cmath>

namespace {

bool Near(float left, float right)
{
    return std::abs(left - right) < 0.0001f;
}

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

/**
 * The water block is what the hit shader shades from, so the packing rules that
 * matter are pinned here: a dry material must carry a zero mask, and a water
 * material must compact its disabled ripple sources out rather than leave holes
 * the shader would loop over.
 */
bool TestWaterPacking()
{
    Concord::ModelAsset asset{};
    Concord::ModelMaterial dry{};
    dry.name = "dry";
    Concord::ModelMaterial wet{};
    wet.name = "wet";
    wet.water = Concord::WaterMaterial{};
    wet.water->opacity = 0.4f;
    wet.water->ior = 1.2f;
    wet.water->absorptionDistance = 9.0f;
    wet.water->refraction = 0.5f;
    wet.water->waveAmplitude = 2.0f;
    wet.water->heading = {0.0f, 2.0f};
    wet.water->ripples[1] = Concord::WaterRipple{
        .centre = {3.0f, -4.0f}, .wavelength = 2.5f, .speed = 1.1f,
        .strength = 0.8f, .falloff = 0.3f};
    wet.water->ripples[3] = Concord::WaterRipple{
        .centre = {-1.0f, 2.0f}, .wavelength = 5.0f, .strength = 0.2f};
    asset.materials.push_back(dry);
    asset.materials.push_back(wet);
    asset.meshes.push_back(Concord::ModelMesh{.name = "first", .primitives = {Triangle(0.0f, 0)}});
    asset.meshes.push_back(Concord::ModelMesh{.name = "second", .primitives = {Triangle(1.0f, 1)}});

    Concord::VulkanModelUploadData upload{};
    if (!Concord::BuildVulkanModelUpload(asset, upload) || upload.materials.size() != 2) {
        return false;
    }
    const Concord::VulkanWaterMaterial& dryPacked = upload.materials[0].water;
    const Concord::VulkanModelMaterial& packed = upload.materials[1];
    if (upload.materials[0].emissive.w != 0.0f || dryPacked.surface.z != 0.0f ||
        packed.emissive.w != 1.0f) {
        return false;
    }
    return Near(packed.water.optics.x, 1.2f) && Near(packed.water.optics.y, 0.4f) &&
           Near(packed.water.optics.z, 9.0f) && Near(packed.water.optics.w, 0.5f) &&
           Near(packed.water.wave.x, 2.0f) &&
           // A heading is a direction, so it reaches the shader unit length.
           Near(packed.water.flow.x, 0.0f) && Near(packed.water.flow.y, 1.0f) &&
           Near(packed.water.surface.z, 2.0f) &&
           Near(packed.water.rippleShape[0].x, 3.0f) &&
           Near(packed.water.rippleShape[0].y, -4.0f) &&
           Near(packed.water.rippleShape[0].z, 2.5f) &&
           Near(packed.water.rippleShape[0].w, 0.8f) &&
           Near(packed.water.rippleMotion[0].x, 1.1f) &&
           Near(packed.water.rippleMotion[0].y, 0.3f) &&
           Near(packed.water.rippleShape[1].x, -1.0f) &&
           Near(packed.water.rippleShape[1].y, 2.0f) &&
           Near(packed.water.rippleShape[2].w, 0.0f);
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
    return TestFlattening() && TestWaterPacking() && TestRejectsInvalidAsset() ? 0 : 1;
}
