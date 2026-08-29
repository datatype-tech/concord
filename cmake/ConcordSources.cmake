# The engine's translation units, split by which DLL they build into.
#
# Listed explicitly rather than globbed so that adding a file is a visible,
# reviewable change and CMake never silently misses one.

# ConcordFlashGameEngineRuntime.dll: lifecycle, ECS, scenes, windowing.
# Depends on nothing under render/ except the IRenderBackend interface and
# the factory slot a render DLL plugs into.
set(CONCORD_RUNTIME_SOURCES
    src/engine/app/Game.cpp
    src/engine/app/GameWindow.cpp

    src/engine/ecs/WorldId.cpp
    src/engine/ecs/SystemSchedule.cpp

    src/engine/render/RenderBackendFactory.cpp
    src/engine/render/VulkanPassRegistry.cpp

    src/engine/window/SdlWindowFlags.cpp
    src/engine/window/WindowAccess.cpp
    src/engine/window/Window.cpp
    src/engine/window/WindowProperties.cpp
    src/engine/window/WindowState.cpp
)

# ConcordFlashGameEngineRender.dll: the Vulkan backend. Links against the
# runtime DLL for Window access, so the dependency between the two DLLs runs
# one way only.
set(CONCORD_RENDER_SOURCES
    src/engine/render/RenderFrameData.cpp
    src/engine/render/RenderSceneSnapshot.cpp
    src/engine/render/VulkanRenderBackend.cpp
    src/engine/render/VulkanRenderBackendShutdown.cpp
    src/engine/render/VulkanRenderBackendDraw.cpp
    src/engine/render/VulkanRenderBackendDebug.cpp
    src/engine/render/VulkanRenderBackendExtensions.cpp
    src/engine/render/VulkanRenderBackendFrame.cpp
    src/engine/render/VulkanRenderBackendShadow.cpp
    src/engine/render/vulkan/VulkanBackendRegistration.cpp
    src/engine/render/vulkan/VulkanBoxPipeline.cpp
    src/engine/render/vulkan/VulkanBoxPipelineColor.cpp
    src/engine/render/vulkan/VulkanBoxPipelineLayout.cpp
    src/engine/render/vulkan/VulkanBoxPipelineState.cpp
    src/engine/render/vulkan/VulkanBoxPipelineRecord.cpp
    src/engine/render/vulkan/VulkanBuffer.cpp
    src/engine/render/vulkan/VulkanBufferCreate.cpp
    src/engine/render/vulkan/VulkanBufferSync.cpp
    src/engine/render/vulkan/VulkanClearPass.cpp
    src/engine/render/vulkan/VulkanDepthBuffer.cpp
    src/engine/render/vulkan/VulkanDevice.cpp
    src/engine/render/vulkan/VulkanFrameSync.cpp
    src/engine/render/vulkan/VulkanFrameDataResources.cpp
    src/engine/render/vulkan/VulkanFrameDataResourcesCreate.cpp
    src/engine/render/vulkan/VulkanFrameDataResourcesLifecycle.cpp
    src/engine/render/vulkan/VulkanImageBarrier.cpp
    src/engine/render/vulkan/VulkanInstance.cpp
    src/engine/render/vulkan/VulkanPhysicalDevice.cpp
    src/engine/render/vulkan/VulkanPresent.cpp
    src/engine/render/vulkan/VulkanResult.cpp
    src/engine/render/vulkan/VulkanRayTracingSupport.cpp
    src/engine/render/vulkan/VulkanRayTracingPipeline.cpp
    src/engine/render/vulkan/VulkanRayTracingScene.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRing.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneGeometry.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneBottomLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneTopLevel.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneRecord.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneInstances.cpp
    src/engine/render/vulkan/VulkanRayTracingSceneDescriptor.cpp
    src/engine/render/vulkan/VulkanSurface.cpp
    src/engine/render/vulkan/VulkanSurfaceFormat.cpp
    src/engine/render/vulkan/VulkanSwapchain.cpp
    src/engine/render/vulkan/VulkanSwapchainResources.cpp
    src/engine/render/vulkan/VulkanShaderModule.cpp
    src/engine/render/vulkan/VulkanTileLightCulling.cpp
    src/engine/render/vulkan/VulkanShadowMap.cpp
    src/engine/render/vulkan/VulkanShadowMapDescriptors.cpp
    src/engine/render/vulkan/VulkanShadowMapTransitions.cpp
    src/engine/render/vulkan/VulkanShadowPipeline.cpp
    src/engine/render/vulkan/VulkanShadowPipelineRecord.cpp
    src/engine/render/vulkan/VulkanShadowMath.cpp
)
