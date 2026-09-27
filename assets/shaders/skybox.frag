// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

struct FrameCameraData {
    mat4 view;
    mat4 projection;
};

layout(std140, set = 0, binding = 0) uniform FrameDataBlock {
    uvec4 header;
    FrameCameraData camera;
} frame;

layout(set = 1, binding = 0) uniform sampler2D skybox;

layout(location = 0) in vec2 clipXY;
layout(location = 0) out vec4 outColor;

void main()
{
    if (frame.header.x == 0u) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec4 view = inverse(frame.camera.projection) * vec4(clipXY, 1.0, 1.0);
    vec3 viewDirection = view.xyz / view.w;
    vec3 world = normalize((inverse(frame.camera.view) * vec4(viewDirection, 0.0)).xyz);
    // Latitude 0 is the north pole, which is the top row of an equirectangular
    // image. The same mapping is used by the ray-miss sky so play matches the
    // viewport.
    float u = atan(world.z, world.x) * 0.15915494309 + 0.5;
    float v = acos(clamp(world.y, -1.0, 1.0)) * 0.31830988618;
    outColor = vec4(texture(skybox, vec2(u, clamp(v, 0.0, 1.0))).rgb, 1.0);
}
