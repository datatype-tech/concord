// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RenderBackendFactory.h"

namespace Concord {

namespace {

CreateRenderBackendFn g_factory = nullptr;

} // namespace

void RegisterRenderBackend(CreateRenderBackendFn factory) { g_factory = factory; }

std::unique_ptr<IRenderBackend> CreateRenderBackend()
{
    return g_factory ? g_factory() : nullptr;
}

} // namespace Concord
