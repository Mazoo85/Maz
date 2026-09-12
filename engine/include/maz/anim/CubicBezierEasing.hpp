#pragma once

#include <algorithm> // std::clamp
#include <cmath>     // std::fabs

// maz::anim CUBIC-BEZIER EASING — arbitrary motion curves defined exactly like CSS `cubic-bezier(x1,y1,x2,y2)` and
// the browser `ease`/`ease-in`/`ease-out`/`ease-in-out` presets. The engine already has a fixed menu of named
// easings (Transition/Tween); this lets a designer dial in ANY curve by placing the two control handles, then
// evaluate it per-frame to drive a tween, a UI transition, or a camera move. Endpoints are fixed at (0,0)→(1,1);
// the input is progress `t` in [0,1] (the curve's x), and the output is the eased value (its y). Solved with the
// standard Newton–Raphson-then-bisection root find on x (WebKit's UnitBezier method). Header-only, deterministic.
//
// Scope note (honest): x1/x2 are clamped to [0,1] so x stays monotonic (a well-defined function); y1/y2 are
// unclamped, so springy curves may intentionally overshoot below 0 or above 1 (as in CSS).
namespace maz::anim {

class CubicBezierEasing {
public:
    CubicBezierEasing() = default; // default is linear (identity)
    CubicBezierEasing(float x1, float y1, float x2, float y2) {
        x1 = std::clamp(x1, 0.0f, 1.0f);
        x2 = std::clamp(x2, 0.0f, 1.0f);
        // Polynomial coefficients for a cubic Bezier with P0=(0,0), P3=(1,1).
        m_cx = 3.0f * x1;
        m_bx = 3.0f * (x2 - x1) - m_cx;
        m_ax = 1.0f - m_cx - m_bx;
        m_cy = 3.0f * y1;
        m_by = 3.0f * (y2 - y1) - m_cy;
        m_ay = 1.0f - m_cy - m_by;
    }

    // Eased value at progress `t` (clamped to [0,1]). f(0)==0, f(1)==1.
    float operator()(float t) const {
        if (t <= 0.0f) return 0.0f;
        if (t >= 1.0f) return 1.0f;
        return sampleY(solveForU(t));
    }

private:
    float sampleX(float u) const { return ((m_ax * u + m_bx) * u + m_cx) * u; }
    float sampleY(float u) const { return ((m_ay * u + m_by) * u + m_cy) * u; }
    float sampleXDeriv(float u) const { return (3.0f * m_ax * u + 2.0f * m_bx) * u + m_cx; }

    // Find the curve parameter u such that sampleX(u) == x.
    float solveForU(float x) const {
        // Newton–Raphson (fast when it works).
        float u = x;
        for (int i = 0; i < 8; ++i) {
            const float err = sampleX(u) - x;
            if (std::fabs(err) < 1e-6f) return u;
            const float d = sampleXDeriv(u);
            if (std::fabs(d) < 1e-6f) break;
            u -= err / d;
        }
        // Fallback: bisection, guaranteed to converge on [0,1].
        float lo = 0.0f, hi = 1.0f;
        u = x;
        if (u < lo) return lo;
        if (u > hi) return hi;
        for (int i = 0; i < 24; ++i) {
            const float xu = sampleX(u);
            if (std::fabs(xu - x) < 1e-6f) return u;
            if (xu < x) lo = u; else hi = u;
            u = 0.5f * (lo + hi);
        }
        return u;
    }

    // Coefficients (linear identity by default).
    float m_ax = 0.0f, m_bx = 0.0f, m_cx = 1.0f;
    float m_ay = 0.0f, m_by = 0.0f, m_cy = 1.0f;
};

// CSS preset curves.
inline CubicBezierEasing easeCurve()      { return CubicBezierEasing(0.25f, 0.1f, 0.25f, 1.0f); }
inline CubicBezierEasing easeInCurve()    { return CubicBezierEasing(0.42f, 0.0f, 1.0f, 1.0f); }
inline CubicBezierEasing easeOutCurve()   { return CubicBezierEasing(0.0f, 0.0f, 0.58f, 1.0f); }
inline CubicBezierEasing easeInOutCurve() { return CubicBezierEasing(0.42f, 0.0f, 0.58f, 1.0f); }

} // namespace maz::anim
