# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ConcordFlashGameEngineRuntime.dll: lifecycle, ECS, scenes, and windowing.
# Runtime only consumes the render interfaces and factory slot; it never
# names a concrete Vulkan implementation.
set(CONCORD_RUNTIME_SOURCES
    src/engine/app/Game.cpp
    src/engine/app/GameWindow.cpp

    ${CONCORD_ASSET_SOURCES}

    src/engine/ecs/WorldId.cpp
    src/engine/ecs/AnimationSystem.cpp
    src/engine/ecs/SystemSchedule.cpp

    src/engine/render/RenderBackendFactory.cpp
    src/engine/render/VulkanPassRegistry.cpp

    src/engine/window/SdlWindowFlags.cpp
    src/engine/window/WindowAccess.cpp
    src/engine/window/Window.cpp
    src/engine/window/WindowProperties.cpp
    src/engine/window/WindowState.cpp
)
