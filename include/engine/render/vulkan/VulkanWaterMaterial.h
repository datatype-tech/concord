// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANWATERMATERIAL_H
#define CONCORD_VULKANWATERMATERIAL_H

#include "engine/asset/WaterMaterial.h"
#include "engine/core/Vec4.h"

#include <array>
#include <cstddef>
#include <type_traits>

namespace Concord {

/**
 * std430-compatible water block, read by the model closest-hit shader.
 *
 * Deliberately dependency-free -- it pulls in no `vulkan.h` -- because both the
 * raster material and the ray-tracing primitive metadata embed it, and the
 * ray-tracing layout is compiled into CPU tests that never open a device. Every
 * field is a `vec4` so the struct needs no layout reasoning beyond "four
 * floats, sixteen-byte aligned", and the shader mirror is a field-for-field
 * copy of this declaration.
 *
 * The ripple arrays are split into shape and motion rather than interleaved so
 * each keeps a uniform stride: std430 gives an array of `vec4` a sixteen-byte
 * stride, which is exactly what an array of `Vec4` already has.
 */
struct alignas(16) VulkanWaterMaterial {
    /** x index of refraction, y opacity, z absorption distance, w refraction strength. */
    Vec4 optics{};
    /** Per-channel extinction of light travelling through the volume, and the scattering strength in w. */
    Vec4 absorbance{};
    /** x amplitude, y wavelength scale, z speed, w choppiness. */
    Vec4 wave{};
    /** xy unit heading, z octave spread in radians, w drift in radians per second. */
    Vec4 flow{};
    /** x foam threshold, y foam intensity, z active ripple count, w fall. */
    Vec4 surface{};
    /** Per active source: xy centre, z wavelength, w strength. */
    std::array<Vec4, kMaxWaterRipples> rippleShape{};
    /** Per active source: x speed, y falloff, z reach radius. */
    std::array<Vec4, kMaxWaterRipples> rippleMotion{};
};

static_assert(sizeof(VulkanWaterMaterial) == sizeof(Vec4) * 13);
static_assert(alignof(VulkanWaterMaterial) == 16);
static_assert(std::is_standard_layout_v<VulkanWaterMaterial> &&
              std::is_trivially_copyable_v<VulkanWaterMaterial>);
static_assert(offsetof(VulkanWaterMaterial, optics) == 0);
static_assert(offsetof(VulkanWaterMaterial, absorbance) == sizeof(Vec4));
static_assert(offsetof(VulkanWaterMaterial, wave) == sizeof(Vec4) * 2);
static_assert(offsetof(VulkanWaterMaterial, flow) == sizeof(Vec4) * 3);
static_assert(offsetof(VulkanWaterMaterial, surface) == sizeof(Vec4) * 4);
static_assert(offsetof(VulkanWaterMaterial, rippleShape) == sizeof(Vec4) * 5);
static_assert(offsetof(VulkanWaterMaterial, rippleMotion) == sizeof(Vec4) * 9);

} // namespace Concord

#endif // CONCORD_VULKANWATERMATERIAL_H
