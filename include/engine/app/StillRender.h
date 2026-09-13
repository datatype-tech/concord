// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_STILLRENDER_H
#define CONCORD_STILLRENDER_H

#include "engine/core/Types.h"

namespace Concord {

/**
 * How a single offline frame is produced, for `Game::RenderStill`.
 *
 * A still is not one iteration of the frame loop written to a file. A scene
 * that has only ever been described has nothing settled about it: bodies are
 * wherever they were authored rather than where they come to rest, emitters
 * have produced no particles, the clock has not moved, and any system that
 * builds its state incrementally has run zero times. Rendering that frame
 * gives a picture of a scene mid-construction. The warm-up is what makes the
 * still a picture of the scene instead.
 */
struct StillRenderDesc {
    /**
     * Frames simulated before the one that is kept.
     *
     * The default is enough for a dropped body to fall a couple of metres and
     * for a particle system to reach steady state, which is what most scenes
     * need before they photograph well. A scene with nothing in motion can set
     * it to zero and pay nothing.
     */
    u32 warmupFrames = 90;

    /**
     * Seconds each of those frames advances the simulation.
     *
     * Fixed rather than measured. An offline render has no frame rate to
     * measure -- a 4K trace takes as long as it takes -- and feeding the real
     * elapsed time to the systems would make the amount of simulated time
     * depend on how fast the machine happens to be, so the same scene would
     * photograph differently on two computers.
     */
    f32 deltaTime = 1.0f / 60.0f;

    /**
     * Whether the debug overlay is drawn into the still.
     *
     * Off, unlike the live window: a still is usually wanted as an image of
     * the scene, and the frame counter in the corner is the one thing in it
     * that is not.
     */
    bool includeOverlay = false;
};

} // namespace Concord

#endif // CONCORD_STILLRENDER_H
