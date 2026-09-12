// tests/platform/motion.cpp — verifies platform::Motion policy: turning a raw MotionState (accelerometer +
// gyroscope) into a tilt steering vector, a shake gesture, a flat-device test, and a low-pass smoother, plus
// PlatformBackend::motionState()'s all-zero default on desktop. Pure deterministic math; the live IMU reading
// comes from a mobile backend.
#include "maz/platform/Motion.hpp"

#include "maz/platform/DesktopBackend.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::platform;

static bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
    const float G = kStandardGravity;

    // --- 1. Magnitudes: a still device flat face-up reads ~1 g on z; gyro speed is the vector length. ---
    {
        MotionState flat;
        flat.accelZ = G;
        CHECK(approx(accelMagnitude(flat), G), "flat magnitude is ~1 g");
        CHECK(approx(rotationRate(flat), 0.0f), "still device has no rotation");
        MotionState spin;
        spin.gyroX = 3.0f;
        spin.gyroY = 4.0f;
        CHECK(approx(rotationRate(spin), 5.0f), "rotation rate is the gyro vector length");
    }

    // --- 2. hasMotionData: the all-zero neutral (no IMU) reads false; any real reading reads true. ---
    {
        CHECK(!hasMotionData(MotionState{}), "all-zero neutral has no motion data");
        MotionState real;
        real.accelZ = G;
        CHECK(hasMotionData(real), "a real gravity reading has motion data");
    }

    // --- 3. tiltVector: flat -> ~0; tilt leaks gravity into the screen plane; extremes clamp to [-1,1]. ---
    {
        MotionState flat;
        flat.accelZ = G;
        const maz::math::vec2 t0 = tiltVector(flat);
        CHECK(approx(t0.x, 0.0f) && approx(t0.y, 0.0f), "flat device does not steer");

        // Device rolled onto its right side: gravity now fully along +x -> full +1 deflection on x.
        MotionState side;
        side.accelX = G;
        const maz::math::vec2 tx = tiltVector(side);
        CHECK(approx(tx.x, 1.0f) && approx(tx.y, 0.0f), "90-degree roll is full x deflection");

        // Half a g of lateral pull with fullTiltG=1 -> 0.5 steer.
        MotionState half;
        half.accelY = 0.5f * G;
        CHECK(approx(tiltVector(half).y, 0.5f), "half-g tilt is half deflection");

        // Beyond a full g still clamps to 1 (never runs away).
        MotionState over;
        over.accelX = 3.0f * G;
        CHECK(approx(tiltVector(over).x, 1.0f), "excess tilt clamps to 1");

        // Sensitivity: fullTiltG < 1 reaches full deflection sooner.
        CHECK(approx(tiltVector(half, 0.5f).y, 1.0f), "fullTiltG=0.5 makes half-g reach full deflection");
    }

    // --- 4. tiltAngleRad + deviceIsFlat: flat ~0 rad; on its side ~PI/2; the neutral is not "flat". ---
    {
        MotionState flat;
        flat.accelZ = G;
        CHECK(approx(tiltAngleRad(flat), 0.0f), "flat tilt angle is ~0");
        CHECK(deviceIsFlat(flat), "flat device reads flat");

        MotionState faceDown;
        faceDown.accelZ = -G;
        CHECK(deviceIsFlat(faceDown), "face-down also reads flat (screen up or down)");

        MotionState side;
        side.accelX = G;
        CHECK(approx(tiltAngleRad(side), 3.14159265f / 2.0f, 1e-3f), "on its side is ~90 degrees");
        CHECK(!deviceIsFlat(side), "a vertical device is not flat");

        CHECK(!deviceIsFlat(MotionState{}), "the no-IMU neutral is not a confident flat");
    }

    // --- 5. isShaking: gravity alone never trips it; a hard jerk above the threshold does. ---
    {
        MotionState still;
        still.accelZ = G;
        CHECK(!isShaking(still), "holding still (1 g) is not shaking");
        MotionState jerk;
        jerk.accelX = 2.0f * G; // 2 g total, above the 1.8 g default
        CHECK(isShaking(jerk), "a 2 g jerk is shaking");
        CHECK(!isShaking(jerk, 2.5f), "raising the threshold above the reading suppresses it");
    }

    // --- 6. lowPassFilter: alpha=1 passes the new sample through; alpha=0 holds the old; 0.5 is the mean. ---
    {
        MotionState prev;
        prev.accelX = 0.0f;
        MotionState cur;
        cur.accelX = 10.0f;
        CHECK(approx(lowPassFilter(prev, cur, 1.0f).accelX, 10.0f), "alpha=1 takes the new sample");
        CHECK(approx(lowPassFilter(prev, cur, 0.0f).accelX, 0.0f), "alpha=0 holds the previous value");
        CHECK(approx(lowPassFilter(prev, cur, 0.5f).accelX, 5.0f), "alpha=0.5 is the midpoint");
        // Out-of-range alpha is clamped, not trusted.
        CHECK(approx(lowPassFilter(prev, cur, 2.0f).accelX, 10.0f), "alpha>1 clamps to 1");
    }

    // --- 7. Backend seam: DesktopBackend reports an all-zero MotionState (no IMU). ---
    {
        DesktopBackend db("MazEngine", "MotionTest");
        const MotionState m = db.motionState();
        CHECK(!hasMotionData(m), "desktop has no motion data");
        const maz::math::vec2 t = tiltVector(m);
        CHECK(approx(t.x, 0.0f) && approx(t.y, 0.0f), "desktop tilt is zero (controls no-op)");
        CHECK(!isShaking(m), "desktop never shakes");
    }

    if (g_fail == 0) {
        std::printf("motion: OK — tilt vector, tilt angle/flat, shake, low-pass, backend seam.\n");
        return 0;
    }
    std::printf("motion: %d failure(s).\n", g_fail);
    return 1;
}
