#pragma once

#include "maz/math/Math.hpp"        // math::vec3
#include "maz/render/MeshTools.hpp"  // computeNormals
#include "maz/render/Shapes.hpp"     // shapes::MeshData, MeshVertex

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render HEIGHTFIELD / TERRAIN MESH — turn a flat grid of height numbers into a rolling 3D terrain surface.
// You hand it a `cols` × `rows` grid of heights (row-major: one float per grid point, e.g. straight out of a
// Perlin/fbm noise function, a greyscale heightmap image, or hand-authored contours) and it lays down a vertex
// at every grid point, lifts each one to its height, and stitches the whole sheet together with two triangles
// per cell. This is the bread-and-butter of outdoor game worlds — hills, dunes, valleys, ocean floors, golf
// courses — and it's exactly Godot's HeightMapShape3D / a terrain node's mesh. The grid is centred on the origin
// and laid on the XZ plane with height along +Y, so it drops straight into a scene. Smooth per-vertex normals
// come from the engine's area-weighted `computeNormals`, so lighting follows the slopes for free. Header-only,
// deterministic, headless — pure CPU geometry, no GPU needed to build or verify it.
//
// Scope note (honest): this builds an open single-sided sheet (no skirt/underside/walls) — it is the terrain
// SURFACE, not a closed solid; add a skirt or extrude down if you need thickness or watertightness. `cellSize`
// is the world spacing between neighbouring grid points and `heightScale` multiplies the raw height values.
// Needs at least a 2×2 grid, and `heights.size()` must equal `cols*rows`, or an empty mesh is returned.
namespace maz::render {

// Build a terrain mesh from a row-major `cols`×`rows` grid of heights. Centred on the origin, +Y up.
inline shapes::MeshData buildHeightfield(const std::vector<float>& heights, int cols, int rows,
                                         float cellSize = 1.0f, float heightScale = 1.0f) {
    shapes::MeshData out;
    if (cols < 2 || rows < 2) return out;
    const std::size_t need = static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
    if (heights.size() != need) return out;

    const float cx = static_cast<float>(cols - 1) * 0.5f; // centre the grid on the origin
    const float cz = static_cast<float>(rows - 1) * 0.5f;

    // --- Positions: one lifted vertex per grid point. ---
    std::vector<math::vec3> positions;
    positions.reserve(need);
    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < cols; ++i) {
            const float h = heights[static_cast<std::size_t>(j) * static_cast<std::size_t>(cols) +
                                    static_cast<std::size_t>(i)];
            positions.push_back(math::vec3((static_cast<float>(i) - cx) * cellSize, h * heightScale,
                                           (static_cast<float>(j) - cz) * cellSize));
        }
    }

    // --- Faces: two upward-facing triangles per cell (diagonal v00-v11). ---
    std::vector<std::uint32_t> indices;
    indices.reserve(static_cast<std::size_t>(cols - 1) * static_cast<std::size_t>(rows - 1) * 6);
    for (int j = 0; j + 1 < rows; ++j) {
        for (int i = 0; i + 1 < cols; ++i) {
            const std::uint32_t v00 = static_cast<std::uint32_t>(j * cols + i);
            const std::uint32_t v10 = static_cast<std::uint32_t>(j * cols + i + 1);
            const std::uint32_t v01 = static_cast<std::uint32_t>((j + 1) * cols + i);
            const std::uint32_t v11 = static_cast<std::uint32_t>((j + 1) * cols + i + 1);
            indices.push_back(v00); indices.push_back(v11); indices.push_back(v10); // +Y winding
            indices.push_back(v00); indices.push_back(v01); indices.push_back(v11);
        }
    }

    // --- Smooth normals + assemble vertices (white, UV across the grid). ---
    const std::vector<math::vec3> normals = computeNormals(positions, indices);
    out.vertices.reserve(need);
    for (int j = 0; j < rows; ++j) {
        for (int i = 0; i < cols; ++i) {
            const std::size_t k = static_cast<std::size_t>(j) * static_cast<std::size_t>(cols) +
                                  static_cast<std::size_t>(i);
            MeshVertex v{};
            v.px = positions[k].x; v.py = positions[k].y; v.pz = positions[k].z;
            v.nx = normals[k].x; v.ny = normals[k].y; v.nz = normals[k].z;
            v.r = v.g = v.b = 1.0f;
            v.u = static_cast<float>(i) / static_cast<float>(cols - 1);
            v.v = static_cast<float>(j) / static_cast<float>(rows - 1);
            out.vertices.push_back(v);
        }
    }
    out.indices = std::move(indices);
    return out;
}

} // namespace maz::render
