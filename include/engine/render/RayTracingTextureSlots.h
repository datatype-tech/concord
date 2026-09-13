// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_RAYTRACINGTEXTURESLOTS_H
#define CONCORD_RAYTRACINGTEXTURESLOTS_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace Concord {

/**
 * Sampler slots the closest-hit shader can index.
 *
 * A fixed array rather than descriptor indexing: the engine already bounds
 * every other per-frame resource this way, and the cap keeps the descriptor
 * set a plain, portable binding instead of an opt-in device feature.
 */
inline constexpr u32 kMaxRayTracingTextureSlots = 64;

/**
 * Maps texture cache keys to the array indices the hit shader reads.
 *
 * Slot 0 is reserved for the cache's white fallback, so a primitive with no
 * texture — or one whose key could not be registered — always resolves to a
 * valid slot the renderer is guaranteed to have bound.
 */
struct RayTracingTextureSlots {
    /** Slot index to texture cache key; slot 0 stays empty as the fallback. */
    std::vector<std::string> keys;

    /** Slots in use, including the reserved fallback slot. */
    [[nodiscard]] usize Count() const noexcept { return keys.size(); }

    /** Whether the table has been armed with its fallback slot. */
    [[nodiscard]] bool IsReady() const noexcept { return !keys.empty(); }

    /**
     * Returns the slot owning \p key, registering it when there is room.
     *
     * An empty key, or one that would overflow the fixed capacity, resolves
     * to slot 0. Callers must treat that as "use the fallback", never as a
     * distinct texture.
     */
    [[nodiscard]] CRENDER_API u32 Acquire(std::string_view key);

    /** Returns the slot owning \p key, or 0 when it has none. */
    [[nodiscard]] CRENDER_API u32 Find(std::string_view key) const noexcept;
};

/** Arms a table with its reserved fallback slot and discards any entries. */
CRENDER_API void ResetRayTracingTextureSlots(RayTracingTextureSlots& slots) noexcept;

/** Returns the key bound to \p slot, or an empty view when out of range. */
[[nodiscard]] CRENDER_API std::string_view RayTracingTextureSlotKey(
    const RayTracingTextureSlots& slots, u32 slot) noexcept;

} // namespace Concord

#endif // CONCORD_RAYTRACINGTEXTURESLOTS_H
