// Unit tests for procedural primitive mesh generation. Pure CPU, no GPU.

#include "maz/assets/Primitives.hpp"

#include <cmath>
#include <cstdio>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

bool nearly(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// True if every vertex normal in the mesh is unit length.
bool allNormalsUnit(const assets::Mesh& m) {
    for (const assets::Vertex& v : m.vertices) {
        const float len =
            std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] + v.normal[2] * v.normal[2]);
        if (!nearly(len, 1.0f)) {
            return false;
        }
    }
    return true;
}

// True if every triangle is wound CCW-outward: its geometric normal (v1-v0)x(v2-v0) agrees
// (positive dot) with the averaged shading normal of its three vertices.
bool windingAgreesWithNormals(const assets::Mesh& m) {
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        const assets::Vertex& v0 = m.vertices[m.indices[i]];
        const assets::Vertex& v1 = m.vertices[m.indices[i + 1]];
        const assets::Vertex& v2 = m.vertices[m.indices[i + 2]];
        const float e1[3] = {v1.position[0] - v0.position[0], v1.position[1] - v0.position[1],
                             v1.position[2] - v0.position[2]};
        const float e2[3] = {v2.position[0] - v0.position[0], v2.position[1] - v0.position[1],
                             v2.position[2] - v0.position[2]};
        const float geo[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                              e1[0] * e2[1] - e1[1] * e2[0]};
        // Degenerate (zero-area) triangles have no winding: the pole rows of a sphere collapse to a
        // single point. Skip them rather than flag their zero-length geometric normal.
        const float geoLen2 = geo[0] * geo[0] + geo[1] * geo[1] + geo[2] * geo[2];
        if (geoLen2 < 1e-12f) {
            continue;
        }
        const float avg[3] = {(v0.normal[0] + v1.normal[0] + v2.normal[0]) / 3.0f,
                              (v0.normal[1] + v1.normal[1] + v2.normal[1]) / 3.0f,
                              (v0.normal[2] + v1.normal[2] + v2.normal[2]) / 3.0f};
        const float d = geo[0] * avg[0] + geo[1] * avg[1] + geo[2] * avg[2];
        if (!(d > 0.0f)) {
            return false;
        }
    }
    return true;
}

// Axis-aligned extents over all vertex positions.
void extents(const assets::Mesh& m, float mn[3], float mx[3]) {
    for (int k = 0; k < 3; ++k) {
        mn[k] = mx[k] = 0.0f;
    }
    bool seeded = false;
    for (const assets::Vertex& v : m.vertices) {
        for (int k = 0; k < 3; ++k) {
            if (!seeded || v.position[k] < mn[k]) mn[k] = v.position[k];
            if (!seeded || v.position[k] > mx[k]) mx[k] = v.position[k];
        }
        seeded = true;
    }
}

} // namespace

int main() {
    // Box: 24 verts / 36 indices, per-face normals, extents = +/- half size.
    {
        const assets::Mesh box = assets::makeBox(2.0f, 4.0f, 6.0f);
        check(box.vertices.size() == 24, "box has 24 vertices");
        check(box.indices.size() == 36, "box has 36 indices");
        check(allNormalsUnit(box), "box normals are unit length");
        check(windingAgreesWithNormals(box), "box triangle winding agrees with normals");
        float mn[3], mx[3];
        extents(box, mn, mx);
        check(nearly(mn[0], -1.0f) && nearly(mx[0], 1.0f), "box x extent = +/-1");
        check(nearly(mn[1], -2.0f) && nearly(mx[1], 2.0f), "box y extent = +/-2");
        check(nearly(mn[2], -3.0f) && nearly(mx[2], 3.0f), "box z extent = +/-3");
    }

    // Sphere: (rings+1)*(segments+1) verts, rings*segments*6 indices, radius extents.
    {
        const int segments = 20;
        const int rings = 10;
        const assets::Mesh sphere = assets::makeSphere(1.5f, segments, rings);
        const size_t expectVerts = static_cast<size_t>((rings + 1) * (segments + 1));
        const size_t expectIndices = static_cast<size_t>(rings * segments * 6);
        check(sphere.vertices.size() == expectVerts, "sphere vertex count = (rings+1)*(segments+1)");
        check(sphere.indices.size() == expectIndices, "sphere index count = rings*segments*6");
        check(allNormalsUnit(sphere), "sphere normals are unit length");
        check(windingAgreesWithNormals(sphere), "sphere triangle winding agrees with normals");
        float mn[3], mx[3];
        extents(sphere, mn, mx);
        check(nearly(mn[0], -1.5f) && nearly(mx[0], 1.5f), "sphere x extent = +/- radius");
        check(nearly(mn[1], -1.5f) && nearly(mx[1], 1.5f), "sphere y extent = +/- radius");
    }

    // Cylinder: unit normals, radius/height extents.
    {
        const assets::Mesh cyl = assets::makeCylinder(0.5f, 3.0f, 24);
        check(!cyl.vertices.empty() && !cyl.indices.empty(), "cylinder is non-empty");
        check(cyl.indices.size() % 3 == 0, "cylinder index count is a multiple of 3");
        check(allNormalsUnit(cyl), "cylinder normals are unit length");
        check(windingAgreesWithNormals(cyl), "cylinder triangle winding agrees with normals");
        float mn[3], mx[3];
        extents(cyl, mn, mx);
        check(nearly(mn[1], -1.5f) && nearly(mx[1], 1.5f), "cylinder y extent = +/- half height");
        check(nearly(mn[0], -0.5f) && nearly(mx[0], 0.5f), "cylinder x extent = +/- radius");
    }

    // Plane: 4 verts / 6 indices, all normals +Y, y == 0.
    {
        const assets::Mesh plane = assets::makePlane(4.0f, 8.0f);
        check(plane.vertices.size() == 4, "plane has 4 vertices");
        check(plane.indices.size() == 6, "plane has 6 indices");
        bool up = true;
        for (const assets::Vertex& v : plane.vertices) {
            up = up && nearly(v.normal[1], 1.0f) && nearly(v.position[1], 0.0f);
        }
        check(up, "plane faces +Y at y=0");
        check(windingAgreesWithNormals(plane), "plane triangle winding agrees with normals");
        float mn[3], mx[3];
        extents(plane, mn, mx);
        check(nearly(mn[0], -2.0f) && nearly(mx[0], 2.0f), "plane width extent = +/-2");
        check(nearly(mn[2], -4.0f) && nearly(mx[2], 4.0f), "plane depth extent = +/-4");
    }

    // Dispatch: makePrimitive matches the direct generator.
    {
        assets::PrimitiveParams params;
        params.size[0] = params.size[1] = params.size[2] = 1.0f;
        const assets::Mesh viaDispatch = assets::makePrimitive(assets::PrimitiveKind::Box, params);
        check(viaDispatch.vertices.size() == 24, "makePrimitive(Box) yields a box");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
