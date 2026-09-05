// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/animation/AnimationController.h"
#include "engine/animation/AnimationStateMachine.h"
#include "engine/ecs/AnimationSystem.h"
#include "engine/scene/Scene.h"

#include <cmath>
#include <cstdio>
#include <memory>

namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) < 0.01f;
}

std::shared_ptr<Concord::ModelAsset> MakeTwoClipAsset()
{
    auto asset = std::make_shared<Concord::ModelAsset>();
    Concord::Skeleton skeleton{};
    skeleton.joints.push_back(Concord::Joint{.name = "root", .parent = -1});
    skeleton.nodeIndices = {0};
    skeleton.root = 0;
    asset->skeletons.push_back(skeleton);
    asset->nodes.push_back(Concord::ModelNode{.name = "root"});

    Concord::AnimationChannel walkChannel{};
    walkChannel.sourceNode = 0;
    walkChannel.path = Concord::AnimationPath::Translation;
    walkChannel.vec3Keys = {{.time = 0.0f, .value = {0.0f, 0.0f, 0.0f}},
                            {.time = 1.0f, .value = {2.0f, 0.0f, 0.0f}}};
    Concord::AnimationClip walk{.name = "walk", .duration = 1.0f};
    walk.channels.push_back(walkChannel);

    Concord::AnimationChannel runChannel = walkChannel;
    runChannel.vec3Keys = {{.time = 0.0f, .value = {0.0f, 0.0f, 0.0f}},
                           {.time = 1.0f, .value = {4.0f, 0.0f, 0.0f}}};
    Concord::AnimationClip run{.name = "run", .duration = 1.0f};
    run.channels.push_back(runChannel);

    asset->animations.push_back(std::move(walk));
    asset->animations.push_back(std::move(run));
    return asset;
}

Concord::AnimationGraph MakeGraph()
{
    Concord::AnimationGraph graph{};
    graph.states.push_back({.name = "walk", .clipIndex = 0});
    graph.states.push_back({.name = "run", .clipIndex = 1});
    graph.transitions.push_back(
        {.fromState = 0, .toState = 1, .duration = 0.5f});
    graph.initialState = 0;
    return graph;
}

bool TestCrossfade()
{
    const auto asset = MakeTwoClipAsset();
    const Concord::AnimationGraph graph = MakeGraph();
    Concord::AnimationStateMachineState runtime;
    Concord::SkeletonPose pose;

    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.0f, runtime, pose)) {
        return false;
    }
    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.5f, runtime, pose)) {
        return false;
    }
    if (!Near(pose.local[0].translation.x, 1.0f)) return false;

    if (!Concord::RequestAnimationTransition(graph, std::string_view{"run"}, runtime)) {
        return false;
    }
    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.1f, runtime, pose)) {
        return false;
    }
    const float earlyBlend = pose.local[0].translation.x;
    if (!Near(earlyBlend, 1.2f * 0.8f + 0.4f * 0.2f)) return false;

    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.4f, runtime, pose)) {
        return false;
    }
    if (runtime.nextState != Concord::kInvalidAnimationState) return false;
    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.0f, runtime, pose)) {
        return false;
    }
    return runtime.currentState == 1 && Near(pose.local[0].translation.x, 2.0f);
}

bool TestExitTimeTransition()
{
    const auto asset = MakeTwoClipAsset();
    Concord::AnimationGraph graph{};
    graph.states.push_back({.name = "walk", .clipIndex = 0, .loop = false});
    graph.states.push_back({.name = "run", .clipIndex = 1, .loop = false});
    graph.transitions.push_back(
        {.fromState = 0, .toState = 1, .duration = 0.2f, .hasExitTime = true,
         .exitTime = 0.5f});
    graph.initialState = 0;

    Concord::AnimationStateMachineState runtime;
    Concord::SkeletonPose pose;
    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.0f, runtime, pose)) {
        std::printf("  exit-time: initial evaluate failed\n");
        return false;
    }
    if (runtime.nextState != Concord::kInvalidAnimationState) {
        std::printf("  exit-time: premature transition (next=%u time=%.3f)\n",
                    runtime.nextState, runtime.currentTime);
        return false;
    }
    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.45f, runtime, pose)) {
        std::printf("  exit-time: pre-exit evaluate failed\n");
        return false;
    }
    if (runtime.nextState != Concord::kInvalidAnimationState) {
        std::printf("  exit-time: fired before exit time (next=%u)\n", runtime.nextState);
        return false;
    }
    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.1f, runtime, pose)) {
        std::printf("  exit-time: exit evaluate failed\n");
        return false;
    }
    if (runtime.nextState != 1) {
        std::printf("  exit-time: no transition started (cur=%u next=%u time=%.3f)\n",
                    runtime.currentState, runtime.nextState, runtime.currentTime);
        return false;
    }
    if (!Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.1f, runtime, pose) ||
        !Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.1f, runtime, pose)) {
        std::printf("  exit-time: transition evaluate failed\n");
        return false;
    }
    if (runtime.currentState != 1 || runtime.nextState != Concord::kInvalidAnimationState) {
        std::printf("  exit-time: transition not complete (cur=%u next=%u)\n",
                    runtime.currentState, runtime.nextState);
        return false;
    }
    return true;
}

bool TestInvalidGraphFails()
{
    const auto asset = MakeTwoClipAsset();
    Concord::AnimationGraph graph = MakeGraph();
    graph.states[0].clipIndex = 99;
    Concord::AnimationStateMachineState runtime;
    Concord::SkeletonPose pose;
    return !Concord::EvaluateAnimationStateMachine(*asset, graph, 0, 0.1f, runtime, pose);
}

bool TestControllerComponentEndToEnd()
{
    const auto asset = MakeTwoClipAsset();
    Concord::AnimationGraph base = MakeGraph();
    Concord::AnimationGraph layerGraph{};
    layerGraph.states.push_back({.name = "run", .clipIndex = 1});
    layerGraph.initialState = 0;

    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationControllerComponent>(Concord::AnimationControllerComponent{
            .asset = asset.get(), .skeletonIndex = 0, .graph = &base})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();

    if (Concord::UpdateAnimationControllers(scene.GetWorld(), 0.5f) != 1) return false;
    const auto* pose = scene.GetWorld().Get<Concord::SkinningPoseComponent>(entity);
    if (pose == nullptr || pose->local.size() != 1 ||
        !Near(pose->local[0].translation.x, 1.0f)) {
        return false;
    }

    auto* controller =
        scene.GetWorld().Get<Concord::AnimationControllerComponent>(entity);
    if (controller == nullptr) return false;
    controller->layers[0].enabled = true;
    controller->layers[0].graph = &layerGraph;
    controller->layers[0].weight = 1.0f;
    controller->layers[0].mode = Concord::AnimationBlendMode::Override;
    if (Concord::UpdateAnimationControllers(scene.GetWorld(), 0.0f) != 1) return false;
    return pose != nullptr && Near(pose->local[0].translation.x, 0.0f);
}

bool TestControllerOverridesLegacyComponent()
{
    const auto asset = MakeTwoClipAsset();
    Concord::AnimationGraph base = MakeGraph();
    Concord::Scene scene;
    const Concord::Entity entity = scene.CreateEntity()
        .Add<Concord::AnimationComponent>(Concord::AnimationComponent{
            .asset = asset.get(), .skeletonIndex = 0, .clipIndex = 1})
        .Add<Concord::AnimationControllerComponent>(Concord::AnimationControllerComponent{
            .asset = asset.get(), .skeletonIndex = 0, .graph = &base})
        .Add<Concord::SkinningPoseComponent>(Concord::SkinningPoseComponent{})
        .Id();

    Concord::World& world = scene.GetWorld();
    if (Concord::UpdateAnimationComponents(world, 0.5f) != 1) return false;
    const auto* pose = world.Get<Concord::SkinningPoseComponent>(entity);
    return pose != nullptr && Near(pose->local[0].translation.x, 1.0f);
}

} // namespace

int main()
{
    struct Case {
        const char* name;
        bool (*run)();
    };
    const Case cases[] = {
        {"crossfade", TestCrossfade},
        {"exit-time", TestExitTimeTransition},
        {"invalid-graph", TestInvalidGraphFails},
        {"controller-end-to-end", TestControllerComponentEndToEnd},
        {"controller-overrides-legacy", TestControllerOverridesLegacyComponent},
    };
    for (const Case& testCase : cases) {
        if (!testCase.run()) {
            std::printf("FAIL %s\n", testCase.name);
            return 1;
        }
    }
    return 0;
}
