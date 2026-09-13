// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/**
 * Analytic sky shared by the miss and closest-hit stages.
 *
 * Single-scatter Rayleigh blue plus a Mie forward lobe, both attenuated by
 * the air they crossed: the sun reddens itself as it sets, the horizon
 * brightens because the line of sight crosses more air, and night falls out
 * naturally when the sun leaves because transSun goes to zero. No textures,
 * no marching, no layer: the dome is a closed-form function of direction.
 *
 * The host must declare the frame block (zenithColor / horizonColor) before
 * including this file.
 */

#ifndef CONCORD_SKY_GLSL
#define CONCORD_SKY_GLSL

const float kSkyPi = 3.14159265359;

float SkyPhaseHg(float cosTheta, float anisotropy)
{
    float g = clamp(anisotropy, -0.95, 0.95);
    float g2 = g * g;
    float denominator = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * kSkyPi * pow(max(denominator, 0.0001), 1.5));
}

/**
 * Analytic single-scatter sky shared by the miss stage and the environment
 * term. Rayleigh blue plus a Mie forward lobe, both attenuated by the air
 * they crossed: the sun reddens itself as it sets, the horizon brightens
 * because the line of sight crosses more air, and night falls out naturally
 * when the sun leaves because transSun goes to zero.
 *
 * @param direction     View ray, normalized.
 * @param toSun         Unit vector toward the sun.
 * @param sunIrradiance Sun colour in sky units (SunRadiance * ~20).
 * @param nightZenith   Day-cycle zenith, which is the night floor after sunset.
 * @param nightHorizon  Day-cycle horizon, same.
 */
vec3 AnalyticSky(vec3 direction, vec3 toSun, vec3 sunIrradiance, vec3 nightZenith,
                 vec3 nightHorizon)
{
    float viewElevation = direction.y;
    float sunElevation = toSun.y;
    // 0.89 overhead, ~8.3 at the horizon: the same curve the old two-colour
    // gradient was fitted to, reused here as an optical-depth scale.
    float viewAirmass = 1.0 / (max(viewElevation, 0.0) + 0.12);
    float sunAirmass = 1.0 / (max(sunElevation, 0.0) + 0.12);
    // Sea-level coefficients, per metre. Times an 8 km / 1.2 km scale height
    // they give a zenith optical depth of ~0.05/0.11/0.27: blue survives
    // overhead, only red survives a horizontal path.
    vec3 betaR = vec3(5.8e-6, 13.5e-6, 33.1e-6);
    vec3 betaM = vec3(3.9e-6);
    float heightR = 8000.0;
    float heightM = 1200.0;
    vec3 extinction = betaR * heightR + betaM * heightM;
    vec3 tauView = extinction * viewAirmass;
    vec3 tauSun = extinction * sunAirmass;
    vec3 transView = exp(-tauView);
    vec3 transSun = exp(-tauSun);
    float cosTheta = dot(direction, toSun);
    float phaseR = 3.0 / (16.0 * kSkyPi) * (1.0 + cosTheta * cosTheta);
    float phaseM = SkyPhaseHg(cosTheta, 0.76);
    vec3 scatter = (phaseR * betaR * heightR + phaseM * betaM * heightM) / max(extinction, vec3(1e-4));
    vec3 day = sunIrradiance * transSun * scatter * (1.0 - transView);
    // Below the horizon there is no sky, only ground haze: fade to the night
    // floor instead of mirroring blue under the world.
    float above = smoothstep(-0.08, 0.06, viewElevation);
    float nightAmount = 1.0 - clamp(dot(sunIrradiance, vec3(0.05)), 0.0, 1.0);
    vec3 night = mix(nightHorizon, nightZenith, clamp(viewElevation, 0.0, 1.0));
    vec3 lit = mix(day, night, nightAmount * smoothstep(0.12, -0.12, sunElevation));
    return mix(night * 0.35, lit, above);
}

#endif
