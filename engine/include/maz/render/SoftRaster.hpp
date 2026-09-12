#pragma once

#include "maz/math/Math.hpp"
#include "maz/render/Image.hpp"
#include "maz/render/Shapes.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

// maz::render SOFTWARE RASTERISER — triangles into a picture, with no GPU anywhere near it.
//
// The engine draws 3D through Vulkan, which needs a device, a display, a shader compiler and a driver.
// That is the right thing for a game, which is played on a machine, and the wrong thing for a FILM,
// which is rendered: a film wants to come out the same on a laptop, on a build server, and in a
// headless container, frame by frame into an image file, with no window ever opening. So this is the
// other path — the same meshes, the same matrices, the same Vulkan clip conventions, rasterised on the
// CPU into a render::Image.
//
// It is deliberately small: a depth buffer, near-plane clipping, backface culling, perspective-correct
// interpolation, and one key light with an ambient and a fill. No textures, no shadows, no
// transparency sorting. That is enough for a picture with real depth in it, and everything it does not
// do, it does not do in a way you can see rather than in a way that corrupts the frame.
//
// Conventions are the engine's own, so a scene composed for the Vulkan renderer renders here unchanged:
// right-handed world, `math::perspective` (clip-space y down, depth 0..1), screen origin top-left. The
// projection maths matches render::Camera3D::worldToScreen exactly, on purpose — a point the camera
// says is at a pixel is the pixel this fills.
namespace maz::render {

// How a mesh takes light. One key, one ambient, one fill — the three-light setup, which is what the
// film's 2D renderer already thinks in, and enough that a curved surface reads as curved.
struct Surface {
    // The direction the key light TRAVELS (so a key from the upper front-left of a subject facing +Z
    // points down, right and back). Normalised on use.
    math::vec3 keyDirection{-0.45f, -0.75f, -0.48f};
    Color key{1.0f, 0.97f, 0.92f, 1.0f}; // the key's own colour, multiplied into the surface colour
    float ambient = 0.22f;               // floor brightness: what a surface gets facing nowhere
    float fill = 0.16f;                  // a soft bounce from directly opposite the key
    float emissive = 0.0f;               // 0..1 of the vertex colour that ignores light entirely
    float alpha = 1.0f;                  // < 1 blends and stops writing depth
};

class SoftRaster {
  public:
    SoftRaster(int width, int height)
        : m_w(width < 1 ? 1 : width), m_h(height < 1 ? 1 : height),
          m_depth(static_cast<std::size_t>((width < 1 ? 1 : width)) *
                      static_cast<std::size_t>((height < 1 ? 1 : height)),
                  1.0f) {}

    int width() const { return m_w; }
    int height() const { return m_h; }

    // Reset every pixel to the far plane. Call once per frame, before the first draw.
    void clearDepth(float far_ = 1.0f) {
        for (float& d : m_depth) {
            d = far_;
        }
        m_tris = 0;
    }

    // The depth standing at a pixel: 0 at the near plane, 1 at the far. Out of bounds reads as far.
    float depthAt(int x, int y) const {
        if (x < 0 || y < 0 || x >= m_w || y >= m_h) {
            return 1.0f;
        }
        return m_depth[static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) +
                       static_cast<std::size_t>(x)];
    }

    // Triangles that survived clipping and culling in the draws since the last clearDepth.
    std::size_t trianglesDrawn() const { return m_tris; }

    // Draw one mesh. `model` places it in the world; `viewProj` is the camera.
    void draw(Image& target, const shapes::MeshData& mesh, const math::mat4& model,
              const math::mat4& viewProj, const Surface& surf) {
        if (mesh.indices.size() < 3 || target.width() != m_w || target.height() != m_h) {
            return;
        }
        const math::mat4 mvp = viewProj * model;
        // Normals need the inverse-transpose, or a non-uniform scale (a squashed head, a stretched
        // limb) lights as though it were still a sphere.
        const math::mat3 normalM = math::mat3(glm::transpose(glm::inverse(model)));

        const math::vec3 key = math::normalize(surf.keyDirection);
        const std::size_t n = mesh.indices.size() / 3u * 3u;
        for (std::size_t i = 0; i < n; i += 3) {
            Vert tri[3];
            bool ok = true;
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t vi = mesh.indices[i + static_cast<std::size_t>(k)];
                if (vi >= mesh.vertices.size()) {
                    ok = false;
                    break;
                }
                const MeshVertex& v = mesh.vertices[vi];
                tri[k].clip = mvp * math::vec4(v.px, v.py, v.pz, 1.0f);
                tri[k].normal = normalM * math::vec3(v.nx, v.ny, v.nz);
                tri[k].color = math::vec3(v.r, v.g, v.b);
            }
            if (!ok) {
                continue;
            }
            clipAndFill(target, tri, key, surf);
        }
    }

  private:
    // A vertex on its way down the pipe: still in clip space, carrying what the shade needs.
    struct Vert {
        math::vec4 clip{0.0f};
        math::vec3 normal{0.0f};
        math::vec3 color{0.0f};
    };

    static Vert mix(const Vert& a, const Vert& b, float t) {
        Vert o;
        o.clip = a.clip + (b.clip - a.clip) * t;
        o.normal = a.normal + (b.normal - a.normal) * t;
        o.color = a.color + (b.color - a.color) * t;
        return o;
    }

    // Clip against the near plane (clip-space z >= 0, which is where Vulkan's 0..1 depth range starts)
    // and fill whatever is left. Without this, a triangle with a corner behind the eye divides by a w
    // through zero and paints the whole frame; it is the single most visible way a rasteriser breaks.
    void clipAndFill(Image& target, const Vert (&tri)[3], const math::vec3& key, const Surface& surf) {
        Vert poly[4];
        int count = 0;
        for (int k = 0; k < 3; ++k) {
            const Vert& cur = tri[k];
            const Vert& next = tri[(k + 1) % 3];
            const bool curIn = cur.clip.z >= 0.0f;
            const bool nextIn = next.clip.z >= 0.0f;
            if (curIn) {
                poly[count++] = cur;
            }
            if (curIn != nextIn) {
                const float d = cur.clip.z - next.clip.z;
                if (std::fabs(d) > 1e-12f && count < 4) {
                    poly[count++] = mix(cur, next, cur.clip.z / d);
                }
            }
        }
        if (count < 3) {
            return;
        }
        // A quad at most: the near plane cuts a triangle into a triangle or a quad, never more.
        fillTriangle(target, poly[0], poly[1], poly[2], key, surf);
        if (count == 4) {
            fillTriangle(target, poly[0], poly[2], poly[3], key, surf);
        }
    }

    void fillTriangle(Image& target, const Vert& a, const Vert& b, const Vert& c,
                      const math::vec3& key, const Surface& surf) {
        if (a.clip.w <= 0.0f || b.clip.w <= 0.0f || c.clip.w <= 0.0f) {
            return;
        }
        const float fw = static_cast<float>(m_w);
        const float fh = static_cast<float>(m_h);
        // Screen space, exactly as Camera3D::worldToScreen computes it.
        const math::vec3 na = math::vec3(a.clip) / a.clip.w;
        const math::vec3 nb = math::vec3(b.clip) / b.clip.w;
        const math::vec3 nc = math::vec3(c.clip) / c.clip.w;
        const float x0 = (na.x * 0.5f + 0.5f) * fw, y0 = (na.y * 0.5f + 0.5f) * fh;
        const float x1 = (nb.x * 0.5f + 0.5f) * fw, y1 = (nb.y * 0.5f + 0.5f) * fh;
        const float x2 = (nc.x * 0.5f + 0.5f) * fw, y2 = (nc.y * 0.5f + 0.5f) * fh;

        // The signed area in SCREEN space, which is a mirror of world space in y (clip y points down).
        // A front face — wound counter-clockwise in the world, seen from outside — comes out NEGATIVE
        // here. Culling on that one sign is what keeps the inside of every object out of the picture.
        const double area = static_cast<double>(x1 - x0) * static_cast<double>(y2 - y0) -
                            static_cast<double>(x2 - x0) * static_cast<double>(y1 - y0);
        if (area >= 0.0) {
            return; // back-facing, or edge-on and infinitely thin
        }
        ++m_tris;

        int minX = static_cast<int>(std::floor(std::min({x0, x1, x2})));
        int maxX = static_cast<int>(std::ceil(std::max({x0, x1, x2})));
        int minY = static_cast<int>(std::floor(std::min({y0, y1, y2})));
        int maxY = static_cast<int>(std::ceil(std::max({y0, y1, y2})));
        minX = std::max(minX, 0);
        minY = std::max(minY, 0);
        maxX = std::min(maxX, m_w - 1);
        maxY = std::min(maxY, m_h - 1);
        if (minX > maxX || minY > maxY) {
            return;
        }

        // 1/w per corner, for perspective-correct colour and normals. Screen-space z needs no
        // correction: z/w is already linear across the screen, which is the whole reason a depth
        // buffer stores it.
        const double iwa = 1.0 / static_cast<double>(a.clip.w);
        const double iwb = 1.0 / static_cast<double>(b.clip.w);
        const double iwc = 1.0 / static_cast<double>(c.clip.w);

        for (int py = minY; py <= maxY; ++py) {
            const double sy = static_cast<double>(py) + 0.5;
            for (int px = minX; px <= maxX; ++px) {
                const double sx = static_cast<double>(px) + 0.5;
                // Edge functions. Every edge is tested inclusively, so two triangles sharing an edge
                // BOTH claim the pixels on it: the seam is covered twice rather than not at all, and
                // the depth test throws the second one away. A gap on a shared edge would put a line
                // of background through every flat surface in the film.
                const double w0 = edge(x1, y1, x2, y2, sx, sy);
                const double w1 = edge(x2, y2, x0, y0, sx, sy);
                const double w2 = edge(x0, y0, x1, y1, sx, sy);
                if (!(w0 <= 0.0 && w1 <= 0.0 && w2 <= 0.0)) {
                    continue;
                }
                const double l0 = w0 / area;
                const double l1 = w1 / area;
                const double l2 = w2 / area;

                const float z = static_cast<float>(l0 * static_cast<double>(na.z) +
                                                   l1 * static_cast<double>(nb.z) +
                                                   l2 * static_cast<double>(nc.z));
                if (z < 0.0f || z > 1.0f) {
                    continue;
                }
                const std::size_t di = static_cast<std::size_t>(py) * static_cast<std::size_t>(m_w) +
                                       static_cast<std::size_t>(px);
                if (z >= m_depth[di]) {
                    continue;
                }

                const double iw = l0 * iwa + l1 * iwb + l2 * iwc;
                if (iw <= 0.0) {
                    continue;
                }
                const double pa = l0 * iwa / iw;
                const double pb = l1 * iwb / iw;
                const double pc = l2 * iwc / iw;
                const math::vec3 nrm = a.normal * static_cast<float>(pa) +
                                       b.normal * static_cast<float>(pb) +
                                       c.normal * static_cast<float>(pc);
                const math::vec3 col = a.color * static_cast<float>(pa) +
                                       b.color * static_cast<float>(pb) +
                                       c.color * static_cast<float>(pc);

                const Color lit = shade(nrm, col, key, surf);
                if (surf.alpha >= 1.0f) {
                    target.setPixel(px, py, lit);
                    m_depth[di] = z;
                } else {
                    const Color dst = target.getPixel(px, py);
                    const float t = surf.alpha < 0.0f ? 0.0f : surf.alpha;
                    target.setPixel(px, py,
                                    Color{lit.r * t + dst.r * (1.0f - t), lit.g * t + dst.g * (1.0f - t),
                                          lit.b * t + dst.b * (1.0f - t), 1.0f});
                }
            }
        }
    }

    static double edge(float ax, float ay, float bx, float by, double px, double py) {
        return (static_cast<double>(bx) - static_cast<double>(ax)) * (py - static_cast<double>(ay)) -
               (static_cast<double>(by) - static_cast<double>(ay)) * (px - static_cast<double>(ax));
    }

    // Key + fill + ambient. The fill comes from directly opposite the key at a fraction of its
    // strength, which is what stops a surface turned away from the light from going to pure black —
    // on a face, black reads as a hole rather than as shadow.
    static Color shade(const math::vec3& normal, const math::vec3& albedo, const math::vec3& key,
                       const Surface& surf) {
        const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        const math::vec3 nrm = len > 1e-8f ? normal / len : math::vec3(0.0f, 0.0f, 1.0f);
        const float toKey = -(nrm.x * key.x + nrm.y * key.y + nrm.z * key.z);
        const float kLit = toKey > 0.0f ? toKey : 0.0f;
        const float fLit = toKey < 0.0f ? -toKey : 0.0f;
        const float amount = surf.ambient + kLit + fLit * surf.fill;
        Color out;
        out.r = clamp01(albedo.x * (amount * surf.key.r + surf.emissive));
        out.g = clamp01(albedo.y * (amount * surf.key.g + surf.emissive));
        out.b = clamp01(albedo.z * (amount * surf.key.b + surf.emissive));
        out.a = 1.0f;
        return out;
    }

    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    int m_w = 1;
    int m_h = 1;
    std::vector<float> m_depth;
    std::size_t m_tris = 0;
};

} // namespace maz::render
