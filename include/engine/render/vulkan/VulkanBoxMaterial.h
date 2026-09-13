// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANBOXMATERIAL_H
#define CONCORD_VULKANBOXMATERIAL_H

#include "engine/core/Vec4.h"

#include <cstddef>
#include <type_traits>

namespace Concord {

struct Material;

/**
 * Marks a Box instance's custom index as carrying a material slot.
 *
 * A Box instance's custom index is otherwise free, so the low bits name a slot
 * in the box material SSBO and this bit says the slot is meaningful. Instances
 * placed before any material exists -- the stand-in geometry the acceleration
 * structure is built from when a scene has no objects at all -- leave the bit
 * clear and keep shading from the fallback palette.
 *
 * A model instance is identified by its own, higher bit first, so the two
 * encodings never overlap.
 */
inline constexpr u32 kVulkanRayTracingBoxMaterialBit = 1u << 22;
inline constexpr u32 kVulkanRayTracingBoxMaterialMask = kVulkanRayTracingBoxMaterialBit - 1u;

/**
 * std430-compatible box material, read by the closest-hit shader.
 *
 * Field for field the pair of push-constant vectors the raster box path uses,
 * so one authored `Material` reaches the screen identically whichever path
 * draws it. `MakeVulkanBoxMaterial` is the single conversion both go through.
 */
struct alignas(16) VulkanBoxMaterial {
    /** Linear base colour in rgb, authored opacity in w. */
    Vec4 albedo{};
    /** x metallic, y roughness, z emissive, w unused. */
    Vec4 surface{};
};

static_assert(sizeof(VulkanBoxMaterial) == sizeof(Vec4) * 2);
static_assert(alignof(VulkanBoxMaterial) == 16);
static_assert(std::is_standard_layout_v<VulkanBoxMaterial> &&
              std::is_trivially_copyable_v<VulkanBoxMaterial>);
static_assert(offsetof(VulkanBoxMaterial, albedo) == 0);
static_assert(offsetof(VulkanBoxMaterial, surface) == sizeof(Vec4));

/**
 * Converts an authored material into the values both box paths upload.
 *
 * Out-of-range and non-finite inputs are clamped here rather than at each call
 * site: a NaN metallic reaching either shader blanks the surface, and the two
 * paths disagreeing about a clamp is a difference nobody would think to look
 * for.
 */
[[nodiscard]] VulkanBoxMaterial MakeVulkanBoxMaterial(const Material& material) noexcept;

} // namespace Concord

#endif // CONCORD_VULKANBOXMATERIAL_H
