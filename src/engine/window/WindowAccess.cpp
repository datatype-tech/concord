// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/window/WindowAccess.h"

#include "engine/window/Window.h"
#include "engine/window/WindowImpl.h"

namespace Concord {

void* WindowAccess::NativeHandle(const Window& window) noexcept
{
    return window.NativeHandle();
}

bool WindowAccess::ConsumeResizeFlag(Window& window) noexcept
{
    return window.ConsumeResizeFlag();
}

} // namespace Concord
