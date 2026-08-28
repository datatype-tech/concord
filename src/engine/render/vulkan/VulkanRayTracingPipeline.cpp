// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanRayTracingPipeline.h"

#include <limits>

namespace Concord {
namespace {

constexpr VkDeviceSize kMaxSize = std::numeric_limits<VkDeviceSize>::max();

bool AddSize(VkDeviceSize left, VkDeviceSize right, VkDeviceSize& result) noexcept
{
    if (right > kMaxSize - left) {
        return false;
    }
    result = left + right;
    return true;
}

bool MultiplySize(VkDeviceSize value, u32 count, VkDeviceSize& result) noexcept
{
    if (count != 0 && value > kMaxSize / count) {
        return false;
    }
    result = value * count;
    return true;
}

bool AlignSize(VkDeviceSize value, VkDeviceSize alignment, VkDeviceSize& result) noexcept
{
    if (alignment == 0) {
        return false;
    }
    const VkDeviceSize remainder = value % alignment;
    if (remainder == 0) {
        result = value;
        return true;
    }
    return AddSize(value, alignment - remainder, result);
}

bool RegionAddress(VkDeviceAddress base, VkDeviceSize offset, VkDeviceAddress& result) noexcept
{
    if (offset > kMaxSize - base) {
        return false;
    }
    result = base + offset;
    return true;
}

bool RegionFits(const VulkanRayTracingSbtRegion& region, VkDeviceSize total,
                VkDeviceSize alignment, bool singleRecord) noexcept
{
    VkDeviceSize end = 0;
    return region.IsReady() && region.offset % alignment == 0 &&
           (!singleRecord || region.size == region.stride) &&
           region.size % region.stride == 0 && AddSize(region.offset, region.size, end) &&
           end <= total;
}

bool LayoutIsValid(const VulkanRayTracingSbtLayout& layout) noexcept
{
    if (!layout.IsReady() || layout.raygen.offset != 0 ||
        !RegionFits(layout.raygen, layout.totalSize, layout.baseAlignment, true) ||
        !RegionFits(layout.miss, layout.totalSize, layout.baseAlignment, false) ||
        !RegionFits(layout.hit, layout.totalSize, layout.baseAlignment, false)) {
        return false;
    }
    VkDeviceSize raygenEnd = 0;
    VkDeviceSize missEnd = 0;
    return AddSize(layout.raygen.offset, layout.raygen.size, raygenEnd) &&
           AddSize(layout.miss.offset, layout.miss.size, missEnd) &&
           layout.miss.offset >= raygenEnd && layout.hit.offset >= missEnd;
}

} // namespace

VulkanRayTracingSbtLayout BuildVulkanRayTracingSbtLayout(
    const VulkanRayTracingSupport& support, u32 raygenRecordCount,
    u32 missRecordCount, u32 hitRecordCount) noexcept
{
    VulkanRayTracingSbtLayout result{};
    if (!support.IsUsable() || raygenRecordCount != 1 || missRecordCount == 0 ||
        hitRecordCount == 0) {
        return result;
    }
    VkDeviceSize stride = 0;
    if (!AlignSize(support.shaderGroupHandleSize, support.shaderGroupHandleAlignment, stride) ||
        (support.maxShaderGroupStride != 0 && stride > support.maxShaderGroupStride)) {
        return result;
    }
    VkDeviceSize missSize = 0;
    VkDeviceSize hitSize = 0;
    if (!MultiplySize(stride, missRecordCount, missSize) ||
        !MultiplySize(stride, hitRecordCount, hitSize)) {
        return result;
    }
    VkDeviceSize missOffset = 0;
    VkDeviceSize hitOffset = 0;
    VkDeviceSize end = stride;
    if (!AlignSize(end, support.shaderGroupBaseAlignment, missOffset) ||
        !AddSize(missOffset, missSize, end) ||
        !AlignSize(end, support.shaderGroupBaseAlignment, hitOffset) ||
        !AddSize(hitOffset, hitSize, end) ||
        !AlignSize(end, support.shaderGroupBaseAlignment, result.totalSize)) {
        return VulkanRayTracingSbtLayout{};
    }
    result.raygen = {0, stride, stride};
    result.miss = {missOffset, missSize, stride};
    result.hit = {hitOffset, hitSize, stride};
    result.baseAlignment = support.shaderGroupBaseAlignment;
    return result;
}

bool BuildVulkanRayTracingSbtRegions(
    VkDeviceAddress baseAddress, const VulkanRayTracingSbtLayout& layout,
    VkStridedDeviceAddressRegionKHR& raygen, VkStridedDeviceAddressRegionKHR& miss,
    VkStridedDeviceAddressRegionKHR& hit) noexcept
{
    raygen = {};
    miss = {};
    hit = {};
    if (baseAddress == 0 || !LayoutIsValid(layout) ||
        baseAddress % layout.baseAlignment != 0) {
        return false;
    }
    VkDeviceAddress raygenAddress = 0;
    VkDeviceAddress missAddress = 0;
    VkDeviceAddress hitAddress = 0;
    if (!RegionAddress(baseAddress, layout.raygen.offset, raygenAddress) ||
        !RegionAddress(baseAddress, layout.miss.offset, missAddress) ||
        !RegionAddress(baseAddress, layout.hit.offset, hitAddress)) {
        return false;
    }
    raygen = {raygenAddress, layout.raygen.stride, layout.raygen.size};
    miss = {missAddress, layout.miss.stride, layout.miss.size};
    hit = {hitAddress, layout.hit.stride, layout.hit.size};
    return true;
}

} // namespace Concord
