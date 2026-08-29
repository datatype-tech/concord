// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanShadowMap.h"
#include "engine/render/vulkan/VulkanShadowPipeline.h"
#include "engine/render/vulkan/VulkanShadowPipelineInternal.h"
#include "engine/render/vulkan/VulkanShadowMath.h"

#include <cmath>
#include <cstddef>

int main()
{
    using namespace Concord;
    if (kDirectionalShadowMapSize != 1024 || kDirectionalShadowMapBinding != 0 ||
        kDirectionalShadowMapSet != 1 ||
        sizeof(VulkanShadowPushConstants) != 128 ||
        offsetof(VulkanShadowPushConstants, model) != 64 ||
        sizeof(VulkanModelShadowPushConstants) != 128 ||
        offsetof(VulkanModelShadowPushConstants, model) != 64) {
        return 1;
    }

    VulkanShadowMap shadowMap{};
    VulkanShadowPipeline pipeline{};
    if (shadowMap.IsReady() || pipeline.IsReady() || pipeline.HasModel() ||
        shadowMap.extent.width != 0 ||
        shadowMap.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
        return 1;
    }
    RenderSceneSnapshot snapshot{};
    snapshot.hasCamera = true;
    snapshot.camera.target = {0.0f, 1.0f, 0.0f};
    snapshot.lights.push_back(RenderLightSnapshot{
        .light = LightComponent{.type = LightType::Directional, .intensity = 2.0f},
    });
    snapshot.objects.push_back(RenderObjectSnapshot{.model = Mat4::Translate({1.0f, 0.0f, 0.0f})});
    const VulkanDirectionalShadowState state = BuildVulkanDirectionalShadowState(snapshot);
    if (!state.enabled || state.lightIndex != 0 || !std::isfinite(state.viewProjection.col[0].x)) {
        return 1;
    }
    snapshot.lights[0].light.castShadow = false;
    return BuildVulkanDirectionalShadowState(snapshot).enabled ? 1 : 0;
}
