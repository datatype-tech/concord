// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/cvm/CvmModelRegistry.h"

#include <cstdint>
#include <mutex>
#include <string>

/**
 * Model loading, and the diagnostic that goes with it.
 *
 * Loading is the only place a CVM program touches the filesystem, so it is the
 * only place that can fail for a reason the program cannot see. Returning 0 and
 * then offering the loader's own message is the difference between "your path
 * is wrong" and "something went wrong".
 */
namespace {

std::mutex g_errorMutex;
std::string g_lastError;

} // namespace

extern "C" {

std::int64_t ConcordCvmModelLoad(const char* path, double scale)
{
    if (path == nullptr) {
        std::lock_guard<std::mutex> guard(g_errorMutex);
        g_lastError = "no path given";
        return 0;
    }
    std::string error;
    const std::int64_t handle = Concord::Cvm::LoadModel(path, scale, error);
    std::lock_guard<std::mutex> guard(g_errorMutex);
    g_lastError = handle == 0 ? error : std::string();
    return handle;
}

std::int64_t ConcordCvmModelFree(std::int64_t model)
{
    return Concord::Cvm::FreeModel(model);
}

const char* ConcordCvmLastError(void)
{
    std::lock_guard<std::mutex> guard(g_errorMutex);
    return g_lastError.c_str();
}

std::int64_t ConcordCvmModelMaterialCount(std::int64_t model)
{
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    return asset == nullptr ? 0 : static_cast<std::int64_t>(asset->materials.size());
}

std::int64_t ConcordCvmModelClipCount(std::int64_t model)
{
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    return asset == nullptr ? 0 : static_cast<std::int64_t>(asset->animations.size());
}

std::int64_t ConcordCvmModelSkeletonCount(std::int64_t model)
{
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    return asset == nullptr ? 0 : static_cast<std::int64_t>(asset->skeletons.size());
}

const char* ConcordCvmModelClipName(std::int64_t model, std::int64_t index)
{
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    if (asset == nullptr || index < 0) return "";
    const auto clip = static_cast<std::size_t>(index);
    if (clip >= asset->animations.size()) return "";
    return asset->animations[clip].name.c_str();
}

double ConcordCvmModelClipDuration(std::int64_t model, std::int64_t index)
{
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    if (asset == nullptr || index < 0) return 0.0;
    const auto clip = static_cast<std::size_t>(index);
    if (clip >= asset->animations.size()) return 0.0;
    return static_cast<double>(asset->animations[clip].duration);
}

std::int64_t ConcordCvmModelFindClip(std::int64_t model, const char* name)
{
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    if (asset == nullptr || name == nullptr) return -1;
    for (std::size_t index = 0; index < asset->animations.size(); ++index) {
        if (asset->animations[index].name == name) {
            return static_cast<std::int64_t>(index);
        }
    }
    return -1;
}

std::int64_t ConcordCvmModelSetAlbedoTexture(std::int64_t model, std::int64_t index,
                                             const char* path)
{
    std::shared_ptr<Concord::ModelAsset> asset = Concord::Cvm::AcquireModel(model);
    if (asset == nullptr || index < 0) return 0;
    const auto material = static_cast<std::size_t>(index);
    if (material >= asset->materials.size()) return 0;
    asset->materials[material].baseColorTexture = path == nullptr ? std::string() : path;
    return 1;
}

} // extern "C"
