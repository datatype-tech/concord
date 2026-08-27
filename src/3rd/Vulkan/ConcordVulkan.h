// Concord/Vulkan.h - Vulkan wrapper.
// Requires: src/3rd/Vulkan/vulkan/vulkan.h, lib/vulkan-1.lib
// (the vulkan-1.dll loader is provided by the GPU drivers at runtime).
#pragma once

#include <Concord/Core.h>

#include <vulkan/vulkan.h>

namespace concord::vk {

[[nodiscard]] inline const char* result_string(VkResult result) noexcept {
    switch (result) {
        case VK_SUCCESS:                       return "VK_SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY:      return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:   return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:       return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:   return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:     return "VK_ERROR_INCOMPATIBLE_DRIVER";
        default:                               return "VK_RESULT_UNKNOWN";
    }
}

} // namespace concord::vk
