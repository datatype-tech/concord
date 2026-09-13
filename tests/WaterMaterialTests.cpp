// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/asset/WaterMaterial.h"

#include "engine/asset/ModelAsset.h"

#include <cmath>

namespace {

using Concord::f32;

bool Nearly(f32 left, f32 right)
{
    return std::abs(left - right) < 0.0001f;
}

/** The shipped defaults have to survive their own validator untouched. */
bool TestDefaultsAreAccepted()
{
    Concord::WaterMaterial surface{};
    return Concord::SanitizeWaterMaterial(surface);
}

bool TestOutOfRangeValuesAreClampedAndReported()
{
    Concord::WaterMaterial surface{};
    surface.waveAmplitude = -1.0f;
    surface.choppiness = 9.0f;
    surface.spread = 12.0f;
    surface.drift = 40.0f;
    surface.opacity = 4.0f;
    surface.ior = 0.2f;
    surface.refraction = -3.0f;
    surface.absorption = {-1.0f, 0.0f, 100.0f};
    surface.absorptionDistance = 0.0f;
    surface.foamThreshold = 99.0f;
    surface.foamIntensity = -2.0f;
    surface.ripples[0].wavelength = 0.0f;
    surface.ripples[1].strength = -5.0f;
    surface.ripples[2].falloff = 90.0f;

    if (Concord::SanitizeWaterMaterial(surface)) {
        return false;
    }
    if (!Nearly(surface.waveAmplitude, 0.0f) || !Nearly(surface.choppiness, 1.0f) ||
        !Nearly(surface.spread, 3.14159265f) || !Nearly(surface.drift, 1.0f) ||
        !Nearly(surface.opacity, 1.0f) || !Nearly(surface.ior, 1.0f) ||
        !Nearly(surface.refraction, 0.0f) || !Nearly(surface.absorption.x, 0.0f) ||
        !Nearly(surface.absorption.z, 64.0f) || !Nearly(surface.absorptionDistance, 0.05f) ||
        !Nearly(surface.foamThreshold, 1.0f) || !Nearly(surface.foamIntensity, 0.0f)) {
        return false;
    }
    return Nearly(surface.ripples[0].wavelength, 0.05f) &&
           Nearly(surface.ripples[1].strength, 0.0f) &&
           Nearly(surface.ripples[2].falloff, 4.0f);
}

/**
 * A heading is a direction, so scaling it to unit length is what the field is
 * for. Reporting that as a correction would make every hand-authored heading
 * look like a mistake.
 */
bool TestHeadingIsNormalizedWithoutBeingReported()
{
    Concord::WaterMaterial surface{};
    surface.heading = {3.0f, 4.0f};
    if (!Concord::SanitizeWaterMaterial(surface)) {
        return false;
    }
    return Nearly(surface.heading.x, 0.6f) && Nearly(surface.heading.y, 0.8f);
}

/** A heading with no direction at all has to become something usable. */
bool TestDegenerateHeadingFallsBack()
{
    Concord::WaterMaterial surface{};
    surface.heading = {0.0f, 0.0f};
    if (Concord::SanitizeWaterMaterial(surface)) {
        return false;
    }
    return Nearly(surface.heading.x, 1.0f) && Nearly(surface.heading.y, 0.0f);
}

/**
 * A non-finite component would reach the shader as a NaN normal and blank the
 * whole surface. Replacing it with zero would leave a heading that happens to
 * be unit length but was never authored, so the whole direction falls back.
 */
bool TestNonFiniteValuesAreReportedAndReplaced()
{
    const f32 notANumber = std::nanf("");
    Concord::WaterMaterial surface{};
    surface.heading = {notANumber, 1.0f};
    surface.opacity = notANumber;
    surface.ripples[0].centre = {notANumber, notANumber};
    if (Concord::SanitizeWaterMaterial(surface)) {
        return false;
    }
    return Nearly(surface.heading.x, 1.0f) && Nearly(surface.heading.y, 0.0f) &&
           Nearly(surface.opacity, 0.0f) && Nearly(surface.ripples[0].centre.x, 0.0f) &&
           Nearly(surface.ripples[0].centre.y, 0.0f);
}

bool TestApplyStampsEveryMaterial()
{
    Concord::ModelAsset asset{};
    asset.materials.push_back(Concord::ModelMaterial{.name = "deck"});
    asset.materials.push_back(Concord::ModelMaterial{.name = "hull"});
    Concord::WaterMaterial surface{};
    surface.opacity = 0.4f;
    surface.ripples[1].strength = 0.7f;
    if (!Concord::ApplyWaterMaterial(asset, surface)) {
        return false;
    }
    return asset.materials[0].water.has_value() && asset.materials[1].water.has_value() &&
           Nearly(asset.materials[0].water->opacity, 0.4f) &&
           Nearly(asset.materials[1].water->ripples[1].strength, 0.7f) &&
           Nearly(asset.materials[1].water->ripples[0].strength, 0.0f);
}

/** An asset with no material would render dry, which has to be visible. */
bool TestApplyReportsAnAssetWithNoMaterials()
{
    Concord::ModelAsset asset{};
    return !Concord::ApplyWaterMaterial(asset, Concord::WaterMaterial{});
}

/** What is stamped is what the shader reads, so it is sanitized on the way in. */
bool TestApplySanitizesWhatItStamps()
{
    Concord::ModelAsset asset{};
    asset.materials.push_back(Concord::ModelMaterial{});
    Concord::WaterMaterial surface{};
    surface.opacity = 12.0f;
    if (!Concord::ApplyWaterMaterial(asset, surface)) {
        return false;
    }
    return asset.materials[0].water.has_value() &&
           Nearly(asset.materials[0].water->opacity, 1.0f);
}

/**
 * The open-water preset has to be a different sea state rather than a renamed
 * default, and it has to survive the same validator every authored surface
 * goes through.
 */
bool TestOceanPreset()
{
    Concord::WaterMaterial ocean = Concord::MakeOceanSurface();
    if (!Concord::SanitizeWaterMaterial(ocean)) {
        return false;
    }
    const Concord::WaterMaterial calm{};
    return ocean.waveScale > calm.waveScale * 3.0f &&
           ocean.waveAmplitude > calm.waveAmplitude &&
           ocean.choppiness > calm.choppiness &&
           ocean.absorptionDistance < calm.absorptionDistance &&
           !ocean.ripples[0].strength &&
           ocean.ripples[0].wavelength > 0.0f;
}

bool TestRiverPreset()
{
    Concord::WaterMaterial river = Concord::MakeRiverWater();
    Concord::WaterMaterial ocean = Concord::MakeOceanSurface();
    if (!Concord::SanitizeWaterMaterial(river)) {
        return false;
    }
    return river.waveSpeed > ocean.waveSpeed && river.spread < ocean.spread &&
           river.waveScale < ocean.waveScale && !river.ripples[0].strength;
}

bool TestWaterfallPreset()
{
    Concord::WaterMaterial fall = Concord::MakeWaterfall();
    Concord::WaterMaterial river = Concord::MakeRiverWater();
    if (!Concord::SanitizeWaterMaterial(fall)) {
        return false;
    }
    return fall.waveSpeed > river.waveSpeed && fall.foamIntensity > river.foamIntensity &&
           fall.waveScale < river.waveScale && fall.fall > 1.0f;
}

/** Presence is the mask: a material with no surface stays dry. */
bool TestDefaultMaterialIsDry()
{
    const Concord::ModelMaterial material{};
    return !material.water.has_value();
}

} // namespace

int main()
{
    return TestDefaultsAreAccepted() && TestOutOfRangeValuesAreClampedAndReported() &&
                   TestHeadingIsNormalizedWithoutBeingReported() &&
                   TestDegenerateHeadingFallsBack() &&
                   TestNonFiniteValuesAreReportedAndReplaced() &&
                   TestOceanPreset() && TestRiverPreset() && TestWaterfallPreset() &&
                   TestApplyStampsEveryMaterial() &&
                   TestApplyReportsAnAssetWithNoMaterials() &&
                   TestApplySanitizesWhatItStamps() && TestDefaultMaterialIsDry()
               ? 0
               : 1;
}
