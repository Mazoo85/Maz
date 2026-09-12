// tests/game/voxelraycast.cpp — verifies 3D voxel ray traversal (game VoxelRaycast.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * an axis-aligned ray visits the expected consecutive cells;
//   * the traversal starts at floor(origin), is 6-connected (each step one axis by +/-1), and each axis
//     moves monotonically in the ray's direction;
//   * SOUNDNESS: every traversed cell is genuinely crossed by the ray with positive length (an independent
//     ray-vs-unit-box slab test, in double precision — robust near near-diagonal crossings where naive dense
//     sampling would skip a cell);
//   * COMPLETENESS: every voxel a densely-sampled point falls in appears somewhere in the traversal;
//   * voxelRaycast stops at the first solid cell; degenerate inputs return nothing.
#include "maz/game/VoxelRaycast.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::game::traverseVoxels;
using maz::game::voxelFloor;
using maz::game::voxelRaycast;
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
    float sym() { return (static_cast<float>(next()) / 4294967296.0f) * 2.0f - 1.0f; }
};

static bool eq(const Vector3i& a, const Vector3i& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// Parametric length of the ray (origin o, direction d) crossing the unit box at cell C. Negative if missed.
static double boxCrossLength(const vec3& o, const vec3& d, const Vector3i& C) {
    const double oc[3] = {o.x, o.y, o.z};
    const double dc[3] = {d.x, d.y, d.z};
    const double lo[3] = {static_cast<double>(C.x), static_cast<double>(C.y), static_cast<double>(C.z)};
    double tEnter = -1e300, tExit = 1e300;
    for (int a = 0; a < 3; ++a) {
        const double bmin = lo[a], bmax = lo[a] + 1.0;
        if (std::fabs(dc[a]) < 1e-300) {
            if (oc[a] < bmin || oc[a] > bmax) return -1.0; // parallel and outside the slab
        } else {
            double t1 = (bmin - oc[a]) / dc[a];
            double t2 = (bmax - oc[a]) / dc[a];
            if (t1 > t2) { const double tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tEnter) tEnter = t1;
            if (t2 < tExit) tExit = t2;
        }
    }
    if (tExit < tEnter) return -1.0;
    const double a0 = tEnter > 0.0 ? tEnter : 0.0; // clamp to the forward ray
    return tExit - a0;
}

int main() {
    // --- 1. Axis-aligned ray. ---
    {
        const auto cells = traverseVoxels(vec3{0.5f, 0.5f, 0.5f}, vec3{1.0f, 0.0f, 0.0f}, 3.4f);
        CHECK(cells.size() == 4, "unit +X ray over 3.4 units visits 4 cells");
        CHECK(eq(cells[0], {0, 0, 0}) && eq(cells[1], {1, 0, 0}) && eq(cells[2], {2, 0, 0}) &&
                  eq(cells[3], {3, 0, 0}), "consecutive cells along +X");
    }

    // --- 2. Start cell + 6-connectivity + monotonicity on a diagonal ray. ---
    {
        const vec3 o{-1.3f, 2.7f, 0.2f};
        const vec3 d{0.7f, -0.4f, 0.55f};
        const auto cells = traverseVoxels(o, d, 12.0f);
        CHECK(!cells.empty() && eq(cells[0], {voxelFloor(o.x), voxelFloor(o.y), voxelFloor(o.z)}),
              "traversal starts at floor(origin)");
        const int sx = (d.x > 0) - (d.x < 0), sy = (d.y > 0) - (d.y < 0), sz = (d.z > 0) - (d.z < 0);
        bool ok = true;
        for (std::size_t i = 1; i < cells.size(); ++i) {
            const int dx = cells[i].x - cells[i - 1].x;
            const int dy = cells[i].y - cells[i - 1].y;
            const int dz = cells[i].z - cells[i - 1].z;
            if (std::abs(dx) + std::abs(dy) + std::abs(dz) != 1) { ok = false; break; } // 6-connected
            if (dx != 0 && dx != sx) { ok = false; break; }                              // monotone
            if (dy != 0 && dy != sy) { ok = false; break; }
            if (dz != 0 && dz != sz) { ok = false; break; }
        }
        CHECK(ok, "steps are 6-connected and monotone in the ray direction");
    }

    // --- 3. Soundness + completeness over random rays. ---
    {
        Lcg rng{0x501DAAu};
        bool sound = true, complete = true;
        long totalCells = 0;
        for (int trial = 0; trial < 500 && (sound && complete); ++trial) {
            const vec3 o{rng.sym() * 3.0f, rng.sym() * 3.0f, rng.sym() * 3.0f};
            const vec3 d{rng.sym(), rng.sym(), rng.sym()};
            const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (len < 0.2f) continue;
            const vec3 nd{d.x / len, d.y / len, d.z / len};

            const float maxDist = 20.0f;
            const auto T = traverseVoxels(o, d, maxDist, 8192);
            totalCells += static_cast<long>(T.size());

            // Soundness: each traversed cell is really crossed (positive length) by the ray.
            for (const Vector3i& c : T) {
                if (boxCrossLength(o, d, c) <= 1e-7) { sound = false; break; }
            }
            // Completeness: each densely-sampled voxel (< maxDist) is somewhere in the traversal.
            for (float t = 0.0f; t <= 19.0f && complete; t += 0.01f) {
                const Vector3i v{voxelFloor(o.x + nd.x * t), voxelFloor(o.y + nd.y * t),
                                 voxelFloor(o.z + nd.z * t)};
                bool found = false;
                for (const Vector3i& c : T) {
                    if (eq(c, v)) { found = true; break; }
                }
                if (!found) complete = false;
            }
        }
        CHECK(sound, "every traversed cell is genuinely crossed by the ray (no spurious cells)");
        CHECK(complete, "no voxel the ray passes through is missing from the traversal");
        CHECK(totalCells > 5000, "the random rays actually crossed many voxels");
    }

    // --- 4. voxelRaycast hits the first solid cell. ---
    {
        const auto hit = voxelRaycast(vec3{0.5f, 0.5f, 0.5f}, vec3{1.0f, 0.0f, 0.0f}, 100.0f,
                                      [](const Vector3i& c) { return c.x >= 5; });
        CHECK(hit.has_value() && eq(*hit, {5, 0, 0}), "raycast stops at the first solid voxel");
        const auto miss = voxelRaycast(vec3{0.5f, 0.5f, 0.5f}, vec3{1.0f, 0.0f, 0.0f}, 3.0f,
                                       [](const Vector3i& c) { return c.x >= 5; });
        CHECK(!miss.has_value(), "raycast returns nullopt when no solid within range");
    }

    // --- 5. Degenerate inputs. ---
    {
        CHECK(traverseVoxels(vec3{0, 0, 0}, vec3{0, 0, 0}, 10.0f).empty(), "zero direction -> empty");
        CHECK(traverseVoxels(vec3{0, 0, 0}, vec3{1, 0, 0}, 0.0f).empty(), "zero distance -> empty");
    }

    if (g_fail == 0) {
        std::printf("voxelraycast: OK — axis ray, 6-connectivity, slab soundness, completeness, raycast.\n");
        return 0;
    }
    std::printf("voxelraycast: %d failure(s).\n", g_fail);
    return 1;
}
