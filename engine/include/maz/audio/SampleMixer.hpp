#pragma once

#include "maz/audio/Wav.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace maz::audio {

// Sample-playback mixer — Godot's AudioStreamPlayer over an AudioStreamWAV. Until now Maz could *decode* a
// .wav into float samples (audio::Wav) and *synthesize* procedural tones (audio::Audio), but there was no
// way to take a decoded clip and actually PLAY it back through a mixer — start it as a voice, set its gain
// and stereo pan, loop it, or pitch-shift it, and have several such voices summed into one output buffer.
// That runtime is this header. It is a self-contained, offline (buffer-in / buffer-out) stereo mixer: it
// never touches the SDL audio device, so it unit-tests headlessly and byte-deterministically, yet it feeds
// exactly the interleaved-float format a real device callback wants. The app (or a future device backend)
// owns the callback and simply asks the mixer to fill each block.
//
// A voice references a WavData clip by pointer (the caller owns the clip and must outlive the voice). Reads
// are linearly interpolated so pitch/speed and sample-rate conversion are smooth; a mono clip is panned
// into both output channels, a stereo clip maps its two channels straight through (with pan attenuating the
// opposite side). Non-looping voices deactivate automatically when they run past the end.

struct SampleVoice {
    const WavData* clip = nullptr;
    double position = 0.0; // playhead in FRAMES (fractional for interpolation)
    float gain = 1.0f;
    float pan = 0.0f;   // -1 = full left, 0 = center, +1 = full right
    float speed = 1.0f; // 1 = clip's native pitch; 2 = an octave up / twice as fast
    bool loop = false;
    bool active = false;
};

namespace detail {

// Read one interpolated sample from channel `ch` at fractional frame position `pos`. Out-of-range reads
// return 0 so the edges fade rather than click.
inline float sampleAt(const WavData& clip, double pos, std::uint16_t ch) {
    const std::size_t frames = clip.frameCount();
    if (frames == 0 || ch >= clip.channels) {
        return 0.0f;
    }
    if (pos < 0.0) {
        pos = 0.0;
    }
    const std::size_t i0 = static_cast<std::size_t>(pos);
    if (i0 >= frames) {
        return 0.0f;
    }
    const std::size_t i1 = (i0 + 1 < frames) ? i0 + 1 : i0;
    const float frac = static_cast<float>(pos - static_cast<double>(i0));
    const float s0 = clip.samples[i0 * clip.channels + ch];
    const float s1 = clip.samples[i1 * clip.channels + ch];
    return s0 + (s1 - s0) * frac;
}

} // namespace detail

class SampleMixer {
public:
    // Start a clip playing. Returns a voice id (index) usable with stop(), or -1 if the clip is empty. The
    // clip must outlive the voice (the mixer stores a pointer, not a copy).
    int play(const WavData& clip, float gain = 1.0f, float pan = 0.0f, bool loop = false, float speed = 1.0f) {
        if (clip.frameCount() == 0 || clip.channels == 0) {
            return -1;
        }
        SampleVoice v;
        v.clip = &clip;
        v.position = 0.0;
        v.gain = gain;
        v.pan = pan < -1.0f ? -1.0f : (pan > 1.0f ? 1.0f : pan);
        v.speed = speed;
        v.loop = loop;
        v.active = true;

        // Reuse a dead slot if one exists, else append.
        for (std::size_t i = 0; i < m_voices.size(); ++i) {
            if (!m_voices[i].active) {
                m_voices[i] = v;
                return static_cast<int>(i);
            }
        }
        m_voices.push_back(v);
        return static_cast<int>(m_voices.size() - 1);
    }

    // Silence and free a voice. Out-of-range / already-dead ids are ignored.
    void stop(int voice) {
        if (voice >= 0 && static_cast<std::size_t>(voice) < m_voices.size()) {
            m_voices[static_cast<std::size_t>(voice)].active = false;
        }
    }

    void clear() { m_voices.clear(); }

    std::size_t activeVoices() const {
        std::size_t n = 0;
        for (const SampleVoice& v : m_voices) {
            if (v.active) {
                ++n;
            }
        }
        return n;
    }

    // Fill `out` with `frames` interleaved stereo frames (out[2*i]=L, out[2*i+1]=R) at `outRate` Hz,
    // summing every active voice. The buffer is zeroed first, so callers can hand over any scratch memory.
    void mix(float* out, std::size_t frames, std::uint32_t outRate) {
        if (!out || frames == 0) {
            return;
        }
        for (std::size_t i = 0; i < frames * 2; ++i) {
            out[i] = 0.0f;
        }
        if (outRate == 0) {
            return;
        }

        for (SampleVoice& v : m_voices) {
            if (!v.active || !v.clip) {
                continue;
            }
            const WavData& clip = *v.clip;
            const std::size_t clipFrames = clip.frameCount();
            if (clipFrames == 0) {
                v.active = false;
                continue;
            }
            // Frames of the clip consumed per output frame (native-rate ratio × pitch).
            const double step =
                (static_cast<double>(clip.sampleRate) / static_cast<double>(outRate)) *
                static_cast<double>(v.speed);
            // Equal-power-ish linear pan: pan −1 mutes right, +1 mutes left.
            const float lGain = v.gain * (v.pan <= 0.0f ? 1.0f : 1.0f - v.pan);
            const float rGain = v.gain * (v.pan >= 0.0f ? 1.0f : 1.0f + v.pan);
            const bool stereo = clip.channels >= 2;

            for (std::size_t f = 0; f < frames; ++f) {
                if (v.position >= static_cast<double>(clipFrames)) {
                    if (v.loop) {
                        // Wrap, preserving the fractional overshoot.
                        v.position -= static_cast<double>(clipFrames) *
                                      static_cast<double>(
                                          static_cast<std::size_t>(v.position) / clipFrames);
                        if (v.position >= static_cast<double>(clipFrames)) {
                            v.position = 0.0;
                        }
                    } else {
                        v.active = false;
                        break;
                    }
                }
                const float srcL = detail::sampleAt(clip, v.position, 0);
                const float srcR = stereo ? detail::sampleAt(clip, v.position, 1) : srcL;
                out[f * 2] += srcL * lGain;
                out[f * 2 + 1] += srcR * rGain;
                v.position += step;
            }
        }
    }

private:
    std::vector<SampleVoice> m_voices;
};

} // namespace maz::audio
