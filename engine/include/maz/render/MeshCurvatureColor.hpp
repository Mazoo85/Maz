#pragma once

#include "maz/render/MeshCurvature.hpp" // computeCurvature, MeshCurvature
#include "maz/render/Shapes.hpp"        // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render CURVATURE HEATMAP — paint a mesh so you can SEE where it bends: flat regions go cool blue, gently
// curved areas green, and sharp creases/tips hot red. This is the standard "curvature map" every DCC tool and
// inspection package shows — it's how modellers spot pinching, lumps, and over-sharp edges that will shade badly,
// and how a retopo/QA pass finds the high-detail zones. It runs the engine's `computeCurvature` (M535) and maps
// each vertex's curvature MAGNITUDE through a blue→green→red colour ramp, normalised so the mesh's own maximum
// (or a supplied `maxValue`) becomes full red. Pick mean curvature |H| (creases, the usual choice) or Gaussian
// curvature |K| (spherical-vs-saddle points). Header-only, pure CPU, deterministic.
//
// Scope note (honest): this overwrites the RGB of every vertex with a debug colour (positions/normals/UVs
// untouched); it is a visualisation, not a physical signal. Curvature at open BOUNDARY vertices is unreliable
// (the estimate needs a full one-ring), so rim vertices may read low — trust the interior. Auto-normalisation
// means the colours are RELATIVE to this mesh's own range; pass an explicit `maxValue` to compare two meshes on
// the same scale.
namespace maz::render {

enum class CurvatureKind { Mean, Gaussian };

namespace detail {
// Blue (t=0) -> green (0.5) -> red (1) ramp. t is clamped to [0,1].
inline void curvatureColor(float t, float& r, float& g, float& b) {
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (t < 0.5f) { const float u = t * 2.0f; r = 0.0f; g = u; b = 1.0f - u; }
    else { const float u = (t - 0.5f) * 2.0f; r = u; g = 1.0f - u; b = 0.0f; }
}
} // namespace detail

// Return a copy of `mesh` tinted by curvature magnitude. `maxValue <= 0` auto-normalises to the mesh's own peak.
inline shapes::MeshData curvatureHeatmap(const shapes::MeshData& mesh, CurvatureKind kind = CurvatureKind::Mean,
                                         float maxValue = 0.0f) {
    shapes::MeshData out = mesh;
    if (out.vertices.empty()) return out;

    const MeshCurvature cur = computeCurvature(mesh);
    const std::vector<float>& src = (kind == CurvatureKind::Gaussian) ? cur.gaussian : cur.mean;
    if (src.size() != out.vertices.size()) return out; // defensive

    // Magnitude per vertex, and the normalising maximum.
    std::vector<float> mag(src.size());
    float peak = 0.0f;
    for (std::size_t i = 0; i < src.size(); ++i) {
        mag[i] = std::fabs(src[i]);
        if (mag[i] > peak) peak = mag[i];
    }
    const float norm = maxValue > 0.0f ? maxValue : peak;
    const float inv = norm > 0.0f ? 1.0f / norm : 0.0f; // all-flat mesh -> every t is 0 -> all blue

    for (std::size_t i = 0; i < out.vertices.size(); ++i) {
        const float t = mag[i] * inv;
        float r = 0, g = 0, b = 1;
        detail::curvatureColor(t, r, g, b);
        out.vertices[i].r = r;
        out.vertices[i].g = g;
        out.vertices[i].b = b;
    }
    return out;
}

} // namespace maz::render
