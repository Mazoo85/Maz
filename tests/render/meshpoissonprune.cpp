// tests/render/meshpoissonprune.cpp — verifies blue-noise pruning of surface points (render::prunePointsPoisson /
// scatterBlueNoise). Ground truths: after pruning, NO two kept points are closer than minDistance; earlier points
// win over later ones; a larger radius keeps fewer points; minDistance<=0 keeps everything; scattering on a real
// mesh gives a well-spaced subset. Pure CPU, headless.
#include "maz/render/MeshPoissonPrune.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static SurfacePoint sp(float x, float y, float z) {
    SurfacePoint p; p.position = maz::math::vec3(x, y, z); p.normal = maz::math::vec3(0, 1, 0); return p;
}
static float dist(const SurfacePoint& a, const SurfacePoint& b) {
    const maz::math::vec3 d = a.position - b.position;
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}
// Assert the spacing invariant over a kept set.
static bool wellSpaced(const std::vector<SurfacePoint>& pts, float minDistance) {
    for (std::size_t i = 0; i < pts.size(); ++i)
        for (std::size_t j = i + 1; j < pts.size(); ++j)
            if (dist(pts[i], pts[j]) < minDistance - 1e-5f) return false;
    return true;
}

// A flat 10x10 grid of points spaced 1 apart (100 points), as a dense clumpable cloud.
static std::vector<SurfacePoint> grid10() {
    std::vector<SurfacePoint> v;
    for (int gx = 0; gx < 10; ++gx)
        for (int gz = 0; gz < 10; ++gz)
            v.push_back(sp(static_cast<float>(gx), 0.0f, static_cast<float>(gz)));
    return v;
}

int main() {
    // --- 1. Two points closer than the radius: the second is dropped, the first is kept. ---
    {
        const std::vector<SurfacePoint> in = {sp(0, 0, 0), sp(0.1f, 0, 0)};
        const std::vector<SurfacePoint> out = prunePointsPoisson(in, 0.5f);
        CHECK(out.size() == 1, "a point within the radius of an earlier one is dropped");
        CHECK(out[0].position.x == 0.0f, "the EARLIER point is the one kept");
    }

    // --- 2. Two points farther apart than the radius: both survive. ---
    {
        const std::vector<SurfacePoint> in = {sp(0, 0, 0), sp(1, 0, 0)};
        const std::vector<SurfacePoint> out = prunePointsPoisson(in, 0.5f);
        CHECK(out.size() == 2, "points beyond the radius both survive");
    }

    // --- 3. Spacing invariant holds on a dense grid, and a larger radius keeps strictly fewer. ---
    {
        const std::vector<SurfacePoint> in = grid10();
        const std::vector<SurfacePoint> r15 = prunePointsPoisson(in, 1.5f);
        const std::vector<SurfacePoint> r3 = prunePointsPoisson(in, 3.0f);
        CHECK(wellSpaced(r15, 1.5f), "no two kept points are closer than 1.5");
        CHECK(wellSpaced(r3, 3.0f), "no two kept points are closer than 3.0");
        CHECK(!r15.empty() && r15.size() < in.size(), "pruning at 1.5 thins the 100-point grid");
        CHECK(r3.size() < r15.size(), "a bigger radius keeps fewer points");
        // The normal came along for the ride.
        CHECK(r15[0].normal.y == 1.0f, "kept points retain their normal");
    }

    // --- 4. minDistance <= 0 keeps everything unchanged. ---
    {
        const std::vector<SurfacePoint> in = grid10();
        CHECK(prunePointsPoisson(in, 0.0f).size() == in.size(), "radius 0 keeps every point");
        CHECK(prunePointsPoisson(in, -1.0f).size() == in.size(), "negative radius keeps every point");
    }

    // --- 5. Empty input is safe. ---
    {
        CHECK(prunePointsPoisson(std::vector<SurfacePoint>{}, 1.0f).empty(), "empty in -> empty out");
    }

    // --- 6. End-to-end scatter on a real mesh (a quad) gives a well-spaced subset. ---
    {
        shapes::MeshData m; // 2x2 quad in the XZ plane
        MeshVertex a{}, b{}, c{}, d{};
        a.px = 0; a.pz = 0; b.px = 2; b.pz = 0; c.px = 2; c.pz = 2; d.px = 0; d.pz = 2;
        a.ny = b.ny = c.ny = d.ny = 1.0f;
        m.vertices = {a, b, c, d};
        m.indices = {0, 1, 2, 0, 2, 3};
        const std::vector<SurfacePoint> scattered = scatterBlueNoise(m, 0.4f, 400, 7u);
        CHECK(!scattered.empty(), "scattering produced points");
        CHECK(wellSpaced(scattered, 0.4f), "scattered points respect the 0.4 spacing");
    }

    if (g_fail == 0) {
        std::printf("meshpoissonprune: OK — kept points respect the spacing, earlier wins, bigger radius keeps fewer.\n");
        return 0;
    }
    std::printf("meshpoissonprune: %d failure(s).\n", g_fail);
    return 1;
}
