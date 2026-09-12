#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

// maz::math Marching Tetrahedra — extract a triangle mesh of an isosurface (field == iso) from a scalar
// field sampled on a 3-D grid. The engine has 2-D MarchingSquares and the dual-vertex SurfaceNets mesher;
// this is the classic PRIMAL marching-simplex method and fills the "give me the level set of a 3-D field as
// triangles" gap for voxel terrain, metaballs/blobby surfaces, CSG/SDF meshing and scientific/medical
// volumes. It splits every grid cube into SIX tetrahedra and marches each one: because adjacent tetrahedra
// share a triangular face and the crossing on that face is fixed by the three shared corner values, the
// output is WATERTIGHT and crack-free by construction — no ambiguous 256-case cube table, and none of
// marching cubes' hole artefacts. Surface vertices are interpolated along grid edges and de-duplicated by
// edge, so the mesh is a proper 2-manifold. Godot has no isosurface extractor. The field is passed as a
// callable `float(vec3)`. Header-only, std-only, deterministic.
namespace maz::math {

struct IsoMesh {
    std::vector<vec3> positions;
    std::vector<std::uint32_t> indices; // triangle list, CCW when viewed from outside (higher field)
};

// Mesh the surface field(p) == iso inside the box [mn, mx] using `res` cubes per axis (each split into 6
// tetrahedra). `inside` is the region where field < iso; triangles are wound so their normal points toward
// the field-increasing (outside) side. Returns an empty mesh if res < 1 or the field never crosses iso.
template <class Field>
inline IsoMesh marchingTetrahedra(Field&& field, const vec3& mn, const vec3& mx, int res,
                                  float iso = 0.0f) {
    IsoMesh out;
    if (res < 1) {
        return out;
    }
    const int n = res;
    const int g = n + 1;
    const vec3 span = mx - mn;
    const vec3 cell(span.x / static_cast<float>(n), span.y / static_cast<float>(n),
                    span.z / static_cast<float>(n));
    auto cornerPos = [&](int x, int y, int z) {
        return vec3(mn.x + static_cast<float>(x) * cell.x, mn.y + static_cast<float>(y) * cell.y,
                    mn.z + static_cast<float>(z) * cell.z);
    };
    auto gi = [&](int x, int y, int z) {
        return (static_cast<std::size_t>(z) * static_cast<std::size_t>(g) +
                static_cast<std::size_t>(y)) *
                   static_cast<std::size_t>(g) +
               static_cast<std::size_t>(x);
    };

    // Sample the field on the (res+1)^3 corner grid, offset by iso so the surface is at value 0.
    std::vector<float> val(static_cast<std::size_t>(g) * static_cast<std::size_t>(g) *
                           static_cast<std::size_t>(g));
    for (int z = 0; z < g; ++z) {
        for (int y = 0; y < g; ++y) {
            for (int x = 0; x < g; ++x) {
                val[gi(x, y, z)] = static_cast<float>(field(cornerPos(x, y, z))) - iso;
            }
        }
    }

    std::unordered_map<std::uint64_t, std::uint32_t> vcache;
    auto edgeVert = [&](int ax, int ay, int az, int bx, int by, int bz) -> std::uint32_t {
        const std::size_t ia = gi(ax, ay, az);
        const std::size_t ib = gi(bx, by, bz);
        const std::uint64_t lo = static_cast<std::uint64_t>(ia < ib ? ia : ib);
        const std::uint64_t hi = static_cast<std::uint64_t>(ia < ib ? ib : ia);
        const std::uint64_t key = (lo << 32) | hi;
        const auto it = vcache.find(key);
        if (it != vcache.end()) {
            return it->second;
        }
        const float va = val[ia];
        const float vb = val[ib];
        const float denom = va - vb;
        const float t = (denom > 1e-20f || denom < -1e-20f) ? va / denom : 0.5f; // where value crosses 0
        const vec3 pa = cornerPos(ax, ay, az);
        const vec3 pb = cornerPos(bx, by, bz);
        const vec3 p = pa + (pb - pa) * t;
        const std::uint32_t idx = static_cast<std::uint32_t>(out.positions.size());
        out.positions.push_back(p);
        vcache.emplace(key, idx);
        return idx;
    };

    // Cube corner offsets and a 6-tetrahedron split sharing the main diagonal 0->6.
    static const int co[8][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                 {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
    static const int tets[6][4] = {{0, 1, 2, 6}, {0, 2, 3, 6}, {0, 3, 7, 6},
                                   {0, 7, 4, 6}, {0, 4, 5, 6}, {0, 5, 1, 6}};

    auto emitTri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c, const vec3& outDir) {
        const vec3& pa = out.positions[a];
        const vec3& pb = out.positions[b];
        const vec3& pc = out.positions[c];
        const vec3 nrm = glm::cross(pb - pa, pc - pa);
        if (glm::dot(nrm, outDir) < 0.0f) {
            std::swap(b, c); // orient so the normal points to the field-increasing (outside) side
        }
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    };

    for (int z = 0; z < n; ++z) {
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                for (int t = 0; t < 6; ++t) {
                    int cx[4], cy[4], cz[4];
                    float tv[4];
                    for (int k = 0; k < 4; ++k) {
                        const int* o = co[tets[t][k]];
                        cx[k] = x + o[0];
                        cy[k] = y + o[1];
                        cz[k] = z + o[2];
                        tv[k] = val[gi(cx[k], cy[k], cz[k])];
                    }
                    int in[4], outv[4], ni = 0, no = 0;
                    for (int k = 0; k < 4; ++k) {
                        if (tv[k] < 0.0f) {
                            in[ni++] = k;
                        } else {
                            outv[no++] = k;
                        }
                    }
                    if (ni == 0 || ni == 4) {
                        continue;
                    }
                    // Direction from inside toward outside — used to orient triangle normals.
                    const vec3 outDir = cornerPos(cx[outv[0]], cy[outv[0]], cz[outv[0]]) -
                                        cornerPos(cx[in[0]], cy[in[0]], cz[in[0]]);
                    auto ev = [&](int a, int b) {
                        return edgeVert(cx[a], cy[a], cz[a], cx[b], cy[b], cz[b]);
                    };
                    if (ni == 1) {
                        const std::uint32_t e0 = ev(in[0], outv[0]);
                        const std::uint32_t e1 = ev(in[0], outv[1]);
                        const std::uint32_t e2 = ev(in[0], outv[2]);
                        emitTri(e0, e1, e2, outDir);
                    } else if (ni == 3) {
                        const std::uint32_t e0 = ev(outv[0], in[0]);
                        const std::uint32_t e1 = ev(outv[0], in[1]);
                        const std::uint32_t e2 = ev(outv[0], in[2]);
                        emitTri(e0, e1, e2, outDir);
                    } else { // ni == 2: the crossing is a quad across four edges.
                        const std::uint32_t a = ev(in[0], outv[0]);
                        const std::uint32_t b = ev(in[1], outv[0]);
                        const std::uint32_t c = ev(in[1], outv[1]);
                        const std::uint32_t d = ev(in[0], outv[1]);
                        emitTri(a, b, c, outDir);
                        emitTri(a, c, d, outDir);
                    }
                }
            }
        }
    }
    return out;
}

} // namespace maz::math
