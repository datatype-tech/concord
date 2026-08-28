// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSHADOWPIPELINEINTERNAL_H
#define CONCORD_VULKANSHADOWPIPELINEINTERNAL_H

#include "engine/core/Mat4.h"

#include <cstddef>

namespace Concord {

/** Push-constant values consumed by the directional shadow vertex shader. */
struct VulkanShadowPushConstants {
    Mat4 lightViewProjection{};
    Mat4 model{};
};

static_assert(sizeof(VulkanShadowPushConstants) == 128);
static_assert(offsetof(VulkanShadowPushConstants, model) == 64);

} // namespace Concord

#endif // CONCORD_VULKANSHADOWPIPELINEINTERNAL_H
