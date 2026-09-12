#pragma once

#include "maz/audio/Dsp.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::audio {

// Pitch shifter — Godot's AudioEffectPitchShift. Raises or lowers the pitch of a signal WITHOUT changing
// its speed (a monster voice an octave down, a chipmunk an octave up, a pickup jingle nudged up a few
// semitones). This is the classic time-domain GRANULAR / overlap-add shifter: recent input is kept in a
// ring buffer and read back through TWO overlapping "grains" whose read pointer moves at the pitch ratio
// relative to the write pointer; the two grains are crossfaded with a Hann window so that as one grain
// runs off the end of the buffer it fades out while the other (half a window out of phase) fades in —
// keeping the output continuous and the buffer from over/under-running. Cheaper and more deterministic
// than a phase vocoder. Mono float->float, so it drops straight into a Bus/Effect chain
// (PitchShiftEffect). Deterministic -> it unit-tests (unity ratio passes through delayed; a shifted
// sine's dominant period scales by the ratio) and drives a golden.
//
// Scope note (honest): a granular shifter trades some quality for simplicity — on very wide shifts or
// transient-heavy material it has mild warble/smearing (as Godot's does). A phase-vocoder or
// formant-preserving path is the higher-fidelity follow-up.

struct PitchShifter {
    float pitchScale = 1.0f; // 2 = up an octave, 0.5 = down an octave
    int grainSamples = 1024; // grain / window size (larger = smoother, more latency)

    void setSemitones(float semis) { pitchScale = std::pow(2.0f, semis / 12.0f); }

    void reset() {
        rebuild();
    }

    float process(float x) {
        if (m_buf.empty()) {
            rebuild();
        }
        const int g = m_grain;
        m_buf[static_cast<std::size_t>(m_write)] = x;

        const float ph1 = m_phase - std::floor(m_phase);
        float ph2 = m_phase + 0.5f;
        ph2 -= std::floor(ph2);

        const float twoPi = 6.28318530718f;
        const float w1 = 0.5f - 0.5f * std::cos(twoPi * ph1);
        const float w2 = 0.5f - 0.5f * std::cos(twoPi * ph2);
        const float d1 = ph1 * static_cast<float>(g - 1);
        const float d2 = ph2 * static_cast<float>(g - 1);
        const float out = readAt(static_cast<float>(m_write) - d1) * w1 +
                          readAt(static_cast<float>(m_write) - d2) * w2;

        // Read pointer moves at `pitchScale` relative to the write pointer: phaseInc = (1-ratio)/grain.
        m_phase += (1.0f - pitchScale) / static_cast<float>(g);
        m_phase -= std::floor(m_phase);
        m_write = (m_write + 1) % m_bufLen;
        return out;
    }

    void configure(float ratio, int grain) {
        pitchScale = ratio;
        grainSamples = grain;
        rebuild();
    }

private:
    void rebuild() {
        m_grain = grainSamples < 16 ? 16 : grainSamples;
        m_bufLen = m_grain * 2;
        m_buf.assign(static_cast<std::size_t>(m_bufLen), 0.0f);
        m_write = 0;
        m_phase = 0.0f;
    }

    // Linearly-interpolated read from the ring buffer at (possibly negative, fractional) position `pos`.
    float readAt(float pos) const {
        float bp = pos;
        const float len = static_cast<float>(m_bufLen);
        while (bp < 0.0f) {
            bp += len;
        }
        while (bp >= len) {
            bp -= len;
        }
        const int i0 = static_cast<int>(bp);
        const int i1 = (i0 + 1) % m_bufLen;
        const float frac = bp - static_cast<float>(i0);
        return m_buf[static_cast<std::size_t>(i0)] * (1.0f - frac) +
               m_buf[static_cast<std::size_t>(i1)] * frac;
    }

    std::vector<float> m_buf;
    int m_grain = 1024;
    int m_bufLen = 2048;
    int m_write = 0;
    float m_phase = 0.0f;
};

struct PitchShiftEffect : Effect {
    PitchShifter shifter;
    explicit PitchShiftEffect(const PitchShifter& s) : shifter(s) { shifter.reset(); }
    float process(float x) override { return shifter.process(x); }
    void reset() override { shifter.reset(); }
};

} // namespace maz::audio
