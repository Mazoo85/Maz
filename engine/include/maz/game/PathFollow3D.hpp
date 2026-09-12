#pragma once

#include "maz/math/Curve3D.hpp" // math::Curve3D, math::vec3

#include <algorithm>
#include <cmath>

// maz::game Path3D / PathFollow3D — Godot's 3D path nodes, the twin of Path2D/PathFollow2D (M221). A
// Path3D holds a Curve3D; a PathFollow3D walks it at a *progress* measured in arc length, so something
// advances along it at constant speed. Exposes loop (wrap vs clamp) and reports the position + the
// forward tangent at that progress (the caller builds an orientation from it). Pure, header-only,
// deterministic. Honest scope: Godot's PathFollow3D rotation modes (Y/XY/XYZ/ORIENTED) and per-point
// tilt/up-vector banking are not modelled — this returns the forward tangent for the caller to use.
namespace maz::game {

struct PathSample3D {
    math::vec3 position{0.0f, 0.0f, 0.0f};
    math::vec3 forward{1.0f, 0.0f, 0.0f}; // unit tangent (direction of travel)
};

class Path3D {
public:
    math::Curve3D& curve() { return m_curve; }
    const math::Curve3D& curve() const { return m_curve; }
    float length() {
        m_curve.sampleBaked(0.0f); // auto-bakes
        return m_curve.bakedLength();
    }

private:
    math::Curve3D m_curve;
};

class PathFollow3D {
public:
    PathFollow3D() = default;
    explicit PathFollow3D(math::Curve3D* curve) : m_curve(curve) {}

    void setCurve(math::Curve3D* curve) {
        m_curve = curve;
        setProgress(m_progress);
    }
    math::Curve3D* curve() const { return m_curve; }

    void setLoop(bool loop) { m_loop = loop; }
    bool loop() const { return m_loop; }

    float length() const {
        if (!m_curve) {
            return 0.0f;
        }
        m_curve->sampleBaked(0.0f);
        return m_curve->bakedLength();
    }

    void setProgress(float distance) { m_progress = wrapClamp(distance); }
    float progress() const { return m_progress; }

    void setProgressRatio(float ratio) { setProgress(ratio * length()); }
    float progressRatio() const {
        const float len = length();
        return len > 0.0f ? m_progress / len : 0.0f;
    }

    void advance(float delta) { setProgress(m_progress + delta); }

    PathSample3D sample() const {
        PathSample3D s;
        if (!m_curve) {
            return s;
        }
        const float len = length();
        const float d = m_progress;
        s.position = m_curve->sampleBaked(d);
        const float e = std::max(len * 0.001f, 0.01f);
        const math::vec3 ahead = m_curve->sampleBaked(std::min(d + e, len));
        const math::vec3 behind = m_curve->sampleBaked(std::max(d - e, 0.0f));
        math::vec3 dir = ahead - behind;
        const float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        s.forward = dl > 1e-6f ? math::vec3(dir.x / dl, dir.y / dl, dir.z / dl)
                               : math::vec3(1.0f, 0.0f, 0.0f);
        return s;
    }

private:
    float wrapClamp(float distance) const {
        const float len = length();
        if (len <= 0.0f) {
            return 0.0f;
        }
        if (m_loop) {
            float dd = std::fmod(distance, len);
            if (dd < 0.0f) {
                dd += len;
            }
            return dd;
        }
        return std::clamp(distance, 0.0f, len);
    }

    math::Curve3D* m_curve = nullptr;
    float m_progress = 0.0f;
    bool m_loop = false;
};

} // namespace maz::game
