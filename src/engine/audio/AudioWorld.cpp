// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "engine/audio/AudioWorld.h"

#include "engine/core/Angle.h"
#include "engine/core/Transform.h"
#include "engine/ecs/AudioComponents.h"
#include "engine/ecs/Components.h"
#include "engine/ecs/Entity.h"
#include "engine/ecs/World.h"
#include "engine/scene/Scene.h"

#include <SDL3/SDL.h>

#include <phonon.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Concord {
namespace {

struct SteamAudioApi {
    using ContextCreate = IPLerror(IPLCALL*)(IPLContextSettings*, IPLContext*);
    using ContextRelease = void(IPLCALL*)(IPLContext*);
    using HrtfCreate = IPLerror(IPLCALL*)(IPLContext, IPLAudioSettings*, IPLHRTFSettings*, IPLHRTF*);
    using HrtfRelease = void(IPLCALL*)(IPLHRTF*);
    using BinauralCreate =
        IPLerror(IPLCALL*)(IPLContext, IPLAudioSettings*, IPLBinauralEffectSettings*,
                           IPLBinauralEffect*);
    using BinauralRelease = void(IPLCALL*)(IPLBinauralEffect*);
    using BinauralApply = IPLAudioEffectState(IPLCALL*)(IPLBinauralEffect, IPLBinauralEffectParams*,
                                                        IPLAudioBuffer*, IPLAudioBuffer*);
    using BufferAllocate = IPLerror(IPLCALL*)(IPLContext, IPLint32, IPLint32, IPLAudioBuffer*);
    using BufferFree = void(IPLCALL*)(IPLContext, IPLAudioBuffer*);
    using RelativeDirection = IPLVector3(IPLCALL*)(IPLContext, IPLVector3, IPLVector3, IPLVector3,
                                                   IPLVector3);

    HMODULE library = nullptr;
    ContextCreate contextCreate = nullptr;
    ContextRelease contextRelease = nullptr;
    HrtfCreate hrtfCreate = nullptr;
    HrtfRelease hrtfRelease = nullptr;
    BinauralCreate binauralCreate = nullptr;
    BinauralRelease binauralRelease = nullptr;
    BinauralApply binauralApply = nullptr;
    BufferAllocate bufferAllocate = nullptr;
    BufferFree bufferFree = nullptr;
    RelativeDirection relativeDirection = nullptr;

    [[nodiscard]] bool Ready() const noexcept
    {
        return contextCreate && contextRelease && hrtfCreate && hrtfRelease && binauralCreate &&
               binauralRelease && binauralApply && bufferAllocate && bufferFree;
    }
};

template <typename T>
T LoadProc(HMODULE library, const char* name)
{
    return reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(library, name)));
}

SteamAudioApi LoadSteamAudio()
{
    SteamAudioApi api{};
    api.library = LoadLibraryW(L"phonon.dll");
    if (api.library == nullptr) {
        return api;
    }
    api.contextCreate = LoadProc<SteamAudioApi::ContextCreate>(api.library, "iplContextCreate");
    api.contextRelease = LoadProc<SteamAudioApi::ContextRelease>(api.library, "iplContextRelease");
    api.hrtfCreate = LoadProc<SteamAudioApi::HrtfCreate>(api.library, "iplHRTFCreate");
    api.hrtfRelease = LoadProc<SteamAudioApi::HrtfRelease>(api.library, "iplHRTFRelease");
    api.binauralCreate = LoadProc<SteamAudioApi::BinauralCreate>(api.library, "iplBinauralEffectCreate");
    api.binauralRelease =
        LoadProc<SteamAudioApi::BinauralRelease>(api.library, "iplBinauralEffectRelease");
    api.binauralApply = LoadProc<SteamAudioApi::BinauralApply>(api.library, "iplBinauralEffectApply");
    api.bufferAllocate = LoadProc<SteamAudioApi::BufferAllocate>(api.library, "iplAudioBufferAllocate");
    api.bufferFree = LoadProc<SteamAudioApi::BufferFree>(api.library, "iplAudioBufferFree");
    api.relativeDirection =
        LoadProc<SteamAudioApi::RelativeDirection>(api.library, "iplCalculateRelativeDirection");
    return api;
}

Vec3 ListenerAhead(const Transform& transform) noexcept
{
    const f32 yaw = Radians(transform.rotation.y);
    const f32 pitch = Radians(transform.rotation.x);
    const f32 cosine = std::cos(pitch);
    return Normalize({-std::sin(yaw) * cosine, std::sin(pitch), -std::cos(yaw) * cosine});
}

Vec3 ListenerUp(const Transform& transform) noexcept
{
    const f32 yaw = Radians(transform.rotation.y);
    const f32 pitch = Radians(transform.rotation.x);
    return Normalize({std::sin(yaw) * std::sin(pitch), std::cos(pitch),
                      std::cos(yaw) * std::sin(pitch)});
}

f32 DistanceGain(f32 distance, f32 minDistance, f32 maxDistance) noexcept
{
    const f32 min = std::max(minDistance, 0.05f);
    const f32 max = std::max(maxDistance, min + 0.05f);
    if (distance <= min) {
        return 1.0f;
    }
    if (distance >= max) {
        return 0.0f;
    }
    const f32 inverse = min / distance;
    const f32 fade = 1.0f - (distance - min) / (max - min);
    return std::clamp(inverse * fade, 0.0f, 1.0f);
}

void EqualPowerPan(Vec3 listenerSpace, f32 gain, f32& left, f32& right) noexcept
{
    const f32 azimuth = std::atan2(listenerSpace.x, -listenerSpace.z);
    const f32 pan = std::clamp(std::sin(azimuth), -1.0f, 1.0f);
    left = gain * std::sqrt(std::clamp(0.5f * (1.0f - pan), 0.0f, 1.0f));
    right = gain * std::sqrt(std::clamp(0.5f * (1.0f + pan), 0.0f, 1.0f));
}

u64 PackEntity(Entity entity) noexcept
{
    return (static_cast<u64>(entity.generation) << 32) | static_cast<u64>(entity.index);
}

std::mutex g_bindMutex;
std::unordered_map<const Scene*, AudioWorld*> g_bound;

struct Voice {
    std::shared_ptr<const AudioClip> clip{};
    Vec3 position{};
    f32 volume = 1.0f;
    f32 pitch = 1.0f;
    f32 minDistance = 1.0f;
    f32 maxDistance = 48.0f;
    f32 cursor = 0.0f;
    bool spatial = true;
    bool loop = false;
    bool playing = false;
};

} // namespace

struct AudioWorld::Impl {
    AudioSettings settings{};
    SteamAudioApi steam{};
    IPLContext context = nullptr;
    IPLHRTF hrtf = nullptr;
    IPLAudioSettings iplAudio{};
    IPLAudioBuffer monoBuffer{};
    IPLAudioBuffer stereoBuffer{};
    bool buffersReady = false;
    SDL_AudioStream* stream = nullptr;
    bool audioSubsystem = false;
    Scene* scene = nullptr;
    std::mutex mixMutex;
    Transform listener{};
    f32 listenerGain = 1.0f;
    std::unordered_map<u64, Voice> voices{};
    std::unordered_map<u64, IPLBinauralEffect> effects{};

    explicit Impl(const AudioSettings& authored) : settings(authored)
    {
        if (settings.sampleRate == 0) {
            settings.sampleRate = 48000;
        }
        if (settings.frameSize == 0) {
            settings.frameSize = 512;
        }
        iplAudio.samplingRate = static_cast<IPLint32>(settings.sampleRate);
        iplAudio.frameSize = static_cast<IPLint32>(settings.frameSize);

        steam = LoadSteamAudio();
        if (steam.Ready()) {
            IPLContextSettings contextSettings{};
            contextSettings.version = STEAMAUDIO_VERSION;
            contextSettings.simdLevel = IPL_SIMDLEVEL_AVX2;
            if (steam.contextCreate(&contextSettings, &context) == IPL_STATUS_SUCCESS &&
                context != nullptr) {
                IPLHRTFSettings hrtfSettings{};
                hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;
                hrtfSettings.volume = 1.0f;
                hrtfSettings.normType = IPL_HRTFNORMTYPE_NONE;
                if (steam.hrtfCreate(context, &iplAudio, &hrtfSettings, &hrtf) !=
                        IPL_STATUS_SUCCESS ||
                    hrtf == nullptr) {
                    steam.contextRelease(&context);
                    context = nullptr;
                } else if (steam.bufferAllocate(context, 1, iplAudio.frameSize, &monoBuffer) ==
                               IPL_STATUS_SUCCESS &&
                           steam.bufferAllocate(context, 2, iplAudio.frameSize, &stereoBuffer) ==
                               IPL_STATUS_SUCCESS) {
                    buffersReady = true;
                }
            }
        }

        if (settings.playOnDevice) {
            audioSubsystem = SDL_InitSubSystem(SDL_INIT_AUDIO);
            if (audioSubsystem) {
                SDL_AudioSpec spec{};
                spec.format = SDL_AUDIO_F32;
                spec.channels = 2;
                spec.freq = static_cast<int>(settings.sampleRate);
                stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                                   &Impl::Pull, this);
                if (stream != nullptr) {
                    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
                }
            }
        }
    }

    ~Impl()
    {
        if (stream != nullptr) {
            SDL_DestroyAudioStream(stream);
            stream = nullptr;
        }
        if (audioSubsystem) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
        DestroyEffects();
        if (steam.Ready() && context != nullptr) {
            if (buffersReady) {
                steam.bufferFree(context, &monoBuffer);
                steam.bufferFree(context, &stereoBuffer);
            }
            if (hrtf != nullptr) {
                steam.hrtfRelease(&hrtf);
            }
            steam.contextRelease(&context);
        }
        if (steam.library != nullptr) {
            FreeLibrary(steam.library);
        }
    }

    static void SDLCALL Pull(void* userdata, SDL_AudioStream* stream, int additional, int)
    {
        auto* impl = static_cast<Impl*>(userdata);
        if (impl == nullptr || additional <= 0) {
            return;
        }
        const int frames = additional / static_cast<int>(sizeof(f32) * 2);
        if (frames <= 0) {
            return;
        }
        std::vector<f32> buffer(static_cast<std::size_t>(frames) * 2);
        impl->Mix(buffer.data(), static_cast<u32>(frames));
        SDL_PutAudioStreamData(stream, buffer.data(), frames * static_cast<int>(sizeof(f32) * 2));
    }

    void DestroyEffects()
    {
        if (!steam.Ready()) {
            effects.clear();
            return;
        }
        for (auto& [key, effect] : effects) {
            if (effect != nullptr) {
                steam.binauralRelease(&effect);
            }
        }
        effects.clear();
    }

    IPLBinauralEffect EffectFor(u64 key)
    {
        if (!HrtfLive()) {
            return nullptr;
        }
        const auto found = effects.find(key);
        if (found != effects.end()) {
            return found->second;
        }
        IPLBinauralEffectSettings created{};
        created.hrtf = hrtf;
        IPLBinauralEffect effect = nullptr;
        if (steam.binauralCreate(context, &iplAudio, &created, &effect) != IPL_STATUS_SUCCESS) {
            return nullptr;
        }
        effects[key] = effect;
        return effect;
    }

    [[nodiscard]] bool HrtfLive() const noexcept
    {
        return steam.Ready() && context != nullptr && hrtf != nullptr && buffersReady;
    }

    f32 SampleClip(const Voice& voice, f32 at) const
    {
        if (!voice.clip || !voice.clip->IsValid()) {
            return 0.0f;
        }
        const auto samples = voice.clip->Samples();
        if (samples.empty()) {
            return 0.0f;
        }
        const f32 last = static_cast<f32>(samples.size() - 1);
        f32 index = at;
        if (voice.loop) {
            const f32 size = static_cast<f32>(samples.size());
            index = std::fmod(index, size);
            if (index < 0.0f) {
                index += size;
            }
        } else if (index < 0.0f || index > last) {
            return 0.0f;
        }
        const u32 a = static_cast<u32>(index);
        const u32 b = std::min(a + 1, static_cast<u32>(samples.size() - 1));
        const f32 t = index - static_cast<f32>(a);
        return samples[a] + (samples[b] - samples[a]) * t;
    }

    void Mix(f32* interleaved, u32 frames)
    {
        if (interleaved == nullptr || frames == 0) {
            return;
        }
        std::memset(interleaved, 0, frames * 2 * sizeof(f32));
        std::lock_guard<std::mutex> guard(mixMutex);
        const Vec3 ahead = ListenerAhead(listener);
        const Vec3 up = ListenerUp(listener);
        const Vec3 right = Normalize(Cross(ahead, up));

        for (auto& [key, voice] : voices) {
            if (!voice.playing || !voice.clip || !voice.clip->IsValid()) {
                continue;
            }
            const f32 pitch = std::isfinite(voice.pitch) && voice.pitch > 0.0f ? voice.pitch : 1.0f;
            Vec3 toSource = voice.position - listener.position;
            const f32 distance = Length(toSource);
            f32 gain = std::clamp(voice.volume, 0.0f, 4.0f) * listenerGain;
            if (voice.spatial) {
                gain *= DistanceGain(distance, voice.minDistance, voice.maxDistance);
            }
            if (gain <= 1.0e-5f) {
                voice.cursor += pitch * static_cast<f32>(frames);
                if (!voice.loop && voice.cursor >= static_cast<f32>(voice.clip->FrameCount())) {
                    voice.playing = false;
                    voice.cursor = 0.0f;
                }
                continue;
            }

            Vec3 local = toSource;
            if (distance > 1.0e-5f) {
                local = {Dot(toSource, right), Dot(toSource, up), Dot(toSource, -ahead)};
                local = Normalize(local);
            } else {
                local = {0.0f, 0.0f, -1.0f};
            }

            IPLBinauralEffect effect = nullptr;
            if (voice.spatial && HrtfLive() && frames == static_cast<u32>(iplAudio.frameSize)) {
                effect = EffectFor(key);
            }

            if (effect != nullptr && steam.relativeDirection) {
                const IPLVector3 direction = steam.relativeDirection(
                    context, {voice.position.x, voice.position.y, voice.position.z},
                    {listener.position.x, listener.position.y, listener.position.z},
                    {ahead.x, ahead.y, ahead.z}, {up.x, up.y, up.z});
                for (u32 i = 0; i < frames; ++i) {
                    monoBuffer.data[0][i] = SampleClip(voice, voice.cursor + pitch * static_cast<f32>(i)) * gain;
                }
                IPLBinauralEffectParams params{};
                params.direction = direction;
                params.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
                params.spatialBlend = 1.0f;
                params.hrtf = hrtf;
                steam.binauralApply(effect, &params, &monoBuffer, &stereoBuffer);
                for (u32 i = 0; i < frames; ++i) {
                    interleaved[i * 2] += stereoBuffer.data[0][i];
                    interleaved[i * 2 + 1] += stereoBuffer.data[1][i];
                }
            } else {
                f32 leftGain = gain;
                f32 rightGain = gain;
                if (voice.spatial) {
                    EqualPowerPan(local, gain, leftGain, rightGain);
                }
                for (u32 i = 0; i < frames; ++i) {
                    const f32 sample = SampleClip(voice, voice.cursor + pitch * static_cast<f32>(i));
                    interleaved[i * 2] += sample * leftGain;
                    interleaved[i * 2 + 1] += sample * rightGain;
                }
            }

            voice.cursor += pitch * static_cast<f32>(frames);
            if (!voice.loop && voice.cursor >= static_cast<f32>(voice.clip->FrameCount())) {
                voice.playing = false;
                voice.cursor = 0.0f;
            } else if (voice.loop && voice.clip->FrameCount() > 0) {
                const f32 size = static_cast<f32>(voice.clip->FrameCount());
                voice.cursor = std::fmod(voice.cursor, size);
                if (voice.cursor < 0.0f) {
                    voice.cursor += size;
                }
            }
        }
    }
};

AudioWorld::AudioWorld(const AudioSettings& settings)
{
    m_impl = std::make_unique<Impl>(settings);
}

AudioWorld::~AudioWorld()
{
    UnbindScene();
    m_impl.reset();
}

void AudioWorld::BindScene(Scene& scene)
{
    std::lock_guard<std::mutex> guard(g_bindMutex);
    if (m_impl->scene != nullptr) {
        g_bound.erase(m_impl->scene);
    }
    m_impl->scene = &scene;
    g_bound[&scene] = this;
}

void AudioWorld::UnbindScene()
{
    std::lock_guard<std::mutex> guard(g_bindMutex);
    if (m_impl && m_impl->scene != nullptr) {
        g_bound.erase(m_impl->scene);
        m_impl->scene = nullptr;
    }
    if (m_impl) {
        std::lock_guard<std::mutex> mix(m_impl->mixMutex);
        m_impl->voices.clear();
        m_impl->DestroyEffects();
    }
}

void AudioWorld::Step(Scene& scene, f32)
{
    World& world = scene.GetWorld();
    Transform listener = m_impl->listener;
    f32 listenerGain = 1.0f;
    bool haveListener = false;
    world.Query<AudioListener, Transform>(
        [&](Entity, const AudioListener& ears, const Transform& transform) {
            if (!ears.enabled || haveListener) {
                return;
            }
            listener = transform;
            listenerGain = ears.gain;
            haveListener = true;
        });
    if (!haveListener) {
        const Entity camera = scene.MainCamera();
        if (const Transform* transform = world.Get<Transform>(camera)) {
            listener = *transform;
        }
    }

    std::unordered_map<u64, Voice> next;
    world.Query<AudioSource, Transform>(
        [&](Entity entity, AudioSource& source, const Transform& transform) {
            if (source.playOnStart && source.clip && source.clip->IsValid()) {
                source.playing = true;
                source.playOnStart = false;
            }
            Voice voice{};
            voice.clip = source.clip;
            voice.position = transform.position;
            voice.volume = source.volume;
            voice.pitch = source.pitch;
            voice.minDistance = source.minDistance;
            voice.maxDistance = source.maxDistance;
            voice.cursor = source.cursor;
            voice.spatial = source.spatial;
            voice.loop = source.loop;
            voice.playing = source.playing;
            next[PackEntity(entity)] = voice;
        });

    std::unordered_map<u64, Voice> playback;
    {
        std::lock_guard<std::mutex> guard(m_impl->mixMutex);
        for (auto& [key, voice] : next) {
            const auto existing = m_impl->voices.find(key);
            if (existing != m_impl->voices.end()) {
                voice.cursor = existing->second.cursor;
                voice.playing = existing->second.playing && voice.playing;
            }
        }
        m_impl->voices.swap(next);
        m_impl->listener = listener;
        m_impl->listenerGain = listenerGain;
        std::vector<u64> stale;
        for (const auto& [key, effect] : m_impl->effects) {
            if (!m_impl->voices.contains(key)) {
                stale.push_back(key);
            }
        }
        for (const u64 key : stale) {
            if (m_impl->steam.Ready() && m_impl->effects[key] != nullptr) {
                m_impl->steam.binauralRelease(&m_impl->effects[key]);
            }
            m_impl->effects.erase(key);
        }
        if (m_impl->HrtfLive()) {
            for (const auto& [key, voice] : m_impl->voices) {
                if (voice.spatial && voice.playing) {
                    (void)m_impl->EffectFor(key);
                }
            }
        }
        playback = m_impl->voices;
    }

    world.Query<AudioSource>([&](Entity entity, AudioSource& source) {
        const auto found = playback.find(PackEntity(entity));
        if (found == playback.end()) {
            return;
        }
        source.cursor = found->second.cursor;
        source.playing = found->second.playing;
    });
}

bool AudioWorld::Play(Entity entity)
{
    if (m_impl->scene == nullptr) {
        return false;
    }
    AudioSource* source = m_impl->scene->GetWorld().Get<AudioSource>(entity);
    if (source == nullptr || !source->clip || !source->clip->IsValid()) {
        return false;
    }
    source->playing = true;
    source->playOnStart = false;
    std::lock_guard<std::mutex> guard(m_impl->mixMutex);
    Voice& voice = m_impl->voices[PackEntity(entity)];
    voice.clip = source->clip;
    voice.playing = true;
    if (const Transform* transform = m_impl->scene->GetWorld().Get<Transform>(entity)) {
        voice.position = transform->position;
    }
    voice.volume = source->volume;
    voice.pitch = source->pitch;
    voice.minDistance = source->minDistance;
    voice.maxDistance = source->maxDistance;
    voice.spatial = source->spatial;
    voice.loop = source->loop;
    return true;
}

bool AudioWorld::Stop(Entity entity)
{
    if (m_impl->scene == nullptr) {
        return false;
    }
    bool stopped = false;
    if (AudioSource* source = m_impl->scene->GetWorld().Get<AudioSource>(entity)) {
        source->playing = false;
        source->cursor = 0.0f;
        source->playOnStart = false;
        stopped = true;
    }
    std::lock_guard<std::mutex> guard(m_impl->mixMutex);
    const auto found = m_impl->voices.find(PackEntity(entity));
    if (found != m_impl->voices.end()) {
        found->second.playing = false;
        found->second.cursor = 0.0f;
        stopped = true;
    }
    return stopped;
}

void AudioWorld::MixStereo(f32* interleaved, u32 frames)
{
    m_impl->Mix(interleaved, frames);
}

bool AudioWorld::DeviceOpen() const noexcept
{
    return m_impl->stream != nullptr;
}

bool AudioWorld::HrtfReady() const noexcept
{
    return m_impl->HrtfLive();
}

AudioWorld* AudioWorld::Find(const Scene& scene)
{
    std::lock_guard<std::mutex> guard(g_bindMutex);
    const auto found = g_bound.find(&scene);
    return found == g_bound.end() ? nullptr : found->second;
}

} // namespace Concord
