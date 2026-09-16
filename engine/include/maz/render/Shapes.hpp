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

// UV sphere (latitude/longitude): `rings` = latitude segments, `sectors` = longitude segments. The
// everyday sphere, and the one to reach for when you need per-vertex UVs that run cleanly around and
// over the ball.
//
// Scope note (honest): this is NOT a closed shell, and its triangles are not evenly shaped — both are
// inherent to the lat/long construction rather than fixable here, so pick deliberately.
//   * OPEN. The wrap seam duplicates a whole column of vertices (the same positions, different U), and
//     each pole row is a run of coincident vertices for the same reason. Geometrically the ball is
//     sealed; topologically it has a boundary at the seam and at both poles, so analyzeWatertight
//     reports it open. Anything needing a real solid — volume, booleans, printing — wants makeIcosphere
//     (M-icosphere), which is watertight.
//   * UNEVEN. Triangles crowd toward the poles: at 10x20 the worst shape-quality score is 0.47 against
//     an icosphere's 0.98. Fine for texturing, poor as a base for subdivision or decimation.
// What it does NOT do any more is emit degenerate triangles. A textbook UV sphere makes two triangles
// per quad everywhere, including against the pole rows where both "corners" on the pole are the same
// point and one triangle is therefore flat — 2 * sectors of them, 10% of a 10x20 sphere. They drew
// nothing but broke normal averaging, defeated decimation and tripped every mesh-health check, so the
// flat one is skipped. The pole rows are also snapped to exactly 0 and +/-radius: float pi makes
// sin(theta) at the south pole -8.7e-08 rather than 0, which spread that pole over a ring 1e-7 wide
// whose triangles slipped past any is-this-zero-area test while scoring 7e-08 on quality.
MeshData makeSphere(float radius, int rings, int sectors, const Color& color);

// Flat quad on the XZ plane (y = 0), spanning [-halfSize, +halfSize], normal pointing up.
MeshData makePlane(float halfSize, const Color& color);

} // namespace maz::render::shapes
