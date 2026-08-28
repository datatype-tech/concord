// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WORLDID_H
#define CONCORD_WORLDID_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

#include <atomic>

namespace Concord {

/** Allocates a process-wide identity for a component world. */
#if defined(CONCORD_SHARED)
CENGINE_API u64 AllocateWorldId() noexcept;
#else
inline u64 AllocateWorldId() noexcept
{
    static std::atomic<u64> nextWorldId{1};
    u64 id = nextWorldId.fetch_add(1, std::memory_order_relaxed);
    while (id == 0) {
        id = nextWorldId.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}
#endif

} // namespace Concord

#endif // CONCORD_WORLDID_H
