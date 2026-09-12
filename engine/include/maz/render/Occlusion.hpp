#pragma once

#include "maz/math/Math.hpp"

#include <cstddef>
#include <vector>

// maz::render software occlusion culling — a conservative low-resolution occlusion buffer, the CPU
// technique behind Godot's OccluderInstance3D culling. Big nearby occluders (walls, terrain) are
// rasterized into a coarse depth grid; each writes the depth of its FARTHEST face, so the buffer
// records, per cell, a depth beyond which geometry is CERTAINLY hidden. A candidate object is culled
// only if every cell it covers is solid AND the object's nearest point lies at/behind that solid
// depth — so the test never hides something visible (no false occlusion), it only ever culls what is
// provably blocked. projectAabb() maps a world AABB through a view-projection matrix to the screen
// rect + depth range this buffer consumes. Pure math (no GPU); the coarse buffer keeps it cheap and
// unit-testable.
namespace maz::render {

// Screen-space rectangle [x0,x1) x [y0,y1) in pixels plus a depth range, produced by projectAabb.
struct ScreenRect {
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    float nearDepth = 0.0f; // closest projected depth of the box (smallest)
    float farDepth = 0.0f;  // farthest projected depth (largest)
    bool valid = false;     // false if the box crosses the camera plane / is fully behind
};

class OcclusionBuffer {
  public:
    OcclusionBuffer(int width, int height)
        : m_w(width < 1 ? 1 : width), m_h(height < 1 ? 1 : height),
          m_depth(static_cast<std::size_t>((width < 1 ? 1 : width) * (height < 1 ? 1 : height)),
                  kUncovered) {}

    int width() const { return m_w; }
    int height() const { return m_h; }

    // Reset all cells to "uncovered".
    void clear() {
        for (float& d : m_depth) {
            d = kUncovered;
        }
    }

    // Rasterize an occluder: mark its screen cells solid out to `farDepth`, keeping the deepest
    // (max) coverage where occluders overlap.
    void addOccluder(const ScreenRect& r) {
        if (!r.valid) {
            return;
        }
        int cx0 = 0;
        int cy0 = 0;
        int cx1 = 0;
        int cy1 = 0;
        if (!cellRange(r, cx0, cy0, cx1, cy1)) {
            return;
        }
        for (int y = cy0; y < cy1; ++y) {
            for (int x = cx0; x < cx1; ++x) {
                float& d = m_depth[idx(x, y)];
                if (d == kUncovered || r.farDepth > d) {
                    d = r.farDepth;
                }
            }
        }
    }
    void addOccluder(float x0, float y0, float x1, float y1, float farDepth) {
        ScreenRect r;
        r.x0 = x0;
        r.y0 = y0;
        r.x1 = x1;
        r.y1 = y1;
        r.farDepth = farDepth;
        r.nearDepth = farDepth;
        r.valid = true;
        addOccluder(r);
    }

    // Is a candidate hidden? True only if every cell it covers is solid AND its nearest point is at
    // or behind the solid depth there. Any uncovered cell, or the object being in front, => visible.
    bool isOccluded(const ScreenRect& r) const {
        if (!r.valid) {
            return false; // spans the camera / unknown -> never cull
        }
        int cx0 = 0;
        int cy0 = 0;
        int cx1 = 0;
        int cy1 = 0;
        if (!cellRange(r, cx0, cy0, cx1, cy1)) {
            return false; // off screen -> let normal frustum cull handle it, don't claim occluded
        }
        for (int y = cy0; y < cy1; ++y) {
            for (int x = cx0; x < cx1; ++x) {
                const float d = m_depth[idx(x, y)];
                if (d == kUncovered) {
                    return false; // a gap in coverage -> possibly visible
                }
                if (r.nearDepth < d) {
                    return false; // object's nearest point is in front of the solid depth -> visible
                }
            }
        }
        return true;
    }
    bool isOccluded(float x0, float y0, float x1, float y1, float nearDepth) const {
        ScreenRect r;
        r.x0 = x0;
        r.y0 = y0;
        r.x1 = x1;
        r.y1 = y1;
        r.nearDepth = nearDepth;
        r.farDepth = nearDepth;
        r.valid = true;
        return isOccluded(r);
    }

    // Direct cell depth read (kUncovered if empty) — for tests/inspection.
    float cell(int x, int y) const {
        if (x < 0 || y < 0 || x >= m_w || y >= m_h) {
            return kUncovered;
        }
        return m_depth[idx(x, y)];
    }
    static constexpr float uncovered() { return kUncovered; }

  private:
    static constexpr float kUncovered = -1.0f;

    std::size_t idx(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) +
               static_cast<std::size_t>(x);
    }

    // Convert a pixel rect to a half-open cell range clamped to the buffer. Returns false if empty.
    bool cellRange(const ScreenRect& r, int& cx0, int& cy0, int& cx1, int& cy1) const {
        cx0 = static_cast<int>(std::floor(r.x0));
        cy0 = static_cast<int>(std::floor(r.y0));
        cx1 = static_cast<int>(std::ceil(r.x1));
        cy1 = static_cast<int>(std::ceil(r.y1));
        if (cx0 < 0) cx0 = 0;
        if (cy0 < 0) cy0 = 0;
        if (cx1 > m_w) cx1 = m_w;
        if (cy1 > m_h) cy1 = m_h;
        return cx1 > cx0 && cy1 > cy0;
    }

    int m_w;
    int m_h;
    std::vector<float> m_depth;
};

// Project a world-space AABB (min..max) through a view-projection matrix to a screen rect + depth
// range for the given viewport size. valid=false when any corner is at/behind the camera plane
// (w <= 0), in which case the caller should not occlusion-cull it.
inline ScreenRect projectAabb(const math::vec3& mn, const math::vec3& mx, const math::mat4& viewProj,
                              float viewportW, float viewportH) {
    ScreenRect out;
    float minX = 1e30f;
    float minY = 1e30f;
    float minZ = 1e30f;
    float maxX = -1e30f;
    float maxY = -1e30f;
    float maxZ = -1e30f;
    for (int c = 0; c < 8; ++c) {
        const math::vec3 corner((c & 1) ? mx.x : mn.x, (c & 2) ? mx.y : mn.y, (c & 4) ? mx.z : mn.z);
        const math::vec4 clip = viewProj * math::vec4(corner, 1.0f);
        if (clip.w <= 1e-6f) {
            out.valid = false; // crosses/behind the camera
            return out;
        }
        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        const float ndcZ = clip.z / clip.w;
        const float sx = (ndcX * 0.5f + 0.5f) * viewportW;
        const float sy = (ndcY * 0.5f + 0.5f) * viewportH;
        if (sx < minX) minX = sx;
        if (sy < minY) minY = sy;
        if (ndcZ < minZ) minZ = ndcZ;
        if (sx > maxX) maxX = sx;
        if (sy > maxY) maxY = sy;
        if (ndcZ > maxZ) maxZ = ndcZ;
    }
    out.x0 = minX;
    out.y0 = minY;
    out.x1 = maxX;
    out.y1 = maxY;
    out.nearDepth = minZ;
    out.farDepth = maxZ;
    out.valid = true;
    return out;
}

} // namespace maz::render
