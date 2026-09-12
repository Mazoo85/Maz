// tests/render/ssr.cpp — verifies the screen-space-reflection ray-march (render::ssrTrace) against
// hand-built depth buffers. All pure projection + depth-comparison math (the exact arithmetic the SSR
// shader runs), so it is provable headlessly: a ray aimed at a wall lands on that wall's UV at the wall's
// depth, a ray into empty screen or off the edge misses, and reflect() mirrors a vector about a normal.
#include "maz/render/ScreenSpaceReflection.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

int main() {
    const float pi = 3.14159265358979323846f;
    const SsrCamera cam = SsrCamera::fromFovY(pi / 3.0f, 1.0f); // 60° vertical FOV, square aspect

    SsrParams params;
    params.maxDistance = 30.0f;
    params.maxSteps = 256;
    params.thickness = 0.6f;
    params.refineSteps = 12;

    // --- 1. Ray aimed at a flat wall (constant view-space depth) must hit at the predicted UV. ---
    {
        DepthBuffer depth(64, 64, 10.0f); // a wall filling the screen at z = 10
        // Fragment at (-2,0,4), reflection ray heading deeper and to the right: (1,0,1).
        // Analytic hit: z reaches 10 at t=(10-4)/(1/sqrt2)=8.485, x=-2+0.7071*8.485=4.0 -> view (4,0,10).
        // u = 0.5 + focalX*(4/10), v = 0.5.
        const SsrHit h = ssrTrace(cam, depth, -2.0f, 0.0f, 4.0f, 1.0f, 0.0f, 1.0f, params);
        CHECK(h.hit, "ray hits the wall");
        const float expectedU = 0.5f + cam.focalX * (4.0f / 10.0f);
        CHECK(std::fabs(h.u - expectedU) < 0.02f, "hit U matches analytic reflection point");
        CHECK(std::fabs(h.v - 0.5f) < 0.02f, "hit V stays on the centerline");
        CHECK(std::fabs(h.distance - 8.485f) < 0.2f, "hit distance matches analytic travel");
        CHECK(h.confidence > 0.0f && h.confidence <= 1.0f, "confidence in (0,1]");
    }

    // --- 2. Empty screen (all sky) must miss no matter where the ray points. ---
    {
        DepthBuffer sky(64, 64, -1.0f);
        const SsrHit h = ssrTrace(cam, sky, 0.0f, 0.0f, 4.0f, 1.0f, 0.0f, 1.0f, params);
        CHECK(!h.hit, "ray through empty screen misses");
    }

    // --- 3. Ray that leaves the screen before reaching any surface must miss (SSR can't reflect off-screen). ---
    {
        DepthBuffer wall(64, 64, 30.0f); // far wall
        // Shallow ray: mostly +X, barely +Z -> exits the right edge long before z reaches 30.
        const SsrHit h = ssrTrace(cam, wall, 0.0f, 0.0f, 4.0f, 1.0f, 0.0f, 0.1f, params);
        CHECK(!h.hit, "ray leaving the screen misses");
    }

    // --- 4. A nearer surface the ray flies high above (diff >> thickness) must NOT false-hit. ---
    {
        DepthBuffer floor(64, 64, 2.0f); // everything reads as a very near surface at z=2
        // Fragment already far behind that depth, ray going deeper: it is always >thickness behind -> no hit.
        const SsrHit h = ssrTrace(cam, floor, 0.0f, 0.0f, 8.0f, 0.0f, 0.0f, 1.0f, params);
        CHECK(!h.hit, "ray far behind a near surface does not false-hit (thickness gate)");
    }

    // --- 5. reflect(): mirror a downward view ray about an up normal -> ray goes up, horizontal preserved. ---
    {
        float rx, ry, rz;
        reflect(1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, rx, ry, rz);
        CHECK(std::fabs(rx - 1.0f) < 1e-5f, "reflect keeps tangential X");
        CHECK(std::fabs(ry - 1.0f) < 1e-5f, "reflect flips normal-direction Y");
        CHECK(std::fabs(rz - 0.0f) < 1e-5f, "reflect keeps Z");
    }

    // --- 6. Projection round-trip: a known view point projects to the expected UV. ---
    {
        float u, v;
        const bool ok = ssrProject(cam, 0.0f, 0.0f, 5.0f, u, v);
        CHECK(ok && std::fabs(u - 0.5f) < 1e-5f && std::fabs(v - 0.5f) < 1e-5f, "on-axis point projects to screen center");
        float u2, v2;
        CHECK(!ssrProject(cam, 0.0f, 0.0f, -1.0f, u2, v2), "behind-camera point rejected");
    }

    if (g_fail == 0) {
        std::printf("ssr: OK — wall hit UV/depth, sky miss, off-screen miss, thickness gate, reflect, project.\n");
        return 0;
    }
    std::printf("ssr: %d failure(s).\n", g_fail);
    return 1;
}
