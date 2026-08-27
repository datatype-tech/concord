// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_COMPONENTREGISTRY_H
#define CONCORD_COMPONENTREGISTRY_H

#include "engine/ecs/ComponentStorage.h"

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace Concord {

/**
 * Owns one sparse set per component type, created on first use.
 *
 * Type erasure through `IComponentStorage` is what lets an entity be
 * destroyed without the caller naming every component it might carry.
 */
class ComponentRegistry {
public:
    /** Returns the storage for `T`, creating it when this is the first use. */
    template <typename T>
    ComponentStorage<T>& StorageFor()
    {
        const std::type_index key(typeid(T));
        auto it = m_storages.find(key);
        if (it == m_storages.end()) {
            it = m_storages.emplace(key, std::make_unique<ComponentStorage<T>>()).first;
        }
        return *static_cast<ComponentStorage<T>*>(it->second.get());
    }

    /** Returns the storage for `T`, or nullptr when nothing ever used it. */
    template <typename T>
    [[nodiscard]] ComponentStorage<T>* Find() noexcept
    {
        const auto it = m_storages.find(std::type_index(typeid(T)));
        return it == m_storages.end() ? nullptr
                                      : static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template <typename T>
    [[nodiscard]] const ComponentStorage<T>* Find() const noexcept
    {
        return const_cast<ComponentRegistry*>(this)->Find<T>();
    }

    /** Erases every component belonging to a slot, whatever its type. */
    void EraseAll(u32 index)
    {
        for (auto& [type, storage] : m_storages) {
            storage->Erase(index);
        }
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_storages;
};

} // namespace Concord

#endif // CONCORD_COMPONENTREGISTRY_H
