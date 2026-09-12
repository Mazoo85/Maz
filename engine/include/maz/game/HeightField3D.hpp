#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

// maz::game height-field collider — Godot's HeightMapShape3D: terrain stored as a regular grid of
// height samples rather than a full mesh, so a large landscape costs one float per cell. The core
// queries are heightAt/normalAt (the surface under a world XZ position, bilinearly interpolated —
// what a character controller, object placement, or foot IK needs) and a general raycast that marches
// the ray across the grid (2D DDA) testing each cell's two triangles with Möller–Trumbore. Local
// space: the grid's corner is the origin, X spans (cols-1)*cellX, Z spans (rows-1)*cellZ, Y is the
// stored height. Pure math (no renderer/physics), deterministic, header-only, unit-tested.
namespace maz::game {

class HeightField3D {
  public:
    HeightField3D() = default;
    HeightField3D(int cols, int rows, float cellX = 1.0f, float cellZ = 1.0f)
        : m_cols(cols < 1 ? 1 : cols), m_rows(rows < 1 ? 1 : rows), m_cellX(cellX), m_cellZ(cellZ),
          m_heights(static_cast<std::size_t>((cols < 1 ? 1 : cols) * (rows < 1 ? 1 : rows)), 0.0f) {}

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    float cellX() const { return m_cellX; }
    float cellZ() const { return m_cellZ; }
    float spanX() const { return static_cast<float>(m_cols - 1) * m_cellX; }
    float spanZ() const { return static_cast<float>(m_rows - 1) * m_cellZ; }

    void setHeight(int ix, int iz, float h) {
        if (ix >= 0 && ix < m_cols && iz >= 0 && iz < m_rows) {
            m_heights[idx(ix, iz)] = h;
        }
    }
    float height(int ix, int iz) const {
        if (ix < 0) ix = 0;
        if (iz < 0) iz = 0;
        if (ix >= m_cols) ix = m_cols - 1;
        if (iz >= m_rows) iz = m_rows - 1;
        return m_heights[idx(ix, iz)];
    }

    bool insideXZ(float x, float z) const {
        return x >= 0.0f && x <= spanX() && z >= 0.0f && z <= spanZ();
    }

    // Bilinearly-interpolated surface height at local (x,z). Clamps to the edge outside the grid.
    float heightAt(float x, float z) const {
        float fx = m_cellX > 0.0f ? x / m_cellX : 0.0f;
        float fz = m_cellZ > 0.0f ? z / m_cellZ : 0.0f;
        if (fx < 0.0f) fx = 0.0f;
        if (fz < 0.0f) fz = 0.0f;
        const float maxX = static_cast<float>(m_cols - 1);
        const float maxZ = static_cast<float>(m_rows - 1);
        if (fx > maxX) fx = maxX;
        if (fz > maxZ) fz = maxZ;
        const int ix = static_cast<int>(std::floor(fx));
        const int iz = static_cast<int>(std::floor(fz));
        const float tx = fx - static_cast<float>(ix);
        const float tz = fz - static_cast<float>(iz);
        const float h00 = height(ix, iz);
        const float h10 = height(ix + 1, iz);
        const float h01 = height(ix, iz + 1);
        const float h11 = height(ix + 1, iz + 1);
        const float a = h00 + (h10 - h00) * tx;
        const float b = h01 + (h11 - h01) * tx;
        return a + (b - a) * tz;
    }

    // Surface normal at local (x,z) from central differences of the height field.
    math::vec3 normalAt(float x, float z) const {
        const float e = 0.5f * (m_cellX + m_cellZ) * 0.5f + 1e-4f;
        const float hl = heightAt(x - e, z);
        const float hr = heightAt(x + e, z);
        const float hd = heightAt(x, z - e);
        const float hu = heightAt(x, z + e);
        math::vec3 n(hl - hr, 2.0f * e, hd - hu);
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        return len > 0.0f ? math::vec3(n.x / len, n.y / len, n.z / len) : math::vec3(0, 1, 0);
    }

    // Cast a ray (local space). On hit, fills outT (distance along dir) and outPoint. dir need not be
    // normalized; maxDist is in units of |dir|. Marches the grid cell-by-cell (2D DDA), testing each
    // cell's two triangles — so cost is O(cells crossed), not O(all cells).
    bool raycast(const math::vec3& origin, const math::vec3& dir, float maxDist, float& outT,
                 math::vec3& outPoint) const {
        // Determine the XZ cell the ray starts in (or would enter). We simply iterate parametrically
        // by stepping to each successive X or Z grid line and testing the cell we're leaving.
        const float invMax = maxDist;
        float bestT = invMax;
        bool hit = false;

        const int cellCountX = m_cols - 1;
        const int cellCountZ = m_rows - 1;
        if (cellCountX < 1 || cellCountZ < 1) {
            return false;
        }

        // Walk every cell whose XZ box the ray segment overlaps, in ascending t via DDA.
        // Start point clamped into the grid along the ray if it begins outside.
        float t0 = 0.0f;
        math::vec3 p = origin;
        // Advance to grid XZ bounds if starting outside (only along the ray).
        // (Simple approach: if outside, step t until inside or exceed maxDist.)
        if (!insideXZ(p.x, p.z)) {
            // Solve entry into [0,spanX]x[0,spanZ] on X and Z slabs.
            float tEnter = 0.0f;
            float tExit = maxDist;
            if (!slab(origin.x, dir.x, 0.0f, spanX(), tEnter, tExit)) {
                return false;
            }
            if (!slab(origin.z, dir.z, 0.0f, spanZ(), tEnter, tExit)) {
                return false;
            }
            if (tEnter > maxDist) {
                return false;
            }
            t0 = tEnter > 0.0f ? tEnter : 0.0f;
            p = origin + dir * t0;
        }

        int ix = clampCell(static_cast<int>(std::floor(p.x / m_cellX)), cellCountX);
        int iz = clampCell(static_cast<int>(std::floor(p.z / m_cellZ)), cellCountZ);

        const int stepX = dir.x > 0.0f ? 1 : (dir.x < 0.0f ? -1 : 0);
        const int stepZ = dir.z > 0.0f ? 1 : (dir.z < 0.0f ? -1 : 0);

        auto nextBoundaryT = [&](float o, float d, int cell, int step, float cellSz) -> float {
            if (step == 0) {
                return maxDist + 1.0f; // never crosses on this axis
            }
            const float boundary = static_cast<float>(cell + (step > 0 ? 1 : 0)) * cellSz;
            return o + d * ((boundary - (o /* placeholder */)) );
        };
        (void)nextBoundaryT; // (kept for clarity; explicit math below)

        float tMaxX = axisBoundaryT(origin.x, dir.x, ix, stepX, m_cellX, maxDist);
        float tMaxZ = axisBoundaryT(origin.z, dir.z, iz, stepZ, m_cellZ, maxDist);
        const float tDeltaX = stepX != 0 ? std::fabs(m_cellX / dir.x) : maxDist + 1.0f;
        const float tDeltaZ = stepZ != 0 ? std::fabs(m_cellZ / dir.z) : maxDist + 1.0f;

        for (int guard = 0; guard < (cellCountX + cellCountZ) * 2 + 4; ++guard) {
            if (ix >= 0 && ix < cellCountX && iz >= 0 && iz < cellCountZ) {
                float ct = 0.0f;
                math::vec3 cp;
                if (cellRaycast(ix, iz, origin, dir, maxDist, ct, cp) && ct < bestT) {
                    bestT = ct;
                    outPoint = cp;
                    hit = true;
                    // The ray can only hit closer cells first in DDA order, so we can stop once a
                    // cell yields a hit at t <= the next boundary. Conservatively keep the best.
                }
            }
            // Step to the next cell.
            if (tMaxX <= tMaxZ) {
                if (tMaxX > maxDist) break;
                ix += stepX;
                tMaxX += tDeltaX;
            } else {
                if (tMaxZ > maxDist) break;
                iz += stepZ;
                tMaxZ += tDeltaZ;
            }
            if (hit && bestT <= (tMaxX < tMaxZ ? tMaxX : tMaxZ)) {
                break; // nearest hit is confirmed
            }
            if (ix < -1 || ix > cellCountX || iz < -1 || iz > cellCountZ) {
                break;
            }
        }

        if (hit) {
            outT = bestT;
        }
        return hit;
    }

  private:
    std::size_t idx(int ix, int iz) const {
        return static_cast<std::size_t>(iz) * static_cast<std::size_t>(m_cols) +
               static_cast<std::size_t>(ix);
    }
    static int clampCell(int c, int count) {
        if (c < 0) return 0;
        if (c >= count) return count - 1;
        return c;
    }

    // 1D slab clip: refine [tEnter,tExit] to where origin+t*dir stays in [lo,hi]. Returns false if empty.
    static bool slab(float o, float d, float lo, float hi, float& tEnter, float& tExit) {
        if (std::fabs(d) < 1e-9f) {
            return o >= lo && o <= hi;
        }
        float t1 = (lo - o) / d;
        float t2 = (hi - o) / d;
        if (t1 > t2) {
            const float tmp = t1;
            t1 = t2;
            t2 = tmp;
        }
        if (t1 > tEnter) tEnter = t1;
        if (t2 < tExit) tExit = t2;
        return tEnter <= tExit;
    }

    static float axisBoundaryT(float o, float d, int cell, int step, float cellSz, float maxDist) {
        if (step == 0) {
            return maxDist + 1.0f;
        }
        const float boundary = static_cast<float>(cell + (step > 0 ? 1 : 0)) * cellSz;
        return (boundary - o) / d;
    }

    math::vec3 corner(int ix, int iz) const {
        return math::vec3(static_cast<float>(ix) * m_cellX, height(ix, iz),
                          static_cast<float>(iz) * m_cellZ);
    }

    bool cellRaycast(int ix, int iz, const math::vec3& o, const math::vec3& d, float maxDist,
                     float& outT, math::vec3& outPoint) const {
        const math::vec3 c00 = corner(ix, iz);
        const math::vec3 c10 = corner(ix + 1, iz);
        const math::vec3 c01 = corner(ix, iz + 1);
        const math::vec3 c11 = corner(ix + 1, iz + 1);
        float t = 0.0f;
        bool any = false;
        float best = maxDist;
        if (triRay(o, d, c00, c10, c11, t) && t < best) {
            best = t;
            any = true;
        }
        if (triRay(o, d, c00, c11, c01, t) && t < best) {
            best = t;
            any = true;
        }
        if (any) {
            outT = best;
            outPoint = o + d * best;
        }
        return any;
    }

    // Möller–Trumbore ray/triangle, front+back faces, t in [0,maxDist implicit via caller].
    static bool triRay(const math::vec3& o, const math::vec3& d, const math::vec3& a,
                       const math::vec3& b, const math::vec3& c, float& outT) {
        const math::vec3 e1(b.x - a.x, b.y - a.y, b.z - a.z);
        const math::vec3 e2(c.x - a.x, c.y - a.y, c.z - a.z);
        const math::vec3 pv(d.y * e2.z - d.z * e2.y, d.z * e2.x - d.x * e2.z,
                            d.x * e2.y - d.y * e2.x);
        const float det = e1.x * pv.x + e1.y * pv.y + e1.z * pv.z;
        if (std::fabs(det) < 1e-12f) {
            return false;
        }
        const float inv = 1.0f / det;
        const math::vec3 tv(o.x - a.x, o.y - a.y, o.z - a.z);
        const float u = (tv.x * pv.x + tv.y * pv.y + tv.z * pv.z) * inv;
        if (u < -1e-5f || u > 1.0f + 1e-5f) {
            return false;
        }
        const math::vec3 qv(tv.y * e1.z - tv.z * e1.y, tv.z * e1.x - tv.x * e1.z,
                            tv.x * e1.y - tv.y * e1.x);
        const float v = (d.x * qv.x + d.y * qv.y + d.z * qv.z) * inv;
        if (v < -1e-5f || u + v > 1.0f + 1e-5f) {
            return false;
        }
        const float t = (e2.x * qv.x + e2.y * qv.y + e2.z * qv.z) * inv;
        if (t < 0.0f) {
            return false;
        }
        outT = t;
        return true;
    }

    int m_cols = 1;
    int m_rows = 1;
    float m_cellX = 1.0f;
    float m_cellZ = 1.0f;
    std::vector<float> m_heights;
};

} // namespace maz::game
