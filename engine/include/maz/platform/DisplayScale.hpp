#pragma once

#include <cmath>
#include <cstdint>

// maz::platform HiDPI display scaling — the pure math for turning between LOGICAL coordinates (the
// size you lay UI out in, e.g. "a 200pt-wide button") and PHYSICAL pixels (what the GPU actually
// rasterizes). On a HiDPI / Retina display the OS reports a content scale > 1 (1.5, 2.0, …); a
// window that is 1280×720 logical points is 2560×1440 real pixels at 2.0. Getting this wrong makes
// UI microscopic on a 4K laptop or blurry when it's up-scaled. Godot handles this via
// `content_scale_factor` / display DPI; here it's a small, deterministic, unit-tested helper — the
// SDL side (querying the live scale) lives in platform::Window (`contentScale()`).
namespace maz::platform {

// A content scale of 0 or less is meaningless; treat it as 1.0 (no scaling).
inline float sanitizeScale(float scale) {
    return (scale > 0.0f && std::isfinite(scale)) ? scale : 1.0f;
}

// Logical points -> physical pixels (round to nearest whole pixel).
inline int logicalToPixels(float logical, float scale) {
    return static_cast<int>(std::lround(logical * sanitizeScale(scale)));
}

// Physical pixels -> logical points.
inline float pixelsToLogical(int pixels, float scale) {
    return static_cast<float>(pixels) / sanitizeScale(scale);
}

// Scale a logical WxH size to a physical pixel size (both dimensions rounded).
struct PixelSize {
    int width = 0;
    int height = 0;
};
inline PixelSize scaledSize(int logicalW, int logicalH, float scale) {
    const float s = sanitizeScale(scale);
    return {static_cast<int>(std::lround(static_cast<float>(logicalW) * s)),
            static_cast<int>(std::lround(static_cast<float>(logicalH) * s))};
}

} // namespace maz::platform
