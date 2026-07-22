#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

// maz::render::Trail — the motion-ribbon behind sword swings, projectile streaks, dash after-images, and
// vehicle skid trails: keep a short history of where a point has been, and turn it into a flat ribbon of
// triangles that tapers to nothing at the old end and fades out over a lifetime. Every frame you push the
// current position and advance time; old points expire, and buildRibbon() emits a camera-facing quad strip
// (two vertices per history point, offset sideways perpendicular to both the trail's direction and the view
// direction) ready to draw. The width tapers by age so the head is full-width and the tail pinches to a
// point — the look everyone expects from a trail. This is a common Godot request (its built-in trails live
// only inside the particle system); here it is a small standalone builder over an arbitrary moving point.
// Header-only, std-only, deterministic — the ribbon geometry is exact CPU-side and testable without a GPU.
namespace maz::render {

struct RibbonMesh {
    std::vector<math::vec3> vertices;     // 2 per history point: [2i]=left edge, [2i+1]=right edge
    std::vector<std::uint32_t> indices;   // 2*(n-1) triangles
};

class Trail {
public:
    Trail(float lifetime, float width, std::size_t maxPoints = 256)
        : m_lifetime(lifetime > 1e-6f ? lifetime : 1e-6f),
          m_width(width),
          m_maxPoints(maxPoints < 2 ? 2 : maxPoints) {}

    // Append the current position at the head of the trail (age 0). Oldest points drop once maxPoints is hit.
    void push(const math::vec3& pos) {
        m_points.push_back(Point{pos, 0.0f});
        while (m_points.size() > m_maxPoints) {
            m_points.pop_front();
        }
    }

    // Advance time: age every point and expire those older than the lifetime.
    void update(float dt) {
        for (Point& p : m_points) {
            p.age += dt;
        }
        while (!m_points.empty() && m_points.front().age > m_lifetime) {
            m_points.pop_front();
        }
    }

    std::size_t pointCount() const { return m_points.size(); }
    void clear() { m_points.clear(); }

    // Build a camera-facing ribbon. `viewDir` is the direction from the trail toward the camera (need not be
    // normalized). Empty if fewer than two points remain.
    RibbonMesh buildRibbon(const math::vec3& viewDir) const {
        RibbonMesh mesh;
        const std::size_t n = m_points.size();
        if (n < 2) {
            return mesh;
        }
        mesh.vertices.resize(2 * n);
        for (std::size_t i = 0; i < n; ++i) {
            const math::vec3 pos = m_points[i].pos;
            // Tangent along the trail (central difference where possible).
            math::vec3 tangent;
            if (i == 0) {
                tangent = sub(m_points[1].pos, m_points[0].pos);
            } else if (i + 1 == n) {
                tangent = sub(m_points[n - 1].pos, m_points[n - 2].pos);
            } else {
                tangent = sub(m_points[i + 1].pos, m_points[i - 1].pos);
            }
            math::vec3 side = normalize(cross(tangent, viewDir));
            // Width tapers with age: full at the head (age 0), zero at the tail (age == lifetime).
            const float t = m_points[i].age / m_lifetime; // 0..1
            const float halfW = 0.5f * m_width * (1.0f - clamp01(t));
            mesh.vertices[2 * i] = add(pos, scale(side, halfW));       // left edge
            mesh.vertices[2 * i + 1] = sub(pos, scale(side, halfW));   // right edge
        }
        // Two triangles per segment between consecutive cross-sections.
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const std::uint32_t a = static_cast<std::uint32_t>(2 * i);
            const std::uint32_t b = static_cast<std::uint32_t>(2 * i + 1);
            const std::uint32_t c = static_cast<std::uint32_t>(2 * i + 2);
            const std::uint32_t d = static_cast<std::uint32_t>(2 * i + 3);
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
            mesh.indices.push_back(b);
            mesh.indices.push_back(d);
            mesh.indices.push_back(c);
        }
        return mesh;
    }

private:
    struct Point {
        math::vec3 pos;
        float age;
    };

    static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
    static math::vec3 add(const math::vec3& a, const math::vec3& b) { return math::vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
    static math::vec3 sub(const math::vec3& a, const math::vec3& b) { return math::vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
    static math::vec3 scale(const math::vec3& a, float s) { return math::vec3{a.x * s, a.y * s, a.z * s}; }
    static math::vec3 cross(const math::vec3& a, const math::vec3& b) {
        return math::vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    static math::vec3 normalize(const math::vec3& v) {
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len < 1e-12f) {
            return math::vec3{0.0f, 0.0f, 0.0f};
        }
        return math::vec3{v.x / len, v.y / len, v.z / len};
    }

    float m_lifetime;
    float m_width;
    std::size_t m_maxPoints;
    std::deque<Point> m_points; // front = oldest, back = newest (head)
};

} // namespace maz::render
