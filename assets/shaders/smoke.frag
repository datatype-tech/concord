// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

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
} frame;

layout(location = 0) in vec2 texcoord;
layout(location = 1) in vec4 color;
layout(location = 2) in vec3 worldPosition;

layout(location = 0) out vec4 outColor;

const float kPi = 3.14159265359;

/** Direction from the scene toward the frame's directional light. */
vec3 TowardSun()
{
    uint lightCount = min(frame.header.y, 64u);
    for (uint index = 0u; index < lightCount; ++index) {
        if (frame.lights[index].positionType.w == 0.0 &&
            frame.lights[index].colorIntensity.w > 0.0) {
            return normalize(-frame.lights[index].directionRange.xyz);
        }
    }
    return normalize(vec3(-0.35, 0.78, 0.52));
}

vec3 SunRadiance()
{
    uint lightCount = min(frame.header.y, 64u);
    for (uint index = 0u; index < lightCount; ++index) {
        if (frame.lights[index].positionType.w == 0.0 &&
            frame.lights[index].colorIntensity.w > 0.0) {
            return frame.lights[index].colorIntensity.rgb *
                   frame.lights[index].colorIntensity.w;
        }
    }
    return vec3(0.0);
}

/**
 * Henyey-Greenstein phase function for the smoke's own scattering.
 *
 * Smoke is a cloud of particles, not a surface: what makes it read as smoke
 * rather than as grey paint is that it flares when the sun is behind it and
 * goes dull when the sun is behind the camera. An evenly lit billboard cannot
 * do that at any opacity, which is why this is evaluated rather than faked
 * with a brightness constant.
 */
float PhaseHG(float cosTheta, float anisotropy)
{
    float g = clamp(anisotropy, -0.95, 0.95);
    float g2 = g * g;
    float denominator = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * kPi * pow(max(denominator, 0.0001), 1.5));
}

/** Stable hash of a cell, for the erosion noise below. */
float SmokeHash(vec2 cell)
{
    vec3 hash = fract(vec3(cell.x, cell.y, cell.x) * vec3(0.1031, 0.1030, 0.0973));
    hash += dot(hash, hash.yzx + 33.33);
    return fract((hash.x + hash.y) * hash.z);
}

/** Smooth value noise: the wisps, not the puff. */
float SmokeNoise(vec2 point)
{
    vec2 base = floor(point);
    vec2 fraction = point - base;
    vec2 blend = fraction * fraction * (3.0 - 2.0 * fraction);
    float lowA = SmokeHash(base);
    float lowB = SmokeHash(base + vec2(1.0, 0.0));
    float highA = SmokeHash(base + vec2(0.0, 1.0));
    float highB = SmokeHash(base + vec2(1.0, 1.0));
    return mix(mix(lowA, lowB, blend.x), mix(highA, highB, blend.x), blend.y);
}

/** Three octaves of detail riding on one slow shape. */
float SmokeFbm(vec2 point)
{
    float total = SmokeNoise(point) * 0.55;
    total += SmokeNoise(point * 2.13 + vec2(7.3, 3.1)) * 0.28;
    total += SmokeNoise(point * 4.31 + vec2(13.7, 9.2)) * 0.17;
    return total;
}

/**
 * Shades one particle as a soft round sprite that scatters the frame's light.
 *
 * The camera-facing quad gives the scattering angle directly: a billboard's
 * normal is the view direction, so the angle between where the light comes
 * from and where the eye is looking is all the phase function needs.
 *
 * The output is premultiplied, exactly as the additive path's is, so one vertex
 * stage serves both and the two differ only in what the blend state does with
 * the same four numbers.
 */
void main()
{
    vec2 centered = texcoord * 2.0 - 1.0;
    float radiusSquared = dot(centered, centered);
    if (radiusSquared >= 1.0) {
        discard;
    }
    // Squared falloff rather than a linear ramp: a linear one leaves a visible
    // rim where the quad ends, which on overlapping puffs reads as facets.
    float falloff = 1.0 - radiusSquared;
    falloff *= falloff;
    // Erosion breaks the perfect disc: every puff is otherwise the same smooth
    // round sprite, and identical sprites are what reads as billboard soup.
    // Sampled in world space rather than billboard UV so neighbouring puffs
    // read as one continuous turbulent field, and so the pattern drifts
    // through a rising puff instead of being stamped onto it.
    vec2 erosionPoint = worldPosition.xz * 1.6 + vec2(worldPosition.y * 0.50,
                                                      -worldPosition.y * 0.35);
    float grain = SmokeFbm(erosionPoint);
    float breakup = smoothstep(0.25, 0.80, falloff * 0.65 + grain * 0.55);
    float alpha = clamp(color.a, 0.0, 1.0) * falloff * breakup;

    // The view matrix is rigid, so its rotation is its transpose and the eye
    // sits at the negated translation rotated back into world space.
    mat3 basis = mat3(frame.cameraView);
    vec3 eye = -transpose(basis) * frame.cameraView[3].xyz;
    vec3 viewDirection = normalize(eye - worldPosition);

    // Forward scattering: light travels from the sun, through the puff, to the
    // eye, so the angle that matters is between the sun and the view.
    vec3 toSun = normalize(TowardSun());
    float phase = PhaseHG(dot(toSun, viewDirection), 0.55);
    vec3 ambient = frame.ambientColorIntensity.rgb * frame.ambientColorIntensity.w;
    vec3 lit = ambient + SunRadiance() * phase * 4.0;
    // Denser knots shade themselves: a flat-lit puff is paint, and the grain
    // already says where the puff is thick.
    lit *= 0.72f + 0.56f * grain;

    vec3 albedo = max(color.rgb, vec3(0.0));
    outColor = vec4(albedo * lit * alpha, alpha);
}
