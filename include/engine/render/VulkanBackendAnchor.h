// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANBACKENDANCHOR_H
#define CONCORD_VULKANBACKENDANCHOR_H

#include "Concord/CExport.h"

namespace Concord {

/**
 * Registers the Vulkan backend with `RenderBackendFactory`.
 *
 * Runtime.dll never names `VulkanRenderBackend`, so nothing forces the
 * linker to pull Render.dll in or run any of its code. An application calls
 * this once, before constructing its first `Game`, to link the DLL in and
 * wire the registration explicitly — deliberately not a static initializer,
 * so load order stays something a reader can see rather than infer.
 */
CRENDER_API void LinkVulkanRenderBackend();

} // namespace Concord

#endif // CONCORD_VULKANBACKENDANCHOR_H
