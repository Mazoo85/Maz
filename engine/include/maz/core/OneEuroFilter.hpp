#pragma once

// maz::core::OneEuroFilter — an adaptive low-pass filter for noisy, human-driven signals (Casiez,
// Roussel & Vogel 2012). Raw pointer/touch/stylus/VR-pose input is jittery when still yet needs to
// feel responsive when moving fast — two goals a fixed low-pass can't serve at once (smooth enough to
// kill jitter is always too laggy in motion). The 1€ filter resolves this by RAISING its cutoff
// frequency as the signal's speed rises: nearly stationary input is filtered hard (jitter vanishes),
// fast input is filtered lightly (lag vanishes). Two intuitive knobs: minCutoff sets the smoothing at
// rest (lower = smoother/laggier), beta sets how aggressively responsiveness ramps with speed (higher
// = less lag when moving). Distinct from SmoothDamp (which chases a target, not denoises a stream) and
// from a plain EMA (fixed cutoff). Header-only, std-only, deterministic. Godot has no 1€ filter.
namespace maz::core {

// A single exponential low-pass stage: y = alpha*x + (1-alpha)*y_prev (holds y at the first sample).
class LowPassFilter {
public:
    double filter(double x, double alpha) {
        if (!m_init) {
            m_y = x;
            m_init = true;
        } else {
            m_y = alpha * x + (1.0 - alpha) * m_y;
        }
        return m_y;
    }
    double value() const { return m_y; }
    bool initialized() const { return m_init; }
    void reset() {
        m_y = 0.0;
        m_init = false;
    }

private:
    double m_y = 0.0;
    bool m_init = false;
};

class OneEuroFilter {
public:
    // minCutoff (Hz): smoothing when nearly still. beta: speed coefficient (0 = plain low-pass).
    // dCutoff (Hz): cutoff of the internal speed estimate.
    explicit OneEuroFilter(double minCutoff = 1.0, double beta = 0.0, double dCutoff = 1.0)
        : m_minCutoff(minCutoff > 0.0 ? minCutoff : 1.0e-6),
          m_beta(beta),
          m_dCutoff(dCutoff > 0.0 ? dCutoff : 1.0e-6) {}

    void setMinCutoff(double v) { m_minCutoff = v > 0.0 ? v : 1.0e-6; }
    void setBeta(double v) { m_beta = v; }
    void setDerivativeCutoff(double v) { m_dCutoff = v > 0.0 ? v : 1.0e-6; }

    double minCutoff() const { return m_minCutoff; }
    double beta() const { return m_beta; }

    // Filter one sample given the elapsed time dt (seconds). dt <= 0 passes the sample through.
    double filter(double x, double dt) {
        if (dt <= 0.0) {
            m_xPrev = x;
            m_hasPrev = true;
            return m_x.filter(x, 1.0); // alpha 1 -> pass through, but seed the stage
        }

        // Estimate the signal's rate of change and smooth it.
        const double dx = m_hasPrev ? (x - m_xPrev) / dt : 0.0;
        const double edx = m_dx.filter(dx, alpha(m_dCutoff, dt));

        // Speed-adaptive cutoff: faster motion -> higher cutoff -> less smoothing/lag.
        const double speed = edx < 0.0 ? -edx : edx;
        const double cutoff = m_minCutoff + m_beta * speed;

        const double out = m_x.filter(x, alpha(cutoff, dt));
        m_xPrev = x;
        m_hasPrev = true;
        return out;
    }

    double value() const { return m_x.value(); }

    void reset() {
        m_x.reset();
        m_dx.reset();
        m_hasPrev = false;
        m_xPrev = 0.0;
    }

private:
    // Smoothing factor for a given cutoff frequency and timestep.
    static double alpha(double cutoff, double dt) {
        constexpr double kPi = 3.14159265358979323846;
        const double tau = 1.0 / (2.0 * kPi * cutoff);
        return 1.0 / (1.0 + tau / dt);
    }

    double m_minCutoff;
    double m_beta;
    double m_dCutoff;
    LowPassFilter m_x;
    LowPassFilter m_dx;
    double m_xPrev = 0.0;
    bool m_hasPrev = false;
};

} // namespace maz::core
