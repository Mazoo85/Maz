#include "maz/assets/Primitives.hpp"

#include <cmath>

namespace maz::assets {

namespace {

constexpr float kPi = 3.14159265358979323846f;

// Append one vertex.
void pushVertex(Mesh& m, float px, float py, float pz, float nx, float ny, float nz, float u,
                float v) {
    Vertex vert;
    vert.position[0] = px;
    vert.position[1] = py;
    vert.position[2] = pz;
    vert.normal[0] = nx;
    vert.normal[1] = ny;
    vert.normal[2] = nz;
    vert.uv[0] = u;
    vert.uv[1] = v;
    m.vertices.push_back(vert);
}

// Append a quad as two triangles from four corner positions (a,b,c,d) sharing one normal. Corners
// are in CCW order seen from the front; winding is not load-bearing today (the pipeline culls no
// faces) but is kept consistent for when back-face culling lands.
void pushQuad(Mesh& m, float ax, float ay, float az, float bx, float by, float bz, float cx,
              float cy, float cz, float dx, float dy, float dz, float nx, float ny, float nz) {
    const auto base = static_cast<uint32_t>(m.vertices.size());
    pushVertex(m, ax, ay, az, nx, ny, nz, 0.0f, 0.0f);
    pushVertex(m, bx, by, bz, nx, ny, nz, 1.0f, 0.0f);
    pushVertex(m, cx, cy, cz, nx, ny, nz, 1.0f, 1.0f);
    pushVertex(m, dx, dy, dz, nx, ny, nz, 0.0f, 1.0f);
    const uint32_t quad[6] = {base, base + 1u, base + 2u, base, base + 2u, base + 3u};
    for (uint32_t idx : quad) {
        m.indices.push_back(idx);
    }
}

} // namespace

Mesh makeBox(float sx, float sy, float sz) {
    Mesh m;
    m.name = "box";
    const float hx = sx * 0.5f;
    const float hy = sy * 0.5f;
    const float hz = sz * 0.5f;

    // +X
    pushQuad(m, hx, -hy, -hz, hx, -hy, hz, hx, hy, hz, hx, hy, -hz, 1.0f, 0.0f, 0.0f);
    // -X
    pushQuad(m, -hx, -hy, hz, -hx, -hy, -hz, -hx, hy, -hz, -hx, hy, hz, -1.0f, 0.0f, 0.0f);
    // +Y
    pushQuad(m, -hx, hy, -hz, hx, hy, -hz, hx, hy, hz, -hx, hy, hz, 0.0f, 1.0f, 0.0f);
    // -Y
    pushQuad(m, -hx, -hy, hz, hx, -hy, hz, hx, -hy, -hz, -hx, -hy, -hz, 0.0f, -1.0f, 0.0f);
    // +Z
    pushQuad(m, -hx, -hy, hz, hx, -hy, hz, hx, hy, hz, -hx, hy, hz, 0.0f, 0.0f, 1.0f);
    // -Z
    pushQuad(m, hx, -hy, -hz, -hx, -hy, -hz, -hx, hy, -hz, hx, hy, -hz, 0.0f, 0.0f, -1.0f);
    return m;
}

Mesh makeSphere(float radius, int segments, int rings) {
    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;

    Mesh m;
    m.name = "sphere";
    const uint32_t stride = static_cast<uint32_t>(segments) + 1u;

    for (int ring = 0; ring <= rings; ++ring) {
        const float vFrac = static_cast<float>(ring) / static_cast<float>(rings);
        const float theta = vFrac * kPi; // 0 at the north pole, pi at the south
        const float sinT = std::sin(theta);
        const float cosT = std::cos(theta);
        for (int seg = 0; seg <= segments; ++seg) {
            const float uFrac = static_cast<float>(seg) / static_cast<float>(segments);
            const float phi = uFrac * 2.0f * kPi;
            const float nx = sinT * std::cos(phi);
            const float ny = cosT;
            const float nz = sinT * std::sin(phi);
            pushVertex(m, nx * radius, ny * radius, nz * radius, nx, ny, nz, uFrac, vFrac);
        }
    }

    for (uint32_t ring = 0; ring < static_cast<uint32_t>(rings); ++ring) {
        for (uint32_t seg = 0; seg < static_cast<uint32_t>(segments); ++seg) {
            const uint32_t a = ring * stride + seg;
            const uint32_t b = a + stride;
            const uint32_t quad[6] = {a, b, a + 1u, a + 1u, b, b + 1u};
            for (uint32_t idx : quad) {
                m.indices.push_back(idx);
            }
        }
    }
    return m;
}

Mesh makeCylinder(float radius, float height, int segments) {
    if (segments < 3) segments = 3;

    Mesh m;
    m.name = "cylinder";
    const float hy = height * 0.5f;
    const uint32_t segU = static_cast<uint32_t>(segments);

    // Side: a ring of quads. The seam column (seg == segments) is duplicated so side UVs run 0..1.
    const auto sideBase = static_cast<uint32_t>(m.vertices.size());
    for (int seg = 0; seg <= segments; ++seg) {
        const float uFrac = static_cast<float>(seg) / static_cast<float>(segments);
        const float phi = uFrac * 2.0f * kPi;
        const float cx = std::cos(phi);
        const float cz = std::sin(phi);
        pushVertex(m, cx * radius, -hy, cz * radius, cx, 0.0f, cz, uFrac, 0.0f);
        pushVertex(m, cx * radius, hy, cz * radius, cx, 0.0f, cz, uFrac, 1.0f);
    }
    for (uint32_t seg = 0; seg < segU; ++seg) {
        const uint32_t a = sideBase + seg * 2u; // bottom of this column
        const uint32_t b = a + 1u;              // top of this column
        const uint32_t c = a + 2u;              // bottom of the next column
        const uint32_t d = a + 3u;              // top of the next column
        const uint32_t quad[6] = {a, c, b, b, c, d};
        for (uint32_t idx : quad) {
            m.indices.push_back(idx);
        }
    }

    // Caps: a central vertex plus a triangle fan, one for +Y then -Y.
    for (int side = 0; side < 2; ++side) {
        const float y = side == 0 ? hy : -hy;
        const float ny = side == 0 ? 1.0f : -1.0f;
        const auto center = static_cast<uint32_t>(m.vertices.size());
        pushVertex(m, 0.0f, y, 0.0f, 0.0f, ny, 0.0f, 0.5f, 0.5f);
        for (int seg = 0; seg <= segments; ++seg) {
            const float phi = static_cast<float>(seg) / static_cast<float>(segments) * 2.0f * kPi;
            const float cx = std::cos(phi);
            const float cz = std::sin(phi);
            pushVertex(m, cx * radius, y, cz * radius, 0.0f, ny, 0.0f, cx * 0.5f + 0.5f,
                       cz * 0.5f + 0.5f);
        }
        for (uint32_t seg = 0; seg < segU; ++seg) {
            const uint32_t rim = center + 1u + seg;
            // Wind the two caps opposite ways so both face outward.
            if (side == 0) {
                const uint32_t tri[3] = {center, rim, rim + 1u};
                for (uint32_t idx : tri) {
                    m.indices.push_back(idx);
                }
            } else {
                const uint32_t tri[3] = {center, rim + 1u, rim};
                for (uint32_t idx : tri) {
                    m.indices.push_back(idx);
                }
            }
        }
    }
    return m;
}

Mesh makePlane(float width, float depth) {
    Mesh m;
    m.name = "plane";
    const float hx = width * 0.5f;
    const float hz = depth * 0.5f;
    pushQuad(m, -hx, 0.0f, hz, hx, 0.0f, hz, hx, 0.0f, -hz, -hx, 0.0f, -hz, 0.0f, 1.0f, 0.0f);
    return m;
}

Mesh makePrimitive(PrimitiveKind kind, const PrimitiveParams& params) {
    switch (kind) {
    case PrimitiveKind::Box:
        return makeBox(params.size[0], params.size[1], params.size[2]);
    case PrimitiveKind::Sphere:
        return makeSphere(params.radius, params.segments, params.rings);
    case PrimitiveKind::Cylinder:
        return makeCylinder(params.radius, params.height, params.segments);
    case PrimitiveKind::Plane:
        return makePlane(params.size[0], params.size[2]);
    }
    return Mesh{};
}

} // namespace maz::assets
