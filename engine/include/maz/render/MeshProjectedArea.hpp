#pragma once

#include "maz/math/Math.hpp"     // math::vec3, cross, dot
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>

// maz::render PROJECTED / FRONTAL AREA — how big a shadow does this model cast when you look at it from a given
// direction? It measures the area of the model's outline as projected onto the screen — the "frontal area" an
// engineer means by cross-section. That number drives a lot of game and sim math: aerodynamic / water drag (drag
// scales with frontal area), wind load on a structure, how much sunlight a solar panel or leaf catches, the size
// of the shadow a light casts, or how tightly a camera needs to frame an object. It sums the projected area of
// every triangle that FACES the given direction (a triangle's contribution is its area times how square-on it is),
// which for a closed convex shape is exactly the silhouette area. Reuses only vec3/cross/dot. Header-only, pure CPU.
//
// Scope note (honest): this is EXACT for a convex closed mesh (a box, a sphere, a convex hull) — there the
// front-facing triangles tile the silhouette with no overlap. For a CONCAVE mesh it is an UPPER BOUND: folds that
// hide behind nearer surface still count, so the true visible silhouette can be smaller (use a rasterised/coverage
// method if you need the exact projected area of a concave shape). `direction` need not be unit length. The value
// is symmetric in the view direction (a closed mesh has the same frontal area from the front and the back).
namespace maz::render {

// Frontal (projected) area of `mesh` as seen along `direction`. Exact for convex closed meshes.
inline float projectedArea(const shapes::MeshData& mesh, const math::vec3& direction) {
    const float dl = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (dl < 1e-20f) return 0.0f;
    const math::vec3 d(direction.x / dl, direction.y / dl, direction.z / dl);

    auto pos = [&](std::uint32_t i) {
        const MeshVertex& v = mesh.vertices[i];
        return math::vec3(v.px, v.py, v.pz);
    };

    float area = 0.0f;
    const std::size_t triN = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < triN; ++t) {
        const std::uint32_t ia = mesh.indices[t * 3 + 0];
        const std::uint32_t ib = mesh.indices[t * 3 + 1];
        const std::uint32_t ic = mesh.indices[t * 3 + 2];
        if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() || ic >= mesh.vertices.size()) continue;
        const math::vec3 a = pos(ia), b = pos(ib), c = pos(ic);
        // cross = 2*area*unitNormal; its dot with d is 2*(signed projected area). Keep the front-facing half.
        const float proj = 0.5f * math::dot(math::cross(b - a, c - a), d);
        if (proj > 0.0f) area += proj; // front-facing triangles only
    }
    return area;
}

} // namespace maz::render
