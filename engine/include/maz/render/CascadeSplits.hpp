#pragma once

#include <cmath>
#include <vector>

// maz::render cascaded shadow-map splits — a single directional-light shadow map can't cover a huge
// view distance without either blurring near geometry or wasting all its resolution far away.
// Cascaded Shadow Maps (CSM) fix this by slicing the camera frustum along depth into N cascades,
// each with its OWN shadow map: the near cascade gets crisp, tight coverage, far cascades cover
// more world per texel. The key decision is WHERE to cut — the "split distances". This is the
// standard Practical Split Scheme (Zhang et al., the PSSM `lambda` blend), pure and unit-tested;
// the actual per-cascade depth passes + shader selection are the GPU half (Godot's
// DirectionalLight3D shadow "4 splits" is the same idea).
namespace maz::render {

// Far distance (view-space, positive) of each of `count` cascades. The last equals farZ. Cascade i
// spans (splitFar[i-1], splitFar[i]] with cascade 0 starting at nearZ. `lambda` blends uniform
// (0.0 — even world-space slices) and logarithmic (1.0 — even perspective/texel distribution)
// splitting; ~0.5 is the usual practical default. nearZ must be > 0 and < farZ.
inline std::vector<float> cascadeSplits(float nearZ, float farZ, int count, float lambda = 0.5f) {
    std::vector<float> splits;
    if (count <= 0 || !(nearZ > 0.0f) || !(farZ > nearZ)) {
        return splits;
    }
    if (lambda < 0.0f) {
        lambda = 0.0f;
    } else if (lambda > 1.0f) {
        lambda = 1.0f;
    }
    splits.reserve(static_cast<size_t>(count));
    const float range = farZ - nearZ;
    const float ratio = farZ / nearZ;
    for (int i = 1; i <= count; ++i) {
        const float p = static_cast<float>(i) / static_cast<float>(count);
        const float uniform = nearZ + range * p;
        const float logarithmic = nearZ * std::pow(ratio, p);
        float d = lambda * logarithmic + (1.0f - lambda) * uniform;
        if (i == count) {
            d = farZ; // pin the last split exactly to the far plane (no float drift)
        }
        splits.push_back(d);
    }
    return splits;
}

// Convenience: contiguous [near, far] view-space ranges per cascade (near of 0 = nearZ; near of i =
// far of i-1). Empty on invalid input.
struct CascadeRange {
    float nearZ = 0.0f;
    float farZ = 0.0f;
};
inline std::vector<CascadeRange> cascadeRanges(float nearZ, float farZ, int count,
                                               float lambda = 0.5f) {
    std::vector<CascadeRange> ranges;
    const std::vector<float> splits = cascadeSplits(nearZ, farZ, count, lambda);
    if (splits.empty()) {
        return ranges;
    }
    ranges.reserve(splits.size());
    float prev = nearZ;
    for (float s : splits) {
        ranges.push_back({prev, s});
        prev = s;
    }
    return ranges;
}

} // namespace maz::render
