#pragma once

#include <limits>

// maz::core::PidController — a classic proportional-integral-derivative feedback controller. Given a
// target (setpoint) and a measurement each step, it returns a control signal that drives the
// measurement toward the target: the P term reacts to the current error, the I term accumulates past
// error to erase steady-state offset (e.g. a constant disturbance the P term alone can't cancel), and
// the D term damps by reacting to the error's rate of change. The workhorse behind self-correcting
// systems Godot has no built-in for: a turret smoothly tracking a moving target, a hovering/thrusting
// vehicle holding altitude, an auto-throttle or cruise control, a self-balancing biped, or an
// adaptive-difficulty signal chasing a target win-rate. Includes anti-windup (the integral
// accumulator is clamped), output clamping, and an optional derivative-on-measurement mode that
// avoids the output spike ("derivative kick") a sudden setpoint change would otherwise cause.
// Header-only, std-only, deterministic.
namespace maz::core {

class PidController {
public:
    PidController() = default;
    PidController(double kp, double ki, double kd) : m_kp(kp), m_ki(ki), m_kd(kd) {}

    void setGains(double kp, double ki, double kd) {
        m_kp = kp;
        m_ki = ki;
        m_kd = kd;
    }
    // Clamp the returned control signal to [lo, hi].
    void setOutputLimits(double lo, double hi) {
        m_outMin = lo;
        m_outMax = hi;
    }
    // Clamp the magnitude of the integral accumulator (anti-windup). Pass a large value to disable.
    void setIntegralLimit(double limit) { m_iLimit = limit < 0.0 ? -limit : limit; }
    // When true, the derivative reacts to the measurement's change instead of the error's, so a
    // sudden setpoint change does not produce a derivative spike.
    void setDerivativeOnMeasurement(bool on) { m_derivOnMeasurement = on; }

    // Advance one step and return the control output. dt <= 0 skips integration/derivative (returns
    // the proportional + current integral contribution) so a paused frame can't corrupt state.
    double update(double setpoint, double measured, double dt) {
        const double error = setpoint - measured;

        if (dt <= 0.0) {
            const double held = clamp(m_kp * error + m_ki * m_integral, m_outMin, m_outMax);
            m_lastError = error;
            m_lastMeasured = measured;
            m_lastOutput = held;
            return held;
        }

        // Integral with anti-windup clamp.
        m_integral += error * dt;
        m_integral = clamp(m_integral, -m_iLimit, m_iLimit);

        // Derivative (zero on the first sample, since there is no prior reference).
        double deriv = 0.0;
        if (!m_firstUpdate) {
            deriv = m_derivOnMeasurement ? -(measured - m_lastMeasured) / dt
                                         : (error - m_lastError) / dt;
        }

        double out = m_kp * error + m_ki * m_integral + m_kd * deriv;
        out = clamp(out, m_outMin, m_outMax);

        m_lastError = error;
        m_lastMeasured = measured;
        m_lastOutput = out;
        m_firstUpdate = false;
        return out;
    }

    // Clear integral accumulation and derivative history (keeps gains and limits).
    void reset() {
        m_integral = 0.0;
        m_lastError = 0.0;
        m_lastMeasured = 0.0;
        m_lastOutput = 0.0;
        m_firstUpdate = true;
    }

    double kp() const { return m_kp; }
    double ki() const { return m_ki; }
    double kd() const { return m_kd; }
    double integral() const { return m_integral; }
    double lastError() const { return m_lastError; }
    double lastOutput() const { return m_lastOutput; }

private:
    static double clamp(double v, double lo, double hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    double m_kp = 0.0;
    double m_ki = 0.0;
    double m_kd = 0.0;
    double m_integral = 0.0;
    double m_lastError = 0.0;
    double m_lastMeasured = 0.0;
    double m_lastOutput = 0.0;
    double m_iLimit = std::numeric_limits<double>::infinity();
    double m_outMin = -std::numeric_limits<double>::infinity();
    double m_outMax = std::numeric_limits<double>::infinity();
    bool m_derivOnMeasurement = false;
    bool m_firstUpdate = true;
};

} // namespace maz::core
