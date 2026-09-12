// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PARTICLECOMPONENTS_H
#define CONCORD_PARTICLECOMPONENTS_H

#include "engine/particle/ParticleTypes.h"

namespace Concord {

/**
 * An emitter together with the pool of particles it currently owns.
 *
 * The pool lives in the component rather than in the system so that a Scene
 * remains the single component store, and so that a handle still resolves to
 * its particles after a system is re-registered.
 */
struct ParticleEmitterComponent {
    /** Spawn and integration parameters. */
    ParticleEmitterSettings settings{};

    /** Live particles and generator state, advanced by ParticleSystem. */
    ParticleEmitterState state{};

    /**
     * Set to true to re-arm the emitter on the next update.
     *
     * Re-arming reallocates the pool from the current settings and discards
     * every live particle, which is how a runtime capacity change takes
     * effect. ParticleSystem clears the flag once it has restarted.
     */
    bool restart = false;

    /** Counters from the most recent step, for HUDs and debugging. */
    ParticleStepResult lastStep{};
};

} // namespace Concord

#endif // CONCORD_PARTICLECOMPONENTS_H
