#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace maz::core {

// Hierarchical CPU profiler: named, nestable timing zones that answer "where did the frame go?". A
// zone is opened with begin(name) and closed with end(); zones nest, so the tree records both
// inclusive time (the whole span) and self time (inclusive minus the direct children) — the two
// numbers that actually locate a hotspot. Per frame each distinct zone name aggregates its call
// count, inclusive, and self microseconds; across frames an exponential moving average smooths the
// display so the numbers are readable instead of flickering.
//
// The core is time-source-agnostic: begin/end take a monotonic microsecond timestamp, so tests and
// deterministic demos feed synthetic timestamps (no wall clock) and get reproducible output, while
// real code uses nowMicros() (steady_clock) or the ScopedZone RAII helper. Header-only, std only.

inline uint64_t nowMicros() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

class Profiler {
public:
    struct Zone {
        std::string name;
        int depth = 0;
        uint64_t calls = 0;
        uint64_t inclusiveUs = 0; // this frame: whole span (self + children)
        uint64_t selfUs = 0;      // this frame: inclusive minus direct children
        double smoothedMs = 0.0;  // EMA of inclusive time (ms) across frames
        uint64_t childUs = 0;     // internal: accumulated direct-child time this frame
    };

    explicit Profiler(double emaAlpha = 0.2) : m_alpha(emaAlpha) {}

    // Start a new frame: clear per-frame accumulation (smoothed stats persist).
    void beginFrame() {
        for (Zone& z : m_zones) {
            z.calls = 0;
            z.inclusiveUs = 0;
            z.selfUs = 0;
            z.childUs = 0;
        }
        m_stack.clear();
    }

    // Open a zone at the current timestamp (microseconds).
    void begin(const std::string& name, uint64_t nowUs) {
        const int depth = static_cast<int>(m_stack.size());
        const size_t idx = findOrAdd(name, depth);
        m_stack.push_back(Frame{idx, nowUs});
    }

    // Close the most-recently-opened zone at the current timestamp.
    void end(uint64_t nowUs) {
        if (m_stack.empty()) return;
        const Frame f = m_stack.back();
        m_stack.pop_back();
        const uint64_t dur = nowUs >= f.startUs ? nowUs - f.startUs : 0;
        Zone& z = m_zones[f.idx];
        z.inclusiveUs += dur;
        z.calls += 1;
        // This zone's whole span counts as "child time" for whatever encloses it.
        if (!m_stack.empty()) m_zones[m_stack.back().idx].childUs += dur;
    }

    // Finish the frame: compute self time and fold inclusive time into the moving average.
    void endFrame() {
        for (Zone& z : m_zones) {
            z.selfUs = z.inclusiveUs >= z.childUs ? z.inclusiveUs - z.childUs : 0;
            const double incMs = static_cast<double>(z.inclusiveUs) / 1000.0;
            z.smoothedMs = z.smoothedMs <= 0.0 ? incMs : z.smoothedMs + m_alpha * (incMs - z.smoothedMs);
        }
    }

    const std::vector<Zone>& zones() const { return m_zones; }
    bool empty() const { return m_zones.empty(); }

    const Zone* find(const std::string& name) const {
        for (const Zone& z : m_zones)
            if (z.name == name) return &z;
        return nullptr;
    }

    // RAII: open a zone on construction (real steady_clock), close it on scope exit.
    //   { Profiler::ScopedZone _(prof, "physics"); ...work... }
    class ScopedZone {
    public:
        ScopedZone(Profiler& p, const std::string& name) : m_p(p) { m_p.begin(name, nowMicros()); }
        ~ScopedZone() { m_p.end(nowMicros()); }
        ScopedZone(const ScopedZone&) = delete;
        ScopedZone& operator=(const ScopedZone&) = delete;

    private:
        Profiler& m_p;
    };

private:
    struct Frame {
        size_t idx;
        uint64_t startUs;
    };

    size_t findOrAdd(const std::string& name, int depth) {
        for (size_t i = 0; i < m_zones.size(); ++i)
            if (m_zones[i].name == name) return i;
        Zone z;
        z.name = name;
        z.depth = depth;
        m_zones.push_back(std::move(z));
        return m_zones.size() - 1;
    }

    double m_alpha;
    std::vector<Zone> m_zones;       // first-seen order (stable for display)
    std::vector<Frame> m_stack;      // currently-open zones
};

} // namespace maz::core
