#pragma once

#include "maz/core/Jobs.hpp"
#include "maz/ecs/World.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

// maz::ecs::Scheduler — an ordered system runner with optional intra-phase parallelism. Systems are
// registered into named PHASES (e.g. Input < Simulation < LateUpdate) and, within a phase, an
// integer order; run() executes them deterministically phase-by-phase, order-by-order. Systems the
// caller marks parallelSafe (they touch disjoint data) can run concurrently within their phase via
// a core::JobSystem, with a hard barrier between phases so ordering across phases is always exact.
//
// This is the "ordered + parallel system execution" an ECS scheduler needs — Godot runs node
// _process callbacks single-threaded, so deterministic phase ordering WITH opt-in parallelism
// inside a phase is a capability beyond it. Header-only; the JobSystem is the caller's (reused
// across frames).
namespace maz::ecs {

class Scheduler {
  public:
    using SystemFn = std::function<void(World&)>;

    struct System {
        std::string name;
        SystemFn fn;
        int phase = 0;             // lower phases run first
        int order = 0;             // tie-breaker within a phase (lower first)
        bool parallelSafe = false; // may run concurrently with other parallelSafe systems in-phase
    };

    // Register a system. Returns its index (handy for tests / introspection).
    size_t add(std::string name, SystemFn fn, int phase = 0, int order = 0,
               bool parallelSafe = false) {
        m_systems.push_back(System{std::move(name), std::move(fn), phase, order, parallelSafe});
        m_sorted = false;
        return m_systems.size() - 1;
    }

    size_t count() const { return m_systems.size(); }
    const std::vector<System>& systems() const { return m_systems; }

    // Deterministic order the systems will run in (stable within equal phase+order = insertion
    // order).
    std::vector<std::string> runOrder() {
        ensureSorted();
        std::vector<std::string> out;
        out.reserve(m_order.size());
        for (size_t i : m_order)
            out.push_back(m_systems[i].name);
        return out;
    }

    // Run every system serially in (phase, order, insertion) order.
    void run(World& world) {
        ensureSorted();
        for (size_t i : m_order) {
            m_systems[i].fn(world);
        }
    }

    // Run phase-by-phase: within each phase, parallelSafe systems run concurrently on `jobs` while
    // the non-parallel ones run serially in order; a barrier separates phases. Phase ORDER is
    // always honoured — only systems inside the same phase overlap.
    void runParallel(World& world, core::JobSystem& jobs) {
        ensureSorted();
        size_t i = 0;
        while (i < m_order.size()) {
            const int phase = m_systems[m_order[i]].phase;
            // Collect this phase's systems.
            std::vector<size_t> serial, parallel;
            size_t j = i;
            while (j < m_order.size() && m_systems[m_order[j]].phase == phase) {
                if (m_systems[m_order[j]].parallelSafe)
                    parallel.push_back(m_order[j]);
                else
                    serial.push_back(m_order[j]);
                ++j;
            }
            // Kick the parallel group, run the serial ones meanwhile, then join.
            std::vector<std::future<void>> futures;
            futures.reserve(parallel.size());
            for (size_t idx : parallel) {
                System* s = &m_systems[idx];
                futures.push_back(jobs.submit([s, &world] { s->fn(world); }));
            }
            for (size_t idx : serial) {
                m_systems[idx].fn(world);
            }
            for (std::future<void>& f : futures)
                f.get(); // barrier: wait out the phase
            i = j;
        }
    }

  private:
    void ensureSorted() {
        if (m_sorted)
            return;
        m_order.resize(m_systems.size());
        for (size_t i = 0; i < m_order.size(); ++i)
            m_order[i] = i;
        std::stable_sort(m_order.begin(), m_order.end(), [this](size_t a, size_t b) {
            if (m_systems[a].phase != m_systems[b].phase)
                return m_systems[a].phase < m_systems[b].phase;
            return m_systems[a].order < m_systems[b].order;
        });
        m_sorted = true;
    }

    std::vector<System> m_systems;
    std::vector<size_t> m_order;
    bool m_sorted = false;
};

} // namespace maz::ecs
