// tests/render/meshsimplifyquadric.cpp — verifies feature-preserving quadric-error edge-collapse
// simplification (render::simplifyQuadric). A correct QEM decimation: reaches the triangle budget, keeps
// every output triangle non-degenerate + in range, preserves the bounding box and a curved surface's peak
// (flat interior collapses before the silhouette), is deterministic, and is a no-op when the budget already
// exceeds the input. The source is a curved height-field (a central bump) so the quadrics carry real
// curvature — a flat grid would be degenerate for QEM. Pure CPU, headless.
#include "maz/render/MeshSimplifyQuadric.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// A (w+1)x(h+1) height-field grid over [0,w]x[0,h], py = a single smooth bump peaking at the center and
// falling to 0 at the borders. Two triangles per quad; unique vertices (welding is a no-op).
static shapes::MeshData bumpGrid(int w, int h, float amp) {
    shapes::MeshData m;
    const float pi = 3.14159265358979323846f;
    for (int y = 0; y <= h; ++y) {
        for (int x = 0; x <= w; ++x) {
            MeshVertex v{};
            v.px = static_cast<float>(x);
            v.pz = static_cast<float>(y);
            v.py = amp * std::sin(static_cast<float>(x) / static_cast<float>(w) * pi)
                       * std::sin(static_cast<float>(y) / static_cast<float>(h) * pi);
            v.r = v.g = v.b = 1.0f;
            m.vertices.push_back(v);
        }
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(y * (w + 1) + x);
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + static_cast<std::uint32_t>(w + 1);
            const std::uint32_t d = c + 1;
            m.indices.insert(m.indices.end(), {a, b, c, b, d, c});
        }
    }
    return m;
}

static void bbox(const shapes::MeshData& m, float& mnx, float& mny, float& mnz,
                 float& mxx, float& mxy, float& mxz) {
    mnx = mny = mnz = 1e9f; mxx = mxy = mxz = -1e9f;
    for (const MeshVertex& v : m.vertices) {
        mnx = std::min(mnx, v.px); mxx = std::max(mxx, v.px);
        mny = std::min(mny, v.py); mxy = std::max(mxy, v.py);
        mnz = std::min(mnz, v.pz); mxz = std::max(mxz, v.pz);
    }
}

static bool allTrisValid(const shapes::MeshData& m) {
    if (m.indices.size() % 3 != 0) return false;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const std::uint32_t a = m.indices[t], b = m.indices[t + 1], c = m.indices[t + 2];
        if (a == b || b == c || a == c) return false;
        if (a >= m.vertices.size() || b >= m.vertices.size() || c >= m.vertices.size()) return false;
    }
    return true;
}

int main() {
    const int w = 20, h = 20;
    const float amp = 5.0f;
    const shapes::MeshData src = bumpGrid(w, h, amp); // 441 verts, 800 tris
    const std::size_t srcTris = src.indices.size() / 3;

    // --- 1. Decimate to ~1/4 of the triangles: budget met, all triangles valid. ---
    {
        const std::size_t target = srcTris / 4;
        const shapes::MeshData out = simplifyQuadric(src, target);
        CHECK(!out.indices.empty(), "produced a non-empty mesh");
        CHECK(out.indices.size() / 3 <= target, "triangle budget respected");
        CHECK(out.indices.size() / 3 < srcTris, "triangle count reduced");
        CHECK(out.vertices.size() < src.vertices.size(), "vertex count reduced");
        CHECK(allTrisValid(out), "every output triangle is non-degenerate and in range");

        // Bounding box preserved: QEM keeps the boundary (high error there) so X/Z extent is intact, and
        // the central bump's peak is not flattened away.
        float smnx, smny, smnz, smxx, smxy, smxz;
        float omnx, omny, omnz, omxx, omxy, omxz;
        bbox(src, smnx, smny, smnz, smxx, smxy, smxz);
        bbox(out, omnx, omny, omnz, omxx, omxy, omxz);
        CHECK(std::fabs(omnx - smnx) <= 1.0f && std::fabs(omxx - smxx) <= 1.0f, "X extent preserved");
        CHECK(std::fabs(omnz - smnz) <= 1.0f && std::fabs(omxz - smxz) <= 1.0f, "Z extent preserved");
        CHECK(omxy >= smxy * 0.6f, "curved peak survives (silhouette preserved, not flattened)");

        // Output normals are unit-length.
        bool unit = true;
        for (const MeshVertex& v : out.vertices) {
            const float l = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (std::fabs(l - 1.0f) > 1e-3f) { unit = false; break; }
        }
        CHECK(unit, "recomputed normals are unit-length");
    }

    // --- 2. A tighter budget yields even fewer triangles (monotone). ---
    {
        const std::size_t coarse = simplifyQuadric(src, srcTris / 8).indices.size() / 3;
        const std::size_t fine = simplifyQuadric(src, srcTris / 2).indices.size() / 3;
        CHECK(coarse < fine, "a smaller budget produces fewer triangles");
        CHECK(coarse > 0, "even the coarse budget keeps some geometry");
    }

    // --- 3. Deterministic: identical input yields byte-identical output. ---
    {
        const shapes::MeshData a = simplifyQuadric(src, srcTris / 4);
        const shapes::MeshData b = simplifyQuadric(src, srcTris / 4);
        bool same = a.indices == b.indices && a.vertices.size() == b.vertices.size();
        if (same)
            for (std::size_t i = 0; i < a.vertices.size(); ++i)
                if (a.vertices[i].px != b.vertices[i].px || a.vertices[i].py != b.vertices[i].py ||
                    a.vertices[i].pz != b.vertices[i].pz) { same = false; break; }
        CHECK(same, "simplification is deterministic");
    }

    // --- 4. A budget at/above the input is a no-op (no collapses). ---
    {
        const shapes::MeshData out = simplifyQuadric(src, srcTris);
        CHECK(out.indices.size() / 3 == srcTris, "no triangles dropped when budget >= input");
        CHECK(out.vertices.size() == src.vertices.size(), "no vertices merged when budget >= input");
    }

    if (g_fail == 0) {
        std::printf("meshsimplifyquadric: OK — QEM hits budget, valid tris, bbox+peak kept, deterministic.\n");
        return 0;
    }
    std::printf("meshsimplifyquadric: %d failure(s).\n", g_fail);
    return 1;
}
