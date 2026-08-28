// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/SystemSchedule.h"

#include "engine/scene/Scene.h"

#include <exception>
#include <utility>

namespace Concord {

namespace {

/** Adapter letting a plain callable serve as an ISystem. */
class FunctionSystem final : public ISystem {
public:
    FunctionSystem(std::string name, std::function<void(Scene&, f32)> update)
        : m_name(std::move(name)), m_update(std::move(update))
    {
    }

    void OnUpdate(Scene& scene, f32 deltaTime) override
    {
        if (m_update) {
            m_update(scene, deltaTime);
        }
    }

    [[nodiscard]] const std::string& Name() const noexcept { return m_name; }

private:
    std::string m_name;
    std::function<void(Scene&, f32)> m_update;
};

} // namespace

void SystemSchedule::AddFunction(std::string name, std::function<void(Scene&, f32)> update)
{
    Add<FunctionSystem>(std::move(name), std::move(update));
}

void SystemSchedule::Start(Scene& scene)
{
    usize started = 0;
    try {
        for (; started < m_systems.size(); ++started) {
            m_systems[started]->OnStart(scene);
        }
    } catch (...) {
        const std::exception_ptr failure = std::current_exception();
        while (started > 0) {
            --started;
            try {
                m_systems[started]->OnStop(scene);
            } catch (...) {
            }
        }
        std::rethrow_exception(failure);
    }
}

void SystemSchedule::Update(Scene& scene, f32 deltaTime)
{
    for (auto& system : m_systems) {
        system->OnUpdate(scene, deltaTime);
    }
}

void SystemSchedule::Stop(Scene& scene)
{
    std::exception_ptr failure;
    for (auto it = m_systems.rbegin(); it != m_systems.rend(); ++it) {
        try {
            (*it)->OnStop(scene);
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

} // namespace Concord
