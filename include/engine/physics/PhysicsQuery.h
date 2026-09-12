// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_PHYSICSQUERY_H
#define CONCORD_PHYSICSQUERY_H

#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/ecs/Entity.h"

namespace Concord {

/**
 * The nearest body a physics ray met.
 *
 * Distance is along the authored direction after it has been normalised, so
 * a caller that passed a long vector still reads metres rather than a
 * parameter on that vector.
 */
struct PhysicsRayHit {
    Entity entity{};
    Vec3 point{};
    Vec3 normal{0.0f, 1.0f, 0.0f};
    f32 distance = 0.0f;
};

} // namespace Concord

#endif // CONCORD_PHYSICSQUERY_H
