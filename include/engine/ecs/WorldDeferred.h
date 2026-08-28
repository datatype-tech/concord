// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WORLDDEFERRED_H
#define CONCORD_WORLDDEFERRED_H

#include "engine/core/Types.h"

#include <deque>
#include <functional>
#include <utility>

namespace Concord {

/** FIFO storage for structural commands collected during a World query. */
class DeferredMutationQueue {
public:
    /** Appends a callable without executing it. */
    template <typename Fn>
    void Enqueue(Fn&& command)
    {
        m_commands.emplace_back(std::forward<Fn>(command));
    }

    /** Whether no deferred commands are waiting. */
    [[nodiscard]] bool Empty() const noexcept { return m_commands.empty(); }

    /** Number of commands waiting to run. */
    [[nodiscard]] usize Size() const noexcept { return m_commands.size(); }

    /** Returns the oldest command without removing it. */
    [[nodiscard]] std::function<void()>& Front() noexcept { return m_commands.front(); }

    /** Removes the command that was just applied. */
    void Pop() noexcept { m_commands.pop_front(); }

private:
    std::deque<std::function<void()>> m_commands;
};

} // namespace Concord

#endif // CONCORD_WORLDDEFERRED_H
