// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ASSET_WATERMATERIAL_H
#define CONCORD_ASSET_WATERMATERIAL_H

#include "Concord/CExport.h"
#include "engine/core/Vec2.h"
#include "engine/core/Vec3.h"

#include <array>

namespace Concord {

struct ModelAsset;

/** Number of authored ripple sources one body of water can carry. */
inline constexpr u32 kMaxWaterRipples = 4;

/**
 * One radial ripple source: rings expanding from a fixed point on the surface.
 *
 * The rings are analytic functions of the hit position, so a source costs a
 * dozen instructions per shaded pixel and needs no simulation buffer, no
 * texture and no per-frame upload. Only `centre` places the disturbance; the
 * rings travel outward from wherever it sits.
 */
struct WaterRipple {
    /** World-space horizontal position the rings expand from. */
    Vec2 centre{};
    /** Distance between successive crests, in world units. */
    f32 wavelength = 6.0f;
    /** Crest propagation speed, in world units per second. */
    f32 speed = 1.8f;
    /** Slope amplitude at the centre. Zero disables the source. */
    f32 strength = 0.0f;
    /** Spatial decay per world unit; larger values keep the rings local. */
    f32 falloff = 0.10f;
    /**
     * Radius past which the source contributes nothing at all.
     *
     * The falloff alone only makes a ring faint with distance, which is not
     * the same as absent: at a hundred units a source still lifted the water.
     * A hard reach is what lets every water body in a scene share one list of
     * disturbances -- a splash on one pond cannot reach another.
     *
     * It doubles as the source's own lifetime when it is authored rather than
     * spawned: an authored ring repeats once its wavefront has travelled this
     * far, so the ring is born at the centre and dies at the rim.
     */
    f32 reach = 6.0f;
};

/**
 * Authored appearance and motion of one body of water.
 *
 * Every field is a control rather than a constant baked into a shader: the sea
 * state, the current, how much of what lies below shows through, the optics of
 * the volume, and the localized disturbances running on top of the swell.
 * Shading is the only thing this drives -- the mesh stays exactly where it was
 * authored and every bit of detail comes from the evaluated hit position, so a
 * flat quad renders as any sea state and no simulation buffer is ever needed.
 */
struct WaterMaterial {
    /**
     * Multiplies every octave's steepness. Zero leaves the surface flat.
     *
     * The default is a still body rather than open water, because that is what
     * most bodies of water are and what a scene that says nothing about its
     * surface should get. Authored at this level the ten octaves sum to a
     * slope of about 0.15, which is a mirror that breathes rather than a sea:
     * a surface only reads as water because it is never perfectly still, and
     * the point of the default is to be that and no more. Ask for
     * MakeOceanSurface() to get a sea.
     */
    f32 waveAmplitude = 0.16f;
    /** Multiplies every octave's wavelength; larger values make a longer swell. */
    f32 waveScale = 1.6f;
    /** Multiplies every octave's phase speed. */
    f32 waveSpeed = 0.35f;
    /** Gerstner steepness: 0 rounds the swells, 1 peaks them toward breaking. */
    f32 choppiness = 0.28f;

    /** Horizontal heading the swell octaves are fanned around. */
    Vec2 heading{1.0f, 0.35f};
    /** How far the octaves fan away from the heading, in radians. */
    f32 spread = 1.1f;
    /** Radians per second the whole wave field swings, so it never settles. */
    f32 drift = 0.010f;

    /** Fraction of the body replaced by what is seen through it, 0 to 1. */
    f32 opacity = 0.85f;
    /** Index of refraction. 1.0 passes the ray through unbent. */
    f32 ior = 1.333f;
    /** Strength of the Snell bend: 0 goes straight through, 1 is full refraction. */
    f32 refraction = 1.0f;
    /** Per-channel extinction of light travelling through the volume. */
    Vec3 absorption{0.55f, 0.12f, 0.075f};
    /** Distance over which a channel with extinction 1.0 loses all but 1/e. */
    f32 absorptionDistance = 6.0f;

    /**
     * How much light the volume itself scatters back toward the eye.
     *
     * Absorbing alone makes water a tinted window: whatever is behind it fades,
     * and where nothing is behind it the surface goes black. What actually
     * makes deep water read as water is that the medium scatters the light
     * entering it, which is why a lake is blue-green at depth rather than
     * dark. Zero leaves the volume purely absorbing. The scattering colour is
     * derived from the absorption rather than authored separately, because
     * water absorbs selectively and scatters nearly everything equally.
     *
     * Quoted in the same units as `absorption`, which is to say as a rate
     * relative to `absorptionDistance`. It has to stay small next to the
     * absorption to read as water: once scattering dominates the extinction the
     * single-scattering albedo approaches one for every wavelength at once, the
     * colour stops depending on how far the light travelled, and the volume
     * turns into milk.
     */
    f32 scattering = 0.014f;

    /**
     * Crest height at which foam starts to gather, on a normalized -1..1 crest.
     *
     * Normalized rather than raw so the value survives a change of sea state:
     * the raw swell sum grows with both amplitude and wavelength, and a
     * threshold authored against one of them would miss on the next. One is
     * the very top of a crest, so foam is switched off with foamIntensity
     * rather than by pushing this past anything reachable.
     *
     * The default sits high because the default surface is a still one: foam
     * is what a breaking wave leaves behind, and calm water only ever gathers
     * a trace of it.
     */
    f32 foamThreshold = 0.80f;
    /** How strongly foam lightens a crest. */
    f32 foamIntensity = 0.18f;

    /**
     * How strongly the surface falls instead of swelling, 0 for a basin.
     *
     * Packed into the shader's unused surface.w. Above zero the hit uses the
     * mesh normal and a downward streak field, which is what a waterfall is;
     * the horizontal Gerstner sum would otherwise shade a vertical sheet as
     * a pond stood on its side.
     */
    f32 fall = 0.0f;

    /** Localized disturbances layered on top of the swell. */
    std::array<WaterRipple, kMaxWaterRipples> ripples{};
};

/**
 * A sea state for open water rather than a basin.
 *
 * Longer swell, harder crests, deeper extinction and no authored ring: an ocean
 * reads through its swell and its colour gradient with distance, and a
 * hand-placed ring on it looks like a stamp. The reach widens with the swell so
 * a disturbance spawned on open water still travels.
 *
 * The surface's altitude is the spawning transform's business, not the
 * material's, so nothing here decides where the sea level is.
 */
[[nodiscard]] CENGINE_API WaterMaterial MakeOceanSurface() noexcept;

/**
 * A basin at rest: the preset a scene that wants water and nothing else gets.
 *
 * Every field matches the structure's own defaults, and the function exists
 * anyway so that a caller can name the intent instead of relying on them -- a
 * reader should not have to know that an empty brace means "still" rather than
 * "unset". The surface is a mirror that breathes: a swell small enough that the
 * reflection never breaks up, no ripple source lit, and foam all but switched
 * off.
 */
[[nodiscard]] CENGINE_API WaterMaterial MakeStillWater() noexcept;

/**
 * A flowing channel: shorter waves, a tight heading, and greener extinction.
 *
 * A river is not a narrow ocean. The swell stays aligned with the current so
 * the surface reads as water moving along a bed rather than as open swell
 * that happens to be cropped.
 */
[[nodiscard]] CENGINE_API WaterMaterial MakeRiverWater() noexcept;

/**
 * A falling sheet: fast, short, foamy, and almost opaque.
 *
 * The mesh is what makes it a waterfall -- this preset is the motion and the
 * foam a vertical sheet needs so it does not render as a standing pond on
 * its side.
 */
[[nodiscard]] CENGINE_API WaterMaterial MakeWaterfall() noexcept;

/**
 * Clamps every field into the range the hit shader can consume.
 *
 * @return Whether the material was already inside those ranges, so a caller
 *         that authored an out-of-range value can report it rather than
 *         silently rendering something else.
 */
[[nodiscard]] CENGINE_API bool SanitizeWaterMaterial(WaterMaterial& material) noexcept;

/**
 * Stamps one surface onto every material of a model asset.
 *
 * Water is a material property rather than a geometry type, so a body of water
 * is an ordinary model whose materials carry a `WaterMaterial`. A body that
 * owns its asset exclusively is the expected arrangement: two bodies sharing
 * one asset also share the surface stamped onto it last.
 *
 * @return Whether the asset had at least one material to stamp.
 */
[[nodiscard]] CENGINE_API bool ApplyWaterMaterial(ModelAsset& asset,
                                                  const WaterMaterial& material) noexcept;

} // namespace Concord

#endif // CONCORD_ASSET_WATERMATERIAL_H
