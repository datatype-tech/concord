// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanSurface.h"

#include "engine/window/Window.h"
#include "engine/window/WindowAccess.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdio>

namespace Concord {

bool CreateVulkanSurface(VulkanContext& context, const Window& window)
{
    auto* handle = static_cast<SDL_Window*>(WindowAccess::NativeHandle(window));
    if (!handle) {
        std::fprintf(stderr, "[Concord] window is not open; attach it before initializing\n");
        return false;
    }

    if (!SDL_Vulkan_CreateSurface(handle, context.instance, nullptr, &context.surface)) {
        std::fprintf(stderr, "[Concord] SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

void DestroyVulkanSurface(VulkanContext& context)
{
    if (context.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
        context.surface = VK_NULL_HANDLE;
    }
}

} // namespace Concord
