#pragma once

#include "maz/render/Renderer.hpp"

#include <cstdint>
#include <vector>

namespace maz::render::shapes {

// CPU geometry ready to hand to Renderer::createMesh. Normals point outward; every vertex is
// tinted `color`.
struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;
};

// Axis-aligned cube of the given full edge length, centered at the origin.
MeshData makeBox(float size, const Color& color);

// UV sphere. `rings` = latitude segments, `sectors` = longitude segments.
MeshData makeSphere(float radius, int rings, int sectors, const Color& color);

// Flat quad on the XZ plane (y = 0), spanning [-halfSize, +halfSize], normal pointing up.
MeshData makePlane(float halfSize, const Color& color);

} // namespace maz::render::shapes
