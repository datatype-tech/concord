// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 450

layout(location = 0) out vec2 clipXY;

void main()
{
    // One triangle large enough to cover the clip square. Interpolating the
    // clip position is the ray the camera would have traced through that pixel.
    vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    clipXY = positions[gl_VertexIndex];
    gl_Position = vec4(clipXY, 0.0, 1.0);
}
