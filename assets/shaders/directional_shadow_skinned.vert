// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

layout(std430, set = 0, binding = 0) readonly buffer SkinningPaletteBlock {
    mat4 joints[];
} palette;

layout(push_constant) uniform ShadowSkinningPushConstants {
    mat4 model;
    uvec4 paletteRange;
} shadow;

layout(location = 0) in vec3 vertexPosition;
layout(location = 4) in uvec4 vertexJoints;
layout(location = 5) in vec4 vertexWeights;

const uint kMaxSkinningJoints = 256u;

bool ValidWeight(float value)
{
    return !isnan(value) && !isinf(value) && value > 0.0;
}

mat4 SkinMatrix()
{
    uint count = min(shadow.paletteRange.y, kMaxSkinningJoints);
    uint first = shadow.paletteRange.x;
    uint length = palette.joints.length();
    if (count == 0u || first >= length) return mat4(1.0);
    count = min(count, length - first);
    uvec4 indices = min(vertexJoints, uvec4(count - 1u));
    vec4 weights = vec4(ValidWeight(vertexWeights.x) ? vertexWeights.x : 0.0,
                        ValidWeight(vertexWeights.y) ? vertexWeights.y : 0.0,
                        ValidWeight(vertexWeights.z) ? vertexWeights.z : 0.0,
                        ValidWeight(vertexWeights.w) ? vertexWeights.w : 0.0);
    float sum = dot(weights, vec4(1.0));
    if (sum <= 0.00001) return mat4(1.0);
    weights /= sum;
    return weights.x * palette.joints[first + indices.x] +
           weights.y * palette.joints[first + indices.y] +
           weights.z * palette.joints[first + indices.z] +
           weights.w * palette.joints[first + indices.w];
}

void main()
{
    gl_Position = shadow.model * SkinMatrix() * vec4(vertexPosition, 1.0);
}
