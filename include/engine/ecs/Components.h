// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_COMPONENTS_H
#define CONCORD_COMPONENTS_H

#include "engine/core/Color.h"
#include "engine/core/Transform.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"
#include "engine/scene/Material.h"

#include <string>

namespace Concord {

/**
 * Shapes the mesh renderer knows how to draw without an imported asset.
 */
enum class PrimitiveShape {
    Box,
    Sphere,
    Plane,
};

/**
 * Marks an entity as drawable and says what geometry to use.
 *
 * Kept separate from Material so that a shared mesh and a per-instance
 * surface can vary independently.
 */
struct MeshRenderer {
    /** Which built-in primitive to draw. */
    PrimitiveShape shape = PrimitiveShape::Box;

    /** Extent along each axis, before Transform::scale applies. */
    Vec3 size{1.0f, 1.0f, 1.0f};

    /** Whether this entity is submitted to the renderer at all. */
    bool visible = true;

    /** Whether this entity contributes to shadow maps. */
    bool castShadow = true;
};

/** Viewpoint parameters; pairs with a Transform for position. */
struct CameraComponent {
    /** Point the camera looks at, in world space. */
    Vec3 target{0.0f, 0.0f, 0.0f};

    /** World up direction. */
    Vec3 up{0.0f, 1.0f, 0.0f};

    /** Vertical field of view in degrees. */
    f32 fovYDegrees = 60.0f;

    /** Near clip distance; must be greater than zero. */
    f32 nearPlane = 0.1f;

    /** Far clip distance. */
    f32 farPlane = 1000.0f;

    /** The scene renders through the camera with the lowest priority. */
    i32 priority = 0;
};

/** Kinds of light the renderer supports. */
enum class LightType {
    Directional,
    Point,
    Spot,
};

/** Light emission parameters; pairs with a Transform for placement. */
struct LightComponent {
    /** Which lighting model applies. */
    LightType type = LightType::Directional;

    /** Emitted color. */
    ColorRGBA color = COLOR_RGB(255, 244, 214);

    /** Radiance multiplier. */
    f32 intensity = 3.0f;

    /**
     * Angle above the horizon in degrees, for directional lights: 0 sits at
     * the horizon and 90 straight overhead. Point and spot lights take their
     * position from the Transform instead.
     */
    f32 elevationDegrees = 45.0f;

    /** Compass direction in degrees, measured clockwise about the Y axis. */
    f32 azimuthDegrees = 200.0f;

    /** Reach of a point or spot light, in world units. */
    f32 range = 25.0f;

    /** Cone half-angle in degrees, for spot lights. */
    f32 spotAngleDegrees = 35.0f;

    /** Whether this light casts shadows. */
    bool castShadow = true;
};

/**
 * Human-readable label, attached by Spawn and useful when debugging a query.
 */
struct Name {
    std::string value;
};

} // namespace Concord

#endif // CONCORD_COMPONENTS_H
