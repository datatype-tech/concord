// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/audio/AudioClip.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Concord {
namespace {

/** Mix rate every preset renders at; matches the mixer's own rate. */
constexpr f32 kPresetRate = 48000.0f;

/** Crossfade hiding the loop point, in seconds. Covers slow swells too. */
constexpr f32 kLoopFadeSeconds = 0.6f;

/** Peak every loop normalizes to, leaving headroom for spatial gain. */
constexpr f32 kLoopPeak = 0.5f;

constexpr f32 kTwoPi = 6.28318530718f;

/** Deterministic white noise in -1..1; the same seed replays identically. */
f32 White(u32& seed) noexcept
{
    seed = seed * 1664525u + 1013904223u;
    return static_cast<f32>(seed >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

/** Uniform in 0..1 from the same generator. */
f32 Unit(u32& seed) noexcept
{
    return White(seed) * 0.5f + 0.5f;
}

/** One-pole lowpass step. */
f32 Lowpass(f32 input, f32& state, f32 coefficient) noexcept
{
    state += coefficient * (input - state);
    return state;
}

/**
 * Folds the tail over the head with a raised-cosine crossfade so the loop
 * point is inaudible whatever the content does. The stream is generated
 * fadeFrames longer than the loop; each head frame blends from the matching
 * tail frame into itself, so the last loop frame hands off to samples
 * generated right after it rather than to samples generated seconds earlier.
 */
void HideLoopSeam(std::vector<f32>& loop) noexcept
{
    const u32 frames = static_cast<u32>(loop.size());
    u32 fade = static_cast<u32>(kLoopFadeSeconds * kPresetRate + 0.5f);
    fade = std::min(fade, frames / 2u);
    if (fade < 2u) {
        return;
    }
    std::vector<f32> tail(loop.end() - fade, loop.end());
    loop.resize(frames - fade);
    for (u32 index = 0; index < fade; ++index) {
        const f32 weight = 0.5f - 0.5f * std::cos(kTwoPi * static_cast<f32>(index) /
                                                  static_cast<f32>(fade));
        loop[index] = tail[index] * (1.0f - weight) + loop[index] * weight;
    }
}

/** Length of the crossfade hiding the loop point, in frames. */
u32 LoopFadeFrames() noexcept
{
    return static_cast<u32>(kLoopFadeSeconds * kPresetRate + 0.5f);
}

/**
 * Keeps a transient out of the crossfade zone: one landing inside it would be
 * doubled across the seam instead of hidden by it.
 */
u32 PlaceTransient(u32& seed, u32 loopFrames, u32 fade) noexcept
{
    u32 at = static_cast<u32>(Unit(seed) * static_cast<f32>(loopFrames - 1));
    if (at < fade) {
        at += fade;
    } else if (at >= loopFrames - fade) {
        at -= fade;
    }
    return at;
}

/** Scales the loop to the preset peak so voices sum without clipping. */
void NormalizeLoop(std::vector<f32>& loop) noexcept
{
    f32 peak = 1.0e-6f;
    for (f32 sample : loop) {
        peak = std::max(peak, std::fabs(sample));
    }
    const f32 gain = kLoopPeak / peak;
    for (f32& sample : loop) {
        sample *= gain;
    }
}

u32 LoopFrames(f32 seconds, f32& clamped) noexcept
{
    clamped = std::clamp(seconds, 1.0f, 12.0f);
    if (!std::isfinite(clamped)) {
        clamped = 4.0f;
    }
    const u32 fade = static_cast<u32>(kLoopFadeSeconds * kPresetRate + 0.5f);
    return static_cast<u32>(clamped * kPresetRate + 0.5f) + fade;
}

} // namespace

std::shared_ptr<AudioClip> AudioClip::WaterfallLoop(f32 seconds)
{
    f32 clamped = 4.0f;
    const u32 frames = LoopFrames(seconds, clamped);
    std::vector<f32> loop(frames, 0.0f);
    u32 seed = 0xA7E8FA11u;
    f32 brown = 0.0f;
    f32 hissLow = 0.0f;
    f32 hissHigh = 0.0f;
    f32 swellPhase = 0.0f;
    for (u32 index = 0; index < frames; ++index) {
        const f32 noise = White(seed);
        // Low rumble bed: a leaky integrator, tamed so it cannot wander.
        brown = (brown + 0.018f * noise) / 1.018f;
        // Mid hiss band: the difference of two lowpasses.
        const f32 low = Lowpass(noise, hissLow, 0.30f);
        const f32 deep = Lowpass(noise, hissHigh, 0.035f);
        // Slow breathing the ear reads as moving water rather than static.
        swellPhase += kTwoPi * 0.11f / kPresetRate;
        const f32 swell = 0.82f + 0.18f * std::sin(swellPhase + 0.6f * std::sin(swellPhase * 0.37f));
        loop[index] = (brown * 2.4f + (low - deep) * 0.55f) * swell;
    }
    // Sparse droplet transients on top of the bed.
    u32 dropSeed = 0xD2000001u;
    const u32 drops = static_cast<u32>(clamped * 13.0f);
    const u32 fade = LoopFadeFrames();
    for (u32 drop = 0; drop < drops; ++drop) {
        const u32 at = PlaceTransient(dropSeed, frames, fade);
        const f32 frequency = 320.0f + Unit(dropSeed) * 520.0f;
        const f32 decay = 16.0f + Unit(dropSeed) * 16.0f;
        const f32 amplitude = 0.10f + Unit(dropSeed) * 0.14f;
        const f32 phase = Unit(dropSeed) * kTwoPi;
        const u32 length = static_cast<u32>(kPresetRate * 0.25f);
        for (u32 tap = 0; tap < length && at + tap < frames; ++tap) {
            const f32 tapTime = static_cast<f32>(tap) / kPresetRate;
            // Fast attack: a sine starting at nonzero phase is a step, and a
            // step is a click. Three milliseconds in is inaudible as a fade.
            const f32 attack = std::min(tapTime / 0.003f, 1.0f);
            const f32 envelope = std::exp(-tapTime * decay) * attack;
            loop[at + tap] += amplitude * envelope *
                              std::sin(kTwoPi * frequency * tapTime + phase);
        }
    }
    HideLoopSeam(loop);
    NormalizeLoop(loop);
    return Adopt(std::move(loop));
}

std::shared_ptr<AudioClip> AudioClip::WindLoop(f32 seconds)
{
    f32 clamped = 6.0f;
    const u32 frames = LoopFrames(seconds, clamped);
    std::vector<f32> loop(frames, 0.0f);
    u32 seed = 0xB1000001u;
    f32 bed = 0.0f;
    f32 airLow = 0.0f;
    f32 whistlePhase = 0.0f;
    for (u32 index = 0; index < frames; ++index) {
        const f32 time = static_cast<f32>(index) / kPresetRate;
        const f32 noise = White(seed);
        // Deep whoosh bed plus a breathy band above it.
        const f32 low = Lowpass(noise, bed, 0.07f);
        const f32 band = Lowpass(noise, airLow, 0.35f) - low;
        // Two slow swells a fifth apart: the gusting never sits still.
        const f32 swell = 0.60f + 0.28f * std::sin(kTwoPi * 0.07f * time) +
                          0.12f * std::sin(kTwoPi * 0.13f * time + 2.0f);
        // Faint whistle riding the gusts, kept far under the bed.
        const f32 whistleFrequency = 520.0f + 70.0f * std::sin(kTwoPi * 0.09f * time + 1.0f);
        whistlePhase += kTwoPi * whistleFrequency / kPresetRate;
        const f32 whistle = std::sin(whistlePhase) * 0.035f * std::max(swell - 0.55f, 0.0f);
        loop[index] = (low * 2.6f + band * 0.9f) * swell + whistle;
    }
    HideLoopSeam(loop);
    NormalizeLoop(loop);
    return Adopt(std::move(loop));
}

std::shared_ptr<AudioClip> AudioClip::FireLoop(f32 seconds)
{
    f32 clamped = 3.0f;
    const u32 frames = LoopFrames(seconds, clamped);
    std::vector<f32> loop(frames, 0.0f);
    u32 seed = 0xF12E0001u;
    f32 rumble = 0.0f;
    f32 bodyLow = 0.0f;
    for (u32 index = 0; index < frames; ++index) {
        const f32 noise = White(seed);
        const f32 low = Lowpass(noise, rumble, 0.05f);
        const f32 mid = Lowpass(noise, bodyLow, 0.30f) - low;
        loop[index] = low * 2.2f + mid * 0.5f;
    }
    // Crackle pops in density-modulated bursts: fire breathes in gusts.
    u32 popSeed = 0xC2AC0001u;
    const u32 pops = static_cast<u32>(clamped * 42.0f);
    const u32 fade = LoopFadeFrames();
    for (u32 pop = 0; pop < pops; ++pop) {
        const u32 at = PlaceTransient(popSeed, frames, fade);
        const f32 atTime = static_cast<f32>(at) / kPresetRate;
        const f32 density = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(kTwoPi * 0.4f * atTime + 0.8f *
                                                                   std::sin(kTwoPi * 0.13f * atTime)));
        if (Unit(popSeed) > density) {
            continue;
        }
        const bool loud = Unit(popSeed) > 0.93f;
        const f32 frequency = loud ? 500.0f + Unit(popSeed) * 700.0f
                                   : 900.0f + Unit(popSeed) * 1900.0f;
        const f32 decay = loud ? 60.0f + Unit(popSeed) * 60.0f : 160.0f + Unit(popSeed) * 340.0f;
        const f32 amplitude = loud ? 0.30f + Unit(popSeed) * 0.20f : 0.10f + Unit(popSeed) * 0.22f;
        const f32 phase = Unit(popSeed) * kTwoPi;
        const u32 length = static_cast<u32>(kPresetRate * (loud ? 0.20f : 0.06f));
        const f32 attackTime = loud ? 0.0015f : 0.0007f;
        for (u32 tap = 0; tap < length && at + tap < frames; ++tap) {
            const f32 tapTime = static_cast<f32>(tap) / kPresetRate;
            const f32 attack = std::min(tapTime / attackTime, 1.0f);
            loop[at + tap] += amplitude * std::exp(-tapTime * decay) * attack *
                              std::sin(kTwoPi * frequency * tapTime + phase);
        }
    }
    HideLoopSeam(loop);
    NormalizeLoop(loop);
    return Adopt(std::move(loop));
}

} // namespace Concord
