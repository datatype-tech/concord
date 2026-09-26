// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#version 450

layout(constant_id = 0) const bool linearizeColor = true;
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;
layout(push_constant) uniform Projection { vec2 scale; vec2 translate; } projection;
layout(location = 0) out vec4 vertexColor;
layout(location = 1) out vec2 textureUv;

vec3 ToLinear(vec3 value)
{
    return mix(pow((value + 0.055) / 1.055, vec3(2.4)), value / 12.92, lessThanEqual(value, vec3(0.04045)));
}

void main()
{
    // Conversion stays floating point so dark UI colors retain their precision.
    vertexColor = vec4(linearizeColor ? ToLinear(color.rgb) : color.rgb, color.a);
    textureUv = uv;
    gl_Position = vec4(position * projection.scale + projection.translate, 0.0, 1.0);
}
