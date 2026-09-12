// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_AUDIOSETTINGS_H
#define CONCORD_AUDIOSETTINGS_H

#include "engine/core/Types.h"

namespace Concord {

/**
 * Tunables for one audio world.
 *
 * The mix is always stereo float at `sampleRate`. Steam Audio's HRTF and the
 * SDL device, when present, are built at this rate so a clip is resampled
 * once on load rather than on every voice.
 */
struct AudioSettings {
    /** Mix and device rate, in Hertz. */
    u32 sampleRate = 48000;

    /** Samples produced per mix callback. Independent of channel count. */
    u32 frameSize = 512;

    /**
     * When true the world opens the default playback device.
     *
     * Tests turn this off so MixStereo can be measured without a speaker.
     * A missing device is not an error: voices still mix, they are just
     * not heard.
     */
    bool playOnDevice = true;
};

} // namespace Concord

#endif // CONCORD_AUDIOSETTINGS_H
