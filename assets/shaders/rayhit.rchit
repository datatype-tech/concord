// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

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
    mat4 shadowViewProjection;
    // x is seconds since start, driving every animated water surface below.
    vec4 frameTime;
    // Declared only to reach surfaceInfo past them; this stage reads neither.
    vec4 grade;
    vec4 postFx;
    // x live disturbance count, y 1 when the camera is below a water surface.
    vec4 surfaceInfo;
    // x density, y height falloff, z base height, w anisotropy.
    vec4 fog;
    // x sun scattering, y ambient scattering, z march steps, w march distance.
    vec4 fogLight;
    // rgb linear sky overhead and at the horizon: what a wide reflection lobe
    // averages to, evaluated by EnvironmentRadiance below. Declared in the
    // host's own order, which std140 requires and nothing here re-checks.
    vec4 zenithColor;
    vec4 horizonColor;
    // Cloud uniforms are currently unread: the sky is a clear analytic dome.
    // Trailing fields stay in host order so celestial cannot slide.
    vec4 cloud;
    vec4 cloudDetail;
    vec4 cloudShape;
    vec4 cloudMarch;
    vec4 celestial;
    // Water surfaces for the "wet near water" look on dry materials. Each is
    // x/z centre, y world-space surface height, w radius; a zero radius is an
    // empty slot. Declared only here, past every offset a plain
    // forward-shading shader relies on, for the same append-only reason the
    // fields above it were.
    vec4 wetnessBodies[8];
} frame;

#include "sky.glsl"

layout(set = 2, binding = 0) uniform accelerationStructureEXT scene;
struct RtModelVertex { vec4 position; vec4 normal; vec4 texcoord; };
/**
 * Mirror of VulkanWaterMaterial's std430 layout.
 *
 * Every member is a vec4, so an array of them has a sixteen-byte stride and no
 * padding rule can differ between this declaration and the host's. The two
 * arrays are shape and motion rather than an interleaved pair so each keeps
 * that uniform stride.
 */
struct RtWaterMaterial {
    vec4 optics;
    vec4 absorbance;
    vec4 wave;
    vec4 flow;
    vec4 surface;
    vec4 rippleShape[4];
    vec4 rippleMotion[4];
};
struct RtModelPrimitiveInfo {
    uvec4 range;
    vec4 baseColor;
    vec4 emissive;
    vec4 surface;
    RtWaterMaterial water;
};
layout(std430, set = 2, binding = 1) readonly buffer RtModelVertices { RtModelVertex vertices[]; } modelVertices;
layout(std430, set = 2, binding = 2) readonly buffer RtModelIndices { uint indices[]; } modelIndices;
layout(std430, set = 2, binding = 3) readonly buffer RtModelPrimitives { RtModelPrimitiveInfo primitives[]; } modelPrimitives;
/** Authored material of every Box instance, mirroring VulkanBoxMaterial. */
struct RtBoxMaterial { vec4 albedo; vec4 surface; };
layout(std430, set = 2, binding = 4) readonly buffer RtBoxMaterials { RtBoxMaterial materials[]; } boxMaterials;
/** Live water disturbances, mirroring VulkanRippleSource. */
struct RtRippleSource { vec4 shape; vec4 motion; };
layout(std430, set = 2, binding = 5) readonly buffer RtRippleSources { RtRippleSource sources[]; } rippleSources;
// Material base-colour textures, picked per hit by the slot the host wrote
// into the primitive's range.w. The array is indexed dynamically, which is
// why the device is created with shaderSampledImageArrayDynamicIndexing.
layout(set = 3, binding = 0) uniform sampler2D modelTextures[64];
layout(location = 0) rayPayloadInEXT vec4 payload;
// Secondary payload for sun occlusion rays (miss record 1 sets it to lit).
layout(location = 1) rayPayloadEXT vec4 shadowPayload;
// The reflection ray reuses location 0: a payload slot is addressed by the
// location the trace names, so a nested hit writing location 0 was never
// visible to a parent reading location 2. Sharing one slot makes the nested
// result readable and lets its w carry the recursion depth back up.
hitAttributeEXT vec2 hitAttributes;

const vec3 faceNormals[12] = vec3[](vec3(0.0, 0.0, -1.0), vec3(0.0, 0.0, -1.0), vec3(1.0, 0.0, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(0.0, 0.0, 1.0), vec3(-1.0, 0.0, 0.0), vec3(-1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, -1.0, 0.0), vec3(0.0, -1.0, 0.0));
const vec3 palette[8] = vec3[](vec3(0.035, 0.045, 0.085), vec3(0.018, 0.028, 0.065), vec3(0.92, 0.045, 0.055), vec3(0.045, 0.28, 0.96), vec3(0.98, 0.22, 0.055), vec3(1.0, 0.66, 0.06), vec3(0.04, 0.82, 0.62), vec3(0.72, 0.12, 0.9));
const float roughnesses[8] = float[](0.78, 0.9, 0.2, 0.15, 0.12, 0.08, 0.18, 0.25);
const float metallics[8] = float[](0.05, 0.0, 0.72, 0.84, 0.88, 0.9, 0.58, 0.65);
const uint MODEL_INSTANCE_BIT = 0x00800000u;
const uint MODEL_INSTANCE_MASK = MODEL_INSTANCE_BIT - 1u;
// A Box instance's custom index names a slot in the box material array. The
// bit stays clear on the stand-in instance the acceleration structure is built
// from when a scene has no objects, which keeps shading it from the palette.
const uint BOX_MATERIAL_BIT = 0x00400000u;
const uint BOX_MATERIAL_MASK = BOX_MATERIAL_BIT - 1u;
// Mirrors kVulkanRayTracingMaskLightBlocker: the instance mask bit a shadow
// ray tests against, so geometry that casts no shadow does not occlude.
const uint LIGHT_BLOCKER_MASK = 0x01u;
vec3 NormalizeOrUp(vec3 value)
{
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.000001 ? value * inversesqrt(lengthSquared) : vec3(0.0, 1.0, 0.0);
}
const float kTwoPi = 6.28318530718;

/**
 * Added to the payload's depth component to mark a ray looking through water.
 *
 * A hit or miss produced by such a ray reports how far it travelled in the
 * payload's w instead of a recursion depth, which is what turns the authored
 * extinction into absorption over the real path length rather than over a
 * constant. The offset sits far above any reachable recursion depth, so a
 * normal ray can never be misread as a transmission ray.
 */
const float kTransmissionFlag = 8.0;

/** Water block with every ripple source disabled, used by dry surfaces. */
RtWaterMaterial DryWater()
{
    vec4 none = vec4(0.0);
    return RtWaterMaterial(none, none, none, vec4(1.0, 0.0, 0.0, 0.0), none,
                           vec4[4](none, none, none, none), vec4[4](none, none, none, none));
}

/**
 * Slope of the authored swell at a world position.
 *
 * Octaves are summed analytically, so the surface needs no simulation buffer
 * and no per-frame vertex work: the plane stays where it was authored and only
 * its shading moves. The wavelength ratios are deliberately untidy (17 : 11.3 :
 * 7.1 : 4.3 : 2.7 : 1.7 rather than 2:1 or 3:1), because a tidy six-wave sum
 * repeats within seconds and reads as a looping texture instead of as water;
 * `flow.w` swings the whole field on top of that, so even a long shot settles
 * into nothing.
 *
 * @param crest Receives the summed crest height, which is what places foam.
 */
void SwellSlope(vec3 position, float time, RtWaterMaterial water, float footprint,
                out float slopeX, out float slopeZ, out float crest,
                out float unresolvedSlope)
{
    // Eight octaves. Six carries a swell but no sea: the smallest detail is
    // then metres across, the normal is smooth, and the sun's reflection
    // collapses to a point instead of the broad glitter path a real surface
    // throws back. Two more than that buys breadth without buying chop -- the
    // two that used to sit below them were the shortest and the steepest, they
    // contributed the least width to the reflection, and they were the most
    // expensive thing in the loop.
    const float baseWavelengths[8] = float[](17.0, 11.3, 7.1, 4.3, 2.7, 1.7, 1.05, 0.63);
    const float baseAmplitudes[8] = float[](0.42, 0.30, 0.20, 0.13, 0.080, 0.048, 0.030, 0.018);
    const float baseSpeeds[8] = float[](0.90, 1.05, 1.22, 1.41, 1.63, 1.88, 2.15, 2.46);
    // Untidy spacing as well, so the fan itself has no period either.
    const float baseAngles[8] = float[](-1.50, -0.85, -0.25, 0.35, 0.95, 1.55, -1.18, 0.62);
    // Each octave swings at its own rate. Sharing one heading makes the
    // relative geometry of the ten waves permanently fixed, and a fixed set of
    // relative angles sums to a standing interference lattice that merely
    // slides -- which is what a repeating pattern on water actually is. Giving
    // them separate rates shears the octaves against each other, and the
    // lattice can never reassemble. The values are deliberately unrelated.
    const float baseDrift[8] =
        float[](0.0113, -0.0071, 0.0163, -0.0137, 0.0049, 0.0191, -0.0167, 0.0083);

    // Directions live in the horizontal plane, so they are 2D and dot against
    // position.xz directly.
    vec2 heading = water.flow.xy;
    heading = dot(heading, heading) > 0.0001 ? normalize(heading) : vec2(1.0, 0.0);
    float headingAngle = atan(heading.y, heading.x) + time * water.flow.w;
    float wavelengthScale = max(water.wave.y, 0.05);
    float steepest = clamp(water.wave.w, 0.0, 1.0);

    slopeX = 0.0;
    slopeZ = 0.0;
    float crestSum = 0.0;
    float steepnessSum = 0.0;
    float unresolved = 0.0;
    for (int index = 0; index < 8; ++index) {
        float angle = headingAngle + water.flow.z * baseAngles[index] +
                      time * baseDrift[index] * (1.0 + water.flow.w * 8.0);
        vec2 direction = vec2(cos(angle), sin(angle));
        float wavelength = baseWavelengths[index] * wavelengthScale;
        float waveNumber = kTwoPi / wavelength;
        float fullSteepness = baseAmplitudes[index] * water.wave.x * waveNumber;
        float phase = waveNumber * dot(direction, position.xz) -
                      baseSpeeds[index] * water.wave.z * waveNumber * time;
        // Detail smaller than the pixel sampling it does not shade, it
        // scintillates: an ocean covered in sub-pixel chop reads as television
        // static however correct the wave sum is. Fading each octave out once
        // its wavelength approaches the footprint is what an image pyramid does
        // for a texture, done analytically here because the waves are analytic.
        float resolved = 1.0 - smoothstep(0.8, 2.5, footprint / max(wavelength, 0.0001));
        if (resolved <= 0.0) {
            unresolved += fullSteepness;
            continue;
        }
        float steepness = fullSteepness * resolved;
        unresolved += fullSteepness * (1.0 - resolved);
        // Gerstner steepness pinches the horizontal term, sharpening a crest
        // the way a real one sharpens before it breaks. A plain sine stays a
        // round swell however far the amplitude is pushed, which is why
        // choppiness is a control of its own rather than more amplitude.
        float pinch = max(1.0 - steepest * steepness * sin(phase), 0.15);
        slopeX += steepness * cos(phase) * direction.x / pinch;
        slopeZ += steepness * cos(phase) * direction.y / pinch;
        crestSum += steepness * sin(phase);
        steepnessSum += steepness;
    }
    // How much of the surface's slope cannot be resolved at this distance, as
    // a fraction of all of it. Unresolved detail does not leave the image: it
    // becomes a wider specular lobe. Reporting it is what lets a
    // mirror-smooth surface stay a mirror up close and stop throwing isolated
    // blown-out glints wherever an under-sampled normal happens to line up.
    unresolvedSlope = steepnessSum + unresolved > 0.00001
                          ? unresolved / (steepnessSum + unresolved)
                          : 0.0;
    // Normalized to roughly -1..1. The raw sum scales with both amplitude and
    // wavelength, so an authored foam threshold would have to be re-tuned
    // every time either changed; the slopes above stay in physical units
    // because they are what the reflection actually depends on.
    crest = steepnessSum > 0.00001 ? crestSum / steepnessSum : 0.0;
}

/** One 32-bit avalanche. Cheap, and well enough mixed to key a silhouette. */
uint MixBits(uint value)
{
    value ^= value >> 16u;
    value *= 0x7FEB352Du;
    value ^= value >> 15u;
    value *= 0x846CA68Bu;
    value ^= value >> 16u;
    return value;
}

/** A mixed word as a 0..1 float. Twenty-four bits is all a float holds. */
float UnitFloat(uint value)
{
    return float(value >> 8u) * (1.0 / 16777216.0);
}

/**
 * The silhouette of one disturbance, keyed by where it sits.
 *
 * Not one random number. A ring that differs from the last only by a rotation
 * is still the same ring, and a surface covered in the same ring at different
 * angles is precisely the repetition a body of water must not have. What has
 * to differ between one disturbance and the next is the shape: how many lobes
 * the wavefront has, how deep they run, how far the whole thing is pulled out
 * of round and along which axis, and how wide its crest is drawn.
 *
 * All of it comes from one hashed key rather than from four sine hashes,
 * because this runs per source per water pixel and the old field cost six
 * transcendentals for a shape that was only ever rotated. The key uses the
 * centre's own bits, so two drops a millimetre apart are unrelated, and it
 * takes the cycle number so that a repeating source is a different ring every
 * time it comes back.
 */
struct RippleShape {
    /** Phase the lobes start at. */
    float phase;
    /** The two lobe counts. Coprime by construction, so they never align. */
    vec2 lobes;
    /** How deep each of the two runs, as a fraction of the displacement. */
    vec2 depth;
    /** How far out of round the wavefront is pulled, and along which axis. */
    float stretch;
    float axis;
    /** Crest width as a multiple of the authored one. */
    float width;
};

RippleShape Fingerprint(vec2 centre, float cycle)
{
    uint key = floatBitsToUint(centre.x) ^ (floatBitsToUint(centre.y) * 0x9E3779B9u) ^
               (floatBitsToUint(cycle) * 0x85EBCA6Bu);
    float a = UnitFloat(MixBits(key));
    float b = UnitFloat(MixBits(key ^ 0x68E31DA4u));
    float c = UnitFloat(MixBits(key ^ 0xB5297A4Du));
    float d = UnitFloat(MixBits(key ^ 0x1B56C4E9u));
    RippleShape shape;
    shape.phase = a * kTwoPi;
    // Two to five lobes, and six to thirteen. Neither count divides the other,
    // so a ring can never settle into a shape with a period of its own.
    shape.lobes = vec2(2.0 + floor(b * 4.0), 6.0 + floor(c * 7.0));
    shape.depth = vec2(0.26 + c * 0.26, 0.09 + d * 0.15);
    shape.stretch = 0.05 + d * 0.17;
    shape.axis = c * kTwoPi;
    shape.width = 0.70 + b * 0.70;
    return shape;
}

/**
 * A disturbance whose wavefront is not a circle.
 *
 * The analytic ring was the last thing on the surface still reading as
 * generated: a perfect circle of perfect sinusoid, every time, from every
 * drop. Nothing in water behaves that way. Real rings are irregular from the
 * moment they form, and they break up further as they spread, because the
 * crests that travel fastest outrun the ones beside them.
 *
 * The silhouette comes from Fingerprint(), which is keyed by the drop rather
 * than by the frame -- the same drop always leaves the same shape, and no two
 * drops leave the same one.
 */
void AddRippleSlope(vec3 position, float time, vec4 shape, vec4 motion, float footprint,
                    inout float slopeX, inout float slopeZ, inout float crest)
{
    float strength = shape.w;
    if (strength <= 0.0) {
        return;
    }
    float wavelength = max(shape.z, 0.05);
    if (footprint > wavelength * 2.0) {
        return;
    }
    // Reach is a hard limit, not a decay.
    //
    // A decay on its own never quite removes a disturbance: a few metres out a
    // ring is faint, but faint is not absent, and a body of water ends up
    // criss-crossed by every ring ever dropped on it. One radius the source
    // genuinely cannot cross is what keeps a disturbance local -- and it is
    // what lets every body of water in a scene share one list of them, since a
    // splash on one pond then cannot reach another.
    float reach = max(motion.z, 0.05);
    float speed = max(motion.x, 0.02);
    float age = motion.w;
    float cycle = 0.0;
    if (age < 0.0) {
        // An authored source has no age of its own; it repeats. One period is
        // the time its own wavefront takes to travel its own reach, so the ring
        // is born at the centre and dies at the rim instead of being cut off
        // somewhere in the middle of its life -- and every lap is keyed
        // separately, so a repeating source is not the same ring forever.
        float period = reach / speed;
        float laps = time / period;
        cycle = floor(laps);
        age = (laps - cycle) * period;
    }
    vec2 offset = position.xz - shape.xy;
    float radius = length(offset);
    if (radius > reach) {
        return;
    }
    vec2 radial = radius > 0.0001 ? offset / radius : vec2(1.0, 0.0);
    float angle = atan(radial.y, radial.x);
    RippleShape fingerprint = Fingerprint(shape.xy, cycle);

    // The wavefront is displaced outward and inward around the circle, so it
    // arrives at different times in different directions.
    float warp = sin(angle * fingerprint.lobes.x + fingerprint.phase) * fingerprint.depth.x +
                 sin(angle * fingerprint.lobes.y + fingerprint.phase * 1.7 + 1.1) *
                     fingerprint.depth.y;
    // And pulled out of round. A wavefront that is only wobbled in radius is
    // still a circle, and a circle is the one shape water never makes.
    float elongate = 1.0 + fingerprint.stretch * cos(2.0 * (angle - fingerprint.axis));
    float warpedRadius = radius * elongate + warp * wavelength * 0.42;
    float front = speed * age;
    if (front >= reach) {
        return;
    }
    float behind = warpedRadius - front;
    float width = max(motion.y, 0.35) * wavelength * fingerprint.width;
    if (behind < -width * 2.5 || behind > width * 2.5) {
        return;
    }
    // Both ends of the reach fade rather than clip. A ring that appears or
    // vanishes at a hard radius reads as a hole in the surface, not as a
    // disturbance that ended.
    float extentFade = (1.0 - smoothstep(reach * 0.55, reach, radius)) *
                       (1.0 - smoothstep(reach * 0.65, reach, front));
    float envelope = exp(-(behind * behind) / (width * width));
    float amplitude = strength * envelope * extentFade / (1.0 + front * 1.4);
    // Crests that outrun their neighbours lose amplitude, so a ring thins out
    // unevenly as it spreads instead of staying a clean circle of even height.
    float thinning = 0.62 + 0.38 * sin(angle * fingerprint.lobes.y + fingerprint.phase * 2.3);
    amplitude *= max(thinning, 0.0);
    // Ring spacing is perturbed as well, so the crests are not evenly pitched.
    float waveNumber = kTwoPi / wavelength;
    float phase = waveNumber * behind * (1.0 + 0.22 * warp);
    float ringSlope = cos(phase) * waveNumber * amplitude;

    slopeX += ringSlope * radial.x;
    slopeZ += ringSlope * radial.y;
    crest += sin(phase) * amplitude * 2.0;
}

bool LoadModelHit(out vec3 position, out vec3 normal, out vec3 baseColor, out float metallic,
                  out float roughness, out float emissive, out float waterMask,
                  out RtWaterMaterial waterOut, out vec2 texcoordOut)
{
    waterMask = 0.0;
    texcoordOut = vec2(0.5);
    // Every early return below leaves the surface dry, so a rejected hit can
    // never shade with whatever the previous invocation left behind.
    waterOut = DryWater();
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
    vec2 texcoord = a.texcoord.xy * bary.x + b.texcoord.xy * bary.y + c.texcoord.xy * bary.z;
    texcoordOut = texcoord;
    uint textureSlot = min(info.range.w, 63u);
    vec3 sampled = max(texture(modelTextures[textureSlot], texcoord).rgb, vec3(0.0));
    baseColor = max(info.baseColor.rgb, vec3(0.0)) * sampled;
    metallic = clamp(info.surface.x, 0.0, 1.0);
    roughness = clamp(info.surface.y, 0.04, 1.0);
    emissive = max(dot(info.emissive.rgb, vec3(0.2126, 0.7152, 0.0722)), 0.0);
    // emissive.w carries the host's water mask; its rgb channels still hold
    // emissive, so the two never contend for the same field.
    waterMask = clamp(info.emissive.w, 0.0, 1.0);
    waterOut = info.water;
    return true;
}
const float kPi = 3.14159265359;

/**
 * Henyey-Greenstein phase function.
 *
 * The medium does not scatter evenly: it throws light forward, which is why a
 * low sun blazes through haze and the same haze goes flat once the sun is
 * behind the camera. An isotropic medium cannot produce that, and it is most
 * of what separates fog from a grey overlay.
 */
float PhaseHG(float cosTheta, float anisotropy)
{
    float g = clamp(anisotropy, -0.95, 0.95);
    float g2 = g * g;
    float denominator = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * kPi * pow(max(denominator, 0.0001), 1.5));
}

/**
 * Optical depth of an exponential height fog along one straight segment.
 *
 * Integrated analytically rather than marched: the density is exponential in
 * height and the segment is a straight line, so the integral closes in one
 * expression. There is no reason to pay for sampling something with a closed
 * form, and no noise to denoise afterwards.
 */
float FogOpticalDepth(vec3 origin, vec3 direction, float distance, vec4 fog)
{
    float falloff = max(fog.y, 0.0001);
    float rise = direction.y * distance;
    float height = exp(-falloff * (origin.y - fog.z));
    // The limit of the ratio as the climb goes to zero is one, which is what a
    // horizontal ray sees.
    float span = abs(rise) > 0.001 ? (1.0 - exp(-falloff * rise)) / (falloff * rise) : 1.0;
    return max(fog.x, 0.0) * height * span * distance;
}

/** Direction of the frame's directional light, kept after sunset. */
vec3 TowardSun()
{
    uint lightCount = min(frame.header.y, 64u);
    vec3 found = vec3(-0.35, 0.78, 0.52);
    for (uint index = 0u; index < lightCount; ++index) {
        if (frame.lights[index].positionType.w != 0.0) {
            continue;
        }
        found = -frame.lights[index].directionRange.xyz;
        if (frame.lights[index].colorIntensity.w > 0.0) {
            break;
        }
    }
    return NormalizeOrUp(found);
}

const float kNoonSunIntensity = 6.5;

/** Radiance of the frame's directional light, in the units the sky uses. */
vec3 SunRadiance()
{
    uint lightCount = min(frame.header.y, 64u);
    for (uint index = 0u; index < lightCount; ++index) {
        if (frame.lights[index].positionType.w == 0.0 &&
            frame.lights[index].colorIntensity.w > 0.0) {
            // Same normalisation the miss stage uses. Raw intensity is in the
            // scene's exposure units (noon is 6.5); fog and foam that kept the
            // raw value painted every surface white and ignored the clock.
            return frame.lights[index].colorIntensity.rgb *
                   (frame.lights[index].colorIntensity.w / kNoonSunIntensity);
        }
    }
    return vec3(0.0);
}

/** How much of the day is still in the light, 0 after sunset. */
float DayAmount()
{
    return clamp(dot(SunRadiance(), vec3(0.3333)), 0.0, 1.0);
}

/** Moonlight that reaches a surface once the sun has gone. */
vec3 MoonRadiance()
{
    return vec3(0.20, 0.24, 0.36) * frame.celestial.w * (1.0 - DayAmount());
}

/** Unit direction toward the moon, or straight up when none was packed. */
vec3 TowardMoon()
{
    return length(frame.celestial.xyz) > 0.001 ? normalize(frame.celestial.xyz)
                                               : vec3(0.0, 1.0, 0.0);
}

/**
 * Average radiance of the environment a wide reflection lobe covers.
 *
 * A lobe much wider than a single ray cannot be represented by one mirror
 * sample. The sample is a sharp, full-detail copy of whatever geometry lies
 * along the mirror direction, and laying that onto a surface whose lobe spans
 * tens of degrees reads as an offset duplicate of the scene -- a ghost -- and
 * not as a reflection at all. What a wide lobe actually integrates is the
 * average of everything it covers, and the average of a sky is its gradient:
 * the sun disc and the cloud edges are precisely the high-frequency content
 * that widening washes out. So this term is the dome's own two-colour gradient
 * and nothing else, which is what makes it incapable of ghosting.
 */
vec3 EnvironmentRadiance(vec3 direction)
{
    vec3 zenith = max(frame.zenithColor.rgb, vec3(0.0));
    vec3 horizon = max(frame.horizonColor.rgb, vec3(0.0));
    // The same analytic dome the miss stage draws, so a rough surface and the
    // sky above it cannot disagree about which way is up. The sun disc is the
    // high-frequency content widening washes out, so only the sky itself.
    vec3 sky = AnalyticSky(normalize(direction), TowardSun(), SunRadiance() * 20.0, zenith, horizon);
    // Below the horizon the lobe is looking at the ground, which is neither
    // dome colour: it receives the frame's own ambient instead.
    vec3 ground = frame.ambientColorIntensity.rgb * frame.ambientColorIntensity.w;
    return mix(ground * 0.35, sky, smoothstep(-0.35, 0.05, direction.y));
}

/** Traces an occlusion ray toward a directional light; 1.0 when unobstructed. */
float SunVisibility(vec3 position, vec3 normal, vec3 toLight)
{
    shadowPayload = vec4(0.0);
    vec3 origin = position + normal * 0.02 + toLight * 0.02;
    uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT |
                 gl_RayFlagsOpaqueEXT;
    // Only instances that block light are tested. An object authored not to
    // cast a shadow is still in the tree for the camera to see, and this mask
    // is what keeps it from occluding the sun as well.
    traceRayEXT(scene, flags, LIGHT_BLOCKER_MASK, 0u, 0u, 1u, origin, 0.001, toLight, 600.0, 1);
    return shadowPayload.x;
}

/**
 * Fraction of the sun that reaches the medium along a segment.
 *
 * Averaged over a few points rather than evaluated once at the far end. One
 * sample answers "is the surface lit", which is a different question from "is
 * the air in front of it lit", and only the second one puts a shaft behind a
 * pillar.
 */
float FogSunVisibility(vec3 origin, vec3 direction, float distance, vec3 toSun, uint steps)
{
    if (steps == 0u) {
        return 1.0;
    }
    float visible = 0.0;
    for (uint index = 0u; index < steps; ++index) {
        float t = (float(index) + 0.5) / float(steps) * distance;
        // No surface normal at a point in mid-air, so only the light-side
        // offset applies.
        visible += SunVisibility(origin + direction * t, vec3(0.0), toSun);
    }
    return visible / float(steps);
}

/**
 * Attenuates a surface by the medium in front of it and adds what the medium
 * itself scatters back.
 *
 * @param primary Whether this is a first-bounce camera ray. Only those pay for
 *        the shaft samples: a reflection sees the same air, and marching it
 *        again per bounce would multiply the cost without changing the picture.
 */
vec3 ApplyMedium(vec3 color, vec3 origin, vec3 direction, float distance, bool primary)
{
    if (frame.fog.x <= 0.0 || distance <= 0.0) {
        return color;
    }
    float marched = min(distance, max(frame.fogLight.w, 0.0));
    float steps = primary ? frame.fogLight.z : 0.0;
    vec3 toSun = TowardSun();
    float sunReach = FogSunVisibility(origin, direction, marched, toSun, uint(steps));
    float opticalDepth = FogOpticalDepth(origin, direction, distance, frame.fog);
    float transmittance = exp(-opticalDepth);
    float phase = PhaseHG(dot(direction, toSun), frame.fog.w);
    vec3 scattered = SunRadiance() * phase * frame.fogLight.x * sunReach +
                     frame.ambientColorIntensity.rgb * frame.ambientColorIntensity.w *
                         frame.fogLight.y;
    // What arrives is the surface seen through the medium plus the medium's own
    // light. Both terms carry the same transmittance, so nothing is counted
    // twice and nothing is left unattenuated.
    return color * transmittance + scattered * (1.0 - transmittance);
}

/**
 * One light's contribution to a surface, split into its two lobes.
 *
 * Kept apart because the two do not lose light the same way. A shadow removes
 * what a surface would have scattered and what it would have mirrored back, but
 * a transparent surface only ever loses the second: the light that crossed it
 * carried on to whatever is underneath, and that surface casts its own shadow.
 * Occluding both here is what puts a second copy of every shadow on the water.
 */
struct SurfaceLight {
    /** Light scattered by the surface itself. */
    vec3 diffuse;
    /** Light mirrored back toward the eye. */
    vec3 specular;
};

SurfaceLight ShadeLight(FrameLightData light, vec3 baseColor, vec3 normal,
                        vec3 position, vec3 viewDirection, float metallic, float roughness)
{
    SurfaceLight result;
    result.diffuse = vec3(0.0);
    result.specular = vec3(0.0);
    if (light.colorIntensity.w <= 0.0) return result;
    vec3 toLight;
    float attenuation = 1.0;
    float intensity = light.colorIntensity.w;
    if (light.positionType.w == 0.0) {
        toLight = NormalizeOrUp(-light.directionRange.xyz);
    } else {
        vec3 offset = light.positionType.xyz - position;
        float distanceToLight = length(offset);
        float range = max(light.directionRange.w, 0.001);
        if (distanceToLight >= range) return result;
        toLight = NormalizeOrUp(offset);
        float falloff = 1.0 - distanceToLight / range;
        attenuation = falloff * falloff / (1.0 + distanceToLight * distanceToLight * 0.055);
        intensity *= 0.22;
        if (light.positionType.w == 2.0 &&
            dot(NormalizeOrUp(position - light.positionType.xyz),
                NormalizeOrUp(light.directionRange.xyz)) < light.spotShadow.x) {
            return result;
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
    vec3 scale = light.colorIntensity.rgb * intensity * attenuation;
    result.diffuse = scale * diffuseColor;
    result.specular = scale * specularColor * specular * 0.22;
    return result;
}

/** 2D hash used by the waterfall flow field. */
float WaterHash12(vec2 cell)
{
    vec3 hash = fract(vec3(cell.x, cell.y, cell.x) * vec3(0.1031, 0.1030, 0.0973));
    hash += dot(hash, hash.yzx + 33.33);
    return fract((hash.x + hash.y) * hash.z);
}

/** Value noise on the sheet, so the fall can panner a field instead of sines. */
float WaterValueNoise2(vec2 point)
{
    vec2 base = floor(point);
    vec2 fraction = point - base;
    vec2 t = fraction * fraction * fraction * (fraction * (fraction * 6.0 - 15.0) + 10.0);
    float c00 = WaterHash12(base);
    float c10 = WaterHash12(base + vec2(1.0, 0.0));
    float c01 = WaterHash12(base + vec2(0.0, 1.0));
    float c11 = WaterHash12(base + vec2(1.0, 1.0));
    return mix(mix(c00, c10, t.x), mix(c01, c11, t.x), t.y);
}

float WaterFractalNoise2(vec2 point, int octaves)
{
    float total = 0.0;
    float weight = 1.0;
    float normalizer = 0.0;
    for (int index = 0; index < octaves; ++index) {
        total += WaterValueNoise2(point) * weight;
        normalizer += weight;
        point = point * 2.11 + vec2(17.2, 9.4);
        weight *= 0.5;
    }
    return normalizer > 0.0 ? total / normalizer : 0.0;
}

/**
 * Everything the water pass needs after the surface has been evaluated.
 *
 * Basin and waterfall share this record so the lighting below does not have
 * to know which recipe produced the normal, the foam or the window.
 */
struct WaterSurfaceEval {
    vec3 normal;
    vec3 albedo;
    float roughness;
    float crest;
    float foam;
    float opacity;
    bool falling;
};

/**
 * Horizontal swell plus every live ripple, which is what a basin is.
 */
WaterSurfaceEval EvaluateBasinSurface(vec3 position, float time, RtWaterMaterial water,
                                      float footprint)
{
    WaterSurfaceEval ev;
    ev.falling = false;
    ev.opacity = 1.0;
    ev.albedo = vec3(0.82, 0.88, 0.92);
    float slopeX = 0.0;
    float slopeZ = 0.0;
    float crest = 0.0;
    float unresolvedSlope = 0.0;
    SwellSlope(position, time, water, footprint, slopeX, slopeZ, crest, unresolvedSlope);
    uint rippleCount = min(uint(water.surface.z), 4u);
    for (uint index = 0u; index < rippleCount; ++index) {
        AddRippleSlope(position, time, water.rippleShape[index], water.rippleMotion[index],
                       footprint, slopeX, slopeZ, crest);
    }
    uint liveRipples = min(uint(frame.surfaceInfo.x), uint(rippleSources.sources.length()));
    for (uint index = 0u; index < liveRipples; ++index) {
        RtRippleSource source = rippleSources.sources[index];
        AddRippleSlope(position, time, source.shape, source.motion, footprint, slopeX, slopeZ,
                       crest);
    }
    ev.normal = NormalizeOrUp(vec3(-slopeX, 1.0, -slopeZ));
    ev.roughness = clamp(0.02 + 0.70 * unresolvedSlope, 0.02, 0.55);
    ev.crest = crest;
    ev.foam = clamp(smoothstep(water.surface.x, water.surface.x + 0.4, crest) * water.surface.y,
                    0.0, 1.0);
    return ev;
}

/**
 * A falling sheet, evaluated the way a UE water material is authored.
 *
 * Two panners at unrelated scales, a domain warp so the streaks are not a
 * barcode, a ragged UV edge so the silhouette is not a rectangle, and an
 * opacity the caller can punch through -- that last is what lets the rock
 * behind show in the gaps, which is the whole difference between a waterfall
 * and a painted slat.
 */
WaterSurfaceEval EvaluateWaterfallSurface(vec3 position, vec3 geometricNormal, vec2 uv,
                                          float time, RtWaterMaterial water)
{
    WaterSurfaceEval ev;
    ev.falling = true;
    vec3 geom = NormalizeOrUp(geometricNormal);
    vec3 down = vec3(0.0, -1.0, 0.0);
    vec3 across = cross(geom, down);
    if (dot(across, across) < 0.0001) {
        across = cross(geom, vec3(1.0, 0.0, 0.0));
    }
    across = normalize(across);
    vec3 along = normalize(cross(across, geom));

    vec2 chart = uv;
    // A mesh without texcoords still has to flow. The face chart is a
    // fallback, not the authored look: UV is what makes the panners line up
    // across a card the way a UE material expects.
    if (chart.x < 0.0 || chart.x > 1.0 || (abs(chart.x) + abs(chart.y)) < 0.0001) {
        chart = vec2(clamp(dot(position, across) * 0.32 + 0.5, 0.0, 1.0),
                     fract(dot(position, -along) * 0.16));
    }

    float speed = max(water.wave.z, 0.4);
    // UE waterfall card: a few persistent columns, fast downward streaks
    // inside them, and empty UV everywhere else so the rock reads through.
    float columns = WaterFractalNoise2(vec2(chart.x * 6.4, 0.16), 2);
    float columnMask = smoothstep(0.38, 0.60, columns);
    vec2 flowUv = vec2(chart.x * 5.8 + columns * 0.45, chart.y * 14.0 + time * speed * 0.58);
    float flow = WaterFractalNoise2(flowUv, 4);
    float streaks = pow(smoothstep(0.36, 0.70, flow), 1.15) * columnMask;
    float threads = pow(smoothstep(0.72, 0.90,
                                   WaterFractalNoise2(vec2(chart.x * 16.0,
                                                           chart.y * 10.0 + time * speed * 0.80),
                                                      2)),
                        1.5);
    float ragged = 0.05 + (1.0 - columnMask) * 0.16;
    float edge = smoothstep(0.0, ragged, chart.x) * smoothstep(1.0, 1.0 - ragged, chart.x);
    float lip = pow(smoothstep(0.86, 1.0, chart.y), 0.65);
    float splash = pow(1.0 - smoothstep(0.0, 0.14, chart.y), 1.15);
    ev.opacity = clamp((streaks + threads * 0.55 + lip * 0.80 + splash * 0.42) * edge, 0.0, 1.0);

    float eps = 0.010;
    float gradientX = WaterFractalNoise2(flowUv + vec2(eps, 0.0), 3) - flow;
    float gradientY = WaterFractalNoise2(flowUv + vec2(0.0, eps), 3) - flow;
    float fall = max(water.surface.w, 0.35);
    ev.normal = NormalizeOrUp(geom + across * gradientX * fall * 3.8 + along * gradientY * fall * 3.0);
    ev.roughness = 0.16;
    ev.crest = streaks;
    ev.foam = clamp(streaks * water.surface.y * 0.90 + lip * 0.85 + splash * 0.50, 0.0, 1.0);
    ev.albedo = mix(vec3(0.10, 0.28, 0.38), vec3(0.90, 0.94, 0.98), ev.foam);
    return ev;
}

/**
 * Beer-Lambert through the volume, plus the light the volume itself scatters.
 *
 * This is the window a basin is. A waterfall must not call it: refracting
 * through a vertical card copies the courtyard beside the column.
 */
vec3 TraceWaterRefraction(vec3 position, vec3 rayDirection, vec3 facingNormal,
                          vec3 viewDirection, vec3 towardSun, vec3 volumeLight,
                          RtWaterMaterial water, float depth)
{
    vec3 bent = refract(rayDirection, facingNormal, 1.0 / max(water.optics.x, 1.0));
    if (dot(bent, bent) <= 0.5 || depth >= 1.0) {
        return vec3(0.0);
    }
    vec3 through = normalize(mix(rayDirection, bent, clamp(water.optics.w, 0.0, 1.0)));
    payload = vec4(0.0, 0.0, 0.0, depth + 1.0 + kTransmissionFlag);
    traceRayEXT(scene, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, position, 0.01, through,
                10000.0, 0);
    float depthScale = max(water.optics.z, 0.05);
    vec3 absorption = max(water.absorbance.rgb, vec3(0.0)) / depthScale;
    float scatterRate = max(water.absorbance.w, 0.0) / depthScale;
    vec3 extinction = absorption + vec3(scatterRate);
    vec3 transmittance = exp(-extinction * payload.w);
    vec3 albedo = vec3(scatterRate) / max(extinction, vec3(0.0001));
    float phase = PhaseHG(dot(viewDirection, towardSun), 0.42);
    vec3 scattered = volumeLight * albedo * phase;
    return payload.rgb * transmittance + scattered * (vec3(1.0) - transmittance);
}

bool WaterRefracts(vec3 rayDirection, vec3 facingNormal, RtWaterMaterial water)
{
    vec3 bent = refract(rayDirection, facingNormal, 1.0 / max(water.optics.x, 1.0));
    return dot(bent, bent) > 0.5;
}

// How far past a footprint's edge the wet look still reaches, in world units.
// Splash and capillary rise wet the ground beside a pool, not only the pool
// itself, so the fade has to start outside the authored radius rather than
// exactly on it.
const float kWetnessFringe = 1.2;
// How far above the surface height the wet look still reaches. A shoreline
// is damp for a hand's width above the waterline, not for a metre.
const float kWetnessBand = 0.55;

/**
 * How wet a dry surface point should look, 0..1.
 *
 * `WaterBodyComponent` gives `WaterSplashSystem` an exact rectangle to test a
 * falling body against; here the same footprints are approximated as circles
 * (see RenderWaterBodySnapshot) because this runs per dry fragment rather
 * than once per physics step, and every body sampled so far draws round
 * enough that the difference never reads. The nearest body wins rather than
 * summing every one, so two footprints close together do not double-wet the
 * ground between them.
 */
float Wetness(vec3 position)
{
    float wetness = 0.0;
    for (int index = 0; index < frame.wetnessBodies.length(); ++index) {
        vec4 body = frame.wetnessBodies[index];
        float radius = body.w;
        if (radius <= 0.0) {
            continue;
        }
        float edgeDistance = length(position.xz - vec2(body.x, body.z)) - radius;
        float horizontal = 1.0 - smoothstep(0.0, kWetnessFringe, edgeDistance);
        if (horizontal <= 0.0) {
            continue;
        }
        // Fully wet at or slightly below the surface; fades out climbing above
        // it. The small negative bias keeps a fragment sitting exactly on the
        // waterline from landing on the fade's own knee.
        float heightAbove = position.y - body.y;
        float vertical = 1.0 - smoothstep(-0.15, kWetnessBand, heightAbove);
        wetness = max(wetness, horizontal * clamp(vertical, 0.0, 1.0));
    }
    return clamp(wetness, 0.0, 1.0);
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
    float waterMask;
    RtWaterMaterial water;
    vec2 texcoord = vec2(0.5);
    const bool modelInstance = (gl_InstanceCustomIndexEXT & MODEL_INSTANCE_BIT) != 0u;
    if (!LoadModelHit(position, normal, baseColor, metallic, roughness, emissive, waterMask,
                      water, texcoord) &&
        modelInstance) {
        baseColor = vec3(1.0, 0.0, 1.0);
        roughness = 1.0;
        metallic = 0.0;
        emissive = 0.0;
        waterMask = 0.0;
        water = DryWater();
        normal = NormalizeOrUp(transpose(mat3(gl_WorldToObjectEXT)) * vec3(0.0, 1.0, 0.0));
        position = gl_WorldRayOriginEXT + rayDirection * gl_HitTEXT;
    } else if (!modelInstance) {
        uint instance = gl_InstanceCustomIndexEXT;
        uint slot = instance & BOX_MATERIAL_MASK;
        if ((instance & BOX_MATERIAL_BIT) != 0u && slot < boxMaterials.materials.length()) {
            RtBoxMaterial authored = boxMaterials.materials[slot];
            baseColor = max(authored.albedo.rgb, vec3(0.0));
            metallic = clamp(authored.surface.x, 0.0, 1.0);
            roughness = clamp(authored.surface.y, 0.04, 1.0);
            emissive = max(authored.surface.z, 0.0);
        } else {
            // Only the material-less stand-in geometry lands here. A slot that
            // is set but out of range means the host and this shader disagree
            // about the array, which shows up as an unmistakable magenta.
            bool authoredSlot = (instance & BOX_MATERIAL_BIT) != 0u;
            baseColor = authoredSlot ? vec3(1.0, 0.0, 1.0) : palette[slot % 8u];
            roughness = authoredSlot ? 1.0 : roughnesses[slot % 8u];
            metallic = authoredSlot ? 0.0 : metallics[slot % 8u];
            emissive = 0.0;
        }
        uint primitive = min(uint(max(gl_PrimitiveID, 0)), 11u);
        normal = NormalizeOrUp(transpose(mat3(gl_WorldToObjectEXT)) * faceNormals[primitive]);
        position = gl_WorldRayOriginEXT + rayDirection * gl_HitTEXT;
        waterMask = 0.0;
        water = DryWater();
    }
    // Only a dry surface can be wetted: water already answers for its own
    // look in the block below, and wetting its reflection a second time would
    // double the effect right where it is least deniable.
    if (waterMask <= 0.5) {
        float wet = Wetness(position);
        if (wet > 0.0) {
            // Darker and shinier, the two things every wet material does
            // regardless of what it is dry: water fills in the microfacets a
            // rough surface would otherwise scatter light out of, and it adds
            // its own thin specular layer on top.
            baseColor *= mix(1.0, 0.70, wet);
            roughness = mix(roughness, min(roughness, 0.10), wet);
            metallic = mix(metallic, max(metallic, 0.03), wet);
        }
    }
    // A ray looking through water reports how far it travelled in the payload's
    // w rather than a recursion depth, so the two encodings are told apart once
    // here. Either way the value is read before the water pass below reuses the
    // payload slot for its own refraction ray and clobbers it.
    const bool isTransmission = payload.w >= kTransmissionFlag;
    const float depth = isTransmission ? payload.w - kTransmissionFlag : payload.w;
    vec3 transmission = vec3(0.0);
    float transmissionWeight = 0.0;
    // Negative for dry surfaces, which leaves the reflection block below to
    // compute its own Schlick term.
    float waterReflectance = -1.0;
    // Carried out of the water block so the foam can be laid over the finished
    // surface rather than only over its body.
    float foamAmount = 0.0;
    vec3 foamRadiance = vec3(0.0);
    vec3 geometricNormal = normal;
    if (waterMask > 0.5) {
        float footprint = gl_HitTEXT * frame.surfaceInfo.z;
        WaterSurfaceEval ev = water.surface.w > 0.001
                                  ? EvaluateWaterfallSurface(position, geometricNormal, texcoord,
                                                             frame.frameTime.x, water)
                                  : EvaluateBasinSurface(position, frame.frameTime.x, water,
                                                         footprint);
        normal = ev.normal;
        roughness = ev.roughness;
        metallic = 0.0;

        vec3 towardSun = TowardSun();
        vec3 volumeLight = SunRadiance() + MoonRadiance() +
                           frame.ambientColorIntensity.rgb * frame.ambientColorIntensity.w;
        foamAmount = ev.foam;
        foamRadiance = volumeLight * (ev.falling ? 0.30 : 0.16) * vec3(0.94, 0.96, 0.99);

        if (ev.falling && ev.opacity < 0.055 && depth < 2.0) {
            payload = vec4(0.0, 0.0, 0.0, depth + 1.0);
            traceRayEXT(scene, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, position, 0.008,
                        rayDirection, 10000.0, 0);
            vec3 punched = ApplyMedium(payload.rgb, gl_WorldRayOriginEXT, rayDirection,
                                       gl_HitTEXT, depth < 0.5 && !isTransmission);
            payload = isTransmission ? vec4(punched, gl_HitTEXT) : vec4(punched, depth + 1.0);
            return;
        }

        vec3 facingNormal = dot(rayDirection, normal) > 0.0 ? -normal : normal;
        float facing = abs(dot(viewDirection, facingNormal));
        waterReflectance = clamp(0.02 + 0.98 * pow(1.0 - facing, 5.0), 0.0, 1.0);

        if (ev.falling) {
            waterReflectance = 0.055;
            baseColor = ev.albedo;
            if (ev.opacity < 0.96 && depth < 2.0) {
                payload = vec4(0.0, 0.0, 0.0, depth + 1.0);
                traceRayEXT(scene, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, position, 0.008,
                            rayDirection, 10000.0, 0);
                transmission = payload.rgb;
                transmissionWeight = 1.0 - ev.opacity;
            }
        } else {
            baseColor = mix(baseColor, ev.albedo, foamAmount);
            bool refracts = WaterRefracts(rayDirection, facingNormal, water);
            if (refracts) {
                transmission = TraceWaterRefraction(position, rayDirection, facingNormal,
                                                    viewDirection, towardSun, volumeLight, water,
                                                    depth);
            }
            transmissionWeight = refracts ? water.optics.y * (1.0 - waterReflectance) : 0.0;
        }
    }
    // The 6.5% floor used to ignore the clock: after sunset every surface
    // still carried a daylight fill, which is why Night looked like dusk.
    float dayFill = DayAmount();
    vec3 color = baseColor * (vec3(0.010 + 0.055 * dayFill) + frame.ambientColorIntensity.rgb *
                 frame.ambientColorIntensity.w * (0.7 + 0.3 * max(normal.y, 0.0)));
    color += baseColor * MoonRadiance() * max(dot(normal, TowardMoon()), 0.0) * 0.55;
    if (transmissionWeight > 0.0) {
        // What came through the surface replaces the body in proportion to what
        // the surface let past. The light loop below still adds the specular
        // the wave faces catch, so the sun keeps glittering off the water it is
        // also shining through.
        color = mix(color, transmission, transmissionWeight);
    }
    if (foamAmount > 0.0) {
        // Laid over the finished surface, not mixed into the body. Foam only
        // drawn into the body disappears exactly when a water surface is
        // clearest, which is most of the time it is looked at: the whitecaps
        // that give waves their shape were being mixed away by transmission.
        color = mix(color, foamRadiance, foamAmount);
    }
    uint lightCount = min(frame.header.y, 64u);
    for (uint index = 0u; index < lightCount; ++index) {
        SurfaceLight lit = ShadeLight(frame.lights[index], baseColor, normal, position,
                                      viewDirection, metallic, roughness);
        if (waterMask > 0.5) {
            // A basin is not a Lambert lid. Its body is the transmission
            // already mixed in; adding a diffuse lobe paints a second, opaque
            // surface on the plane and every object reads twice. A falling
            // sheet has no such window, so it keeps a share of the lobe --
            // that is the white body of aerated water.
            if (water.surface.w > 0.001) {
                lit.diffuse *= 0.70;
            } else {
                lit.diffuse = vec3(0.0);
            }
        }
        if (frame.lights[index].positionType.w == 0.0) {
            float occlusion = SunVisibility(
                position, normal, NormalizeOrUp(-frame.lights[index].directionRange.xyz));
            // The sun a mirror turns back is the sun a mirror can lose, which is
            // why a passing shadow puts out the glitter on water. What a shadow
            // does *not* remove from water is its body: that light crossed the
            // surface rather than being turned back by it, and it is the floor
            // two metres below that has to answer for whether the sun reached
            // it -- through the refraction above, which applies that shadow
            // already.
            lit.specular *= occlusion;
            if (waterMask <= 0.5) {
                lit.diffuse *= occlusion;
            }
        }
        color += lit.diffuse + lit.specular;
    }
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 4.0);
    color += mix(vec3(0.04, 0.08, 0.18) * dayFill + MoonRadiance() * 0.35, baseColor, 0.45) *
             rim * (0.18 + metallic * 0.3);
    color += baseColor * emissive;

    // One bounce for every surface, not just smooth metals. Schlick says a
    // rough dielectric still reflects at grazing angles, which is what stops
    // a matte floor from reading as a flat card; metalness then decides how
    // strong that reflection is head-on. The nested payload's w seeds the
    // next depth, so recursion terminates after a single bounce.
    vec3 f0 = mix(vec3(0.04), baseColor, metallic);
    float grazing = pow(1.0 - max(dot(viewDirection, normal), 0.0), 5.0);
    const float smoothness = 1.0 - roughness;
    float reflectivity =
        clamp(dot(mix(f0, vec3(1.0), grazing), vec3(0.3333)), 0.0, 1.0) * smoothness * smoothness;
    // How much of the lobe one mirror ray actually stands for.
    //
    // A reflection narrows as a surface smooths and spreads as it roughens, and
    // one ray cannot spread anything: what it returns is a sharp, full-detail
    // copy of whatever lies along the mirror direction, which on a surface whose
    // lobe spans tens of degrees reads as an offset duplicate of the scene and
    // not as a reflection at all. The ray is therefore only representative in
    // proportion to how narrow the lobe is. Measured in roughness rather than
    // in smoothness, because that is the quantity the lobe's own width is
    // quoted in: a polished surface keeps all of its ray, satin keeps about a
    // fifth, and anything past a third of the way to fully rough keeps none and
    // is answered by the environment instead.
    //
    // The bands matter at both ends. Reaching exactly 1 below a mirror's own
    // roughness is what keeps a polished sphere a mirror rather than a slightly
    // milky one, and reaching exactly 0 by a third is what removes the ghost
    // rather than merely dimming it.
    // Written as an inverted ascending ramp rather than a descending one:
    // smoothstep is only defined for edge0 < edge1, and handing it the reversed
    // pair is undefined rather than merely backwards -- which on this hardware
    // returned 1 for the roughest surfaces and left every ghost in place.
    float mirrorFraction = 1.0 - smoothstep(0.06, 0.32, roughness);
    if (waterReflectance >= 0.0) {
        // Water already spent this exact term deciding how much light crossed
        // the surface. Reusing it is what keeps transmission and reflection
        // from together exceeding the light that arrived. Its lobe is the one
        // the unresolved wave slope above just widened, so mirrorFraction is
        // deliberately left to that same roughness: close enough to resolve the
        // waves the water is the mirror water is, and past that distance it is
        // the sky those waves average out to rather than a sharp copy of every
        // object afloat.
        reflectivity = waterReflectance;
        // A sharp scene copy sitting next to every object on the water is the
        // ghost. The sky the waves actually average to does not carry one.
        mirrorFraction = 0.0;
    }
    if (depth < 1.0 && reflectivity > 0.004) {
        vec3 reflected = NormalizeOrUp(reflect(rayDirection, normal));
        // The environment costs a gradient, so a surface whose lobe is wider
        // than a ray gets its reflection without paying for a trace it could
        // not have used.
        vec3 sampled = EnvironmentRadiance(reflected);
        if (mirrorFraction > 0.02) {
            // Seed the shared slot before tracing; the nested hit overwrites it
            // with its own colour and a depth one past ours, which is what stops
            // the bounce from recursing.
            payload = vec4(0.0, 0.0, 0.0, depth + 1.0);
            vec3 reflOrigin = position + normal * 0.02 + reflected * 0.02;
            traceRayEXT(scene, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, reflOrigin,
                        0.001, reflected, 10000.0, 0);
            sampled = mix(sampled, payload.rgb, mirrorFraction);
        }
        color = mix(color, sampled * mix(vec3(1.0), baseColor, 0.15),
                    clamp(reflectivity, 0.0, 1.0));
    }
    // The medium sits in front of everything this invocation produced, so it
    // is applied once, last, to the finished radiance rather than to each
    // contribution on the way in.
    color = ApplyMedium(color, gl_WorldRayOriginEXT, rayDirection, gl_HitTEXT,
                        depth < 0.5 && !isTransmission);
    // Radiance leaves this shader uncompressed so a surface that reflects it
    // can keep accumulating light; raygen maps the final value once. A
    // transmission ray is answered with the distance it covered instead, which
    // is what the parent needs to absorb over.
    payload = isTransmission ? vec4(color, gl_HitTEXT) : vec4(color, depth + 1.0);
}
