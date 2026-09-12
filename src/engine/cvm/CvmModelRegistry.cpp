// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmModelRegistry.h"

#include "engine/asset/ModelLoader.h"

#include <cstddef>
#include <mutex>
#include <utility>
#include <vector>

namespace Concord::Cvm {

namespace {

std::mutex g_modelsMutex;
std::vector<std::shared_ptr<ModelAsset>> g_models;
std::vector<std::size_t> g_freeModels;

/** Resolves a handle while the caller already holds the lock. */
std::shared_ptr<ModelAsset>* LookupLocked(std::int64_t handle)
{
    if (handle <= 0) return nullptr;
    const std::size_t index = static_cast<std::size_t>(handle - 1);
    if (index >= g_models.size()) return nullptr;
    if (g_models[index] == nullptr) return nullptr;
    return &g_models[index];
}

} // namespace

std::int64_t LoadModel(const std::string& path, double scale, std::string& error)
{
    ModelLoadOptions options;
    options.scale = static_cast<f32>(scale);
    ModelLoadResult loaded = ModelLoader::Load(path, options);
    if (!loaded.Succeeded()) {
        error = loaded.ErrorMessage().empty() ? "could not load '" + path + "'"
                                              : loaded.ErrorMessage();
        return 0;
    }

    auto asset = std::make_shared<ModelAsset>(std::move(loaded.asset));
    std::lock_guard<std::mutex> guard(g_modelsMutex);
    std::size_t index = 0;
    if (!g_freeModels.empty()) {
        index = g_freeModels.back();
        g_freeModels.pop_back();
    } else {
        g_models.emplace_back();
        index = g_models.size() - 1;
    }
    g_models[index] = std::move(asset);
    return static_cast<std::int64_t>(index) + 1;
}

std::int64_t FreeModel(std::int64_t handle)
{
    std::lock_guard<std::mutex> guard(g_modelsMutex);
    std::shared_ptr<ModelAsset>* found = LookupLocked(handle);
    if (found == nullptr) return 0;
    found->reset();
    g_freeModels.push_back(static_cast<std::size_t>(handle - 1));
    return 1;
}

std::shared_ptr<ModelAsset> AcquireModel(std::int64_t handle)
{
    std::lock_guard<std::mutex> guard(g_modelsMutex);
    const std::shared_ptr<ModelAsset>* found = LookupLocked(handle);
    return found == nullptr ? nullptr : *found;
}

} // namespace Concord::Cvm
