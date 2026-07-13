#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>  // std::min/std::max
#include <chrono>     // steady_clock, duration_cast, nanoseconds
#include <map>

#include "maz/core/StringId.hpp"

namespace maz::core {

// A lightweight CPU profiler — the Godot built-in-profiler analog. Named timing
// scopes are keyed by StringId, each aggregating a call count plus total/min/max
// nanoseconds. Scopes live in std::map<StringId, Stat> ordered by StringId's
// operator< (StringId has operator< but no std::hash). std::map nodes are
// address-stable, so a const Stat* returned by get() survives further record()
// of OTHER names; reset(), however, clears the map and invalidates every Stat*.
//
// Min-seeding rule: the FIRST sample for a name seeds min=max=nanos (NOT
// min=0) — otherwise minNs would stay stuck at the zero-initialized value.
//
// NOT thread-safe: real multi-threaded profiling needs per-thread stat maps
// merged at report time — a future refinement. ScopedTimer lifetime: the
// Profiler must outlive every ScopedTimer targeting it.
//
// Future refinements (not built here): a MAZ_PROFILE_SCOPE(profiler, name)
// convenience macro, and per-frame stat buckets plus a frame marker.

// Aggregated timing for one named scope. All fields are uint64 nanoseconds;
// totalNs as uint64 ns spans ~584 years, so overflow is a non-issue.
struct Stat {
    uint64_t count   = 0;
    uint64_t totalNs = 0;
    uint64_t minNs   = 0;  // meaningless until count > 0; seeded on the first sample
    uint64_t maxNs   = 0;
    // No avgNs() — callers compute totalNs/count, keeping the integer-only test
    // contract clean. A trivial future add if wanted.
};

class Profiler {
public:
    // Record one timing sample of nanos against the scope named name.
    void record(StringId name, uint64_t nanos) {
        Stat& s = m_stats[name];  // inserts a zero-initialized Stat if absent
        if (s.count == 0) {
            s.minNs = nanos;  // FIRST sample seeds both bounds — the min-seeding rule
            s.maxNs = nanos;
        } else {
            s.minNs = std::min(s.minNs, nanos);
            s.maxNs = std::max(s.maxNs, nanos);
        }
        ++s.count;
        s.totalNs += nanos;
    }

    // The aggregated Stat for name, or nullptr if the scope was never recorded.
    const Stat* get(StringId name) const {
        auto it = m_stats.find(name);
        return it == m_stats.end() ? nullptr : &it->second;
    }

    bool has(StringId name) const { return m_stats.find(name) != m_stats.end(); }

    std::size_t scopeCount() const { return m_stats.size(); }

    void reset() { m_stats.clear(); }

    // Read-only iteration for reporting: exposes the map for dumping all scopes
    // without a copy (like SparseSet::keys()/values()).
    const std::map<StringId, Stat>& stats() const { return m_stats; }

private:
    std::map<StringId, Stat> m_stats;
};

// RAII timing scope: records the elapsed steady_clock time into the target
// Profiler when it goes out of scope. Always use as a local — the Profiler must
// outlive the timer.
class ScopedTimer {
public:
    ScopedTimer(Profiler& profiler, StringId name)
        : m_profiler(&profiler), m_name(name), m_start(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        const auto end = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();  // signed rep (long long)
        const uint64_t ns = delta < 0 ? uint64_t{0} : static_cast<uint64_t>(delta);  // steady_clock monotonic so delta>=0; clamp defensively, explicit cast for -Wconversion
        m_profiler->record(m_name, ns);
    }

    // Non-copyable, non-movable: a scope timer is always a local; duplicating
    // would double-record.
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    Profiler* m_profiler;  // stored as pointer; must outlive the timer
    StringId  m_name;
    std::chrono::steady_clock::time_point m_start;
};

} // namespace maz::core
