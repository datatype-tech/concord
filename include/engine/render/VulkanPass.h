// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANPASS_H
#define CONCORD_VULKANPASS_H

#include "Concord/CExport.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Concord {

/** Position at which a user pass is called inside the backend lifecycle. */
enum class VulkanPassPhase : std::uint32_t {
    Initialize = 0,
    BeforeScene = 1,
    AfterScene = 2,
    Shutdown = 3,
};

/** ABI version written to VulkanPassContext::version by the engine. */
inline constexpr std::uint32_t VulkanPassAbiVersion = 1u;
/** Sentinel used for imageIndex when no swapchain image is acquired. */
inline constexpr std::uint32_t VulkanPassInvalidImageIndex = UINT32_MAX;

/**
 * Opaque Vulkan frame state exposed to an explicitly registered user pass.
 *
 * Handles use a fixed 64-bit opaque representation; the header
 * deliberately includes no Vulkan declarations, so normal Concord consumers
 * remain independent of the SDK. A callback that needs native types includes
 * `<vulkan/vulkan.h>` in its own translation unit and converts these fields.
 * Callbacks should check `version` and `structSize` before reading fields
 * added by a future ABI revision. `featureFlags` reports device capabilities
 * plus per-invocation availability; a ray-query capability bit alone does not
 * guarantee that a TLAS is supplied. During Initialize and Shutdown,
 * `imageIndex` is `VulkanPassInvalidImageIndex` and all command, attachment,
 * descriptor, and acceleration-structure handles are zero; those phases are
 * lifecycle notifications only.
 */
struct VulkanPassContext {
    std::uint32_t structSize = 0;
    std::uint32_t version = VulkanPassAbiVersion;
    VulkanPassPhase phase = VulkanPassPhase::AfterScene;
    std::uint32_t frameIndex = 0;
    std::uint32_t imageIndex = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t colorFormat = 0;
    std::uint32_t depthFormat = 0;
    std::uint32_t graphicsQueueFamily = 0;
    std::uint32_t swapchainImageCount = 0;
    std::uint64_t swapchainGeneration = 0;
    std::uint32_t featureFlags = 0;
    std::uint32_t commandBufferRecording = 0;
    std::uint64_t instance = 0;
    std::uint64_t physicalDevice = 0;
    std::uint64_t device = 0;
    std::uint64_t graphicsQueue = 0;
    std::uint64_t commandBuffer = 0;
    std::uint64_t colorImage = 0;
    std::uint64_t colorView = 0;
    std::uint64_t swapchain = 0;
    std::uint64_t depthImage = 0;
    std::uint64_t depthView = 0;
    std::uint64_t frameDescriptorSet = 0;
    std::uint64_t shadowDescriptorSet = 0;
    std::uint64_t rayTracingDescriptorSet = 0;
    std::uint64_t topLevelAccelerationStructure = 0;
    std::uint32_t colorLayout = 0;
    std::uint32_t depthLayout = 0;
};

static_assert(std::is_standard_layout_v<VulkanPassContext>);
static_assert(std::is_trivially_copyable_v<VulkanPassContext>);

/** Feature bits reported in VulkanPassContext::featureFlags. */
inline constexpr std::uint32_t VulkanPassFeatureRayQuery = 1u << 0;
inline constexpr std::uint32_t VulkanPassFeatureRayTracingPipeline = 1u << 1;
inline constexpr std::uint32_t VulkanPassFeatureCommandRecording = 1u << 2;
/** Set only when this invocation supplies a valid TLAS/RT descriptor set. */
inline constexpr std::uint32_t VulkanPassFeatureRayTracingScene = 1u << 3;

/** Converts an opaque context handle to the caller's Vulkan handle type. */
template <typename T>
[[nodiscard]] T VulkanHandle(std::uint64_t value) noexcept
{
    if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(value));
    } else {
        return static_cast<T>(value);
    }
}

/**
 * Callback invoked during backend initialization, frame command recording, or
 * shutdown. Only BeforeScene and AfterScene have command recording enabled;
 * Initialize and Shutdown are lifecycle notifications and must not issue
 * commands on the engine-owned command buffer. BeforeScene runs after the
 * color/depth attachment transitions and before Concord's scene passes;
 * AfterScene runs after them and before the present transition. A callback
 * returning false is logged and isolated; it does not abort the frame. A
 * callback must not end or submit the engine command buffer, or change image
 * layouts behind Concord's resource tracking.
 */
using VulkanPassCallback = bool (*)(const VulkanPassContext& context,
                                    void* userData);

/** Description used to register one deterministic custom Vulkan pass. */
struct VulkanPassDesc {
    const char* name = nullptr;
    VulkanPassPhase phase = VulkanPassPhase::AfterScene;
    std::uint32_t order = 0;
    VulkanPassCallback callback = nullptr;
    void* userData = nullptr;
};

/** Registers a pass; names are non-empty and case-insensitively unique. */
CENGINE_API bool RegisterVulkanPass(const VulkanPassDesc& description) noexcept;

/** Removes a previously registered pass by name. */
CENGINE_API bool UnregisterVulkanPass(const char* name) noexcept;

/** Removes every registered pass; useful during application teardown/tests. */
CENGINE_API void ClearVulkanPasses() noexcept;

/**
 * Invoked by the Render DLL. Application code normally does not call this.
 * A false result means at least one callback returned false or threw; other
 * registered callbacks are still run so one optional pass cannot starve the
 * rest of the frame.
 */
CENGINE_API bool RunVulkanPasses(VulkanPassPhase phase,
                                 const VulkanPassContext& context) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANPASS_H
