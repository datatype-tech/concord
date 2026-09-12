// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/core/Types.h"
#include "engine/cvm/CvmSceneAccess.h"
#include "engine/ecs/AnimationComponents.h"
#include "engine/scene/ModelRenderer.h"
#include "engine/scene/Scene.h"

#include <cstddef>
#include <cstdint>
#include <limits>

/**
 * Playback of a clip already sitting on a spawned model.
 *
 * The asset lives on the model renderer, so this never takes a model handle:
 * whatever the entity is drawing is the clip source. A missing skeleton or an
 * out-of-range clip is a failed call rather than a bind-pose that looks like
 * success.
 */
namespace {

bool ClipIndex(std::int64_t clip, std::size_t count, Concord::u32& index)
{
    if (clip < 0) return false;
    if (static_cast<std::uint64_t>(clip) > std::numeric_limits<Concord::u32>::max()) return false;
    index = static_cast<Concord::u32>(clip);
    return static_cast<std::size_t>(index) < count;
}

} // namespace

extern "C" {

std::int64_t ConcordCvmEntityPlayAnimation(std::int64_t entity, std::int64_t clip, double speed,
                                           std::int64_t loop)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity target;
    if (!Concord::Cvm::ResolveEntity(entity, scene, target)) return 0;
    Concord::ModelRenderer* renderer = scene->GetWorld().Get<Concord::ModelRenderer>(target);
    if (renderer == nullptr || renderer->asset == nullptr) return 0;
    const Concord::ModelAsset& asset = *renderer->asset;
    if (asset.skeletons.empty()) return 0;
    Concord::u32 clipIndex = 0;
    if (!ClipIndex(clip, asset.animations.size(), clipIndex)) return 0;

    Concord::AnimationComponent animation;
    animation.asset = renderer->asset.get();
    animation.skeletonIndex = 0;
    animation.clipIndex = clipIndex;
    animation.time = 0.0f;
    animation.speed = static_cast<Concord::f32>(speed);
    animation.loop = loop != 0;
    animation.playing = true;
    scene->GetWorld().Add<Concord::AnimationComponent>(target, animation);
    scene->GetWorld().Add<Concord::SkinningPoseComponent>(target, Concord::SkinningPoseComponent{});
    return 1;
}

std::int64_t ConcordCvmEntityStopAnimation(std::int64_t entity)
{
    return Concord::Cvm::EditComponent<Concord::AnimationComponent>(
        entity, [](Concord::AnimationComponent& animation) { animation.playing = false; });
}

std::int64_t ConcordCvmEntitySetAnimationSpeed(std::int64_t entity, double speed)
{
    return Concord::Cvm::EditComponent<Concord::AnimationComponent>(
        entity, [speed](Concord::AnimationComponent& animation) {
            animation.speed = static_cast<Concord::f32>(speed);
        });
}

double ConcordCvmEntityAnimationTime(std::int64_t entity)
{
    double time = 0.0;
    Concord::Cvm::EditComponent<Concord::AnimationComponent>(
        entity, [&time](Concord::AnimationComponent& animation) {
            time = static_cast<double>(animation.time);
        });
    return time;
}

} // extern "C"
