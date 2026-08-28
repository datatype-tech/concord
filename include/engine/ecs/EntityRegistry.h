// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ENTITYREGISTRY_H
#define CONCORD_ENTITYREGISTRY_H

#include "engine/core/Types.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/WorldId.h"

#include <vector>

namespace Concord {

/**
 * Allocates entity slots and tracks which are live.
 *
 * Split out of World so that slot lifetime is testable and reviewable on its
 * own, independent of how components are stored. Its identity is non-movable.
 */
class EntityRegistry {
public:
    EntityRegistry() noexcept : m_worldId(AllocateWorldId()) {}

    EntityRegistry(const EntityRegistry&) = delete;
    EntityRegistry& operator=(const EntityRegistry&) = delete;
    EntityRegistry(EntityRegistry&&) = delete;
    EntityRegistry& operator=(EntityRegistry&&) = delete;

    /** Allocates a slot, reusing a retired one with a bumped generation. */
    Entity Create()
    {
        if (m_free.empty()) {
            const u32 index = static_cast<u32>(m_generations.size());
            try {
                m_generations.push_back(1);
                m_alive.push_back(true);
            } catch (...) {
                m_generations.resize(index);
                m_alive.resize(index);
                throw;
            }
            return Entity{index, 1, m_worldId};
        }

        const u32 index = m_free.back();
        m_free.pop_back();
        m_alive[index] = true;
        return Entity{index, m_generations[index], m_worldId};
    }

    /**
     * Retires a slot so its index can be reused.
     *
     * Bumping the generation is what makes a surviving handle to this entity
     * compare unequal to the slot's next occupant.
     *
     * @return False when the handle was already stale.
     */
    bool Retire(Entity entity)
    {
        if (!IsAlive(entity)) {
            return false;
        }
        m_free.push_back(entity.index);
        m_alive[entity.index] = false;
        ++m_generations[entity.index];
        return true;
    }

    /** Whether the handle still names a live entity. */
    [[nodiscard]] bool IsAlive(Entity entity) const noexcept
    {
        return entity.IsValid() && entity.worldId == m_worldId && entity.index < m_alive.size() &&
               m_alive[entity.index] &&
               m_generations[entity.index] == entity.generation;
    }

    /** Whether a slot index is live, for iteration that already holds one. */
    [[nodiscard]] bool IsSlotAlive(u32 index) const noexcept
    {
        return index < m_alive.size() && m_alive[index];
    }

    /** Rebuilds the handle naming a live slot. */
    [[nodiscard]] Entity HandleAt(u32 index) const noexcept
    {
        return Entity{index, m_generations[index], m_worldId};
    }

    /** Number of live entities. */
    [[nodiscard]] usize Count() const noexcept { return m_alive.size() - m_free.size(); }

private:
    std::vector<u32> m_generations;
    std::vector<bool> m_alive;
    std::vector<u32> m_free;
    u64 m_worldId = 0;
};

} // namespace Concord

#endif // CONCORD_ENTITYREGISTRY_H
