// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

/**
 * World-space volumetric cloud layer shared by the stages that draw sky.
 *
 * The layer is a horizontal slab between cloudAltitude and that plus
 * cloudThickness, and every sample is evaluated at its own world position
 * rather than along the view direction. That is the whole difference between
 * a cloud deck and a dome texture: a slab sampled in world space has a near
 * face and a far face, slides past as the camera moves, and can be flown into,
 * because the noise argument changes with position and not only with the angle
 * the ray was cast at.
 *
 * The march is adaptive rather than bracketed. An earlier arrangement scanned
 * the slab coarsely, kept the first and last samples that hit cloud, and
 * divided *that* interval into the fine steps -- which makes the step length a
 * per-pixel quantity derived from where a coarse sample happened to land. Two
 * neighbouring pixels whose coarse scans disagreed by one sample then marched
 * the same cloud at strides hundreds of units apart, and the sky broke into
 * rectangular tiles of visibly different density. Here the stride is a fixed
 * length instead: long while the ray is in clear air, short the moment it
 * enters cloud, and never a function of what a previous sample found.
 *
 * The host must declare the frame block (cloud / cloudDetail / cloudShape /
 * cloudMarch) before including this file. Lighting arrives as parameters
 * instead, because the stages disagree about what the sun is worth to them.
 */

#ifndef CONCORD_CLOUD_GLSL
#define CONCORD_CLOUD_GLSL

/**
 * How far a single ray is allowed to march, as a multiple of layer thickness.
 *
 * A slab has no far wall along a near-horizontal ray: the crossing length goes
 * as thickness / sin(elevation), so a ray a degree above the horizon would
 * march for tens of kilometres and spend the whole step budget on one pixel.
 * The bound is what keeps the cost of a frame independent of where the camera
 * happens to be looking.
 *
 * Six layer depths rather than eighteen. The far end of that walk was both the
 * most expensive part of the frame -- near-horizon rays are a wide band across
 * the middle of it -- and the least honest: at ten kilometres a cloud is behind
 * enough air to have lost its contrast entirely, so what the extra distance
 * bought was a faint grey smear costing three times what the visible sky cost.
 * The distance fade below retires the layer before the bound is reached, so the
 * cut is a haze rather than an edge.
 */
const float kCloudMaxMarch = 6.0;

/**
 * Distance, in layer depths, over which a cloud fades into the air in front of
 * it.
 *
 * Aerial perspective, and also what makes the march bound above invisible. A
 * hard stop at a fixed range draws a line across the sky; the same stop with
 * the last third faded out reads as the horizon haze that is genuinely there.
 */
const float kCloudFadeStart = 3.2;

/** Transmittance below which the rest of the march cannot change the pixel. */
const float kCloudMinTransmittance = 0.015;

/**
 * How much longer a clear-air step is than an in-cloud one.
 *
 * The ratio is what makes the march affordable without making it inaccurate:
 * crossing empty sky at the resolution a cloud edge needs is the entire cost of
 * a naive march, and overshooting a cloud boundary by one long step is undone
 * by the step back below.
 */
const float kCloudSkipRatio = 5.0;

/** Consecutive empty samples before the march gives up on the cloud it was in. */
const int kCloudEmptyRun = 6;

/**
 * How much finer the noise is vertically than horizontally.
 *
 * A cloud is wider than it is deep, so one isotropic lattice sized to look
 * right across the sky has barely two cells through the layer, and every cloud
 * in it is the same shape at every altitude -- which is what a deck of flat
 * sheets stacked on each other actually is. Squashing the lattice vertically
 * buys the structure that makes a cloud read as a body with a base, a middle
 * and a top.
 */
const float kCloudVerticalDetail = 2.6;

float CloudHash13(vec3 cell)
{
    vec3 hash = fract(cell * vec3(0.1031, 0.1030, 0.0973));
    hash += dot(hash, hash.yzx + 33.33);
    return fract((hash.x + hash.y) * hash.z);
}

float CloudHash12(vec2 cell)
{
    vec3 hash = fract(vec3(cell.x, cell.y, cell.x) * vec3(0.1031, 0.1030, 0.0973));
    hash += dot(hash, hash.yzx + 33.33);
    return fract((hash.x + hash.y) * hash.z);
}

/** Trilinear value noise on the unit lattice, smoothstepped so it has no creases. */
float CloudValueNoise3(vec3 point)
{
    vec3 base = floor(point);
    vec3 offset = fract(point);
    offset = offset * offset * (3.0 - 2.0 * offset);
    float c000 = CloudHash13(base);
    float c100 = CloudHash13(base + vec3(1.0, 0.0, 0.0));
    float c010 = CloudHash13(base + vec3(0.0, 1.0, 0.0));
    float c110 = CloudHash13(base + vec3(1.0, 1.0, 0.0));
    float c001 = CloudHash13(base + vec3(0.0, 0.0, 1.0));
    float c101 = CloudHash13(base + vec3(1.0, 0.0, 1.0));
    float c011 = CloudHash13(base + vec3(0.0, 1.0, 1.0));
    float c111 = CloudHash13(base + vec3(1.0, 1.0, 1.0));
    return mix(mix(mix(c000, c100, offset.x), mix(c010, c110, offset.x), offset.y),
               mix(mix(c001, c101, offset.x), mix(c011, c111, offset.x), offset.y), offset.z);
}

float CloudValueNoise2(vec2 point)
{
    vec2 base = floor(point);
    vec2 offset = fract(point);
    offset = offset * offset * (3.0 - 2.0 * offset);
    float c00 = CloudHash12(base);
    float c10 = CloudHash12(base + vec2(1.0, 0.0));
    float c01 = CloudHash12(base + vec2(0.0, 1.0));
    float c11 = CloudHash12(base + vec2(1.0, 1.0));
    return mix(mix(c00, c10, offset.x), mix(c01, c11, offset.x), offset.y);
}

/**
 * Fractal sum, rotated between octaves.
 *
 * Doubling the frequency on axis leaves every octave's lattice aligned with
 * the one below it, and the sum then shows the lattice as a grid of creases
 * running through the cloud. The rotation is what breaks that alignment.
 */
float CloudFbm3(vec3 point, int octaves)
{
    const mat3 turn = mat3(0.00, 0.80, 0.60, -0.80, 0.36, -0.48, -0.60, -0.48, 0.64);
    float total = 0.0;
    float weight = 0.5;
    float normalization = 0.0;
    for (int index = 0; index < octaves; ++index) {
        total += CloudValueNoise3(point) * weight;
        normalization += weight;
        point = turn * point * 2.02;
        weight *= 0.5;
    }
    return total / max(normalization, 0.0001);
}

float CloudFbm2(vec2 point, int octaves)
{
    const mat2 turn = mat2(0.80, 0.60, -0.60, 0.80);
    float total = 0.0;
    float weight = 0.5;
    float normalization = 0.0;
    for (int index = 0; index < octaves; ++index) {
        total += CloudValueNoise2(point) * weight;
        normalization += weight;
        point = turn * point * 2.03;
        weight *= 0.5;
    }
    return total / max(normalization, 0.0001);
}

/**
 * Large-scale weather at one place on the ground.
 *
 * Sampled over XZ only, because weather is a map and not a volume: what varies
 * with height is the profile below, and letting the coverage vary with height
 * as well would punch holes through the middle of a cloud rather than leaving
 * gaps between clouds.
 *
 * Sampled once per marched point and then carried, rather than re-derived
 * inside every density evaluation. The light march below runs a few hundred
 * units through a field whose cells are a couple of thousand across, so it is
 * reading the same weather its originating sample already paid for -- and it
 * runs several times per marched point, which made that the most repeated work
 * in the whole layer.
 */
struct CloudWeather {
    /** Local coverage, 0 for clear sky and 1 for solid cloud. */
    float coverage;
    /** Local type, 0 for a flat sheet and 1 for a tower. */
    float type;
    /** Fraction of the layer this cell's cloud reaches up to. */
    float top;
};

CloudWeather SampleCloudWeather(vec2 ground, vec2 wind)
{
    float scale = max(frame.cloud.w, 1.0);
    vec2 chart = (ground + wind) / scale;
    float cells = CloudFbm2(chart, 3);
    // Stretched hard away from its own mean before it is used. A fractal sum
    // sits tightly around 0.5, so a coverage driven by the raw value is one
    // uniform sky with a faint modulation on it -- an overcast lid. Pushing the
    // contrast up is what makes most of the sky clear and the rest a bank of
    // cloud, which is the arrangement an eye reads as weather.
    float shaped = clamp((cells - 0.52) * 3.4 + 0.5, 0.0, 1.0);
    // One more field, read two ways. Both the kind of cloud a cell grows and
    // how tall it grows vary over the same landscape scale, and sampling that
    // landscape twice to ask two questions about it costs twice as much for a
    // correlation nobody can see.
    float character = CloudFbm2(chart * 0.71 + 23.4, 2);
    CloudWeather weather;
    weather.coverage = clamp(frame.cloud.x * shaped * 1.45, 0.0, 1.0);
    weather.type = clamp(frame.cloudShape.w * (0.45 + 1.05 * character), 0.0, 1.0);
    // How far up this particular cell builds. A deck whose every cloud stops at
    // the same height is a ceiling however well it is shaded; letting the tops
    // disagree is most of what makes a sky read as three-dimensional. The base
    // is deliberately *not* varied: real cumulus all condense at the same
    // altitude, and a flat common base is a thing the eye expects to see.
    weather.top = mix(0.32, 1.0, 1.0 - character);
    return weather;
}

/**
 * Where through its own depth a cloud of the given type keeps its mass.
 *
 * Evaluated against the cell's own top rather than the layer ceiling, so a
 * short cell is a complete small cloud and not the bottom slice of a tall one.
 */
float CloudHeightProfile(float heightFraction, float type, float top)
{
    float within = heightFraction / max(top, 0.05);
    if (within > 1.0) {
        return 0.0;
    }
    float sheet = smoothstep(0.0, 0.10, within) * (1.0 - smoothstep(0.30, 0.80, within));
    float tower = smoothstep(0.0, 0.16, within) * (1.0 - smoothstep(0.50, 1.0, within));
    return clamp(mix(sheet, tower, clamp(type, 0.0, 1.0)), 0.0, 1.0);
}

/**
 * Density of the layer at one world point, 0 to 1.
 *
 * @param octaves How many octaves the base shape is summed over. The light
 *        march and the skip test ask for fewer: one integrates an optical
 *        depth and the other only asks whether there is anything here at all,
 *        and detail neither can see is detail neither should pay for.
 * @param erode   Whether the fractal boundary is carved.
 */
float CloudDensity(vec3 position, float heightFraction, vec2 wind, CloudWeather weather,
                   int octaves, bool erode)
{
    if (weather.coverage <= 0.0) {
        return 0.0;
    }
    float profile = CloudHeightProfile(heightFraction, weather.type, weather.top);
    if (profile <= 0.0) {
        return 0.0;
    }
    // The shape cell is the weather cell's own size scaled down: a cloud is a
    // feature inside a weather system, so tying the two keeps their ratio fixed
    // when a scene retunes the weather and stops the shapes from becoming a
    // second, finer sky of their own.
    float shapeScale = max(frame.cloud.w * 0.19, 1.0);
    vec3 drifted = position + vec3(wind.x, 0.0, wind.y);
    vec3 chart = vec3(drifted.x, drifted.y * kCloudVerticalDetail, drifted.z) / shapeScale;
    float base = CloudFbm3(chart, octaves);

    // Thresholded against where the noise actually lives rather than against
    // 1 - coverage. A fractal sum is packed around its mean, so a linear
    // threshold spends most of its travel on values the noise never reaches:
    // the cloud thins to filaments and the coverage control goes dead over most
    // of its range. These two ends are the span the sum does occupy.
    float threshold = mix(0.60, 0.18, weather.coverage);
    float density = smoothstep(threshold, threshold + 0.16, base * profile);
    if (density <= 0.0) {
        return 0.0;
    }

    if (erode && frame.cloudShape.z > 0.0) {
        float detailScale = max(frame.cloudShape.y, 1.0);
        // Drifts faster than the base and along a different heading, so the
        // billows boil as the bank moves rather than sliding with it rigidly.
        vec3 detailDrift = position + vec3(wind.y, -wind.x, wind.x) * 1.7;
        vec3 detailChart =
            vec3(detailDrift.x, detailDrift.y * kCloudVerticalDetail, detailDrift.z) / detailScale;
        float detail = CloudFbm3(detailChart, 3);
        // Weighted toward the rim. Carving the core as hard as the boundary
        // hollows a cumulus out from the inside; what erosion is for is the
        // fractal edge, which is where the density is already on its way down.
        float rim = 1.0 - smoothstep(0.10, 0.72, density);
        float carve = (1.0 - detail) * clamp(frame.cloudShape.z, 0.0, 1.0) * rim;
        density = clamp((density - carve) / max(1.0 - carve, 0.001), 0.0, 1.0);
    }
    return density;
}

/**
 * Henyey-Greenstein, normalized so that isotropic scattering is exactly 1.
 *
 * The physical phase function carries a 1/4pi, which makes every value it
 * returns a small fraction and pushes that factor into whatever gain is
 * multiplied by it. Dividing it out here leaves a number that reads as what it
 * is -- how much more light goes this way than would go any way at all -- so
 * cloudLightGain means "times the sun" rather than "times the sun over 4pi".
 */
float CloudPhaseHg(float cosTheta, float anisotropy)
{
    float g = clamp(anisotropy, -0.95, 0.95);
    float g2 = g * g;
    float denominator = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / pow(max(denominator, 0.0001), 1.5);
}

/** Forward lobe for the silver lining plus a backward one for the far side. */
float CloudPhase(float cosTheta, float width)
{
    return mix(CloudPhaseHg(cosTheta, 0.80 * width), CloudPhaseHg(cosTheta, -0.30 * width), 0.36);
}

/**
 * Optical depth from a sample toward the sun.
 *
 * The strides grow as they go. A cloud a long way along the sun ray only ever
 * contributes a bulk optical depth -- its own shape has been averaged out by
 * everything in front of it -- so spending uniform steps out there buys nothing
 * the first two steps have not already bought.
 */
float CloudSunDepth(vec3 position, vec3 toSun, vec2 wind, CloudWeather weather, float floorY,
                    float thickness, uint steps)
{
    if (steps == 0u) {
        return 0.0;
    }
    // Sized so that the whole walk covers about half the layer's depth however
    // many steps it is given. Self-shadowing is a question about the cloud the
    // sample is standing in, and a walk that keeps going past it is answering a
    // different one: at a reach of two layer depths the integral runs through
    // every cloud between here and the sun, the optical depth comes back in the
    // tens, and the sun is switched off inside a cloud it should be lighting.
    float stride = thickness * 0.02;
    float depth = 0.0;
    vec3 walk = position;
    for (uint index = 0u; index < steps; ++index) {
        walk += toSun * stride;
        float heightFraction = (walk.y - floorY) / thickness;
        if (heightFraction > 1.0) {
            break;
        }
        if (heightFraction >= 0.0) {
            depth += CloudDensity(walk, heightFraction, wind, weather, 2, false) * stride;
        }
        stride *= 1.55;
    }
    return depth;
}

/**
 * How much sunlight reaches a sample, including the light that got there by
 * bouncing.
 *
 * Beer's law alone is the reason a single-scatter cloud renders as a dark grey
 * lump: it accounts only for the photons that crossed the cloud without ever
 * being deflected, and in a medium whose scattering albedo is essentially one
 * those are a small minority of the photons that actually arrive. The rest
 * arrive diffusely, having bounced, and they are most of a real cloud's
 * brightness.
 *
 * Summed here as a few "scattering octaves": successive terms attenuate more
 * slowly, scatter more evenly and contribute less, which is what multiply
 * scattered light does without solving for it. Three terms is where the sum
 * stops changing the picture.
 */
vec3 CloudSunEnergy(float sunDepth, float extinction, float cosTheta, vec3 sunColor)
{
    vec3 energy = vec3(0.0);
    float contribution = 1.0;
    float attenuation = 1.0;
    float width = 1.0;
    for (int octave = 0; octave < 4; ++octave) {
        energy += sunColor * contribution * CloudPhase(cosTheta, width) *
                  exp(-sunDepth * extinction * attenuation);
        contribution *= 0.62;
        // Falls away far faster than the contribution does, and that asymmetry
        // is the whole point. Light that has bounced several times no longer
        // travelled the straight path the optical depth was measured along, so
        // the depth it actually crossed is a fraction of it -- which is why a
        // thick cloud is white rather than black, and why an octave sum whose
        // attenuations decay gently stays as dark as the single-scatter term it
        // was meant to rescue.
        attenuation *= 0.34;
        width *= 0.5;
    }
    return energy;
}

/**
 * Marches the layer along one ray.
 *
 * @param origin    World-space ray origin.
 * @param direction Normalized ray direction.
 * @param toSun     Unit vector toward the sun.
 * @param sunColor  Sun radiance in the units the sky is drawn in.
 * @param skyColor  What the sky around the cloud is worth, which is what lights
 *                  the parts of it the sun does not reach.
 * @param jitter    0..1 offset of the first step, trading the march's banding
 *                  for noise the eye reads as grain instead of as rings.
 * @param quality   Fraction of the authored step budget to spend, 0..1. A ray
 *                  spawned by a reflection asks for less: whatever it returns
 *                  is about to be weighted by a Fresnel term and blurred into
 *                  a surface, and the sky is most of what a reflective scene
 *                  reflects, so paying full price for it twice is most of the
 *                  layer's cost for none of its visible detail.
 * @return rgb what the layer scattered toward the eye, a the transmittance
 *         through it -- so a caller composites with `sky * a + rgb`.
 */
vec4 CloudLayer(vec3 origin, vec3 direction, vec3 toSun, vec3 sunColor, vec3 skyColor,
                float jitter, float quality)
{
    vec4 clear = vec4(0.0, 0.0, 0.0, 1.0);
    // Floored at four: below that the march cannot resolve a cloud's near face
    // at all, and a reflection showing a different *shape* of cloud than the
    // sky above it is worse than one showing a blurrier version of the same.
    uint fineSteps = uint(max(frame.cloudDetail.x * clamp(quality, 0.05, 1.0), 4.0));
    if (frame.cloud.x <= 0.0 || frame.cloud.y <= 0.0 || fineSteps == 0u) {
        return clear;
    }
    float thickness = max(frame.cloudShape.x, 1.0);
    float floorY = frame.cloud.z;
    float ceilY = floorY + thickness;

    // A ray running along the slab has no entry or exit to solve for, and the
    // reciprocal below would be an infinity. There is no cloud worth drawing
    // dead level either, so it is simply excluded.
    if (abs(direction.y) < 0.0001) {
        return clear;
    }
    float toFloor = (floorY - origin.y) / direction.y;
    float toCeil = (ceilY - origin.y) / direction.y;
    float enter = max(min(toFloor, toCeil), 0.0);
    float exitAt = max(toFloor, toCeil);
    if (exitAt <= enter) {
        return clear;
    }
    exitAt = min(exitAt, enter + thickness * kCloudMaxMarch);

    // Nothing survives the air at the horizon, and the slab is least defensible
    // exactly there: a flat layer would otherwise run to a hard line where the
    // march bound cuts it off. The fade retires it before that line is reached.
    float horizon = smoothstep(0.012, 0.15, direction.y);
    if (horizon <= 0.0) {
        return clear;
    }
    // How much of the layer survives the air between here and where the ray
    // entered it. Read at the entry rather than per sample: the depth of one
    // cloud is a rounding error next to the distance to it, so a single
    // attenuation for the whole body is both cheaper and no less correct.
    float fadeStart = thickness * kCloudFadeStart;
    float fadeEnd = thickness * kCloudMaxMarch;
    float reach = 1.0 - smoothstep(fadeStart, fadeEnd, enter);
    float visibility = horizon * reach;
    if (visibility <= 0.002) {
        return clear;
    }

    vec2 wind = vec2(1.0, 0.32) * frame.cloudDetail.y * frame.cloudDetail.z;
    uint lightSteps = uint(max(frame.cloudMarch.y * clamp(quality, 0.05, 1.0), 2.0));
    float extinction = max(frame.cloud.y, 0.0);
    float lightGain = max(frame.cloudDetail.w, 0.0);
    float ambientGain = max(frame.cloudMarch.z, 0.0);
    float cosTheta = dot(direction, toSun);

    // An absolute length, not a division of some interval this pixel happened
    // to find. Two neighbouring rays therefore sample the same cloud at the
    // same rate however differently their skip phases played out, which is what
    // keeps the layer from tiling.
    float baseStride = thickness / float(fineSteps) * 1.4;
    // Enough iterations to skip a long slant and still march the cloud at the
    // end of it, and a hard ceiling so a grazing ray cannot cost more than a
    // vertical one by an unbounded factor.
    uint maxIterations = fineSteps * 3u + 24u;

    vec3 scattered = vec3(0.0);
    float transmittance = 1.0;
    float t = enter;
    bool inside = false;
    bool jittered = false;
    int emptyRun = 0;

    for (uint index = 0u; index < maxIterations && t < exitAt; ++index) {
        if (transmittance < kCloudMinTransmittance) {
            break;
        }
        vec3 probe = origin + direction * t;
        float heightFraction = (probe.y - floorY) / thickness;
        CloudWeather weather = SampleCloudWeather(probe.xz, wind);
        // Steps lengthen with distance, because what a step has to resolve is
        // not a length in the world but a feature on the screen, and a cloud
        // far down the ray puts the same billow across a fraction of the pixels
        // one overhead does.
        //
        // Without this the cost of the layer is proportional to how far the ray
        // travels through it -- which goes as 1/sin(elevation) -- so looking
        // level cost three or four times what looking up cost, and panning the
        // camera between the two read as the frame rate collapsing and
        // recovering.
        //
        // Held at full rate through the near field, and only then allowed to
        // grow. Scaling the stride from the very first step coarsens the clouds
        // a viewer is actually looking at, which trades the whole quality of
        // the layer for frames it did not need: the saving is in the long tail,
        // and the long tail is the part the distance fade above is already
        // busy retiring.
        float lod = 1.0 + max(t - thickness * 2.2, 0.0) / (thickness * 3.0);
        float fineStride = baseStride * lod;
        float coarseStride = fineStride * kCloudSkipRatio;

        if (!inside) {
            // Cheap question while skipping: is there anything here at all.
            if (CloudDensity(probe, heightFraction, wind, weather, 2, false) > 0.0) {
                // Stepped back to just before the boundary the long stride
                // jumped over, so the cloud's near face is marched at the fine
                // rate rather than started from wherever the skip landed.
                inside = true;
                emptyRun = 0;
                t = max(t - coarseStride, enter);
                // Offset once, on entry, and by a fine stride rather than a
                // coarse one. The offset exists to keep neighbouring pixels
                // from sharing step boundaries, and a fine stride is the
                // spacing those boundaries actually have; jittering by the
                // skip stride instead spreads the entry point over five times
                // the distance, and at a thin cloud edge -- where a stride
                // either lands in cloud or misses it entirely -- that reads as
                // salt-and-pepper speckle along every rim.
                if (!jittered) {
                    jittered = true;
                    t += jitter * fineStride;
                }
                continue;
            }
            t += coarseStride;
            continue;
        }

        float density = CloudDensity(probe, heightFraction, wind, weather, 4, true);
        if (density <= 0.0) {
            // One empty sample is a gap between billows, not the end of the
            // cloud. Leaving fine mode too eagerly steps straight back over the
            // next billow at the long stride and eats its near face.
            if (++emptyRun > kCloudEmptyRun) {
                inside = false;
            }
            t += fineStride;
            continue;
        }
        emptyRun = 0;

        float sigma = density * extinction;
        // The light march is by far the most expensive thing here, and a wisp
        // at the edge of a cloud neither casts a shadow worth resolving nor
        // receives one: below this the sample is lit as if the sun reached it,
        // which is very nearly true of something that thin.
        float sunDepth = density > 0.04 ? CloudSunDepth(probe, toSun, wind, weather, floorY,
                                                        thickness, lightSteps)
                                        : 0.0;
        vec3 direct = CloudSunEnergy(sunDepth, extinction, cosTheta, sunColor) * lightGain;
        // Powder: the face of a cloud turned toward the sun is darker than its
        // rim, because a photon arriving there has had no depth to scatter out
        // of. Beer alone makes that face the brightest part of the cloud, which
        // is the single most recognisable way a cloud renders wrong.
        direct *= mix(0.45, 1.0, 1.0 - exp(-density * 4.0));
        // The sky lighting the cloud back. Weighted to the top of the layer,
        // which is the part with sky above it, and to the parts the sun has
        // already been shown not to reach.
        vec3 fill = skyColor * ambientGain * mix(0.30, 1.0, clamp(heightFraction, 0.0, 1.0));
        vec3 luminance = direct + fill;

        float stepTransmittance = exp(-sigma * fineStride);
        // Integrated across the step rather than sampled at its midpoint. A
        // cloud's own extinction over one step is not small, so the difference
        // between the two is a visible ladder through every soft edge.
        scattered += transmittance * luminance * (1.0 - stepTransmittance);
        transmittance *= stepTransmittance;
        t += fineStride;
    }

    float visible = visibility * (1.0 - transmittance);
    return vec4(scattered * visibility, 1.0 - visible);
}

#endif
