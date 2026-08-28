// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

struct FrameCameraData {
    mat4 view;
    mat4 projection;
};

struct FrameLightData {
    vec4 positionType;
    vec4 directionRange;
    vec4 colorIntensity;
    vec4 spotShadow;
};

layout(std140, set = 0, binding = 0) uniform FrameDataBlock {
    uvec4 header;
    FrameCameraData camera;
    vec4 ambientColorIntensity;
    FrameLightData lights[64];
} frame;

layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
    vec4 albedo;
    vec4 material;
} object;

layout(location = 0) out vec3 worldNormal;
layout(location = 1) out vec3 worldPosition;

const vec3 vertices[36] = vec3[](
    vec3(-0.5, -0.5,  0.5), vec3( 0.5, -0.5,  0.5), vec3( 0.5,  0.5,  0.5),
    vec3(-0.5, -0.5,  0.5), vec3( 0.5,  0.5,  0.5), vec3(-0.5,  0.5,  0.5),
    vec3( 0.5, -0.5, -0.5), vec3(-0.5, -0.5, -0.5), vec3(-0.5,  0.5, -0.5),
    vec3( 0.5, -0.5, -0.5), vec3(-0.5,  0.5, -0.5), vec3( 0.5,  0.5, -0.5),
    vec3(-0.5, -0.5, -0.5), vec3(-0.5, -0.5,  0.5), vec3(-0.5,  0.5,  0.5),
    vec3(-0.5, -0.5, -0.5), vec3(-0.5,  0.5,  0.5), vec3(-0.5,  0.5, -0.5),
    vec3( 0.5, -0.5,  0.5), vec3( 0.5, -0.5, -0.5), vec3( 0.5,  0.5, -0.5),
    vec3( 0.5, -0.5,  0.5), vec3( 0.5,  0.5, -0.5), vec3( 0.5,  0.5,  0.5),
    vec3(-0.5,  0.5,  0.5), vec3( 0.5,  0.5,  0.5), vec3( 0.5,  0.5, -0.5),
    vec3(-0.5,  0.5,  0.5), vec3( 0.5,  0.5, -0.5), vec3(-0.5,  0.5, -0.5),
    vec3(-0.5, -0.5, -0.5), vec3( 0.5, -0.5, -0.5), vec3( 0.5, -0.5,  0.5),
    vec3(-0.5, -0.5, -0.5), vec3( 0.5, -0.5,  0.5), vec3(-0.5, -0.5,  0.5));

const vec3 faceNormals[6] = vec3[](
    vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, -1.0), vec3(-1.0, 0.0, 0.0),
    vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, -1.0, 0.0));

void main()
{
    vec3 localPosition = vertices[gl_VertexIndex];
    vec4 world = object.model * vec4(localPosition, 1.0);
    gl_Position = frame.camera.projection * frame.camera.view * world;
    worldPosition = world.xyz;
    mat3 linear = mat3(object.model);
    vec3 localNormal = faceNormals[gl_VertexIndex / 6];
    float linearDeterminant = determinant(linear);
    worldNormal = abs(linearDeterminant) > 0.000001
                      ? normalize(transpose(inverse(linear)) * localNormal)
                      : localNormal;
}
