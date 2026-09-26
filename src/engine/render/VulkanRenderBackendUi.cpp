// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#include "engine/render/VulkanRenderBackend.h"
#include "engine/render/VulkanRenderBackendState.h"
#include "Concord/CUiToolkit.h"
#include <backends/imgui_impl_vulkan.h>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace Concord {
bool VulkanRenderBackend::InitializeUi(UiToolkit& ui)
{
    return CreateVulkanUiToolkit(m_impl->context,m_impl->swapchain.format,ui,m_impl->toolkit);
}
void VulkanRenderBackend::PrepareUiFrame()
{
    auto& impl=*m_impl;
    if (!impl.toolkit.initialized) return;
    impl.toolkit.ui->Activate();
    if (!RefreshVulkanUiFormat(impl.context,impl.swapchain.format,impl.toolkit)) {
        throw std::runtime_error("cannot rebuild native UI for the swapchain format");
    }
    auto& frame=impl.frames.Current();
    if (vkWaitForFences(impl.context.device,1,&frame.inFlight,VK_TRUE,std::numeric_limits<u64>::max()) != VK_SUCCESS ||
        !EnsureVulkanUiViewport(impl.context,impl.toolkit,impl.frames.currentFrame)) {
        throw std::runtime_error("cannot prepare editor viewport");
    }
    impl.toolkit.ui->SetSceneTexture(reinterpret_cast<u64>(impl.toolkit.viewports[impl.frames.currentFrame].texture));
    if (!UploadVulkanUiImages(impl.context,impl.toolkit)) {
        std::fprintf(stderr, "[Concord] one or more UI images could not be uploaded\n");
    }
    ImGui_ImplVulkan_NewFrame();
}
}
