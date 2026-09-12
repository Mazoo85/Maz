#pragma once

#include "maz/math/Math.hpp" // quat, vec3

#include <cmath>

// maz::math AHRS attitude filter — fuse a gyroscope and an accelerometer into a drift-corrected orientation
// quaternion (Madgwick's gradient-descent IMU filter, 2010). A raw gyro gives smooth short-term rotation but
// its integrated angle drifts without bound; a raw accelerometer gives an absolute "which way is down" but is
// noisy and useless while the device is being shaken. This filter blends them: the gyro drives the estimate
// forward each step and a single gradient step toward the measured gravity direction bleeds off the drift.
//
// This is the standard sensor-fusion behind motion controls, phone/tablet tilt input, VR/AR controller
// tracking, and any "point the device and the game reacts" mechanic. Godot exposes raw
// Input.get_gyroscope()/get_accelerometer() but gives you no fusion — you get the noisy sensors and have to
// build this yourself. Header-only, std-only, deterministic, allocation-free (fine for a per-frame update).
//
// Convention (Madgwick's): the accelerometer, at rest, reads the reaction to gravity — pointing UP, i.e.
// world +Z expressed in the body frame; gyro is body-frame angular velocity in rad/s. With only a gyro and an
// accelerometer, tilt (roll & pitch relative to gravity) is observable and corrected, but heading (yaw about
// the gravity axis) is unobservable and comes from gyro integration alone — add a magnetometer (a MARG
// filter) to reference heading. The quaternion maps body -> world.
namespace maz::math {

// The direction of gravity (world +Z) expressed in the body frame of orientation `q` — i.e. what an ideal
// accelerometer at rest would read for this orientation. Independent of the filter; handy for synthesizing or
// checking readings. `q` is assumed unit-length.
inline vec3 gravityDirectionBody(const quat& q) {
    const float w = q.w, x = q.x, y = q.y, z = q.z;
    return vec3(2.0f * (x * z - w * y), 2.0f * (w * x + y * z), w * w - x * x - y * y + z * z);
}

// Madgwick IMU (gyro + accelerometer) attitude filter. Construct, then call `updateImu` once per sample with
// the elapsed time; read the fused orientation from `orientation()`.
struct MadgwickFilter {
    quat q = quat(1.0f, 0.0f, 0.0f, 0.0f); // current estimate, body -> world (starts at identity)
    // Filter gain: how hard the accelerometer pulls the estimate toward "down" each step. Madgwick derives
    // beta from the gyro's noise floor; ~0.1 is a good default, larger converges/rejects drift faster but
    // tracks accelerometer noise more, smaller is smoother but sluggish to correct.
    float beta = 0.1f;

    void reset() { q = quat(1.0f, 0.0f, 0.0f, 0.0f); }
    const quat& orientation() const { return q; }

    // Advance the estimate by `dt` seconds using body-frame angular velocity `gyro` (rad/s) and the
    // accelerometer reading `accel` (any units — only its direction matters; magnitude is normalised away).
    // A zero-length accel (free-fall / invalid sample) skips the correction and integrates the gyro alone.
    void updateImu(const vec3& gyro, const vec3& accel, float dt) {
        float q0 = q.w, q1 = q.x, q2 = q.y, q3 = q.z;
        const float gx = gyro.x, gy = gyro.y, gz = gyro.z;

        // Rate of change of the quaternion from the gyroscope: qDot = 0.5 * q (x) (0, gx, gy, gz).
        float qDot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
        float qDot1 = 0.5f * (q0 * gx + q2 * gz - q3 * gy);
        float qDot2 = 0.5f * (q0 * gy - q1 * gz + q3 * gx);
        float qDot3 = 0.5f * (q0 * gz + q1 * gy - q2 * gx);

        float ax = accel.x, ay = accel.y, az = accel.z;
        const float aMag = std::sqrt(ax * ax + ay * ay + az * az);
        if (aMag > 0.0f) {
            const float inv = 1.0f / aMag;
            ax *= inv;
            ay *= inv;
            az *= inv;

            // Gradient of the objective f = (predicted gravity in body) - (measured accel), where the
            // predicted gravity is gravityDirectionBody(q). This is Madgwick's closed-form Jacobian^T f.
            const float _2q0 = 2.0f * q0, _2q1 = 2.0f * q1, _2q2 = 2.0f * q2, _2q3 = 2.0f * q3;
            const float _4q0 = 4.0f * q0, _4q1 = 4.0f * q1, _4q2 = 4.0f * q2;
            const float _8q1 = 8.0f * q1, _8q2 = 8.0f * q2;
            const float q0q0 = q0 * q0, q1q1 = q1 * q1, q2q2 = q2 * q2, q3q3 = q3 * q3;

            float s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
            float s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay - _4q1 + _8q1 * q1q1 +
                       _8q1 * q2q2 + _4q1 * az;
            float s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay - _4q2 + _8q2 * q1q1 +
                       _8q2 * q2q2 + _4q2 * az;
            float s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
            const float sNorm = std::sqrt(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
            if (sNorm > 0.0f) {
                const float si = 1.0f / sNorm;
                s0 *= si;
                s1 *= si;
                s2 *= si;
                s3 *= si;
                // Apply the feedback step, subtracting the normalised gradient scaled by beta.
                qDot0 -= beta * s0;
                qDot1 -= beta * s1;
                qDot2 -= beta * s2;
                qDot3 -= beta * s3;
            }
        }

        // Integrate to yield the new quaternion, then renormalise.
        q0 += qDot0 * dt;
        q1 += qDot1 * dt;
        q2 += qDot2 * dt;
        q3 += qDot3 * dt;
        const float n = std::sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
        if (n > 0.0f) {
            const float ni = 1.0f / n;
            q = quat(q0 * ni, q1 * ni, q2 * ni, q3 * ni); // glm::quat(w, x, y, z)
        }
    }
};

} // namespace maz::math
