#pragma once

#include "maz/math/Math.hpp"       // vec3
#include "maz/math/VectorInt.hpp"  // Vector3i

#include <cstddef>
#include <functional>
#include <vector>

// maz::render greedy voxel meshing — turn a 3D grid of blocks into a renderable surface mesh, merging every
// run of coplanar, same-type, equally-exposed block faces into ONE big quad (Mikola Lysenko's "greedy"
// algorithm). This is the reverse of MeshVoxelize (mesh -> voxels); it is what makes voxel worlds actually
// drawable. The naive approach emits two triangles per exposed block face — a flat 100x100 floor becomes
// 20,000 triangles; greedy meshing turns that same floor into a SINGLE quad (2 triangles). On real
// Minecraft/Teardown-style terrain it routinely cuts triangle counts by 5-10x, which is the difference
// between a chunk that renders and one that tanks the GPU. Only faces that are actually exposed (a solid
// cell whose neighbour across that face is empty) are emitted, and only faces of the same block type merge.
// Output is a list of axis-aligned quads with outward-facing winding, ready to expand into triangles.
// Header-only, std-only, deterministic. Godot has no voxel mesher.
namespace maz::render {

struct VoxelQuad {
    math::vec3 corner[4];    // counter-clockwise seen from the outside (normal) side
    math::Vector3i normal;   // one axis +/-1
    int type = 0;            // block type of the cell that owns this face (>0)
};

// Build the greedy mesh for an nx*ny*nz grid. `voxelAt(x,y,z)` returns the block type (>0) or 0 for empty;
// it is only queried inside [0,nx)x[0,ny)x[0,nz). Faces on the grid boundary are exposed.
inline std::vector<VoxelQuad> greedyVoxelMesh(int nx, int ny, int nz,
                                              const std::function<int(int, int, int)>& voxelAt) {
    std::vector<VoxelQuad> quads;
    if (nx <= 0 || ny <= 0 || nz <= 0) {
        return quads;
    }
    const int dim[3] = {nx, ny, nz};
    auto vox = [&](int c0, int c1, int c2) -> int {
        if (c0 < 0 || c1 < 0 || c2 < 0 || c0 >= nx || c1 >= ny || c2 >= nz) {
            return 0;
        }
        return voxelAt(c0, c1, c2);
    };

    for (int d = 0; d < 3; ++d) {
        const int u = (d + 1) % 3;
        const int v = (d + 2) % 3;
        const int W = dim[u];
        const int H = dim[v];

        // Assemble a world-space position with the three axis coordinates placed on their axes.
        auto makePos = [&](double dc, double uc, double vc) -> math::vec3 {
            double p[3];
            p[static_cast<std::size_t>(d)] = dc;
            p[static_cast<std::size_t>(u)] = uc;
            p[static_cast<std::size_t>(v)] = vc;
            return math::vec3{static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])};
        };
        auto voxAxis = [&](int dc, int uc, int vc) -> int {
            int c[3];
            c[static_cast<std::size_t>(d)] = dc;
            c[static_cast<std::size_t>(u)] = uc;
            c[static_cast<std::size_t>(v)] = vc;
            return vox(c[0], c[1], c[2]);
        };

        std::vector<int> mask(static_cast<std::size_t>(W) * static_cast<std::size_t>(H));
        for (int s = -1; s < dim[d]; ++s) {
            // Mask value: +type = face facing +d (owned by the cell at s); -type = face facing -d (owned by
            // the cell at s+1); 0 = interior or empty on both sides. Equal values merge.
            for (int j = 0; j < H; ++j) {
                for (int i = 0; i < W; ++i) {
                    const int a = (s >= 0) ? voxAxis(s, i, j) : 0;
                    const int b = (s + 1 < dim[d]) ? voxAxis(s + 1, i, j) : 0;
                    int m = 0;
                    if (a != 0 && b == 0) {
                        m = a;
                    } else if (a == 0 && b != 0) {
                        m = -b;
                    }
                    mask[static_cast<std::size_t>(i) + static_cast<std::size_t>(j) * static_cast<std::size_t>(W)] = m;
                }
            }

            // Greedy-merge the mask into maximal rectangles.
            for (int j = 0; j < H; ++j) {
                for (int i = 0; i < W;) {
                    const std::size_t idx0 = static_cast<std::size_t>(i) +
                                             static_cast<std::size_t>(j) * static_cast<std::size_t>(W);
                    const int val = mask[idx0];
                    if (val == 0) {
                        ++i;
                        continue;
                    }
                    // Width: extend along u while equal.
                    int w = 1;
                    while (i + w < W &&
                           mask[static_cast<std::size_t>(i + w) +
                                static_cast<std::size_t>(j) * static_cast<std::size_t>(W)] == val) {
                        ++w;
                    }
                    // Height: extend along v while the whole row matches.
                    int h = 1;
                    bool grow = true;
                    while (j + h < H && grow) {
                        for (int k = 0; k < w; ++k) {
                            if (mask[static_cast<std::size_t>(i + k) +
                                     static_cast<std::size_t>(j + h) * static_cast<std::size_t>(W)] != val) {
                                grow = false;
                                break;
                            }
                        }
                        if (grow) {
                            ++h;
                        }
                    }

                    // Emit the quad at the boundary plane d = s+1.
                    const double dc = static_cast<double>(s + 1);
                    const math::vec3 c00 = makePos(dc, i, j);
                    const math::vec3 c10 = makePos(dc, i + w, j);
                    const math::vec3 c11 = makePos(dc, i + w, j + h);
                    const math::vec3 c01 = makePos(dc, i, j + h);
                    VoxelQuad q;
                    int nn[3] = {0, 0, 0};
                    nn[static_cast<std::size_t>(d)] = (val > 0) ? 1 : -1;
                    q.normal = math::Vector3i{nn[0], nn[1], nn[2]};
                    q.type = (val > 0) ? val : -val;
                    if (val > 0) { // +d: CCW from +d side
                        q.corner[0] = c00; q.corner[1] = c10; q.corner[2] = c11; q.corner[3] = c01;
                    } else {       // -d: reversed winding
                        q.corner[0] = c00; q.corner[1] = c01; q.corner[2] = c11; q.corner[3] = c10;
                    }
                    quads.push_back(q);

                    // Clear the consumed region.
                    for (int jj = 0; jj < h; ++jj) {
                        for (int ii = 0; ii < w; ++ii) {
                            mask[static_cast<std::size_t>(i + ii) +
                                 static_cast<std::size_t>(j + jj) * static_cast<std::size_t>(W)] = 0;
                        }
                    }
                    i += w;
                }
            }
        }
    }
    return quads;
}

} // namespace maz::render
