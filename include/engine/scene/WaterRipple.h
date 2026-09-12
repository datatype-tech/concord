// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERRIPPLE_H
#define CONCORD_WATERRIPPLE_H

#include "engine/asset/WaterMaterial.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/WaterRippleComponents.h"
#include "engine/ecs/World.h"

namespace Concord::Object {

/** Every field a water ripple can be spawned from. */
struct WaterRippleDesc {
    /** Ring shape: centre, wavelength, speed, strength, falloff and reach. */
    WaterRipple ripple{};

    /** Seconds the disturbance keeps emitting before it is retired. */
    f32 lifetime = 2.4f;
};

/**
 * A disturbance on the water surface, spawned while the game runs.
 *
 * Spawning one of these is how a scene reacts to something happening on the
 * water -- a body entering it, a projectile landing, rain -- without the water
 * body knowing anything about it. The ring is picked up by whichever surfaces
 * its reach covers, and `WaterRippleSystem` retires it when it has run its
 * course.
 *
 * @code
 * scene.Spawn<Object::WaterRipple>({
 *     .ripple = {.centre = {2.0f, 1.5f}, .wavelength = 1.8f, .strength = 0.8f},
 * });
 * @endcode
 */
struct WaterRipple {
    using Desc = WaterRippleDesc;

    /** Attaches the ripple component that the system ages and retires. */
    static void Build(World& world, Entity entity, const WaterRippleDesc& desc)
    {
        WaterRippleComponent component{};
        component.ripple = desc.ripple;
        // A non-positive lifetime would retire the ripple on the very frame it
        // appeared, which reads as the call having done nothing at all.
        component.lifetime = desc.lifetime > 0.0f && desc.lifetime == desc.lifetime
                                 ? desc.lifetime
                                 : 0.016f;
        world.Add<WaterRippleComponent>(entity, component);
    }
};

/** Every field a water ripple field can be spawned from. */
struct WaterRippleFieldDesc {
    /** Rate, area, ring shape and reach; see WaterRippleFieldSettings. */
    WaterRippleFieldSettings settings{};
};

/**
 * An area that keeps disturbing whatever water covers it.
 *
 * The difference between a pond and a downpour is this one number: a dense
 * field makes a hundred identical rings overlap into a texture, while a sparse
 * one reads as a surface something occasionally touches. `rate = 0` is calm
 * water -- still water, still reflecting the sky, simply undisturbed.
 */
struct WaterRippleField {
    using Desc = WaterRippleFieldDesc;

    /** Attaches the field component the ripple system spawns from. */
    static void Build(World& world, Entity entity, const WaterRippleFieldDesc& desc)
    {
        WaterRippleFieldComponent component{};
        component.settings = desc.settings;
        component.state = desc.settings.seed | 1u;
        world.Add<WaterRippleFieldComponent>(entity, component);
    }
};

} // namespace Concord::Object

#endif // CONCORD_WATERRIPPLE_H
