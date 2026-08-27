// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RESOLUTION_H
#define CONCORD_RESOLUTION_H

#include "engine/core/Types.h"

namespace Concord {

/** Client-area size in pixels. */
struct Resolution {
    u32 width = 1280;
    u32 height = 720;
};

} // namespace Concord

#endif // CONCORD_RESOLUTION_H
