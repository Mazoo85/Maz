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
            // Per-bus solo: when any bus is soloed, silence the buses that are not.
            if (mixer_.anyTrackSoloed()) {
                if (!mixer_.track(MixerBus::Drums).soloed()) {
                    std::fill(stemDrums_.begin(), stemDrums_.end(), 0.0f);
                }
                if (!mixer_.track(MixerBus::Lead).soloed()) {
                    std::fill(stemLead_.begin(), stemLead_.end(), 0.0f);
                }
                if (!mixer_.track(MixerBus::Bass).soloed()) {
                    std::fill(stemBass_.begin(), stemBass_.end(), 0.0f);
                }
            }
            // Per-bus aux sends: sum each bus's (post-insert, post-solo) signal scaled by its own
            // reverb/delay send into the shared return feeds, handed to the master before its returns.
            const float rsD = mixer_.track(MixerBus::Drums).reverbSend();
            const float rsL = mixer_.track(MixerBus::Lead).reverbSend();
            const float rsB = mixer_.track(MixerBus::Bass).reverbSend();
            const float dsD = mixer_.track(MixerBus::Drums).delaySend();
            const float dsL = mixer_.track(MixerBus::Lead).delaySend();
            const float dsB = mixer_.track(MixerBus::Bass).delaySend();
            if (rsD > 0.0f || rsL > 0.0f || rsB > 0.0f) {
                reverbAuxBuf_.assign(n2, 0.0f);
                for (size_t i = 0; i < n2; ++i) {
                    reverbAuxBuf_[i] = stemDrums_[i] * rsD + stemLead_[i] * rsL + stemBass_[i] * rsB;
                }
                mixer_.setReverbAux(reverbAuxBuf_);
            }
            if (dsD > 0.0f || dsL > 0.0f || dsB > 0.0f) {
                delayAuxBuf_.assign(n2, 0.0f);
                for (size_t i = 0; i < n2; ++i) {
                    delayAuxBuf_[i] = stemDrums_[i] * dsD + stemLead_[i] * dsL + stemBass_[i] * dsB;
                }
                mixer_.setDelayAux(delayAuxBuf_);
            }
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
        mixer_.chorus().updateTempo(sequencer_.bpm());      // sync the chorus/flanger/phaser LFO rates
        mixer_.vibrato().updateTempo(sequencer_.bpm());     // sync the vibrato LFO rate too
        mixer_.flanger().updateTempo(sequencer_.bpm());
        mixer_.phaser().updateTempo(sequencer_.bpm());
        mixer_.autopan().updateTempo(sequencer_.bpm());     // sync the auto-pan rate too
        mixer_.process(out, frames, cfg_.sampleRate);
    } else {
        // Fallback mono path: sum the oscillator only (the sequencer targets stereo).
        scratch_.assign(static_cast<size_t>(frames), 0.0f);
        voice_.render(scratch_.data(), frames, cfg_.sampleRate);
        for (int i = 0; i < frames; ++i) {
            out[static_cast<size_t>(i)] += scratch_[static_cast<size_t>(i)];
        }
    }

    // Master output metering: peak + RMS of the finished block, for the UI level meter.
    {
        const int meterN = frames * cfg_.channels;
        float pk = 0.0f;
        double sq = 0.0;
        for (int i = 0; i < meterN; ++i) {
            const float a = std::fabs(out[i]);
            if (a > pk) {
                pk = a;
            }
            sq += static_cast<double>(out[i]) * static_cast<double>(out[i]);
        }
        masterPeak_ = pk;
        masterRms_ = meterN > 0 ? static_cast<float>(std::sqrt(sq / static_cast<double>(meterN))) : 0.0f;
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

AudioEngine::Stems AudioEngine::renderStemsOffline(double seconds) {
    Stems stems;
    if (cfg_.channels != 2 || cfg_.sampleRate <= 0) {
        return stems; // stems are stereo-only
    }
    const int frames = static_cast<int>(seconds * static_cast<double>(cfg_.sampleRate));
    if (frames <= 0) {
        return stems;
    }
    const size_t total = static_cast<size_t>(frames) * 2;
    stems.drums.assign(total, 0.0f);
    stems.lead.assign(total, 0.0f);
    stems.bass.assign(total, 0.0f);

    // Block loop mirroring renderOffline/render, but capturing the three per-bus buffers post-track,
    // pre-master. Automation is applied per block so track-level lanes (e.g. lead volume/pan) are
    // reflected in the stems, matching the master render.
    constexpr int kBlock = 512;
    std::vector<float> blkDrums, blkLead, blkBass;
    int done = 0;
    while (done < frames) {
        const int n = std::min(kBlock, frames - done);
        if (automation_.anyEnabled()) {
            // Time base runs from 0 for this bounce, so an automation-driven stem export lines up
            // with a master render made the same way from the top.
            automation_.apply(*this, static_cast<double>(done) / static_cast<double>(cfg_.sampleRate),
                              sequencer_.bpm());
        }
        const size_t bn = static_cast<size_t>(n) * 2;
        blkDrums.assign(bn, 0.0f);
        blkLead.assign(bn, 0.0f);
        blkBass.assign(bn, 0.0f);
        sequencer_.renderStems(blkDrums.data(), blkLead.data(), blkBass.data(), n, cfg_.sampleRate);
        mixer_.track(MixerBus::Drums).process(blkDrums.data(), n, cfg_.sampleRate);
        mixer_.track(MixerBus::Lead).process(blkLead.data(), n, cfg_.sampleRate);
        mixer_.track(MixerBus::Bass).process(blkBass.data(), n, cfg_.sampleRate);
        const size_t off = static_cast<size_t>(done) * 2;
        for (size_t i = 0; i < bn; ++i) {
            stems.drums[off + i] = blkDrums[i];
            stems.lead[off + i] = blkLead[i];
            stems.bass[off + i] = blkBass[i];
        }
        framesRendered_ += static_cast<uint64_t>(n);
        done += n;
    }
    return stems;
}

} // namespace maz::audio
