// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_ENVIRONMENTSETTINGS_H
#define CONCORD_ENVIRONMENTSETTINGS_H

#include "engine/core/Color.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"

namespace Concord {

/** Sky and ambient lighting for a scene. */
struct EnvironmentSettings {
    /**
     * Linear sky colour straight overhead and at the horizon.
     *
     * Separate from skyColor below, which is an sRGB clear colour: these two
     * are what a sky is actually drawn from, and a sky cannot be authored in a
     * display-referred space without clipping its own sunset.
     */
    Vec3 zenithColor{0.030f, 0.055f, 0.130f};
    Vec3 horizonColor{0.075f, 0.105f, 0.215f};

    /** Cloud coverage, 0 for clear sky and 1 for overcast. */
    f32 cloudCoverage = 0.0f;

    /**
     * Extinction of the layer where it is at full density, per world unit.
     *
     * An extinction coefficient, exactly like fogDensity. The default thickness
     * is a couple of hundred units, so this puts a full-density core at an
     * optical depth of about two: solid enough to read as a volume, soft enough
     * that the rim still lets the sky through.
     */
    f32 cloudDensity = 0.0090f;

    /**
     * Altitude of the cloud layer's floor above the scene, in world units.
     *
     * The layer is sampled in world space, so altitude is a real distance
     * rather than a parameter of a projection: a camera that climbs toward the
     * floor sees the clouds grow and slide past, and one that rises through it
     * ends up above the weather. That is only true because the march below
     * evaluates the noise at the sample point, which is the departure from the
     * dome-sampled layer this replaced.
     *
     * Kept low on purpose. What a viewer reads as "far away" is not the
     * altitude but the elevation a cloud first appears at, and that is
     * altitude / tan(elevation): at five kilometres a layer is only reached by
     * rays within roughly thirty degrees of straight up, so the sky looks
     * empty everywhere the eye actually rests and the weather collapses into a
     * strip along the top of the frame. A layer of a few hundred metres is the
     * same weather seen from underneath it, which is where a sky is read from.
     */
    f32 cloudAltitude = 200.0f;

    /**
     * Depth of the layer, in world units.
     *
     * Read together with cloudAltitude, because what an eye judges is the
     * ratio: a layer much thinner than it is high is a sheet, and one whose
     * depth rivals its altitude is a sky with a floor and a ceiling. The
     * default matches them, which is what gives a cumulus a shadowed base, a
     * lit shoulder and enough depth between them for the two to be different
     * colours.
     */
    f32 cloudThickness = 200.0f;

    /**
     * Size of the largest weather cells, in world units.
     *
     * The weather is what decides *where* there are clouds, as against the
     * shape noise that decides what one cloud looks like: it is sampled over
     * the ground and is the only thing on this list authored at the scale of a
     * landscape. Setting it near cloudAltitude gives one cell across the whole
     * sky; the default gives a handful, which is a weather system rather than a
     * uniform overcast.
     */
    f32 cloudWeatherScale = 380.0f;

    /**
     * Size of the detail shapes carving a cloud's edge, in world units.
     *
     * About an eighth of cloudWeatherScale at the defaults, which is the
     * ratio that makes erosion read as billows on a cloud instead of as a
     * second, smaller sky of clouds sitting inside the first one.
     */
    f32 cloudDetailScale = 50.0f;

    /**
     * How deeply the detail shapes are carved out of a cloud's edge, 0 to 1.
     *
     * The one control that decides whether a cloud has a fractal boundary or a
     * soft one. Held back from the core: the carve is weighted by how close a
     * sample already is to the boundary, so a high value thins the rims without
     * hollowing the middle.
     */
    f32 cloudDetailStrength = 0.28f;

    /**
     * Cloud type, 0 for a flat stratus sheet and 1 for a towering cumulus.
     *
     * The two differ in where through the layer their mass sits, so this is a
     * single number interpolating one vertical profile into the other. It is
     * varied per weather cell as well, which is what lets one sky hold both.
     */
    f32 cloudType = 0.76f;

    /**
     * Fine ray-march steps taken through the part of the layer that has cloud.
     *
     * Zero disables the layer entirely. A coarse pass runs first and finds that
     * part, so these steps are spent on cloud rather than on the empty sky in
     * front of it -- which is what makes detail affordable, and the reason this
     * count buys shape here where it bought nothing before.
     */
    u32 cloudSteps = 32;

    /**
     * Coarse steps used to find the interval of the layer that holds cloud.
     *
     * Each one is a single low-frequency sample, an order cheaper than a fine
     * step, and between them they are the difference between marching the whole
     * depth of the layer and marching the tenth of it that is not empty.
     */
    u32 cloudCoarseSteps = 8;

    /**
     * Steps taken toward the sun from each marched sample.
     *
     * Zero lights the layer with no shadowing at all, which is flat but free.
     * Every step is a density evaluation, so this is the one control in the
     * cloud worth counting beyond the march itself; the strides grow as they
     * go, because a cloud far along the ray contributes an optical depth its
     * own detail has already averaged away.
     */
    u32 cloudLightSteps = 5;

    /** How fast the layer is carried along, in world units per second. */
    f32 cloudDriftSpeed = 24.0f;

    /**
     * How brightly the droplets light the sky back.
     *
     * Multiplies the frame's directional light, so it is the sun's own colour
     * that a cloud turns white at noon and deep orange at dusk.
     */
    f32 cloudLightGain = 3.8f;

    /**
     * How brightly the sky lights the inside of a cloud.
     *
     * A single-scattering model with an honest optical depth renders a thick
     * cumulus as a hole punched in the sky, because the light that actually
     * reaches its interior arrived by bouncing. This stands in for that: it is
     * weighted by how dark the direct term has become, so it fills the shadowed
     * parts and leaves the lit ones alone.
     */
    f32 cloudAmbientGain = 2.4f;

    /**
     * Multiplier on every directional light in the scene.
     *
     * The one control that answers "the sun is too bright" without touching
     * every light an author placed, and the one a settings screen would move.
     * It scales the sun's radiance rather than the exposure below, so a scene
     * dimmed with it darkens the way less light does -- shadows stay put, the
     * sun's own colour is preserved, and the sky the clouds are lit by follows
     * because both read the same light.
     *
     * Point and spot lights are deliberately not scaled by it: this is the sun,
     * and a scene that wants its lamps dimmed has their own intensity to move.
     */
    f32 sunIntensity = 1.0f;

    /** Color the framebuffer is cleared to, standing in for the sky. */
    ColorRGBA skyColor = COLOR_RGB(38, 48, 66);

    /** Ambient light color. */
    ColorRGBA ambientColor = COLOR_RGB(148, 168, 209);

    /** Ambient light strength. */
    f32 ambientIntensity = 0.4f;

    /**
     * Linear exposure applied before the tone curve.
     *
     * Scene-referred: doubling it doubles the light reaching the film, which
     * is what a stop is. One leaves the authored lighting untouched, so a scene
     * that already reads correctly is not dimmed by the grade being switched on.
     */
    f32 exposure = 1.0f;

    /** Strength of the film S-curve around mid grey. 0 is a straight line. */
    f32 contrast = 0.30f;

    /** Colourfulness around luminance. 1 leaves it alone, 0 is monochrome. */
    f32 saturation = 1.12f;

    /**
     * Corner darkening, as a fraction of the frame's half-diagonal.
     *
     * A lens falls off toward its edges and a print is rarely lit flat; the
     * gradient is what stops a wide shot reading as a flat panel.
     */
    f32 vignette = 0.34f;

    /**
     * Exposed luminance above which a surface starts to glow.
     *
     * Bloom is light scattering inside the lens, so it belongs to the sun and
     * to specular peaks. A threshold that sits inside ordinary diffuse
     * lighting smears every cube into a halo of its own colour.
     */
    f32 bloomThreshold = 8.5f;

    /** How much of the blurred highlight is added back. */
    f32 bloomIntensity = 0.12f;

    /** Radius of the widest gather, in pixels of the traced frame. */
    f32 bloomRadius = 14.0f;

    /**
     * Radial colour fringing, as a fraction of the frame at the corner.
     *
     * A real lens focuses each wavelength at a slightly different radius, so
     * the error grows with distance from the optical centre. Zero disables it.
     */
    f32 chromaticAberration = 0.0f;

    /**
     * Extinction of the participating medium at its base height, per world unit.
     *
     * Zero disables the medium entirely and costs nothing beyond one branch.
     * This is the control that decides whether a scene has air in it: without a
     * medium every surface sits at the same distance from the eye, and a wide
     * shot reads as a flat diagram rather than as a place.
     */
    f32 fogDensity = 0.0f;

    /** How quickly the medium thins with height, per world unit. */
    f32 fogHeightFalloff = 0.055f;

    /** Altitude the density is quoted at, so a scene can sit above or below it. */
    f32 fogBaseHeight = 1.0f;

    /**
     * Henyey-Greenstein anisotropy, -1 to 1.
     *
     * Positive values throw light forward, which is what makes a low sun blaze
     * through the medium and the same medium look flat once the sun is behind
     * the camera. Zero scatters evenly in every direction.
     */
    f32 fogAnisotropy = 0.62f;

    /** How strongly the sun lights the medium. This is what makes shafts. */
    f32 fogSunScattering = 1.0f;

    /** How strongly the sky lights it, which is what keeps shadows off black. */
    f32 fogAmbientScattering = 0.35f;

    /**
     * Shadow rays taken along the ray to resolve shafts.
     *
     * Zero lights the medium uniformly, which is cheap and flat: the sun's
     * reach is then evaluated only where the ray lands, so a pillar casts no
     * shaft behind it. Every step costs one shadow ray per pixel, so this is
     * the one control in the medium worth counting.
     */
    u32 fogSteps = 0;

    /** How far along the ray those samples are taken, in world units. */
    f32 fogStepDistance = 40.0f;

    /**
     * Unit vector from the scene toward the moon.
     *
     * Written by the day cycle so the miss shader and the night fill agree
     * about where the moon sits. Left at zenith when a scene has no clock.
     */
    Vec3 moonDirection{0.0f, 1.0f, 0.0f};

    /**
     * How brightly the moon lights the sky and the night fill, 0 to 1.
     *
     * Zero hides the disc and leaves night to starlight alone.
     */
    f32 moonIntensity = 0.0f;
};

} // namespace Concord

#endif // CONCORD_ENVIRONMENTSETTINGS_H
