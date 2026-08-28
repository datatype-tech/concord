// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 460
#extension GL_EXT_ray_query : require

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
struct TileHeader { uint offset; uint count; uint overflow; uint reserved; };
layout(std430, set = 0, binding = 1) readonly buffer TileLightList {
    TileHeader tiles[16384];
    uint indices[1048576];
} tile;
layout(set = 2, binding = 0) uniform accelerationStructureEXT scene;

layout(push_constant) uniform MaterialPushConstants {
    mat4 model;
    vec4 albedo;
    vec4 material;
} object;
layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 worldNormal;
layout(location = 1) in vec3 worldPosition;

/** Normalizes a vector while keeping degenerate directions finite. */
vec3 NormalizeOrUp(vec3 value)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001 ? value * inversesqrt(lengthSquared) : vec3(0.0, 1.0, 0.0);
}

bool IsFiniteVector(vec3 value)
{
    return !any(isnan(value)) && !any(isinf(value));
}

/** Reconstructs the world-space camera position from the rigid view matrix. */
vec3 CameraPosition()
{
    mat3 basis = mat3(frame.camera.view);
    return -transpose(basis) * frame.camera.view[3].xyz;
}

/** Returns true when the TLAS contains an opaque blocker along a light ray. */
bool RayOccluded(vec3 origin, vec3 direction, float distanceToLight)
{
    if (!IsFiniteVector(origin) || !IsFiniteVector(direction) || isnan(distanceToLight) ||
        isinf(distanceToLight) || distanceToLight <= 0.003) return false;
    rayQueryEXT query;
    float maxDistance = min(distanceToLight, 100000.0);
    rayQueryInitializeEXT(query, scene, gl_RayFlagsTerminateOnFirstHitEXT |
                          gl_RayFlagsOpaqueEXT, 0xff, origin, 0.002, direction,
                          max(maxDistance, 0.003));
    while (rayQueryProceedEXT(query)) { }
    return rayQueryGetIntersectionTypeEXT(query, true) !=
           gl_RayQueryCommittedIntersectionNoneEXT;
}

/** Evaluates one light and applies hardware ray-query visibility when requested. */
vec3 EvaluateLight(FrameLightData light, uint lightIndex, vec3 baseColor, vec3 normal,
                   vec3 position, vec3 viewDirection, float metallic, float roughness)
{
    if (light.colorIntensity.w <= 0.0) return vec3(0.0);
    float attenuation = 1.0;
    float lightDistance = 10000.0;
    vec3 toLight;
    if (light.positionType.w == 0.0) {
        toLight = NormalizeOrUp(-light.directionRange.xyz);
    } else {
        vec3 offset = light.positionType.xyz - position;
        lightDistance = length(offset);
        float range = max(light.directionRange.w, 0.001);
        if (lightDistance >= range) return vec3(0.0);
        toLight = NormalizeOrUp(offset);
        float falloff = 1.0 - lightDistance / range;
        attenuation = falloff * falloff;
        if (light.positionType.w == 2.0 &&
            dot(NormalizeOrUp(position - light.positionType.xyz),
                NormalizeOrUp(light.directionRange.xyz)) < light.spotShadow.x) {
            return vec3(0.0);
        }
    }
    float shadow = 1.0;
    if (light.spotShadow.y > 0.5) {
        shadow = RayOccluded(position + normal * 0.003, toLight, lightDistance) ? 0.38 : 1.0;
    }
    float normalLight = dot(normal, toLight);
    float diffuse = mix(max(normalLight, 0.0),
                        clamp((normalLight + 0.25) / 1.25, 0.0, 1.0), 0.18);
    vec3 halfVector = NormalizeOrUp(toLight + viewDirection);
    float specularPower = mix(8.0, 128.0, 1.0 - roughness);
    float specular = pow(max(dot(normal, halfVector), 0.0), specularPower);
    vec3 f0 = mix(vec3(0.04), baseColor, metallic);
    float fresnel = pow(1.0 - max(dot(viewDirection, halfVector), 0.0), 5.0);
    vec3 specularColor = mix(f0, vec3(1.0), fresnel);
    return light.colorIntensity.rgb * light.colorIntensity.w * attenuation * shadow *
           (baseColor * (1.0 - metallic) * diffuse + specularColor * specular * 0.35);
}

/** Compresses HDR lighting before it reaches the 8-bit swapchain. */
vec3 ToneMap(vec3 color)
{
    vec3 value = max(color, vec3(0.0));
    return clamp((value * (2.51 * value + 0.03)) /
                     (value * (2.43 * value + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec3 baseColor = object.albedo.rgb;
    float metallic = clamp(object.material.x, 0.0, 1.0);
    float roughness = clamp(object.material.y, 0.04, 1.0);
    vec3 normal = NormalizeOrUp(worldNormal);
    vec3 viewDirection = NormalizeOrUp(CameraPosition() - worldPosition);
    float hemisphere = mix(0.72, 1.0, clamp(normal.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 color = baseColor * frame.ambientColorIntensity.rgb *
                 frame.ambientColorIntensity.w * hemisphere * (1.0 - metallic);
    uint lightCount = min(frame.header.y, 64u);
    bool useTiles = (frame.header.w & 1u) != 0u;
    if (useTiles) {
        uint tileX = uint(gl_FragCoord.x) / 16u;
        uint tileY = uint(gl_FragCoord.y) / 16u;
        if (tileX >= 128u || tileY >= 128u) useTiles = false;
        else {
            TileHeader header = tile.tiles[tileY * 128u + tileX];
            if (header.overflow != 0u) useTiles = false;
            else for (uint j = 0u; j < min(header.count, 64u); ++j) {
                uint index = tile.indices[header.offset + j];
                if (index < lightCount) color += EvaluateLight(frame.lights[index], index,
                    baseColor, normal, worldPosition, viewDirection, metallic, roughness);
            }
        }
    }
    if (!useTiles) for (uint i = 0u; i < lightCount; ++i)
        color += EvaluateLight(frame.lights[i], i, baseColor, normal, worldPosition,
                               viewDirection, metallic, roughness);
    outColor = vec4(ToneMap(color + baseColor * max(object.material.z, 0.0)), object.albedo.a);
}
