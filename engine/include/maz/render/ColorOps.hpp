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

} // namespace maz::render
