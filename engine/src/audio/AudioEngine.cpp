#include "maz/audio/AudioEngine.hpp"

#include "maz/audio/WavWriter.hpp"
#include "maz/core/Log.hpp"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include <algorithm>
#include <cmath>
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
    if (captureStream_ != nullptr) {
        SDL_DestroyAudioStream(captureStream_);
        captureStream_ = nullptr;
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
        automation_.apply(*this,
                          static_cast<double>(framesRendered_) / static_cast<double>(cfg_.sampleRate),
                          sequencer_.bpm());
    }

    const size_t total = static_cast<size_t>(frames) * static_cast<size_t>(ch);
    std::fill(out, out + total, 0.0f);

    if (ch == 2) {
        // Stereo path: the oscillator sits at center; the sequencer adds its own panned stereo mix.
        constexpr float kCenter = 0.70710678f;
        scratch_.assign(static_cast<size_t>(frames), 0.0f);
        voice_.render(scratch_.data(), frames, cfg_.sampleRate);
        for (int i = 0; i < frames; ++i) {
            const float s = scratch_[static_cast<size_t>(i)] * kCenter;
            out[2 * i] += s;
            out[2 * i + 1] += s;
        }
        if (mixer_.anyTrackActive()) {
            // Per-track path: render the three buses separately, run each through its insert strip,
            // then sum with the tanh bus soft-limit (matching Sequencer::render's summation).
            const size_t n2 = static_cast<size_t>(frames) * 2;
            stemDrums_.assign(n2, 0.0f);
            stemLead_.assign(n2, 0.0f);
            stemBass_.assign(n2, 0.0f);
            sequencer_.renderStems(stemDrums_.data(), stemLead_.data(), stemBass_.data(), frames,
                                   cfg_.sampleRate);
            mixer_.track(MixerBus::Drums).process(stemDrums_.data(), frames, cfg_.sampleRate);
            mixer_.track(MixerBus::Lead).process(stemLead_.data(), frames, cfg_.sampleRate);
            mixer_.track(MixerBus::Bass).process(stemBass_.data(), frames, cfg_.sampleRate);
            for (size_t i = 0; i < n2; ++i) {
                const double s = static_cast<double>(stemDrums_[i]) +
                                 static_cast<double>(stemLead_[i]) +
                                 static_cast<double>(stemBass_[i]);
                out[i] += static_cast<float>(std::tanh(s));
            }
        } else {
            sequencer_.render(out, frames, cfg_.sampleRate);
        }
        // Master bus: the mixer's effect chain + master gain + limiter over the stereo output.
        mixer_.delay().updateTempo(sequencer_.bpm());       // sync the delay time to the transport tempo
        mixer_.stereoDelay().updateTempo(sequencer_.bpm()); // sync the dual-delay L/R times too
        mixer_.tremolo().updateTempo(sequencer_.bpm());     // sync the trance-gate rate too
        mixer_.chorus().updateTempo(sequencer_.bpm());      // sync the chorus/flanger LFO rates too
        mixer_.flanger().updateTempo(sequencer_.bpm());
        mixer_.process(out, frames, cfg_.sampleRate);
    } else {
        // Fallback mono path: sum the oscillator only (the sequencer targets stereo).
        scratch_.assign(static_cast<size_t>(frames), 0.0f);
        voice_.render(scratch_.data(), frames, cfg_.sampleRate);
        for (int i = 0; i < frames; ++i) {
            out[static_cast<size_t>(i)] += scratch_[static_cast<size_t>(i)];
        }
    }

    // Capture the finished output if recording is armed.
    if (recording_) {
        recordBuffer_.insert(recordBuffer_.end(), out, out + total);
    }

    framesRendered_ += static_cast<uint64_t>(frames);
}

void AudioEngine::armRecording() {
    recordBuffer_.clear();
    recording_ = true;
}

void AudioEngine::stopRecording() {
    recording_ = false;
}

bool AudioEngine::saveRecording(const std::string& path, std::string* err) const {
    const int ch = cfg_.channels > 0 ? cfg_.channels : 2;
    const int frames = static_cast<int>(recordBuffer_.size()) / ch;
    return writeWav16(path, recordBuffer_.data(), frames, ch, cfg_.sampleRate, err);
}

bool AudioEngine::startInputCapture(const AudioConfig& cfg) {
    cfg_ = cfg;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        MAZ_LOG_ERROR("audio: SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }
    SDL_AudioSpec spec{};
    spec.freq = cfg_.sampleRate;
    spec.format = SDL_AUDIO_F32;
    spec.channels = cfg_.channels;
    captureStream_ =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, nullptr, nullptr);
    if (captureStream_ == nullptr) {
        MAZ_LOG_ERROR("audio: input capture open failed: %s", SDL_GetError());
        return false;
    }
    SDL_ResumeAudioStreamDevice(captureStream_);
    armRecording();
    MAZ_LOG_INFO("audio: input capture open (%d Hz, %d ch)", cfg_.sampleRate, cfg_.channels);
    return true;
}

void AudioEngine::stopInputCapture() {
    stopRecording();
    if (captureStream_ != nullptr) {
        SDL_DestroyAudioStream(captureStream_);
        captureStream_ = nullptr;
    }
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
