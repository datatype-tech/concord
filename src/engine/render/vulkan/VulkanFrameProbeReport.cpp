// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/render/vulkan/VulkanFrameProbe.h"

#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace Concord {
namespace {

/** Expands one binary16 value; the RT output format stores nothing else. */
f32 HalfToFloat(u16 value) noexcept
{
    const u32 sign = static_cast<u32>(value >> 15) & 1u;
    const u32 exponent = static_cast<u32>(value >> 10) & 0x1fu;
    const u32 mantissa = static_cast<u32>(value) & 0x3ffu;
    u32 bits = 0;
    if (exponent == 0u) {
        if (mantissa == 0u) {
            bits = sign << 31;
        } else {
            u32 shift = mantissa;
            u32 unbiased = 127u - 15u + 1u;
            while ((shift & 0x400u) == 0u) {
                shift <<= 1;
                --unbiased;
            }
            bits = (sign << 31) | (unbiased << 23) | ((shift & 0x3ffu) << 13);
        }
    } else if (exponent == 0x1fu) {
        bits = (sign << 31) | 0x7f800000u | (mantissa << 13);
    } else {
        bits = (sign << 31) | ((exponent + 127u - 15u) << 23) | (mantissa << 13);
    }
    f32 result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

/** Mean and standard deviation of a luminance band, in row order. */
struct Band {
    f32 mean = 0.0f;
    f32 deviation = 0.0f;
};

/**
 * Summarizes a horizontal band of the frame.
 *
 * The deviation is what distinguishes a surface with structure in it from a
 * flat fill: a body of water that reads as a painted lid has a mean like any
 * other, and only the spread says whether anything is visible through it.
 */
Band SummarizeBand(const std::vector<f32>& luma, u32 width, u32 firstRow, u32 lastRow) noexcept
{
    Band band{};
    if (width == 0 || firstRow >= lastRow) {
        return band;
    }
    f64 total = 0.0;
    f64 totalSquared = 0.0;
    usize count = 0;
    for (u32 row = firstRow; row < lastRow; ++row) {
        const usize base = static_cast<usize>(row) * width;
        for (u32 column = 0; column < width; ++column) {
            const f32 value = luma[base + column];
            total += value;
            totalSquared += static_cast<f64>(value) * value;
            ++count;
        }
    }
    if (count == 0) {
        return band;
    }
    band.mean = static_cast<f32>(total / static_cast<f64>(count));
    const f32 variance =
        static_cast<f32>(totalSquared / static_cast<f64>(count)) - band.mean * band.mean;
    band.deviation = variance > 0.0f ? std::sqrt(variance) : 0.0f;
    return band;
}

/** Share of the sorted band that lies at or below a threshold. */
f32 ShareBelow(const std::vector<f32>& sorted, f32 threshold) noexcept
{
    const auto at = std::lower_bound(sorted.begin(), sorted.end(), threshold);
    return static_cast<f32>(at - sorted.begin()) / static_cast<f32>(sorted.size());
}

f32 Percentile(const std::vector<f32>& sorted, f32 fraction) noexcept
{
    const usize index = static_cast<usize>(
        fraction * static_cast<f32>(sorted.size() - 1) + 0.5f);
    return sorted[std::min(index, sorted.size() - 1)];
}

/**
 * Writes the captured frame to disk when `CONCORD_FRAME_PROBE_PNG` names a path.
 *
 * The statistics above answer whether a frame is crushed, blown or flat. They
 * cannot answer what it looks like, and a screen grab cannot be trusted to
 * answer that either: whatever the desktop puts in front of the window lands in
 * the file, and the one time it matters is the one time something is in front
 * of it. This is the only capture that sees exactly the image the renderer
 * produced and nothing else.
 *
 * The frame index is spliced in ahead of the extension rather than one path
 * being overwritten, because what is worth looking at is often transient -- a
 * splash outlives its own screenshot by less than a second -- and a sequence is
 * the only way to catch it.
 */
bool EncodeProbeImage(bool displayReferred, const u16* words, u32 width, u32 height,
                      const char* path)
{
    const usize pixels = static_cast<usize>(width) * height;
    std::vector<unsigned char> image;
    try {
        image.resize(pixels * 3u);
    } catch (...) {
        return false;
    }
    for (usize index = 0; index < pixels; ++index) {
        for (u32 channel = 0; channel < 3u; ++channel) {
            f32 value = HalfToFloat(words[index * 4u + channel]);
            // An ungraded frame is scene referred, so it has to cross the
            // display transform before it means anything as a pixel value.
            if (!displayReferred) {
                value = std::pow(std::max(value, 0.0f), 1.0f / 2.2f);
            }
            image[index * 3u + channel] =
                static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }
    return stbi_write_png(path, static_cast<int>(width), static_cast<int>(height), 3,
                          image.data(), static_cast<int>(width) * 3) != 0;
}

void WriteProbeImage(const VulkanFrameProbe& probe, const u16* words, u32 width, u32 height)
{
    const char* pattern = std::getenv("CONCORD_FRAME_PROBE_PNG");
    if (pattern == nullptr || *pattern == '\0') {
        return;
    }
    char stamp[32];
    std::snprintf(stamp, sizeof(stamp), "-%06llu",
                  static_cast<unsigned long long>(probe.captureFrame));
    std::string path(pattern);
    const usize dot = path.find_last_of('.');
    const usize separator = path.find_last_of("/\\");
    if (dot != std::string::npos && (separator == std::string::npos || dot > separator)) {
        path.insert(dot, stamp);
    } else {
        path.append(stamp).append(".png");
    }
    const bool written =
        EncodeProbeImage(probe.displayReferred, words, width, height, path.c_str());
    std::fprintf(stderr, "[probe]   %s %s\n", written ? "wrote" : "FAILED to write",
                 path.c_str());
}

} // namespace

bool WriteVulkanFrameProbeImage(const VulkanFrameProbe& probe, const char* path) noexcept
{
    const u32 width = probe.extent.width;
    const u32 height = probe.extent.height;
    const usize pixels = static_cast<usize>(width) * height;
    if (path == nullptr || *path == '\0' || pixels == 0 || !probe.staging.IsMapped() ||
        probe.staging.size < pixels * 8u) {
        std::fprintf(stderr, "[Concord] still: nothing staged to write\n");
        return false;
    }
    const bool written = EncodeProbeImage(
        probe.displayReferred, static_cast<const u16*>(probe.staging.mapped), width, height, path);
    std::fprintf(stderr, "[Concord] still: %s %s (%ux%u)\n", written ? "wrote" : "FAILED to write",
                 path, width, height);
    return written;
}

void ReportVulkanFrameProbe(VulkanFrameProbe& probe) noexcept
{
    const u32 width = probe.extent.width;
    const u32 height = probe.extent.height;
    const usize pixels = static_cast<usize>(width) * height;
    if (!probe.staging.IsMapped() || pixels == 0 ||
        probe.staging.size < pixels * 8u) {
        std::fprintf(stderr, "[probe] frame %llu: staging unavailable\n",
                     static_cast<unsigned long long>(probe.captureFrame));
        return;
    }
    const u16* words = static_cast<const u16*>(probe.staging.mapped);
    std::vector<f32> luma;
    std::vector<f32> shade;
    std::vector<f32> sorted;
    try {
        luma.resize(pixels);
        shade.resize(pixels);
        sorted.resize(pixels);
    } catch (...) {
        return;
    }
    f64 red = 0.0;
    f64 green = 0.0;
    f64 blue = 0.0;
    f32 peak = 0.0f;
    for (usize index = 0; index < pixels; ++index) {
        const f32 r = HalfToFloat(words[index * 4u]);
        const f32 g = HalfToFloat(words[index * 4u + 1u]);
        const f32 b = HalfToFloat(words[index * 4u + 2u]);
        red += r;
        green += g;
        blue += b;
        const f32 value = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        luma[index] = value;
        // The graded frame is already display referred, so its luminance is
        // what the eye receives directly. An ungraded linear frame is passed
        // through the display transform first, because that is the only form in
        // which the two are comparable.
        shade[index] = probe.displayReferred
                           ? value
                           : std::pow(std::max(value, 0.0f), 1.0f / 2.2f);
        sorted[index] = shade[index];
        peak = std::max(peak, value);
    }
    std::sort(sorted.begin(), sorted.end());
    const f32 inverse = 1.0f / static_cast<f32>(pixels);
    const Band sky = SummarizeBand(shade, width, 0, height / 7);
    const Band water = SummarizeBand(shade, width, height * 6u / 10u, height);
    // The band the water occupies, in colour rather than in luminance. A mean
    // luminance says nothing about a cast, and a cast is exactly what a wrong
    // absorption curve produces.
    f64 bandRed = 0.0;
    f64 bandGreen = 0.0;
    f64 bandBlue = 0.0;
    usize bandPixels = 0;
    for (u32 row = height * 6u / 10u; row < height; ++row) {
        const usize base = static_cast<usize>(row) * width;
        for (u32 column = 0; column < width; ++column) {
            const u16* texel = words + (base + column) * 4u;
            bandRed += HalfToFloat(texel[0]);
            bandGreen += HalfToFloat(texel[1]);
            bandBlue += HalfToFloat(texel[2]);
            ++bandPixels;
        }
    }
    const f64 bandScale = bandPixels != 0 ? 1.0 / static_cast<f64>(bandPixels) : 0.0;
    // Rate over the interval since the previous capture, in the same units the
    // frame counter uses. Reported rather than inferred: a frame time that
    // spikes is invisible in an average and is exactly what reads as stutter.
    const auto now = std::chrono::steady_clock::now();
    f64 framesPerSecond = 0.0;
    if (probe.lastReport != std::chrono::steady_clock::time_point{} &&
        now > probe.lastReport) {
        const f64 seconds = std::chrono::duration<f64>(now - probe.lastReport).count();
        if (seconds > 0.0) {
            framesPerSecond = static_cast<f64>(probe.interval) / seconds;
        }
    }
    probe.lastReport = now;
    std::fprintf(stderr,
                 "[probe] frame %llu  %ux%u  %.1f fps\n"
                 "[probe]   display luma  mean %.3f  p01 %.3f  p50 %.3f  p90 %.3f  p99 %.3f"
                 "  peak(scene) %.2f\n"
                 "[probe]   crushed %.1f%% (<0.05)  blown %.1f%% (>0.98)\n"
                 "[probe]   sky band %.3f +- %.3f   lower band %.3f +- %.3f\n"
                 "[probe]   linear channel means  r %.3f  g %.3f  b %.3f\n"
                 "[probe]   lower band colour   r %.3f  g %.3f  b %.3f\n",
                 static_cast<unsigned long long>(probe.captureFrame), width, height,
                 framesPerSecond,
                 Percentile(sorted, 0.50f), Percentile(sorted, 0.01f),
                 Percentile(sorted, 0.50f), Percentile(sorted, 0.90f),
                 Percentile(sorted, 0.99f), static_cast<f64>(peak),
                 static_cast<f64>(ShareBelow(sorted, 0.05f) * 100.0f),
                 static_cast<f64>((1.0f - ShareBelow(sorted, 0.98f)) * 100.0f),
                 static_cast<f64>(sky.mean), static_cast<f64>(sky.deviation),
                 static_cast<f64>(water.mean), static_cast<f64>(water.deviation),
                 red * inverse, green * inverse, blue * inverse, bandRed * bandScale,
                 bandGreen * bandScale, bandBlue * bandScale);
    WriteProbeImage(probe, words, width, height);
}

} // namespace Concord
