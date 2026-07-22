// tests/render/meshgeodesic.cpp — verifies mesh geodesic distance (render::geodesicDistance), Dijkstra over the
// vertex/edge graph. On a regular flat grid whose quads are split along the main diagonal, the shortest edge
// path from a corner runs straight along the bottom row (dist == x*spacing) and straight down the diagonal
// (dist(n,n) == n*sqrt(2)*spacing) — both EQUAL to the Euclidean straight line, so the estimate is exact there.
// Also checks multi-source nearest-feature seeding, unreachable islands, path reconstruction, empty safety.
#include "maz/render/MeshGeodesic.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// N x N grid of quads (each split along the (x,z)->(x+1,z+1) diagonal) with the given cell spacing, on XZ.
static shapes::MeshData grid(int N, float h) {
    shapes::MeshData m;
    for (int z = 0; z <= N; ++z)
        for (int x = 0; x <= N; ++x)
            m.vertices.push_back(vtx(static_cast<float>(x) * h, 0.0f, static_cast<float>(z) * h));
    for (int z = 0; z < N; ++z)
        for (int x = 0; x < N; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(z * (N + 1) + x);
            const std::uint32_t b = static_cast<std::uint32_t>(z * (N + 1) + x + 1);
            const std::uint32_t c = static_cast<std::uint32_t>((z + 1) * (N + 1) + x + 1);
            const std::uint32_t d = static_cast<std::uint32_t>((z + 1) * (N + 1) + x);
            m.indices.insert(m.indices.end(), {a, b, c, a, c, d}); // diagonal a-c
        }
    return m;
}

int main() {
    const int N = 6;
    const float h = 0.5f;
    auto id = [&](int x, int z) { return static_cast<std::uint32_t>(z * (N + 1) + x); };

    // --- 1. Single source at corner (0,0): exact distances along row and main diagonal. ---
    {
        const shapes::MeshData m = grid(N, h);
        const GeodesicResult r = geodesicDistance(m, id(0, 0));
        CHECK(r.distance[id(0, 0)] == 0.0f, "source distance is zero");
        CHECK(near(r.distance[id(N, 0)], static_cast<float>(N) * h, 1e-4f), "distance along the bottom row is x*spacing");
        CHECK(near(r.distance[id(0, N)], static_cast<float>(N) * h, 1e-4f), "distance up the left column is z*spacing");
        const float diag = static_cast<float>(N) * h * std::sqrt(2.0f);
        CHECK(near(r.distance[id(N, N)], diag, 1e-4f), "distance to the far corner is n*sqrt(2)*spacing (the diagonal)");
        // Geodesic can never be shorter than the Euclidean straight line.
        bool ok = true;
        for (int z = 0; z <= N; ++z)
            for (int x = 0; x <= N; ++x) {
                const float euclid = std::sqrt(static_cast<float>(x * x + z * z)) * h;
                if (r.distance[id(x, z)] + 1e-4f < euclid) ok = false;
            }
        CHECK(ok, "no geodesic distance is shorter than the straight-line distance");
    }

    // --- 2. Path reconstruction from far corner returns a chain that ends at the source. ---
    {
        const shapes::MeshData m = grid(N, h);
        const GeodesicResult r = geodesicDistance(m, id(0, 0));
        const std::vector<std::uint32_t> path = r.pathFrom(id(N, N));
        CHECK(!path.empty(), "path to far corner exists");
        CHECK(path.front() == id(N, N), "path starts at the target");
        CHECK(path.back() == id(0, 0), "path ends at the source");
        CHECK(path.size() == static_cast<std::size_t>(N) + 1, "diagonal path visits N+1 vertices");
    }

    // --- 3. Multi-source: each vertex takes the distance to its NEAREST source. ---
    {
        const shapes::MeshData m = grid(N, h);
        const GeodesicResult r = geodesicDistance(m, std::vector<std::uint32_t>{id(0, 0), id(N, N)});
        CHECK(r.distance[id(0, 0)] == 0.0f && r.distance[id(N, N)] == 0.0f, "both sources are at zero");
        // A vertex nearer the second source measures from it, not the first.
        const float fromA = static_cast<float>(N) * h;                 // (N,0) from (0,0)
        const float fromB = static_cast<float>(N) * h;                 // (N,0) from (N,N)
        CHECK(near(r.distance[id(N, 0)], std::min(fromA, fromB), 1e-4f), "corner takes nearest of the two sources");
        // Every vertex is <= its single-source distance from source A.
        const GeodesicResult ra = geodesicDistance(m, id(0, 0));
        bool le = true;
        for (std::size_t v = 0; v < r.distance.size(); ++v)
            if (r.distance[v] > ra.distance[v] + 1e-4f) le = false;
        CHECK(le, "multi-source distance never exceeds the single-source distance");
    }

    // --- 4. Disconnected island is unreachable (infinite distance). ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0),      // triangle A
                      vtx(9,0,0), vtx(10,0,0), vtx(9,1,0)};    // triangle B, far away, no shared edge
        m.indices = {0,1,2, 3,4,5};
        const GeodesicResult r = geodesicDistance(m, 0u);
        CHECK(r.reachable(1) && r.reachable(2), "same-island vertices are reachable");
        CHECK(!r.reachable(3) && !r.reachable(4) && !r.reachable(5), "other-island vertices are unreachable");
        CHECK(r.pathFrom(4).empty(), "no path to an unreachable vertex");
    }

    // --- 5. Empty mesh / bad source are safe. ---
    {
        const GeodesicResult r = geodesicDistance(shapes::MeshData{}, 0u);
        CHECK(r.distance.empty() && r.previous.empty(), "empty mesh -> empty result");
        const shapes::MeshData m = grid(2, 1.0f);
        const GeodesicResult r2 = geodesicDistance(m, 999u); // out-of-range source ignored
        bool allInf = true;
        for (float d : r2.distance) if (std::isfinite(d)) allInf = false;
        CHECK(allInf, "out-of-range source leaves all distances infinite");
    }

    if (g_fail == 0) {
        std::printf("meshgeodesic: OK — grid row/diagonal exact, multi-source nearest, islands unreachable, paths.\n");
        return 0;
    }
    std::printf("meshgeodesic: %d failure(s).\n", g_fail);
    return 1;
}
