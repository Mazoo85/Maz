#pragma once

// maz::core Kalman filters — the statistically-optimal recursive estimator that fuses a noisy
// measurement with a model prediction while tracking its own uncertainty (variance/covariance). This is
// a different tool from the engine's other smoothers: OneEuroFilter is a tuned low-pass, SmoothDamp is a
// critically-damped spring, and PidController is a controller — none of them model measurement noise or
// carry an uncertainty estimate. A Kalman filter does, which is what makes it the right choice for
// de-noising jittery analog/sensor input, smoothing network-replicated positions, and light sensor
// fusion. Godot ships no Kalman filter, so this is a beyond-Godot utility. Header-only, std-only,
// deterministic.
namespace maz::core {

// Scalar (1D) Kalman filter over a random-walk process: the true value is assumed roughly constant but
// allowed to drift by `processVar` (Q) each step, and each measurement is noisy with variance
// `measureVar` (R). Larger Q -> trust measurements more (snappier); larger R -> trust the model more
// (smoother). predict() inflates uncertainty by Q; update(z) folds in a measurement and shrinks it.
class Kalman1D {
public:
    Kalman1D() = default;
    Kalman1D(float processVar, float measureVar, float initialValue = 0.0f, float initialVar = 1.0f)
        : m_q(processVar), m_r(measureVar), m_x(initialValue), m_p(initialVar) {}

    // Advance the model one step: the estimate is unchanged (random-walk), uncertainty grows by Q.
    void predict() { m_p += m_q; }

    // Fold in a measurement z and return the new estimate. K = P/(P+R) is the Kalman gain in [0,1]:
    // K->1 trusts the measurement, K->0 keeps the prediction.
    float update(float z) {
        const float k = m_p / (m_p + m_r);
        m_x += k * (z - m_x);
        m_p *= (1.0f - k);
        m_lastGain = k;
        return m_x;
    }

    // Convenience: predict then update in one call (the usual per-frame step).
    float step(float z) {
        predict();
        return update(z);
    }

    void reset(float value, float variance = 1.0f) {
        m_x = value;
        m_p = variance;
        m_lastGain = 0.0f;
    }

    void setProcessVar(float q) { m_q = q; }
    void setMeasureVar(float r) { m_r = r; }
    float value() const { return m_x; }
    float variance() const { return m_p; }
    float gain() const { return m_lastGain; }

private:
    float m_q = 1e-3f;    // process (model) variance Q
    float m_r = 1e-2f;    // measurement variance R
    float m_x = 0.0f;     // state estimate
    float m_p = 1.0f;     // estimate variance P
    float m_lastGain = 0.0f;
};

// Constant-velocity (1D position + velocity) Kalman tracker. The state is [position, velocity]; only
// position is measured. It recovers a smooth position AND an inferred velocity from a stream of noisy
// position samples — the classic tool for smoothing a moving target or reconciling a networked entity.
// `accelVar` is the process (unmodelled-acceleration) variance; `measureVar` is the position-measurement
// variance. The 2x2 covariance is carried as explicit symmetric terms (p01 == p10) to stay std-only.
class KalmanCV {
public:
    KalmanCV() = default;
    KalmanCV(float accelVar, float measureVar) : m_qa(accelVar), m_r(measureVar) {}

    void reset(float position, float velocity = 0.0f, float posVar = 1.0f, float velVar = 1.0f) {
        m_x = position;
        m_v = velocity;
        m_p00 = posVar;
        m_p01 = 0.0f;
        m_p11 = velVar;
    }

    // Predict forward by `dt`: x += v*dt, then P = F P F^T + Q with the standard constant-acceleration
    // process-noise matrix Q = accelVar * [[dt^4/4, dt^3/2],[dt^3/2, dt^2]].
    void predict(float dt) {
        m_x += m_v * dt;

        // P = F P F^T, with F = [[1, dt],[0, 1]].
        const float p00 = m_p00 + dt * (m_p01 + m_p01 + dt * m_p11);
        const float p01 = m_p01 + dt * m_p11;
        const float p11 = m_p11;

        // + Q.
        const float dt2 = dt * dt;
        const float dt3 = dt2 * dt;
        const float dt4 = dt2 * dt2;
        m_p00 = p00 + m_qa * dt4 * 0.25f;
        m_p01 = p01 + m_qa * dt3 * 0.5f;
        m_p11 = p11 + m_qa * dt2;
    }

    // Fold in a position measurement z. Returns the updated position estimate.
    float update(float z) {
        const float s = m_p00 + m_r;      // innovation variance
        const float k0 = m_p00 / s;       // gain (position)
        const float k1 = m_p01 / s;       // gain (velocity)
        const float y = z - m_x;          // innovation
        m_x += k0 * y;
        m_v += k1 * y;

        // P = (I - K H) P, H = [1 0].
        const float p00 = (1.0f - k0) * m_p00;
        const float p01 = (1.0f - k0) * m_p01;
        const float p11 = m_p11 - k1 * m_p01;
        m_p00 = p00;
        m_p01 = p01;
        m_p11 = p11;
        return m_x;
    }

    float step(float dt, float z) {
        predict(dt);
        return update(z);
    }

    float position() const { return m_x; }
    float velocity() const { return m_v; }
    float posVariance() const { return m_p00; }
    float velVariance() const { return m_p11; }

private:
    float m_qa = 1.0f;    // acceleration (process) variance
    float m_r = 1e-2f;    // position measurement variance
    float m_x = 0.0f;     // position estimate
    float m_v = 0.0f;     // velocity estimate
    float m_p00 = 1.0f;   // cov(position, position)
    float m_p01 = 0.0f;   // cov(position, velocity)
    float m_p11 = 1.0f;   // cov(velocity, velocity)
};

} // namespace maz::core
