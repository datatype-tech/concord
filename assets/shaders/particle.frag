// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

layout(location = 0) in vec2 texcoord;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 outColor;

/**
 * Shades one particle as a soft round sprite.
 *
 * The falloff is analytic rather than sampled so particles need no texture,
 * and the output is premultiplied because the particle pass blends with
 * (ONE, ONE): emission adds light instead of occluding it, which also makes
 * the draw order irrelevant and removes the need to sort particles.
 */
void main()
{
    vec2 centered = texcoord * 2.0 - 1.0;
    float radiusSquared = dot(centered, centered);
    if (radiusSquared >= 1.0) {
        discard;
    }
    float falloff = 1.0 - radiusSquared;
    falloff *= falloff;
    float alpha = clamp(color.a, 0.0, 1.0) * falloff;
    outColor = vec4(max(color.rgb, vec3(0.0)) * alpha, alpha);
}
