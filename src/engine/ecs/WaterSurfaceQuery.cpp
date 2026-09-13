// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/WaterSurfaceQuery.h"

#include "engine/core/Transform.h"
#include "engine/ecs/WaterBodyComponent.h"
#include "engine/ecs/World.h"

#include <cmath>

namespace Concord {

void CollectWaterSurfaces(World& world, std::vector<WaterSurface>& surfaces)
{
    surfaces.clear();
    world.Query<WaterBodyComponent, Transform>(
        [&surfaces](Entity, const WaterBodyComponent& body, const Transform& transform) {
            const f32 halfX = body.extent.x * std::abs(transform.scale.x);
            const f32 halfZ = body.extent.y * std::abs(transform.scale.z);
            if (!(halfX > 0.0f) || !(halfZ > 0.0f)) {
                return;
            }
            surfaces.push_back(WaterSurface{.worldY = transform.position.y,
                                            .centreX = transform.position.x,
                                            .centreZ = transform.position.z,
                                            .halfExtentX = halfX,
                                            .halfExtentZ = halfZ});
        });
}

const WaterSurface* FindWaterSurface(const std::vector<WaterSurface>& surfaces, f32 x,
                                     f32 z) noexcept
{
    for (const WaterSurface& surface : surfaces) {
        if (std::abs(x - surface.centreX) <= surface.halfExtentX &&
            std::abs(z - surface.centreZ) <= surface.halfExtentZ) {
            return &surface;
        }
    }
    return nullptr;
}

} // namespace Concord
