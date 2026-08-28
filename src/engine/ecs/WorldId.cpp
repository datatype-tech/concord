// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/WorldId.h"

#include <atomic>

namespace Concord {

#if defined(CONCORD_SHARED)
namespace {

std::atomic<u64> g_nextWorldId{1};

} // namespace

u64 AllocateWorldId() noexcept
{
    u64 id = g_nextWorldId.fetch_add(1, std::memory_order_relaxed);
    while (id == 0) {
        id = g_nextWorldId.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}
#endif

} // namespace Concord
