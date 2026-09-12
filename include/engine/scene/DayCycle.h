// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CONCORD_DAYCYCLE_H
#define CONCORD_DAYCYCLE_H

#include "Concord/CExport.h"
#include "engine/core/Types.h"
#include "engine/core/Vec3.h"

namespace Concord {

/** How a day is shaped: how long it takes, and the sky it runs under. */
struct DayCycleSettings {
    /**
     * Real seconds one full day takes.
     *
     * The single control that makes a cycle watchable: a minute per day for a
     * demo, several hours for a game.
     */
    f32 secondsPerDay = 600.0f;

    /** Hour of day the clock starts at, 0 to 24. */
    f32 startHour = 8.5f;

    /**
     * Highest the sun climbs above the horizon, in degrees.
     *
     * Stands in for latitude: it decides whether noon is overhead or the sun
     * only ever grazes the sky, and therefore whether a scene has a summer or
     * a winter light.
     */
    f32 peakSunElevationDegrees = 58.0f;

    /** Compass direction the sun rises from, in degrees clockwise from north. */
    f32 sunriseAzimuthDegrees = 92.0f;

    /**
     * Cloud coverage, 0 for clear and 1 for overcast.
     *
     * Also what a sunset is made of: the colour of a dusk is not the sky's, it
     * is the light the clouds catch after the sun has left the ground.
     */
    f32 cloudCoverage = 0.22f;
};

/**
 * Sun, sky and cloud state at one instant.
 *
 * Everything needed to draw a time of day, produced by one model so the sun
 * cannot disagree with the sky it sits in or with the light it casts.
 */
struct SkyState {
    /** Unit vector from the scene toward the sun. */
    Vec3 sunDirection{0.0f, 1.0f, 0.0f};
    /** Radiance of the sun, faded out as it sets. */
    Vec3 sunColor{1.0f, 0.95f, 0.86f};
    /** Multiplier on sunColor; zero below the horizon. */
    f32 sunIntensity = 1.0f;
    /** Sky colour straight overhead. */
    Vec3 zenith{0.02f, 0.05f, 0.12f};
    /** Sky colour at the horizon. */
    Vec3 horizon{0.30f, 0.16f, 0.09f};
    /** Light reaching a surface from the whole sky. */
    Vec3 ambient{0.10f, 0.12f, 0.18f};
    /** Multiplier on ambient. */
    f32 ambientIntensity = 1.0f;
    /** Unit vector from the scene toward the moon. */
    Vec3 moonDirection{0.0f, 1.0f, 0.0f};
    /** How brightly the moon lights the night, 0 when it is down. */
    f32 moonIntensity = 0.0f;
    /** Height above the horizon in degrees; negative once the sun has set. */
    f32 elevationDegrees = 45.0f;
    /** Compass bearing of the sun in degrees, in the form a light is authored. */
    f32 azimuthDegrees = 180.0f;
    /** 0 at midnight, 12 at noon. */
    f32 hour = 12.0f;
    /**
     * How much of the day is still in the sky, 0 at night and 1 in full day.
     *
     * Wider than `sunIntensity`: dusk still counts as day for exposure and
     * fog, so a night that only looked at the disc never actually went dark.
     */
    f32 dayFactor = 1.0f;
};

/**
 * A clock that drives a sky.
 *
 * Holds no scene and touches nothing: advancing it produces a SkyState, and
 * applying that to a scene is a separate, deliberate step. That split is what
 * lets a whole cycle be tested without a renderer, and lets a game read the
 * sky without changing it.
 */
class CENGINE_API DayCycle {
public:
    explicit DayCycle(const DayCycleSettings& settings = {}) noexcept;

    /** Advances the clock by a real-time delta. */
    void Advance(f32 deltaSeconds) noexcept;

    /** Jumps to an hour, which is how a game sets a specific time of day. */
    void SetHour(f32 hour) noexcept;

    /** Clock position in hours, 0 to 24. */
    [[nodiscard]] f32 Hour() const noexcept { return m_hour; }

    /** Whether the sun is above the horizon. */
    [[nodiscard]] bool IsDay() const noexcept;

    /** The sun, sky and cloud state right now. */
    [[nodiscard]] SkyState Evaluate() const noexcept;

    /** The settings this cycle was built with. */
    [[nodiscard]] const DayCycleSettings& Settings() const noexcept { return m_settings; }

private:
    DayCycleSettings m_settings{};
    f32 m_hour = 12.0f;
};

} // namespace Concord

#endif // CONCORD_DAYCYCLE_H
