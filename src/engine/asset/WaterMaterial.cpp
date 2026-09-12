// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/WaterMaterial.h"

#include "engine/asset/ModelAsset.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

/** Clamps into range, mapping a non-finite value onto the low bound. */
f32 ClampFinite(f32 value, f32 low, f32 high) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : low;
}

/** Replaces a non-finite component with zero, which every consumer accepts. */
f32 Finite(f32 value) noexcept
{
    return std::isfinite(value) ? value : 0.0f;
}

bool SanitizeSwell(WaterMaterial& material) noexcept
{
    const WaterMaterial before = material;
    material.waveAmplitude = ClampFinite(material.waveAmplitude, 0.0f, 64.0f);
    material.waveScale = ClampFinite(material.waveScale, 0.05f, 64.0f);
    material.waveSpeed = ClampFinite(material.waveSpeed, 0.0f, 32.0f);
    material.choppiness = ClampFinite(material.choppiness, 0.0f, 1.0f);

    const f32 headingX = Finite(material.heading.x);
    const f32 headingZ = Finite(material.heading.y);
    const f32 headingLength = std::sqrt(headingX * headingX + headingZ * headingZ);
    // A heading is a direction, so scaling it to unit length is what the field
    // is for rather than a correction; only a heading with no direction at all
    // counts as authored out of range.
    const bool headingUsable = std::isfinite(headingLength) && headingLength >= 0.0001f &&
                              std::isfinite(before.heading.x) && std::isfinite(before.heading.y);
    material.heading = headingUsable
                           ? Vec2{headingX / headingLength, headingZ / headingLength}
                           : Vec2{1.0f, 0.0f};
    // A fan wider than a half turn would fold the octaves back onto each other.
    material.spread = ClampFinite(material.spread, 0.0f, 3.14159265f);
    material.drift = ClampFinite(material.drift, -1.0f, 1.0f);
    return headingUsable && before.waveAmplitude == material.waveAmplitude &&
           before.waveScale == material.waveScale && before.waveSpeed == material.waveSpeed &&
           before.choppiness == material.choppiness && before.spread == material.spread &&
           before.drift == material.drift;
}

bool SanitizeOptics(WaterMaterial& material) noexcept
{
    const WaterMaterial before = material;
    material.opacity = ClampFinite(material.opacity, 0.0f, 1.0f);
    // Below 1.0 the surface would magnify, which has no physical reading.
    material.ior = ClampFinite(material.ior, 1.0f, 2.5f);
    material.refraction = ClampFinite(material.refraction, 0.0f, 1.0f);
    material.absorption =
        Vec3{ClampFinite(material.absorption.x, 0.0f, 64.0f),
             ClampFinite(material.absorption.y, 0.0f, 64.0f),
             ClampFinite(material.absorption.z, 0.0f, 64.0f)};
    material.absorptionDistance = ClampFinite(material.absorptionDistance, 0.05f, 4096.0f);
    material.scattering = ClampFinite(material.scattering, 0.0f, 4.0f);
    material.foamThreshold = ClampFinite(material.foamThreshold, 0.0f, 1.0f);
    material.foamIntensity = ClampFinite(material.foamIntensity, 0.0f, 1.0f);
    material.fall = ClampFinite(material.fall, 0.0f, 8.0f);
    return before.opacity == material.opacity && before.ior == material.ior &&
           before.refraction == material.refraction &&
           before.absorption.x == material.absorption.x &&
           before.absorption.y == material.absorption.y &&
           before.absorption.z == material.absorption.z &&
           before.absorptionDistance == material.absorptionDistance &&
           before.scattering == material.scattering &&
           before.foamThreshold == material.foamThreshold &&
           before.foamIntensity == material.foamIntensity &&
           before.fall == material.fall;
}

bool SanitizeRipples(WaterMaterial& material) noexcept
{
    bool clean = true;
    for (WaterRipple& ripple : material.ripples) {
        const WaterRipple before = ripple;
        ripple.centre = Vec2{Finite(ripple.centre.x), Finite(ripple.centre.y)};
        ripple.wavelength = ClampFinite(ripple.wavelength, 0.05f, 512.0f);
        ripple.speed = ClampFinite(ripple.speed, -64.0f, 64.0f);
        ripple.strength = ClampFinite(ripple.strength, 0.0f, 64.0f);
        ripple.falloff = ClampFinite(ripple.falloff, 0.0f, 4.0f);
        // Zero would mean "reach nowhere", which no author means by leaving it
        // alone; the smallest useful ring is a few wavelengths across.
        ripple.reach = ClampFinite(ripple.reach, 0.05f, 4096.0f);
        clean = clean && before.centre.x == ripple.centre.x && before.centre.y == ripple.centre.y &&
                before.wavelength == ripple.wavelength && before.speed == ripple.speed &&
                before.strength == ripple.strength && before.falloff == ripple.falloff &&
                before.reach == ripple.reach;
    }
    return clean;
}

} // namespace

WaterMaterial MakeOceanSurface() noexcept
{
    WaterMaterial ocean{};
    // Long and slow: open-water swell runs to tens of metres between crests.
    // The shading fades whatever falls below a pixel on its own, so the
    // authored scale can be honest about the size of the thing.
    ocean.waveAmplitude = 1.55f;
    ocean.waveScale = 6.5f;
    ocean.waveSpeed = 1.25f;
    ocean.choppiness = 0.78f;
    ocean.heading = {0.94f, 0.34f};
    ocean.spread = 1.25f;
    ocean.drift = 0.012f;
    // Read as water rather than as glass: what carries the depth is extinction
    // over the long grazing paths a low camera sees.
    ocean.opacity = 0.88f;
    ocean.ior = 1.340f;
    ocean.refraction = 1.0f;
    // Open water is metres deep everywhere, so it takes the real coefficients
    // rather than a basin's, and it is depth rather than the curve that makes
    // it blue.
    ocean.absorption = {0.25f, 0.06f, 0.015f};
    ocean.absorptionDistance = 1.0f;
    // Open water is deep everywhere the eye can see, so almost all of its
    // colour arrives by scattering rather than by transmission from a bottom.
    ocean.scattering = 0.0035f;
    ocean.foamThreshold = 0.72f;
    ocean.foamIntensity = 0.68f;
    (void)SanitizeWaterMaterial(ocean);
    return ocean;
}

WaterMaterial MakeStillWater() noexcept
{
    // Spelled out rather than returned as a default-constructed structure, so
    // that the preset is readable as a sea state and the structure's defaults
    // stay free to move without silently redefining what "still" means.
    WaterMaterial still{};
    // Forty degrees of slope summed over the whole spectrum would be a rough
    // sea; a basin at rest wants an order of magnitude less than that, which
    // is a normal that leans rather than a surface that rolls.
    still.waveAmplitude = 0.16f;
    still.waveScale = 1.6f;
    still.waveSpeed = 0.35f;
    still.choppiness = 0.28f;
    still.heading = {1.0f, 0.35f};
    still.spread = 1.1f;
    still.drift = 0.010f;
    still.opacity = 0.94f;
    still.ior = 1.333f;
    still.refraction = 1.0f;
    // Pure water's absorption per metre, averaged over what each channel
    // actually covers rather than sampled at its most extreme wavelength. The
    // far end of red's range is three times the average, and taking it makes a
    // two-metre basin absorb like a ten-metre one.
    still.absorption = {0.25f, 0.06f, 0.015f};
    still.absorptionDistance = 1.0f;
    // Real water scatters orders of magnitude less than it absorbs.
    still.scattering = 0.003f;
    still.foamThreshold = 0.80f;
    still.foamIntensity = 0.10f;
    // Every ripple source stays at zero strength. Still water is the absence
    // of a disturbance, not a quiet one, and a preset that ships one lit would
    // make every calm scene in every project carry the same ring.
    (void)SanitizeWaterMaterial(still);
    return still;
}

WaterMaterial MakeRiverWater() noexcept
{
    WaterMaterial river{};
    river.waveAmplitude = 0.42f;
    river.waveScale = 1.15f;
    river.waveSpeed = 1.85f;
    river.choppiness = 0.46f;
    river.heading = {1.0f, 0.08f};
    river.spread = 0.28f;
    river.drift = 0.004f;
    river.opacity = 0.90f;
    river.ior = 1.333f;
    river.refraction = 1.0f;
    river.absorption = {0.32f, 0.08f, 0.035f};
    river.absorptionDistance = 1.4f;
    river.scattering = 0.0045f;
    river.foamThreshold = 0.78f;
    river.foamIntensity = 0.22f;
    (void)SanitizeWaterMaterial(river);
    return river;
}

WaterMaterial MakeWaterfall() noexcept
{
    WaterMaterial fall{};
    fall.waveAmplitude = 1.10f;
    fall.waveScale = 0.40f;
    fall.waveSpeed = 5.4f;
    fall.choppiness = 0.80f;
    fall.heading = {0.0f, 1.0f};
    fall.spread = 0.55f;
    fall.drift = 0.030f;
    fall.opacity = 0.78f;
    fall.ior = 1.333f;
    fall.refraction = 0.12f;
    fall.absorption = {0.18f, 0.05f, 0.02f};
    fall.absorptionDistance = 0.8f;
    fall.scattering = 0.016f;
    fall.foamThreshold = 0.22f;
    fall.foamIntensity = 0.88f;
    fall.fall = 1.85f;
    (void)SanitizeWaterMaterial(fall);
    return fall;
}

bool SanitizeWaterMaterial(WaterMaterial& material) noexcept
{
    const bool swellClean = SanitizeSwell(material);
    const bool opticsClean = SanitizeOptics(material);
    const bool ripplesClean = SanitizeRipples(material);
    return swellClean && opticsClean && ripplesClean;
}

bool ApplyWaterMaterial(ModelAsset& asset, const WaterMaterial& material) noexcept
{
    if (asset.materials.empty()) {
        return false;
    }
    WaterMaterial sanitized = material;
    // The stamp is what the shader reads, so it is clamped on the way in. The
    // caller still learns about its own out-of-range values from the explicit
    // SanitizeWaterMaterial call it is expected to make first.
    (void)SanitizeWaterMaterial(sanitized);
    for (ModelMaterial& entry : asset.materials) {
        entry.water = sanitized;
    }
    return true;
}

} // namespace Concord
