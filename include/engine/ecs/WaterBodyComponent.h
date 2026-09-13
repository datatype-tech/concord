// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERBODYCOMPONENT_H
#define CONCORD_WATERBODYCOMPONENT_H

#include "engine/core/Vec2.h"

namespace Concord {

/**
 * Horizontal footprint `WaterSplashSystem` tests dynamic bodies against.
 *
 * Water itself carries no Jolt collider -- a body of water is a material on a
 * flat quad, not a volume the solver should push against, or anything falling
 * onto it would stop dead at the surface instead of splashing through. This
 * component is the physics-free substitute: an axis-aligned rectangle in the
 * owning entity's local X/Z plane (scaled by `Transform::scale`, ignoring
 * rotation, which matches every water asset actually authored so far), at the
 * entity's `Transform::position.y`. `WaterSplashSystem` walks it once per
 * frame to answer "is this point over some water, and how high is it".
 *
 * Either axis at zero disables the surface for splash purposes without
 * removing the component -- a vertical `Object::Waterfall` sheet has no
 * horizontal footprint to fall onto, so it is spawned with the default.
 */
struct WaterBodyComponent {
    /** Half extent of the splash rectangle, in local X/Z, before scale. */
    Vec2 extent{};
};

} // namespace Concord

#endif // CONCORD_WATERBODYCOMPONENT_H
