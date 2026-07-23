#include "maz/audio/AudioEngine.hpp"

#include "maz/audio/ProjectIO.hpp"
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

// SDL calls this on its capture thread when recorded input is available. We pull the new bytes out
// of the stream and append them to the engine's record buffer — without this drain the capture
// stream fills and the input is never recorded.
void SDLCALL captureCallback(void* userdata, SDL_AudioStream* stream, int additionalAmount,
                             int /*totalAmount*/) {
    auto* engine = static_cast<AudioEngine*>(userdata);
    if (additionalAmount <= 0) {
        return;
    }
    std::vector<float> buffer(static_cast<size_t>(additionalAmount) / sizeof(float), 0.0f);
    const int got = SDL_GetAudioStreamData(stream, buffer.data(), additionalAmount);
    if (got > 0) {
        engine->appendCapturedInput(buffer.data(), got / static_cast<int>(sizeof(float)));
    }
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

void AudioEngine::snapshotForUndo() {
    undoHistory_.push(saveProjectToString(sequencer_, mixer_, automation_));
}

bool AudioEngine::undoEdit() {
    std::string current = saveProjectToString(sequencer_, mixer_, automation_);
    std::string prev;
    if (!undoHistory_.undo(current, prev)) {
        return false;
    }
    return loadProjectFromString(prev, sequencer_, mixer_, automation_);
}

bool AudioEngine::redoEdit() {
    std::string current = saveProjectToString(sequencer_, mixer_, automation_);
    std::string next;
    if (!undoHistory_.redo(current, next)) {
        return false;
    }
    return loadProjectFromString(next, sequencer_, mixer_, automation_);
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
    applyTimelineAutomationClips(); // 2-D playlist automation clips, gated by song-bar position

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
        if (mixer_.anyTrackActive() || mixer_.groupCount() > 0) {
            // Per-track path: render the three buses separately, run each through its insert strip,
            // then sum with the tanh bus soft-limit (matching Sequencer::render's summation). Also
            // taken whenever submix groups exist, since routing a bus into a group needs this path.
            const size_t n2 = static_cast<size_t>(frames) * 2;
            const int ng = mixer_.groupCount();
            stemDrums_.assign(n2, 0.0f);
            stemLead_.assign(n2, 0.0f);
            stemBass_.assign(n2, 0.0f);
            // Group buffers: allocate + zero BEFORE rendering so instrument channels routed to a group
            // render straight into them (their own insert chain); the buses add in afterward. With no
            // groups this collapses to the plain 3-stem render (bit-identical).
            groupPtrs_.clear();
            if (ng > 0) {
                if (static_cast<int>(groupBufs_.size()) < ng) {
                    groupBufs_.resize(static_cast<size_t>(ng));
                }
                groupPtrs_.resize(static_cast<size_t>(ng));
                for (int g = 0; g < ng; ++g) {
                    groupBufs_[static_cast<size_t>(g)].assign(n2, 0.0f);
                    groupPtrs_[static_cast<size_t>(g)] = groupBufs_[static_cast<size_t>(g)].data();
                }
            }
            sequencer_.renderStems(stemDrums_.data(), stemLead_.data(), stemBass_.data(),
                                   ng > 0 ? groupPtrs_.data() : nullptr, ng, frames, cfg_.sampleRate);
            mixer_.track(MixerBus::Drums).process(stemDrums_.data(), frames, cfg_.sampleRate);
            mixer_.track(MixerBus::Lead).process(stemLead_.data(), frames, cfg_.sampleRate);
            mixer_.track(MixerBus::Bass).process(stemBass_.data(), frames, cfg_.sampleRate);
            // Unified solo across buses + submix groups: when anything is soloed, only "audible" strips
            // reach the master. A strip is audible if it is downstream of a soloed strip (carrying the
            // solo out to master) or on the input side of a soloed GROUP (so a soloed group keeps the
            // buses/groups feeding it). Resolved over the small forward-only routing DAG. Node indices:
            // 0..2 = the drums/lead/bass buses, 3+g = submix group g. audible[] is reused below to gate
            // the per-group fold.
            const bool anySolo = mixer_.anyTrackSoloed() || mixer_.anyGroupSoloed();
            const int nNodes = 3 + ng;
            soloAudible_.assign(static_cast<size_t>(nNodes), 1);
            if (anySolo) {
                auto target = [&](int node) -> int { // output node index, or -1 for master
                    if (node < 3) {
                        const int t = mixer_.track(node).output();
                        return (t >= 0 && t < ng) ? 3 + t : -1;
                    }
                    const int g = node - 3;
                    const int t = mixer_.group(g).output();
                    return (t > g && t < ng) ? 3 + t : -1;
                };
                auto isSoloed = [&](int node) {
                    return node < 3 ? mixer_.track(node).soloed() : mixer_.group(node - 3).soloed();
                };
                soloDown_.assign(static_cast<size_t>(nNodes), 0);
                soloFeeds_.assign(static_cast<size_t>(nNodes), 0);
                // downstream-of-solo: walk forward from each soloed node along its output chain.
                for (int n = 0; n < nNodes; ++n) {
                    if (isSoloed(n)) {
                        for (int cur = n; cur >= 0 && !soloDown_[static_cast<size_t>(cur)];
                             cur = target(cur)) {
                            soloDown_[static_cast<size_t>(cur)] = 1;
                        }
                    }
                }
                // feeds-a-soloed-group: a node whose output path passes through a soloed group. The DAG
                // is forward-only (groups ascend, buses feed groups), so a decreasing-index pass sees
                // each node's target (higher index) already resolved.
                for (int n = nNodes - 1; n >= 0; --n) {
                    const int t = target(n);
                    if (t >= 0) {
                        const bool tSoloedGroup = t >= 3 && mixer_.group(t - 3).soloed();
                        soloFeeds_[static_cast<size_t>(n)] =
                            (tSoloedGroup || soloFeeds_[static_cast<size_t>(t)]) ? 1 : 0;
                    }
                }
                for (int n = 0; n < nNodes; ++n) {
                    soloAudible_[static_cast<size_t>(n)] =
                        (soloDown_[static_cast<size_t>(n)] || soloFeeds_[static_cast<size_t>(n)]) ? 1 : 0;
                }
                // Silence the stems of non-audible buses before routing / aux sends.
                if (!soloAudible_[0]) {
                    std::fill(stemDrums_.begin(), stemDrums_.end(), 0.0f);
                }
                if (!soloAudible_[1]) {
                    std::fill(stemLead_.begin(), stemLead_.end(), 0.0f);
                }
                if (!soloAudible_[2]) {
                    std::fill(stemBass_.begin(), stemBass_.end(), 0.0f);
                }
            }
            // Aux sends: each bus's (post-insert, post-solo) signal scaled by its own reverb/delay send
            // feeds the shared return, PLUS each audible submix group's post-insert signal by its own
            // send (accumulated in the group loop below). The feeds are handed to the master once, after
            // routing, so the master's returns process buses and groups alike.
            const float rsD = mixer_.track(MixerBus::Drums).reverbSend();
            const float rsL = mixer_.track(MixerBus::Lead).reverbSend();
            const float rsB = mixer_.track(MixerBus::Bass).reverbSend();
            const float dsD = mixer_.track(MixerBus::Drums).delaySend();
            const float dsL = mixer_.track(MixerBus::Lead).delaySend();
            const float dsB = mixer_.track(MixerBus::Bass).delaySend();
            bool anyGrpRev = false, anyGrpDly = false;
            for (int g = 0; g < ng; ++g) {
                anyGrpRev = anyGrpRev || mixer_.group(g).reverbSend() > 0.0f;
                anyGrpDly = anyGrpDly || mixer_.group(g).delaySend() > 0.0f;
            }
            const bool anyRev = rsD > 0.0f || rsL > 0.0f || rsB > 0.0f || anyGrpRev;
            const bool anyDly = dsD > 0.0f || dsL > 0.0f || dsB > 0.0f || anyGrpDly;
            if (anyRev) {
                reverbAuxBuf_.assign(n2, 0.0f);
                for (size_t i = 0; i < n2; ++i) {
                    reverbAuxBuf_[i] = stemDrums_[i] * rsD + stemLead_[i] * rsL + stemBass_[i] * rsB;
                }
            }
            if (anyDly) {
                delayAuxBuf_.assign(n2, 0.0f);
                for (size_t i = 0; i < n2; ++i) {
                    delayAuxBuf_[i] = stemDrums_[i] * dsD + stemLead_[i] * dsL + stemBass_[i] * dsB;
                }
            }
            if (ng == 0) {
                // Fast path (no groups): sum the three buses straight into master, tanh-saturated.
                for (size_t i = 0; i < n2; ++i) {
                    const double s = static_cast<double>(stemDrums_[i]) +
                                     static_cast<double>(stemLead_[i]) +
                                     static_cast<double>(stemBass_[i]);
                    out[i] += static_cast<float>(std::tanh(s));
                }
            } else {
                // Routed path: each bus feeds either the master accumulator or a group's buffer, then
                // each group runs its own insert chain and folds into master; tanh saturates the sum.
                // (groupBufs_ were already allocated + zeroed above, and may already hold the audio of
                // instrument channels routed directly to a group — the buses add on top here.)
                masterAcc_.assign(n2, 0.0f);
                float* stems[3] = {stemDrums_.data(), stemLead_.data(), stemBass_.data()};
                for (int b = 0; b < 3; ++b) {
                    const int outIdx = mixer_.track(b).output();
                    float* dst = (outIdx >= 0 && outIdx < ng)
                                     ? groupBufs_[static_cast<size_t>(outIdx)].data()
                                     : masterAcc_.data();
                    for (size_t i = 0; i < n2; ++i) {
                        dst[i] += stems[b][i];
                    }
                }
                for (int g = 0; g < ng; ++g) {
                    mixer_.group(g).process(groupBufs_[static_cast<size_t>(g)].data(), frames,
                                            cfg_.sampleRate);
                    // Solo: a non-audible group does not fold into the mix (its inputs weren't part of
                    // the soloed submix). No-solo path always folds (soloAudible_ is all-1).
                    if (anySolo && !soloAudible_[static_cast<size_t>(3 + g)]) {
                        continue;
                    }
                    // Group aux sends: an audible group feeds the shared reverb/delay returns by its own
                    // send amount (its post-insert output), exactly like the buses do.
                    const float grs = mixer_.group(g).reverbSend();
                    if (grs > 0.0f && !reverbAuxBuf_.empty()) {
                        for (size_t i = 0; i < n2; ++i) {
                            reverbAuxBuf_[i] += groupBufs_[static_cast<size_t>(g)][i] * grs;
                        }
                    }
                    const float gds = mixer_.group(g).delaySend();
                    if (gds > 0.0f && !delayAuxBuf_.empty()) {
                        for (size_t i = 0; i < n2; ++i) {
                            delayAuxBuf_[i] += groupBufs_[static_cast<size_t>(g)][i] * gds;
                        }
                    }
                    // Nested submix: a group may route into a HIGHER-index group (forward-only, so this
                    // ascending pass stays valid); master or any other target folds into the master acc.
                    const int gOut = mixer_.group(g).output();
                    float* gdst = (gOut > g && gOut < ng) ? groupBufs_[static_cast<size_t>(gOut)].data()
                                                          : masterAcc_.data();
                    for (size_t i = 0; i < n2; ++i) {
                        gdst[i] += groupBufs_[static_cast<size_t>(g)][i];
                    }
                }
                for (size_t i = 0; i < n2; ++i) {
                    out[i] += static_cast<float>(std::tanh(static_cast<double>(masterAcc_[i])));
                }
            }
            // Hand the accumulated bus + group aux feeds to the master, whose reverb/delay returns run
            // in mixer_.process() below (which clears them). Set once here so both paths are covered.
            if (anyRev) {
                mixer_.setReverbAux(reverbAuxBuf_);
            }
            if (anyDly) {
                mixer_.setDelayAux(delayAuxBuf_);
            }
        } else {
            sequencer_.render(out, frames, cfg_.sampleRate);
        }
        // Master bus: the mixer's effect chain + master gain + limiter over the stereo output.
        mixer_.delay().updateTempo(sequencer_.bpm());       // sync the delay time to the transport tempo
        mixer_.stereoDelay().updateTempo(sequencer_.bpm()); // sync the dual-delay L/R times too
        mixer_.tremolo().updateTempo(sequencer_.bpm());     // sync the trance-gate rate too
        mixer_.stepGate().updateTempo(sequencer_.bpm());    // sync the 16-step gate pattern length too
        mixer_.beatRepeat().updateTempo(sequencer_.bpm());  // sync the beat-repeat cell length too
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

bool AudioEngine::saveRecording(const std::string& path, std::string* err, bool dither,
                                int bits) const {
    const int ch = cfg_.channels > 0 ? cfg_.channels : 2;
    const int frames = static_cast<int>(recordBuffer_.size()) / ch;
    return writeWav16(path, recordBuffer_.data(), frames, ch, cfg_.sampleRate, err, dither, bits);
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
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, captureCallback, this);
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

void AudioEngine::appendCapturedInput(const float* data, int count) {
    if (!recording_ || data == nullptr || count <= 0) {
        return;
    }
    recordBuffer_.insert(recordBuffer_.end(), data, data + count);
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

void AudioEngine::applyTimelineAutomationClips() {
    if (sequencer_.automationClipCount() == 0) {
        return;
    }
    // Timeline automation clips only sound in clip-song playback (songBar_ is only advanced there).
    if (!(sequencer_.songMode() && sequencer_.songUsesClips() && sequencer_.playing())) {
        return;
    }
    const double pos = sequencer_.songPositionBars();
    for (int i = 0; i < sequencer_.automationClipCount(); ++i) {
        const AutomationClip& c = sequencer_.automationClip(i);
        const int span = c.bars < 1 ? 1 : c.bars;
        if (c.muted || !sequencer_.trackAudible(c.track) ||
            pos < static_cast<double>(c.startBar) ||
            pos >= static_cast<double>(c.startBar + span)) {
            continue;
        }
        // Evaluate the drawn envelope at the normalized position within the clip's span, remap into
        // [lo, hi], and write it onto the target through the shared apply path. Applied AFTER the
        // continuous lanes above, so a placed clip wins on its target during its span; the parameter
        // simply holds its last value once the clip ends.
        const float u = c.unipolarAt(pos); // loop/one-shot handled in AutomationClip (single source)
        const float v = c.lo + u * (c.hi - c.lo);
        Automation::applyTargetValue(*this, static_cast<AutoTarget>(c.target), v);
    }
}

std::vector<float> AudioEngine::renderOfflineTail(double maxTailSeconds, float threshold) {
    std::vector<float> tail;
    if (cfg_.sampleRate <= 0 || cfg_.channels <= 0 || maxTailSeconds <= 0.0) {
        return tail;
    }
    // Keep rendering blocks (the transport keeps advancing, but with the song finished it emits only the
    // ringing reverb/delay/release tails) until a whole block falls below the silence threshold or the
    // max-tail cap is reached — so an export doesn't truncate a decaying tail.
    const int maxFrames = static_cast<int>(maxTailSeconds * static_cast<double>(cfg_.sampleRate));
    constexpr int kBlock = 512;
    std::vector<float> block(static_cast<size_t>(kBlock) * static_cast<size_t>(cfg_.channels), 0.0f);
    int done = 0;
    while (done < maxFrames) {
        const int n = std::min(kBlock, maxFrames - done);
        render(block.data(), n); // render() zeroes then fills, so no pre-clear needed
        const size_t count = static_cast<size_t>(n) * static_cast<size_t>(cfg_.channels);
        float pk = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            pk = std::max(pk, std::fabs(block[i]));
        }
        tail.insert(tail.end(), block.begin(), block.begin() + static_cast<long>(count));
        done += n;
        if (pk < threshold) {
            break; // fully decayed → stop early
        }
    }
    return tail;
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
        applyTimelineAutomationClips(); // 2-D playlist automation clips, gated by song-bar position
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
