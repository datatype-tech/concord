// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSURFACE_H
#define CONCORD_VULKANSURFACE_H

#include "engine/render/vulkan/VulkanContext.h"
#include "engine/window/Window.h"

namespace Concord {

/** Creates a Vulkan presentation surface for an open Window. */
bool CreateVulkanSurface(VulkanContext& context, const Window& window);

/**
 * Destroys the presentation surface and clears the handle.
 *
 * Creation uses the private WindowAccess bridge and remains inside Render.dll.
 */
void DestroyVulkanSurface(VulkanContext& context);

} // namespace Concord

#endif // CONCORD_VULKANSURFACE_H
