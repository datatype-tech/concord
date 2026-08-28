// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ENTITY_H
#define CONCORD_ENTITY_H

#include "engine/core/Types.h"

namespace Concord {

/**
 * Stable handle to an entity.
 *
 * Pairs a slot index with a generation counter, so a handle kept across a
 * destroy-and-recreate cycle compares unequal to the slot's new occupant
 * instead of silently addressing it.
 */
struct Entity {
    u32 index = kInvalidIndex;
    u32 generation = 0;
    u64 worldId = 0;

    /** Sentinel index meaning "names no slot". */
    static constexpr u32 kInvalidIndex = 0xFFFFFFFFu;

    /** Whether this handle has a non-zero slot and World identity. */
    [[nodiscard]] bool IsValid() const noexcept
    {
        return index != kInvalidIndex && worldId != 0;
    }

    friend bool operator==(Entity, Entity) noexcept = default;
};

/** The handle every default-constructed Entity compares equal to. */
inline constexpr Entity kInvalidEntity{};

} // namespace Concord

#endif // CONCORD_ENTITY_H
