#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

// maz::render isosurface mesher — turns a signed distance field (see game::Csg) into a real triangle
// mesh, the step that gives Godot's CSG nodes their actual polygons. This uses Naive Surface Nets: the
// field is sampled on a grid; every cell the surface passes through gets ONE vertex placed at the mean
// of the surface crossings on that cell's edges; and each grid edge that flips sign emits a quad tying
// the four cells around it together. The result is a watertight, evenly-tessellated mesh that follows
// the field — union/intersection/subtraction of primitives come out as one connected surface. Pure CPU
// + deterministic (no GPU), so it unit-tests headlessly by checking that every output vertex lies on
// the isosurface; the renderer just uploads the buffers.
namespace maz::render {

struct SdfMesh {
    std::vector<math::vec3> positions;
    std::vector<math::vec3> normals;
    std::vector<uint32_t> indices; // triangle list
    bool empty() const { return positions.empty() || indices.empty(); }
};

// Distance function: <0 inside, >0 outside (matches game::Sdf).
using DistanceFn = std::function<float(const math::vec3&)>;

// Mesh the isosurface (field == 0) inside [mn, mx] using res cells per axis. Returns an empty mesh if
// res < 1 or the field never crosses zero in the box. `estimateNormals` fills per-vertex normals from
// the field gradient (points outward).
inline SdfMesh surfaceNets(const DistanceFn& field, const math::vec3& mn, const math::vec3& mx,
                           int res, bool estimateNormals = true) {
    SdfMesh out;
    if (res < 1 || !field) {
        return out;
    }
    const int nx = res, ny = res, nz = res;
    const math::vec3 span = mx - mn;
    const math::vec3 cell(span.x / static_cast<float>(nx), span.y / static_cast<float>(ny),
                          span.z / static_cast<float>(nz));

    auto cornerPos = [&](int x, int y, int z) {
        return math::vec3(mn.x + static_cast<float>(x) * cell.x, mn.y + static_cast<float>(y) * cell.y,
                          mn.z + static_cast<float>(z) * cell.z);
    };
    const int gx = nx + 1, gy = ny + 1;
    auto gi = [&](int x, int y, int z) {
        return (static_cast<std::size_t>(z) * static_cast<std::size_t>(gy) +
                static_cast<std::size_t>(y)) *
                   static_cast<std::size_t>(gx) +
               static_cast<std::size_t>(x);
    };
    // Sample the field on the (nx+1)^3 corner grid.
    std::vector<float> g(static_cast<std::size_t>(gx) * static_cast<std::size_t>(gy) *
                         static_cast<std::size_t>(nz + 1));
    for (int z = 0; z <= nz; ++z) {
        for (int y = 0; y <= ny; ++y) {
            for (int x = 0; x <= nx; ++x) {
                g[gi(x, y, z)] = field(cornerPos(x, y, z));
            }
        }
    }

    // The 12 edges of a cube as (cornerA, cornerB); corner c = (c&1, (c>>1)&1, (c>>2)&1).
    static const std::array<std::array<int, 2>, 12> kEdges = {{{{0, 1}},
                                                               {{2, 3}},
                                                               {{4, 5}},
                                                               {{6, 7}},
                                                               {{0, 2}},
                                                               {{1, 3}},
                                                               {{4, 6}},
                                                               {{5, 7}},
                                                               {{0, 4}},
                                                               {{1, 5}},
                                                               {{2, 6}},
                                                               {{3, 7}}}};
    auto cornerOffset = [](int c) {
        return math::vec3(static_cast<float>(c & 1), static_cast<float>((c >> 1) & 1),
                          static_cast<float>((c >> 2) & 1));
    };

    // One vertex per surface-crossing cell; -1 elsewhere.
    std::vector<int> cellVert(static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny) *
                                  static_cast<std::size_t>(nz),
                              -1);
    auto ci = [&](int x, int y, int z) {
        return (static_cast<std::size_t>(z) * static_cast<std::size_t>(ny) +
                static_cast<std::size_t>(y)) *
                   static_cast<std::size_t>(nx) +
               static_cast<std::size_t>(x);
    };

    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                float cv[8];
                int inside = 0;
                for (int c = 0; c < 8; ++c) {
                    const math::vec3 o = cornerOffset(c);
                    cv[c] = g[gi(x + static_cast<int>(o.x), y + static_cast<int>(o.y),
                                 z + static_cast<int>(o.z))];
                    if (cv[c] < 0.0f) {
                        ++inside;
                    }
                }
                if (inside == 0 || inside == 8) {
                    continue; // no surface through this cell
                }
                math::vec3 sum(0.0f);
                int crossings = 0;
                for (const auto& e : kEdges) {
                    const float a = cv[e[0]];
                    const float b = cv[e[1]];
                    if ((a < 0.0f) == (b < 0.0f)) {
                        continue;
                    }
                    const float t = a / (a - b); // zero crossing along the edge
                    const math::vec3 pa = cornerPos(x, y, z) + cornerOffset(e[0]) * cell;
                    const math::vec3 pb = cornerPos(x, y, z) + cornerOffset(e[1]) * cell;
                    sum += pa + (pb - pa) * t;
                    ++crossings;
                }
                if (crossings == 0) {
                    continue;
                }
                cellVert[ci(x, y, z)] = static_cast<int>(out.positions.size());
                out.positions.push_back(sum / static_cast<float>(crossings));
            }
        }
    }

    auto quad = [&](int a, int b, int c, int d, bool flip) {
        const uint32_t ua = static_cast<uint32_t>(a);
        const uint32_t ub = static_cast<uint32_t>(b);
        const uint32_t uc = static_cast<uint32_t>(c);
        const uint32_t ud = static_cast<uint32_t>(d);
        if (!flip) {
            out.indices.insert(out.indices.end(), {ua, ub, uc, ua, uc, ud});
        } else {
            out.indices.insert(out.indices.end(), {ua, uc, ub, ua, ud, uc});
        }
    };

    // For each interior grid edge that flips sign, connect the four cells around it into a quad.
    for (int z = 0; z <= nz; ++z) {
        for (int y = 0; y <= ny; ++y) {
            for (int x = 0; x <= nx; ++x) {
                const float s = g[gi(x, y, z)];
                // +X edge, shared by cells at (x, y-1..y, z-1..z).
                if (x < nx && y >= 1 && y < ny && z >= 1 && z < nz) {
                    const float s2 = g[gi(x + 1, y, z)];
                    if ((s < 0.0f) != (s2 < 0.0f)) {
                        const int v0 = cellVert[ci(x, y - 1, z - 1)];
                        const int v1 = cellVert[ci(x, y, z - 1)];
                        const int v2 = cellVert[ci(x, y, z)];
                        const int v3 = cellVert[ci(x, y - 1, z)];
                        if (v0 >= 0 && v1 >= 0 && v2 >= 0 && v3 >= 0) {
                            quad(v0, v1, v2, v3, s < 0.0f);
                        }
                    }
                }
                // +Y edge, shared by cells at (x-1..x, y, z-1..z).
                if (y < ny && x >= 1 && x < nx && z >= 1 && z < nz) {
                    const float s2 = g[gi(x, y + 1, z)];
                    if ((s < 0.0f) != (s2 < 0.0f)) {
                        const int v0 = cellVert[ci(x - 1, y, z - 1)];
                        const int v1 = cellVert[ci(x, y, z - 1)];
                        const int v2 = cellVert[ci(x, y, z)];
                        const int v3 = cellVert[ci(x - 1, y, z)];
                        if (v0 >= 0 && v1 >= 0 && v2 >= 0 && v3 >= 0) {
                            quad(v0, v1, v2, v3, s >= 0.0f);
                        }
                    }
                }
                // +Z edge, shared by cells at (x-1..x, y-1..y, z).
                if (z < nz && x >= 1 && x < nx && y >= 1 && y < ny) {
                    const float s2 = g[gi(x, y, z + 1)];
                    if ((s < 0.0f) != (s2 < 0.0f)) {
                        const int v0 = cellVert[ci(x - 1, y - 1, z)];
                        const int v1 = cellVert[ci(x, y - 1, z)];
                        const int v2 = cellVert[ci(x, y, z)];
                        const int v3 = cellVert[ci(x - 1, y, z)];
                        if (v0 >= 0 && v1 >= 0 && v2 >= 0 && v3 >= 0) {
                            quad(v0, v1, v2, v3, s < 0.0f);
                        }
                    }
                }
            }
        }
    }

    if (estimateNormals) {
        out.normals.resize(out.positions.size());
        const float e = std::min(std::min(cell.x, cell.y), cell.z) * 0.5f;
        for (std::size_t i = 0; i < out.positions.size(); ++i) {
            const math::vec3& p = out.positions[i];
            math::vec3 grad(field(p + math::vec3(e, 0, 0)) - field(p - math::vec3(e, 0, 0)),
                            field(p + math::vec3(0, e, 0)) - field(p - math::vec3(0, e, 0)),
                            field(p + math::vec3(0, 0, e)) - field(p - math::vec3(0, 0, e)));
            const float len = std::sqrt(glm::dot(grad, grad));
            out.normals[i] = len > 1e-12f ? grad / len : math::vec3(0, 1, 0);
        }
    }
    return out;
}

} // namespace maz::render
