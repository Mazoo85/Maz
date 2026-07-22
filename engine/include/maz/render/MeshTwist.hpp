#pragma once

#include "maz/math/Math.hpp"     // math::vec3
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

// maz::render TWIST — spiral a mesh around an axis: the further a vertex sits along the axis, the more it is
// rotated about it, so a straight bar becomes a corkscrew, a blade gains a spiral flute, a tower gets a helical
// sweep. This is Blender's "Simple Deform → Twist" and the classic way to add a wound, spiralled, or barley-sugar
// look to otherwise straight geometry without hand-modelling every ring. The rotation angle at a vertex is
// `radiansPerUnit × (its distance along the axis from `centre`)`, applied in the plane perpendicular to the axis,
// about the axis line through `centre`. Both the position AND the normal's perpendicular components are rotated,
// so lighting follows the twist. axis: 0 = X, 1 = Y, 2 = Z. Header-only, pure CPU, deterministic.
//
// Scope note (honest): this is a rigid rotation per cross-section — it preserves each vertex's height along the
// axis and its distance from the axis exactly (no stretching), so a cylinder stays the same radius as it winds.
// The visible smoothness of the spiral is limited by how many rings the mesh has along the axis: a bar with only
// two rings (top and bottom) just shears into a parallelogram twist; subdivide along the axis first for a smooth
// helix. `radiansPerUnit` may be negative to wind the other way.
namespace maz::render {

// Twist `mesh` about `axis` (0=X,1=Y,2=Z): each vertex rotates by radiansPerUnit * (axis-coord − centre) about the
// axis line through `centre`. Returns a twisted copy.
inline shapes::MeshData twistMesh(const shapes::MeshData& mesh, int axis, float radiansPerUnit,
                                  math::vec3 centre = math::vec3(0, 0, 0)) {
    shapes::MeshData out = mesh;
    const int a = axis < 0 ? 0 : (axis > 2 ? 2 : axis);

    // Along `a` is the twist axis; (u, v) are the two perpendicular components rotated in-plane.
    // a=0(X): coord=x, (u,v)=(y,z); a=1(Y): coord=y, (u,v)=(z,x); a=2(Z): coord=z, (u,v)=(x,y).
    auto axisCoord = [a](float x, float y, float z) { return a == 0 ? x : (a == 1 ? y : z); };
    const float cAxis = axisCoord(centre.x, centre.y, centre.z);
    const float cU = a == 0 ? centre.y : (a == 1 ? centre.z : centre.x);
    const float cV = a == 0 ? centre.z : (a == 1 ? centre.x : centre.y);

    for (MeshVertex& vtx : out.vertices) {
        const float coord = axisCoord(vtx.px, vtx.py, vtx.pz);
        const float angle = radiansPerUnit * (coord - cAxis);
        const float s = std::sin(angle), co = std::cos(angle);

        // Read this vertex's (u, v) perpendicular components relative to the centre, and its normal's.
        const float pu = (a == 0 ? vtx.py : (a == 1 ? vtx.pz : vtx.px)) - cU;
        const float pv = (a == 0 ? vtx.pz : (a == 1 ? vtx.px : vtx.py)) - cV;
        const float nu = a == 0 ? vtx.ny : (a == 1 ? vtx.nz : vtx.nx);
        const float nv = a == 0 ? vtx.nz : (a == 1 ? vtx.nx : vtx.ny);

        const float pu2 = pu * co - pv * s, pv2 = pu * s + pv * co;
        const float nu2 = nu * co - nv * s, nv2 = nu * s + nv * co;

        // Write the rotated components back (adding the centre offset to positions).
        if (a == 0) {
            vtx.py = pu2 + cU; vtx.pz = pv2 + cV; vtx.ny = nu2; vtx.nz = nv2;
        } else if (a == 1) {
            vtx.pz = pu2 + cU; vtx.px = pv2 + cV; vtx.nz = nu2; vtx.nx = nv2;
        } else {
            vtx.px = pu2 + cU; vtx.py = pv2 + cV; vtx.nx = nu2; vtx.ny = nv2;
        }
    }
    return out;
}

} // namespace maz::render
