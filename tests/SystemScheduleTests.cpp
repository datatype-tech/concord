// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/SystemSchedule.h"
#include "engine/scene/Scene.h"

#include <stdexcept>
#include <vector>

namespace {

class ProbeSystem final : public Concord::ISystem {
public:
    ProbeSystem(std::vector<int>& calls, int id) : m_calls(calls), m_id(id) {}

    void OnStart(Concord::Scene&) override { m_calls.push_back(m_id); }
    void OnUpdate(Concord::Scene&, Concord::f32) override { m_calls.push_back(m_id * 10); }
    void OnStop(Concord::Scene&) override { m_calls.push_back(m_id * 100); }

private:
    std::vector<int>& m_calls;
    int m_id = 0;
};

class ThrowingStartSystem final : public Concord::ISystem {
public:
    explicit ThrowingStartSystem(std::vector<int>& calls) : m_calls(calls) {}

    void OnStart(Concord::Scene&) override
    {
        m_calls.push_back(3);
        throw std::runtime_error("start failed");
    }

    void OnUpdate(Concord::Scene&, Concord::f32) override {}

    void OnStop(Concord::Scene&) override { m_calls.push_back(-3); }

private:
    std::vector<int>& m_calls;
};

class ThrowingStopSystem final : public Concord::ISystem {
public:
    explicit ThrowingStopSystem(std::vector<int>& calls) : m_calls(calls) {}

    void OnUpdate(Concord::Scene&, Concord::f32) override {}

    void OnStop(Concord::Scene&) override
    {
        m_calls.push_back(-4);
        throw std::runtime_error("stop failed");
    }

private:
    std::vector<int>& m_calls;
};

} // namespace

int main()
{
    Concord::Scene scene;
    Concord::SystemSchedule systems;
    std::vector<int> calls;
    systems.Add<ProbeSystem>(calls, 1);
    systems.Add<ProbeSystem>(calls, 2);

    systems.Start(scene);
    systems.Update(scene, 0.016f);
    systems.Stop(scene);

    const std::vector<int> expected{1, 2, 10, 20, 200, 100};
    if (calls != expected) {
        return 1;
    }

    std::vector<int> failedStart;
    Concord::SystemSchedule startSchedule;
    startSchedule.Add<ProbeSystem>(failedStart, 1);
    startSchedule.Add<ThrowingStartSystem>(failedStart);
    bool startThrew = false;
    try {
        startSchedule.Start(scene);
    } catch (const std::runtime_error&) {
        startThrew = true;
    }
    if (!startThrew || failedStart != std::vector<int>{1, 3, 100}) {
        return 1;
    }

    std::vector<int> failedStop;
    Concord::SystemSchedule stopSchedule;
    stopSchedule.Add<ProbeSystem>(failedStop, 5);
    stopSchedule.Add<ThrowingStopSystem>(failedStop);
    stopSchedule.Start(scene);
    bool stopThrew = false;
    try {
        stopSchedule.Stop(scene);
    } catch (const std::runtime_error&) {
        stopThrew = true;
    }
    return stopThrew && failedStop == std::vector<int>{5, -4, 500} ? 0 : 1;
}
