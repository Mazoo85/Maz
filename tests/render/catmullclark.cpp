// tests/render/catmullclark.cpp — verifies Catmull-Clark subdivision (render CatmullClark.hpp).
// Ground truths, deterministic (fixed cube + planar grid, no <random>, no clock):
//   * TOPOLOGY: one pass of a cube (V=8,E=12,F=6) yields the exact Catmull-Clark counts — V'=V+F+E=26,
//     F'=4F=24, and E'=48, so the Euler characteristic V-E+F=2 is preserved (still a closed sphere);
//   * every output face is a quad;
//   * BOUNDING: all new points stay inside the control cube's box, and each original corner moves strictly
//     inward toward the centre (the corners round off);
//   * PLANARITY: subdividing a flat z=0 grid keeps every vertex in the z=0 plane (a flat cage stays flat);
//   * determinism.
#include "maz/render/CatmullClark.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_set>
#include <vector>

using maz::math::vec3;
using maz::render::PolyMesh;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static std::size_t countEdges(const PolyMesh& m) {
    std::unordered_set<std::uint64_t> set;
    for (const auto& f : m.faces) {
        const std::size_t k = f.size();
        for (std::size_t i = 0; i < k; ++i) {
            const std::uint32_t a = f[i], b = f[(i + 1) % k];
            const std::uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
            set.insert((static_cast<std::uint64_t>(lo) << 32) | hi);
        }
    }
    return set.size();
}

static PolyMesh unitCube() {
    PolyMesh m;
    m.verts = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
               {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}};
    // Six quads, outward CCW (winding is irrelevant to the counts we check).
    m.faces = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4}, {2, 3, 7, 6}, {1, 2, 6, 5}, {0, 3, 7, 4}};
    return m;
}

int main() {
    // --- 1. Cube topology + quad-only + Euler. ---
    {
        const PolyMesh cube = unitCube();
        const PolyMesh s = maz::render::catmullClark(cube, 1);
        CHECK(s.verts.size() == 26, "cube subdivides to 26 vertices (8 + 6 faces + 12 edges)");
        CHECK(s.faces.size() == 24, "cube subdivides to 24 quads (4 per original face)");
        bool allQuads = true;
        for (const auto& f : s.faces)
            if (f.size() != 4) allQuads = false;
        CHECK(allQuads, "every output face is a quad");
        const std::size_t E = countEdges(s);
        CHECK(E == 48, "cube subdivides to 48 edges");
        // Euler characteristic of a closed sphere.
        const long chi = static_cast<long>(s.verts.size()) - static_cast<long>(E) + static_cast<long>(s.faces.size());
        CHECK(chi == 2, "Euler characteristic V-E+F == 2 (still a closed genus-0 surface)");
    }

    // --- 2. Bounding + corner rounding. ---
    {
        const PolyMesh cube = unitCube();
        const PolyMesh s = maz::render::catmullClark(cube, 1);
        bool inBox = true;
        for (const vec3& v : s.verts)
            if (v.x < -1.0001f || v.x > 1.0001f || v.y < -1.0001f || v.y > 1.0001f ||
                v.z < -1.0001f || v.z > 1.0001f)
                inBox = false;
        CHECK(inBox, "all subdivided points stay inside the control cube");
        // Original 8 corners (indices 0..7) must move strictly toward the centre.
        bool inward = true;
        for (std::size_t i = 0; i < 8; ++i) {
            const float before = std::sqrt(3.0f); // |(+-1,+-1,+-1)|
            const vec3& a = s.verts[i];
            const float after = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
            if (after >= before - 1e-3f) inward = false;
        }
        CHECK(inward, "each original corner rounds inward toward the centre");
    }

    // --- 3. Planarity: a flat cage stays flat. ---
    {
        PolyMesh grid;
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                grid.verts.push_back(vec3(static_cast<float>(x), static_cast<float>(y), 0.0f));
        auto idx = [](int x, int y) { return static_cast<std::uint32_t>(y * 3 + x); };
        for (int cy = 0; cy < 2; ++cy)
            for (int cx = 0; cx < 2; ++cx)
                grid.faces.push_back({idx(cx, cy), idx(cx + 1, cy), idx(cx + 1, cy + 1), idx(cx, cy + 1)});
        const PolyMesh s = maz::render::catmullClark(grid, 2); // two passes
        bool flat = true;
        for (const vec3& v : s.verts)
            if (std::fabs(v.z) > 1e-6f) flat = false;
        CHECK(flat, "subdividing a flat z=0 grid keeps every vertex in the z=0 plane");
    }

    // --- 4. Determinism. ---
    {
        const PolyMesh cube = unitCube();
        const PolyMesh a = maz::render::catmullClark(cube, 2);
        const PolyMesh b = maz::render::catmullClark(cube, 2);
        bool same = a.verts.size() == b.verts.size() && a.faces.size() == b.faces.size();
        for (std::size_t i = 0; same && i < a.verts.size(); ++i)
            if (a.verts[i].x != b.verts[i].x || a.verts[i].y != b.verts[i].y || a.verts[i].z != b.verts[i].z)
                same = false;
        CHECK(same, "identical inputs produce identical subdivisions");
    }

    if (g_fail == 0) {
        std::printf("catmullclark: OK — cube topology, quads, Euler, bounding, corner rounding, planarity.\n");
        return 0;
    }
    std::printf("catmullclark: %d failure(s).\n", g_fail);
    return 1;
}
