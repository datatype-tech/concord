// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERRIPPLECOMPONENTS_H
#define CONCORD_WATERRIPPLECOMPONENTS_H

#include "engine/asset/WaterMaterial.h"
#include "engine/ecs/WaterRippleField.h"

namespace Concord {

/**
 * One disturbance travelling across a water surface, alive for a while.
 *
 * The ring is the same authored type a water body carries, so a runtime splash
 * and a hand-placed ripple are one thing drawn from one control surface. The
 * difference is that this one ages: `WaterRippleSystem` advances `age` and
 * retires the entity at `lifetime`, and the ring's strength fades across that
 * span so it dies away instead of disappearing between two frames.
 */
struct WaterRippleComponent {
    /** Ring shape, strength and reach, identical to the authored form. */
    WaterRipple ripple{};

    /** Seconds the disturbance keeps emitting before it is retired. */
    f32 lifetime = 2.4f;

    /** Seconds elapsed since it appeared. */
    f32 age = 0.0f;
};

/**
 * An area that keeps scattering disturbances onto whatever water covers it.
 *
 * Holds only the settings and the accumulator; the rings it produces are
 * ordinary entities the same system ages and retires, so a field has no
 * lifetime of its own and a ring's whole life is governed in one place.
 */
struct WaterRippleFieldComponent {
    WaterRippleFieldSettings settings{};

    /** Seconds of emission owed but not yet spawned. */
    f32 pending = 0.0f;

    /** Generator state, so a seed replays the same field. */
    u32 state = 1u;
};

} // namespace Concord

#endif // CONCORD_WATERRIPPLECOMPONENTS_H
