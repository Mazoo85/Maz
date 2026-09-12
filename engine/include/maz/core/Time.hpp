#pragma once

#include <cstdint>

namespace maz::core {

// Fixed-timestep clock. Decouples the deterministic simulation step from render frame rate
// using the classic accumulator pattern (default step = 1/60 s).
//
// Usage per frame:
//   clock.beginFrame();
//   while (clock.consumeFixedStep()) { update(clock.fixedDelta()); }
//   render(clock.interpolationAlpha());
class Clock {
public:
    explicit Clock(double fixedStepSeconds = 1.0 / 60.0);

    // Sample wall time, compute frame delta (clamped to avoid the "spiral of death"),
    // and feed the fixed-step accumulator.
    void beginFrame();

    // Drain one fixed step from the accumulator. Returns false when none remain this frame.
    bool consumeFixedStep();

    double fixedDelta() const { return m_fixedStep; }
    double frameDelta() const { return m_frameDelta; }      // real seconds since last frame
    double interpolationAlpha() const;                       // [0,1) leftover for render lerp
    double elapsed() const { return m_elapsed; }             // total seconds since first frame
    uint64_t frameCount() const { return m_frameCount; }

private:
    double m_fixedStep;
    double m_accumulator = 0.0;
    double m_frameDelta = 0.0;
    double m_elapsed = 0.0;
    uint64_t m_lastTicks = 0;   // platform performance-counter ticks
    uint64_t m_frameCount = 0;
    bool m_started = false;
};

} // namespace maz::core
