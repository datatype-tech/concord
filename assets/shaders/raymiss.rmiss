// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 payload;

/** Supplies a deep-blue atmospheric gradient when a primary ray misses. */
void main()
{
    vec3 direction = normalize(gl_WorldRayDirectionEXT);
    float horizon = clamp(direction.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 color = mix(vec3(0.004, 0.008, 0.025), vec3(0.05, 0.12, 0.28), horizon);
    float sun = pow(max(dot(direction, normalize(vec3(-0.35, 0.78, 0.52))), 0.0), 180.0);
    payload = vec4(color + vec3(1.0, 0.45, 0.12) * sun * 2.5, 1.0);
}
