// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#version 450

layout(set = 0, binding = 0) uniform sampler2D imageTexture;
layout(location = 0) in vec4 vertexColor;
layout(location = 1) in vec2 textureUv;
layout(location = 0) out vec4 fragmentColor;

void main()
{
    // Scene targets are sRGB images: hardware has already decoded their samples.
    // The font/icon atlas provides white RGB and alpha coverage.
    fragmentColor = vertexColor * texture(imageTexture, textureUv);
}
