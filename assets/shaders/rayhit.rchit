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
struct RtModelVertex { vec4 position; vec4 normal; vec4 texcoord; };
struct RtModelPrimitiveInfo { uvec4 range; vec4 baseColor; vec4 emissive; vec4 surface; };
layout(std430, set = 2, binding = 1) readonly buffer RtModelVertices { RtModelVertex vertices[]; } modelVertices;
layout(std430, set = 2, binding = 2) readonly buffer RtModelIndices { uint indices[]; } modelIndices;
layout(std430, set = 2, binding = 3) readonly buffer RtModelPrimitives { RtModelPrimitiveInfo primitives[]; } modelPrimitives;
layout(location = 0) rayPayloadInEXT vec4 payload;
// Secondary payload for sun occlusion rays (miss record 1 sets it to lit).
layout(location = 1) rayPayloadEXT vec4 shadowPayload;
// Reflection payload (location 2); w seeds the nested hit's depth.
layout(location = 2) rayPayloadEXT vec4 reflectionPayload;
hitAttributeEXT vec2 hitAttributes;

const vec3 faceNormals[12] = vec3[](vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, -1.0), vec3(1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 1.0), vec3(-1.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, -1.0, 0.0));
const vec3 palette[8] = vec3[](vec3(0.035, 0.045, 0.085), vec3(0.018, 0.028, 0.065), vec3(0.92, 0.045, 0.055), vec3(0.045, 0.28, 0.96), vec3(0.98, 0.22, 0.055), vec3(1.0, 0.66, 0.06), vec3(0.04, 0.82, 0.62), vec3(0.72, 0.12, 0.9));
const float roughnesses[8] = float[](0.78, 0.9, 0.2, 0.15, 0.12, 0.08, 0.18, 0.25);
const float metallics[8] = float[](0.05, 0.0, 0.72, 0.84, 0.88, 0.9, 0.58, 0.65);
const uint MODEL_INSTANCE_BIT = 0x00800000u;
const uint MODEL_INSTANCE_MASK = MODEL_INSTANCE_BIT - 1u;
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
bool LoadModelHit(out vec3 position, out vec3 normal, out vec3 baseColor, out float metallic, out float roughness, out float emissive)
{
    uint instance = gl_InstanceCustomIndexEXT;
    if ((instance & MODEL_INSTANCE_BIT) == 0u) return false;
    uint metadataIndex = instance & MODEL_INSTANCE_MASK;
    if (metadataIndex >= 256u || metadataIndex >= modelPrimitives.primitives.length()) {
        return false;
    }
    RtModelPrimitiveInfo info = modelPrimitives.primitives[metadataIndex];
    uint triangle = uint(max(gl_PrimitiveID, 0));
    uint triangleCount = info.range.z / 3u;
    if (triangle >= triangleCount || triangleCount == 0u ||
        triangle > (0xffffffffu - info.range.y) / 3u) {
        return false;
    }
    uint indexBase = info.range.y + triangle * 3u;
    if (indexBase + 2u < indexBase || indexBase + 2u >= modelIndices.indices.length()) {
        return false;
    }
    uint local0 = modelIndices.indices[indexBase];
    uint local1 = modelIndices.indices[indexBase + 1u];
    uint local2 = modelIndices.indices[indexBase + 2u];
    if (local0 > 0xffffffffu - info.range.x || local1 > 0xffffffffu - info.range.x ||
        local2 > 0xffffffffu - info.range.x) {
        return false;
    }
    uint i0 = info.range.x + local0;
    uint i1 = info.range.x + local1;
    uint i2 = info.range.x + local2;
    if (i0 >= modelVertices.vertices.length() || i1 >= modelVertices.vertices.length() ||
        i2 >= modelVertices.vertices.length()) {
        return false;
    }
    RtModelVertex a = modelVertices.vertices[i0];
    RtModelVertex b = modelVertices.vertices[i1];
    RtModelVertex c = modelVertices.vertices[i2];
    vec3 bary = vec3(1.0 - hitAttributes.x - hitAttributes.y, hitAttributes.x, hitAttributes.y);
    vec3 localPosition = a.position.xyz * bary.x + b.position.xyz * bary.y + c.position.xyz * bary.z;
    vec3 localNormal = NormalizeOrUp(a.normal.xyz * bary.x + b.normal.xyz * bary.y + c.normal.xyz * bary.z);
    position = (gl_ObjectToWorldEXT * vec4(localPosition, 1.0)).xyz;
    normal = NormalizeOrUp(transpose(mat3(gl_WorldToObjectEXT)) * localNormal);
    baseColor = max(info.baseColor.rgb, vec3(0.0));
    metallic = clamp(info.surface.x, 0.0, 1.0);
    roughness = clamp(info.surface.y, 0.04, 1.0);
    emissive = max(dot(info.emissive.rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0);
    return true;
}
/** Traces an occlusion ray toward a directional light; 1.0 when unobstructed. */
float SunVisibility(vec3 position, vec3 normal, vec3 toLight)
{
    shadowPayload = vec4(0.0);
    vec3 origin = position + normal * 0.02 + toLight * 0.02;
    uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT |
                 gl_RayFlagsOpaqueEXT;
    traceRayEXT(scene, flags, 0xFFu, 0u, 0u, 1u, origin, 0.001, toLight, 600.0, 1);
    return shadowPayload.x;
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
    vec3 rayDirection = NormalizeOrUp(gl_WorldRayDirectionEXT);
    vec3 viewDirection = -rayDirection;
    vec3 position;
    vec3 normal;
    vec3 baseColor;
    float roughness;
    float metallic;
    float emissive;
    const bool modelInstance = (gl_InstanceCustomIndexEXT & MODEL_INSTANCE_BIT) != 0u;
    if (!LoadModelHit(position, normal, baseColor, metallic, roughness, emissive) &&
        modelInstance) {
        baseColor = vec3(1.0, 0.0, 1.0);
        roughness = 1.0;
        metallic = 0.0;
        emissive = 0.0;
        normal = NormalizeOrUp(transpose(mat3(gl_WorldToObjectEXT)) * vec3(0.0, 1.0, 0.0));
        position = gl_WorldRayOriginEXT + rayDirection * gl_HitTEXT;
    } else if (!modelInstance) {
        uint materialIndex = gl_InstanceCustomIndexEXT % 8u;
        baseColor = palette[materialIndex];
        roughness = roughnesses[materialIndex];
        metallic = metallics[materialIndex];
        uint primitive = min(uint(max(gl_PrimitiveID, 0)), 11u);
        normal = NormalizeOrUp(transpose(mat3(gl_WorldToObjectEXT)) * faceNormals[primitive]);
        position = gl_WorldRayOriginEXT + rayDirection * gl_HitTEXT;
        emissive = 0.0;
    }
    vec3 color = baseColor * (vec3(0.065) + frame.ambientColorIntensity.rgb *
                 frame.ambientColorIntensity.w * (0.7 + 0.3 * max(normal.y, 0.0)));
    uint lightCount = min(frame.header.y, 64u);
    for (uint index = 0u; index < lightCount; ++index) {
        vec3 contribution = ShadeLight(frame.lights[index], baseColor, normal, position,
                                       viewDirection, metallic, roughness);
        if (frame.lights[index].positionType.w == 0.0) {
            contribution *= SunVisibility(
                position, normal, NormalizeOrUp(-frame.lights[index].directionRange.xyz));
        }
        color += contribution;
    }
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.0);
    color += mix(vec3(0.04, 0.08, 0.18), baseColor, 0.45) *
             rim * (0.18 + metallic * 0.3);
    color += baseColor * emissive;

    // One mirror bounce for smooth metallic model surfaces (the demo's
    // reflective sphere). The nested payload's w seeds the next depth so
    // recursion terminates after a single bounce.
    float depth = payload.w;
    float reflectivity = metallic * (1.0 - roughness);
    if (modelInstance && depth < 1.0 && reflectivity > 0.2) {
        reflectionPayload = vec4(0.0, 0.0, 0.0, depth + 1.0);
        vec3 reflected = NormalizeOrUp(reflect(rayDirection, normal));
        vec3 reflOrigin = position + normal * 0.02 + reflected * 0.02;
        traceRayEXT(scene, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, reflOrigin,
                    0.001, reflected, 10000.0, 2);
        color = mix(color, reflectionPayload.rgb * mix(vec3(1.0), baseColor, 0.15),
                    clamp(reflectivity, 0.0, 1.0));
    }
    payload = vec4(ToneMap(color), depth + 1.0);
}
