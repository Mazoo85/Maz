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

} // namespace maz::render
