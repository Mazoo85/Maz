#pragma once

#include "maz/core/Profiler.hpp"

#include <string>
#include <unordered_map>
#include <vector>

// maz::core::PerfBudget — performance budgets layered on the hierarchical Profiler. A game sets a
// per-section time budget ("physics ≤ 4 ms", "render ≤ 8 ms") and a whole-frame budget ("≤ 16.6 ms
// for 60 fps"); after each frame, check() compares the profiler's measured times against those
// budgets and reports every OVERAGE — which section blew its budget, by how much. That turns the
// profiler from a passive readout into an active gate: assert it in tests, log it in CI, or flash a
// warning on the debug overlay the instant a subsystem regresses.
//
// This is the "performance budgets + profiling dashboards" backbone — Godot surfaces frame timings
// but has no formal budget/alert layer, so budget-driven regression detection is a capability
// beyond it. check() reads either this frame's inclusive time or the profiler's smoothed (EMA)
// time, so you can alert on a single spike or only on a sustained regression. Header-only,
// deterministic (pure comparison over profiler data), no GPU.
namespace maz::core {

class PerfBudget {
  public:
    struct Overage {
        std::string name;
        double budgetMs = 0.0;
        double actualMs = 0.0;
        double overMs = 0.0; // actual - budget (always > 0 for a reported overage)
    };

    struct Report {
        std::vector<Overage> zones; // per-section overages, in the order budgets were set
        bool frameOverBudget = false;
        double frameMs = 0.0;
        double frameBudgetMs = 0.0; // 0 = no frame budget set
        double frameOverMs = 0.0;

        bool anyOverage() const { return frameOverBudget || !zones.empty(); }
    };

    // Budget a named profiler zone (milliseconds). Re-setting the same name updates it.
    void setBudget(const std::string& zone, double ms) {
        if (m_index.find(zone) == m_index.end()) {
            m_index[zone] = m_order.size();
            m_order.push_back(zone);
        }
        m_budgets[zone] = ms;
    }

    // Budget the whole frame (sum of top-level zones), milliseconds. 0 disables the frame check.
    void setFrameBudget(double ms) { m_frameBudgetMs = ms; }

    void clear() {
        m_budgets.clear();
        m_order.clear();
        m_index.clear();
        m_frameBudgetMs = 0.0;
    }

    double frameBudget() const { return m_frameBudgetMs; }
    bool hasBudget(const std::string& zone) const { return m_budgets.count(zone) != 0; }

    // Compare the profiler's latest measurements against the budgets. When `useSmoothed` is true,
    // each zone is judged by its EMA-smoothed time (alerts only on a sustained regression);
    // otherwise by this frame's inclusive time (alerts on a single spike). The frame total is the
    // sum of the top-level (depth-0) zones by the same metric.
    Report check(const Profiler& prof, bool useSmoothed = false) const {
        Report r;
        r.frameBudgetMs = m_frameBudgetMs;

        for (const std::string& name : m_order) {
            const Profiler::Zone* z = prof.find(name);
            if (!z) {
                continue; // budgeted section didn't run this frame
            }
            const double actual = zoneMs(*z, useSmoothed);
            const double budget = m_budgets.at(name);
            if (actual > budget) {
                r.zones.push_back(Overage{name, budget, actual, actual - budget});
            }
        }

        // Frame total = sum of depth-0 zones (top-level spans don't overlap).
        double frame = 0.0;
        for (const Profiler::Zone& z : prof.zones()) {
            if (z.depth == 0) {
                frame += zoneMs(z, useSmoothed);
            }
        }
        r.frameMs = frame;
        if (m_frameBudgetMs > 0.0 && frame > m_frameBudgetMs) {
            r.frameOverBudget = true;
            r.frameOverMs = frame - m_frameBudgetMs;
        }
        return r;
    }

  private:
    static double zoneMs(const Profiler::Zone& z, bool useSmoothed) {
        return useSmoothed ? z.smoothedMs : static_cast<double>(z.inclusiveUs) / 1000.0;
    }

    std::unordered_map<std::string, double> m_budgets;
    std::vector<std::string> m_order; // budget insertion order (stable reporting)
    std::unordered_map<std::string, size_t> m_index;
    double m_frameBudgetMs = 0.0;
};

} // namespace maz::core
