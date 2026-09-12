// tests/math/marchingtetrahedra.cpp — verifies 3-D isosurface extraction (math MarchingTetrahedra.hpp).
// Ground truths, deterministic (fixed analytic fields, no <random>, no clock):
//   * WATERTIGHT 2-MANIFOLD: on a sphere field, every triangle edge is shared by EXACTLY two triangles
//     (no cracks, no boundary) — the property that makes marching-tetrahedra superior to naive marching
//     cubes;
//   * EULER CHARACTERISTIC: V - E + F == 2 for the closed genus-0 surface;
//   * ON THE SURFACE: every generated vertex lies on the sphere (|v| ~ R to within the grid resolution);
//   * VOLUME: the signed volume of the closed mesh (divergence theorem) matches (4/3)piR^3, which also
//     confirms the triangle winding is consistently outward;
//   * a field that never crosses the iso level yields an empty mesh; determinism.
#include "maz/math/MarchingTetrahedra.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <utility>

using maz::math::vec3;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    const float R = 1.0f;
    auto sphere = [&](vec3 p) { return std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z) - R; };
    const auto mesh = maz::math::marchingTetrahedra(sphere, vec3(-1.5f, -1.5f, -1.5f),
                                                    vec3(1.5f, 1.5f, 1.5f), 24);

    const std::size_t V = mesh.positions.size();
    const std::size_t F = mesh.indices.size() / 3;
    CHECK(V > 100 && F > 100, "the sphere isosurface produced a non-trivial mesh");

    // --- Watertight 2-manifold + Euler characteristic. ---
    {
        std::map<std::pair<std::uint32_t, std::uint32_t>, int> edgeCount;
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
            const std::uint32_t t[3] = {mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]};
            for (int k = 0; k < 3; ++k) {
                std::uint32_t a = t[k], b = t[(k + 1) % 3];
                if (a > b) std::swap(a, b);
                ++edgeCount[{a, b}];
            }
        }
        int nonManifold = 0;
        for (const auto& e : edgeCount) {
            if (e.second != 2) ++nonManifold;
        }
        CHECK(nonManifold == 0, "every edge is shared by exactly two triangles (watertight, crack-free)");
        const long euler = static_cast<long>(V) - static_cast<long>(edgeCount.size()) +
                           static_cast<long>(F);
        CHECK(euler == 2, "Euler characteristic V - E + F == 2 (a closed genus-0 sphere)");
    }

    // --- Every vertex lies on the sphere. ---
    {
        float maxErr = 0.0f;
        for (const vec3& p : mesh.positions) {
            const float r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
            maxErr = std::max(maxErr, std::fabs(r - R));
        }
        CHECK(maxErr < 0.03f, "every vertex lies on the sphere of radius R to within the grid resolution");
    }

    // --- Signed volume matches the analytic sphere (and confirms outward winding). ---
    {
        double vol = 0.0;
        for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
            const vec3 a = mesh.positions[mesh.indices[i]];
            const vec3 b = mesh.positions[mesh.indices[i + 1]];
            const vec3 c = mesh.positions[mesh.indices[i + 2]];
            vol += static_cast<double>(glm::dot(a, glm::cross(b, c))) / 6.0;
        }
        const double trueVol = 4.0 / 3.0 * 3.14159265358979324 * R * R * R;
        CHECK(vol > 0.0, "the mesh has positive signed volume (triangles wound consistently outward)");
        CHECK(std::fabs(vol - trueVol) < trueVol * 0.03, "the closed-mesh volume matches (4/3)piR^3");
    }

    // --- Off-centre field (a box via max-of-planes) is still watertight. ---
    {
        auto boxFld = [](vec3 p) {
            const float dx = std::fabs(p.x) - 0.6f, dy = std::fabs(p.y) - 0.6f, dz = std::fabs(p.z) - 0.6f;
            return std::max(dx, std::max(dy, dz)); // negative inside the cube
        };
        const auto bm = maz::math::marchingTetrahedra(boxFld, vec3(-1, -1, -1), vec3(1, 1, 1), 16);
        std::map<std::pair<std::uint32_t, std::uint32_t>, int> ec;
        for (std::size_t i = 0; i < bm.indices.size(); i += 3) {
            const std::uint32_t t[3] = {bm.indices[i], bm.indices[i + 1], bm.indices[i + 2]};
            for (int k = 0; k < 3; ++k) {
                std::uint32_t a = t[k], b = t[(k + 1) % 3];
                if (a > b) std::swap(a, b);
                ++ec[{a, b}];
            }
        }
        int nm = 0;
        for (const auto& e : ec) if (e.second != 2) ++nm;
        CHECK(!bm.indices.empty() && nm == 0, "a box isosurface is also watertight");
    }

    // --- Empty: a field that never crosses the iso level. ---
    {
        auto neg = [](vec3) { return -1.0f; };
        CHECK(maz::math::marchingTetrahedra(neg, vec3(-1, -1, -1), vec3(1, 1, 1), 8).indices.empty(),
              "a field with no crossing yields an empty mesh");
    }

    // --- Determinism. ---
    {
        const auto a = maz::math::marchingTetrahedra(sphere, vec3(-1.5f, -1.5f, -1.5f),
                                                     vec3(1.5f, 1.5f, 1.5f), 10);
        const auto b = maz::math::marchingTetrahedra(sphere, vec3(-1.5f, -1.5f, -1.5f),
                                                     vec3(1.5f, 1.5f, 1.5f), 10);
        CHECK(a.indices == b.indices && a.positions.size() == b.positions.size(),
              "identical inputs produce identical meshes");
    }

    if (g_fail == 0) {
        std::printf("marchingtetrahedra: OK — watertight 2-manifold, Euler=2, on-surface, volume, box, "
                    "empty, determinism.\n");
        return 0;
    }
    std::printf("marchingtetrahedra: %d failure(s).\n", g_fail);
    return 1;
}
