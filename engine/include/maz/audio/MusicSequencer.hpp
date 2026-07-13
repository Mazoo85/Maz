#pragma once

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace maz::audio {

// Interactive / adaptive music — Godot's AudioStreamInteractive + AudioStreamPlaylist. Game music is not
// one long file: it is a set of SEGMENTS (intro, explore, combat, boss) that the game switches between
// as the action changes, and the switch has to happen MUSICALLY — on the next beat or the next bar, with
// an optional crossfade — or it sounds like a needle scratch. This sequencer is the scheduler that makes
// that clean: each segment carries a tempo (BPM) and a bar length, one plays, and `transitionTo` queues
// the next one with a mode (Immediate / AtNextBeat / AtNextBar / Crossfade). It runs on a sample clock
// and, at any instant, reports which segment(s) are audible and at what gain — so a mixer just multiplies
// the two candidate streams by those gains. Pure timing + gain math (no decoding), deterministic, so the
// beat/bar boundaries and equal-power crossfade unit-test to the exact sample and drive a golden timeline.
//
// The scheduler owns only timing and gains; the caller supplies the actual audio for each segment index.
// A crossfade is equal-power (outGain = cos, inGain = sin over the fade), so the summed loudness stays
// roughly constant through the transition.

enum class TransitionMode { Immediate, AtNextBeat, AtNextBar, Crossfade };

struct MusicSegment {
    std::string name;
    float bpm = 120.0f;
    int beatsPerBar = 4;
};

// Which segments are audible right now and at what gain. `b` is -1 when only one segment plays.
struct MusicMix {
    int a = -1;
    float aGain = 0.0f;
    int b = -1;
    float bGain = 0.0f;
};

class MusicSequencer {
public:
    explicit MusicSequencer(float sampleRate = 44100.0f) : m_sr(sampleRate) {}

    int addSegment(const std::string& name, float bpm, int beatsPerBar) {
        m_segments.push_back(MusicSegment{name, bpm, beatsPerBar});
        return static_cast<int>(m_segments.size()) - 1;
    }
    int segmentCount() const { return static_cast<int>(m_segments.size()); }
    const std::string& segmentName(int i) const { return m_segments[static_cast<std::size_t>(i)].name; }

    double beatLengthSamples(int seg) const {
        return 60.0 * static_cast<double>(m_sr) / static_cast<double>(m_segments[static_cast<std::size_t>(seg)].bpm);
    }
    double barLengthSamples(int seg) const {
        return beatLengthSamples(seg) *
               static_cast<double>(m_segments[static_cast<std::size_t>(seg)].beatsPerBar);
    }

    // Start a segment immediately (resets the clock's musical grid to this instant).
    void play(int seg) {
        m_current = seg;
        m_segStart = m_clock;
        m_pending = false;
        m_crossActive = false;
    }

    // Queue a transition to `next`. `mode` picks the boundary (now / next beat / next bar / now-with-fade);
    // `crossfadeSec` is the fade length applied at that boundary (0 = hard cut). Crossfade mode implies a
    // fade even if crossfadeSec is small.
    void transitionTo(int next, TransitionMode mode, float crossfadeSec = 0.0f) {
        if (m_current < 0) {
            play(next);
            return;
        }
        m_next = next;
        m_fadeLen = static_cast<double>(crossfadeSec) * static_cast<double>(m_sr);
        const double beatLen = beatLengthSamples(m_current);
        const double barLen = barLengthSamples(m_current);
        const double local = m_clock - m_segStart;
        switch (mode) {
        case TransitionMode::Immediate:
        case TransitionMode::Crossfade:
            m_boundary = m_clock;
            break;
        case TransitionMode::AtNextBeat: {
            const double k = std::floor(local / beatLen) + 1.0;
            m_boundary = m_segStart + k * beatLen;
            break;
        }
        case TransitionMode::AtNextBar: {
            const double k = std::floor(local / barLen) + 1.0;
            m_boundary = m_segStart + k * barLen;
            break;
        }
        }
        m_pending = true;
        m_crossActive = false;
    }

    // Advance the sample clock by `n` samples, processing any scheduled boundary / crossfade completion.
    void advance(double n) {
        const double target = m_clock + n;
        if (m_pending && !m_crossActive && target >= m_boundary) {
            if (m_fadeLen <= 0.0) {
                m_current = m_next;
                m_segStart = m_boundary;
                m_pending = false;
            } else {
                m_crossActive = true;
                m_crossStart = m_boundary;
            }
        }
        if (m_crossActive && target >= m_crossStart + m_fadeLen) {
            m_current = m_next;
            m_segStart = m_crossStart;
            m_crossActive = false;
            m_pending = false;
        }
        m_clock = target;
    }

    // Which segment(s) are audible now and at what (equal-power) gain.
    MusicMix mix() const {
        MusicMix s;
        if (m_crossActive) {
            double t = (m_clock - m_crossStart) / m_fadeLen;
            if (t < 0.0) {
                t = 0.0;
            }
            if (t > 1.0) {
                t = 1.0;
            }
            const double half = 1.5707963267948966;
            s.a = m_current;
            s.aGain = static_cast<float>(std::cos(t * half));
            s.b = m_next;
            s.bGain = static_cast<float>(std::sin(t * half));
        } else {
            s.a = m_current;
            s.aGain = m_current >= 0 ? 1.0f : 0.0f;
            s.b = -1;
            s.bGain = 0.0f;
        }
        return s;
    }

    int current() const { return m_current; }
    bool crossfading() const { return m_crossActive; }
    double clock() const { return m_clock; }
    double segmentStart() const { return m_segStart; }

    // Samples from the current clock to the next bar boundary of the current segment.
    double samplesToNextBar() const {
        if (m_current < 0) {
            return 0.0;
        }
        const double barLen = barLengthSamples(m_current);
        const double local = m_clock - m_segStart;
        const double k = std::floor(local / barLen) + 1.0;
        return (m_segStart + k * barLen) - m_clock;
    }

private:
    float m_sr = 44100.0f;
    std::vector<MusicSegment> m_segments;
    int m_current = -1;
    int m_next = -1;
    double m_clock = 0.0;
    double m_segStart = 0.0;
    bool m_pending = false;
    bool m_crossActive = false;
    double m_boundary = 0.0;
    double m_crossStart = 0.0;
    double m_fadeLen = 0.0;
};

} // namespace maz::audio
