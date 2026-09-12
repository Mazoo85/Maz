// tests/math/pointdistribution.cpp — verifies even point distributions (math::fibonacciSphere / Hemisphere /
// vogelDisk). Ground truths: the requested count comes back; sphere/hemisphere points are all unit length;
// the sphere spans both poles and its centroid sits near the origin (the defining "even coverage" signal);
// the hemisphere stays on z>=0 with the same properties; the disc keeps every point within the radius, packs
// its centroid near the centre, reaches nearly the full radius, and puts its first point near the middle;
// everything is deterministic; and non-positive counts are empty. Checked against the distributions' math.
#include "maz/math/PointDistribution.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::math;

static float len3(const vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }
static float len2(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

int main() {
    // --- 1. Fibonacci sphere: count, unit length, pole coverage, centroid near origin. ---
    {
        const int n = 1000;
        const auto pts = fibonacciSphere(n);
        CHECK(static_cast<int>(pts.size()) == n, "sphere returns exactly n points");
        vec3 sum(0, 0, 0);
        float minZ = 1e9f, maxZ = -1e9f;
        for (const vec3& p : pts) {
            CHECK(std::fabs(len3(p) - 1.0f) < 1e-4f, "sphere point is unit length");
            sum += p;
            minZ = std::min(minZ, p.z);
            maxZ = std::max(maxZ, p.z);
        }
        CHECK(minZ < -0.9f && maxZ > 0.9f, "sphere reaches both poles");
        const vec3 centroid = sum / static_cast<float>(n);
        CHECK(len3(centroid) < 0.05f, "sphere centroid near origin (evenly spread)");
    }

    // --- 2. Fibonacci hemisphere: all on z>=0, unit length, top covered, centroid pulled up but centred in xy. ---
    {
        const int n = 500;
        const auto pts = fibonacciHemisphere(n);
        CHECK(static_cast<int>(pts.size()) == n, "hemisphere returns n points");
        vec3 sum(0, 0, 0);
        float maxZ = -1e9f;
        for (const vec3& p : pts) {
            CHECK(p.z >= -1e-6f, "hemisphere point stays on z >= 0");
            CHECK(std::fabs(len3(p) - 1.0f) < 1e-4f, "hemisphere point is unit length");
            sum += p;
            maxZ = std::max(maxZ, p.z);
        }
        CHECK(maxZ > 0.99f, "hemisphere reaches the top pole");
        const vec3 c = sum / static_cast<float>(n);
        CHECK(std::fabs(c.x) < 0.05f && std::fabs(c.y) < 0.05f, "hemisphere centred in x,y");
        CHECK(c.z > 0.2f, "hemisphere centroid pulled toward +z as expected");
    }

    // --- 3. Vogel disc: within radius, centroid near centre, reaches the rim, first point near middle. ---
    {
        const int n = 1000;
        const float radius = 4.0f;
        const auto pts = vogelDisk(n, radius);
        CHECK(static_cast<int>(pts.size()) == n, "disc returns n points");
        vec2 sum(0, 0);
        float maxR = 0.0f;
        for (const vec2& p : pts) {
            const float r = len2(p);
            CHECK(r <= radius + 1e-4f, "disc point stays within the radius");
            sum += p;
            maxR = std::max(maxR, r);
        }
        const vec2 centroid = sum / static_cast<float>(n);
        CHECK(len2(centroid) < 0.05f * radius, "disc centroid near the centre (evenly spread)");
        CHECK(maxR > 0.95f * radius, "disc coverage reaches nearly the full radius");
        CHECK(len2(pts[0]) < 0.1f * radius, "first disc point sits near the centre");
    }

    // --- 4. Determinism and degenerate counts. ---
    {
        const auto a = fibonacciSphere(64);
        const auto b = fibonacciSphere(64);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; i < a.size() && same; ++i)
            if (a[i].x != b[i].x || a[i].y != b[i].y || a[i].z != b[i].z) same = false;
        CHECK(same, "fibonacciSphere is deterministic");

        CHECK(fibonacciSphere(0).empty() && fibonacciSphere(-3).empty(), "sphere: non-positive n -> empty");
        CHECK(fibonacciHemisphere(0).empty() && vogelDisk(0).empty(), "hemisphere/disc: n=0 -> empty");
        CHECK(fibonacciSphere(1).size() == 1, "sphere: n=1 yields one point");
    }

    if (g_fail == 0) {
        std::printf("pointdistribution: OK — sphere unit/poles/centroid, hemisphere z>=0, disc radius/centroid, determinism.\n");
        return 0;
    }
    std::printf("pointdistribution: %d failure(s).\n", g_fail);
    return 1;
}
