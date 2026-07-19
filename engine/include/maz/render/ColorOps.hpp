#pragma once

#include "maz/render/Renderer.hpp" // render::Color {r,g,b,a}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

// maz::render colour operations — the maths behind Godot's Color type and ColorPicker. render::Color is
// a plain linear RGBA float; this adds the conversions and tweaks a picker/theme needs: HSV <-> RGB,
// hex (#rrggbb / #rrggbbaa) parse+format, lighten/darken, lerp, invert, perceptual luminance, and the
// sRGB<->linear transfer functions. All pure value maths, header-only, deterministic — unit-tests
// exactly against known colour identities.
namespace maz::render {

struct Hsv {
    float h = 0.0f; // hue in [0,1)  (matches Godot Color.from_hsv / get_h)
    float s = 0.0f; // saturation [0,1]
    float v = 0.0f; // value [0,1]
    float a = 1.0f;
};

// HSV (h in [0,1)) -> linear RGBA.
inline Color fromHsv(float h, float s, float v, float a = 1.0f) {
    h -= std::floor(h);        // wrap hue into [0,1)
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
    const float hf = h * 6.0f;
    const int i = static_cast<int>(std::floor(hf)) % 6;
    const float f = hf - std::floor(hf);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    Color c;
    c.a = a;
    switch (i) {
    case 0: c.r = v; c.g = t; c.b = p; break;
    case 1: c.r = q; c.g = v; c.b = p; break;
    case 2: c.r = p; c.g = v; c.b = t; break;
    case 3: c.r = p; c.g = q; c.b = v; break;
    case 4: c.r = t; c.g = p; c.b = v; break;
    default: c.r = v; c.g = p; c.b = q; break;
    }
    return c;
}
inline Color fromHsv(const Hsv& hsv) { return fromHsv(hsv.h, hsv.s, hsv.v, hsv.a); }

// RGBA -> HSV (h in [0,1)).
inline Hsv toHsv(const Color& c) {
    const float mx = std::max(c.r, std::max(c.g, c.b));
    const float mn = std::min(c.r, std::min(c.g, c.b));
    const float d = mx - mn;
    Hsv out;
    out.a = c.a;
    out.v = mx;
    out.s = mx > 1e-8f ? d / mx : 0.0f;
    if (d < 1e-8f) {
        out.h = 0.0f;
        return out;
    }
    float h;
    if (mx == c.r) {
        h = (c.g - c.b) / d + (c.g < c.b ? 6.0f : 0.0f);
    } else if (mx == c.g) {
        h = (c.b - c.r) / d + 2.0f;
    } else {
        h = (c.r - c.g) / d + 4.0f;
    }
    out.h = h / 6.0f;
    return out;
}

namespace detail {
inline int hexNybble(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}
inline char hexDigit(int v) { return "0123456789abcdef"[v & 0xF]; }
} // namespace detail

// Parse a hex colour: "#rgb", "#rgba", "#rrggbb", "#rrggbbaa" (leading '#' optional). Returns false on
// malformed input (and leaves `out` unchanged). Channels are treated as sRGB byte values in [0,255].
inline bool fromHtml(const std::string& text, Color& out) {
    std::string s = text;
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }
    auto n = [&](std::size_t i) { return detail::hexNybble(s[i]); };
    float r, g, b, a = 1.0f;
    if (s.size() == 3 || s.size() == 4) {
        for (char ch : s) {
            if (detail::hexNybble(ch) < 0) return false;
        }
        r = static_cast<float>(n(0) * 17) / 255.0f;
        g = static_cast<float>(n(1) * 17) / 255.0f;
        b = static_cast<float>(n(2) * 17) / 255.0f;
        if (s.size() == 4) a = static_cast<float>(n(3) * 17) / 255.0f;
    } else if (s.size() == 6 || s.size() == 8) {
        for (char ch : s) {
            if (detail::hexNybble(ch) < 0) return false;
        }
        r = static_cast<float>(n(0) * 16 + n(1)) / 255.0f;
        g = static_cast<float>(n(2) * 16 + n(3)) / 255.0f;
        b = static_cast<float>(n(4) * 16 + n(5)) / 255.0f;
        if (s.size() == 8) a = static_cast<float>(n(6) * 16 + n(7)) / 255.0f;
    } else {
        return false;
    }
    out.r = r;
    out.g = g;
    out.b = b;
    out.a = a;
    return true;
}

// Format as lowercase hex WITHOUT a leading '#': "rrggbb", or "rrggbbaa" when withAlpha.
inline std::string toHtml(const Color& c, bool withAlpha = false) {
    auto byte = [](float x) {
        const int v = static_cast<int>(std::lround(std::clamp(x, 0.0f, 1.0f) * 255.0f));
        return v < 0 ? 0 : (v > 255 ? 255 : v);
    };
    auto put = [&](std::string& s, int v) {
        s.push_back(detail::hexDigit((v >> 4) & 0xF));
        s.push_back(detail::hexDigit(v & 0xF));
    };
    std::string s;
    put(s, byte(c.r));
    put(s, byte(c.g));
    put(s, byte(c.b));
    if (withAlpha) {
        put(s, byte(c.a));
    }
    return s;
}

// Blend toward white by `amount` in [0,1] (Godot Color.lightened).
inline Color lightened(const Color& c, float amount) {
    Color o;
    o.r = c.r + (1.0f - c.r) * amount;
    o.g = c.g + (1.0f - c.g) * amount;
    o.b = c.b + (1.0f - c.b) * amount;
    o.a = c.a;
    return o;
}
// Scale toward black by `amount` in [0,1] (Godot Color.darkened).
inline Color darkened(const Color& c, float amount) {
    const float k = 1.0f - amount;
    return Color{c.r * k, c.g * k, c.b * k, c.a};
}

inline Color lerpColor(const Color& a, const Color& b, float t) {
    return Color{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                 a.a + (b.a - a.a) * t};
}

// Invert RGB, keep alpha (Godot Color.inverted).
inline Color inverted(const Color& c) { return Color{1.0f - c.r, 1.0f - c.g, 1.0f - c.b, c.a}; }

// Rec.709 perceptual luminance (Godot Color.get_luminance).
inline float luminance(const Color& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

// sRGB transfer functions (per channel).
inline float srgbToLinear(float x) {
    return x <= 0.04045f ? x / 12.92f : std::pow((x + 0.055f) / 1.055f, 2.4f);
}
inline float linearToSrgb(float x) {
    return x <= 0.0031308f ? x * 12.92f : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}
inline Color srgbToLinear(const Color& c) {
    return Color{srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b), c.a};
}
inline Color linearToSrgb(const Color& c) {
    return Color{linearToSrgb(c.r), linearToSrgb(c.g), linearToSrgb(c.b), c.a};
}

// ---- Color completeness (M273) — the remaining Godot Color methods ----------------------------

namespace detail {
// Quantise a [0,1] channel to a [0,255] byte with rounding (matches Godot's channel packing).
inline std::uint32_t to255(float x) {
    const long v = std::lround(std::clamp(x, 0.0f, 1.0f) * 255.0f);
    return static_cast<std::uint32_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}
} // namespace detail

// Alpha-composite `over` on top of `base` (source-over) — Godot's Color.blend. Fully transparent
// `over` leaves `base`; fully opaque `over` replaces it. Returns transparent black if both vanish.
inline Color blend(const Color& base, const Color& over) {
    const float sa = 1.0f - over.a;
    Color res;
    res.a = base.a * sa + over.a;
    if (res.a < 1e-8f) {
        return Color{0.0f, 0.0f, 0.0f, 0.0f};
    }
    res.r = (base.r * base.a * sa + over.r * over.a) / res.a;
    res.g = (base.g * base.a * sa + over.g * over.a) / res.a;
    res.b = (base.b * base.a * sa + over.b * over.a) / res.a;
    return res;
}

// Component-wise clamp into [lo, hi] (defaults to the [0,1] display range) — Godot's Color.clamp.
inline Color clampColor(const Color& c, const Color& lo = Color{0, 0, 0, 0},
                        const Color& hi = Color{1, 1, 1, 1}) {
    return Color{std::clamp(c.r, lo.r, hi.r), std::clamp(c.g, lo.g, hi.g),
                 std::clamp(c.b, lo.b, hi.b), std::clamp(c.a, lo.a, hi.a)};
}

// Approximate equality across all four channels (absolute epsilon) — Godot's Color.is_equal_approx.
inline bool isEqualApprox(const Color& a, const Color& b, float eps = 1e-5f) {
    return std::fabs(a.r - b.r) < eps && std::fabs(a.g - b.g) < eps && std::fabs(a.b - b.b) < eps &&
           std::fabs(a.a - b.a) < eps;
}

// Pack to a 32-bit integer with the byte order named by the function (Godot's to_*32). RGBA puts red
// in the highest byte; ARGB puts alpha highest; ABGR puts alpha highest then B,G,R.
inline std::uint32_t toRgba32(const Color& c) {
    return (detail::to255(c.r) << 24) | (detail::to255(c.g) << 16) | (detail::to255(c.b) << 8) |
           detail::to255(c.a);
}
inline std::uint32_t toArgb32(const Color& c) {
    return (detail::to255(c.a) << 24) | (detail::to255(c.r) << 16) | (detail::to255(c.g) << 8) |
           detail::to255(c.b);
}
inline std::uint32_t toAbgr32(const Color& c) {
    return (detail::to255(c.a) << 24) | (detail::to255(c.b) << 16) | (detail::to255(c.g) << 8) |
           detail::to255(c.r);
}

// Unpack an RGBA-ordered 32-bit integer (red in the highest byte) — Godot's Color(uint32) via rgba32.
inline Color fromRgba32(std::uint32_t v) {
    return Color{static_cast<float>((v >> 24) & 0xFF) / 255.0f,
                 static_cast<float>((v >> 16) & 0xFF) / 255.0f,
                 static_cast<float>((v >> 8) & 0xFF) / 255.0f,
                 static_cast<float>(v & 0xFF) / 255.0f};
}

// Build a Color from 0..255 byte channels (Godot's Color8).
inline Color color8(int r, int g, int b, int a = 255) {
    return Color{static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                 static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f};
}

// Individual 0..255 channel values — Godot's Color.get_r8 / g8 / b8 / a8. Each is the [0,1] channel
// rounded to the nearest byte (clamped to 0..255, matching the engine's channel packing).
inline int r8(const Color& c) { return static_cast<int>(detail::to255(c.r)); }
inline int g8(const Color& c) { return static_cast<int>(detail::to255(c.g)); }
inline int b8(const Color& c) { return static_cast<int>(detail::to255(c.b)); }
inline int a8(const Color& c) { return static_cast<int>(detail::to255(c.a)); }

// Pack to a 64-bit integer with 16 bits per channel, RGBA order (red in the highest word) — Godot's
// Color.to_rgba64. Each channel is quantised to 0..65535, giving high-bit-depth round-tripping.
inline std::uint64_t toRgba64(const Color& c) {
    auto q = [](float x) -> std::uint64_t {
        const float v = std::round(std::clamp(x, 0.0f, 1.0f) * 65535.0f);
        return static_cast<std::uint64_t>(v);
    };
    return (q(c.r) << 48) | (q(c.g) << 32) | (q(c.b) << 16) | q(c.a);
}

// Unpack an RGBA-ordered 64-bit integer (16 bits per channel, red highest) — Godot's Color.hex64.
inline Color fromRgba64(std::uint64_t v) {
    return Color{static_cast<float>((v >> 48) & 0xFFFF) / 65535.0f,
                 static_cast<float>((v >> 32) & 0xFFFF) / 65535.0f,
                 static_cast<float>((v >> 16) & 0xFFFF) / 65535.0f,
                 static_cast<float>(v & 0xFFFF) / 65535.0f};
}

// ---- OKLab / OKLCh perceptual colour space (M304) --------------------------------------------
//
// OKLab (Björn Ottosson, 2020) is the perceptually-uniform colour space that underpins Godot 4's
// OKHSL colour picker (Color.from_ok_hsl) and CSS Color 4's oklab()/oklch(). Equal steps in OKLab
// look like equal perceptual steps, so it is the correct space for lightening/darkening, generating
// palettes, and blending two colours without the muddy grey mid-point that linear- or sRGB-space
// mixing produces. render::Color here is LINEAR RGBA, which is exactly the input the OKLab matrices
// expect, so no sRGB decode happens inside these functions — feed a linear colour, get OKLab back.
//
// L is lightness (0 = black, ~1 = reference white); a is green(-)/red(+); b is blue(-)/yellow(+).
// OKLCh is the polar form of OKLab: the same L, plus chroma C (colourfulness, >= 0) and hue h in
// RADIANS. The transforms are exact inverses (round-trip to floating-point precision).

struct Oklab {
    float L = 0.0f; // lightness  [0, ~1]
    float a = 0.0f; // green(-) .. red(+)
    float b = 0.0f; // blue(-) .. yellow(+)
    float alpha = 1.0f;
};

struct Oklch {
    float L = 0.0f;     // lightness  [0, ~1]
    float C = 0.0f;     // chroma (colourfulness), >= 0
    float h = 0.0f;     // hue in radians, atan2(b, a)
    float alpha = 1.0f;
};

// Linear RGBA -> OKLab (Ottosson's M1 cone response + non-linearity + M2 matrix).
inline Oklab linearToOklab(const Color& c) {
    const float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
    const float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
    const float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;
    const float l_ = std::cbrt(l);
    const float m_ = std::cbrt(m);
    const float s_ = std::cbrt(s);
    return Oklab{0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
                 1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
                 0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_, c.a};
}

// OKLab -> linear RGBA (exact inverse of linearToOklab). May land slightly outside [0,1] for
// out-of-gamut OKLab values; clamp afterwards if a displayable colour is required.
inline Color oklabToLinear(const Oklab& c) {
    const float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
    const float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
    const float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;
    const float l = l_ * l_ * l_;
    const float m = m_ * m_ * m_;
    const float s = s_ * s_ * s_;
    return Color{4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
                 -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
                 -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s, c.alpha};
}

// OKLab <-> OKLCh (Cartesian <-> polar in the a/b plane). Hue is in radians.
inline Oklch oklabToOklch(const Oklab& c) {
    return Oklch{c.L, std::sqrt(c.a * c.a + c.b * c.b), std::atan2(c.b, c.a), c.alpha};
}
inline Oklab oklchToOklab(const Oklch& c) {
    return Oklab{c.L, c.C * std::cos(c.h), c.C * std::sin(c.h), c.alpha};
}

// Perceptually-uniform blend of two linear colours: mix in OKLab, not in RGB. t=0 -> a, t=1 -> b.
// This is what gives smooth, natural-looking gradients (the reason Godot 4 offers OKLab gradient
// interpolation). Result is a linear Color; clamp if you need it strictly in gamut.
inline Color oklabMix(const Color& a, const Color& b, float t) {
    const Oklab la = linearToOklab(a);
    const Oklab lb = linearToOklab(b);
    return oklabToLinear(Oklab{la.L + (lb.L - la.L) * t, la.a + (lb.a - la.a) * t,
                               la.b + (lb.b - la.b) * t, la.alpha + (lb.alpha - la.alpha) * t});
}

// ---- OKHSL colour space (M395) ---------------------------------------------------------------
//
// OKHSL (Björn Ottosson, 2021) is the hue/saturation/lightness model built on OKLab that Godot 4.3's
// colour picker uses (Color.from_ok_hsl / Color.ok_hsl_h/s/l). Unlike classic HSV, its lightness and
// saturation are perceptually even and its saturation is gamut-aware: s = 1 is the most saturated
// colour that still fits in sRGB for the given hue and lightness, so sliders never "clip". This is a
// faithful transcription of Ottosson's reference okhsl (the same code Godot ports); it reuses the
// already-tested OKLab matrices above. h, s, l are all in [0, 1]; h wraps. To stay consistent with
// maz's LINEAR render::Color convention, fromOkhsl returns a LINEAR colour and toOkhsl consumes one
// (Ottosson's final sRGB gamma step is intentionally omitted — the pipeline is linear throughout).
struct Okhsl {
    float h = 0.0f; // hue        [0, 1)
    float s = 0.0f; // saturation [0, 1]
    float l = 0.0f; // lightness  [0, 1]
    float alpha = 1.0f;
};

namespace detail {

inline float okToe(float x) {
    constexpr float k1 = 0.206f, k2 = 0.03f, k3 = (1.0f + k1) / (1.0f + k2);
    return 0.5f * (k3 * x - k1 + std::sqrt((k3 * x - k1) * (k3 * x - k1) + 4.0f * k2 * k3 * x));
}
inline float okToeInv(float x) {
    constexpr float k1 = 0.206f, k2 = 0.03f, k3 = (1.0f + k1) / (1.0f + k2);
    return (x * x + k1 * x) / (k3 * (x + k2));
}

// Max saturation for hue (a,b) before the first sRGB channel goes negative (Ottosson approximation).
inline float okComputeMaxSaturation(float a, float b) {
    float k0, k1, k2, k3, k4, wl, wm, ws;
    if (-1.88170328f * a - 0.80936493f * b > 1.0f) { // red
        k0 = 1.19086277f; k1 = 1.76576728f; k2 = 0.59662641f; k3 = 0.75515197f; k4 = 0.56771245f;
        wl = 4.0767416621f; wm = -3.3077115913f; ws = 0.2309699292f;
    } else if (1.81444104f * a - 1.19445276f * b > 1.0f) { // green
        k0 = 0.73956515f; k1 = -0.45954404f; k2 = 0.08285427f; k3 = 0.12541070f; k4 = 0.14503204f;
        wl = -1.2684380046f; wm = 2.6097574011f; ws = -0.3413193965f;
    } else { // blue
        k0 = 1.35733652f; k1 = -0.00915799f; k2 = -1.15130210f; k3 = -0.50559606f; k4 = 0.00692167f;
        wl = -0.0041960863f; wm = -0.7034186147f; ws = 1.7076147010f;
    }
    float S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;
    const float kl = 0.3963377774f * a + 0.2158037573f * b;
    const float km = -0.1055613458f * a - 0.0638541728f * b;
    const float ks = -0.0894841775f * a - 1.2914855480f * b;
    {
        const float l_ = 1.0f + S * kl, m_ = 1.0f + S * km, s_ = 1.0f + S * ks;
        const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
        const float ldS = 3.0f * kl * l_ * l_, mdS = 3.0f * km * m_ * m_, sdS = 3.0f * ks * s_ * s_;
        const float ldS2 = 6.0f * kl * kl * l_, mdS2 = 6.0f * km * km * m_, sdS2 = 6.0f * ks * ks * s_;
        const float f = wl * l + wm * m + ws * s;
        const float f1 = wl * ldS + wm * mdS + ws * sdS;
        const float f2 = wl * ldS2 + wm * mdS2 + ws * sdS2;
        S = S - f * f1 / (f1 * f1 - 0.5f * f * f2);
    }
    return S;
}

struct OkLC { float L; float C; };
struct OkST { float S; float T; };

// The cusp (most saturated point) of the sRGB gamut for hue (a,b), in OKLab L/C.
inline OkLC okFindCusp(float a, float b) {
    const float sCusp = okComputeMaxSaturation(a, b);
    const Color rgb = oklabToLinear(Oklab{1.0f, sCusp * a, sCusp * b, 1.0f});
    const float lCusp = std::cbrt(1.0f / std::max(std::max(rgb.r, rgb.g), rgb.b));
    return OkLC{lCusp, lCusp * sCusp};
}
inline OkST okToST(OkLC cusp) { return OkST{cusp.C / cusp.L, cusp.C / (1.0f - cusp.L)}; }

inline OkST okGetSTMid(float a, float b) {
    const float S = 0.11516993f +
        1.0f / (7.44778970f + 4.15901240f * b +
                a * (-2.19557347f + 1.75198401f * b +
                     a * (-2.13704948f - 10.02301043f * b +
                          a * (-4.24894561f + 5.38770819f * b + 4.69891013f * a))));
    const float T = 0.11239642f +
        1.0f / (1.61320320f - 0.68124379f * b +
                a * (0.40370612f + 0.90148123f * b +
                     a * (-0.27087943f + 0.61223990f * b +
                          a * (0.00299215f - 0.45399568f * b - 0.14661872f * a))));
    return OkST{S, T};
}

// Distance t (0..1) from (L0,0) toward (L1,C1) at which the sRGB gamut boundary is hit.
inline float okFindGamutIntersection(float a, float b, float L1, float C1, float L0, OkLC cusp) {
    float t;
    if (((L1 - L0) * cusp.C - (cusp.L - L0) * C1) <= 0.0f) {
        t = cusp.C * L0 / (C1 * cusp.L + cusp.C * (L0 - L1));
    } else {
        t = cusp.C * (L0 - 1.0f) / (C1 * (cusp.L - 1.0f) + cusp.C * (L0 - L1));
        const float dL = L1 - L0, dC = C1;
        const float kl = 0.3963377774f * a + 0.2158037573f * b;
        const float km = -0.1055613458f * a - 0.0638541728f * b;
        const float ks = -0.0894841775f * a - 1.2914855480f * b;
        const float ldt = dL + dC * kl, mdt = dL + dC * km, sdt = dL + dC * ks;
        {
            const float L = L0 * (1.0f - t) + t * L1;
            const float C = t * C1;
            const float l_ = L + C * kl, m_ = L + C * km, s_ = L + C * ks;
            const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
            const float ldt_ = 3.0f * ldt * l_ * l_, mdt_ = 3.0f * mdt * m_ * m_, sdt_ = 3.0f * sdt * s_ * s_;
            const float ldt2 = 6.0f * ldt * ldt * l_, mdt2 = 6.0f * mdt * mdt * m_, sdt2 = 6.0f * sdt * sdt * s_;
            const float r = 4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s - 1.0f;
            const float r1 = 4.0767416621f * ldt_ - 3.3077115913f * mdt_ + 0.2309699292f * sdt_;
            const float r2 = 4.0767416621f * ldt2 - 3.3077115913f * mdt2 + 0.2309699292f * sdt2;
            const float ur = r1 / (r1 * r1 - 0.5f * r * r2);
            float tr = -r * ur;
            const float g = -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s - 1.0f;
            const float g1 = -1.2684380046f * ldt_ + 2.6097574011f * mdt_ - 0.3413193965f * sdt_;
            const float g2 = -1.2684380046f * ldt2 + 2.6097574011f * mdt2 - 0.3413193965f * sdt2;
            const float ug = g1 / (g1 * g1 - 0.5f * g * g2);
            float tg = -g * ug;
            const float bb = -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s - 1.0f;
            const float b1 = -0.0041960863f * ldt_ - 0.7034186147f * mdt_ + 1.7076147010f * sdt_;
            const float b2 = -0.0041960863f * ldt2 - 0.7034186147f * mdt2 + 1.7076147010f * sdt2;
            const float ub = b1 / (b1 * b1 - 0.5f * bb * b2);
            float tb = -bb * ub;
            constexpr float big = 3.402823e+38f; // ~FLT_MAX
            tr = ur >= 0.0f ? tr : big;
            tg = ug >= 0.0f ? tg : big;
            tb = ub >= 0.0f ? tb : big;
            t += std::min(tr, std::min(tg, tb));
        }
    }
    return t;
}

struct OkCs { float C0; float CMid; float CMax; };
inline OkCs okGetCs(float L, float a, float b) {
    const OkLC cusp = okFindCusp(a, b);
    const float cMax = okFindGamutIntersection(a, b, L, 1.0f, L, cusp);
    const OkST stMax = okToST(cusp);
    const float k = cMax / std::min(L * stMax.S, (1.0f - L) * stMax.T);
    float cMid;
    {
        const OkST stMid = okGetSTMid(a, b);
        const float cA = L * stMid.S;
        const float cB = (1.0f - L) * stMid.T;
        cMid = 0.9f * k * std::sqrt(std::sqrt(1.0f / (1.0f / (cA * cA * cA * cA) + 1.0f / (cB * cB * cB * cB))));
    }
    float c0;
    {
        const float cA = L * 0.4f;
        const float cB = (1.0f - L) * 0.8f;
        c0 = std::sqrt(1.0f / (1.0f / (cA * cA) + 1.0f / (cB * cB)));
    }
    return OkCs{c0, cMid, cMax};
}

constexpr float kOkTwoPi = 6.28318530717958647692f;

} // namespace detail

// OKHSL -> LINEAR RGBA — Godot's Color.from_ok_hsl (Ottosson okhsl_to_srgb, minus the sRGB encode).
inline Color fromOkhsl(const Okhsl& c) {
    if (c.l >= 1.0f) {
        return Color{1.0f, 1.0f, 1.0f, c.alpha};
    }
    if (c.l <= 0.0f) {
        return Color{0.0f, 0.0f, 0.0f, c.alpha};
    }
    const float a_ = std::cos(detail::kOkTwoPi * c.h);
    const float b_ = std::sin(detail::kOkTwoPi * c.h);
    const float L = detail::okToeInv(c.l);
    const detail::OkCs cs = detail::okGetCs(L, a_, b_);
    constexpr float mid = 0.8f, midInv = 1.25f;
    float C;
    if (c.s < mid) {
        const float t = midInv * c.s;
        const float k1 = mid * cs.C0;
        const float k2 = 1.0f - k1 / cs.CMid;
        C = t * k1 / (1.0f - k2 * t);
    } else {
        const float t = (c.s - mid) / (1.0f - mid);
        const float k0 = cs.CMid;
        const float k1 = (1.0f - mid) * cs.CMid * cs.CMid * midInv * midInv / cs.C0;
        const float k2 = 1.0f - k1 / (cs.CMax - cs.CMid);
        C = k0 + t * k1 / (1.0f - k2 * t);
    }
    return oklabToLinear(Oklab{L, C * a_, C * b_, c.alpha});
}
inline Color fromOkhsl(float h, float s, float l, float alpha = 1.0f) {
    return fromOkhsl(Okhsl{h, s, l, alpha});
}

// LINEAR RGBA -> OKHSL — Godot's Color.ok_hsl_h/s/l (Ottosson srgb_to_okhsl, minus the sRGB decode).
inline Okhsl toOkhsl(const Color& c) {
    const Oklab lab = linearToOklab(c);
    const float C = std::sqrt(lab.a * lab.a + lab.b * lab.b);
    const float a_ = C > 0.0f ? lab.a / C : 0.0f;
    const float b_ = C > 0.0f ? lab.b / C : 0.0f;
    const float L = lab.L;
    Okhsl out;
    out.alpha = c.a;
    out.h = 0.5f + 0.5f * std::atan2(-lab.b, -lab.a) / (0.5f * detail::kOkTwoPi);
    const detail::OkCs cs = detail::okGetCs(L, a_, b_);
    constexpr float mid = 0.8f, midInv = 1.25f;
    if (C < cs.CMid) {
        const float k1 = mid * cs.C0;
        const float k2 = 1.0f - k1 / cs.CMid;
        const float t = C / (k1 + k2 * C);
        out.s = t * mid;
    } else {
        const float k0 = cs.CMid;
        const float k1 = (1.0f - mid) * cs.CMid * cs.CMid * midInv * midInv / cs.C0;
        const float k2 = 1.0f - k1 / (cs.CMax - cs.CMid);
        const float t = (C - k0) / (k1 + k2 * (C - k0));
        out.s = mid + (1.0f - mid) * t;
    }
    out.l = detail::okToe(L);
    return out;
}

} // namespace maz::render
