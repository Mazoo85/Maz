#include "maz/audio/AudioEngine.hpp"

#include "maz/core/Log.hpp"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include <algorithm>
#include <cstddef>

namespace maz::audio {

namespace {

// SDL pulls audio on its own thread and calls this whenever the device wants more data. We render
// exactly the requested amount and hand it back. Rendering here reads the shared voice state
// (frequency/gate) set from the main thread; that benign race is acceptable for this milestone.
void SDLCALL feedCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount,
                          int /*totalAmount*/) {
    auto* engine = static_cast<AudioEngine*>(userdata);
    const int channels = engine->config().channels;
    const int bytesPerFrame = channels * static_cast<int>(sizeof(float));
    if (additionalAmount <= 0 || bytesPerFrame <= 0) {
        return;
    }
    const int frames = additionalAmount / bytesPerFrame;
    if (frames <= 0) {
        return;
    }
    std::vector<float> buffer(static_cast<size_t>(frames) * static_cast<size_t>(channels), 0.0f);
    engine->render(buffer.data(), frames);
    SDL_PutAudioStreamData(stream, buffer.data(), frames * bytesPerFrame);
}

} // namespace

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initRealtime(const AudioConfig& cfg) {
    cfg_ = cfg;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        MAZ_LOG_ERROR("audio: SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec spec{};
    spec.freq = cfg_.sampleRate;
    spec.format = SDL_AUDIO_F32;
    spec.channels = cfg_.channels;

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feedCallback, this);
    if (stream_ == nullptr) {
        MAZ_LOG_ERROR("audio: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(stream_);
    MAZ_LOG_INFO("audio: realtime device open (%d Hz, %d ch)", cfg_.sampleRate, cfg_.channels);
    return true;
}

void AudioEngine::initOffline(const AudioConfig& cfg) {
    cfg_ = cfg;
    MAZ_LOG_INFO("audio: offline engine (%d Hz, %d ch)", cfg_.sampleRate, cfg_.channels);
}

void AudioEngine::shutdown() {
    if (stream_ != nullptr) {
        // Destroying a stream created by SDL_OpenAudioDeviceStream also closes its device.
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
}

void AudioEngine::render(float* out, int frames) {
    const int ch = cfg_.channels;
    if (frames <= 0 || ch <= 0) {
        return;
    }

    // Control-rate automation: evaluate the LFO lanes once for this block at the current transport
    // time and write the swept values onto their target parameters before rendering.
    if (automation_.anyEnabled() && cfg_.sampleRate > 0) {
        automation_.apply(*this, static_cast<double>(framesRendered_) /
                                     static_cast<double>(cfg_.sampleRate));
    }

    const size_t total = static_cast<size_t>(frames) * static_cast<size_t>(ch);
    std::fill(out, out + total, 0.0f);

    // Render the oscillator voice + the step sequencer as mono, then fan out across the channels.
    scratch_.assign(static_cast<size_t>(frames), 0.0f);
    voice_.render(scratch_.data(), frames, cfg_.sampleRate);
    sequencer_.render(scratch_.data(), frames, cfg_.sampleRate);
    for (int i = 0; i < frames; ++i) {
        const float s = scratch_[static_cast<size_t>(i)];
        for (int c = 0; c < ch; ++c) {
            out[static_cast<size_t>(i) * static_cast<size_t>(ch) + static_cast<size_t>(c)] = s;
        }
    }

    // Master bus: run the mixer's effect chain + master gain over the finished stereo output.
    if (ch == 2) {
        mixer_.process(out, frames, cfg_.sampleRate);
    }

    framesRendered_ += static_cast<uint64_t>(frames);
}

std::vector<float> AudioEngine::renderOffline(double seconds) {
    const int frames = static_cast<int>(seconds * static_cast<double>(cfg_.sampleRate));
    std::vector<float> buffer(static_cast<size_t>(std::max(frames, 0)) *
                                  static_cast<size_t>(cfg_.channels),
                              0.0f);
    // Render in blocks, exactly as the device callback would, so the envelope and sample clock
    // behave identically to the real-time path.
    constexpr int kBlock = 512;
    int done = 0;
    while (done < frames) {
        const int n = std::min(kBlock, frames - done);
        render(buffer.data() + static_cast<size_t>(done) * static_cast<size_t>(cfg_.channels), n);
        done += n;
    }
    return buffer;
}

} // namespace maz::audio
