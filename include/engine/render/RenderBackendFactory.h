// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RENDERBACKENDFACTORY_H
#define CONCORD_RENDERBACKENDFACTORY_H

#include "Concord/CExport.h"
#include "engine/render/IRenderBackend.h"

#include <memory>

namespace Concord {

/**
 * Slot the render backend DLL plugs itself into.
 *
 * Runtime.dll never names `VulkanRenderBackend` or includes anything under
 * `vulkan.h`: Game only ever asks this factory for whatever backend is
 * registered. Render.dll registers itself once, at load time (see
 * `VulkanBackendRegistration.cpp`), which is what lets the two DLLs depend
 * on each other in one direction only — Render.dll links against
 * Runtime.dll for `Window` access, never the reverse.
 */
using CreateRenderBackendFn = std::unique_ptr<IRenderBackend> (*)();

/** Called once by the render backend DLL when it loads. */
CENGINE_API void RegisterRenderBackend(CreateRenderBackendFn factory);

/**
 * Constructs the registered backend.
 *
 * @return An empty pointer when no render backend DLL was loaded — Game
 *         treats this the same as `GameConfig::enableRendering = false`.
 */
CENGINE_API std::unique_ptr<IRenderBackend> CreateRenderBackend();

} // namespace Concord

#endif // CONCORD_RENDERBACKENDFACTORY_H
