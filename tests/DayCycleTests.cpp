// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/scene/DayCycle.h"

#include <cmath>

namespace {

bool Near(float left, float right, float tolerance = 0.05f)
{
    return std::fabs(left - right) < tolerance;
}

bool TestNoonHasSunAndNoMoon()
{
    Concord::DayCycle cycle({.startHour = 12.0f});
    const Concord::SkyState sky = cycle.Evaluate();
    return cycle.IsDay() && sky.sunIntensity > 0.8f && sky.dayFactor > 0.8f &&
           sky.moonIntensity < 0.15f &&
           sky.sunDirection.y > 0.4f;
}

bool TestMidnightIsDarkWithMoon()
{
    Concord::DayCycle cycle({.startHour = 0.0f});
    const Concord::SkyState sky = cycle.Evaluate();
    return !cycle.IsDay() && sky.sunIntensity == 0.0f && sky.dayFactor < 0.15f &&
           sky.moonIntensity > 0.4f && sky.moonDirection.y > 0.2f &&
           sky.ambientIntensity < 0.45f;
}

bool TestClockWraps()
{
    Concord::DayCycle cycle({.secondsPerDay = 24.0f, .startHour = 23.5f});
    cycle.Advance(1.0f);
    return Near(cycle.Hour(), 0.5f, 0.05f);
}

} // namespace

int main()
{
    return TestNoonHasSunAndNoMoon() && TestMidnightIsDarkWithMoon() && TestClockWraps() ? 0
                                                                                         : 1;
}
