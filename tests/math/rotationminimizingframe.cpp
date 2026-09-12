// tests/math/rotationminimizingframe.cpp — verifies rotation-minimizing frames (RotationMinimizingFrame.hpp).
// Ground truths, deterministic:
//   * every frame is orthonormal (unit, mutually perpendicular, right-handed binormal = t x n);
//   * along a straight line the reference normal does NOT rotate;
//   * along a planar curve the binormal stays constant (perpendicular to the plane) — minimal twist;
//   * across an S-curve with an inflection the frame stays continuous (no 180° Frenet flip).
#include "maz/math/RotationMinimizingFrame.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::Frame;
using maz::math::rotationMinimizingFrames;
using maz::math::vec3;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }
static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float len(const vec3& v) { return std::sqrt(dot3(v, v)); }

static void checkOrthonormal(const std::vector<Frame>& fs, const char* label) {
    bool ok = true;
    for (const Frame& f : fs) {
        if (!near(len(f.tangent), 1.0f, 1e-3f) || !near(len(f.normal), 1.0f, 1e-3f) ||
            !near(len(f.binormal), 1.0f, 1e-3f))
            ok = false;
        if (!near(dot3(f.tangent, f.normal), 0.0f, 1e-3f) ||
            !near(dot3(f.tangent, f.binormal), 0.0f, 1e-3f) ||
            !near(dot3(f.normal, f.binormal), 0.0f, 1e-3f))
            ok = false;
        // Right-handed: binormal == tangent x normal.
        const vec3 cx(f.tangent.y * f.normal.z - f.tangent.z * f.normal.y,
                      f.tangent.z * f.normal.x - f.tangent.x * f.normal.z,
                      f.tangent.x * f.normal.y - f.tangent.y * f.normal.x);
        if (!near(cx.x, f.binormal.x, 1e-3f) || !near(cx.y, f.binormal.y, 1e-3f) ||
            !near(cx.z, f.binormal.z, 1e-3f))
            ok = false;
    }
    CHECK(ok, label);
}

int main() {
    // --- 1. Straight line: no rotation. ---
    {
        std::vector<vec3> pts, tans;
        for (int i = 0; i < 10; ++i) {
            pts.push_back(vec3(static_cast<float>(i), 0, 0));
            tans.push_back(vec3(1, 0, 0));
        }
        const std::vector<Frame> fs = rotationMinimizingFrames(pts, tans, vec3(0, 1, 0));
        checkOrthonormal(fs, "straight line: frames orthonormal");
        bool constant = true;
        for (const Frame& f : fs)
            if (!near(f.normal.x, 0) || !near(f.normal.y, 1) || !near(f.normal.z, 0)) constant = false;
        CHECK(constant, "straight line: the reference normal never rotates");
    }

    // --- 2. Planar curve (quarter circle in xy-plane): binormal stays constant (out of plane). ---
    {
        std::vector<vec3> pts, tans;
        const int n = 24;
        for (int i = 0; i < n; ++i) {
            const float a = 1.5707963f * static_cast<float>(i) / static_cast<float>(n - 1); // 0..pi/2
            pts.push_back(vec3(std::cos(a), std::sin(a), 0));
            tans.push_back(vec3(-std::sin(a), std::cos(a), 0)); // unit tangent along the circle
        }
        const std::vector<Frame> fs = rotationMinimizingFrames(pts, tans, vec3(0, 0, 1));
        checkOrthonormal(fs, "planar curve: frames orthonormal");
        // With the seed normal out of plane (+z), the tangent is in-plane, so the frame's normal stays
        // +z and the binormal stays in-plane but constant is not guaranteed; instead verify the normal
        // stays +z (the axis perpendicular to the plane never twists).
        bool normalConstant = true;
        for (const Frame& f : fs)
            if (!near(std::fabs(f.normal.z), 1.0f, 1e-3f)) normalConstant = false;
        CHECK(normalConstant, "planar curve: the out-of-plane axis carries through without twist");
    }

    // --- 3. Orthonormality + continuity on a 3D helix (a real twisting path). ---
    {
        std::vector<vec3> pts, tans;
        const int n = 40;
        for (int i = 0; i < n; ++i) {
            const float t = 6.2831853f * static_cast<float>(i) / static_cast<float>(n - 1); // two turns-ish
            pts.push_back(vec3(std::cos(t), std::sin(t), 0.3f * t));
            vec3 d(-std::sin(t), std::cos(t), 0.3f);
            const float l = len(d);
            tans.push_back(vec3(d.x / l, d.y / l, d.z / l));
        }
        const std::vector<Frame> fs = rotationMinimizingFrames(pts, tans, vec3(0, 0, 1));
        checkOrthonormal(fs, "helix: frames orthonormal");
        // Continuity: consecutive normals never jump (a Frenet flip would be a ~180° reversal).
        bool continuous = true;
        for (std::size_t i = 1; i < fs.size(); ++i)
            if (dot3(fs[i].normal, fs[i - 1].normal) < 0.0f) continuous = false;
        CHECK(continuous, "helix: the frame never flips between samples");
    }

    // --- 4. S-curve with an inflection (where a Frenet frame flips) stays continuous. ---
    {
        std::vector<vec3> pts, tans;
        const int n = 40;
        for (int i = 0; i < n; ++i) {
            const float x = -3.0f + 6.0f * static_cast<float>(i) / static_cast<float>(n - 1);
            pts.push_back(vec3(x, std::sin(x), 0)); // an S in the xy-plane, curvature changes sign at x=0
            vec3 d(1.0f, std::cos(x), 0.0f);
            const float l = len(d);
            tans.push_back(vec3(d.x / l, d.y / l, d.z / l));
        }
        const std::vector<Frame> fs = rotationMinimizingFrames(pts, tans, vec3(0, 0, 1));
        checkOrthonormal(fs, "S-curve: frames orthonormal");
        bool continuous = true;
        for (std::size_t i = 1; i < fs.size(); ++i)
            if (dot3(fs[i].normal, fs[i - 1].normal) < 0.5f) continuous = false; // no near-flip
        CHECK(continuous, "S-curve: no 180-degree frame flip at the inflection");
    }

    if (g_fail == 0) {
        std::printf("rotationminimizingframe: OK — orthonormal, straight-line steady, planar axis, "
                    "helix continuity, S-curve no flip.\n");
        return 0;
    }
    std::printf("rotationminimizingframe: %d failure(s).\n", g_fail);
    return 1;
}
