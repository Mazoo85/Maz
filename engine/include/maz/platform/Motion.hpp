#pragma once

#include "maz/math/Rect2.hpp"               // math::vec2
#include "maz/platform/PlatformBackend.hpp" // platform::MotionState (the POD the backend reports)

#include <algorithm>
#include <cmath>

// maz::platform motion-sensor policy — the pure math that turns a raw accelerometer/gyroscope reading
// (PlatformBackend::motionState()) into the things a game actually wants: a tilt steering vector, a shake
// gesture, and a "is the device lying flat?" test. Kept out of PlatformBackend.hpp so that header stays
// dependency-light, exactly like SafeArea.hpp / PowerState.hpp. Every helper is deterministic and unit-tested
// headlessly; on desktop/headless the backend reports an all-zero MotionState, so tiltVector() is the zero
// vector and isShaking() is false — a game can read tilt controls unconditionally and they simply do nothing
// where there is no IMU. Axes follow the POD: +x right, +y up, +z out of the glass; the accelerometer is in
// m/s^2 and includes gravity, so a device flat face-up reads ~+9.81 on z.
namespace maz::platform {

// Standard gravity in m/s^2 — the accelerometer magnitude a still device reads.
inline constexpr float kStandardGravity = 9.80665f;

// Length of the accelerometer vector (m/s^2). ~9.81 for a still device; larger while shaken, ~0 with no IMU.
inline float accelMagnitude(const MotionState& m) {
    return std::sqrt(m.accelX * m.accelX + m.accelY * m.accelY + m.accelZ * m.accelZ);
}

// Angular speed of the gyroscope (rad/s), regardless of axis — how fast the device is rotating.
inline float rotationRate(const MotionState& m) {
    return std::sqrt(m.gyroX * m.gyroX + m.gyroY * m.gyroY + m.gyroZ * m.gyroZ);
}

// Is there a real reading at all? The all-zero neutral (no IMU / desktop / headless) has ~0 magnitude, so a
// game can branch on this to fall back to touch/keyboard steering where motion is unavailable.
inline bool hasMotionData(const MotionState& m, float epsilon = 1e-3f) {
    return accelMagnitude(m) > epsilon;
}

// The tilt steering vector in [-1, 1] on each axis: the component of gravity lying IN the screen plane
// (accelX, accelY), normalized so `fullTiltG` g of lateral pull maps to full deflection, then clamped. A
// device held flat face-up puts all of gravity on z, so this is ~zero (no steering); tilting the top edge
// away pushes +y, tilting it toward you pushes -y, rolling left/right drives x. `fullTiltG` = 1.0 means a
// 90° tilt (device on its side) reaches magnitude 1; a smaller value makes the controls more sensitive.
inline math::vec2 tiltVector(const MotionState& m, float fullTiltG = 1.0f) {
    const float denom = std::max(fullTiltG, 1e-4f) * kStandardGravity;
    const float x = std::clamp(m.accelX / denom, -1.0f, 1.0f);
    const float y = std::clamp(m.accelY / denom, -1.0f, 1.0f);
    return math::vec2{x, y};
}

// The angle (radians) the device is tilted away from lying flat — 0 when the screen faces straight up or
// down, PI/2 when it is vertical (on its side). Derived from how much of gravity remains on the z axis.
inline float tiltAngleRad(const MotionState& m) {
    const float mag = accelMagnitude(m);
    if (mag <= 1e-4f) {
        return 0.0f; // no reading: treat as flat
    }
    const float cosZ = std::clamp(std::fabs(m.accelZ) / mag, 0.0f, 1.0f);
    return std::acos(cosZ);
}

// Is the device lying roughly flat (screen up or down) within `toleranceDeg`? Useful to gate tilt controls
// (a flat device gives no meaningful tilt) or to detect "placed on a table".
inline bool deviceIsFlat(const MotionState& m, float toleranceDeg = 15.0f) {
    if (!hasMotionData(m)) {
        return false; // no reading is not a confident "flat"
    }
    const float tol = toleranceDeg * (3.14159265358979323846f / 180.0f);
    return tiltAngleRad(m) <= tol;
}

// A shake gesture: total acceleration well above the ~1 g of gravity means the device is being jerked around.
// `thresholdG` is the multiple of gravity that counts (1.8 g is a comfortable default — a deliberate shake,
// not ordinary handling).
inline bool isShaking(const MotionState& m, float thresholdG = 1.8f) {
    return accelMagnitude(m) > thresholdG * kStandardGravity;
}

// Exponential low-pass smoothing of a reading toward the previous smoothed value — the standard de-jitter for
// tilt controls, and the classic way to isolate the slow gravity vector from noisy accelerometer data.
// `alpha` in [0, 1] is the weight of the NEW sample (1 = no smoothing, small = heavy smoothing). Applied
// component-wise to both accelerometer and gyroscope.
inline MotionState lowPassFilter(const MotionState& prev, const MotionState& current, float alpha) {
    const float a = std::clamp(alpha, 0.0f, 1.0f);
    MotionState out;
    out.accelX = prev.accelX + a * (current.accelX - prev.accelX);
    out.accelY = prev.accelY + a * (current.accelY - prev.accelY);
    out.accelZ = prev.accelZ + a * (current.accelZ - prev.accelZ);
    out.gyroX = prev.gyroX + a * (current.gyroX - prev.gyroX);
    out.gyroY = prev.gyroY + a * (current.gyroY - prev.gyroY);
    out.gyroZ = prev.gyroZ + a * (current.gyroZ - prev.gyroZ);
    return out;
}

} // namespace maz::platform
