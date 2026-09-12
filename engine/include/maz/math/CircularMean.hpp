#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math circular statistics — the CORRECT way to average and measure the spread of angles. You cannot
// arithmetically average angles: the mean of 350° and 10° is NOT 180°, it is 0°, because angles wrap. The
// circular mean fixes this by treating each angle as a unit vector, averaging the vectors, and taking the
// resulting direction (atan2 of the summed sin/cos). Games need this constantly: averaging the FACING of a
// flock or squad, a smoothed heading from noisy inputs, wind or current direction, wave phases, or a
// gyroscope/compass reading. The paired "resultant length" R in [0,1] measures how CONCENTRATED the angles
// are (1 = all identical, 0 = evenly spread with no meaningful mean), giving circular variance (1-R) and a
// circular standard deviation. Distinct from the engine's lerpAngle / shortestAngle (which interpolate a
// PAIR): this reduces a whole SET of angles. Radians in, radians out (result in (-pi, pi]). Header-only,
// std-only, deterministic. Godot has no circular-statistics helper.
namespace maz::math {

// Length of the mean resultant vector, R in [0,1]: 1 when all angles coincide, ~0 when evenly spread.
inline float resultantLength(const std::vector<float>& angles) {
    if (angles.empty()) {
        return 0.0f;
    }
    double sx = 0.0, sy = 0.0;
    for (float a : angles) {
        sx += std::cos(static_cast<double>(a));
        sy += std::sin(static_cast<double>(a));
    }
    const double n = static_cast<double>(angles.size());
    return static_cast<float>(std::sqrt(sx * sx + sy * sy) / n);
}

// Circular mean of a set of angles (radians), returned in (-pi, pi]. Empty -> 0.
inline float circularMean(const std::vector<float>& angles) {
    if (angles.empty()) {
        return 0.0f;
    }
    double sx = 0.0, sy = 0.0;
    for (float a : angles) {
        sx += std::cos(static_cast<double>(a));
        sy += std::sin(static_cast<double>(a));
    }
    return static_cast<float>(std::atan2(sy, sx));
}

// Weighted circular mean: each angle contributes proportionally to its (non-negative) weight.
inline float circularMeanWeighted(const std::vector<float>& angles, const std::vector<float>& weights) {
    if (angles.empty() || angles.size() != weights.size()) {
        return 0.0f;
    }
    double sx = 0.0, sy = 0.0;
    for (std::size_t i = 0; i < angles.size(); ++i) {
        const double w = weights[i] < 0.0f ? 0.0 : static_cast<double>(weights[i]);
        sx += w * std::cos(static_cast<double>(angles[i]));
        sy += w * std::sin(static_cast<double>(angles[i]));
    }
    if (sx == 0.0 && sy == 0.0) {
        return 0.0f;
    }
    return static_cast<float>(std::atan2(sy, sx));
}

// Circular variance in [0,1]: 0 when all angles coincide, 1 when there is no concentration.
inline float circularVariance(const std::vector<float>& angles) {
    return 1.0f - resultantLength(angles);
}

// Circular standard deviation (radians): 0 for identical angles, grows without bound as R -> 0.
inline float circularStdDev(const std::vector<float>& angles) {
    const float r = resultantLength(angles);
    if (r <= 0.0f) {
        return std::sqrt(-2.0f * std::log(1e-12f)); // effectively "maximum spread"
    }
    if (r >= 1.0f) {
        return 0.0f;
    }
    return std::sqrt(-2.0f * std::log(r));
}

} // namespace maz::math
