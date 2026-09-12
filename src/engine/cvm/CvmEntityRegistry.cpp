// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmEntityRegistry.h"

#include <cstddef>
#include <mutex>
#include <vector>

namespace Concord::Cvm {

namespace {

std::mutex g_entitiesMutex;

/** Slot storage; an empty scene pointer marks a retired slot. */
std::vector<EntityRef> g_entities;
std::vector<std::size_t> g_freeEntities;

/** Resolves a handle while the caller already holds the lock. */
EntityRef* LookupLocked(std::int64_t handle)
{
    if (handle <= 0) return nullptr;
    const std::size_t index = static_cast<std::size_t>(handle - 1);
    if (index >= g_entities.size()) return nullptr;
    if (g_entities[index].scene == nullptr) return nullptr;
    return &g_entities[index];
}

} // namespace

std::int64_t CreateEntity(std::int64_t sceneHandle, Scene& scene, Entity entity)
{
    if (!entity.IsValid()) return 0;
    std::lock_guard<std::mutex> guard(g_entitiesMutex);
    std::size_t index = 0;
    if (!g_freeEntities.empty()) {
        index = g_freeEntities.back();
        g_freeEntities.pop_back();
    } else {
        g_entities.emplace_back();
        index = g_entities.size() - 1;
    }
    g_entities[index] = EntityRef{sceneHandle, &scene, entity};
    return static_cast<std::int64_t>(index) + 1;
}

std::int64_t FindOrCreateEntity(std::int64_t sceneHandle, Scene& scene, Entity entity)
{
    if (!entity.IsValid()) return 0;
    std::lock_guard<std::mutex> guard(g_entitiesMutex);
    for (std::size_t index = 0; index < g_entities.size(); ++index) {
        const EntityRef& ref = g_entities[index];
        if (ref.scene == &scene && ref.entity == entity) {
            return static_cast<std::int64_t>(index) + 1;
        }
    }
    std::size_t index = 0;
    if (!g_freeEntities.empty()) {
        index = g_freeEntities.back();
        g_freeEntities.pop_back();
    } else {
        g_entities.emplace_back();
        index = g_entities.size() - 1;
    }
    g_entities[index] = EntityRef{sceneHandle, &scene, entity};
    return static_cast<std::int64_t>(index) + 1;
}

bool AcquireEntity(std::int64_t handle, EntityRef& out)
{
    std::lock_guard<std::mutex> guard(g_entitiesMutex);
    const EntityRef* found = LookupLocked(handle);
    if (found == nullptr) return false;
    out = *found;
    return true;
}

std::int64_t DestroyEntity(std::int64_t handle)
{
    std::lock_guard<std::mutex> guard(g_entitiesMutex);
    EntityRef* found = LookupLocked(handle);
    if (found == nullptr) return 0;
    found->scene = nullptr;
    found->sceneHandle = 0;
    g_freeEntities.push_back(static_cast<std::size_t>(handle - 1));
    return 1;
}

void ForgetScene(std::int64_t sceneHandle)
{
    std::lock_guard<std::mutex> guard(g_entitiesMutex);
    for (std::size_t index = 0; index < g_entities.size(); ++index) {
        EntityRef& ref = g_entities[index];
        if (ref.scene == nullptr || ref.sceneHandle != sceneHandle) continue;
        ref.scene = nullptr;
        ref.sceneHandle = 0;
        g_freeEntities.push_back(index);
    }
}

} // namespace Concord::Cvm
