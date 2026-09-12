// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SOUND_H
#define CONCORD_SOUND_H

#include "engine/core/Transform.h"
#include "engine/ecs/AudioComponents.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"

#include <memory>

namespace Concord::Object {

/** Ears placed in the world. Usually attached to the camera instead. */
struct ListenerDesc {
    Transform transform{};
    f32 gain = 1.0f;
};

struct Listener {
    using Desc = ListenerDesc;

    static void Build(World& world, Entity entity, const ListenerDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<AudioListener>(entity, AudioListener{.gain = desc.gain});
    }
};

/** A positioned voice. The clip is shared; the cursor is per entity. */
struct SoundDesc {
    Transform transform{};
    std::shared_ptr<AudioClip> clip{};
    f32 volume = 1.0f;
    f32 pitch = 1.0f;
    f32 minDistance = 1.0f;
    f32 maxDistance = 48.0f;
    bool spatial = true;
    bool loop = false;
    bool playOnStart = true;
};

struct Sound {
    using Desc = SoundDesc;

    static void Build(World& world, Entity entity, const SoundDesc& desc)
    {
        world.Add<Transform>(entity, desc.transform);
        world.Add<AudioSource>(entity, AudioSource{
            .clip = desc.clip,
            .volume = desc.volume,
            .pitch = desc.pitch,
            .minDistance = desc.minDistance,
            .maxDistance = desc.maxDistance,
            .spatial = desc.spatial,
            .loop = desc.loop,
            .playing = false,
            .playOnStart = desc.playOnStart,
        });
    }
};

} // namespace Concord::Object

#endif // CONCORD_SOUND_H
