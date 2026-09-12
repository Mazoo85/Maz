#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::render mesh LOD selection — the CPU half of Godot's automatic mesh level-of-detail: as an
// object shrinks on screen, swap in a cheaper mesh so distant geometry costs less. The decision is
// screen-coverage based (like Godot's mesh_lod_threshold): project the object's bounding radius to a
// pixel size for the current camera, then pick the finest LOD whose pixel threshold it still meets;
// below the coarsest threshold the object can be culled entirely. A lod_bias multiplier and optional
// switch hysteresis (to stop LODs flickering at a boundary) round it out. Pure math — no GPU, no
// mesh data — so it unit-tests headlessly; the renderer just draws whichever index this returns.
namespace maz::render {

// Projected pixel radius of a sphere of world `radius` at `distance` from the camera, for a vertical
// field of view `fovYRadians` and a viewport `viewportHeightPx` pixels tall. 0 when behind/at the eye.
inline float projectedRadiusPixels(float radius, float distance, float fovYRadians,
                                   float viewportHeightPx) {
    if (distance <= 1e-4f) {
        return viewportHeightPx; // essentially at the eye -> fills the view
    }
    const float t = std::tan(fovYRadians * 0.5f);
    if (t <= 1e-6f) {
        return 0.0f;
    }
    return (radius / (distance * t)) * (viewportHeightPx * 0.5f);
}

class LodChain {
  public:
    // Add a level (call fine -> coarse). `minPixels` is the smallest projected size for which this
    // level is still used; thresholds must descend (finest level has the largest threshold).
    void addLevel(float minPixels) { m_minPixels.push_back(minPixels); }
    void clear() { m_minPixels.clear(); }
    std::size_t levelCount() const { return m_minPixels.size(); }

    // Cull (select() returns -1) when the object is smaller than the coarsest level's threshold.
    void setCullBelowLast(bool c) { m_cullBelowLast = c; }
    // Global bias: >1 keeps finer LODs longer (Godot's lod_bias), <1 drops to coarse sooner.
    void setBias(float bias) { m_bias = bias > 0.0f ? bias : 1.0f; }

    // Choose a LOD index from a projected pixel size. Returns the finest level whose threshold the
    // (biased) size still meets; -1 if it falls below the coarsest and culling is enabled, else the
    // coarsest index.
    int select(float projectedPixels) const {
        if (m_minPixels.empty()) {
            return -1;
        }
        const float px = projectedPixels * m_bias;
        for (std::size_t i = 0; i < m_minPixels.size(); ++i) {
            if (px >= m_minPixels[i]) {
                return static_cast<int>(i);
            }
        }
        return m_cullBelowLast ? -1 : static_cast<int>(m_minPixels.size() - 1);
    }

    // Convenience: project then select in one call.
    int selectForCamera(float radius, float distance, float fovYRadians, float viewportHeightPx) const {
        return select(projectedRadiusPixels(radius, distance, fovYRadians, viewportHeightPx));
    }

    // Hysteresis-aware select: only change away from `current` once the size crosses the neighbour's
    // threshold by `margin` pixels, so an object sitting on a boundary doesn't flicker between LODs.
    int selectStable(float projectedPixels, int current, float margin = 4.0f) const {
        const int fresh = select(projectedPixels);
        if (current < 0 || current >= static_cast<int>(m_minPixels.size())) {
            return fresh;
        }
        const float px = projectedPixels * m_bias;
        // Stay on `current` unless we clear the boundary to the neighbour by `margin`.
        if (fresh == current + 1) {
            // Dropping to coarser: require px to be below the current level's threshold by margin.
            if (px > m_minPixels[static_cast<std::size_t>(current)] - margin) {
                return current;
            }
        } else if (fresh == current - 1) {
            // Rising to finer: require px above the finer level's threshold by margin.
            if (px < m_minPixels[static_cast<std::size_t>(current - 1)] + margin) {
                return current;
            }
        }
        return fresh;
    }

  private:
    std::vector<float> m_minPixels;
    float m_bias = 1.0f;
    bool m_cullBelowLast = false;
};

} // namespace maz::render
