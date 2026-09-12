// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/audio/AudioClip.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Concord {
namespace {

constexpr u32 kMixRate = 48000;

struct WavHeader {
    char riff[4]{};
    u32 riffSize = 0;
    char wave[4]{};
};

bool ReadExact(std::ifstream& file, void* dest, std::size_t bytes)
{
    file.read(static_cast<char*>(dest), static_cast<std::streamsize>(bytes));
    return file.good();
}

void FoldAndResample(const f32* interleaved, u32 frames, u32 channels, u32 sourceRate,
                     std::vector<f32>& mono)
{
    const u32 ch = channels == 0 ? 1 : channels;
    std::vector<f32> folded(frames);
    for (u32 frame = 0; frame < frames; ++frame) {
        f32 sum = 0.0f;
        for (u32 c = 0; c < ch; ++c) {
            sum += interleaved[frame * ch + c];
        }
        folded[frame] = sum / static_cast<f32>(ch);
    }
    if (sourceRate == 0 || sourceRate == kMixRate) {
        mono = std::move(folded);
        return;
    }
    const f64 ratio = static_cast<f64>(kMixRate) / static_cast<f64>(sourceRate);
    const u32 outFrames =
        static_cast<u32>(std::max<long long>(1, std::llround(frames * ratio)));
    mono.resize(outFrames);
    for (u32 i = 0; i < outFrames; ++i) {
        const f64 src = static_cast<f64>(i) * static_cast<f64>(sourceRate) /
                        static_cast<f64>(kMixRate);
        const u32 a = static_cast<u32>(src);
        const u32 b = std::min(a + 1, frames > 0 ? frames - 1 : 0);
        const f32 t = static_cast<f32>(src - static_cast<f64>(a));
        const f32 left = a < frames ? folded[a] : 0.0f;
        const f32 right = b < frames ? folded[b] : left;
        mono[i] = left + (right - left) * t;
    }
}

} // namespace

struct AudioClip::Impl {
    std::vector<f32> samples;
};

std::shared_ptr<AudioClip> AudioClip::Adopt(std::vector<f32> samples)
{
    auto clip = std::make_shared<AudioClip>();
    clip->m_impl = std::make_shared<Impl>();
    clip->m_impl->samples = std::move(samples);
    return clip;
}

std::shared_ptr<AudioClip> AudioClip::FromPcm16(std::span<const i16> interleaved, u32 channels,
                                                u32 sampleRate)
{
    if (interleaved.empty() || channels == 0) {
        return std::make_shared<AudioClip>();
    }
    const u32 frames = static_cast<u32>(interleaved.size() / channels);
    std::vector<f32> floats(static_cast<std::size_t>(frames) * channels);
    for (std::size_t i = 0; i < floats.size(); ++i) {
        floats[i] = static_cast<f32>(interleaved[i]) / 32768.0f;
    }
    std::vector<f32> mono;
    FoldAndResample(floats.data(), frames, channels, sampleRate, mono);
    return Adopt(std::move(mono));
}

std::shared_ptr<AudioClip> AudioClip::Load(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return std::make_shared<AudioClip>();
    }

    static std::mutex cacheMutex;
    static std::unordered_map<std::string, std::shared_ptr<AudioClip>> cache;
    {
        std::lock_guard<std::mutex> guard(cacheMutex);
        const auto found = cache.find(path);
        if (found != cache.end()) {
            return found->second;
        }
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::make_shared<AudioClip>();
    }

    WavHeader header{};
    if (!ReadExact(file, &header, sizeof(header)) || std::memcmp(header.riff, "RIFF", 4) != 0 ||
        std::memcmp(header.wave, "WAVE", 4) != 0) {
        return std::make_shared<AudioClip>();
    }

    u16 format = 0;
    u16 channels = 0;
    u32 sampleRate = 0;
    u16 bits = 0;
    std::vector<u8> data;
    while (file) {
        char id[4]{};
        u32 size = 0;
        if (!ReadExact(file, id, 4) || !ReadExact(file, &size, 4)) {
            break;
        }
        if (std::memcmp(id, "fmt ", 4) == 0) {
            if (size < 16) {
                return std::make_shared<AudioClip>();
            }
            u16 blockAlign = 0;
            u32 byteRate = 0;
            if (!ReadExact(file, &format, 2) || !ReadExact(file, &channels, 2) ||
                !ReadExact(file, &sampleRate, 4) || !ReadExact(file, &byteRate, 4) ||
                !ReadExact(file, &blockAlign, 2) || !ReadExact(file, &bits, 2)) {
                return std::make_shared<AudioClip>();
            }
            if (size > 16) {
                file.seekg(static_cast<std::streamoff>(size - 16), std::ios::cur);
            }
        } else if (std::memcmp(id, "data", 4) == 0) {
            data.resize(size);
            if (size > 0 && !ReadExact(file, data.data(), size)) {
                return std::make_shared<AudioClip>();
            }
            break;
        } else {
            file.seekg(static_cast<std::streamoff>(size), std::ios::cur);
        }
    }

    if (channels == 0 || sampleRate == 0 || data.empty()) {
        return std::make_shared<AudioClip>();
    }

    std::vector<f32> floats;
    if (format == 1 && bits == 16) {
        const u32 samples = static_cast<u32>(data.size() / 2);
        floats.resize(samples);
        for (u32 i = 0; i < samples; ++i) {
            i16 value = 0;
            std::memcpy(&value, data.data() + i * 2, 2);
            floats[i] = static_cast<f32>(value) / 32768.0f;
        }
    } else if (format == 3 && bits == 32) {
        const u32 samples = static_cast<u32>(data.size() / 4);
        floats.resize(samples);
        std::memcpy(floats.data(), data.data(), samples * 4);
    } else {
        return std::make_shared<AudioClip>();
    }

    const u32 frames = static_cast<u32>(floats.size() / channels);
    std::vector<f32> mono;
    FoldAndResample(floats.data(), frames, channels, sampleRate, mono);
    std::shared_ptr<AudioClip> clip = Adopt(std::move(mono));
    std::lock_guard<std::mutex> guard(cacheMutex);
    cache[path] = clip;
    return clip;
}

bool AudioClip::IsValid() const noexcept
{
    return m_impl && !m_impl->samples.empty();
}

u32 AudioClip::FrameCount() const noexcept
{
    return m_impl ? static_cast<u32>(m_impl->samples.size()) : 0;
}

u32 AudioClip::SampleRate() const noexcept
{
    return kMixRate;
}

std::span<const f32> AudioClip::Samples() const noexcept
{
    if (!m_impl) {
        return {};
    }
    return m_impl->samples;
}

} // namespace Concord
