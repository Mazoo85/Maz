#pragma once

#include "maz/math/Curve2D.hpp"
#include "maz/math/Math.hpp"

#include <cmath>

// maz::game PathFollow2D — Godot's PathFollow2D node: something that travels ALONG a Curve2D at a
// distance offset and, optionally, orients itself to the path's direction. You set progress (arc
// length from the start) or progress_ratio (0..1), advance it each frame, and sample() gives back
// the world position and a heading. `loop` wraps progress at the ends (a patrol that circles a
// track); otherwise it clamps. `hOffset` slides the result sideways along the path normal (a lane
// offset / formation). `rotates` turns the heading to face along the curve. Built on the existing
// arc-length-baked Curve2D so motion is constant-speed regardless of how the curve bends. Pure,
// math-only, unit-tested; a moving platform, a homing rail, or a camera dolly rides one of these.
namespace maz::game {

struct PathSample2D {
    math::vec2 position{0.0f, 0.0f};
    float rotation = 0.0f; // radians; heading along the path tangent (0 when rotates is off)
};

class PathFollow2D {
  public:
    void setProgress(float p) { m_progress = p; }
    float progress() const { return m_progress; }
    void advance(float delta) { m_progress += delta; }

    void setHOffset(float h) { m_hOffset = h; }
    float hOffset() const { return m_hOffset; }
    void setLoop(bool loop) { m_loop = loop; }
    bool loop() const { return m_loop; }
    void setRotates(bool r) { m_rotates = r; }
    bool rotates() const { return m_rotates; }

    // Set/get progress as a 0..1 fraction of the curve's baked length.
    void setProgressRatio(float ratio, math::Curve2D& curve) {
        m_progress = ratio * curveLength(curve);
    }
    float progressRatio(math::Curve2D& curve) const {
        const float len = curveLength(curve);
        return len > 0.0f ? resolve(m_progress, len) / len : 0.0f;
    }

    // Sample the transform at the current progress. `curve` is non-const because Curve2D bakes its
    // arc-length table lazily on first sample.
    PathSample2D sample(math::Curve2D& curve) const {
        PathSample2D out;
        const float len = curveLength(curve);
        if (len <= 0.0f) {
            out.position = curve.sampleBaked(0.0f);
            return out;
        }
        const float d = resolve(m_progress, len);
        const math::vec2 pos = curve.sampleBaked(d);

        // Tangent by central finite difference on the baked path (clamped near the ends).
        const float eps = len * 0.001f + 1e-3f;
        const float a = clampDist(d - eps, len);
        const float b = clampDist(d + eps, len);
        math::vec2 tangent = curve.sampleBaked(b) - curve.sampleBaked(a);
        const float tl = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);
        if (tl > 1e-6f) {
            tangent.x /= tl;
            tangent.y /= tl;
        } else {
            tangent = math::vec2(1.0f, 0.0f);
        }

        // Perpendicular (left normal) for hOffset: rotate tangent +90 degrees.
        const math::vec2 normal(-tangent.y, tangent.x);
        out.position = pos + normal * m_hOffset;
        out.rotation = m_rotates ? std::atan2(tangent.y, tangent.x) : 0.0f;
        return out;
    }

  private:
    static float curveLength(math::Curve2D& curve) {
        if (curve.bakedLength() <= 0.0f) {
            curve.sampleBaked(0.0f); // force a bake
        }
        return curve.bakedLength();
    }

    // Map a raw progress value into [0,len): wrap when looping, clamp otherwise.
    float resolve(float p, float len) const {
        if (len <= 0.0f) {
            return 0.0f;
        }
        if (m_loop) {
            float m = std::fmod(p, len);
            if (m < 0.0f) {
                m += len;
            }
            return m;
        }
        if (p < 0.0f) {
            return 0.0f;
        }
        if (p > len) {
            return len;
        }
        return p;
    }

    static float clampDist(float d, float len) {
        if (d < 0.0f) {
            return 0.0f;
        }
        if (d > len) {
            return len;
        }
        return d;
    }

    float m_progress = 0.0f;
    float m_hOffset = 0.0f;
    bool m_loop = false;
    bool m_rotates = true;
};

} // namespace maz::game
