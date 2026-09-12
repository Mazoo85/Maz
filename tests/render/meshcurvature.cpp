// tests/render/meshcurvature.cpp — verifies per-vertex discrete curvature estimation
// (render::computeCurvature). Ground truths of differential geometry: a sphere of radius r has Gaussian
// curvature K = 1/r^2 and mean curvature H = 1/r everywhere; a flat sheet has both zero. Built on a welded
// UV sphere and a subdivided plane grid, plus a boundary-flag check on an open mesh. Pure CPU, headless.
#include "maz/render/MeshCurvature.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// A fully welded UV sphere: single top/bottom pole, no duplicated seam column. rings latitude bands,
// sectors longitude columns. Closed, manifold — every vertex has a well-defined curvature.
static shapes::MeshData weldedSphere(float r, int rings, int sectors) {
    shapes::MeshData m;
    const double pi = 3.14159265358979323846;
    // Vertices: top pole, then (rings-1) interior latitude rings of `sectors` verts, then bottom pole.
    m.vertices.push_back(vtx(0.0f, r, 0.0f)); // index 0: top pole (+Y)
    for (int i = 1; i < rings; ++i) {
        const double theta = pi * static_cast<double>(i) / rings; // 0..pi from top
        const double y = std::cos(theta) * r;
        const double rr = std::sin(theta) * r;
        for (int j = 0; j < sectors; ++j) {
            const double phi = 2.0 * pi * static_cast<double>(j) / sectors;
            m.vertices.push_back(vtx(static_cast<float>(std::cos(phi) * rr),
                                     static_cast<float>(y),
                                     static_cast<float>(std::sin(phi) * rr)));
        }
    }
    const std::uint32_t bottom = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back(vtx(0.0f, -r, 0.0f)); // bottom pole (-Y)

    auto ringVert = [&](int ring, int j) -> std::uint32_t { // ring in 1..rings-1
        return static_cast<std::uint32_t>(1 + (ring - 1) * sectors + (j % sectors));
    };
    // Top cap fan.
    for (int j = 0; j < sectors; ++j)
        m.indices.insert(m.indices.end(), {0u, ringVert(1, j), ringVert(1, j + 1)});
    // Middle bands.
    for (int i = 1; i < rings - 1; ++i)
        for (int j = 0; j < sectors; ++j) {
            const std::uint32_t a = ringVert(i, j),   b = ringVert(i + 1, j);
            const std::uint32_t c = ringVert(i + 1, j + 1), d = ringVert(i, j + 1);
            m.indices.insert(m.indices.end(), {a, b, d, d, b, c});
        }
    // Bottom cap fan.
    for (int j = 0; j < sectors; ++j)
        m.indices.insert(m.indices.end(), {bottom, ringVert(rings - 1, j + 1), ringVert(rings - 1, j)});
    return m;
}

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Unit sphere: K ~ 1, H ~ 1 at interior vertices (averaged for robustness). ---
    {
        const shapes::MeshData s = weldedSphere(1.0f, 24, 24);
        const MeshCurvature c = computeCurvature(s);
        double sumK = 0, sumH = 0; int n = 0;
        double worstK = 0, worstH = 0;
        for (std::size_t i = 0; i < c.vertexCount; ++i) {
            if (c.boundary[i]) continue;
            sumK += c.gaussian[i]; sumH += c.mean[i]; ++n;
            worstK = std::max(worstK, std::fabs(c.gaussian[i] - 1.0));
            worstH = std::max(worstH, std::fabs(c.mean[i] - 1.0));
        }
        CHECK(n > 0, "sphere has interior vertices");
        const double avgK = sumK / n, avgH = sumH / n;
        CHECK(near(avgK, 1.0, 0.08), "unit sphere average Gaussian curvature ~ 1");
        CHECK(near(avgH, 1.0, 0.08), "unit sphere average mean curvature ~ 1");
        CHECK(worstK < 0.35, "no interior vertex has wildly wrong Gaussian curvature");
        CHECK(worstH < 0.35, "no interior vertex has wildly wrong mean curvature");
        // Sphere is convex everywhere -> Gaussian strictly positive.
        bool allPos = true;
        for (std::size_t i = 0; i < c.vertexCount; ++i)
            if (!c.boundary[i] && c.gaussian[i] <= 0.0f) allPos = false;
        CHECK(allPos, "sphere Gaussian curvature is positive everywhere");
    }

    // --- 2. Radius-2 sphere: curvature scales as K ~ 1/r^2 = 0.25, H ~ 1/r = 0.5. ---
    {
        const shapes::MeshData s = weldedSphere(2.0f, 24, 24);
        const MeshCurvature c = computeCurvature(s);
        double sumK = 0, sumH = 0; int n = 0;
        for (std::size_t i = 0; i < c.vertexCount; ++i) {
            if (c.boundary[i]) continue;
            sumK += c.gaussian[i]; sumH += c.mean[i]; ++n;
        }
        CHECK(near(sumK / n, 0.25, 0.03), "radius-2 sphere average Gaussian ~ 1/4");
        CHECK(near(sumH / n, 0.50, 0.03), "radius-2 sphere average mean ~ 1/2");
    }

    // --- 3. Flat grid: interior curvature ~ 0, rim flagged as boundary. ---
    {
        shapes::MeshData g;
        const int N = 8; // (N+1)^2 vertices in the XZ plane
        for (int z = 0; z <= N; ++z)
            for (int x = 0; x <= N; ++x)
                g.vertices.push_back(vtx(static_cast<float>(x), 0.0f, static_cast<float>(z)));
        auto id = [&](int x, int z) { return static_cast<std::uint32_t>(z * (N + 1) + x); };
        for (int z = 0; z < N; ++z)
            for (int x = 0; x < N; ++x)
                g.indices.insert(g.indices.end(),
                                 {id(x, z), id(x + 1, z), id(x + 1, z + 1),
                                  id(x, z), id(x + 1, z + 1), id(x, z + 1)});
        const MeshCurvature c = computeCurvature(g);
        double worstK = 0, worstH = 0; int interior = 0, boundary = 0;
        for (std::size_t i = 0; i < c.vertexCount; ++i) {
            if (c.boundary[i]) { ++boundary; continue; }
            ++interior;
            worstK = std::max(worstK, std::fabs(static_cast<double>(c.gaussian[i])));
            worstH = std::max(worstH, std::fabs(static_cast<double>(c.mean[i])));
        }
        CHECK(interior == (N - 1) * (N - 1), "flat grid interior vertex count is correct");
        CHECK(boundary == (N + 1) * (N + 1) - (N - 1) * (N - 1), "flat grid rim is flagged boundary");
        CHECK(worstK < 1e-4, "flat grid interior Gaussian curvature is ~ 0");
        CHECK(worstH < 1e-4, "flat grid interior mean curvature is ~ 0");
    }

    // --- 4. Empty mesh is safe. ---
    {
        const MeshCurvature c = computeCurvature(shapes::MeshData{});
        CHECK(c.vertexCount == 0 && c.gaussian.empty() && c.mean.empty(), "empty mesh -> empty result");
    }

    if (g_fail == 0) {
        std::printf("meshcurvature: OK — unit sphere K~1/H~1, r=2 sphere K~.25/H~.5, flat grid ~0, rim flagged.\n");
        return 0;
    }
    std::printf("meshcurvature: %d failure(s).\n", g_fail);
    return 1;
}
