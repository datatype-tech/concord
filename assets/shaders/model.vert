// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

struct FrameCameraData { mat4 view; mat4 projection; };
layout(std140, set = 0, binding = 0) uniform FrameDataBlock {
    uvec4 header;
    FrameCameraData camera;
} frame;

layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
    vec4 albedo;
    vec4 material;
} object;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTexcoord;
layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec3 worldPosition;
layout(location = 2) out vec2 texcoord;

void main()
{
    vec4 world = object.model * vec4(vertexPosition, 1.0);
    mat3 linear = mat3(object.model);
    float determinantValue = determinant(linear);
    worldNormal = abs(determinantValue) > 0.000001
                      ? normalize(transpose(inverse(linear)) * vertexNormal)
                      : vec3(0.0, 1.0, 0.0);
    worldPosition = world.xyz;
    texcoord = vertexTexcoord;
    gl_Position = frame.camera.projection * frame.camera.view * world;
}
