// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SYSTEMSCHEDULE_H
#define CONCORD_SYSTEMSCHEDULE_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"
#include "engine/ecs/System.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Concord {

class Scene;

/**
 * Ordered list of systems a Game ticks each frame.
 *
 * Execution order is registration order, which keeps the model predictable;
 * an explicit dependency graph can be layered on later without changing the
 * calling convention.
 */
class CENGINE_API SystemSchedule {
public:
    SystemSchedule() = default;

    SystemSchedule(const SystemSchedule&) = delete;
    SystemSchedule& operator=(const SystemSchedule&) = delete;
    SystemSchedule(SystemSchedule&&) noexcept = default;
    SystemSchedule& operator=(SystemSchedule&&) noexcept = default;

    /**
     * Constructs and registers a system of type `T`.
     *
     * @return Reference to the stored system, valid until this schedule is
     *         destroyed.
     */
    template <typename T, typename... Args>
    T& Add(Args&&... args)
    {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;
        m_systems.push_back(std::move(system));
        return ref;
    }

    /**
     * Registers a plain function as a system.
     *
     * The lightweight path for logic that needs no state of its own.
     */
    void AddFunction(std::string name, std::function<void(Scene&, f32)> update);

    /**
     * Runs every system's OnStart, in registration order.
     *
     * If a start hook throws, already-started systems receive OnStop in
     * reverse order before the exception is propagated.
     */
    void Start(Scene& scene);

    /** Runs every system's OnUpdate, in registration order. */
    void Update(Scene& scene, f32 deltaTime);

    /**
     * Runs every system's OnStop, in reverse registration order.
     *
     * All hooks are attempted; the first exception is rethrown afterwards.
     */
    void Stop(Scene& scene);

    /** Number of registered systems. */
    [[nodiscard]] usize Count() const noexcept { return m_systems.size(); }

private:
    std::vector<std::unique_ptr<ISystem>> m_systems;
};

} // namespace Concord

#endif // CONCORD_SYSTEMSCHEDULE_H
