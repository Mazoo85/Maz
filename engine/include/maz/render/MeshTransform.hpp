#pragma once

#include "maz/math/Math.hpp"     // math::mat4, mat3, vec3, quat (glm re-exports)
#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>

#include <glm/gtc/matrix_transform.hpp> // glm::translate, rotate, scale

// maz::render APPLY / BAKE TRANSFORM — permanently apply a 4×4 transform (translate + rotate + scale) to a mesh's
// geometry, moving both its POSITIONS and its NORMALS correctly. This "freeze transform / apply transform" step is
// everywhere in a content pipeline: flatten a node's transform into its mesh before export, merge several placed
// copies into one buffer, pre-bake an import fix-up (a rotate to swap Y-up/Z-up, a scale to convert units) so the
// runtime does no per-frame matrix work, or snapshot an instance. The subtlety it gets right: normals do NOT
// transform by the same matrix as positions under NON-UNIFORM scale — they use the INVERSE-TRANSPOSE of the 3×3
// part, then renormalize, so a squashed surface keeps its normals perpendicular (naive transforms leave them
// skewed and lighting goes wrong). Ships with translate/scale/rotate matrix builders so callers needn't touch
// glm. Header-only, pure CPU.
//
// Scope note (honest): this BAKES the transform into vertex data — it does not keep a separate transform (that's a
// scene-graph node's job). Tangents (if present) are not recomputed here (regenerate via computeTangents after a
// mirroring/negative-scale transform, which also flips winding — pair with M562 if the determinant is negative).
// Positions use the full 4×4 (translation included); normals use only the inverse-transpose 3×3 (translation-free)
// and are renormalized, so a zero/degenerate normal stays zero.
namespace maz::render {

// Convenience matrix builders (so callers don't need to include glm directly).
inline math::mat4 translationMatrix(const math::vec3& t) { return glm::translate(math::mat4(1.0f), t); }
inline math::mat4 scaleMatrix(const math::vec3& s) { return glm::scale(math::mat4(1.0f), s); }
inline math::mat4 rotationMatrix(const math::vec3& axis, float radians) {
    return glm::rotate(math::mat4(1.0f), radians, axis);
}

// Return a copy of `mesh` with `m` baked into every vertex position (full 4×4) and normal (inverse-transpose 3×3,
// renormalized). UVs and colours are untouched.
inline shapes::MeshData applyTransform(const shapes::MeshData& mesh, const math::mat4& m) {
    shapes::MeshData out = mesh;
    const math::mat3 normalMat = glm::transpose(glm::inverse(math::mat3(m)));
    for (std::size_t i = 0; i < out.vertices.size(); ++i) {
        MeshVertex& v = out.vertices[i];
        const math::vec4 p = m * math::vec4(v.px, v.py, v.pz, 1.0f);
        v.px = p.x;
        v.py = p.y;
        v.pz = p.z;
        math::vec3 n = normalMat * math::vec3(v.nx, v.ny, v.nz);
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-20f) {
            v.nx = n.x / len;
            v.ny = n.y / len;
            v.nz = n.z / len;
        }
    }
    return out;
}

} // namespace maz::render
