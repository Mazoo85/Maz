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
// Shading is SMOOTH (Phong): vertex normals and colours are interpolated across each triangle, lit by a
// small set of directional lights (a key/fill/rim rig, via threePointRig()) plus a coloured ambient
// term, with a Blinn-Phong specular highlight per light. Edges are anti-aliased by supersampling: the
// frame is drawn at `ssaa`x resolution and box-downsampled. Still a PREVIEW, not the PBR viewport (no
// textures, no shadows, screen-space affine interpolation). Header-only, pure CPU, unit-tested headlessly.
namespace maz::render {

// A directional light: `dir` is the direction the light TRAVELS (from the lamp into the scene).
struct DirLight {
    math::vec3 dir{0.0f, -1.0f, 0.0f};
    math::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity{1.0f};
};

// A preview lighting rig: some directional lights + a coloured ambient fill.
struct PreviewLighting {
    std::vector<DirLight> lights;
    math::vec3 ambient{0.16f, 0.17f, 0.20f};
};

// A pleasing default studio rig: a warm key, a cool fill that opens the shadows, and a bright rim that
// peels the subject off the background.
inline PreviewLighting threePointRig() {
    PreviewLighting r;
    r.lights.push_back({math::vec3(-0.4f, -0.8f, -0.5f), math::vec3(1.0f, 0.96f, 0.88f), 1.0f}); // key
    r.lights.push_back({math::vec3(0.7f, -0.2f, -0.5f), math::vec3(0.55f, 0.68f, 0.85f), 0.35f}); // fill
    r.lights.push_back({math::vec3(0.1f, -0.25f, 0.95f), math::vec3(0.85f, 0.9f, 1.0f), 0.55f}); // rim
    r.ambient = math::vec3(0.16f, 0.17f, 0.20f);
    return r;
}

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

// Core single-sample rasterizer: draws `mesh` into a fresh width x height Image. Multi-light Phong.
inline Image renderCore(const shapes::MeshData& mesh, const math::mat4& viewProj, const math::vec3& eye,
                        const PreviewLighting& lighting, int width, int height, const Color& background) {
    Image img(width, height, background);
    if (width <= 0 || height <= 0 || mesh.indices.size() < 3) {
        return img;
    }
    std::vector<float> depth(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 2.0f);
    const float fw = static_cast<float>(width), fh = static_cast<float>(height);
    constexpr float kShininess = 28.0f, kSpecular = 0.4f;

    // Pre-normalize the "toward the light" directions once.
    struct LightN { math::vec3 toLight; math::vec3 color; float intensity; };
    std::vector<LightN> lights;
    lights.reserve(lighting.lights.size());
    for (const DirLight& l : lighting.lights) {
        const math::vec3 t = normalize3(math::vec3(-l.dir.x, -l.dir.y, -l.dir.z));
        lights.push_back({t, l.color, l.intensity});
    }

    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const MeshVertex& a = mesh.vertices[mesh.indices[i]];
        const MeshVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const MeshVertex& c = mesh.vertices[mesh.indices[i + 2]];

        const math::vec4 ca = viewProj * math::vec4(a.px, a.py, a.pz, 1.0f);
        const math::vec4 cb = viewProj * math::vec4(b.px, b.py, b.pz, 1.0f);
        const math::vec4 cc = viewProj * math::vec4(c.px, c.py, c.pz, 1.0f);
        if (ca.w <= 1e-6f || cb.w <= 1e-6f || cc.w <= 1e-6f) {
            continue; // near-plane cull
        }
        const math::vec3 nd0(ca.x / ca.w, ca.y / ca.w, ca.z / ca.w);
        const math::vec3 nd1(cb.x / cb.w, cb.y / cb.w, cb.z / cb.w);
        const math::vec3 nd2(cc.x / cc.w, cc.y / cc.w, cc.z / cc.w);
        const float sx0 = (nd0.x * 0.5f + 0.5f) * fw, sy0 = (0.5f - nd0.y * 0.5f) * fh;
        const float sx1 = (nd1.x * 0.5f + 0.5f) * fw, sy1 = (0.5f - nd1.y * 0.5f) * fh;
        const float sx2 = (nd2.x * 0.5f + 0.5f) * fw, sy2 = (0.5f - nd2.y * 0.5f) * fh;
        const float area = edgeF(sx0, sy0, sx1, sy1, sx2, sy2);
        if (std::fabs(area) < 1e-7f) {
            continue;
        }
        const math::vec3 p0(a.px, a.py, a.pz), p1(b.px, b.py, b.pz), p2(c.px, c.py, c.pz);
        const math::vec3 n0(a.nx, a.ny, a.nz), n1(b.nx, b.ny, b.nz), n2(c.nx, c.ny, c.nz);

        int minX = std::max(0, static_cast<int>(std::floor(std::min({sx0, sx1, sx2}))));
        int maxX = std::min(width - 1, static_cast<int>(std::ceil(std::max({sx0, sx1, sx2}))));
        int minY = std::max(0, static_cast<int>(std::floor(std::min({sy0, sy1, sy2}))));
        int maxY = std::min(height - 1, static_cast<int>(std::ceil(std::max({sy0, sy1, sy2}))));

        for (int py = minY; py <= maxY; ++py) {
            for (int px = minX; px <= maxX; ++px) {
                const float cx = static_cast<float>(px) + 0.5f, cy = static_cast<float>(py) + 0.5f;
                const float e0 = edgeF(sx1, sy1, sx2, sy2, cx, cy);
                const float e1 = edgeF(sx2, sy2, sx0, sy0, cx, cy);
                const float e2 = edgeF(sx0, sy0, sx1, sy1, cx, cy);
                if (!((e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f))) {
                    continue;
                }
                const float b0 = e0 / area, b1 = e1 / area, b2 = e2 / area;
                const float z = b0 * nd0.z + b1 * nd1.z + b2 * nd2.z;
                if (z < 0.0f || z > 1.0f) {
                    continue;
                }
                const std::size_t idx = static_cast<std::size_t>(py) * static_cast<std::size_t>(width) +
                                        static_cast<std::size_t>(px);
                if (z >= depth[idx]) {
                    continue;
                }
                depth[idx] = z;

                const math::vec3 N = normalize3(math::vec3(b0 * n0.x + b1 * n1.x + b2 * n2.x,
                                                           b0 * n0.y + b1 * n1.y + b2 * n2.y,
                                                           b0 * n0.z + b1 * n1.z + b2 * n2.z));
                const math::vec3 P(b0 * p0.x + b1 * p1.x + b2 * p2.x, b0 * p0.y + b1 * p1.y + b2 * p2.y,
                                   b0 * p0.z + b1 * p1.z + b2 * p2.z);
                const math::vec3 V = normalize3(math::vec3(eye.x - P.x, eye.y - P.y, eye.z - P.z));
                const float cr = b0 * a.r + b1 * b.r + b2 * c.r;
                const float cg = b0 * a.g + b1 * b.g + b2 * c.g;
                const float cbl = b0 * a.b + b1 * b.b + b2 * c.b;

                math::vec3 diff = lighting.ambient; // coloured ambient
                math::vec3 spec(0.0f, 0.0f, 0.0f);
                for (const LightN& l : lights) {
                    const float ndl = std::max(0.0f, dot3(N, l.toLight));
                    if (ndl <= 0.0f) {
                        continue;
                    }
                    const float w = l.intensity * ndl;
                    diff.x += l.color.x * w; diff.y += l.color.y * w; diff.z += l.color.z * w;
                    const math::vec3 H = normalize3(math::vec3(l.toLight.x + V.x, l.toLight.y + V.y, l.toLight.z + V.z));
                    const float s = std::pow(std::max(0.0f, dot3(N, H)), kShininess) * kSpecular * l.intensity;
                    spec.x += l.color.x * s; spec.y += l.color.y * s; spec.z += l.color.z * s;
                }
                img.setPixel(px, py, Color{clamp01(cr * diff.x + spec.x), clamp01(cg * diff.y + spec.y),
                                           clamp01(cbl * diff.z + spec.z), 1.0f});
            }
        }
    }
    return img;
}

} // namespace detail

// Render `mesh` into a `width` x `height` Image. `viewProj` (clip = viewProj * worldPos, depth 0..1
// RH/ZO) projects it; `eye` is the world camera position (for specular); `lighting` is the rig (see
// threePointRig()); `background` fills empty pixels. `ssaa` (1..4) supersamples for anti-aliasing:
// the frame is drawn ssaa× larger and box-downsampled. Vertex positions/normals are world-space.
inline Image renderMeshPreview(const shapes::MeshData& mesh, const math::mat4& viewProj,
                               const math::vec3& eye, const PreviewLighting& lighting, int width,
                               int height, const Color& background, int ssaa = 1) {
    if (width <= 0 || height <= 0) {
        return Image(width, height, background);
    }
    const int s = ssaa < 1 ? 1 : (ssaa > 4 ? 4 : ssaa);
    if (s == 1) {
        return detail::renderCore(mesh, viewProj, eye, lighting, width, height, background);
    }
    const Image hi = detail::renderCore(mesh, viewProj, eye, lighting, width * s, height * s, background);
    Image out(width, height, background);
    const float inv = 1.0f / static_cast<float>(s * s);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
            for (int sy = 0; sy < s; ++sy) {
                for (int sx = 0; sx < s; ++sx) {
                    const Color c = hi.getPixel(x * s + sx, y * s + sy);
                    r += c.r; g += c.g; b += c.b; a += c.a;
                }
            }
            out.setPixel(x, y, Color{r * inv, g * inv, b * inv, a * inv});
        }
    }
    return out;
}

} // namespace maz::render
