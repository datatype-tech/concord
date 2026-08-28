// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderFrameData.h"
#include "engine/render/vulkan/VulkanFrameDataResources.h"
#include "engine/render/vulkan/VulkanTileLightLimits.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace {

template <typename Handle>
Handle FakeHandle() noexcept
{
    if constexpr (std::is_pointer_v<Handle>) {
        return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(1));
    } else {
        return static_cast<Handle>(1);
    }
}

void MakeFrameResourcesUsable(Concord::VulkanFrameDataResources& resources,
                              VkDeviceSize tileSize)
{
    resources.layout = FakeHandle<VkDescriptorSetLayout>();
    resources.pool = FakeHandle<VkDescriptorPool>();
    static int mappedSentinel = 0;
    for (Concord::u32 i = 0; i < Concord::kMaxFramesInFlight; ++i) {
        resources.buffers[i].buffer = FakeHandle<VkBuffer>();
        resources.buffers[i].memory = FakeHandle<VkDeviceMemory>();
        resources.buffers[i].mapped = &mappedSentinel;
        resources.buffers[i].size = sizeof(Concord::RenderFrameData);
        resources.sets[i] = FakeHandle<VkDescriptorSet>();
        resources.tileBuffers[i].buffer = FakeHandle<VkBuffer>();
        resources.tileBuffers[i].memory = FakeHandle<VkDeviceMemory>();
        resources.tileBuffers[i].mapped = &mappedSentinel;
        resources.tileBuffers[i].size = tileSize;
    }
}

bool TestReadinessSemantics()
{
    Concord::VulkanFrameDataResources resources{};
    MakeFrameResourcesUsable(resources, Concord::kTileFallbackBufferBytes);
    if (!resources.IsReady() || resources.IsTileReady()) {
        return false;
    }
    for (Concord::u32 i = 0; i < Concord::kMaxFramesInFlight; ++i) {
        resources.tileBuffers[i].size = Concord::TileLightBufferBytes();
    }
    return resources.IsReady() && resources.IsTileReady();
}

} // namespace

int main()
{
    using namespace Concord;
    if (kTileSizePixels != 16 || kMaxTileColumns != 128 || kMaxTileRows != 128 ||
        kMaxLightsPerTile != 64 || TileLightBufferBytes() !=
            static_cast<usize>(kMaxTileCount) * (kTileHeaderBytes + 64 * sizeof(u32))) {
        return 1;
    }
    RenderFrameData frame{};
    if (frame.header.reserved != 0 || sizeof(RenderFrameHeaderData) != 16 ||
        offsetof(RenderFrameData, camera) != 16) {
        return 1;
    }
    frame.header.reserved = 1;
    return frame.header.reserved == 1 && TestReadinessSemantics() ? 0 : 1;
}
