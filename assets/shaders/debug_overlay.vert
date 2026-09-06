// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

layout(push_constant) uniform OverlayPushConstants {
    vec4 color;
    vec4 offset;
} overlay;

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec2 vertexUV;

layout(location = 0) out vec2 fragmentUV;

void main()
{
    gl_Position = vec4(vertexPosition + overlay.offset.xy, 0.0, 1.0);
    fragmentUV = vertexUV;
}
