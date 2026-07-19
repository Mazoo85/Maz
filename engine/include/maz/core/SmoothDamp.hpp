#pragma once

#include <limits>

// maz::core::smoothDamp — critically-damped spring smoothing toward a (possibly moving) target. Given
// the current value, a target, and a persistent velocity, it eases the value toward the target over an
// approximate time-to-reach (smoothTime) without the overshoot/ringing an under-damped spring produces
// and without the abrupt arrival of a linear move. This is the standard "buttery" follow behaviour —
// a camera trailing the player, a UI element gliding to a resting position, a health bar catching up
// to a new value — where the target keeps changing and you want the motion to stay smooth and
// interruptible frame to frame. The classic Game-Programming-Gems / Unity Mathf.SmoothDamp recurrence:
// stable for any dt, converges monotonically (no overshoot), and an optional maxSpeed caps how fast
// the value may travel. Godot has no SmoothDamp equivalent (its Tween/lerp are fixed-duration, not
// velocity-carrying critically-damped springs). Header-only, std-only, deterministic.
namespace maz::core {

// Ease `current` toward `target`, carrying `velocity` between calls. smoothTime is the approximate
// time (seconds) to reach the target; smaller is snappier. maxSpeed caps the travel rate. dt <= 0
// returns `current` unchanged.
inline double smoothDamp(double current, double target, double& velocity, double smoothTime, double dt,
                         double maxSpeed = std::numeric_limits<double>::infinity()) {
    if (dt <= 0.0) {
        return current;
    }
    // Guard against a zero/negative smoothing time (would divide by zero).
    if (smoothTime < 1.0e-4) {
        smoothTime = 1.0e-4;
    }

    const double omega = 2.0 / smoothTime;
    const double x = omega * dt;
    // Rational approximation of exp(-x) (accurate and cheap; no <cmath> dependency).
    const double expo = 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x);

    double change = current - target;
    const double originalTarget = target;

    // Clamp the maximum change so the value cannot travel faster than maxSpeed.
    const double maxChange = maxSpeed * smoothTime;
    if (change > maxChange) {
        change = maxChange;
    } else if (change < -maxChange) {
        change = -maxChange;
    }
    const double clampedTarget = current - change;

    const double temp = (velocity + omega * change) * dt;
    velocity = (velocity - omega * temp) * expo;
    double output = clampedTarget + (change + temp) * expo;

    // Prevent overshooting the (original) target.
    if ((originalTarget - current > 0.0) == (output > originalTarget)) {
        output = originalTarget;
        velocity = (output - originalTarget) / dt;
    }
    return output;
}

// Stateful convenience wrapper holding the value + velocity so callers need not manage them.
class SmoothDamp {
public:
    SmoothDamp() = default;
    explicit SmoothDamp(double value) : m_value(value) {}

    double value() const { return m_value; }
    double velocity() const { return m_velocity; }

    // Jump directly to `value` and clear the velocity.
    void reset(double value) {
        m_value = value;
        m_velocity = 0.0;
    }

    // Advance one step toward `target`, updating and returning the internal value.
    double step(double target, double smoothTime, double dt,
                double maxSpeed = std::numeric_limits<double>::infinity()) {
        m_value = smoothDamp(m_value, target, m_velocity, smoothTime, dt, maxSpeed);
        return m_value;
    }

private:
    double m_value = 0.0;
    double m_velocity = 0.0;
};

} // namespace maz::core
