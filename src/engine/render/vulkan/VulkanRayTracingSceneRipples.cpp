// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingSceneInternal.h"

#include <array>
#include <span>

namespace Concord {

bool UploadVulkanRayTracingRipples(VulkanRayTracingScene& scene,
                                   const RenderSceneSnapshot* snapshot) noexcept
{
    if (snapshot == nullptr) {
        return true;
    }
    if (scene.rippleSourceBuffer.buffer == VK_NULL_HANDLE) {
        return false;
    }
    std::array<VulkanRippleSource, kMaxRenderRipples> sources{};
    const usize count = std::min<usize>(snapshot->ripples.size(), kMaxRenderRipples);
    for (usize index = 0; index < count; ++index) {
        const RenderRippleSnapshot& ripple = snapshot->ripples[index];
        sources[index].shape = {ripple.centre.x, ripple.centre.y, ripple.wavelength,
                                ripple.strength};
        // w carries the ring's age, which is what places its wavefront. A
        // negative value means an authored source that repeats on a period
        // instead of one that was dropped at an instant.
        sources[index].motion = {ripple.speed, ripple.falloff, ripple.reach, ripple.age};
    }
    // The whole capacity is written every frame rather than just the live
    // prefix, so a frame with fewer disturbances cannot leave the previous
    // frame's rings standing on the water.
    return UploadVulkanBuffer(scene.rippleSourceBuffer,
                              std::as_bytes(std::span(sources.data(), sources.size())));
}

} // namespace Concord
