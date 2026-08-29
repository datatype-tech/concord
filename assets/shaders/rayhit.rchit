// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 460
#extension GL_EXT_ray_tracing : require

struct FrameCameraData { mat4 view; mat4 projection; };
struct FrameLightData {
    vec4 positionType;
    vec4 directionRange;
    vec4 colorIntensity;
    vec4 spotShadow;
};
layout(std140, set = 0, binding = 0) uniform FrameDataBlock {
    uvec4 header;
    FrameCameraData camera;
    vec4 ambientColorIntensity;
    FrameLightData lights[64];
} frame;
layout(set = 2, binding = 0) uniform accelerationStructureEXT scene;
layout(location = 0) rayPayloadInEXT vec4 payload;
hitAttributeEXT vec2 hitAttributes;

const vec3 faceNormals[12] = vec3[](
    vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 1.0),
    vec3(-1.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0),
    vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, -1.0),
    vec3(1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0),
    vec3(0.0, -1.0, 0.0), vec3(0.0, -1.0, 0.0),
    vec3(0.0, 1.0, 0.0), vec3(0.0, 1.0, 0.0));
const vec3 palette[8] = vec3[](
    vec3(0.035, 0.045, 0.085), vec3(0.018, 0.028, 0.065),
    vec3(0.92, 0.045, 0.055), vec3(0.045, 0.28, 0.96),
    vec3(0.98, 0.22, 0.055), vec3(1.0, 0.66, 0.06),
    vec3(0.04, 0.82, 0.62), vec3(0.72, 0.12, 0.9));
const float roughnesses[8] = float[](0.78, 0.9, 0.2, 0.15, 0.12, 0.08, 0.18, 0.25);
const float metallics[8] = float[](0.05, 0.0, 0.72, 0.84, 0.88, 0.9, 0.58, 0.65);

vec3 NormalizeOrUp(vec3 value)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001 ? value * inversesqrt(lengthSquared) : vec3(0.0, 1.0, 0.0);
}

vec3 ToneMap(vec3 value)
{
    value = max(value, vec3(0.0)) * 0.78;
    return clamp((value * (2.51 * value + 0.03)) /
                 (value * (2.43 * value + 0.59) + 0.14), 0.0, 1.0);
}

vec3 ShadeLight(FrameLightData light, vec3 baseColor, vec3 normal,
                vec3 position, vec3 viewDirection, float metallic, float roughness)
{
    if (light.colorIntensity.w <= 0.0) return vec3(0.0);
    vec3 toLight;
    float attenuation = 1.0;
    float intensity = light.colorIntensity.w;
    if (light.positionType.w == 0.0) {
        toLight = NormalizeOrUp(-light.directionRange.xyz);
    } else {
        vec3 offset = light.positionType.xyz - position;
        float distanceToLight = length(offset);
        float range = max(light.directionRange.w, 0.001);
        if (distanceToLight >= range) return vec3(0.0);
        toLight = NormalizeOrUp(offset);
        float falloff = 1.0 - distanceToLight / range;
        attenuation = falloff * falloff / (1.0 + distanceToLight * distanceToLight * 0.055);
        intensity *= 0.22;
        if (light.positionType.w == 2.0 &&
            dot(NormalizeOrUp(position - light.positionType.xyz),
                NormalizeOrUp(light.directionRange.xyz)) < light.spotShadow.x) {
            return vec3(0.0);
        }
    }
    float diffuse = max(dot(normal, toLight), 0.0);
    vec3 halfVector = NormalizeOrUp(toLight + viewDirection);
    float power = mix(10.0, 140.0, 1.0 - roughness);
    float specular = pow(max(dot(normal, halfVector), 0.0), power);
    vec3 f0 = mix(vec3(0.04), baseColor, metallic);
    float fresnel = pow(1.0 - max(dot(viewDirection, halfVector), 0.0), 5.0);
    vec3 specularColor = mix(f0, baseColor + vec3(0.08), fresnel * 0.28);
    vec3 diffuseColor = baseColor * mix(0.36, 1.0, 1.0 - metallic) * diffuse;
    return light.colorIntensity.rgb * intensity * attenuation *
           (diffuseColor + specularColor * specular * 0.22);
}

void main()
{
    uint materialIndex = gl_InstanceCustomIndexEXT % 8u;
    vec3 baseColor = palette[materialIndex];
    float roughness = roughnesses[materialIndex];
    float metallic = metallics[materialIndex];
    uint primitive = min(uint(max(gl_PrimitiveID, 0)), 11u);
    vec3 normal = NormalizeOrUp(transpose(mat3(gl_WorldToObjectEXT)) * faceNormals[primitive]);
    vec3 rayDirection = NormalizeOrUp(gl_WorldRayDirectionEXT);
    vec3 viewDirection = -rayDirection;
    vec3 position = gl_WorldRayOriginEXT + rayDirection * gl_HitTEXT;
    vec3 color = baseColor * (vec3(0.065) + frame.ambientColorIntensity.rgb *
                 frame.ambientColorIntensity.w * (0.7 + 0.3 * max(normal.y, 0.0)));
    uint lightCount = min(frame.header.y, 64u);
    for (uint index = 0u; index < lightCount; ++index) {
        color += ShadeLight(frame.lights[index], baseColor, normal, position,
                            viewDirection, metallic, roughness);
    }
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.0);
    color += mix(vec3(0.04, 0.08, 0.18), baseColor, 0.45) *
             rim * (0.18 + metallic * 0.3);
    payload = vec4(ToneMap(color), 1.0);
}
