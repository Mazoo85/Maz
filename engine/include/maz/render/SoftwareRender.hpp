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
// project each triangle with a view-projection matrix, z-buffer it, and shade it per pixel.
//
// Shading is SMOOTH (Phong): the vertex normals and colours are interpolated across each triangle, so
// curved surfaces read as curved rather than faceted, and a Blinn-Phong specular highlight names the
// material instead of leaving everything matte. It extends the barycentric edge-function fill already in
// ImageDraw.hpp (fillTriangle / detail::edge) with a depth test and per-pixel shading. Still a PREVIEW,
// not the PBR viewport: one directional light, no textures, screen-space (affine) attribute interpolation.
// Header-only, pure CPU, deterministic, unit-tested headlessly.
namespace maz::render {

namespace detail {

// Float edge function (twice the signed area of triangle a,b,p); sign tells which side p is on.
inline float edgeF(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

inline math::vec3 normalize3(const math::vec3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return len > 1e-8f ? math::vec3(v.x / len, v.y / len, v.z / len) : math::vec3(0.0f, 0.0f, 0.0f);
}
inline float dot3(const math::vec3& a, const math::vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

} // namespace detail

// Render `mesh` into a `width` x `height` Image using `viewProj` (clip = viewProj * worldPos, depth
// 0..1 RH/ZO as from math::Projection), a directional light coming FROM `lightDir`, viewed from world
// position `eye` (used for the specular highlight), over `background`. Vertex positions and normals are
// treated as world-space (bake node transforms in first, e.g. with editor::bakeComposite).
inline Image renderMeshPreview(const shapes::MeshData& mesh, const math::mat4& viewProj,
                               const math::vec3& lightDir, const math::vec3& eye, int width, int height,
                               const Color& background) {
    Image img(width, height, background);
    if (width <= 0 || height <= 0 || mesh.indices.size() < 3) {
        return img;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<float> depth(pixels, 2.0f); // NDC z is 0..1; 2.0 is "farther than anything"

    const math::vec3 L = detail::normalize3(lightDir);           // travel direction of the light
    const math::vec3 toLight(-L.x, -L.y, -L.z);                  // direction from surface toward the light
    const float fw = static_cast<float>(width);
    const float fh = static_cast<float>(height);
    constexpr float kAmbient = 0.22f;
    constexpr float kShininess = 28.0f;
    constexpr float kSpecular = 0.35f;

    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const MeshVertex& a = mesh.vertices[mesh.indices[i]];
        const MeshVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const MeshVertex& c = mesh.vertices[mesh.indices[i + 2]];

        const math::vec4 ca = viewProj * math::vec4(a.px, a.py, a.pz, 1.0f);
        const math::vec4 cb = viewProj * math::vec4(b.px, b.py, b.pz, 1.0f);
        const math::vec4 cc = viewProj * math::vec4(c.px, c.py, c.pz, 1.0f);
        // Near-plane cull: drop the whole triangle if any vertex is at/behind the eye (w <= 0).
        if (ca.w <= 1e-6f || cb.w <= 1e-6f || cc.w <= 1e-6f) {
            continue;
        }

        // Perspective divide -> NDC, then NDC -> screen (top-left origin, y down to match Image).
        const math::vec3 nd0(ca.x / ca.w, ca.y / ca.w, ca.z / ca.w);
        const math::vec3 nd1(cb.x / cb.w, cb.y / cb.w, cb.z / cb.w);
        const math::vec3 nd2(cc.x / cc.w, cc.y / cc.w, cc.z / cc.w);
        const float sx0 = (nd0.x * 0.5f + 0.5f) * fw, sy0 = (0.5f - nd0.y * 0.5f) * fh;
        const float sx1 = (nd1.x * 0.5f + 0.5f) * fw, sy1 = (0.5f - nd1.y * 0.5f) * fh;
        const float sx2 = (nd2.x * 0.5f + 0.5f) * fw, sy2 = (0.5f - nd2.y * 0.5f) * fh;

        const float area = detail::edgeF(sx0, sy0, sx1, sy1, sx2, sy2);
        if (std::fabs(area) < 1e-7f) {
            continue; // degenerate / zero-area after projection
        }

        // Per-vertex world attributes for smooth interpolation.
        const math::vec3 p0(a.px, a.py, a.pz), p1(b.px, b.py, b.pz), p2(c.px, c.py, c.pz);
        const math::vec3 n0(a.nx, a.ny, a.nz), n1(b.nx, b.ny, b.nz), n2(c.nx, c.ny, c.nz);

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
                const float e0 = detail::edgeF(sx1, sy1, sx2, sy2, cx, cy);
                const float e1 = detail::edgeF(sx2, sy2, sx0, sy0, cx, cy);
                const float e2 = detail::edgeF(sx0, sy0, sx1, sy1, cx, cy);
                const bool inside =
                    (e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f);
                if (!inside) {
                    continue;
                }
                // Barycentric weights (convex: each in [0,1], summing to 1) via the signed area.
                const float b0 = e0 / area, b1 = e1 / area, b2 = e2 / area;
                const float z = b0 * nd0.z + b1 * nd1.z + b2 * nd2.z;
                if (z < 0.0f || z > 1.0f) {
                    continue; // outside the clip depth range
                }
                const std::size_t idx =
                    static_cast<std::size_t>(py) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(px);
                if (z >= depth[idx]) {
                    continue;
                }
                depth[idx] = z;

                // Interpolate the normal, colour and world position across the triangle (smooth shading).
                const math::vec3 N = detail::normalize3(math::vec3(
                    b0 * n0.x + b1 * n1.x + b2 * n2.x, b0 * n0.y + b1 * n1.y + b2 * n2.y,
                    b0 * n0.z + b1 * n1.z + b2 * n2.z));
                const math::vec3 P(b0 * p0.x + b1 * p1.x + b2 * p2.x,
                                   b0 * p0.y + b1 * p1.y + b2 * p2.y,
                                   b0 * p0.z + b1 * p1.z + b2 * p2.z);
                const float cr = b0 * a.r + b1 * b.r + b2 * c.r;
                const float cg = b0 * a.g + b1 * b.g + b2 * c.g;
                const float cb2 = b0 * a.b + b1 * b.b + b2 * c.b;

                const float ndl = std::max(0.0f, detail::dot3(N, toLight));
                const float diffuse = kAmbient + (1.0f - kAmbient) * ndl;
                float spec = 0.0f;
                if (ndl > 0.0f) {
                    const math::vec3 V = detail::normalize3(math::vec3(eye.x - P.x, eye.y - P.y, eye.z - P.z));
                    const math::vec3 H = detail::normalize3(math::vec3(toLight.x + V.x, toLight.y + V.y, toLight.z + V.z));
                    spec = std::pow(std::max(0.0f, detail::dot3(N, H)), kShininess) * kSpecular;
                }
                img.setPixel(px, py, Color{detail::clamp01(cr * diffuse + spec),
                                           detail::clamp01(cg * diffuse + spec),
                                           detail::clamp01(cb2 * diffuse + spec), 1.0f});
            }
        }
    }
    return img;
}

} // namespace maz::render
