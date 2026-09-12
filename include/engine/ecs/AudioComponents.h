// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_AUDIOCOMPONENTS_H
#define CONCORD_AUDIOCOMPONENTS_H

#include "engine/audio/AudioClip.h"

#include <memory>

namespace Concord {

/**
 * Ears the mixer orients Steam Audio around.
 *
 * Pair with a Transform. When no listener exists, AudioSystem uses the
 * scene's main camera so a first-person view hears from where it looks.
 */
struct AudioListener {
    f32 gain = 1.0f;
    bool enabled = true;
};

/**
 * One playing (or ready-to-play) voice attached to an entity.
 *
 * Behaviour stays in AudioSystem. This component is the authored state a
 * query can read: which clip, how loud, whether it loops, and whether the
 * voice is currently advancing.
 */
struct AudioSource {
    std::shared_ptr<AudioClip> clip{};
    f32 volume = 1.0f;
    f32 pitch = 1.0f;
    /** Distance at which a spatial voice is still at full volume. */
    f32 minDistance = 1.0f;
    /** Distance at which a spatial voice falls silent. */
    f32 maxDistance = 48.0f;
    /** When true the voice is HRTF-spatialized from Transform. */
    bool spatial = true;
    bool loop = false;
    bool playing = false;
    /** Set playing on the first AudioSystem tick. */
    bool playOnStart = true;
    /** Playback cursor in clip frames; owned by the mixer. */
    f32 cursor = 0.0f;
};

} // namespace Concord

#endif // CONCORD_AUDIOCOMPONENTS_H
