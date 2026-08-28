// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSHADOWMAPINTERNAL_H
#define CONCORD_VULKANSHADOWMAPINTERNAL_H

#include "engine/render/vulkan/VulkanShadowMap.h"

namespace Concord {

/** Creates sampler and descriptor objects after the image view exists. */
bool CreateVulkanShadowMapDescriptors(const VulkanContext& context,
                                      VulkanShadowMap& shadowMap);

} // namespace Concord

#endif // CONCORD_VULKANSHADOWMAPINTERNAL_H
