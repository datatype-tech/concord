// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

layout(push_constant) uniform OverlayPushConstants {
    vec4 color;
    vec4 offset;
} overlay;

layout(set = 0, binding = 0) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 fragmentUV;

layout(location = 0) out vec4 outColor;

void main()
{
    float coverage = texture(fontAtlas, fragmentUV).r;
    if (coverage <= 0.0) {
        discard;
    }
    outColor = vec4(overlay.color.rgb, overlay.color.a * coverage);
}
