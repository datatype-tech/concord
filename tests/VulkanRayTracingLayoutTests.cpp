// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingPipeline.h"

#include <limits>

namespace {

Concord::VulkanRayTracingSupport MakeSupport() noexcept
{
    Concord::VulkanRayTracingSupport support{};
    support.supported = true;
    support.bufferDeviceAddress = true;
    support.accelerationStructure = true;
    support.rayTracingPipeline = true;
    support.deferredHostOperations = true;
    support.shaderGroupHandleSize = 24;
    support.shaderGroupHandleAlignment = 32;
    support.shaderGroupBaseAlignment = 64;
    support.maxShaderGroupStride = 256;
    support.maxRayRecursionDepth = 2;
    return support;
}

bool TestLayout() noexcept
{
    const auto layout = Concord::BuildVulkanRayTracingSbtLayout(MakeSupport(), 1, 2, 3);
    if (!layout.IsReady() || layout.raygen.offset != 0 || layout.raygen.size != 32 ||
        layout.miss.offset != 64 || layout.miss.size != 64 || layout.hit.offset != 128 ||
        layout.hit.size != 96 || layout.totalSize != 256) {
        return false;
    }
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    if (!Concord::BuildVulkanRayTracingSbtRegions(0x1000, layout, raygen, miss, hit) ||
        raygen.deviceAddress != 0x1000 || miss.deviceAddress != 0x1040 ||
        hit.deviceAddress != 0x1080 || raygen.stride != 32 || miss.size != 64 ||
        hit.size != 96) {
        return false;
    }
    return !Concord::BuildVulkanRayTracingSbtRegions(0x1001, layout, raygen, miss, hit);
}

bool TestInvalidInputs() noexcept
{
    const Concord::VulkanRayTracingSupport support = MakeSupport();
    if (Concord::BuildVulkanRayTracingSbtLayout({}, 1, 1, 1).IsReady() ||
        Concord::BuildVulkanRayTracingSbtLayout(support, 2, 1, 1).IsReady() ||
        Concord::BuildVulkanRayTracingSbtLayout(support, 1, 0, 1).IsReady()) {
        return false;
    }
    auto constrained = support;
    constrained.maxShaderGroupStride = 16;
    if (Concord::BuildVulkanRayTracingSbtLayout(constrained).IsReady()) {
        return false;
    }
    auto malformed = Concord::BuildVulkanRayTracingSbtLayout(support);
    malformed.hit.offset = malformed.miss.offset;
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    if (Concord::BuildVulkanRayTracingSbtRegions(0x1000, malformed, raygen, miss, hit)) {
        return false;
    }
    auto huge = support;
    huge.shaderGroupHandleSize = std::numeric_limits<Concord::u32>::max();
    huge.shaderGroupHandleAlignment = 1;
    huge.shaderGroupBaseAlignment = 1;
    huge.maxShaderGroupStride = std::numeric_limits<Concord::u32>::max();
    return !Concord::BuildVulkanRayTracingSbtLayout(
                 huge, 1, std::numeric_limits<Concord::u32>::max(), 2)
                  .IsReady();
}

} // namespace

int main()
{
    return TestLayout() && TestInvalidInputs() ? 0 : 1;
}
