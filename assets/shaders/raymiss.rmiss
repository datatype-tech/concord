// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

struct FrameLightData {
    vec4 positionType;
    vec4 directionRange;
    vec4 colorIntensity;
    vec4 spotShadow;
};
layout(std140, set = 0, binding = 0) uniform FrameDataBlock {
    uvec4 header;
    mat4 cameraView;
    mat4 cameraProjection;
    vec4 ambientColorIntensity;
    FrameLightData lights[64];
    mat4 shadowViewProjection;
    vec4 frameTime;
    vec4 grade;
    vec4 postFx;
    vec4 surfaceInfo;
    vec4 fog;
    vec4 fogLight;
    vec4 zenithColor;
    vec4 horizonColor;
    vec4 cloud;
    vec4 cloudDetail;
    vec4 cloudShape;
    vec4 cloudMarch;
    vec4 celestial;
} frame;

/** Radiance the sun reaches at noon, mirroring the day cycle's own mapping. */
const float kNoonSunIntensity = 6.5;

layout(location = 0) rayPayloadInEXT vec4 payload;

/**
 * Set by the closest-hit shader on a ray that is looking through water.
 *
 * A ray seeded with this offset is answered with the distance it covered in
 * the payload's w instead of a colour alpha. Sky counts as an infinite path,
 * so water with nothing under it absorbs the whole way and falls away to the
 * authored body colour rather than turning into a window onto the sky.
 */
const float kTransmissionFlag = 8.0;
const float kFarDistance = 10000.0;

/**
 * Direction from the scene toward the sun.
 *
 * Read from the frame's directional light rather than hardcoded: a sky whose
 * sun sits somewhere else than the light casting the shadows reads as two
 * different suns. Falls back to a default only when the frame has no
 * directional light at all.
 */
vec3 TowardSun()
{
    uint lightCount = min(frame.header.y, 64u);
    vec3 found = vec3(-0.35, 0.78, 0.52);
    for (uint index = 0u; index < lightCount; ++index) {
        if (frame.lights[index].positionType.w != 0.0) {
            continue;
        }
        found = -frame.lights[index].directionRange.xyz;
        if (frame.lights[index].colorIntensity.w > 0.0) {
            break;
        }
    }
    return normalize(found);
}

const float kPi = 3.14159265359;

/**
 * How far the medium is integrated for a ray that never lands.
 *
 * Not infinity: an exponential height fog closes to a finite optical depth, and
 * a ray that climbs out of it is clear long before any horizon distance. What
 * the bound buys is that sky near the horizon sits behind far more air than sky
 * overhead, which is the whole reason a horizon reads as distant.
 */
const float kSkyFogDistance = 3000.0;

float PhaseHG(float cosTheta, float anisotropy)
{
    float g = clamp(anisotropy, -0.95, 0.95);
    float g2 = g * g;
    float denominator = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * kPi * pow(max(denominator, 0.0001), 1.5));
}

float FogOpticalDepth(vec3 origin, vec3 direction, float distance, vec4 fog)
{
    float falloff = max(fog.y, 0.0001);
    float rise = direction.y * distance;
    float height = exp(-falloff * (origin.y - fog.z));
    float span = abs(rise) > 0.001 ? (1.0 - exp(-falloff * rise)) / (falloff * rise) : 1.0;
    return max(fog.x, 0.0) * height * span * distance;
}

/**
 * Radiance of the frame's sun, normalised so a cloud is lit like a surface.
 *
 * The light block carries a scene's own exposure units, which can be anything.
 * Dividing by the noon value puts cloud brightness on the same 0..1 scale the
 * sky colours use, so the two cannot drift apart when a scene retunes its sun.
 */
vec3 SunRadiance()
{
    uint lightCount = min(frame.header.y, 64u);
    for (uint index = 0u; index < lightCount; ++index) {
        if (frame.lights[index].positionType.w == 0.0 &&
            frame.lights[index].colorIntensity.w > 0.0) {
            return frame.lights[index].colorIntensity.rgb *
                   (frame.lights[index].colorIntensity.w / kNoonSunIntensity);
        }
    }
    return vec3(0.0);
}

float Hash12(vec2 cell)
{
    vec3 hash = fract(vec3(cell.x, cell.y, cell.x) * vec3(0.1031, 0.1030, 0.0973));
    hash += dot(hash, hash.yzx + 33.33);
    return fract((hash.x + hash.y) * hash.z);
}

/**
 * Hashes a 3D cell, used to place stars directly on the view-direction grid.
 *
 * Stars used to be placed with `floor(direction.xz * scale + direction.y *
 * scale2)`: adding the elevation term to *both* the x and z cell coordinates
 * makes them shift together as elevation changes, so any pixel that is one
 * star's cell in x and z at one elevation is still that same cell -- shifted
 * diagonally -- at a nearby elevation. A point smears into the streak along
 * that diagonal as the camera looks up or down past it. Quantizing the full
 * 3D direction instead never projects the sphere onto a plane, so there is no
 * shared term to correlate two axes and no pole to compress cells at either.
 */
float Hash13(vec3 cell)
{
    vec3 hash = fract(cell * vec3(0.1031, 0.1030, 0.0973));
    hash += dot(hash, hash.yzx + 33.33);
    return fract((hash.x + hash.y) * hash.z);
}

#include "sky.glsl"
#include "cloud.glsl"

/** Supplies the atmospheric dome and the sun for rays that escape the scene. */
void main()
{
    vec3 direction = normalize(gl_WorldRayDirectionEXT);
    vec3 toSun = TowardSun();
    float sunHeight = clamp(toSun.y, 0.0, 1.0);

    vec3 zenith = max(frame.zenithColor.rgb, vec3(0.0));
    vec3 horizon = max(frame.horizonColor.rgb, vec3(0.0));

    vec3 sunRadiance = SunRadiance();
    // Single-scatter Rayleigh + Mie: the sun reddens itself through transSun,
    // the horizon brightens through a longer view path, night falls out when
    // the sun leaves. Scale puts noon on the 0..1 range the clouds use so the
    // two cannot drift apart when a scene retunes its sun.
    vec3 color = AnalyticSky(direction, toSun, sunRadiance, zenith, horizon);
    // Kept before the sun disc is drawn into it. This is what lights the parts
    // of a cloud the sun cannot reach, and a fill that carried the disc would
    // put a second sun inside every cloud the first one happens to sit behind.
    vec3 skyFill = color;

    float elevation = clamp(direction.y, 0.0, 1.0);
    float alignment = max(dot(direction, toSun), 0.0);
    float disc = pow(alignment, mix(280.0, 1600.0, sunHeight));
    float glow = pow(alignment, mix(3.5, 14.0, sunHeight)) * mix(0.30, 0.08, sunHeight);
    float haze = pow(alignment, 2.2) * 0.10 * (1.0 - elevation);
    color += sunRadiance * (disc * 8.0 + glow + haze);

    float night = 1.0 - smoothstep(0.02, 0.14, max(sunHeight, 0.0));
    night *= 1.0 - clamp(dot(sunRadiance, vec3(0.3333)), 0.0, 1.0);
    if (night > 0.001) {
        vec3 toMoon = length(frame.celestial.xyz) > 0.001
                          ? normalize(frame.celestial.xyz)
                          : vec3(0.0, 1.0, 0.0);
        float moonAlign = max(dot(direction, toMoon), 0.0);
        float moonDisc = pow(moonAlign, 2800.0);
        float moonGlow = pow(moonAlign, 48.0) * 0.18;
        color += vec3(0.78, 0.84, 0.96) * (moonDisc * 4.2 + moonGlow) *
                 frame.celestial.w * night;
        float cell = Hash13(floor(direction * 340.0));
        float star = step(0.9965, cell) * step(0.08, direction.y);
        color += vec3(0.82, 0.88, 1.0) * star * night * 1.15;
    }

    // Composited over everything the dome produced and under the medium. The
    // layer sits between the eye and the sun, the moon and the stars alike, so
    // it has to be able to hide all three; the air, on the other hand, sits in
    // front of the layer as much as in front of the sky behind it.
    //
    // The first step is offset per pixel because a march of a few dozen steps
    // through a soft volume lays down concentric rings wherever the step
    // boundaries line up across neighbouring pixels. Breaking that alignment
    // does not remove the error, it redistributes it into grain, which the eye
    // reads as cloud texture rather than as a rendering artifact.
    float jitter = Hash12(vec2(gl_LaunchIDEXT.xy) + fract(frame.cloudDetail.z) * 977.0);
    // A camera ray pays for the layer in full; anything spawned by a surface
    // pays a third. The payload's w is the recursion depth on an ordinary ray
    // and a distance on a transmission ray, and both mean the same thing here:
    // this is not the ray the viewer is looking along.
    float quality = payload.w >= 1.0 ? 0.34 : 1.0;
    vec4 clouds =
        CloudLayer(gl_WorldRayOriginEXT, direction, toSun, sunRadiance, skyFill, jitter, quality);
    color = color * clouds.a + clouds.rgb;

    if (frame.fog.x > 0.0) {
        vec3 unitSun = normalize(toSun);
        float opticalDepth =
            FogOpticalDepth(gl_WorldRayOriginEXT, direction, kSkyFogDistance, frame.fog);
        float transmittance = exp(-opticalDepth);
        float phase = PhaseHG(dot(direction, unitSun), frame.fog.w);
        vec3 scattered = SunRadiance() * phase * frame.fogLight.x +
                         frame.ambientColorIntensity.rgb * frame.ambientColorIntensity.w *
                             frame.fogLight.y;
        color = color * transmittance + scattered * (1.0 - transmittance);
    }

    payload = payload.w >= kTransmissionFlag ? vec4(color, kFarDistance) : vec4(color, 1.0);
}
