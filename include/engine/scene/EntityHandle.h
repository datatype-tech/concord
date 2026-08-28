// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ENTITYHANDLE_H
#define CONCORD_ENTITYHANDLE_H

#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"

#include <utility>

namespace Concord {

/**
 * Object-oriented grip on an entity.
 *
 * Spawn returns one of these so that the common cases read like method calls
 * on an object, while the underlying data stays in the World's component
 * arrays. It holds no state beyond the handle and a world pointer, so copying
 * it is free and it never owns the entity it names.
 */
class EntityHandle {
public:
    EntityHandle() = default;

    EntityHandle(World& world, Entity entity) noexcept : m_world(&world), m_entity(entity) {}

    /** The underlying handle, for code that prefers the data-oriented API. */
    [[nodiscard]] Entity Id() const noexcept { return m_entity; }

    /** Whether this handle still names a live entity. */
    [[nodiscard]] bool IsAlive() const noexcept
    {
        return m_world != nullptr && m_world->IsAlive(m_entity);
    }

    /** Attaches or replaces a component, returning this handle for chaining. */
    template <typename T, typename... Args>
    EntityHandle& Add(Args&&... args)
    {
        if (IsAlive()) {
            m_world->Add<T>(m_entity, std::forward<Args>(args)...);
        }
        return *this;
    }

    /** Returns a mutable component, or nullptr when absent. */
    template <typename T>
    [[nodiscard]] T* Get() noexcept
    {
        return IsAlive() ? m_world->Get<T>(m_entity) : nullptr;
    }

    /** Returns a read-only component, or nullptr when absent. */
    template <typename T>
    [[nodiscard]] const T* Get() const noexcept
    {
        const World* world = m_world;
        return world && world->IsAlive(m_entity) ? world->Get<T>(m_entity) : nullptr;
    }

    /** Attaches a component from a brace-initialized value. */
    template <typename T>
    EntityHandle& Add(T value)
    {
        if (IsAlive()) {
            m_world->Add<T>(m_entity, std::move(value));
        }
        return *this;
    }

    /** Whether the entity carries component `T`. */
    template <typename T>
    [[nodiscard]] bool Has() const noexcept
    {
        return m_world != nullptr && m_world->Has<T>(m_entity);
    }

    /** Detaches a component, returning this handle for chaining. */
    template <typename T>
    EntityHandle& Remove()
    {
        if (IsAlive()) {
            m_world->Remove<T>(m_entity);
        }
        return *this;
    }

    /** Destroys the entity and every component on it. */
    void Destroy()
    {
        if (m_world) {
            m_world->Destroy(m_entity);
        }
    }

    /** Implicit decay to the raw handle, so it drops into World APIs directly. */
    operator Entity() const noexcept { return m_entity; }

private:
    World* m_world = nullptr;
    Entity m_entity{};
};

} // namespace Concord

#endif // CONCORD_ENTITYHANDLE_H
