# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# ConcordFlashGameEngineRuntime.dll: lifecycle, ECS, scenes, and windowing.
# Runtime only consumes the render interfaces and factory slot; it never
# names a concrete Vulkan implementation.
set(CONCORD_RUNTIME_SOURCES
    src/engine/app/Game.cpp
    src/engine/app/GameWindow.cpp

    # The C ABI ConcordScript's LLVM backend calls. It lives in Runtime because
    # every object it wraps (Game, Window, Scene) does; it never names a render
    # backend, so the Runtime -> Render direction stays one-way.
    src/engine/cvm/CvmEntityRegistry.cpp
    src/engine/cvm/CvmAnimation.cpp
    src/engine/cvm/CvmModel.cpp
    src/engine/cvm/CvmModelRegistry.cpp
    src/engine/cvm/CvmParticles.cpp
    src/engine/cvm/CvmPhysics.cpp
    src/engine/cvm/CvmQuery.cpp
    src/engine/cvm/CvmRun.cpp
    src/engine/cvm/CvmScene.cpp
    src/engine/cvm/CvmSceneRegistry.cpp
    src/engine/cvm/CvmSpawn.cpp
    src/engine/cvm/CvmSystems.cpp

    src/engine/debug/DebugOverlay.cpp
    src/engine/ui/UiCanvas.cpp

    ${CONCORD_ASSET_SOURCES}

    src/engine/animation/AnimationBlend.cpp
    src/engine/animation/AnimationBlendSpace.cpp
    src/engine/animation/AnimationBlendSpace2D.cpp
    src/engine/animation/AnimationBlendSpace2DSampling.cpp
    src/engine/animation/AnimationController.cpp
    src/engine/animation/AnimationGraph.cpp
    src/engine/animation/AnimationLayer.cpp
    src/engine/animation/AnimationRetarget.cpp
    src/engine/animation/AnimationRetargetMath.cpp
    src/engine/animation/AnimationSampling.cpp
    src/engine/animation/Humanoid.cpp
    src/engine/animation/AnimationStateMachine.cpp
    src/engine/animation/AnimationStateMachineTime.cpp
    src/engine/animation/JointMask.cpp

    src/engine/scene/DayCycle.cpp

    src/engine/particle/ParticleSampling.cpp
    src/engine/particle/ParticleSimulation.cpp

    src/engine/input/InputMap.cpp
    src/engine/input/SdlInputCodes.cpp

    src/engine/ecs/DayCycleSystem.cpp
    src/engine/ecs/WaterRippleSystem.cpp
    src/engine/ecs/WorldId.cpp
    src/engine/ecs/AnimationSystem.cpp
    src/engine/ecs/ParticleSystem.cpp
    src/engine/ecs/PhysicsSystem.cpp
    src/engine/ecs/SystemSchedule.cpp

    src/engine/physics/PhysicsWorld.cpp

    src/engine/scene/FirstPersonController.cpp

    src/engine/render/RenderBackendFactory.cpp
    src/engine/render/VulkanPassRegistry.cpp

    src/engine/window/SdlWindowFlags.cpp
    src/engine/window/WindowAccess.cpp
    src/engine/window/Window.cpp
    src/engine/window/WindowProperties.cpp
    src/engine/window/WindowState.cpp
)
