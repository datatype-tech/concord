// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_WATERRIPPLEFIELD_H
#define CONCORD_WATERRIPPLEFIELD_H

#include "engine/core/Types.h"
#include "engine/core/Vec2.h"
#include "engine/particle/ParticleTypes.h"

namespace Concord {

/**
 * Scatters disturbances across a water surface at an authored rate.
 *
 * The rate is the control that decides what a body of water reads as. A dense
 * field is a downpour: a hundred rings of the same shape overlapping become a
 * texture, and the surface stops looking like water at all. A sparse one is a
 * surface something occasionally touches, which is what a pond actually looks
 * like. Leaving the rate at zero is a legitimate setting rather than a disabled
 * feature -- it is still water, and still reflects the sky; it is simply calm.
 */
struct WaterRippleFieldSettings {
    /**
     * Rings spawned per second across the whole area.
     *
     * Zero by default. A field is a decision a scene makes about its water,
     * and the overwhelming majority of water in a scene is not being rained
     * on: a default that spawns rings gives every calm surface in every
     * project the same disturbance it never asked for. Authored explicitly,
     * this is also the one control that decides whether a body reads as calm
     * or as a downpour.
     */
    f32 rate = 0.0f;

    /** Half-extent of the area rings are scattered over, on both axes. */
    Vec2 extent{10.0f, 10.0f};

    /**
     * Distance between rings, drawn per ring.
     *
     * Broad next to the amplitude on purpose. Steepness is amplitude times
     * wave number, so a short ring is a sharp one at any strength, and a
     * surface covered in short sharp rings is the one thing a calm basin must
     * not be.
     */
    ParticleRange wavelength{0.60f, 1.50f};

    /** How fast a ring's crests travel outward, drawn per ring. */
    ParticleRange speed{0.18f, 0.40f};

    /**
     * Slope amplitude at the centre, drawn per ring.
     *
     * Compared against the swell rather than read on its own: the default
     * surface sums to a slope of about 0.15 across all ten of its octaves, so
     * a disturbance at this level is a dimple on a mirror rather than a wave
     * crossing it.
     */
    ParticleRange strength{0.006f, 0.016f};

    /** Spatial decay per world unit; larger values keep a ring compact. */
    f32 falloff = 0.60f;

    /**
     * Radius past which a ring contributes nothing at all.
     *
     * Small on purpose. A single drop in a still basin leaves a disturbance a
     * metre across that is gone in a few seconds; a source that keeps lifting
     * the water out to six metres turns one drop into a wave that crosses the
     * whole body and meets every other wave on it.
     */
    f32 reach = 2.2f;

    /** Seconds a ring keeps emitting, drawn per ring. */
    ParticleRange lifetime{2.0f, 3.6f};

    /** Seed; the same seed and step sequence replays identically. */
    u32 seed = 0x9E3779B9u;

    /** Whether the field is spawning. Zero rate is calm water, not a bug. */
    bool enabled = true;
};

} // namespace Concord

#endif // CONCORD_WATERRIPPLEFIELD_H
