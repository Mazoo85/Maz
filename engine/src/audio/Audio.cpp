#include "maz/audio/Audio.hpp"

#include "maz/core/Log.hpp"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace maz::audio {

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr int kSampleRate = 44100;
} // namespace

struct Audio::Impl {
    static constexpr int kMaxVoices = 24;

    struct Voice {
        bool active = false;
        Wave wave = Wave::Square;
        double phase = 0.0;
        float freq = 440.0f;
        float freqEnd = 0.0f;
        float volume = 0.3f;
        float leftGain = 1.0f;  // stereo pan (per-channel multiplier)
        float rightGain = 1.0f;
        int total = 0;
        int left = 0;
        uint32_t noise = 0x2545f491u;
    };

    SDL_AudioStream* stream = nullptr;
    std::mutex mutex;
    Voice voices[kMaxVoices];
    float master = 0.6f;

    // Looping arpeggio "music bed".
    bool musicOn = false;
    double musicPhase = 0.0;
    int musicCtr = 0;
    int musicStep = 0;

    float renderVoice(Voice& v);
    float renderMusic();
    static void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional, int total);
};

float Audio::Impl::renderVoice(Voice& v) {
    const int pos = v.total - v.left; // samples elapsed
    const float t = v.total > 0 ? static_cast<float>(pos) / static_cast<float>(v.total) : 0.0f;
    const float curFreq = v.freqEnd > 0.0f ? v.freq + (v.freqEnd - v.freq) * t : v.freq;

    float raw = 0.0f;
    switch (v.wave) {
    case Wave::Sine: raw = std::sin(static_cast<float>(v.phase)); break;
    case Wave::Square: raw = std::sin(v.phase) >= 0.0 ? 1.0f : -1.0f; break;
    case Wave::Triangle:
        raw = static_cast<float>(2.0 / kTwoPi * 2.0 * std::asin(std::sin(v.phase)));
        break;
    case Wave::Noise: {
        v.noise ^= v.noise << 13;
        v.noise ^= v.noise >> 17;
        v.noise ^= v.noise << 5;
        raw = static_cast<float>(v.noise & 0xffffu) / 32768.0f - 1.0f;
        break;
    }
    }

    v.phase += kTwoPi * static_cast<double>(curFreq) / kSampleRate;
    if (v.phase > kTwoPi) {
        v.phase -= kTwoPi;
    }

    // Short attack to avoid clicks, then linear decay to zero over the remaining duration.
    const int attack = std::min(static_cast<int>(0.004 * kSampleRate), v.total / 5 + 1);
    float env;
    if (pos < attack) {
        env = static_cast<float>(pos) / static_cast<float>(attack);
    } else {
        env = static_cast<float>(v.left) / static_cast<float>(v.total - attack);
    }

    const float s = raw * env * v.volume;
    if (--v.left <= 0) {
        v.active = false;
    }
    return s;
}

float Audio::Impl::renderMusic() {
    // A simple plucked square-wave arpeggio over a minor-ish pattern.
    static const int pattern[] = {0, 7, 12, 7, 3, 10, 12, 10};
    const int patternLen = static_cast<int>(sizeof(pattern) / sizeof(pattern[0]));
    const int stepLen = kSampleRate / 6; // ~8th notes
    const float root = 130.81f;          // C3

    const float noteEnv = 1.0f - static_cast<float>(musicCtr) / static_cast<float>(stepLen);
    const float semis = static_cast<float>(pattern[musicStep]);
    const float freq = root * std::pow(2.0f, semis / 12.0f);

    musicPhase += kTwoPi * static_cast<double>(freq) / kSampleRate;
    if (musicPhase > kTwoPi) {
        musicPhase -= kTwoPi;
    }
    const float raw = std::sin(musicPhase) >= 0.0 ? 1.0f : -1.0f;

    if (++musicCtr >= stepLen) {
        musicCtr = 0;
        musicStep = (musicStep + 1) % patternLen;
    }
    return raw * 0.07f * noteEnv;
}

void SDLCALL Audio::Impl::feed(void* userdata, SDL_AudioStream* stream, int additional, int) {
    if (additional <= 0) {
        return;
    }
    Impl* self = static_cast<Impl*>(userdata);
    const int frames = additional / static_cast<int>(sizeof(float) * 2); // stereo interleaved L,R
    std::vector<float> buffer(static_cast<size_t>(frames) * 2, 0.0f);

    {
        std::lock_guard<std::mutex> lock(self->mutex);
        for (int i = 0; i < frames; ++i) {
            float mixL = 0.0f, mixR = 0.0f;
            for (Voice& v : self->voices) {
                if (v.active) {
                    const float s = self->renderVoice(v); // advances the voice exactly once per frame
                    mixL += s * v.leftGain;
                    mixR += s * v.rightGain;
                }
            }
            if (self->musicOn) {
                const float m = self->renderMusic();
                mixL += m;
                mixR += m;
            }
            mixL *= self->master;
            mixR *= self->master;
            buffer[static_cast<size_t>(i) * 2] = std::min(1.0f, std::max(-1.0f, mixL));
            buffer[static_cast<size_t>(i) * 2 + 1] = std::min(1.0f, std::max(-1.0f, mixR));
        }
    }
    SDL_PutAudioStreamData(stream, buffer.data(), static_cast<int>(buffer.size() * sizeof(float)));
}

Audio::~Audio() { shutdown(); }

bool Audio::init() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        MAZ_LOG_WARN("audio: SDL_InitSubSystem failed: %s", SDL_GetError());
        return false;
    }
    m_impl = new Impl();

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2; // stereo, for positional panning
    spec.freq = kSampleRate;

    m_impl->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                               &Impl::feed, m_impl);
    if (!m_impl->stream) {
        MAZ_LOG_WARN("audio: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        delete m_impl;
        m_impl = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    SDL_ResumeAudioStreamDevice(m_impl->stream);
    m_active = true;
    MAZ_LOG_INFO("audio active (%d Hz, driver: %s)", kSampleRate, SDL_GetCurrentAudioDriver());
    return true;
}

void Audio::shutdown() {
    if (m_impl) {
        if (m_impl->stream) {
            SDL_DestroyAudioStream(m_impl->stream);
        }
        delete m_impl;
        m_impl = nullptr;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
    m_active = false;
}

void Audio::play(const SoundDesc& sound) {
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    // Find a free voice, else steal the one with the least time remaining.
    Impl::Voice* slot = nullptr;
    for (Impl::Voice& v : m_impl->voices) {
        if (!v.active) {
            slot = &v;
            break;
        }
        if (!slot || v.left < slot->left) {
            slot = &v;
        }
    }
    slot->active = true;
    slot->wave = sound.wave;
    slot->phase = 0.0;
    slot->freq = sound.freq;
    slot->freqEnd = sound.freqEnd;
    slot->volume = sound.volume;
    slot->leftGain = sound.leftGain;
    slot->rightGain = sound.rightGain;
    slot->total = std::max(1, static_cast<int>(sound.duration * kSampleRate));
    slot->left = slot->total;
}

void Audio::setMusic(bool on) {
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->musicOn = on;
}

void Audio::setMasterVolume(float v) {
    if (!m_impl) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->master = std::min(1.0f, std::max(0.0f, v));
}

} // namespace maz::audio
