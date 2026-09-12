#pragma once

#include <cstddef>
#include <deque>

// maz::net::ClockSync — estimate the CLOCK OFFSET between this machine and a remote peer, and the round-trip
// time, from timestamped ping/pong exchanges (the NTP algorithm). Everything else in net/ that "renders in
// the past" (Interpolation) or predicts on server time (Prediction) needs a shared notion of time, but a
// client's clock and the server's clock drift and start at different values — so the client must learn how
// far ahead/behind the server is and how long a round trip takes. Each exchange yields four timestamps: t0
// (client sends), t1 (server receives), t2 (server sends reply), t3 (client receives). NTP then gives
//   offset = ((t1 - t0) + (t2 - t3)) / 2      (server clock minus client clock)
//   delay  = (t3 - t0) - (t2 - t1)            (round-trip time, excluding server processing)
// Queuing jitter corrupts individual samples, so — like NTP's clock filter — the best estimate comes from
// the sample with the SMALLEST delay (least affected by congestion); a smoothed offset is also exposed.
// Times are plain doubles in one consistent unit (seconds or ms); no clock or socket here — the caller
// supplies the stamps. Deterministic, header-only, std-only. Godot's high-level multiplayer hides this.
namespace maz::net {

class ClockSync {
public:
    explicit ClockSync(std::size_t window = 8) : m_window(window == 0 ? 1 : window) {}

    // Record one ping/pong exchange. All four timestamps are in the same unit; t0/t3 are on the client
    // clock, t1/t2 on the server clock.
    void addSample(double t0, double t1, double t2, double t3) {
        const double offset = ((t1 - t0) + (t2 - t3)) * 0.5;
        const double delay = (t3 - t0) - (t2 - t1);
        m_samples.push_back(Sample{offset, delay < 0.0 ? 0.0 : delay});
        while (m_samples.size() > m_window) {
            m_samples.pop_front();
        }
        // Track the min-delay (best) sample in the current window.
        m_best = m_samples.front();
        for (const Sample& s : m_samples) {
            if (s.delay < m_best.delay) {
                m_best = s;
            }
        }
        // Exponential smoothing of the offset for a stable readout.
        if (m_count == 0) {
            m_smoothed = offset;
        } else {
            m_smoothed += (offset - m_smoothed) * m_alpha;
        }
        ++m_count;
    }

    bool ready() const { return m_count > 0; }

    // Best offset (server clock minus client clock) — from the least-jittered sample in the window.
    double offset() const { return m_best.offset; }

    // Round-trip time of the best sample.
    double rtt() const { return m_best.delay; }

    // One-way latency estimate (half the best RTT).
    double latency() const { return m_best.delay * 0.5; }

    // EMA-smoothed offset (steadier than offset(), lags a little).
    double smoothedOffset() const { return m_smoothed; }

    // Convert a local timestamp to the estimated server clock.
    double toServerTime(double localTime) const { return localTime + m_best.offset; }
    // Convert a server timestamp to the estimated local clock.
    double toLocalTime(double serverTime) const { return serverTime - m_best.offset; }

    std::size_t sampleCount() const { return m_samples.size(); }

    // Smoothing factor for smoothedOffset (0..1; higher reacts faster). Default 0.25.
    void setSmoothing(double alpha) { m_alpha = alpha < 0.0 ? 0.0 : (alpha > 1.0 ? 1.0 : alpha); }

    void clear() {
        m_samples.clear();
        m_count = 0;
        m_smoothed = 0.0;
        m_best = Sample{0.0, 0.0};
    }

private:
    struct Sample {
        double offset = 0.0;
        double delay = 0.0;
    };

    std::size_t m_window;
    std::deque<Sample> m_samples;
    Sample m_best{0.0, 0.0};
    double m_smoothed = 0.0;
    double m_alpha = 0.25;
    long long m_count = 0;
};

} // namespace maz::net
