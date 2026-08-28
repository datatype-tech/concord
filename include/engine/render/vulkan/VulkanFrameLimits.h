// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_VULKANFRAMELIMITS_H
#define CONCORD_VULKANFRAMELIMITS_H

#include "engine/core/Types.h"

namespace Concord {

/** Number of frame slots shared by synchronization and GPU frame data. */
inline constexpr u32 kMaxFramesInFlight = 2;

} // namespace Concord

#endif // CONCORD_VULKANFRAMELIMITS_H
