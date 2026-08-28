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

VKAPI_ATTR VkBool32 VKAPI_CALL ValidationCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    const char* level = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0
                            ? "error"
                            : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0
                                  ? "warning"
                                  : "info";
    std::fprintf(stderr, "[Concord][Vulkan][%s] %s\n", level,
                 data && data->pMessage ? data->pMessage : "(no message)");
    return VK_FALSE;
}

/** Whether the instance extension needed by the validation messenger exists. */
bool DebugUtilsAvailable()
{
    u32 count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) {
        return false;
    }
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS) {
        return false;
    }
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
            return true;
        }
    }
    return false;
}

/** Creates a messenger after instance creation when the loader exposes it. */
void CreateDebugMessenger(VulkanContext& context)
{
    if (!context.validationEnabled) {
        return;
    }
    const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(context.instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!create) {
        return;
    }
    VkDebugUtilsMessengerCreateInfoEXT info{};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = ValidationCallback;
    if (create(context.instance, &info, nullptr, &context.debugMessenger) != VK_SUCCESS) {
        context.debugMessenger = VK_NULL_HANDLE;
    }
}

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
    const bool debugUtils = context.validationEnabled && DebugUtilsAvailable();
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + extensionCount);
    if (debugUtils) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

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
    info.enabledExtensionCount = static_cast<u32>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    info.enabledLayerCount = context.validationEnabled ? 1u : 0u;
    info.ppEnabledLayerNames = context.validationEnabled ? &validationLayer : nullptr;

    const VkResult result = vkCreateInstance(&info, nullptr, &context.instance);
    if (result != VK_SUCCESS) {
        return VulkanFailed("vkCreateInstance", result);
    }
    CreateDebugMessenger(context);
    return true;
}

void DestroyVulkanInstance(VulkanContext& context)
{
    if (context.instance != VK_NULL_HANDLE) {
        if (context.debugMessenger != VK_NULL_HANDLE) {
            const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(context.instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) {
                destroy(context.instance, context.debugMessenger, nullptr);
            }
        }
        vkDestroyInstance(context.instance, nullptr);
        context.instance = VK_NULL_HANDLE;
    }
    context.debugMessenger = VK_NULL_HANDLE;
}

} // namespace Concord
