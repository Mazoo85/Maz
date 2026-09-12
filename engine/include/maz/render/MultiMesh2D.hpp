#pragma once

#include "maz/math/Math.hpp"
#include "maz/render/Renderer.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::render {

// 2D multi-mesh instancing — Godot's MultiMesh / MultiMeshInstance2D. Drawing a thousand grass blades, stars,
// or bullets as a thousand separate polygons means a thousand transform setups; a MultiMesh stores ONE base
// shape once plus a compact per-instance buffer (position, rotation, scale, colour) and stamps the shape at
// every instance. This is the 2D data structure for that: a convex base polygon in local space plus a list of
// `Instance2D`s. `transformedPolygon(i)` returns one instance's world-space polygon (for a per-instance
// coloured draw), and `bakeTriangles()` flattens EVERY instance into one triangle-fan soup ready to hand to
// a single batched draw / vertex upload. Header-only, math-only (no GPU state), so the transform math
// unit-tests headlessly; the app draws the baked instances.

struct Instance2D {
    math::vec2 position{0.0f, 0.0f};
    float rotation = 0.0f; // radians
    math::vec2 scale{1.0f, 1.0f};
    Color color{1.0f, 1.0f, 1.0f, 1.0f};
};

// Transform a base-shape local vertex by an instance's scale -> rotation -> translation (the standard 2D TRS
// order).
inline math::vec2 transformInstance(const Instance2D& inst, const math::vec2& local) {
    const float sx = local.x * inst.scale.x;
    const float sy = local.y * inst.scale.y;
    const float c = std::cos(inst.rotation);
    const float s = std::sin(inst.rotation);
    return math::vec2(inst.position.x + sx * c - sy * s, inst.position.y + sx * s + sy * c);
}

class MultiMesh2D {
public:
    std::vector<math::vec2> baseVertices; // local-space convex polygon (drawn as a triangle fan)

    void addInstance(const Instance2D& inst) { m_instances.push_back(inst); }
    void clear() { m_instances.clear(); }

    std::size_t instanceCount() const { return m_instances.size(); }
    const Instance2D& instance(std::size_t i) const { return m_instances[i]; }

    // World-space polygon of one instance (same vertex count as baseVertices).
    std::vector<math::vec2> transformedPolygon(std::size_t i) const {
        std::vector<math::vec2> out;
        out.reserve(baseVertices.size());
        for (const math::vec2& v : baseVertices) {
            out.push_back(transformInstance(m_instances[i], v));
        }
        return out;
    }

    // Number of triangles produced by baking (fan of an n-gon = n-2 tris, times instances).
    std::size_t triangleCount() const {
        return baseVertices.size() < 3 ? 0 : (baseVertices.size() - 2) * m_instances.size();
    }

    // Flatten every instance into one triangle soup (groups of 3 vertices), fanning each instance's polygon
    // from its first vertex. Empty if the base isn't at least a triangle.
    std::vector<math::vec2> bakeTriangles() const {
        std::vector<math::vec2> tris;
        if (baseVertices.size() < 3) {
            return tris;
        }
        tris.reserve(triangleCount() * 3);
        for (const Instance2D& inst : m_instances) {
            std::vector<math::vec2> poly;
            poly.reserve(baseVertices.size());
            for (const math::vec2& v : baseVertices) {
                poly.push_back(transformInstance(inst, v));
            }
            for (std::size_t k = 1; k + 1 < poly.size(); ++k) {
                tris.push_back(poly[0]);
                tris.push_back(poly[k]);
                tris.push_back(poly[k + 1]);
            }
        }
        return tris;
    }

private:
    std::vector<Instance2D> m_instances;
};

} // namespace maz::render
