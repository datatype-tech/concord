// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WORLDDEFERRED_INL
#define CONCORD_WORLDDEFERRED_INL

#include <type_traits>

namespace Concord {

inline void World::Defer(std::function<void()> command)
{
    EnsureDeferredContext();
    if (!command) {
        throw std::invalid_argument("deferred mutation command is empty");
    }
    m_deferred.Enqueue(std::move(command));
}

inline void World::DeferDestroy(Entity entity)
{
    Defer([this, entity] { Destroy(entity); });
}

template <typename T>
void World::DeferRemove(Entity entity)
{
    Defer([this, entity] { Remove<T>(entity); });
}

template <typename T, typename... Args>
void World::DeferAdd(Entity entity, Args&&... args)
{
    static_assert(std::is_copy_constructible_v<T> && std::is_assignable_v<T&, T>,
                  "deferred components must be copy constructible and assignable");
    EnsureDeferredContext();
    T value{std::forward<Args>(args)...};
    Defer([this, entity, value = std::move(value)]() mutable {
        Add<T>(entity, std::move(value));
    });
}

inline usize World::DeferredCount() const noexcept
{
    return m_deferred.Size();
}

inline usize World::FlushDeferred()
{
    EnsureMutable();
    if (m_flushingDeferred) {
        throw std::logic_error("deferred mutations are already being flushed");
    }
    m_flushingDeferred = true;
    usize applied = 0;
    try {
        while (!m_deferred.Empty()) {
            std::function<void()> command = std::move(m_deferred.Front());
            m_deferred.Pop();
            command();
            ++applied;
        }
    } catch (...) {
        m_flushingDeferred = false;
        throw;
    }
    m_flushingDeferred = false;
    return applied;
}

} // namespace Concord

#endif // CONCORD_WORLDDEFERRED_INL
