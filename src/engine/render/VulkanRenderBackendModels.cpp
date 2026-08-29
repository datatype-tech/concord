// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanRenderBackendState.h"

#include "engine/render/vulkan/VulkanModelPipeline.h"
#include "engine/render/vulkan/VulkanSkinnedPipeline.h"

#include <cstdio>

namespace Concord {

void VulkanRenderBackend::Impl::CreateModelPipelines()
{
    if (!frameData.IsReady()) {
        return;
    }
    if (!CreateVulkanModelPipeline(context, swapchain.format, depth[0].format,
                                   frameData.layout, modelPipeline)) {
        std::fprintf(stderr,
                     "[Concord] model shader artifacts unavailable; imported models disabled\n");
    }
    if (!CreateVulkanSkinningResources(context, skinningResources)) {
        std::fprintf(stderr,
                     "[Concord] skinning palette resources unavailable; skinned models disabled\n");
    } else if (!CreateVulkanSkinnedPipeline(context, swapchain.format, depth[0].format,
                                            frameData.layout, skinningResources.layout,
                                            skinnedPipeline)) {
        std::fprintf(stderr,
                     "[Concord] skinned shader artifacts unavailable; skinned models disabled\n");
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
