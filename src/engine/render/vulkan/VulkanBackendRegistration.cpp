// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderBackendFactory.h"
#include "engine/render/VulkanBackendAnchor.h"
#include "engine/render/VulkanRenderBackend.h"

namespace Concord {

namespace {

std::unique_ptr<IRenderBackend> CreateThisBackend()
{
    return std::make_unique<VulkanRenderBackend>();
}

} // namespace

void LinkVulkanRenderBackend() { RegisterRenderBackend(&CreateThisBackend); }

} // namespace Concord
