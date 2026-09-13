// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANRIPPLESOURCE_H
#define CONCORD_VULKANRIPPLESOURCE_H

#include "engine/core/Vec4.h"

#include <cstddef>
#include <type_traits>

namespace Concord {

/**
 * std430-compatible water disturbance, read by the model closest-hit shader.
 *
 * Field for field the authored ripple a water body carries, so a splash spawned
 * at runtime and one placed by hand reach the shader identically. Two vectors
 * rather than one, because the shape is what the ring is and the motion is how
 * it travels, and the two are read at different points in the wave sum.
 */
struct alignas(16) VulkanRippleSource {
    /** xy centre on the surface, z wavelength, w strength (already faded). */
    Vec4 shape{};
    /** x speed, y falloff, z reach radius past which it contributes nothing. */
    Vec4 motion{};
};

static_assert(sizeof(VulkanRippleSource) == sizeof(Vec4) * 2);
static_assert(alignof(VulkanRippleSource) == 16);
static_assert(std::is_standard_layout_v<VulkanRippleSource> &&
              std::is_trivially_copyable_v<VulkanRippleSource>);
static_assert(offsetof(VulkanRippleSource, shape) == 0);
static_assert(offsetof(VulkanRippleSource, motion) == sizeof(Vec4));

} // namespace Concord

#endif // CONCORD_VULKANRIPPLESOURCE_H
