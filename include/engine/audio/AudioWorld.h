// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_AUDIOWORLD_H
#define CONCORD_AUDIOWORLD_H

#include "Concord/CExport.h"
#include "engine/audio/AudioSettings.h"
#include "engine/core/Types.h"
#include "engine/ecs/Entity.h"

#include <memory>

namespace Concord {

class Scene;

/**
 * One stereo mix, hidden behind pimpl so public headers never name Steam
 * Audio or SDL.
 *
 * A Scene does not own this: AudioSystem does, and binds the pair so a
 * script can Play() the same voices the waterfall is dripping into.
 */
class CENGINE_API AudioWorld {
public:
    explicit AudioWorld(const AudioSettings& settings = {});
    ~AudioWorld();

    AudioWorld(const AudioWorld&) = delete;
    AudioWorld& operator=(const AudioWorld&) = delete;

    /** Remembers which scene this mix is hearing. */
    void BindScene(Scene& scene);

    /** Clears the Find() registration and stops every voice. */
    void UnbindScene();

    /**
     * Copies listener and source state out of the scene for the mixer.
     *
     * Safe to call on the main thread while the device callback is mixing.
     */
    void Step(Scene& scene, f32 deltaSeconds);

    /** Starts the source on `entity`. Returns false when it has no clip. */
    bool Play(Entity entity);

    /** Stops the source and rewinds its cursor. */
    bool Stop(Entity entity);

    /**
     * Mixes the current voices into interleaved stereo float frames.
     *
     * Used by the device callback and by tests. Does not require a device.
     */
    void MixStereo(f32* interleaved, u32 frames);

    /** True when SDL opened a playback device. */
    [[nodiscard]] bool DeviceOpen() const noexcept;

    /** True when Steam Audio's HRTF is live; otherwise voices equal-power pan. */
    [[nodiscard]] bool HrtfReady() const noexcept;

    /** The world bound to `scene`, or nullptr when audio is not running. */
    [[nodiscard]] static AudioWorld* Find(const Scene& scene);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Concord

#endif // CONCORD_AUDIOWORLD_H
