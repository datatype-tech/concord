// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <Concord/CAudio.h>
#include <Concord/CCamera.h>
#include <Concord/CScene.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr Concord::u32 kRate = 48000;

std::shared_ptr<Concord::AudioClip> MakeSine(float seconds, float hertz)
{
    const Concord::u32 frames = static_cast<Concord::u32>(seconds * static_cast<float>(kRate));
    std::vector<Concord::i16> pcm(frames);
    for (Concord::u32 index = 0; index < frames; ++index) {
        const float phase = static_cast<float>(index) * hertz * 6.28318530718f /
                            static_cast<float>(kRate);
        pcm[index] = static_cast<Concord::i16>(std::sin(phase) * 24000.0f);
    }
    return Concord::AudioClip::FromPcm16(pcm, 1, kRate);
}

void ChannelRms(const std::vector<float>& interleaved, float& left, float& right)
{
    double leftSum = 0.0;
    double rightSum = 0.0;
    const Concord::u32 frames = static_cast<Concord::u32>(interleaved.size() / 2);
    for (Concord::u32 index = 0; index < frames; ++index) {
        const double l = interleaved[index * 2];
        const double r = interleaved[index * 2 + 1];
        leftSum += l * l;
        rightSum += r * r;
    }
    const double count = frames == 0 ? 1.0 : static_cast<double>(frames);
    left = static_cast<float>(std::sqrt(leftSum / count));
    right = static_cast<float>(std::sqrt(rightSum / count));
}

bool WriteWav(const std::filesystem::path& path, const std::vector<Concord::i16>& pcm)
{
    const Concord::u32 dataBytes = static_cast<Concord::u32>(pcm.size() * 2);
    const Concord::u32 riffSize = 36 + dataBytes;
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&riffSize), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    const Concord::u32 fmtSize = 16;
    const Concord::u16 format = 1;
    const Concord::u16 channels = 1;
    const Concord::u32 rate = kRate;
    const Concord::u16 bits = 16;
    const Concord::u16 blockAlign = 2;
    const Concord::u32 byteRate = kRate * 2;
    file.write(reinterpret_cast<const char*>(&fmtSize), 4);
    file.write(reinterpret_cast<const char*>(&format), 2);
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&rate), 4);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    file.write(reinterpret_cast<const char*>(&bits), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&dataBytes), 4);
    file.write(reinterpret_cast<const char*>(pcm.data()), dataBytes);
    return static_cast<bool>(file);
}

bool TestSourceOnRightIsLouderOnRight()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Listener>({});
    scene.Spawn<Concord::Object::Sound>({
        .transform = {.position = {4.0f, 0.0f, 0.0f}},
        .clip = MakeSine(0.25f, 440.0f),
        .spatial = true,
        .loop = true,
    });
    Concord::AudioSystem audio({.playOnDevice = false});
    audio.OnStart(scene);
    audio.OnUpdate(scene, 0.0f);
    std::vector<float> buffer(256 * 2);
    audio.World().MixStereo(buffer.data(), 256);
    float left = 0.0f;
    float right = 0.0f;
    ChannelRms(buffer, left, right);
    audio.OnStop(scene);
    return right > left * 1.8f && right > 0.02f;
}

bool TestSourceOnLeftIsLouderOnLeft()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Listener>({});
    scene.Spawn<Concord::Object::Sound>({
        .transform = {.position = {-4.0f, 0.0f, 0.0f}},
        .clip = MakeSine(0.25f, 440.0f),
        .spatial = true,
        .loop = true,
    });
    Concord::AudioSystem audio({.playOnDevice = false});
    audio.OnStart(scene);
    audio.OnUpdate(scene, 0.0f);
    std::vector<float> buffer(256 * 2);
    audio.World().MixStereo(buffer.data(), 256);
    float left = 0.0f;
    float right = 0.0f;
    ChannelRms(buffer, left, right);
    audio.OnStop(scene);
    return left > right * 1.8f && left > 0.02f;
}

bool TestStopSilencesVoice()
{
    Concord::Scene scene;
    const Concord::EntityHandle voice = scene.Spawn<Concord::Object::Sound>({
        .transform = {.position = {0.0f, 0.0f, -1.0f}},
        .clip = MakeSine(0.25f, 330.0f),
        .spatial = false,
        .loop = true,
    });
    Concord::AudioSystem audio({.playOnDevice = false});
    audio.OnStart(scene);
    audio.OnUpdate(scene, 0.0f);
    std::vector<float> playing(128 * 2);
    audio.World().MixStereo(playing.data(), 128);
    float left = 0.0f;
    float right = 0.0f;
    ChannelRms(playing, left, right);
    const bool hadEnergy = left + right > 0.02f;
    const bool stopped = audio.Stop(voice.Id());
    std::vector<float> silent(128 * 2);
    audio.World().MixStereo(silent.data(), 128);
    float quietLeft = 0.0f;
    float quietRight = 0.0f;
    ChannelRms(silent, quietLeft, quietRight);
    audio.OnStop(scene);
    return hadEnergy && stopped && quietLeft + quietRight < 1.0e-4f;
}

bool TestFindReturnsBoundWorld()
{
    Concord::Scene scene;
    Concord::AudioSystem audio({.playOnDevice = false});
    audio.OnStart(scene);
    Concord::AudioWorld* found = Concord::AudioWorld::Find(scene);
    audio.OnStop(scene);
    return found == &audio.World() && Concord::AudioWorld::Find(scene) == nullptr;
}

bool TestCameraFallbackListens()
{
    Concord::Scene scene;
    scene.Spawn<Concord::Object::Camera>({.position = {0.0f, 0.0f, 0.0f},
                                         .rotation = {0.0f, 0.0f, 0.0f}});
    scene.Spawn<Concord::Object::Sound>({
        .transform = {.position = {4.0f, 0.0f, 0.0f}},
        .clip = MakeSine(0.25f, 440.0f),
        .spatial = true,
        .loop = true,
    });
    Concord::AudioSystem audio({.playOnDevice = false});
    audio.OnStart(scene);
    audio.OnUpdate(scene, 0.0f);
    std::vector<float> buffer(256 * 2);
    audio.World().MixStereo(buffer.data(), 256);
    float left = 0.0f;
    float right = 0.0f;
    ChannelRms(buffer, left, right);
    audio.OnStop(scene);
    return right > left * 1.8f && right > 0.02f;
}

bool TestWavRoundTrip()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "concord_audio_clip_test.wav";
    std::vector<Concord::i16> pcm(kRate / 10);
    for (Concord::u32 index = 0; index < pcm.size(); ++index) {
        pcm[index] = static_cast<Concord::i16>((index % 64) * 400);
    }
    if (!WriteWav(path, pcm)) {
        return false;
    }
    const std::shared_ptr<Concord::AudioClip> clip = Concord::AudioClip::Load(path.string().c_str());
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return clip && clip->IsValid() && clip->SampleRate() == kRate && clip->FrameCount() > 100;
}

bool TestPresetLoopsAreValidAndSeamless()
{
    const std::shared_ptr<Concord::AudioClip> waterfall = Concord::AudioClip::WaterfallLoop();
    const std::shared_ptr<Concord::AudioClip> wind = Concord::AudioClip::WindLoop();
    const std::shared_ptr<Concord::AudioClip> fire = Concord::AudioClip::FireLoop();
    if (!waterfall || !waterfall->IsValid() || !wind || !wind->IsValid() || !fire ||
        !fire->IsValid()) {
        return false;
    }
    // Default durations render at the mix rate with no resampling involved.
    if (waterfall->FrameCount() != 4u * kRate || wind->FrameCount() != 6u * kRate ||
        fire->FrameCount() != 3u * kRate) {
        return false;
    }
    const std::shared_ptr<Concord::AudioClip> clips[] = {waterfall, wind, fire};
    for (const auto& clip : clips) {
        float peak = 0.0f;
        for (float sample : clip->Samples()) {
            if (!std::isfinite(sample)) {
                return false;
            }
            peak = std::max(peak, std::fabs(sample));
        }
        // Normalized with headroom, and carrying real energy rather than hush.
        if (peak > 0.501f || peak < 0.2f) {
            return false;
        }
        const std::span<const float> samples = clip->Samples();
        // The loop point joins two adjacently generated samples inside one
        // continuous stream, so the step across it must look like the signal's
        // own sample-to-sample steps, not like an edit: compare against the
        // loop's own average step rather than an absolute threshold, because
        // noise is made of large steps and a hum is made of tiny ones.
        double stepSum = 0.0;
        for (Concord::u32 index = 0; index + 1 < samples.size(); ++index) {
            stepSum += std::fabs(samples[index + 1] - samples[index]);
        }
        const double meanStep = stepSum / static_cast<double>(samples.size() - 1);
        const double seamStep =
            std::fabs(samples.back() - samples.front());
        if (seamStep > 8.0 * meanStep + 0.005) {
            return false;
        }
    }
    return true;
}

} // namespace

int main()
{
    return TestSourceOnRightIsLouderOnRight() && TestSourceOnLeftIsLouderOnLeft() &&
                   TestStopSilencesVoice() && TestFindReturnsBoundWorld() &&
                   TestCameraFallbackListens() && TestWavRoundTrip() &&
                   TestPresetLoopsAreValidAndSeamless()
               ? 0
               : 1;
}
