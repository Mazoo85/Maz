#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::math generalized winding number — decide whether a point is INSIDE a closed triangle mesh, robustly.
// "Is this point inside the volume?" is the query behind spawning objects inside an arbitrary shape,
// containment/region tests, voxelizing a solid, inside/outside masks for particle or fluid collision, and
// point-in-lava/point-in-water gameplay checks. The naive way (cast a ray and count surface crossings) is
// brittle: one missing triangle, a T-junction, or a ray grazing an edge flips the answer. The generalized
// winding number sums the solid angle each triangle subtends at the point (Van Oosterom-Strackee), giving
// ~+/-1 for interior points and ~0 for exterior ones, and — crucially — it DEGRADES GRACEFULLY: a mesh with
// small holes still reads ~1 inside, where ray parity would leak. MeshVoxelize's own note calls for exactly
// this. Winding sign follows triangle orientation, so pointInMesh takes the magnitude and is orientation
// -agnostic. Header-only, std-only, deterministic; O(triangles) per query (fine for offline bakes / probes).
namespace maz::math {

// Signed generalized winding number of `p` with respect to the triangle mesh (verts + triangle indices).
// ~+1 or ~-1 inside (sign = winding), ~0 outside.
inline float windingNumber(const std::vector<vec3>& verts, const std::vector<std::uint32_t>& indices,
                           const vec3& p) {
    double total = 0.0;
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        const vec3 a = verts[indices[t]] - p;
        const vec3 b = verts[indices[t + 1]] - p;
        const vec3 c = verts[indices[t + 2]] - p;
        const double la = std::sqrt(static_cast<double>(a.x) * a.x + static_cast<double>(a.y) * a.y +
                                    static_cast<double>(a.z) * a.z);
        const double lb = std::sqrt(static_cast<double>(b.x) * b.x + static_cast<double>(b.y) * b.y +
                                    static_cast<double>(b.z) * b.z);
        const double lc = std::sqrt(static_cast<double>(c.x) * c.x + static_cast<double>(c.y) * c.y +
                                    static_cast<double>(c.z) * c.z);
        // Scalar triple product a . (b x c).
        const double num = static_cast<double>(a.x) * (static_cast<double>(b.y) * c.z - static_cast<double>(b.z) * c.y) +
                           static_cast<double>(a.y) * (static_cast<double>(b.z) * c.x - static_cast<double>(b.x) * c.z) +
                           static_cast<double>(a.z) * (static_cast<double>(b.x) * c.y - static_cast<double>(b.y) * c.x);
        const double ab = static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y + static_cast<double>(a.z) * b.z;
        const double bc = static_cast<double>(b.x) * c.x + static_cast<double>(b.y) * c.y + static_cast<double>(b.z) * c.z;
        const double ca = static_cast<double>(c.x) * a.x + static_cast<double>(c.y) * a.y + static_cast<double>(c.z) * a.z;
        const double den = la * lb * lc + ab * lc + bc * la + ca * lb;
        total += 2.0 * std::atan2(num, den); // signed solid angle
    }
    return static_cast<float>(total / (4.0 * 3.14159265358979323846));
}

// True if `p` is inside the mesh (|winding number| above `threshold`, default 0.5). Orientation-agnostic.
inline bool pointInMesh(const std::vector<vec3>& verts, const std::vector<std::uint32_t>& indices,
                        const vec3& p, float threshold = 0.5f) {
    return std::fabs(windingNumber(verts, indices, p)) > threshold;
}

} // namespace maz::math
