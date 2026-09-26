// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "Concord/CApplication.h"
#include "Concord/CUi.h"

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

/** Keeps a callback regression from leaving the hidden test window running. */
class QuitAfterFrameSystem final : public Concord::ISystem {
public:
    explicit QuitAfterFrameSystem(Concord::Game& game) : m_game(game) {}
    void OnUpdate(Concord::Scene&, Concord::f32) override { m_game.Quit(); }

private:
    Concord::Game& m_game;
};

bool CheckGameFrameCallbacks()
{
    Concord::Window window({.title = "Game callback lifecycle tests", .visible = false});
    Concord::Scene scene;
    Concord::Game game({.enableRendering = false});
    game.AttachWindow(window);
    if (!window.IsOpen()) return false;
    game.LoadScene(scene);
    game.Systems().Add<QuitAfterFrameSystem>(game);

    std::vector<int> calls;
    game.OnUi([&] {
        if (!game.Ui().IsOpen()) throw std::runtime_error("UI callback ran outside its frame");
        calls.push_back(2);
        game.Ui().Label(10, 10, "UI survives script update callbacks");
        game.Quit();
    });
    game.OnUpdate([&](Concord::f32) { calls.push_back(1); });
    game.Run();
    if (calls != std::vector<int>{1, 2} || game.Ui().IsOpen() ||
        game.Ui().DrawList().commands.empty()) return false;

    calls.clear();
    game.OnUpdate([&](Concord::f32) { calls.push_back(3); });
    game.Run();
    if (calls != std::vector<int>{3, 2}) return false;

    calls.clear();
    game.OnUpdate({});
    game.Run();
    if (calls != std::vector<int>{2}) return false;

    calls.clear();
    game.OnUpdate([&](Concord::f32) { calls.push_back(4); game.Quit(); });
    game.OnUi({});
    game.Run();
    return calls == std::vector<int>{4} && game.Ui().DrawList().commands.empty();
}

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
    return stopThrew && failedStop == std::vector<int>{5, -4, 500} && CheckGameFrameCallbacks()
        ? 0 : 1;
}
