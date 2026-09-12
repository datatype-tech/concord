// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/AudioSystem.h"

#include "engine/scene/Scene.h"

namespace Concord {

AudioSystem::AudioSystem(const AudioSettings& settings) : m_settings(settings)
{
    m_world = std::make_unique<AudioWorld>(m_settings);
}

AudioSystem::~AudioSystem() = default;

void AudioSystem::OnStart(Scene& scene)
{
    m_world->BindScene(scene);
}

void AudioSystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    m_world->Step(scene, deltaTime);
}

void AudioSystem::OnStop(Scene&)
{
    m_world->UnbindScene();
}

bool AudioSystem::Play(Entity entity)
{
    return m_world->Play(entity);
}

bool AudioSystem::Stop(Entity entity)
{
    return m_world->Stop(entity);
}

} // namespace Concord
