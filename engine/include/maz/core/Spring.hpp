#pragma once

#include <cmath> // std::ceil, std::fabs

// maz::core SPRING — a damped-harmonic-oscillator value smoother for UI juice and gameplay motion: eases a value
// toward a moving target with a natural bounce. Unlike `core::SmoothDamp` (critically damped — glides in with no
// overshoot), a Spring is tunable from bouncy (low damping, overshoots and settles) through critical to sluggish
// (over-damped), so it drives springy menus, camera lag, knockback recovery, cursor trails, and "pop" on pickups.
// Parameterised by `frequency` (Hz — how fast it oscillates) and `damping` (the damping ratio ζ: <1 bouncy, 1
// critical, >1 sluggish). Integrated with sub-stepped semi-implicit (symplectic) Euler so it stays stable for any
// dt and parameters. Header-only, deterministic. Run one Spring per axis for 2D/3D motion.
namespace maz::core {

class Spring {
public:
    float position = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;
    float frequency = 1.0f; // natural oscillation rate in Hz
    float damping = 1.0f;   // damping ratio ζ (0 = undamped, <1 bouncy, 1 critical, >1 over-damped)

    Spring() = default;
    Spring(float freqHz, float dampingRatio) : frequency(freqHz), damping(dampingRatio) {}

    // Advance the spring by `dt` seconds toward `target`.
    void update(float dt) {
        if (dt <= 0.0f || frequency <= 0.0f) return;
        constexpr float kPi = 3.14159265358979323846f;
        const float omega = 2.0f * kPi * frequency;
        const float k = omega * omega;      // stiffness
        const float c = 2.0f * damping * omega; // damping coefficient

        // Sub-step to a small fixed step so the explicit integrator stays stable at any frequency/dt.
        constexpr float kMaxStep = 1.0f / 240.0f;
        int steps = static_cast<int>(std::ceil(dt / kMaxStep));
        if (steps < 1) steps = 1;
        const float h = dt / static_cast<float>(steps);
        for (int i = 0; i < steps; ++i) {
            velocity += (-k * (position - target) - c * velocity) * h;
            position += velocity * h;
        }
    }

    // Snap to `pos` with zero velocity (no motion).
    void reset(float pos) {
        position = pos;
        velocity = 0.0f;
    }

    // True once the spring has effectively settled on its target.
    bool atRest(float posEps = 1e-3f, float velEps = 1e-3f) const {
        return std::fabs(position - target) < posEps && std::fabs(velocity) < velEps;
    }
};

} // namespace maz::core
