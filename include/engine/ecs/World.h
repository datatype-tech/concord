// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WORLD_H
#define CONCORD_WORLD_H

#include "engine/core/Types.h"
#include "engine/ecs/ComponentRegistry.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/EntityRegistry.h"

#include <utility>

namespace Concord {

/**
 * The component database backing a Scene.
 *
 * Entities are bare handles; all state lives in per-type sparse sets. The
 * object-oriented Spawn API and the data-oriented Query API are two views of
 * this one store, never two copies of the data.
 *
 * Single-threaded by design: the first generation guarded every operation
 * with a mutex, which cost more than it bought for a renderer that walks
 * components from one thread. Parallelism will arrive as an explicit
 * scheduler rather than per-call locks.
 */
class World {
public:
    World() = default;

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    /** Allocates an entity, reusing a retired slot with a fresh generation. */
    Entity Create() { return m_entities.Create(); }

    /**
     * Destroys an entity and every component attached to it.
     *
     * @return False when the handle was already stale.
     */
    bool Destroy(Entity entity)
    {
        if (!m_entities.IsAlive(entity)) {
            return false;
        }
        m_components.EraseAll(entity.index);
        return m_entities.Retire(entity);
    }

    /** Whether the handle still names a live entity. */
    [[nodiscard]] bool IsAlive(Entity entity) const noexcept { return m_entities.IsAlive(entity); }

    /**
     * Attaches component `T` to `entity`, replacing any existing one.
     *
     * @return Reference to the stored component, invalidated by the next
     *         structural change to this component type.
     */
    template <typename T, typename... Args>
    T& Add(Entity entity, Args&&... args)
    {
        return m_components.StorageFor<T>().Emplace(entity.index, std::forward<Args>(args)...);
    }

    /** Returns component `T` on `entity`, or nullptr when absent or stale. */
    template <typename T>
    [[nodiscard]] T* Get(Entity entity) noexcept
    {
        if (!m_entities.IsAlive(entity)) {
            return nullptr;
        }
        ComponentStorage<T>* storage = m_components.Find<T>();
        return storage ? storage->Get(entity.index) : nullptr;
    }

    template <typename T>
    [[nodiscard]] const T* Get(Entity entity) const noexcept
    {
        return const_cast<World*>(this)->Get<T>(entity);
    }

    /** Whether `entity` carries component `T`. */
    template <typename T>
    [[nodiscard]] bool Has(Entity entity) const noexcept
    {
        if (!m_entities.IsAlive(entity)) {
            return false;
        }
        const ComponentStorage<T>* storage = m_components.Find<T>();
        return storage && storage->Contains(entity.index);
    }

    /** Detaches component `T` from `entity`, if present. */
    template <typename T>
    void Remove(Entity entity)
    {
        if (ComponentStorage<T>* storage = m_components.Find<T>()) {
            storage->Erase(entity.index);
        }
    }

    /**
     * Invokes `fn(Entity, T&, Rest&...)` for every entity carrying all of
     * the named components.
     *
     * Iteration is driven by the first component type's dense array, so name
     * the rarest component first. The callback must not create or destroy
     * entities; collect them and act after the loop.
     */
    template <typename T, typename... Rest, typename Fn>
    void Query(Fn&& fn);

    /** Number of live entities. */
    [[nodiscard]] usize EntityCount() const noexcept { return m_entities.Count(); }

    /** Number of entities carrying component `T`. */
    template <typename T>
    [[nodiscard]] usize ComponentCount() const noexcept
    {
        const ComponentStorage<T>* storage = m_components.Find<T>();
        return storage ? storage->Size() : 0;
    }

private:
    EntityRegistry m_entities;
    ComponentRegistry m_components;
};

} // namespace Concord

#include "engine/ecs/Query.inl"

#endif // CONCORD_WORLD_H
