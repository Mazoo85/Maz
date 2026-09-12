// tests/render/greedyvoxelmesh.cpp — verifies greedy voxel meshing (render GreedyVoxelMesh.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * a single voxel emits 6 unit quads; a solid WxHxD box emits exactly 6 (fully merged) quads;
//   * each quad winds outward (its geometric normal agrees with the stated normal);
//   * RIGOROUS: over hundreds of random multi-type grids, decomposing every quad back into unit faces yields
//     EXACTLY the set of exposed faces a brute-force per-cell scan finds — each exposed face covered once
//     (no gaps), no face covered twice (no overlap), and every quad's type matches its owning cell;
//   * greedy never emits more quads than the naive per-face count.
#include "maz/render/GreedyVoxelMesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

using maz::render::greedyVoxelMesh;
using maz::render::VoxelQuad;
using maz::math::vec3;
using maz::math::Vector3i;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

static float axisF(const vec3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }
static int axisI(const Vector3i& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }
static int ri(float f) { return static_cast<int>(std::lround(f)); }

// Map a unit-axis normal to a direction index 0..5 (+x,-x,+y,-y,+z,-z).
static int dirIndex(const Vector3i& n) {
    if (n.x > 0) return 0;
    if (n.x < 0) return 1;
    if (n.y > 0) return 2;
    if (n.y < 0) return 3;
    if (n.z > 0) return 4;
    return 5;
}

int main() {
    // --- 1. Single voxel -> 6 unit quads. ---
    {
        const auto q = greedyVoxelMesh(1, 1, 1, [](int, int, int) { return 7; });
        CHECK(q.size() == 6, "single voxel yields 6 faces");
        bool typed = true;
        for (const auto& f : q) if (f.type != 7) typed = false;
        CHECK(typed, "all faces carry the voxel's type");
    }

    // --- 2. Solid box -> exactly 6 merged quads. ---
    {
        const auto q = greedyVoxelMesh(4, 3, 5, [](int, int, int) { return 1; });
        CHECK(q.size() == 6, "solid box merges to 6 quads (one per side)");
    }

    // --- 3. Outward winding. ---
    {
        const auto q = greedyVoxelMesh(2, 2, 2, [](int, int, int) { return 1; });
        bool ok = true;
        for (const auto& f : q) {
            const vec3 e1{f.corner[1].x - f.corner[0].x, f.corner[1].y - f.corner[0].y, f.corner[1].z - f.corner[0].z};
            const vec3 e2{f.corner[2].x - f.corner[0].x, f.corner[2].y - f.corner[0].y, f.corner[2].z - f.corner[0].z};
            const vec3 cr{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
            const float dot = cr.x * static_cast<float>(f.normal.x) + cr.y * static_cast<float>(f.normal.y) +
                              cr.z * static_cast<float>(f.normal.z);
            if (dot <= 0.0f) { ok = false; break; }
        }
        CHECK(ok, "every quad winds counter-clockwise as seen from outside");
    }

    // --- 4. Rigorous exposed-face partition over random grids. ---
    {
        Lcg rng{0x67EEDu};
        bool ok = true;
        long totalQuads = 0, totalFaces = 0;
        for (int trial = 0; trial < 400 && ok; ++trial) {
            const int nx = 2 + static_cast<int>(rng.next() % 5u);
            const int ny = 2 + static_cast<int>(rng.next() % 5u);
            const int nz = 2 + static_cast<int>(rng.next() % 5u);
            std::vector<int> g(static_cast<std::size_t>(nx * ny * nz));
            auto at = [&](int x, int y, int z) -> int {
                if (x < 0 || y < 0 || z < 0 || x >= nx || y >= ny || z >= nz) return 0;
                return g[static_cast<std::size_t>((z * ny + y) * nx + x)];
            };
            for (auto& c : g) c = (rng.next() % 100u < 55u) ? 0 : static_cast<int>(1u + rng.next() % 3u);

            // Brute-force exposed faces: (cell,dir) -> type.
            const int dv[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
            std::map<std::array<int, 4>, int> brute;
            for (int z = 0; z < nz; ++z)
                for (int y = 0; y < ny; ++y)
                    for (int x = 0; x < nx; ++x) {
                        const int t = at(x, y, z);
                        if (t == 0) continue;
                        for (int dcnt = 0; dcnt < 6; ++dcnt) {
                            if (at(x + dv[dcnt][0], y + dv[dcnt][1], z + dv[dcnt][2]) == 0) {
                                brute[{x, y, z, dcnt}] = t;
                            }
                        }
                    }

            const auto quads = greedyVoxelMesh(nx, ny, nz, at);
            totalQuads += static_cast<long>(quads.size());
            totalFaces += static_cast<long>(brute.size());

            // Decompose each quad into unit faces; each must be a distinct brute face of matching type.
            std::map<std::array<int, 4>, int> covered;
            for (const auto& q : quads) {
                const int a = (q.normal.x != 0) ? 0 : (q.normal.y != 0 ? 1 : 2);
                const int sign = axisI(q.normal, a);
                const int u = (a + 1) % 3, w = (a + 2) % 3;
                const int P = ri(axisF(q.corner[0], a));
                int umin = ri(axisF(q.corner[0], u)), umax = umin;
                int vmin = ri(axisF(q.corner[0], w)), vmax = vmin;
                for (int k = 1; k < 4; ++k) {
                    umin = std::min(umin, ri(axisF(q.corner[k], u)));
                    umax = std::max(umax, ri(axisF(q.corner[k], u)));
                    vmin = std::min(vmin, ri(axisF(q.corner[k], w)));
                    vmax = std::max(vmax, ri(axisF(q.corner[k], w)));
                }
                const int dcell = (sign > 0) ? (P - 1) : P;
                const int di = dirIndex(q.normal);
                for (int iu = umin; iu < umax && ok; ++iu) {
                    for (int iv = vmin; iv < vmax && ok; ++iv) {
                        int cell[3];
                        cell[a] = dcell; cell[u] = iu; cell[w] = iv;
                        const std::array<int, 4> key{cell[0], cell[1], cell[2], di};
                        if (covered.count(key)) { ok = false; break; }       // overlap
                        const auto it = brute.find(key);
                        if (it == brute.end() || it->second != q.type) { ok = false; break; } // spurious/type
                        covered[key] = q.type;
                    }
                }
            }
            if (ok && covered.size() != brute.size()) ok = false; // missing faces
        }
        CHECK(ok, "quads exactly partition the exposed faces (no gaps, no overlaps, right types)");
        CHECK(totalQuads <= totalFaces, "greedy never emits more quads than naive per-face count");
        CHECK(totalFaces > 2000, "the random grids actually produced many faces");
    }

    if (g_fail == 0) {
        std::printf("greedyvoxelmesh: OK — single/box counts, winding, exposed-face partition, quad savings.\n");
        return 0;
    }
    std::printf("greedyvoxelmesh: %d failure(s).\n", g_fail);
    return 1;
}
