// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 460
#extension GL_EXT_ray_tracing : require

// Occlusion rays start with payload 0 ("blocked"); reaching this miss means
// nothing stood between the hit point and the light.
layout(location = 1) rayPayloadInEXT vec4 shadowPayload;

void main()
{
    shadowPayload = vec4(1.0);
}
