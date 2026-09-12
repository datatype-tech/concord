// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/ecs/DayCycleSystem.h"

#include "engine/ecs/Components.h"
#include "engine/scene/Scene.h"

#include <cmath>

namespace Concord {
namespace {

/** Radiance the sun reaches at noon, in the units a scene's exposure assumes. */
constexpr f32 kNoonSunIntensity = 6.5f;

/** Encodes a linear channel for the sRGB colour fields the environment uses. */
f32 EncodeSrgb(f32 linear) noexcept
{
    const f32 value = std::isfinite(linear) && linear > 0.0f ? linear : 0.0f;
    const f32 encoded = value <= 0.0031308f ? value * 12.92f
                                            : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    return std::clamp(encoded, 0.0f, 1.0f);
}

u8 EncodeByte(f32 linear) noexcept
{
    return static_cast<u8>(EncodeSrgb(linear) * 255.0f + 0.5f);
}

ColorRGBA EncodeColor(Vec3 linear) noexcept
{
    return MakeColor(EncodeByte(linear.x), EncodeByte(linear.y), EncodeByte(linear.z));
}

} // namespace

DayCycleSystem::DayCycleSystem(const DayCycleSettings& settings) noexcept : m_cycle(settings)
{
    m_sky = m_cycle.Evaluate();
}

void DayCycleSystem::SetHour(f32 hour) noexcept
{
    m_cycle.SetHour(hour);
    m_sky = m_cycle.Evaluate();
}

void DayCycleSystem::OnUpdate(Scene& scene, f32 deltaTime)
{
    m_cycle.Advance(deltaTime);
    m_sky = m_cycle.Evaluate();

    // Every directional light follows the clock. A scene that authored one sun
    // gets a day cycle; one that authored none is left alone rather than having
    // a light conjured into it.
    scene.Query<LightComponent>([this](Entity, LightComponent& light) {
        if (light.type != LightType::Directional) {
            return;
        }
        light.elevationDegrees = m_sky.elevationDegrees;
        light.azimuthDegrees = m_sky.azimuthDegrees;
        light.color = EncodeColor(m_sky.sunColor);
        // The cycle works in 0..1; a scene's lights work in whatever units
        // its exposure was chosen for, so the mapping is stated once here.
        light.intensity = m_sky.sunIntensity * kNoonSunIntensity;
    });

    EnvironmentSettings environment = scene.Environment();
    if (m_baseExposure < 0.0f) {
        m_baseExposure = environment.exposure > 0.05f ? environment.exposure : 1.0f;
    }
    environment.zenithColor = m_sky.zenith;
    environment.horizonColor = m_sky.horizon;
    environment.skyColor = EncodeColor(m_sky.horizon);
    environment.ambientColor = EncodeColor(m_sky.ambient);
    environment.ambientIntensity = m_sky.ambientIntensity;
    environment.cloudCoverage = m_cycle.Settings().cloudCoverage;
    environment.moonDirection = m_sky.moonDirection;
    environment.moonIntensity = m_sky.moonIntensity;
    environment.sunIntensity = m_sky.sunIntensity;
    // Night is a stop down, not a filter. Leaving the day exposure in place
    // is why pressing Night used to leave the courtyard looking like dusk.
    environment.exposure = m_baseExposure * (0.22f + 0.78f * m_sky.dayFactor);
    environment.fogAmbientScattering = 0.06f + 0.24f * m_sky.dayFactor;
    environment.fogSunScattering = 0.80f * m_sky.dayFactor;
    scene.SetEnvironment(environment);
}

} // namespace Concord
