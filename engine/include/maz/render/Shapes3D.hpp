#pragma once

#include "maz/math/Math.hpp"
#include "maz/render/Renderer.hpp"
#include "maz/render/Shapes.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

// Additional procedural mesh primitives — cylinder, cone, torus, capsule — beyond the box/sphere/plane
// in Shapes.hpp. Godot ships these as built-in meshes (CylinderMesh / CapsuleMesh / TorusMesh, and a cone
// as a cylinder with a zero top radius); Maz had only the three basics, so anything wanting a barrel, a
// spike, a ring, or a rounded-capsule collider proxy had to author a glTF. Each builder returns a
// shapes::MeshData (the same vertex/index buffers Renderer::createMesh consumes) with OUTWARD normals and
// UVs, so it drops straight into the existing lit-mesh path — no renderer change, and it leaves the
// original box/sphere/plane output byte-for-byte untouched. Header-only, pure math, unit-testable.

namespace maz::render::shapes {

namespace detail {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kHalfPi = 1.57079632679489661923f;
inline MeshVertex vtx(float px, float py, float pz, float nx, float ny, float nz, const Color& c,
                      float u, float v) {
    return MeshVertex{px, py, pz, nx, ny, nz, c.r, c.g, c.b, u, v};
}
} // namespace detail

// A cylinder of `radius` and full `height`, centered at the origin, axis along +Y. `sectors` is the
// number of segments around the circumference. Includes the curved side plus flat top/bottom caps.
inline MeshData makeCylinder(float radius, float height, int sectors, const Color& color) {
    if (sectors < 3) {
        sectors = 3;
    }
    MeshData m;
    const float hy = height * 0.5f;

    // Side: two rings (bottom, top) of sectors+1 vertices (seam duplicated for clean UVs).
    const auto sideBase = static_cast<uint32_t>(m.vertices.size());
    for (int i = 0; i <= sectors; ++i) {
        const float a = detail::kTwoPi * static_cast<float>(i) / static_cast<float>(sectors);
        const float cx = std::cos(a), cz = std::sin(a);
        const float u = static_cast<float>(i) / static_cast<float>(sectors);
        m.vertices.push_back(detail::vtx(cx * radius, -hy, cz * radius, cx, 0.0f, cz, color, u, 0.0f));
        m.vertices.push_back(detail::vtx(cx * radius, hy, cz * radius, cx, 0.0f, cz, color, u, 1.0f));
    }
    for (int i = 0; i < sectors; ++i) {
        const uint32_t b = sideBase + static_cast<uint32_t>(i) * 2u;
        // bottom=b, top=b+1, next bottom=b+2, next top=b+3
        m.indices.insert(m.indices.end(), {b, b + 2, b + 3, b, b + 3, b + 1});
    }

    // Caps: a center vertex + a fan. Top faces +Y, bottom faces -Y.
    auto addCap = [&](float y, float ny) {
        const auto center = static_cast<uint32_t>(m.vertices.size());
        m.vertices.push_back(detail::vtx(0.0f, y, 0.0f, 0.0f, ny, 0.0f, color, 0.5f, 0.5f));
        const auto ringStart = static_cast<uint32_t>(m.vertices.size());
        for (int i = 0; i <= sectors; ++i) {
            const float a = detail::kTwoPi * static_cast<float>(i) / static_cast<float>(sectors);
            const float cx = std::cos(a), cz = std::sin(a);
            m.vertices.push_back(detail::vtx(cx * radius, y, cz * radius, 0.0f, ny, 0.0f, color,
                                             cx * 0.5f + 0.5f, cz * 0.5f + 0.5f));
        }
        for (int i = 0; i < sectors; ++i) {
            const uint32_t r0 = ringStart + static_cast<uint32_t>(i);
            if (ny > 0.0f) {
                m.indices.insert(m.indices.end(), {center, r0, r0 + 1});
            } else {
                m.indices.insert(m.indices.end(), {center, r0 + 1, r0});
            }
        }
    };
    addCap(hy, 1.0f);
    addCap(-hy, -1.0f);
    return m;
}

// A cone of base `radius` and full `height`, centered at the origin, apex at +Y, base cap at -Y.
inline MeshData makeCone(float radius, float height, int sectors, const Color& color) {
    if (sectors < 3) {
        sectors = 3;
    }
    MeshData m;
    const float hy = height * 0.5f;
    // Analytic side normal at angle a: proportional to (height*cos, radius, height*sin).
    const float nyBase = radius; // pre-normalization y-component (times 1); x/z use height
    // Side: per-sector triangle (base i, base i+1, apex) with smooth radial+up normals.
    for (int i = 0; i < sectors; ++i) {
        const float a0 = detail::kTwoPi * static_cast<float>(i) / static_cast<float>(sectors);
        const float a1 = detail::kTwoPi * static_cast<float>(i + 1) / static_cast<float>(sectors);
        const float am = 0.5f * (a0 + a1);
        auto sideN = [&](float a) {
            float nx = height * std::cos(a), nz = height * std::sin(a), ny = nyBase;
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            return math::vec3(nx / len, ny / len, nz / len);
        };
        const math::vec3 n0 = sideN(a0), n1 = sideN(a1), na = sideN(am);
        const auto base = static_cast<uint32_t>(m.vertices.size());
        m.vertices.push_back(detail::vtx(std::cos(a0) * radius, -hy, std::sin(a0) * radius, n0.x, n0.y,
                                         n0.z, color, static_cast<float>(i) / static_cast<float>(sectors),
                                         0.0f));
        m.vertices.push_back(detail::vtx(std::cos(a1) * radius, -hy, std::sin(a1) * radius, n1.x, n1.y,
                                         n1.z, color,
                                         static_cast<float>(i + 1) / static_cast<float>(sectors), 0.0f));
        m.vertices.push_back(detail::vtx(0.0f, hy, 0.0f, na.x, na.y, na.z, color, 0.5f, 1.0f));
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2});
    }
    // Base cap facing -Y.
    const auto center = static_cast<uint32_t>(m.vertices.size());
    m.vertices.push_back(detail::vtx(0.0f, -hy, 0.0f, 0.0f, -1.0f, 0.0f, color, 0.5f, 0.5f));
    const auto ringStart = static_cast<uint32_t>(m.vertices.size());
    for (int i = 0; i <= sectors; ++i) {
        const float a = detail::kTwoPi * static_cast<float>(i) / static_cast<float>(sectors);
        const float cx = std::cos(a), cz = std::sin(a);
        m.vertices.push_back(detail::vtx(cx * radius, -hy, cz * radius, 0.0f, -1.0f, 0.0f, color,
                                         cx * 0.5f + 0.5f, cz * 0.5f + 0.5f));
    }
    for (int i = 0; i < sectors; ++i) {
        const uint32_t r0 = ringStart + static_cast<uint32_t>(i);
        m.indices.insert(m.indices.end(), {center, r0 + 1, r0});
    }
    return m;
}

// A torus (ring) around the +Y axis: `majorRadius` = center-of-tube ring radius, `minorRadius` = tube
// radius. `majorSegs` around the ring, `minorSegs` around the tube.
inline MeshData makeTorus(float majorRadius, float minorRadius, int majorSegs, int minorSegs,
                          const Color& color) {
    if (majorSegs < 3) {
        majorSegs = 3;
    }
    if (minorSegs < 3) {
        minorSegs = 3;
    }
    MeshData m;
    const int mu = majorSegs, nv = minorSegs;
    for (int i = 0; i <= mu; ++i) {
        const float U = detail::kTwoPi * static_cast<float>(i) / static_cast<float>(mu);
        const float cU = std::cos(U), sU = std::sin(U);
        for (int j = 0; j <= nv; ++j) {
            const float V = detail::kTwoPi * static_cast<float>(j) / static_cast<float>(nv);
            const float cV = std::cos(V), sV = std::sin(V);
            const float ringR = majorRadius + minorRadius * cV;
            const float px = ringR * cU, py = minorRadius * sV, pz = ringR * sU;
            const float nx = cV * cU, ny = sV, nz = cV * sU; // outward tube normal
            m.vertices.push_back(detail::vtx(px, py, pz, nx, ny, nz, color,
                                             static_cast<float>(i) / static_cast<float>(mu),
                                             static_cast<float>(j) / static_cast<float>(nv)));
        }
    }
    const auto stride = static_cast<uint32_t>(nv + 1);
    for (int i = 0; i < mu; ++i) {
        for (int j = 0; j < nv; ++j) {
            const uint32_t a = static_cast<uint32_t>(i) * stride + static_cast<uint32_t>(j);
            const uint32_t b = a + stride;
            m.indices.insert(m.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return m;
}

// A capsule: a cylinder of full `cylHeight` (the straight middle) capped by two hemispheres of `radius`,
// axis along +Y, centered at the origin. `sectors` around, `rings` = latitude bands per hemisphere.
// Total height = cylHeight + 2*radius.
inline MeshData makeCapsule(float radius, float cylHeight, int sectors, int rings, const Color& color) {
    if (sectors < 3) {
        sectors = 3;
    }
    if (rings < 1) {
        rings = 1;
    }
    MeshData m;
    const float hy = cylHeight * 0.5f;

    // Cylinder side.
    const auto sideBase = static_cast<uint32_t>(m.vertices.size());
    for (int i = 0; i <= sectors; ++i) {
        const float a = detail::kTwoPi * static_cast<float>(i) / static_cast<float>(sectors);
        const float cx = std::cos(a), cz = std::sin(a);
        const float u = static_cast<float>(i) / static_cast<float>(sectors);
        m.vertices.push_back(
            detail::vtx(cx * radius, -hy, cz * radius, cx, 0.0f, cz, color, u, 0.33f));
        m.vertices.push_back(detail::vtx(cx * radius, hy, cz * radius, cx, 0.0f, cz, color, u, 0.66f));
    }
    for (int i = 0; i < sectors; ++i) {
        const uint32_t b = sideBase + static_cast<uint32_t>(i) * 2u;
        m.indices.insert(m.indices.end(), {b, b + 2, b + 3, b, b + 3, b + 1});
    }

    // A hemisphere cap: `sign` +1 => top (centered at +hy), -1 => bottom (centered at -hy).
    auto addHemisphere = [&](float sign) {
        const float cy = sign * hy;
        const auto base = static_cast<uint32_t>(m.vertices.size());
        // Latitude 0 = equator, rings = pole. Build a (rings+1) x (sectors+1) grid.
        for (int r = 0; r <= rings; ++r) {
            const float phi = (static_cast<float>(r) / static_cast<float>(rings)) * detail::kHalfPi;
            const float sinP = std::sin(phi), cosP = std::cos(phi);
            for (int i = 0; i <= sectors; ++i) {
                const float a = detail::kTwoPi * static_cast<float>(i) / static_cast<float>(sectors);
                const float cx = std::cos(a) * cosP, cz = std::sin(a) * cosP;
                const float nx = cx, ny = sign * sinP, nz = cz;
                m.vertices.push_back(detail::vtx(nx * radius, cy + ny * radius, nz * radius, nx, ny, nz,
                                                 color, static_cast<float>(i) / static_cast<float>(sectors),
                                                 sign > 0.0f ? 0.66f + 0.34f * static_cast<float>(r) /
                                                                            static_cast<float>(rings)
                                                             : 0.33f - 0.33f * static_cast<float>(r) /
                                                                           static_cast<float>(rings)));
            }
        }
        const auto stride = static_cast<uint32_t>(sectors + 1);
        for (int r = 0; r < rings; ++r) {
            for (int i = 0; i < sectors; ++i) {
                const uint32_t a0 = base + static_cast<uint32_t>(r) * stride + static_cast<uint32_t>(i);
                const uint32_t a1 = a0 + stride;
                if (sign > 0.0f) {
                    m.indices.insert(m.indices.end(), {a0, a1, a0 + 1, a0 + 1, a1, a1 + 1});
                } else {
                    m.indices.insert(m.indices.end(), {a0, a0 + 1, a1, a0 + 1, a1 + 1, a1});
                }
            }
        }
    };
    addHemisphere(1.0f);
    addHemisphere(-1.0f);
    return m;
}

} // namespace maz::render::shapes
