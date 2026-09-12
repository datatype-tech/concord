// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmSceneRegistry.h"

#include "engine/cvm/CvmEntityRegistry.h"
#include "engine/cvm/CvmPhysicsRuntime.h"
#include "engine/scene/Scene.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace Concord::Cvm {

namespace {

std::mutex g_scenesMutex;
std::vector<std::unique_ptr<Scene>> g_scenes;
std::vector<std::size_t> g_freeScenes;

} // namespace

std::int64_t CreateScene()
{
    auto scene = std::make_unique<Scene>();
    std::lock_guard<std::mutex> guard(g_scenesMutex);
    if (!g_freeScenes.empty()) {
        const std::size_t index = g_freeScenes.back();
        g_freeScenes.pop_back();
        g_scenes[index] = std::move(scene);
        return static_cast<std::int64_t>(index) + 1;
    }
    g_scenes.push_back(std::move(scene));
    return static_cast<std::int64_t>(g_scenes.size());
}

std::int64_t DestroyScene(std::int64_t handle)
{
    if (handle <= 0) return 0;
    const std::size_t index = static_cast<std::size_t>(handle - 1);
    std::lock_guard<std::mutex> guard(g_scenesMutex);
    if (index >= g_scenes.size() || g_scenes[index] == nullptr) return 0;
    ReleaseSimulation(*g_scenes[index]);
    g_scenes[index].reset();
    g_freeScenes.push_back(index);
    // The slot is about to become reusable, so any entity handle still naming
    // this scene would otherwise resolve against whatever replaces it.
    ForgetScene(handle);
    return 1;
}

Scene* AcquireScene(std::int64_t handle)
{
    if (handle <= 0) return nullptr;
    const std::size_t index = static_cast<std::size_t>(handle - 1);
    std::lock_guard<std::mutex> guard(g_scenesMutex);
    if (index >= g_scenes.size()) return nullptr;
    return g_scenes[index].get();
}

} // namespace Concord::Cvm
