// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_SUNLIGHT_H
#define CONCORD_SUNLIGHT_H

#include "engine/core/Angle.h"
#include "engine/core/Color.h"
#include "engine/core/Transform.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"

#include <cmath>

namespace Concord::Object {

/** Every field a SunLight can be spawned from. */
struct SunLightDesc {
    /** Angle above the horizon in degrees: 0 at the horizon, 90 straight overhead. */
    f32 elevationDegrees = 45.0f;

    /** Compass direction in degrees, measured clockwise about the Y axis. */
    f32 azimuthDegrees = 200.0f;

    /** Light color. */
    ColorRGBA color = COLOR_RGB(255, 244, 214);

    /** Radiance multiplier. */
    f32 intensity = 3.0f;

    /** Whether this light casts shadows. */
    bool castShadow = true;
};

/**
 * Directional light standing in for the sun.
 *
 * Authored as elevation and azimuth because those are the two numbers an
 * artist reasons about; the direction vector is derived on demand.
 */
struct SunLight {
    using Desc = SunLightDesc;

    static void Build(World& world, Entity entity, const SunLightDesc& desc)
    {
        world.Add<Transform>(entity, Transform{});
        world.Add<LightComponent>(entity, LightComponent{
            .type = LightType::Directional,
            .color = desc.color,
            .intensity = desc.intensity,
            .elevationDegrees = desc.elevationDegrees,
            .azimuthDegrees = desc.azimuthDegrees,
            .castShadow = desc.castShadow,
        });
    }
};

/** Incident direction of a directional light, pointing from it toward the scene. */
[[nodiscard]] inline Vec3 LightDirection(const LightComponent& light) noexcept
{
    const f32 elevation = Radians(light.elevationDegrees);
    const f32 azimuth = Radians(light.azimuthDegrees);
    const f32 cosElevation = std::cos(elevation);
    return Normalize({-cosElevation * std::sin(azimuth),
                      -std::sin(elevation),
                      -cosElevation * std::cos(azimuth)});
}

} // namespace Concord::Object

#endif // CONCORD_SUNLIGHT_H
