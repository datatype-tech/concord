// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

struct FrameCameraData { mat4 view; mat4 projection; };
layout(std140, set = 0, binding = 0) uniform FrameDataBlock {
    uvec4 header;
    FrameCameraData camera;
} frame;

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexcoord;
layout(location = 2) in vec4 vertexColor;

layout(location = 0) out vec2 texcoord;
layout(location = 1) out vec4 color;
/** World position, which the smoke shading needs to face the camera from. */
layout(location = 2) out vec3 worldPosition;

/** Projects a pre-built camera-facing billboard corner. */
void main()
{
    texcoord = vertexTexcoord;
    color = vertexColor;
    worldPosition = vertexPosition;
    gl_Position = frame.camera.projection * frame.camera.view * vec4(vertexPosition, 1.0);
}
