// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/scene/DayCycle.h"

#include "engine/core/Angle.h"

#include <algorithm>
#include <cmath>

namespace Concord {
namespace {

constexpr f32 kPi = 3.14159265358979323846f;

/** Clamps into range, mapping a non-finite value onto the fallback. */
f32 Safe(f32 value, f32 fallback, f32 low, f32 high) noexcept
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}

/** Wraps an hour into 0..24. */
f32 WrapHour(f32 hour) noexcept
{
    if (!std::isfinite(hour)) {
        return 12.0f;
    }
    const f32 wrapped = std::fmod(hour, 24.0f);
    return wrapped < 0.0f ? wrapped + 24.0f : wrapped;
}

/**
 * How much the sun has risen, from -1 at midnight to 1 at noon.
 *
 * The sun's path is modelled as a great circle tilted by the peak elevation,
 * which is enough to place it and to decide how warm its light is. Modelling
 * the seasons as well would need a date, and nothing in the renderer reads one.
 */
f32 SunHeight(f32 hour, f32 peakElevationDegrees) noexcept
{
    // Midnight is the bottom of the path, noon the top.
    const f32 phase = (hour - 6.0f) / 12.0f * kPi;
    const f32 peak = std::sin(Radians(peakElevationDegrees));
    return std::sin(phase) * peak;
}

/** Warmth of the light at a given sun height, 0 at the horizon to 1 overhead. */
Vec3 SunTint(f32 height) noexcept
{
    const f32 raised = std::clamp(height, 0.0f, 1.0f);
    // A setting sun is red because its light crosses far more atmosphere; the
    // same reason a horizon is warm and a zenith is not.
    const Vec3 low{1.00f, 0.34f, 0.13f};
    const Vec3 high{1.00f, 0.96f, 0.90f};
    const f32 blend = std::pow(raised, 0.42f);
    return {low.x + (high.x - low.x) * blend, low.y + (high.y - low.y) * blend,
            low.z + (high.z - low.z) * blend};
}

} // namespace

DayCycle::DayCycle(const DayCycleSettings& settings) noexcept
{
    m_settings = settings;
    m_settings.secondsPerDay = Safe(settings.secondsPerDay, 600.0f, 1.0f, 864000.0f);
    m_settings.peakSunElevationDegrees =
        Safe(settings.peakSunElevationDegrees, 58.0f, 1.0f, 89.5f);
    m_settings.sunriseAzimuthDegrees = Safe(settings.sunriseAzimuthDegrees, 92.0f, -360.0f, 360.0f);
    m_settings.cloudCoverage = Safe(settings.cloudCoverage, 0.22f, 0.0f, 1.0f);
    m_hour = WrapHour(settings.startHour);
}

void DayCycle::Advance(f32 deltaSeconds) noexcept
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
        return;
    }
    // A very long frame must not teleport the sun; a very short one must not
    // stall the clock.
    const f32 step = std::min(deltaSeconds, 1.0f);
    m_hour = WrapHour(m_hour + step / m_settings.secondsPerDay * 24.0f);
}

void DayCycle::SetHour(f32 hour) noexcept { m_hour = WrapHour(hour); }

bool DayCycle::IsDay() const noexcept { return SunHeight(m_hour, m_settings.peakSunElevationDegrees) > 0.0f; }

SkyState DayCycle::Evaluate() const noexcept
{
    SkyState state{};
    state.hour = m_hour;
    const f32 height = SunHeight(m_hour, m_settings.peakSunElevationDegrees);
    state.elevationDegrees = Degrees(std::asin(std::clamp(height, -1.0f, 1.0f)));

    // The sun rises in the east and sets in the west, so its horizontal
    // position follows the hour while its height follows the path above.
    const f32 horizontal = (m_hour - 6.0f) / 12.0f * kPi;
    const f32 cosHeight = std::sqrt(std::max(1.0f - height * height, 0.0f));
    const f32 azimuth = Radians(m_settings.sunriseAzimuthDegrees) + horizontal;
    state.sunDirection = Vec3{-cosHeight * std::sin(azimuth), height,
                              -cosHeight * std::cos(azimuth)};
    // Reported in the same form a light is authored, so applying the sky to a
    // scene is a copy rather than a conversion.
    state.azimuthDegrees = Degrees(azimuth);

    // Below the horizon the sun lights nothing directly. The fade is smooth so
    // dusk does not switch off between two frames.
    const f32 dayAmount = std::clamp(height / 0.12f, 0.0f, 1.0f);
    state.sunIntensity = dayAmount * dayAmount;
    state.sunColor = SunTint(std::clamp(height, 0.0f, 1.0f));

    // Opposite the sun on the same path: when the sun is down the moon is up,
    // which is what a night sky is actually lit by.
    const f32 moonHeight = SunHeight(WrapHour(m_hour + 12.0f), m_settings.peakSunElevationDegrees);
    const f32 moonHorizontal = (WrapHour(m_hour + 12.0f) - 6.0f) / 12.0f * kPi;
    const f32 moonCosHeight = std::sqrt(std::max(1.0f - moonHeight * moonHeight, 0.0f));
    const f32 moonAzimuth = Radians(m_settings.sunriseAzimuthDegrees) + moonHorizontal;
    state.moonDirection = Vec3{-moonCosHeight * std::sin(moonAzimuth), moonHeight,
                               -moonCosHeight * std::cos(moonAzimuth)};
    const f32 moonAmount = std::clamp(moonHeight / 0.10f, 0.0f, 1.0f);
    state.moonIntensity = moonAmount * moonAmount * (1.0f - dayAmount * 0.92f);

    // The sky is lit by whatever the sun is doing plus a floor of starlight, so
    // night is dark rather than a dim copy of day.
    const f32 twilight = std::clamp((height + 0.22f) / 0.42f, 0.0f, 1.0f);
    const Vec3 dayZenith{0.055f, 0.100f, 0.235f};
    const Vec3 nightZenith{0.0016f, 0.0022f, 0.0065f};
    state.zenith = {nightZenith.x + (dayZenith.x - nightZenith.x) * twilight,
                    nightZenith.y + (dayZenith.y - nightZenith.y) * twilight,
                    nightZenith.z + (dayZenith.z - nightZenith.z) * twilight};

    // A horizon carries the sun's own colour at low angles, which is what makes
    // a sunset orange rather than merely dim.
    const f32 warm = std::clamp(1.0f - std::abs(height) * 3.4f, 0.0f, 1.0f);
    const Vec3 dayHorizon{0.150f + 0.34f * warm, 0.195f + 0.085f * warm,
                          0.365f - 0.150f * warm};
    const Vec3 nightHorizon{0.0040f, 0.0055f, 0.0120f};
    state.horizon = {nightHorizon.x + (dayHorizon.x - nightHorizon.x) * twilight,
                     nightHorizon.y + (dayHorizon.y - nightHorizon.y) * twilight,
                     nightHorizon.z + (dayHorizon.z - nightHorizon.z) * twilight};

    // Sky light is the sky's own colour, scaled by how open it is: cloud is a
    // lid, and the light under it is both dimmer and flatter. Night fill is
    // moonlight, not a leftover daytime ambient that never turns off.
    const f32 openness = 1.0f - 0.55f * m_settings.cloudCoverage;
    state.ambient = {state.zenith.x * 0.55f + state.horizon.x * 0.80f,
                     state.zenith.y * 0.55f + state.horizon.y * 0.80f,
                     state.zenith.z * 0.55f + state.horizon.z * 0.80f};
    const f32 moonFill = 0.07f + 0.28f * state.moonIntensity;
    state.ambientIntensity = (moonFill + 1.05f * twilight) * openness;
    state.dayFactor = twilight;
    return state;
}

} // namespace Concord
