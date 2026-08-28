// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WINDOWIMPL_H
#define CONCORD_WINDOWIMPL_H

#include "engine/window/Window.h"
#include "engine/window/WindowState.h"

namespace Concord {

/** Private pimpl definition shared by Window's implementation units. */
struct Window::Impl {
    WindowState state{};
};

} // namespace Concord

#endif // CONCORD_WINDOWIMPL_H
