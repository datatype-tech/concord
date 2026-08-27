// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRENDERBACKEND_H
#define CONCORD_VULKANRENDERBACKEND_H

#include "engine/render/IRenderBackend.h"

#include <memory>

namespace Concord {

/**
 * Native Vulkan backend targeting a Forward+ pipeline.
 *
 * The foundation stage brings up the instance, device, surface, swapchain
 * and per-frame synchronization, and clears to the scene's sky color; the
 * depth pre-pass, tiled light culling and forward shading land on top of
 * this same lifecycle.
 *
 * All Vulkan handles live in the pimpl so that `vulkan.h` never reaches a
 * public header (AGENTS.md §5).
 */
class VulkanRenderBackend final : public IRenderBackend {
public:
    VulkanRenderBackend();
    ~VulkanRenderBackend() override;

    VulkanRenderBackend(const VulkanRenderBackend&) = delete;
    VulkanRenderBackend& operator=(const VulkanRenderBackend&) = delete;

    bool Init(Window& window, bool enableValidation) override;
    void Shutdown() override;
    bool BeginFrame() override;
    void DrawScene(const Scene& scene) override;
    void EndFrame() override;
    void WaitIdle() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Concord

#endif // CONCORD_VULKANRENDERBACKEND_H
