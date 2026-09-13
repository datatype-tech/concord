// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_BUOYANCYCOMPONENT_H
#define CONCORD_BUOYANCYCOMPONENT_H

#include "engine/core/Types.h"

namespace Concord {

/**
 * What happens to a rigid body once it is under water.
 *
 * Opt-in rather than automatic. A crate and a boulder both fall into the same
 * pond and only one of them comes back up, and nothing about either body's
 * collider says which -- so the scene says it, by attaching this or not.
 */
struct Buoyancy {
    /**
     * Density relative to water, so one is neutral.
     *
     * Below one the body rises until the share of it still submerged matches
     * this number, which is why a value is a waterline rather than a force:
     * 0.6 floats with two fifths of itself out of the water. Above one it
     * sinks, but slowly, because the displaced water still carries most of
     * its weight.
     */
    f32 density = 0.55f;

    /**
     * How hard the water resists being moved through, per second.
     *
     * This is what makes an entry read as an entry. Water is roughly eight
     * hundred times denser than air, so a body that keeps its fall speed after
     * crossing the surface reads as having gone through a hole in a sheet
     * rather than into a volume. The drag is also what stops the buoyancy
     * above from turning into a permanent oscillation: without it a float is
     * an undamped spring and the crate pumps up and down forever.
     */
    f32 drag = 2.6f;

    /** The same resistance applied to tumbling. */
    f32 angularDrag = 3.4f;
};

} // namespace Concord

#endif // CONCORD_BUOYANCYCOMPONENT_H
