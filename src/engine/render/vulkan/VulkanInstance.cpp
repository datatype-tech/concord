// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanInstance.h"

#include "engine/render/vulkan/VulkanResult.h"

#include <SDL3/SDL_vulkan.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace Concord {

namespace {

/** Whether the loader advertises the KHRONOS validation layer. */
bool ValidationLayerAvailable()
{
    u32 count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());

    for (const VkLayerProperties& layer : layers) {
        if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

bool CreateVulkanInstance(VulkanContext& context, bool enableValidation)
{
    u32 extensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!sdlExtensions) {
        std::fprintf(stderr, "[Concord] SDL_Vulkan_GetInstanceExtensions failed\n");
        return false;
    }

    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    context.validationEnabled = enableValidation && ValidationLayerAvailable();

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Concord Flash";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Concord Flash";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = extensionCount;
    info.ppEnabledExtensionNames = sdlExtensions;
    info.enabledLayerCount = context.validationEnabled ? 1u : 0u;
    info.ppEnabledLayerNames = context.validationEnabled ? &validationLayer : nullptr;

    const VkResult result = vkCreateInstance(&info, nullptr, &context.instance);
    return result == VK_SUCCESS ? true : VulkanFailed("vkCreateInstance", result);
}

void DestroyVulkanInstance(VulkanContext& context)
{
    if (context.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(context.instance, nullptr);
        context.instance = VK_NULL_HANDLE;
    }
}

} // namespace Concord
