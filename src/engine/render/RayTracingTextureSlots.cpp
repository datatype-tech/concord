// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/RayTracingTextureSlots.h"

namespace Concord {

void ResetRayTracingTextureSlots(RayTracingTextureSlots& slots) noexcept
{
    slots.keys.clear();
    try {
        slots.keys.emplace_back();
    } catch (...) {
        // An unarmed table reports IsReady() false, and every lookup then
        // resolves to the fallback slot the caller binds unconditionally.
    }
}

u32 RayTracingTextureSlots::Find(std::string_view key) const noexcept
{
    if (!IsReady() || key.empty()) {
        return 0;
    }
    for (usize index = 1; index < keys.size(); ++index) {
        if (keys[index] == key) {
            return static_cast<u32>(index);
        }
    }
    return 0;
}

u32 RayTracingTextureSlots::Acquire(std::string_view key)
{
    const u32 existing = Find(key);
    if (existing != 0 || key.empty() || !IsReady()) {
        return existing;
    }
    if (keys.size() >= kMaxRayTracingTextureSlots) {
        return 0;
    }
    try {
        keys.emplace_back(key);
    } catch (...) {
        return 0;
    }
    return static_cast<u32>(keys.size() - 1);
}

std::string_view RayTracingTextureSlotKey(const RayTracingTextureSlots& slots,
                                          u32 slot) noexcept
{
    return slot < slots.keys.size() ? std::string_view(slots.keys[slot]) : std::string_view{};
}

} // namespace Concord
