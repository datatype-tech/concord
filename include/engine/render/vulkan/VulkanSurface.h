// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANSURFACE_H
#define CONCORD_VULKANSURFACE_H

#include "engine/render/vulkan/VulkanContext.h"
#include "engine/window/Window.h"

namespace Concord {

/**
 * Destroys the presentation surface and clears the handle.
 *
 * The matching creation function is declared in `Window.h`: it needs
 * private access to the native handle, so that header befriends it.
 */
void DestroyVulkanSurface(VulkanContext& context);

} // namespace Concord

#endif // CONCORD_VULKANSURFACE_H
