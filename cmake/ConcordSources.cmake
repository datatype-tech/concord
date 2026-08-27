# The engine's translation units, split by which DLL they build into.
#
# Listed explicitly rather than globbed so that adding a file is a visible,
# reviewable change and CMake never silently misses one.

# ConcordFlashGameEngineRuntime.dll: lifecycle, ECS, scenes, windowing.
# Depends on nothing under render/ except the IRenderBackend interface and
# the factory slot a render DLL plugs into.
set(CONCORD_RUNTIME_SOURCES
    src/engine/app/Game.cpp

    src/engine/ecs/SystemSchedule.cpp

    src/engine/render/RenderBackendFactory.cpp

    src/engine/window/SdlWindowFlags.cpp
    src/engine/window/Window.cpp
    src/engine/window/WindowState.cpp
)

# ConcordFlashGameEngineRender.dll: the Vulkan backend. Links against the
# runtime DLL for Window access, so the dependency between the two DLLs runs
# one way only.
set(CONCORD_RENDER_SOURCES
    src/engine/render/VulkanRenderBackend.cpp
    src/engine/render/vulkan/VulkanBackendRegistration.cpp
    src/engine/render/vulkan/VulkanClearPass.cpp
    src/engine/render/vulkan/VulkanDevice.cpp
    src/engine/render/vulkan/VulkanFrameSync.cpp
    src/engine/render/vulkan/VulkanImageBarrier.cpp
    src/engine/render/vulkan/VulkanInstance.cpp
    src/engine/render/vulkan/VulkanPhysicalDevice.cpp
    src/engine/render/vulkan/VulkanPresent.cpp
    src/engine/render/vulkan/VulkanResult.cpp
    src/engine/render/vulkan/VulkanSurface.cpp
    src/engine/render/vulkan/VulkanSurfaceFormat.cpp
    src/engine/render/vulkan/VulkanSwapchain.cpp
)
