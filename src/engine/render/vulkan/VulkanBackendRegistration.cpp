// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderBackendFactory.h"
#include "engine/render/VulkanRenderBackend.h"

#include "Concord/CExport.h"

namespace Concord {

namespace {

std::unique_ptr<IRenderBackend> CreateThisBackend()
{
    return std::make_unique<VulkanRenderBackend>();
}

/**
 * Registers the Vulkan backend the moment Render.dll's static
 * initializers run — on Windows, while the loader brings the DLL up,
 * before any application code executes. Linking against
 * `concord::render` (which `concord::concord` always does) is by itself
 * enough for `Game::AttachWindow` to find a renderer; nothing in
 * application code has to call anything to wire it up.
 */
struct AutoRegisterVulkanBackend {
    AutoRegisterVulkanBackend() { RegisterRenderBackend(&CreateThisBackend); }
} g_autoRegisterVulkanBackend;

} // namespace

} // namespace Concord

/**
 * Forces Render.dll into a consumer's import table so the Windows loader
 * actually loads it (and runs the static initializer above) at process
 * start, even though no application code ever calls a Render.dll symbol
 * directly. An unreferenced import library contributes nothing to a link
 * on Windows — unlike a Linux .so, listing it is not enough — so
 * concord/CMakeLists.txt force-resolves this exact symbol name against
 * every target that links `concord::render`. `extern "C"` keeps the
 * exported name stable and unmangled so that linker flag never has to
 * track a mangled C++ name.
 */
extern "C" CRENDER_API void ConcordRenderBackendLinkAnchor() {}
