#pragma once

#include <cstddef>
#include <vector>

// maz::net interpolation buffer — the client-side smoothing that makes networked motion look
// fluid despite arriving in discrete ticks. The server sends state ~20-60 times a second; a client
// that snapped to each packet would jitter. Instead the client renders slightly IN THE PAST (by an
// "interpolation delay" of a tick or two) and, at each frame, LERPs between the two buffered
// snapshots that bracket the render time — so motion is continuous even though data is discrete.
// This is Valve-style entity interpolation, the same idea behind Godot's MultiplayerSynchronizer
// interpolation. Optional extrapolation extends the last known velocity a bounded amount when a
// packet is late. Pure, header-only, unit-tested — templated on any value with +, -, and *float
// (a scalar, or a math::vec2/vec3). No sockets or clocks here; the caller supplies timestamps.
namespace maz::net {

// Linear blend used by the buffer. Works for float/double and for vector types that support the
// three operators. `t` is expected in [0,1] but is not clamped (callers pass a bracketed fraction).
template <typename T>
inline T lerpSample(const T& a, const T& b, float t) {
    return a + (b - a) * t;
}

template <typename T>
struct TimedSample {
    double time = 0.0; // seconds (any monotonic timeline the caller chooses)
    T value{};
};

// A bounded, time-ordered history of state samples. Insert as packets arrive; sample() reconstructs
// the value at any render time by interpolating the bracketing pair. Keeps at most `maxSamples`,
// dropping the oldest — a couple hundred ms of history is plenty for interpolation.
template <typename T>
class InterpolationBuffer {
  public:
    explicit InterpolationBuffer(std::size_t maxSamples = 32)
        : m_max(maxSamples < 2 ? 2 : maxSamples) {}

    // Insert a sample. The common case (newest) is O(1); out-of-order packets are placed in time
    // order, and a sample at an existing time replaces it. Oldest samples drop past capacity.
    void insert(double time, const T& value) {
        if (m_samples.empty() || time > m_samples.back().time) {
            m_samples.push_back({time, value});
        } else {
            std::size_t i = m_samples.size();
            while (i > 0 && m_samples[i - 1].time > time) {
                --i;
            }
            if (i > 0 && m_samples[i - 1].time == time) {
                m_samples[i - 1].value = value;
            } else {
                m_samples.insert(m_samples.begin() + static_cast<std::ptrdiff_t>(i), {time, value});
            }
        }
        while (m_samples.size() > m_max) {
            m_samples.erase(m_samples.begin());
        }
    }

    // Interpolate the value at `renderTime`. Before the oldest sample clamps to the oldest; after
    // the newest clamps to the newest (no extrapolation — use sampleExtrapolated for that); in
    // between, lerps the bracketing pair. Returns false only when the buffer is empty.
    bool sample(double renderTime, T& out) const {
        if (m_samples.empty()) {
            return false;
        }
        if (renderTime <= m_samples.front().time) {
            out = m_samples.front().value;
            return true;
        }
        if (renderTime >= m_samples.back().time) {
            out = m_samples.back().value;
            return true;
        }
        for (std::size_t i = 1; i < m_samples.size(); ++i) {
            if (m_samples[i].time >= renderTime) {
                const TimedSample<T>& a = m_samples[i - 1];
                const TimedSample<T>& b = m_samples[i];
                const double span = b.time - a.time;
                const float t = span > 0.0 ? static_cast<float>((renderTime - a.time) / span) : 0.0f;
                out = lerpSample(a.value, b.value, t);
                return true;
            }
        }
        out = m_samples.back().value;
        return true;
    }

    // Like sample(), but when `renderTime` is past the newest sample, extend the last known
    // velocity (from the two newest samples) forward — clamped so we never guess more than
    // `maxExtrapolate` seconds ahead. Hides a single late packet without runaway drift.
    bool sampleExtrapolated(double renderTime, double maxExtrapolate, T& out) const {
        if (m_samples.empty()) {
            return false;
        }
        if (renderTime <= m_samples.back().time || m_samples.size() < 2) {
            return sample(renderTime, out);
        }
        const TimedSample<T>& a = m_samples[m_samples.size() - 2];
        const TimedSample<T>& b = m_samples[m_samples.size() - 1];
        const double span = b.time - a.time;
        if (span <= 0.0) {
            out = b.value;
            return true;
        }
        double ahead = renderTime - b.time;
        if (ahead > maxExtrapolate) {
            ahead = maxExtrapolate;
        }
        // velocity = (b - a) / span; out = b + velocity * ahead
        const float k = static_cast<float>(ahead / span);
        out = b.value + (b.value - a.value) * k;
        return true;
    }

    std::size_t size() const { return m_samples.size(); }
    bool empty() const { return m_samples.empty(); }
    void clear() { m_samples.clear(); }
    double oldestTime() const { return m_samples.front().time; }
    double newestTime() const { return m_samples.back().time; }

  private:
    std::vector<TimedSample<T>> m_samples;
    std::size_t m_max;
};

} // namespace maz::net
