#include "maz/render/Shapes.hpp"

#include <cmath>

namespace maz::render::shapes {

namespace {
constexpr float kPi = 3.14159265358979323846f;

MeshVertex vtx(float px, float py, float pz, float nx, float ny, float nz, const Color& c) {
    return MeshVertex{px, py, pz, nx, ny, nz, c.r, c.g, c.b};
}
} // namespace

MeshData makeBox(float size, const Color& color) {
    const float h = size * 0.5f;
    MeshData m;
    // 8 corners.
    const float px[8][3] = {{-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h},
                            {-h, -h, h},  {h, -h, h},  {h, h, h},  {-h, h, h}};
    struct Face {
        int a, b, c, d;
        float nx, ny, nz;
    };
    const Face faces[6] = {
        {1, 2, 6, 5, 1, 0, 0}, {0, 4, 7, 3, -1, 0, 0}, {3, 7, 6, 2, 0, 1, 0},
        {0, 1, 5, 4, 0, -1, 0}, {4, 5, 6, 7, 0, 0, 1}, {1, 0, 3, 2, 0, 0, -1},
    };
    for (const Face& f : faces) {
        const auto base = static_cast<uint32_t>(m.vertices.size());
        for (int corner : {f.a, f.b, f.c, f.d}) {
            m.vertices.push_back(
                vtx(px[corner][0], px[corner][1], px[corner][2], f.nx, f.ny, f.nz, color));
        }
        m.indices.insert(m.indices.end(),
                         {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    return m;
}

MeshData makeSphere(float radius, int rings, int sectors, const Color& color) {
    MeshData m;
    if (rings < 2) rings = 2;
    if (sectors < 3) sectors = 3;

    for (int i = 0; i <= rings; ++i) {
        const float theta = kPi * static_cast<float>(i) / static_cast<float>(rings);
        const float st = std::sin(theta);
        const float ct = std::cos(theta);
        for (int j = 0; j <= sectors; ++j) {
            const float phi = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sectors);
            const float nx = st * std::cos(phi);
            const float ny = ct;
            const float nz = st * std::sin(phi);
            m.vertices.push_back(
                vtx(nx * radius, ny * radius, nz * radius, nx, ny, nz, color));
        }
    }

    const auto stride = static_cast<uint32_t>(sectors + 1);
    for (uint32_t i = 0; i < static_cast<uint32_t>(rings); ++i) {
        for (uint32_t j = 0; j < static_cast<uint32_t>(sectors); ++j) {
            const uint32_t a = i * stride + j;
            const uint32_t b = a + stride;
            m.indices.insert(m.indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    return m;
}

MeshData makePlane(float halfSize, const Color& color) {
    MeshData m;
    const float s = halfSize;
    m.vertices = {
        vtx(-s, 0.0f, -s, 0, 1, 0, color),
        vtx(s, 0.0f, -s, 0, 1, 0, color),
        vtx(s, 0.0f, s, 0, 1, 0, color),
        vtx(-s, 0.0f, s, 0, 1, 0, color),
    };
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

} // namespace maz::render::shapes
