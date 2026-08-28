// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WORLD_H
#define CONCORD_WORLD_H

#include "engine/core/Types.h"
#include "engine/ecs/ComponentRegistry.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/EntityRegistry.h"
#include "engine/ecs/WorldDeferred.h"

#include <functional>
#include <stdexcept>
#include <utility>

namespace Concord {

/**
 * Single-threaded component database backing a Scene. Object and data APIs
 * address the same per-type sparse sets. Entity identity is stable for this
 * object's lifetime, so moving is disabled.
 */
class World {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    /** Allocates an entity, reusing a retired slot with a fresh generation. */
    Entity Create();
    /** Destroys an entity and every component attached to it. */
    bool Destroy(Entity entity);
    /** Whether the handle still names a live entity. */
    [[nodiscard]] bool IsAlive(Entity entity) const noexcept;

    /** Attaches or replaces component `T`; stale entities throw. */
    template <typename T, typename... Args>
    T& Add(Entity entity, Args&&... args);
    /** Returns component `T`, or nullptr when absent or stale. */
    template <typename T>
    [[nodiscard]] T* Get(Entity entity) noexcept;
    /** Returns component `T` as read-only, or nullptr when absent or stale. */
    template <typename T>
    [[nodiscard]] const T* Get(Entity entity) const noexcept;
    /** Whether `entity` carries component `T`. */
    template <typename T>
    [[nodiscard]] bool Has(Entity entity) const noexcept;
    /** Detaches component `T` from `entity`, if present. */
    template <typename T>
    void Remove(Entity entity);
    /**
     * Invokes `fn(Entity, T&, Rest&...)` for matching live entities. Use the
     * Defer* methods in a callback, then call FlushDeferred after the query.
     */
    template <typename T, typename... Rest, typename Fn>
    void Query(Fn&& fn);

    /** Const query exposing borrowed components as const references. */
    template <typename T, typename... Rest, typename Fn>
    void Query(Fn&& fn) const;

    /** Queues a command during Query (or an active flush) for later execution. */
    void Defer(std::function<void()> command);
    /** Queues destruction of an entity without capturing a callback parameter. */
    void DeferDestroy(Entity entity);
    /** Queues removal of a component from an entity. */
    template <typename T>
    void DeferRemove(Entity entity);
    /** Queues addition or replacement of a copied component value. */
    template <typename T, typename... Args>
    void DeferAdd(Entity entity, Args&&... args);
    /** Number of commands waiting for FlushDeferred. */
    [[nodiscard]] usize DeferredCount() const noexcept;
    /** Applies queued commands in insertion order; a failing command is discarded. */
    usize FlushDeferred();

    /** Number of live entities. */
    [[nodiscard]] usize EntityCount() const noexcept;

    /** Number of entities carrying component `T`. */
    template <typename T>
    [[nodiscard]] usize ComponentCount() const noexcept;

private:
    class QueryScope;

    void EnsureDeferredContext() const
    {
        if (m_queryDepth == 0 && !m_flushingDeferred) {
            throw std::logic_error("deferred mutation requires an active Query or flush");
        }
    }

    void EnsureMutable() const
    {
        if (m_queryDepth != 0) {
            throw std::logic_error("world mutation is forbidden during Query");
        }
    }

    mutable u32 m_queryDepth = 0;
    EntityRegistry m_entities;
    ComponentRegistry m_components;
    DeferredMutationQueue m_deferred;
    bool m_flushingDeferred = false;
};

} // namespace Concord

#include "engine/ecs/World.inl"
#include "engine/ecs/Query.inl"
#include "engine/ecs/WorldDeferred.inl"

#endif // CONCORD_WORLD_H
