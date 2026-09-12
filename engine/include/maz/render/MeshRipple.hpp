#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

// maz::render RIPPLE / WAVE — send concentric ripples across a surface: each vertex is pushed along one axis by a
// sine wave of its DISTANCE from a centre point, so a flat plane becomes a pond after a stone drops, a disc
// becomes a vinyl-record warp, a flag gets a rippling wobble. This is the "Ripple"/"Wave" deformer every DCC tool
// ships. The push along `axis` is `amplitude · sin(radial · frequency − phase)` where `radial` is the distance
// from `centre` measured in the plane perpendicular to `axis` (so the rings are truly circular). `frequency` sets
// how tightly packed the rings are, `amplitude` how tall the crests, and animating `phase` makes the rings travel
// outward (or inward) — the whole animated-water effect is just `phase += speed · dt` each frame. Deterministic
// and pure. Header-only, CPU.
//
// Scope note (honest): this offsets POSITIONS only, along a single axis — normals are left as they were, so the
// stored shading no longer matches the rippled surface; re-run `computeNormals` afterwards for correct lighting on
// the crests and troughs. The wave's smoothness is limited by the mesh's tessellation along the radius: too few
// rings of vertices and the sine reads as jagged facets (subdivide first). `amplitude`/`frequency`/`phase` may be
// any value; frequency 0 pushes the whole surface by a constant `amplitude·sin(−phase)`.
namespace maz::render {

// Ripple `mesh` along `axis` (0=X,1=Y,2=Z): each vertex moves by amplitude·sin(radial·frequency − phase), where
// `radial` is its distance from `centre` in the plane perpendicular to `axis`. Returns a rippled copy.
inline shapes::MeshData rippleMesh(const shapes::MeshData& mesh, int axis, float amplitude, float frequency,
                                   float phase = 0.0f, math::vec3 centre = math::vec3(0, 0, 0)) {
    shapes::MeshData out = mesh;
    const int a = axis < 0 ? 0 : (axis > 2 ? 2 : axis);

    // The two axes perpendicular to `a` span the plane the radius is measured in.
    const int p = (a + 1) % 3, q = (a + 2) % 3;
    auto vcoord = [](const MeshVertex& v, int ax) { return ax == 0 ? v.px : (ax == 1 ? v.py : v.pz); };
    auto ccoord = [&centre](int ax) { return ax == 0 ? centre.x : (ax == 1 ? centre.y : centre.z); };
    const float cp = ccoord(p), cq = ccoord(q);

    for (MeshVertex& v : out.vertices) {
        const float dp = vcoord(v, p) - cp, dq = vcoord(v, q) - cq;
        const float radial = std::sqrt(dp * dp + dq * dq);
        const float offset = amplitude * std::sin(radial * frequency - phase);
        if (a == 0) v.px += offset; else if (a == 1) v.py += offset; else v.pz += offset;
    }
    return out;
}

} // namespace maz::render
