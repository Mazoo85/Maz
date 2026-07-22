#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math arc-length reparameterization for curves — the "move at constant speed along a path" tool.
// A spline or Bézier is naturally parameterized by u in [0,1], but equal steps in u do NOT cover equal
// distance: an object animated by raw u races along straight sections and crawls through tight curves.
// Arc-length reparameterization fixes this: sample the curve densely, build a cumulative chord-length
// table, then map DISTANCE back to the curve parameter (and vice versa). Feed it the points from
// CatmullRomSpline::tessellate or Curve2D and you can drive a camera/enemy/projectile along the path at
// a uniform speed, or place N evenly-spaced points along it. Pure vec2 math over a precomputed table —
// exactly unit-testable (the distance<->parameter round-trip is exact, and a straight curve maps
// distance directly to parameter).
namespace maz::math {

class ArcLengthTable {
  public:
    ArcLengthTable() = default;
    explicit ArcLengthTable(const std::vector<vec2>& samples) { build(samples); }

    // Build from curve samples taken at EQUALLY SPACED parameter values u in [0,1] (samples[0] at u=0,
    // samples.back() at u=1). Needs >= 2 samples.
    void build(const std::vector<vec2>& samples) {
        m_cum.clear();
        m_count = samples.size();
        if (m_count < 2) {
            m_cum.assign(m_count, 0.0f);
            return;
        }
        m_cum.resize(m_count);
        m_cum[0] = 0.0f;
        for (std::size_t i = 1; i < m_count; ++i) {
            const vec2 d = samples[i] - samples[i - 1];
            m_cum[i] = m_cum[i - 1] + std::sqrt(d.x * d.x + d.y * d.y);
        }
    }

    float totalLength() const { return m_count >= 2 ? m_cum.back() : 0.0f; }
    std::size_t sampleCount() const { return m_count; }

    // Curve parameter u in [0,1] at a given arc-length distance from the start (clamped to [0,length]).
    float parameterAtDistance(float distance) const {
        if (m_count < 2) {
            return 0.0f;
        }
        const float total = m_cum.back();
        if (total <= 0.0f) {
            return 0.0f;
        }
        if (distance <= 0.0f) {
            return 0.0f;
        }
        if (distance >= total) {
            return 1.0f;
        }
        // Binary search for the segment whose cumulative range contains `distance`.
        std::size_t lo = 0;
        std::size_t hi = m_count - 1;
        while (lo + 1 < hi) {
            const std::size_t mid = (lo + hi) / 2;
            if (m_cum[mid] <= distance) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        const float segLen = m_cum[hi] - m_cum[lo];
        const float frac = segLen > 0.0f ? (distance - m_cum[lo]) / segLen : 0.0f;
        const float denom = static_cast<float>(m_count - 1);
        return (static_cast<float>(lo) + frac) / denom;
    }

    // Convenience: parameter at a fraction f in [0,1] of the total length.
    float parameterAtFraction(float f) const { return parameterAtDistance(f * totalLength()); }

    // Arc-length distance from the start to curve parameter u in [0,1].
    float distanceAtParameter(float u) const {
        if (m_count < 2) {
            return 0.0f;
        }
        if (u <= 0.0f) {
            return 0.0f;
        }
        if (u >= 1.0f) {
            return m_cum.back();
        }
        const float scaled = u * static_cast<float>(m_count - 1);
        std::size_t i = static_cast<std::size_t>(scaled);
        if (i >= m_count - 1) {
            i = m_count - 2;
        }
        const float frac = scaled - static_cast<float>(i);
        return m_cum[i] + frac * (m_cum[i + 1] - m_cum[i]);
    }

    // Curve parameters for `n` points spaced at EQUAL arc-length intervals (n >= 2, includes both ends).
    std::vector<float> equalArcParameters(int n) const {
        std::vector<float> out;
        if (n < 2 || m_count < 2) {
            return out;
        }
        out.reserve(static_cast<std::size_t>(n));
        for (int k = 0; k < n; ++k) {
            const float f = static_cast<float>(k) / static_cast<float>(n - 1);
            out.push_back(parameterAtFraction(f));
        }
        return out;
    }

  private:
    std::vector<float> m_cum; // cumulative chord length at each sample
    std::size_t m_count = 0;
};

} // namespace maz::math
