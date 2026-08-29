// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

layout(push_constant) uniform ShadowPushConstants {
    mat4 lightViewProjection;
    mat4 model;
} shadow;

layout(location = 0) in vec3 vertexPosition;

void main()
{
    gl_Position = shadow.lightViewProjection * shadow.model *
                  vec4(vertexPosition, 1.0);
}
