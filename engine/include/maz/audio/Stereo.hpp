#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::audio {

// Stereo processors — Godot's AudioEffectStereoEnhance and AudioEffectPanner. Maz's DSP so far is mono
// (one float in, one float out); this adds the small stereo-aware layer that games use to place and
// widen a sound across the two speakers. A StereoFrame is one interleaved L/R sample pair. Two
// processors operate on it: StereoEnhance controls the perceived WIDTH of the stereo image (from mono at
// the center out to a wide, enveloping field) via mid/side scaling plus an optional Haas time-offset;
// and Panner shifts the stereo BALANCE left or right with a constant-power law. Both are pure per-frame
// math — no device, no threads — so they unit-test exactly (width 0 collapses to mono; a centered
// balance is unchanged; a hard pan silences one side) and drive a golden goniometer (vectorscope) view.
//
// Mid/side: mid = (L+R)/2 is the mono-compatible center, side = (L-R)/2 is the stereo difference.
// Scaling `side` by a width factor narrows (<1, toward mono) or widens (>1) the image; a truly mono
// input (L==R) has zero side, so width cannot invent width that isn't there — it stays mono, which is
// correct.
//
// Scope note (honest): these are standalone stereo processors + a StereoFrame type. Rewiring the whole
// BusGraph/effect chain to carry stereo end-to-end (so every effect is stereo-aware) is the larger
// follow-up; the mono effect path is unchanged.

struct StereoFrame {
    float left = 0.0f;
    float right = 0.0f;
};

inline float stereoMid(const StereoFrame& f) { return 0.5f * (f.left + f.right); }
inline float stereoSide(const StereoFrame& f) { return 0.5f * (f.left - f.right); }
inline StereoFrame fromMidSide(float m, float s) { return StereoFrame{m + s, m - s}; }

// Stereo widener — Godot's AudioEffectStereoEnhance. `width` scales the side (difference) signal:
// 0 = mono (fully collapsed), 1 = unchanged, >1 = wider. `haasMs` optionally delays the right channel
// by a few milliseconds (the Haas/precedence effect) to enhance the sense of width on near-mono material.
struct StereoEnhance {
    float width = 1.0f;
    float haasMs = 0.0f;
    float sampleRate = 44100.0f;

    void configure(float width_, float haasMs_, float sr) {
        width = width_;
        haasMs = haasMs_;
        sampleRate = sr;
        rebuild();
    }

    void rebuild() {
        int d = static_cast<int>(haasMs * sampleRate / 1000.0f + 0.5f);
        if (d < 0) {
            d = 0;
        }
        m_delayLen = d;
        m_rbuf.assign(static_cast<std::size_t>(d > 0 ? d : 1), 0.0f);
        m_ridx = 0;
    }

    StereoFrame process(const StereoFrame& in) {
        if (m_rbuf.empty()) {
            rebuild();
        }
        float r = in.right;
        if (m_delayLen > 0) {
            const float dr = m_rbuf[static_cast<std::size_t>(m_ridx)];
            m_rbuf[static_cast<std::size_t>(m_ridx)] = in.right;
            m_ridx = (m_ridx + 1) % m_delayLen;
            r = dr;
        }
        const float m = 0.5f * (in.left + r);
        const float s = 0.5f * (in.left - r) * width;
        return StereoFrame{m + s, m - s};
    }

    void reset() {
        std::fill(m_rbuf.begin(), m_rbuf.end(), 0.0f);
        m_ridx = 0;
    }

private:
    int m_delayLen = 0;
    int m_ridx = 0;
    std::vector<float> m_rbuf;
};

// Stereo balance panner — Godot's AudioEffectPanner. `pan` in [-1, +1] shifts the balance: at 0 both
// channels pass unchanged; panning toward one side leaves that side full and fades the opposite side
// with a constant-power (cosine) taper, so a hard pan (+/-1) silences the far channel.
struct Panner {
    float pan = 0.0f;

    StereoFrame process(const StereoFrame& in) const {
        float p = pan;
        if (p < -1.0f) {
            p = -1.0f;
        }
        if (p > 1.0f) {
            p = 1.0f;
        }
        const float half = 1.5707963267948966f;
        const float lg = p <= 0.0f ? 1.0f : std::cos(p * half);
        const float rg = p >= 0.0f ? 1.0f : std::cos(-p * half);
        return StereoFrame{in.left * lg, in.right * rg};
    }
    void reset() {}
};

} // namespace maz::audio
