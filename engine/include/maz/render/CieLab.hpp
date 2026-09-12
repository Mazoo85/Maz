#pragma once

#include "maz/render/Renderer.hpp" // render::Color {r,g,b,a} — linear RGBA

#include <cmath>

// maz::render CIELAB perceptual colour space + colour-difference (Delta-E).
//
// The engine already has HSV/HSL, hex, and sRGB<->linear (ColorOps.hpp). Those are *device* spaces:
// equal numeric steps do NOT look like equal perceptual steps, so gradients band, palette reduction
// picks the wrong "nearest" colour, and "are these two colours the same?" has no honest threshold.
// CIELAB (CIE L*a*b*, 1976) is a *perceptual* space built on human vision: L* is lightness 0..100,
// a* is green(-)/red(+), b* is blue(-)/yellow(+), and Euclidean-ish distance tracks how different two
// colours actually look. This is what professional tools use for accurate gradient interpolation,
// perceptual colour quantisation, palette matching, and accessibility contrast work.
//
// render::Color here is LINEAR RGB in the sRGB primaries (see ColorOps.hpp), so the pipeline is
// linear-RGB -> CIE XYZ (D65) -> L*a*b*, with the exact inverse for the round trip. Two difference
// metrics are provided: deltaE76 (fast Euclidean, CIE 1976) and deltaE2000 (CIEDE2000, the modern
// perceptual standard with lightness/chroma/hue weighting and the blue-region rotation term).
// Pure value maths, header-only, deterministic — unit-tested against sRGB identities, round trips,
// and the published Sharma-Wu-Dalal CIEDE2000 reference pairs.
namespace maz::render {

struct Xyz {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Lab {
    float L = 0.0f; // lightness   0 (black) .. 100 (white)
    float a = 0.0f; // green(-) .. red(+)
    float b = 0.0f; // blue(-)  .. yellow(+)
};

// D65 reference white (2-degree observer), the sRGB illuminant.
namespace detail {
constexpr float kXn = 0.95047f;
constexpr float kYn = 1.00000f;
constexpr float kZn = 1.08883f;

// CIELAB companding f(t) and its inverse, with the linear segment near black (delta = 6/29).
inline float labF(float t) {
    constexpr float d = 6.0f / 29.0f;
    return t > d * d * d ? std::cbrt(t) : t / (3.0f * d * d) + 4.0f / 29.0f;
}
inline float labFinv(float t) {
    constexpr float d = 6.0f / 29.0f;
    return t > d ? t * t * t : 3.0f * d * d * (t - 4.0f / 29.0f);
}
} // namespace detail

// Linear sRGB-primaries RGB -> CIE XYZ (D65). Standard sRGB matrix.
inline Xyz linearRgbToXyz(const Color& c) {
    Xyz o;
    o.x = 0.4124564f * c.r + 0.3575761f * c.g + 0.1804375f * c.b;
    o.y = 0.2126729f * c.r + 0.7151522f * c.g + 0.0721750f * c.b;
    o.z = 0.0193339f * c.r + 0.1191920f * c.g + 0.9503041f * c.b;
    return o;
}

// CIE XYZ (D65) -> linear sRGB-primaries RGB (inverse matrix). Alpha is passed straight through.
inline Color xyzToLinearRgb(const Xyz& v, float alpha = 1.0f) {
    Color c;
    c.r = 3.2404542f * v.x - 1.5371385f * v.y - 0.4985314f * v.z;
    c.g = -0.9692660f * v.x + 1.8760108f * v.y + 0.0415560f * v.z;
    c.b = 0.0556434f * v.x - 0.2040259f * v.y + 1.0572252f * v.z;
    c.a = alpha;
    return c;
}

inline Lab xyzToLab(const Xyz& v) {
    const float fx = detail::labF(v.x / detail::kXn);
    const float fy = detail::labF(v.y / detail::kYn);
    const float fz = detail::labF(v.z / detail::kZn);
    Lab o;
    o.L = 116.0f * fy - 16.0f;
    o.a = 500.0f * (fx - fy);
    o.b = 200.0f * (fy - fz);
    return o;
}

inline Xyz labToXyz(const Lab& lab) {
    const float fy = (lab.L + 16.0f) / 116.0f;
    const float fx = fy + lab.a / 500.0f;
    const float fz = fy - lab.b / 200.0f;
    Xyz o;
    o.x = detail::kXn * detail::labFinv(fx);
    o.y = detail::kYn * detail::labFinv(fy);
    o.z = detail::kZn * detail::labFinv(fz);
    return o;
}

// Linear render::Color -> L*a*b* and back (the composed conversions callers actually use).
inline Lab toLab(const Color& c) { return xyzToLab(linearRgbToXyz(c)); }
inline Color fromLab(const Lab& lab, float alpha = 1.0f) {
    return xyzToLinearRgb(labToXyz(lab), alpha);
}

// CIE76 colour difference: plain Euclidean distance in L*a*b*. Fast; ~2.3 is a "just noticeable"
// difference in the reference region. Good enough for nearest-colour palette matching.
inline float deltaE76(const Lab& p, const Lab& q) {
    const float dL = p.L - q.L, da = p.a - q.a, db = p.b - q.b;
    return std::sqrt(dL * dL + da * da + db * db);
}

// CIEDE2000 colour difference (Sharma, Wu, Dalal 2005). The modern perceptual standard: corrects
// CIE76's non-uniformity with lightness/chroma/hue weighting functions and a rotation term that
// tames the blue region. Computed in double for the trig/pow precision the formula needs, returned
// as float. Verified against the published reference test pairs.
inline float deltaE2000(const Lab& c1, const Lab& c2) {
    constexpr double pi = 3.14159265358979323846;
    const double L1 = c1.L, a1 = c1.a, b1 = c1.b;
    const double L2 = c2.L, a2 = c2.a, b2 = c2.b;

    const double C1 = std::sqrt(a1 * a1 + b1 * b1);
    const double C2 = std::sqrt(a2 * a2 + b2 * b2);
    const double Cbar = 0.5 * (C1 + C2);
    const double Cbar7 = std::pow(Cbar, 7.0);
    const double G = 0.5 * (1.0 - std::sqrt(Cbar7 / (Cbar7 + std::pow(25.0, 7.0))));

    const double a1p = (1.0 + G) * a1;
    const double a2p = (1.0 + G) * a2;
    const double C1p = std::sqrt(a1p * a1p + b1 * b1);
    const double C2p = std::sqrt(a2p * a2p + b2 * b2);

    auto hueDeg = [&](double ap, double bp) {
        if (ap == 0.0 && bp == 0.0) return 0.0;
        double h = std::atan2(bp, ap) * 180.0 / pi;
        return h < 0.0 ? h + 360.0 : h;
    };
    const double h1p = hueDeg(a1p, b1);
    const double h2p = hueDeg(a2p, b2);

    const double dLp = L2 - L1;
    const double dCp = C2p - C1p;

    double dhp = 0.0;
    if (C1p * C2p != 0.0) {
        dhp = h2p - h1p;
        if (dhp > 180.0) dhp -= 360.0;
        else if (dhp < -180.0) dhp += 360.0;
    }
    const double dHp = 2.0 * std::sqrt(C1p * C2p) * std::sin((dhp * pi / 180.0) / 2.0);

    const double Lbarp = 0.5 * (L1 + L2);
    const double Cbarp = 0.5 * (C1p + C2p);

    double hbarp = h1p + h2p;
    if (C1p * C2p != 0.0) {
        if (std::fabs(h1p - h2p) > 180.0) {
            if (hbarp < 360.0) hbarp += 360.0;
            else hbarp -= 360.0;
        }
        hbarp *= 0.5;
    }

    const double T = 1.0 - 0.17 * std::cos((hbarp - 30.0) * pi / 180.0) +
                     0.24 * std::cos((2.0 * hbarp) * pi / 180.0) +
                     0.32 * std::cos((3.0 * hbarp + 6.0) * pi / 180.0) -
                     0.20 * std::cos((4.0 * hbarp - 63.0) * pi / 180.0);

    const double dTheta = 30.0 * std::exp(-((hbarp - 275.0) / 25.0) * ((hbarp - 275.0) / 25.0));
    const double Cbarp7 = std::pow(Cbarp, 7.0);
    const double Rc = 2.0 * std::sqrt(Cbarp7 / (Cbarp7 + std::pow(25.0, 7.0)));
    const double Sl =
        1.0 + (0.015 * (Lbarp - 50.0) * (Lbarp - 50.0)) / std::sqrt(20.0 + (Lbarp - 50.0) * (Lbarp - 50.0));
    const double Sc = 1.0 + 0.045 * Cbarp;
    const double Sh = 1.0 + 0.015 * Cbarp * T;
    const double Rt = -std::sin(2.0 * dTheta * pi / 180.0) * Rc;

    const double kL = 1.0, kC = 1.0, kH = 1.0;
    const double termL = dLp / (kL * Sl);
    const double termC = dCp / (kC * Sc);
    const double termH = dHp / (kH * Sh);
    const double d = std::sqrt(termL * termL + termC * termC + termH * termH + Rt * termC * termH);
    return static_cast<float>(d);
}

} // namespace maz::render
