// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

struct FrameCameraData { mat4 view; mat4 projection; };
layout(std140, set = 0, binding = 0) uniform FrameDataBlock {
    uvec4 header;
    FrameCameraData camera;
} frame;

layout(std430, set = 1, binding = 0) readonly buffer SkinningPaletteBlock {
    mat4 joints[];
} palette;

layout(push_constant) uniform SkinningObjectPushConstants {
    mat4 model;
    vec4 albedo;
    vec4 material;
    uvec4 paletteRange;
} object;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec4 vertexTangent;
layout(location = 3) in vec2 vertexTexcoord;
layout(location = 4) in uvec4 vertexJoints;
layout(location = 5) in vec4 vertexWeights;
layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec3 worldPosition;
layout(location = 2) out vec2 texcoord;

const uint kMaxSkinningJoints = 256u;

bool ValidWeight(float value)
{
    return !isnan(value) && !isinf(value) && value > 0.0;
}

mat4 SkinMatrix()
{
    uint count = min(object.paletteRange.y, kMaxSkinningJoints);
    uint first = object.paletteRange.x;
    uint paletteLength = palette.joints.length();
    if (count == 0u || first >= paletteLength) return mat4(1.0);
    count = min(count, paletteLength - first);
    if (count == 0u) return mat4(1.0);
    uvec4 indices = min(vertexJoints, uvec4(count - 1u));
    vec4 weights = vec4(ValidWeight(vertexWeights.x) ? vertexWeights.x : 0.0,
                        ValidWeight(vertexWeights.y) ? vertexWeights.y : 0.0,
                        ValidWeight(vertexWeights.z) ? vertexWeights.z : 0.0,
                        ValidWeight(vertexWeights.w) ? vertexWeights.w : 0.0);
    float sum = dot(weights, vec4(1.0));
    if (sum <= 0.00001) return mat4(1.0);
    weights /= sum;
    mat4 result = mat4(0.0);
    result += weights.x * palette.joints[first + indices.x];
    result += weights.y * palette.joints[first + indices.y];
    result += weights.z * palette.joints[first + indices.z];
    result += weights.w * palette.joints[first + indices.w];
    return result;
}

void main()
{
    mat4 skin = SkinMatrix();
    mat4 objectMatrix = object.model * skin;
    vec4 world = objectMatrix * vec4(vertexPosition, 1.0);
    mat3 linear = mat3(objectMatrix);
    float determinantValue = determinant(linear);
    vec3 normal = abs(determinantValue) > 0.000001
                      ? normalize(transpose(inverse(linear)) * vertexNormal)
                      : vec3(0.0, 1.0, 0.0);
    gl_Position = frame.camera.projection * frame.camera.view * world;
    worldPosition = world.xyz;
    worldNormal = normal;
    texcoord = vertexTexcoord;
}
