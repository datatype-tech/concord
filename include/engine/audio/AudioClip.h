// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_AUDIOCLIP_H
#define CONCORD_AUDIOCLIP_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"

#include <memory>
#include <span>
#include <vector>

namespace Concord {

/**
 * Immutable mono PCM a voice can read.
 *
 * Samples are stored as 32-bit float at 48 kHz after load, which is the
 * rate Steam Audio and the SDL device both run at. A clip is shared: many
 * AudioSource components may point at the same one.
 */
class CENGINE_API AudioClip {
public:
    AudioClip() = default;

    /**
     * Loads a RIFF WAVE file (PCM16 or float) and converts it to mono 48 kHz.
     *
     * @return An empty clip when the path cannot be read or the format is
     *         not a WAVE the loader understands.
     */
    [[nodiscard]] static std::shared_ptr<AudioClip> Load(const char* path);

    /**
     * Builds a clip from interleaved PCM16 sitting in memory.
     *
     * Channels are folded to mono and the rate is resampled to 48 kHz.
     */
    [[nodiscard]] static std::shared_ptr<AudioClip> FromPcm16(std::span<const i16> interleaved,
                                                              u32 channels, u32 sampleRate);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] u32 FrameCount() const noexcept;
    [[nodiscard]] u32 SampleRate() const noexcept;

    /** Mono samples in -1..1. Empty when the clip is invalid. */
    [[nodiscard]] std::span<const f32> Samples() const noexcept;

private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;

    static std::shared_ptr<AudioClip> Adopt(std::vector<f32> samples);
};

} // namespace Concord

#endif // CONCORD_AUDIOCLIP_H
