// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_QUERY_INL
#define CONCORD_QUERY_INL

/**
 * World::Query's definition.
 *
 * Kept out of World.h so that the iteration strategy can be read and revised
 * on its own; it is included at the end of that header rather than compiled
 * separately because it is a template. Query callbacks must not immediately
 * change structure; they may queue DeferAdd/DeferRemove/DeferDestroy and flush
 * those commands after the outer query returns. Commands run at most once;
 * commands after a failure remain queued for a later flush.
 */

namespace Concord {

class World::QueryScope {
public:
    explicit QueryScope(const World& world) noexcept : m_world(world) { ++m_world.m_queryDepth; }
    ~QueryScope() { --m_world.m_queryDepth; }

private:
    const World& m_world;
};

template <typename T, typename... Rest, typename Fn>
void World::Query(Fn&& fn)
{
    QueryScope scope(*this);
    ComponentStorage<T>* primary = m_components.Find<T>();
    if (!primary) {
        return;
    }

    const std::vector<u32>& owners = primary->Owners();
    for (usize i = 0; i < owners.size(); ++i) {
        const u32 index = owners[i];
        if (!m_entities.IsSlotAlive(index)) {
            continue;
        }

        if constexpr (sizeof...(Rest) > 0) {
            const bool hasAll =
                ((m_components.Find<Rest>() != nullptr &&
                  m_components.Find<Rest>()->Contains(index)) && ...);
            if (!hasAll) {
                continue;
            }
        }

        fn(m_entities.HandleAt(index), primary->Dense()[i],
           *m_components.Find<Rest>()->Get(index)...);
    }
}

template <typename T, typename... Rest, typename Fn>
void World::Query(Fn&& fn) const
{
    QueryScope scope(*this);
    const ComponentStorage<T>* primary = m_components.Find<T>();
    if (!primary) {
        return;
    }

    const std::vector<u32>& owners = primary->Owners();
    for (usize i = 0; i < owners.size(); ++i) {
        const u32 index = owners[i];
        if (!m_entities.IsSlotAlive(index)) {
            continue;
        }

        if constexpr (sizeof...(Rest) > 0) {
            const bool hasAll =
                ((m_components.Find<Rest>() != nullptr &&
                  m_components.Find<Rest>()->Contains(index)) && ...);
            if (!hasAll) {
                continue;
            }
        }

        fn(m_entities.HandleAt(index), primary->Dense()[i],
           *m_components.Find<Rest>()->Get(index)...);
    }
}

} // namespace Concord

#endif // CONCORD_QUERY_INL
