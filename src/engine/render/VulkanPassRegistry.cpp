// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/VulkanPass.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace Concord {
namespace {
struct RegisteredPass {
    std::string name;
    std::string key;
    VulkanPassPhase phase = VulkanPassPhase::AfterScene;
    std::uint32_t order = 0;
    VulkanPassCallback callback = nullptr;
    void* userData = nullptr;
    std::uint64_t sequence = 0;
};
std::mutex g_passMutex;
std::vector<RegisteredPass> g_passes;
std::uint64_t g_nextSequence = 0;
std::string CanonicalName(const char* name)
{
    std::string key = name == nullptr ? std::string{} : std::string{name};
    for (char& character : key) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return key;
}
bool IsValidPhase(VulkanPassPhase phase) noexcept
{
    return static_cast<std::uint32_t>(phase) <=
           static_cast<std::uint32_t>(VulkanPassPhase::Shutdown);
}
bool ComesBefore(const RegisteredPass& left, const RegisteredPass& right) noexcept
{
    if (left.phase != right.phase) {
        return static_cast<std::uint32_t>(left.phase) <
               static_cast<std::uint32_t>(right.phase);
    }
    if (left.order != right.order) {
        return left.order < right.order;
    }
    return left.sequence < right.sequence;
}
} // namespace

bool RegisterVulkanPass(const VulkanPassDesc& description) noexcept
{
    if (description.name == nullptr || description.name[0] == '\0' ||
        description.callback == nullptr || !IsValidPhase(description.phase)) {
        return false;
    }
    try {
        std::scoped_lock lock(g_passMutex);
        const std::string key = CanonicalName(description.name);
        const auto duplicate = std::find_if(g_passes.begin(), g_passes.end(),
                                             [&key](const RegisteredPass& pass) {
                                                 return pass.key == key;
                                             });
        if (duplicate != g_passes.end()) {
            return false;
        }
        g_passes.push_back(RegisteredPass{
            .name = description.name,
            .key = key,
            .phase = description.phase,
            .order = description.order,
            .callback = description.callback,
            .userData = description.userData,
            .sequence = g_nextSequence++,
        });
        std::stable_sort(g_passes.begin(), g_passes.end(), ComesBefore);
        return true;
    } catch (...) {
        return false;
    }
}
bool UnregisterVulkanPass(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    try {
        std::scoped_lock lock(g_passMutex);
        const std::string key = CanonicalName(name);
        const auto it = std::find_if(g_passes.begin(), g_passes.end(),
                                     [&key](const RegisteredPass& pass) {
                                         return pass.key == key;
                                     });
        if (it == g_passes.end()) {
            return false;
        }
        g_passes.erase(it);
        return true;
    } catch (...) {
        return false;
    }
}

void ClearVulkanPasses() noexcept
{
    try {
        std::scoped_lock lock(g_passMutex);
        g_passes.clear();
    } catch (...) {
    }
}

bool RunVulkanPasses(VulkanPassPhase phase, const VulkanPassContext& context) noexcept
{
    if (!IsValidPhase(phase)) {
        return false;
    }
    std::vector<RegisteredPass> snapshot;
    try {
        std::scoped_lock lock(g_passMutex);
        snapshot = g_passes;
    } catch (...) {
        return false;
    }
    bool success = true;
    VulkanPassContext invocation = context;
    invocation.phase = phase;
    for (const RegisteredPass& pass : snapshot) {
        if (pass.phase != phase) {
            continue;
        }
        try {
            if (!pass.callback(invocation, pass.userData)) {
                success = false;
                std::fprintf(stderr, "[Concord] Vulkan pass '%s' returned false\n",
                             pass.name.c_str());
            }
        } catch (...) {
            success = false;
            std::fprintf(stderr, "[Concord] Vulkan pass '%s' threw an exception\n",
                         pass.name.c_str());
        }
    }
    return success;
}

} // namespace Concord
