#pragma once

#include "maz/math/Math.hpp"     // math::vec3, vec4, mat4
#include "maz/render/Image.hpp"  // render::Image, render::Color
#include "maz/render/Shapes.hpp" // render::shapes::MeshData, render::MeshVertex

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// SOFTWARE MESH PREVIEW — a tiny CPU rasterizer that renders a mesh into an Image with no GPU. The
// engine's real renderer is Vulkan (and has no offscreen-readback path), so to export a composed
// character/item as a picture or a cutscene GIF off the GPU entirely, this draws the mesh on the CPU:
// project each triangle with a view-projection matrix, z-buffer it, and flat-shade it (one Lambert
// term per face from a directional light × the face's averaged vertex colour + ambient). It extends
// the barycentric edge-function fill already in ImageDraw.hpp (fillTriangle / detail::edge) with a
// depth test and shading. Deliberately simple — flat per-triangle shading, no texturing, no
// perspective-correct attribute interpolation — it is a PREVIEW, not the PBR viewport. Header-only,
// pure CPU, deterministic, unit-tested headlessly.
namespace maz::render {

namespace detail {

// Float edge function (twice the signed area of triangle a,b,p); sign tells which side p is on.
inline float edgeF(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

} // namespace detail

// Render `mesh` into a `width` x `height` Image using `viewProj` (clip = viewProj * worldPos, depth
// 0..1 RH/ZO as from math::Projection), a directional light coming FROM `lightDir`, over `background`.
// Vertex positions are treated as world-space (bake node transforms in first, e.g. with
// editor::bakeComposite), as are the vertex normals used for shading.
inline Image renderMeshPreview(const shapes::MeshData& mesh, const math::mat4& viewProj,
                               const math::vec3& lightDir, int width, int height,
                               const Color& background) {
    Image img(width, height, background);
    if (width <= 0 || height <= 0 || mesh.indices.size() < 3) {
        return img;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<float> depth(pixels, 2.0f); // NDC z is 0..1; 2.0 is "farther than anything"

    const float lightLen = std::sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y +
                                     lightDir.z * lightDir.z);
    const math::vec3 L = lightLen > 1e-8f ? (lightDir / lightLen) : math::vec3(0.0f, -1.0f, 0.0f);
    const float fw = static_cast<float>(width);
    const float fh = static_cast<float>(height);
    constexpr float kAmbient = 0.25f;

    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const MeshVertex& a = mesh.vertices[mesh.indices[i]];
        const MeshVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const MeshVertex& c = mesh.vertices[mesh.indices[i + 2]];

        const math::vec4 ca = viewProj * math::vec4(a.px, a.py, a.pz, 1.0f);
        const math::vec4 cb = viewProj * math::vec4(b.px, b.py, b.pz, 1.0f);
        const math::vec4 cc = viewProj * math::vec4(c.px, c.py, c.pz, 1.0f);
        // Near-plane cull: drop the whole triangle if any vertex is at/behind the eye (w <= 0), which
        // would make the perspective divide meaningless.
        if (ca.w <= 1e-6f || cb.w <= 1e-6f || cc.w <= 1e-6f) {
            continue;
        }

        // Perspective divide -> NDC, then NDC -> screen (top-left origin, y down to match Image).
        const math::vec3 na(ca.x / ca.w, ca.y / ca.w, ca.z / ca.w);
        const math::vec3 nb(cb.x / cb.w, cb.y / cb.w, cb.z / cb.w);
        const math::vec3 nc(cc.x / cc.w, cc.y / cc.w, cc.z / cc.w);
        const float sx0 = (na.x * 0.5f + 0.5f) * fw, sy0 = (0.5f - na.y * 0.5f) * fh;
        const float sx1 = (nb.x * 0.5f + 0.5f) * fw, sy1 = (0.5f - nb.y * 0.5f) * fh;
        const float sx2 = (nc.x * 0.5f + 0.5f) * fw, sy2 = (0.5f - nc.y * 0.5f) * fh;

        const float area = detail::edgeF(sx0, sy0, sx1, sy1, sx2, sy2);
        if (std::fabs(area) < 1e-7f) {
            continue; // degenerate / zero-area after projection
        }

        // Flat shade: averaged vertex normal (world space) vs the light, averaged vertex colour.
        math::vec3 nrm(a.nx + b.nx + c.nx, a.ny + b.ny + c.ny, a.nz + b.nz + c.nz);
        const float nlen = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
        if (nlen > 1e-8f) {
            nrm /= nlen;
        }
        const float lambert = std::max(0.0f, -(nrm.x * L.x + nrm.y * L.y + nrm.z * L.z));
        const float shade = kAmbient + (1.0f - kAmbient) * lambert;
        const Color col{((a.r + b.r + c.r) / 3.0f) * shade, ((a.g + b.g + c.g) / 3.0f) * shade,
                        ((a.b + b.b + c.b) / 3.0f) * shade, 1.0f};

        // Bounding box of the projected triangle, clamped to the image.
        int minX = static_cast<int>(std::floor(std::min({sx0, sx1, sx2})));
        int maxX = static_cast<int>(std::ceil(std::max({sx0, sx1, sx2})));
        int minY = static_cast<int>(std::floor(std::min({sy0, sy1, sy2})));
        int maxY = static_cast<int>(std::ceil(std::max({sy0, sy1, sy2})));
        minX = std::max(minX, 0);
        minY = std::max(minY, 0);
        maxX = std::min(maxX, width - 1);
        maxY = std::min(maxY, height - 1);

        for (int py = minY; py <= maxY; ++py) {
            for (int px = minX; px <= maxX; ++px) {
                const float cx = static_cast<float>(px) + 0.5f;
                const float cy = static_cast<float>(py) + 0.5f;
                const float w0 = detail::edgeF(sx1, sy1, sx2, sy2, cx, cy);
                const float w1 = detail::edgeF(sx2, sy2, sx0, sy0, cx, cy);
                const float w2 = detail::edgeF(sx0, sy0, sx1, sy1, cx, cy);
                const bool inside =
                    (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) || (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
                if (!inside) {
                    continue;
                }
                // Barycentric depth (NDC z). Weights normalized by the signed area.
                const float z = (w0 * na.z + w1 * nb.z + w2 * nc.z) / area;
                if (z < 0.0f || z > 1.0f) {
                    continue; // outside the clip depth range
                }
                const std::size_t idx =
                    static_cast<std::size_t>(py) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(px);
                if (z < depth[idx]) {
                    depth[idx] = z;
                    img.setPixel(px, py, col);
                }
            }
        }
    }
    return img;
}

} // namespace maz::render
