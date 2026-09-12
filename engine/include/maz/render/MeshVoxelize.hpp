#pragma once

#include "maz/math/Math.hpp"                    // math::vec3
#include "maz/render/MeshAmbientOcclusion.hpp"  // detail::aoRayTri (Möller–Trumbore ray/triangle)
#include "maz/render/Shapes.hpp"                // shapes::MeshData, MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render SOLID VOXELIZATION — convert a closed triangle mesh into a boolean 3D occupancy grid: each cell
// is 1 if its centre lies INSIDE the solid, 0 if outside. This is the bridge from surface geometry to the
// volumetric representations games lean on: destructible/editable voxel terrain (Minecraft/Teardown-style),
// building nav volumes for 3D pathfinding, GPU-particle/fluid collision masks, fast approximate inside tests,
// procedural interior filling, and the input to a marching-cubes/SurfaceNets re-mesh. Unlike a SURFACE
// voxelization (which only marks cells the triangles pass through), this is a SOLID fill: the inside/outside
// decision at each cell centre is a ray-parity test (cast one oblique ray, an odd triangle-crossing count
// means inside), reusing the M533 Möller–Trumbore ray/triangle — the same sign test MeshSdf (M534) uses at
// its grid corners. Pure CPU, header-only, headless — verifiable because a filled solid's voxel count times
// the cell volume must converge to the mesh's true volume.
//
// Scope note (honest): brute-force O(cells · tris) with a ray-parity sign, so it assumes a watertight mesh
// and is intended for offline bakes; a BVH (game::Bvh) to prune the ray test and a generalized-winding-number
// sign for open meshes are the documented follow-ups (shared with MeshSdf).
namespace maz::render {

struct VoxelGrid {
    int nx = 0, ny = 0, nz = 0;
    math::vec3 origin{0, 0, 0};        // world position of the grid's min corner (cell (0,0,0) centre is +0.5)
    float voxelSize = 0.0f;            // uniform edge length of one cubic cell
    std::vector<std::uint8_t> solid;   // 1 = inside, index = (z*ny + y)*nx + x

    std::uint8_t at(int x, int y, int z) const {
        return solid[(static_cast<std::size_t>(z) * static_cast<std::size_t>(ny)
                      + static_cast<std::size_t>(y)) * static_cast<std::size_t>(nx)
                     + static_cast<std::size_t>(x)];
    }
    // World-space centre of cell (x,y,z).
    math::vec3 cellCenter(int x, int y, int z) const {
        return origin + math::vec3(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f,
                                   static_cast<float>(z) + 0.5f) * voxelSize;
    }
    std::size_t solidCount() const {
        std::size_t n = 0;
        for (std::uint8_t s : solid) n += s;
        return n;
    }
    // Estimated enclosed volume = (number of solid cells) * (cell volume). Converges to the true volume.
    double estimatedVolume() const {
        const double v = static_cast<double>(voxelSize);
        return static_cast<double>(solidCount()) * v * v * v;
    }
};

// Voxelize `mesh` into a solid occupancy grid. The mesh bounding box is expanded by `padding` on every side
// (so the surface never sits exactly on the outer cells), and `resolution` (>= 1) cells span the longest axis;
// the cell edge length is uniform. A degenerate/empty mesh yields an empty grid.
inline VoxelGrid voxelizeSolid(const shapes::MeshData& mesh, int resolution, float padding = 0.0f) {
    VoxelGrid grid;
    if (mesh.indices.size() < 3 || mesh.vertices.empty() || resolution < 1) return grid;

    std::vector<math::vec3> pos(mesh.vertices.size());
    math::vec3 bmin(1e30f, 1e30f, 1e30f), bmax(-1e30f, -1e30f, -1e30f);
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        pos[i] = math::vec3(mesh.vertices[i].px, mesh.vertices[i].py, mesh.vertices[i].pz);
        bmin = math::vec3(std::min(bmin.x, pos[i].x), std::min(bmin.y, pos[i].y), std::min(bmin.z, pos[i].z));
        bmax = math::vec3(std::max(bmax.x, pos[i].x), std::max(bmax.y, pos[i].y), std::max(bmax.z, pos[i].z));
    }
    bmin -= math::vec3(padding, padding, padding);
    bmax += math::vec3(padding, padding, padding);
    const math::vec3 ext = bmax - bmin;
    const float longest = std::max(ext.x, std::max(ext.y, ext.z));
    const float voxel = longest / static_cast<float>(resolution);
    if (voxel <= 0.0f) return grid;

    grid.origin = bmin;
    grid.voxelSize = voxel;
    grid.nx = std::max(1, static_cast<int>(std::ceil(ext.x / voxel)));
    grid.ny = std::max(1, static_cast<int>(std::ceil(ext.y / voxel)));
    grid.nz = std::max(1, static_cast<int>(std::ceil(ext.z / voxel)));
    grid.solid.assign(static_cast<std::size_t>(grid.nx) * static_cast<std::size_t>(grid.ny)
                      * static_cast<std::size_t>(grid.nz), 0);

    const std::size_t triN = mesh.indices.size();
    const math::vec3 rayDir = math::vec3(0.573462f, 0.331298f, 0.748981f); // oblique — dodges axis-aligned edges
    const float maxRay = longest * 4.0f + 1.0f;

    for (int z = 0; z < grid.nz; ++z)
        for (int y = 0; y < grid.ny; ++y)
            for (int x = 0; x < grid.nx; ++x) {
                const math::vec3 c = grid.cellCenter(x, y, z);
                int crossings = 0;
                for (std::size_t t = 0; t + 2 < triN; t += 3) {
                    const math::vec3& a = pos[mesh.indices[t]];
                    const math::vec3& b = pos[mesh.indices[t + 1]];
                    const math::vec3& d = pos[mesh.indices[t + 2]];
                    float tt;
                    if (detail::aoRayTri(c, rayDir, a, b, d, maxRay, tt)) ++crossings;
                }
                if (crossings & 1) {
                    const std::size_t idx = (static_cast<std::size_t>(z) * static_cast<std::size_t>(grid.ny)
                                             + static_cast<std::size_t>(y)) * static_cast<std::size_t>(grid.nx)
                                            + static_cast<std::size_t>(x);
                    grid.solid[idx] = 1;
                }
            }
    return grid;
}

} // namespace maz::render
