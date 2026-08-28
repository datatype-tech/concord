// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WORLD_INL
#define CONCORD_WORLD_INL

namespace Concord {

inline Entity World::Create()
{
    EnsureMutable();
    return m_entities.Create();
}

inline bool World::Destroy(Entity entity)
{
    EnsureMutable();
    if (!m_entities.IsAlive(entity)) {
        return false;
    }
    m_components.EraseAll(entity.index);
    return m_entities.Retire(entity);
}

inline bool World::IsAlive(Entity entity) const noexcept
{
    return m_entities.IsAlive(entity);
}

template <typename T, typename... Args>
T& World::Add(Entity entity, Args&&... args)
{
    EnsureMutable();
    if (!m_entities.IsAlive(entity)) {
        throw std::invalid_argument("cannot add a component to a stale entity");
    }
    return m_components.StorageFor<T>().Emplace(entity.index, std::forward<Args>(args)...);
}

template <typename T>
T* World::Get(Entity entity) noexcept
{
    if (!m_entities.IsAlive(entity)) {
        return nullptr;
    }
    ComponentStorage<T>* storage = m_components.Find<T>();
    return storage ? storage->Get(entity.index) : nullptr;
}

template <typename T>
const T* World::Get(Entity entity) const noexcept
{
    if (!m_entities.IsAlive(entity)) {
        return nullptr;
    }
    const ComponentStorage<T>* storage = m_components.Find<T>();
    return storage ? storage->Get(entity.index) : nullptr;
}

template <typename T>
bool World::Has(Entity entity) const noexcept
{
    if (!m_entities.IsAlive(entity)) {
        return false;
    }
    const ComponentStorage<T>* storage = m_components.Find<T>();
    return storage && storage->Contains(entity.index);
}

template <typename T>
void World::Remove(Entity entity)
{
    EnsureMutable();
    if (!m_entities.IsAlive(entity)) {
        return;
    }
    if (ComponentStorage<T>* storage = m_components.Find<T>()) {
        storage->Erase(entity.index);
    }
}

inline usize World::EntityCount() const noexcept
{
    return m_entities.Count();
}

template <typename T>
usize World::ComponentCount() const noexcept
{
    const ComponentStorage<T>* storage = m_components.Find<T>();
    return storage ? storage->Size() : 0;
}

} // namespace Concord

#endif // CONCORD_WORLD_INL
