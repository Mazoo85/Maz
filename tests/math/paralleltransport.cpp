// tests/math/paralleltransport.cpp — verifies rotation-minimizing frames (math ParallelTransport.hpp).
// Ground truths, deterministic (fixed curves, no <random>, no clock):
//   * ORTHONORMALITY: every frame's (tangent, normal, binormal) is a unit right-handed orthonormal basis;
//   * TANGENT ALIGNMENT: each frame's tangent follows the local direction of travel;
//   * ZERO TWIST on a planar curve (a circle in z=0): the frame does not spin about the tangent — with an
//     out-of-plane up-hint the normal stays exactly the plane normal at every sample (the property the
//     Frenet frame fails), while the binormal genuinely rotates in-plane to follow the curve;
//   * a straight line yields identical frames (no drift);
//   * CONTINUITY: consecutive normals never flip (dot > 0);
//   * determinism.
#include "maz/math/ParallelTransport.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::math::Frame;
using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static float dot3(const vec3& a, const vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static float len3(const vec3& a) { return std::sqrt(dot3(a, a)); }

static bool orthonormal(const Frame& f) {
    return std::fabs(len3(f.tangent) - 1.0f) < 1e-3f && std::fabs(len3(f.normal) - 1.0f) < 1e-3f &&
           std::fabs(len3(f.binormal) - 1.0f) < 1e-3f && std::fabs(dot3(f.tangent, f.normal)) < 1e-3f &&
           std::fabs(dot3(f.normal, f.binormal)) < 1e-3f && std::fabs(dot3(f.tangent, f.binormal)) < 1e-3f;
}

int main() {
    const float pi = 3.14159265358979323846f;

    // --- 1. Circle in the z=0 plane: orthonormal, zero-twist, binormal rotates. ---
    {
        const int M = 64;
        std::vector<vec3> pts;
        for (int k = 0; k < M; ++k) {
            const float t = 2.0f * pi * static_cast<float>(k) / static_cast<float>(M);
            pts.push_back(vec3(std::cos(t), std::sin(t), 0.0f));
        }
        const std::vector<Frame> frames = maz::math::parallelTransportFrames(pts, vec3(0.0f, 0.0f, 1.0f));
        CHECK(frames.size() == pts.size(), "one frame per point");

        bool allOrtho = true, normalConstant = true, contOk = true;
        for (std::size_t i = 0; i < frames.size(); ++i) {
            if (!orthonormal(frames[i])) allOrtho = false;
            // Out-of-plane up-hint => the normal is the plane normal at every sample (no twist).
            if (std::fabs(frames[i].normal.z) < 0.999f) normalConstant = false;
            // Tangent lies in the plane (z ~ 0).
            if (std::fabs(frames[i].tangent.z) > 1e-3f) allOrtho = false;
            if (i > 0 && dot3(frames[i].normal, frames[i - 1].normal) <= 0.0f) contOk = false;
        }
        CHECK(allOrtho, "every frame is a unit orthonormal basis with an in-plane tangent");
        CHECK(normalConstant, "planar curve: the normal stays the plane normal (zero twist)");
        CHECK(contOk, "consecutive normals never flip");

        // The binormal must actually rotate to follow the circle (not degenerate/constant).
        float maxBinormalSpread = 0.0f;
        for (std::size_t i = 0; i < frames.size(); ++i)
            maxBinormalSpread = std::max(maxBinormalSpread, 1.0f - dot3(frames[i].binormal, frames[0].binormal));
        CHECK(maxBinormalSpread > 1.0f, "the binormal rotates around the circle (frame follows the curve)");
    }

    // --- 2. Tangent alignment on a wavy 3D curve. ---
    {
        std::vector<vec3> pts;
        for (int k = 0; k < 40; ++k) {
            const float x = static_cast<float>(k) * 0.25f;
            pts.push_back(vec3(x, std::sin(x), 0.3f * std::cos(0.5f * x)));
        }
        const std::vector<Frame> frames = maz::math::parallelTransportFrames(pts);
        bool tangentOk = true, orthoOk = true;
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            vec3 seg = pts[i + 1] - pts[i];
            const float l = len3(seg);
            seg = vec3(seg.x / l, seg.y / l, seg.z / l);
            if (dot3(frames[i].tangent, seg) < 0.9f) tangentOk = false; // roughly aligned
            if (!orthonormal(frames[i])) orthoOk = false;
        }
        CHECK(tangentOk, "each frame's tangent follows the direction of travel");
        CHECK(orthoOk, "frames on a wavy 3D curve stay orthonormal");
    }

    // --- 3. Straight line: identical frames. ---
    {
        std::vector<vec3> pts;
        for (int k = 0; k < 10; ++k) pts.push_back(vec3(static_cast<float>(k), 0.0f, 0.0f));
        const std::vector<Frame> frames = maz::math::parallelTransportFrames(pts, vec3(0.0f, 1.0f, 0.0f));
        bool same = true;
        for (std::size_t i = 1; i < frames.size(); ++i)
            if (std::fabs(dot3(frames[i].normal, frames[0].normal) - 1.0f) > 1e-4f) same = false;
        CHECK(same, "a straight line yields identical frames (no drift)");
    }

    // --- 4. Determinism. ---
    {
        std::vector<vec3> pts{{0, 0, 0}, {1, 1, 0}, {2, 0, 1}, {3, -1, 1}, {4, 0, 0}};
        const std::vector<Frame> a = maz::math::parallelTransportFrames(pts);
        const std::vector<Frame> b = maz::math::parallelTransportFrames(pts);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); ++i)
            if (a[i].normal.x != b[i].normal.x || a[i].normal.y != b[i].normal.y ||
                a[i].normal.z != b[i].normal.z)
                same = false;
        CHECK(same, "identical inputs produce identical frames");
    }

    if (g_fail == 0) {
        std::printf("paralleltransport: OK — orthonormal, tangent, zero-twist, straight, continuity, determinism.\n");
        return 0;
    }
    std::printf("paralleltransport: %d failure(s).\n", g_fail);
    return 1;
}
