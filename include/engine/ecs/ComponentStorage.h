// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_COMPONENTSTORAGE_H
#define CONCORD_COMPONENTSTORAGE_H

#include "engine/core/Types.h"
#include "engine/ecs/Entity.h"

#include <utility>
#include <vector>

namespace Concord {

/**
 * Type-erased base so a World can destroy components of any type without
 * knowing what they are.
 */
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    /** Removes the component belonging to `index`, if any. */
    virtual void Erase(u32 index) = 0;

    /** Whether `index` currently owns a component here. */
    [[nodiscard]] virtual bool Contains(u32 index) const noexcept = 0;
};

/**
 * Sparse-set storage for one component type.
 *
 * Components live in a densely packed array so iteration is cache-friendly,
 * while a sparse index array maps an entity slot to its dense position.
 * Erasing swaps the last element into the hole, which keeps the dense array
 * contiguous at the cost of not preserving insertion order.
 */
template <typename T>
class ComponentStorage final : public IComponentStorage {
public:
    /** Adds or replaces the component owned by `index`. */
    template <typename... Args>
    T& Emplace(u32 index, Args&&... args)
    {
        if (index < m_sparse.size() && m_sparse[index] != kEmpty) {
            m_dense[m_sparse[index]] = T{std::forward<Args>(args)...};
            return m_dense[m_sparse[index]];
        }

        if (index >= m_sparse.size()) {
            m_sparse.resize(index + 1, kEmpty);
        }

        m_sparse[index] = static_cast<u32>(m_dense.size());
        m_dense.push_back(T{std::forward<Args>(args)...});
        m_owners.push_back(index);
        return m_dense.back();
    }

    /** Returns the component owned by `index`, or nullptr when absent. */
    [[nodiscard]] T* Get(u32 index) noexcept
    {
        if (index >= m_sparse.size() || m_sparse[index] == kEmpty) {
            return nullptr;
        }
        return &m_dense[m_sparse[index]];
    }

    [[nodiscard]] const T* Get(u32 index) const noexcept
    {
        return const_cast<ComponentStorage*>(this)->Get(index);
    }

    void Erase(u32 index) override
    {
        if (index >= m_sparse.size() || m_sparse[index] == kEmpty) {
            return;
        }

        const u32 dense = m_sparse[index];
        const u32 last = static_cast<u32>(m_dense.size() - 1);

        if (dense != last) {
            m_dense[dense] = std::move(m_dense[last]);
            m_owners[dense] = m_owners[last];
            m_sparse[m_owners[dense]] = dense;
        }

        m_dense.pop_back();
        m_owners.pop_back();
        m_sparse[index] = kEmpty;
    }

    [[nodiscard]] bool Contains(u32 index) const noexcept override
    {
        return index < m_sparse.size() && m_sparse[index] != kEmpty;
    }

    /** The packed components, for direct iteration. */
    [[nodiscard]] std::vector<T>& Dense() noexcept { return m_dense; }
    [[nodiscard]] const std::vector<T>& Dense() const noexcept { return m_dense; }

    /** Entity slot owning each packed component, parallel to Dense(). */
    [[nodiscard]] const std::vector<u32>& Owners() const noexcept { return m_owners; }

    [[nodiscard]] usize Size() const noexcept { return m_dense.size(); }

private:
    /** Sparse slot value meaning "this entity owns no component here". */
    static constexpr u32 kEmpty = 0xFFFFFFFFu;

    std::vector<u32> m_sparse;
    std::vector<u32> m_owners;
    std::vector<T> m_dense;
};

} // namespace Concord

#endif // CONCORD_COMPONENTSTORAGE_H
