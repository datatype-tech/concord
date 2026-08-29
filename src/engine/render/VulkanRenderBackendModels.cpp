// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendState.h"

#include "engine/render/vulkan/VulkanModelPipeline.h"
#include "engine/render/vulkan/VulkanShadowPipeline.h"
#include "engine/render/vulkan/VulkanSkinnedPipeline.h"

#include <cstdio>
#include <filesystem>

namespace Concord {

void VulkanRenderBackend::Impl::CreateModelPipelines()
{
    if (!frameData.IsReady()) {
        return;
    }
    const VkDescriptorSetLayout textureLayout = textureCache.DescriptorLayout();
    if (!CreateVulkanModelPipeline(context, swapchain.format, depth[0].format,
                                   frameData.layout, textureLayout, modelPipeline)) {
        std::fprintf(stderr,
                     "[Concord] model shader artifacts unavailable; imported models disabled\n");
    }
    if (!CreateVulkanSkinningResources(context, skinningResources)) {
        std::fprintf(stderr,
                     "[Concord] skinning palette resources unavailable; skinned models disabled\n");
    } else if (!CreateVulkanSkinnedPipeline(context, swapchain.format, depth[0].format,
                                            frameData.layout, skinningResources.layout,
                                            textureLayout, skinnedPipeline)) {
        std::fprintf(stderr,
                     "[Concord] skinned shader artifacts unavailable; skinned models disabled\n");
    } else if (!CreateVulkanSkinnedShadowPipeline(context, depth[0].format,
                                                   skinningResources.layout, shadowPipeline)) {
        std::fprintf(stderr,
                     "[Concord] skinned shadow shader unavailable; animated shadows disabled\n");
    }
}

bool VulkanRenderBackend::Impl::PrepareModelAssets(const RenderSceneSnapshot& snapshot,
                                                   bool& hasModelObjects,
                                                   bool& hasBoxObjects,
                                                   bool& hasStaticModelObjects,
                                                   bool& hasSkinnedModelObjects)
{
    hasModelObjects = false;
    hasBoxObjects = false;
    hasStaticModelObjects = false;
    hasSkinnedModelObjects = false;
    bool ready = true;
    for (const RenderObjectSnapshot& object : snapshot.objects) {
        hasBoxObjects = hasBoxObjects || object.shape == PrimitiveShape::Box;
        if (object.shape != PrimitiveShape::Model || !object.modelAsset) {
            continue;
        }
        hasModelObjects = true;
        hasSkinnedModelObjects = hasSkinnedModelObjects ||
                                 object.skinningRange.jointCount != 0;
        hasStaticModelObjects = hasStaticModelObjects ||
                                object.skinningRange.jointCount == 0;
        ready = modelAssets.Ensure(context, object.modelAsset) && ready;
        const std::filesystem::path baseDirectory =
            object.modelAsset->sourcePath.has_extension()
                ? object.modelAsset->sourcePath.parent_path()
                : object.modelAsset->sourcePath;
        for (const ModelMaterial& material : object.modelAsset->materials) {
            if (!material.baseColorTexture.empty() &&
                !textureCache.Ensure(context, material.baseColorTexture, baseDirectory)) {
                std::fprintf(stderr, "[Concord] base-color texture unavailable: %s\n",
                             material.baseColorTexture.c_str());
            }
        }
    }
    return ready;
}

bool VulkanRenderBackend::Impl::UploadSkinningFrame(const RenderSceneSnapshot& snapshot,
                                                    u32 frameIndex) noexcept
{
    if (snapshot.skinningPalette.jointMatrices.empty()) {
        return true;
    }
    return UploadVulkanSkinningPalette(skinningResources, frameIndex,
                                       snapshot.skinningPalette);
}

} // namespace Concord
