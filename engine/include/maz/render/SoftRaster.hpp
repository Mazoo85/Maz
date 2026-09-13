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

// Which side of a triangle is thrown away. Back is the normal case; Front is what a shadow pass wants
// (see ShadowMap below); None is for geometry that genuinely has no inside.
enum class Cull { Back, Front, None };

// ------------------------------------------------------------------------------------ shadows
//
// A depth map rendered FROM THE LIGHT. Everything the light can see is lit; everything hidden behind
// something the light can see is in shadow. That is the whole idea, and it is the one thing that was
// missing that made every figure look as though it were floating: without a shadow there is nothing to
// say where a body meets the floor, and the eye reads "hovering" long before it reads "unlit".
//
// Two things make the difference between shadows and a mess of dark speckles:
//
// Two decisions, and both were arrived at by measuring rather than by reading.
//
// The first is to cast from BACK faces. If the surfaces facing the light are the ones
// that write the map, then every lit surface's own depth is the depth in the map, and half of it
// shadows itself — the stippled crawling pattern called shadow acne, which is the usual reason a
// shadow map ends up buried under a pile of bias constants. Casting from the far side of each object
// instead puts the whole thickness of the object between the receiver and the map, and the problem is
// gone rather than biased away.
//
// The second is a SLOPE-SCALED bias along the light. Back-face casting settles flat geometry
// completely, and a body is not flat: near the silhouette of an arm the far side the map recorded and
// the near side being lit are within a texel of each other, and the soft edge's nine samples straddle
// the two. Measured against a raycast — fire rays at a standing body from where the light is, and
// whatever each one hits first is by definition lit — twelve per cent of those points came back
// shadowed. Half a texel of bias, divided by how squarely the surface faces the light, takes it to one
// per cent; the textbook normal offset was tried first and made it WORSE, doubling the error, because
// at a silhouette the normal points across the light rather than away from it and the nudge lands on a
// neighbouring texel that is no better.
//
// The price is that casting needs CLOSED geometry: a single-sided plane has no far side, so it casts
// nothing. Everything in the film is closed (boxes, capsules, lofted bodies); a bare quad is not, and
// the test says so out loud rather than leaving it to be discovered.
class ShadowMap;

// How much of the key light this point is missing: 0 fully lit, 1 fully in shadow. Public because
// "is this point in shadow?" is a question worth being able to ask without rendering anything —
// a test asks it directly, and so could a game.
inline float shadowFactor(const ShadowMap& map, const math::vec3& where, const math::vec3& normal);

// How a mesh takes light. One key, one ambient, one fill — the three-light setup, which is what the
// film's 2D renderer already thinks in, and enough that a curved surface reads as curved.
struct Surface {
    // The direction the key light TRAVELS (so a key from the upper front-left of a subject facing +Z
    // points down, right and back). Normalised on use.
    math::vec3 keyDirection{-0.45f, -0.75f, -0.48f};
    Color key{1.0f, 0.97f, 0.92f, 1.0f}; // the key's own colour, multiplied into the surface colour
    // What the ambient and the fill are coloured by, which is NOT the key: the fill is light that has
    // bounced off the room and the sky, so it carries their colour, not the lamp's. Giving the ambient
    // the key's colour tints every surface in the frame the same hue and the picture comes out
    // monochrome — which is exactly what happened the first time, and is the difference between a
    // scene lit green and a scene printed on green stock.
    Color ambientTint{0.86f, 0.90f, 1.0f, 1.0f};
    float ambient = 0.22f;               // floor brightness: what a surface gets facing nowhere
    float fill = 0.16f;                  // a soft bounce from directly opposite the key
    float emissive = 0.0f;               // 0..1 of the vertex colour that ignores light entirely
    float alpha = 1.0f;                  // < 1 blends and stops writing depth

    // AERIAL PERSPECTIVE. Everything far away is paler and closer to the colour of the air, because
    // there is air in between — it is how the eye reads distance outdoors, and how a painter has
    // faked it since the fifteenth century. In a renderer it does three jobs at once: it gives a
    // frame depth, it separates a figure from the ground behind them, and it hides the edge of the
    // world, so a ground plane no longer has to stop somewhere visible.
    //
    // Measured in world distance ALONG THE VIEW AXIS, not in depth-buffer units, so the numbers are
    // metres and mean the same thing at every focal length. fogEnd <= fogStart turns it off.
    Color fog{0.5f, 0.55f, 0.62f, 1.0f};
    float fogStart = 0.0f;
    float fogEnd = 0.0f;
    float fogMax = 0.85f;                // how far toward the air colour the furthest thing goes

    // What the key light cannot see. Null means nothing casts.
    const ShadowMap* shadows = nullptr;
    float shadowStrength = 1.0f;         // 1 takes the whole key away; less leaves some in
    Cull cull = Cull::Back;
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
        if (target.width() != m_w || target.height() != m_h) {
            return;
        }
        render(&target, mesh, model, viewProj, surf);
    }

    // Depth only, with no picture and no shading: what a shadow map is made of. `cull` is Front for a
    // shadow pass, so it is the far side of each object that writes the depth.
    void drawDepth(const shapes::MeshData& mesh, const math::mat4& model, const math::mat4& viewProj,
                   Cull cull = Cull::Front) {
        Surface depthOnly;
        depthOnly.cull = cull;
        render(nullptr, mesh, model, viewProj, depthOnly);
    }

  private:
    void render(Image* target, const shapes::MeshData& mesh, const math::mat4& model,
                const math::mat4& viewProj, const Surface& surf) {
        if (mesh.indices.size() < 3) {
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
                const math::vec4 local(v.px, v.py, v.pz, 1.0f);
                tri[k].clip = mvp * local;
                tri[k].world = math::vec3(model * local);
                tri[k].normal = normalM * math::vec3(v.nx, v.ny, v.nz);
                tri[k].color = math::vec3(v.r, v.g, v.b);
            }
            if (!ok) {
                continue;
            }
            clipAndFill(target, tri, key, surf);
        }
    }

    // A vertex on its way down the pipe: still in clip space, carrying what the shade needs.
    struct Vert {
        math::vec4 clip{0.0f};
        math::vec3 world{0.0f};
        math::vec3 normal{0.0f};
        math::vec3 color{0.0f};
    };

    static Vert mix(const Vert& a, const Vert& b, float t) {
        Vert o;
        o.clip = a.clip + (b.clip - a.clip) * t;
        o.world = a.world + (b.world - a.world) * t;
        o.normal = a.normal + (b.normal - a.normal) * t;
        o.color = a.color + (b.color - a.color) * t;
        return o;
    }

    // Clip against the near plane (clip-space z >= 0, which is where Vulkan's 0..1 depth range starts)
    // and fill whatever is left. Without this, a triangle with a corner behind the eye divides by a w
    // through zero and paints the whole frame; it is the single most visible way a rasteriser breaks.
    void clipAndFill(Image* target, const Vert (&tri)[3], const math::vec3& key, const Surface& surf) {
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

    void fillTriangle(Image* target, const Vert& a, const Vert& b, const Vert& c,
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
        if (area == 0.0) {
            return; // edge-on and infinitely thin
        }
        if ((surf.cull == Cull::Back && area > 0.0) || (surf.cull == Cull::Front && area < 0.0)) {
            return;
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
                const bool inside = area < 0.0 ? (w0 <= 0.0 && w1 <= 0.0 && w2 <= 0.0)
                                               : (w0 >= 0.0 && w1 >= 0.0 && w2 >= 0.0);
                if (!inside) {
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

                if (target == nullptr) {
                    m_depth[di] = z;       // a depth pass has nothing else to do
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
                const math::vec3 here = a.world * static_cast<float>(pa) +
                                        b.world * static_cast<float>(pb) +
                                        c.world * static_cast<float>(pc);

                Color lit = shade(nrm, col, here, key, surf);
                if (surf.fogEnd > surf.fogStart) {
                    // 1/iw is the perspective-correct distance along the view axis at this pixel.
                    const float away = static_cast<float>(1.0 / iw);
                    float f = (away - surf.fogStart) / (surf.fogEnd - surf.fogStart);
                    f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
                    f *= surf.fogMax;
                    lit.r += (surf.fog.r - lit.r) * f;
                    lit.g += (surf.fog.g - lit.g) * f;
                    lit.b += (surf.fog.b - lit.b) * f;
                }
                if (surf.alpha >= 1.0f) {
                    target->setPixel(px, py, lit);
                    m_depth[di] = z;
                } else {
                    const Color dst = target->getPixel(px, py);
                    const float t = surf.alpha < 0.0f ? 0.0f : surf.alpha;
                    target->setPixel(px, py,
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
    static Color shade(const math::vec3& normal, const math::vec3& albedo, const math::vec3& where,
                       const math::vec3& key, const Surface& surf) {
        const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        const math::vec3 nrm = len > 1e-8f ? normal / len : math::vec3(0.0f, 0.0f, 1.0f);
        const float toKey = -(nrm.x * key.x + nrm.y * key.y + nrm.z * key.z);
        float kLit = toKey > 0.0f ? toKey : 0.0f;
        // Shadow takes away the KEY and nothing else. A shadow is an absence of the lamp, not an
        // absence of light: the ambient and the bounce still reach into it, which is why a real shadow
        // has colour in it and a subtracted one is a black hole.
        if (surf.shadows != nullptr && kLit > 0.0f) {
            kLit *= 1.0f - shadowFactor(*surf.shadows, where, nrm) * surf.shadowStrength;
        }
        const float fLit = toKey < 0.0f ? -toKey : 0.0f;
        const float soft = surf.ambient + fLit * surf.fill;
        Color out;
        out.r = clamp01(albedo.x * (kLit * surf.key.r + soft * surf.ambientTint.r + surf.emissive));
        out.g = clamp01(albedo.y * (kLit * surf.key.g + soft * surf.ambientTint.g + surf.emissive));
        out.b = clamp01(albedo.z * (kLit * surf.key.b + soft * surf.ambientTint.b + surf.emissive));
        out.a = 1.0f;
        return out;
    }

    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    int m_w = 1;
    int m_h = 1;
    std::vector<float> m_depth;
    std::size_t m_tris = 0;
};

// The depth the light can see, and the matrix it saw it through.
class ShadowMap {
  public:
    explicit ShadowMap(int size = 1024)
        : m_raster(size < 16 ? 16 : size, size < 16 ? 16 : size), m_size(size < 16 ? 16 : size) {}

    // Start a new map. `lightViewProj` should cover the whole of what can cast into frame; use
    // `directionalLight()` to build one.
    void begin(const math::mat4& lightViewProj) {
        m_vp = lightViewProj;
        // How much world one texel covers, read back out of the matrix rather than passed in, so a
        // caller cannot hand over a light and a texel size that disagree. For an orthographic
        // projection times a rigid view, the length of the matrix's first row is 1 / half-width.
        const float rowLen = std::sqrt(m_vp[0][0] * m_vp[0][0] + m_vp[1][0] * m_vp[1][0] +
                                       m_vp[2][0] * m_vp[2][0]);
        m_texel = rowLen > 1e-9f ? 2.0f / (rowLen * static_cast<float>(m_size)) : 0.01f;
        m_raster.clearDepth();
    }

    void add(const shapes::MeshData& mesh, const math::mat4& model = math::mat4(1.0f)) {
        m_raster.drawDepth(mesh, model, m_vp, Cull::Front);
    }

    int size() const { return m_size; }
    float worldPerTexel() const { return m_texel; }
    const math::mat4& viewProj() const { return m_vp; }
    float depthAt(int x, int y) const { return m_raster.depthAt(x, y); }

  private:
    SoftRaster m_raster;
    int m_size;
    float m_texel = 0.01f;
    math::mat4 m_vp{1.0f};
};

// A light's view of a box of world. `dir` is the direction the light TRAVELS; the box is the part of
// the world that may cast. Orthographic, because a key light in a film is the sun or a lamp far
// enough away to be one.
inline math::mat4 directionalLight(const math::vec3& boxMin, const math::vec3& boxMax,
                                   const math::vec3& dir) {
    const math::vec3 centre = (boxMin + boxMax) * 0.5f;
    const math::vec3 half = (boxMax - boxMin) * 0.5f;
    const float radius = std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z) + 0.01f;
    math::vec3 d = dir;
    const float dl = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    d = dl > 1e-6f ? d / dl : math::vec3(0.0f, -1.0f, 0.0f);
    // Stand the light off by the radius so the whole box is in front of it, and fit the frustum to a
    // SPHERE around the box rather than to the box: the box's own extent changes as the light turns,
    // and a frustum that changes size makes the shadows crawl.
    const math::vec3 eye = centre - d * (radius * 2.0f);
    const math::vec3 up =
        std::fabs(d.y) > 0.95f ? math::vec3(0.0f, 0.0f, 1.0f) : math::vec3(0.0f, 1.0f, 0.0f);
    const math::mat4 view = glm::lookAt(eye, centre, up);
    return math::orthographic(-radius, radius, -radius, radius, 0.05f, radius * 4.0f) * view;
}

// How much of the key this point is missing, 0 lit to 1 fully shadowed. Three by three, so the edge of
// a shadow is a soft edge rather than a staircase.
inline float shadowFactor(const ShadowMap& map, const math::vec3& where, const math::vec3& normal) {
    // Toward the light by half a texel, more where the surface stands steeply to it. `key` is not
    // known here, but the light's own forward axis is: it is the third row of the view-projection.
    const math::vec3 towardLight =
        -glm::normalize(math::vec3(map.viewProj()[0][2], map.viewProj()[1][2], map.viewProj()[2][2]));
    const float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    const math::vec3 n = len > 1e-8f ? normal / len : towardLight;
    const float face = std::fmax(math::dot(n, towardLight), 0.22f);
    const math::vec3 probe = where + towardLight * (map.worldPerTexel() * 0.75f / face);
    const math::vec4 clip = map.viewProj() * math::vec4(probe, 1.0f);
    if (clip.w <= 1e-6f) {
        return 0.0f;
    }
    const math::vec3 ndc = math::vec3(clip) / clip.w;
    if (ndc.z < 0.0f || ndc.z > 1.0f) {
        return 0.0f;                       // outside the light's range: nothing known, so lit
    }
    const float fx = (ndc.x * 0.5f + 0.5f) * static_cast<float>(map.size());
    const float fy = (ndc.y * 0.5f + 0.5f) * static_cast<float>(map.size());
    const int cx = static_cast<int>(fx);
    const int cy = static_cast<int>(fy);
    if (cx < 1 || cy < 1 || cx >= map.size() - 1 || cy >= map.size() - 1) {
        return 0.0f;                       // off the edge of the map: lit, never a hard black border
    }
    int blocked = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (map.depthAt(cx + dx, cy + dy) < ndc.z) {
                ++blocked;
            }
        }
    }
    return static_cast<float>(blocked) / 9.0f;
}

} // namespace maz::render
