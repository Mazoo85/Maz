#pragma once

#include <algorithm> // std::fmod via <cmath>; std::max
#include <cmath>

// maz::core fixed-timestep accumulator — the classic "Fix Your Timestep" game-loop driver (Glenn Fiedler).
// Simulation wants a CONSTANT dt so physics and gameplay are deterministic and stable, but real frames
// arrive at a variable, display-driven rate. This accumulates the variable frame time and hands back how
// many fixed steps to run this frame, keeping the leftover as an interpolation ALPHA for smooth rendering.
//
// It is the missing driver for the engine's `core::Interpolated<T>` (whose own docs say "push once per fixed
// step, sample(alpha) once per render frame" but which ships no way to compute that step count or alpha): a
// frame does `for (int i = 0, n = ts.advance(dt); i < n; ++i) simulate(ts.step()); render(ts.alpha());`.
// Distinct from `core::GameClock` (accumulates scaled seconds, no fixed decomposition), `core::Scheduler`
// (fires callbacks at times) and `game::TimeControl` (produces a scaled delta) — this turns one variable
// delta into a whole number of fixed steps plus a blend fraction.
//
// Spiral-of-death protection: if a frame stalls (a huge dt — a breakpoint, a slow load), running every
// backlogged step would make the next frame even slower, forever. So the accumulator is capped at
// maxSteps * step: at most maxSteps run per frame and the excess real time is DROPPED (simulation time
// slows rather than the game freezing). Header-only, std-only, deterministic.
namespace maz::core {

class FixedTimestep {
public:
    // `step`: the fixed simulation dt in seconds (e.g. 1.0/60.0). `maxSteps`: the cap on steps run in one
    // frame (spiral-of-death guard). Both clamped to sane minimums.
    explicit FixedTimestep(double step = 1.0 / 60.0, int maxSteps = 8)
        : m_step(step > 1e-9 ? step : 1e-9), m_maxSteps(maxSteps < 1 ? 1 : maxSteps) {}

    // Add the real frame delta and return how many fixed steps to run now (0..maxSteps), consuming that much
    // simulated time. A non-positive dt adds nothing and returns 0. Excess beyond maxSteps*step is dropped.
    int advance(double realDt) {
        if (realDt > 0.0) m_accum += realDt;
        const double cap = m_step * static_cast<double>(m_maxSteps);
        if (m_accum > cap) m_accum = cap; // drop backlog: slow down rather than spiral
        int steps = 0;
        while (m_accum >= m_step) {
            m_accum -= m_step;
            ++steps;
        }
        return steps;
    }

    // Interpolation blend in [0,1): how far into the next fixed step the leftover time has reached. Feed to
    // Interpolated<T>::sample so rendering glides between the previous and current simulated states.
    double alpha() const { return m_accum / m_step; }

    double step() const { return m_step; }             // the fixed simulation dt
    int maxSteps() const { return m_maxSteps; }
    double accumulated() const { return m_accum; }      // leftover time not yet consumed (< step)

    // Change the fixed step (e.g. switching sim rate); clears any leftover so alpha stays valid.
    void setStep(double step) {
        m_step = step > 1e-9 ? step : 1e-9;
        m_accum = 0.0;
    }
    void setMaxSteps(int maxSteps) { m_maxSteps = maxSteps < 1 ? 1 : maxSteps; }
    void reset() { m_accum = 0.0; }

private:
    double m_step;
    int m_maxSteps;
    double m_accum = 0.0;
};

} // namespace maz::core
