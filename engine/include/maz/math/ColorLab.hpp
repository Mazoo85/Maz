#pragma once

#include "maz/math/Math.hpp" // vec3

#include <cmath>

// maz::math CIE color science — convert LINEAR RGB (the engine's working colour space) to CIE XYZ and then to
// CIELAB (L*a*b*), plus the CIE76 perceptual colour difference (Delta-E). CIELAB is the classic
// PERCEPTUALLY-UNIFORM colour space: equal numeric steps look like equal visual steps, so it is the right
// space for measuring "how different do these two colours look?" — palette matching, colour quantisation /
// nearest-swatch, gradient generation, and accessibility (perceptual contrast). L* is lightness 0..100, a*
// is green(−)↔red(+), b* is blue(−)↔yellow(+). This complements the engine's Oklab (ColorOps.hpp) with the
// long-standing CIE standard. Godot exposes no Lab/Delta-E. Uses the sRGB/Rec.709 primaries at the D65 white
// point. Header-only, std-only, deterministic.
namespace maz::math {

// Linear RGB (Rec.709 primaries) -> CIE XYZ, D65.
inline vec3 linearRgbToXyz(const vec3& c) {
    return vec3(0.4124564f * c.x + 0.3575761f * c.y + 0.1804375f * c.z,
                0.2126729f * c.x + 0.7151522f * c.y + 0.0721750f * c.z,
                0.0193339f * c.x + 0.1191920f * c.y + 0.9503041f * c.z);
}

// CIE XYZ -> linear RGB (inverse of linearRgbToXyz).
inline vec3 xyzToLinearRgb(const vec3& c) {
    return vec3(3.2404542f * c.x - 1.5371385f * c.y - 0.4985314f * c.z,
                -0.9692660f * c.x + 1.8760108f * c.y + 0.0415560f * c.z,
                0.0556434f * c.x - 0.2040259f * c.y + 1.0572252f * c.z);
}

namespace lab_detail {
constexpr float kXn = 0.95047f, kYn = 1.0f, kZn = 1.08883f; // D65 reference white
constexpr float kDelta = 6.0f / 29.0f;
inline float f(float t) {
    return t > kDelta * kDelta * kDelta ? std::cbrt(t) : t / (3.0f * kDelta * kDelta) + 4.0f / 29.0f;
}
inline float fInv(float t) {
    return t > kDelta ? t * t * t : 3.0f * kDelta * kDelta * (t - 4.0f / 29.0f);
}
} // namespace lab_detail

// CIE XYZ -> CIELAB (L in [0,100], a/b typically in [-128,127]).
inline vec3 xyzToLab(const vec3& xyz) {
    using namespace lab_detail;
    const float fx = f(xyz.x / kXn), fy = f(xyz.y / kYn), fz = f(xyz.z / kZn);
    return vec3(116.0f * fy - 16.0f, 500.0f * (fx - fy), 200.0f * (fy - fz));
}

// CIELAB -> CIE XYZ (inverse of xyzToLab).
inline vec3 labToXyz(const vec3& lab) {
    using namespace lab_detail;
    const float fy = (lab.x + 16.0f) / 116.0f;
    const float fx = fy + lab.y / 500.0f;
    const float fz = fy - lab.z / 200.0f;
    return vec3(kXn * fInv(fx), kYn * fInv(fy), kZn * fInv(fz));
}

// Convenience: linear RGB <-> CIELAB.
inline vec3 linearRgbToLab(const vec3& c) { return xyzToLab(linearRgbToXyz(c)); }
inline vec3 labToLinearRgb(const vec3& lab) { return xyzToLinearRgb(labToXyz(lab)); }

// CIE76 perceptual colour difference: the Euclidean distance in L*a*b*. ~2.3 is the "just noticeable
// difference" threshold. Inputs are Lab triples.
inline float deltaE76(const vec3& labA, const vec3& labB) {
    const float dL = labA.x - labB.x, da = labA.y - labB.y, db = labA.z - labB.z;
    return std::sqrt(dL * dL + da * da + db * db);
}

} // namespace maz::math
