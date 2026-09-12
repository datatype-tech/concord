// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/cvm/CvmApi.h"

#include "engine/audio/AudioClip.h"
#include "engine/audio/AudioWorld.h"
#include "engine/core/Transform.h"
#include "engine/cvm/CvmSceneAccess.h"
#include "engine/ecs/AudioComponents.h"
#include "engine/ecs/World.h"
#include "engine/scene/Scene.h"
#include "engine/scene/Sound.h"

#include <cmath>
#include <cstdint>
#include <memory>

namespace {

Concord::f32 GainFromAbi(double volume) noexcept
{
    return std::isfinite(volume) && volume >= 0.0 ? static_cast<Concord::f32>(volume) : 0.0f;
}

} // namespace

extern "C" {

CENGINE_API std::int64_t ConcordCvmSceneSpawnListener(std::int64_t scene, double x, double y,
                                                      double z)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) {
        return 0;
    }
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::Listener>({
        .transform = {.position = Concord::Cvm::ToVec3(x, y, z)},
    });
    return Concord::Cvm::Record(scene, *found, handle);
}

CENGINE_API std::int64_t ConcordCvmEntityAddListener(std::int64_t entity, double gain)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0;
    }
    Concord::World& world = scene->GetWorld();
    if (!world.Has<Concord::Transform>(id)) {
        return 0;
    }
    const Concord::f32 ears = GainFromAbi(gain);
    if (Concord::AudioListener* listener = world.Get<Concord::AudioListener>(id)) {
        listener->gain = ears;
        listener->enabled = true;
        return 1;
    }
    world.Add<Concord::AudioListener>(id, Concord::AudioListener{.gain = ears});
    return 1;
}

CENGINE_API std::int64_t ConcordCvmSceneSpawnSound(std::int64_t scene, double x, double y, double z,
                                                   const char* path, double volume,
                                                   std::int64_t loop, std::int64_t spatial)
{
    Concord::Scene* found = Concord::Cvm::AcquireScene(scene);
    if (found == nullptr) {
        return 0;
    }
    std::shared_ptr<Concord::AudioClip> clip = Concord::AudioClip::Load(path);
    if (!clip || !clip->IsValid()) {
        return 0;
    }
    const Concord::EntityHandle handle = found->Spawn<Concord::Object::Sound>({
        .transform = {.position = Concord::Cvm::ToVec3(x, y, z)},
        .clip = std::move(clip),
        .volume = GainFromAbi(volume),
        .spatial = spatial != 0,
        .loop = loop != 0,
    });
    return Concord::Cvm::Record(scene, *found, handle);
}

CENGINE_API std::int64_t ConcordCvmEntityPlay(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0;
    }
    if (Concord::AudioWorld* audio = Concord::AudioWorld::Find(*scene)) {
        return audio->Play(id) ? 1 : 0;
    }
    Concord::AudioSource* source = scene->GetWorld().Get<Concord::AudioSource>(id);
    if (source == nullptr || !source->clip || !source->clip->IsValid()) {
        return 0;
    }
    source->playing = true;
    source->playOnStart = false;
    return 1;
}

CENGINE_API std::int64_t ConcordCvmEntityStop(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0;
    }
    if (Concord::AudioWorld* audio = Concord::AudioWorld::Find(*scene)) {
        audio->Stop(id);
        return scene->GetWorld().Has<Concord::AudioSource>(id) ? 1 : 0;
    }
    if (Concord::AudioSource* source = scene->GetWorld().Get<Concord::AudioSource>(id)) {
        source->playing = false;
        source->cursor = 0.0f;
        source->playOnStart = false;
        return 1;
    }
    return 0;
}

CENGINE_API std::int64_t ConcordCvmEntitySetVolume(std::int64_t entity, double volume)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0;
    }
    Concord::AudioSource* source = scene->GetWorld().Get<Concord::AudioSource>(id);
    if (source == nullptr) {
        return 0;
    }
    source->volume = GainFromAbi(volume);
    return 1;
}

CENGINE_API double ConcordCvmEntityVolume(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0.0;
    }
    const Concord::AudioSource* source = scene->GetWorld().Get<Concord::AudioSource>(id);
    return source != nullptr ? source->volume : 0.0;
}

CENGINE_API std::int64_t ConcordCvmEntitySetPitch(std::int64_t entity, double pitch)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0;
    }
    Concord::AudioSource* source = scene->GetWorld().Get<Concord::AudioSource>(id);
    if (source == nullptr || !std::isfinite(pitch) || pitch <= 0.0) {
        return 0;
    }
    source->pitch = static_cast<Concord::f32>(pitch);
    return 1;
}

CENGINE_API double ConcordCvmEntityPitch(std::int64_t entity)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0.0;
    }
    const Concord::AudioSource* source = scene->GetWorld().Get<Concord::AudioSource>(id);
    return source != nullptr ? source->pitch : 0.0;
}

CENGINE_API std::int64_t ConcordCvmEntitySetRange(std::int64_t entity, double minDistance,
                                                  double maxDistance)
{
    Concord::Scene* scene = nullptr;
    Concord::Entity id;
    if (!Concord::Cvm::ResolveEntity(entity, scene, id)) {
        return 0;
    }
    Concord::AudioSource* source = scene->GetWorld().Get<Concord::AudioSource>(id);
    if (source == nullptr || !std::isfinite(minDistance) || !std::isfinite(maxDistance) ||
        minDistance <= 0.0 || maxDistance <= minDistance) {
        return 0;
    }
    source->minDistance = static_cast<Concord::f32>(minDistance);
    source->maxDistance = static_cast<Concord::f32>(maxDistance);
    return 1;
}

} // extern "C"
