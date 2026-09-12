#pragma once

#include "maz/math/Math.hpp"
#include "maz/render/Renderer.hpp"
#include "maz/ui/Rect.hpp"
#include "maz/ui/StyleBox.hpp"

#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace maz::ui {

// StyleBoxFlat + Theme — the other half of Godot's theming (M103 gave the nine-patch StyleBoxTexture).
// Godot draws almost every default control through a StyleBoxFlat: a solid, ROUNDED-corner rectangle
// with an optional border and a soft drop shadow, generated procedurally — no texture asset. A Theme
// then names those styles per control class + state ("Button/normal", "Button/hover", …) so a whole UI
// restyles from one place. This module is that: dependency-light rounded-rect geometry (unit-testable),
// a StyleBoxFlat value type + a draw helper that layers shadow → border → fill through the 2D renderer,
// and a small Theme registry with a default fallback.

// Per-corner radius (top-left, top-right, bottom-right, bottom-left), in pixels.
struct Corners {
    float tl = 0.0f, tr = 0.0f, br = 0.0f, bl = 0.0f;

    Corners() = default;
    explicit Corners(float all) : tl(all), tr(all), br(all), bl(all) {}
    Corners(float topLeft, float topRight, float bottomRight, float bottomLeft)
        : tl(topLeft), tr(topRight), br(bottomRight), bl(bottomLeft) {}
};

// Outline vertices of a rounded rectangle, counter-clockwise in screen space (origin top-left), starting
// at the top-left corner arc. Each non-zero corner is approximated by `seg` segments; a zero-radius
// corner collapses to the sharp rectangle corner. Every corner radius is clamped so opposite corners on
// a side never overlap (max radius = half the shorter side). A rounded rectangle is convex, so the result
// drops straight into Renderer::drawConvexPolygon.
inline std::vector<render::Point2> roundedRectPolygon(const Rect& box, Corners c, int seg = 6) {
    if (seg < 1) {
        seg = 1;
    }
    const float maxR = 0.5f * (box.w < box.h ? box.w : box.h);
    auto clampR = [maxR](float r) { return r < 0.0f ? 0.0f : (r > maxR ? maxR : r); };
    c.tl = clampR(c.tl);
    c.tr = clampR(c.tr);
    c.br = clampR(c.br);
    c.bl = clampR(c.bl);

    const float x0 = box.x, y0 = box.y, x1 = box.right(), y1 = box.bottom();
    std::vector<render::Point2> out;
    out.reserve(static_cast<std::size_t>(4 * (seg + 1)));

    // Trace an arc of `r` around centre (cx,cy) from angle a0 to a1 (radians). r==0 emits the single
    // corner point once. Screen y grows downward, so we sweep clockwise on screen using standard math
    // angles measured from +x with +y downward.
    auto arc = [&](float cx, float cy, float r, float a0, float a1) {
        if (r <= 0.0f) {
            out.push_back({cx, cy});
            return;
        }
        for (int i = 0; i <= seg; ++i) {
            const float t = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(seg);
            out.push_back({cx + std::cos(t) * r, cy + std::sin(t) * r});
        }
    };

    const float pi = 3.14159265358979323846f;
    // CCW in screen space (top-left → bottom-left → bottom-right → top-right visually is CW math; we go
    // top-left → top-right → bottom-right → bottom-left which is clockwise on screen = CCW in +y-down).
    // Top-left corner: centre inset by radius; arc from 180° to 270°.
    arc(x0 + c.tl, y0 + c.tl, c.tl, pi, 1.5f * pi);
    // Top-right corner: arc from 270° to 360°.
    arc(x1 - c.tr, y0 + c.tr, c.tr, 1.5f * pi, 2.0f * pi);
    // Bottom-right corner: arc from 0° to 90°.
    arc(x1 - c.br, y1 - c.br, c.br, 0.0f, 0.5f * pi);
    // Bottom-left corner: arc from 90° to 180°.
    arc(x0 + c.bl, y1 - c.bl, c.bl, 0.5f * pi, pi);
    return out;
}

// A procedurally-drawn flat panel style — Godot StyleBoxFlat.
struct StyleBoxFlat {
    render::Color bg{0.20f, 0.22f, 0.27f, 1.0f};
    render::Color border{0.0f, 0.0f, 0.0f, 0.0f};
    float borderWidth = 0.0f;
    Corners radius;
    render::Color shadow{0.0f, 0.0f, 0.0f, 0.0f};
    float shadowSize = 0.0f;          // how far the shadow spreads beyond the box, in pixels
    math::vec2 shadowOffset{0.0f, 0.0f};
    Border contentMargin;             // inner padding a control lays its content inside

    // The rectangle a control should place its content in, inset by the content margins.
    Rect contentRect(const Rect& box) const {
        return Rect{box.x + contentMargin.left, box.y + contentMargin.top,
                    box.w - contentMargin.left - contentMargin.right,
                    box.h - contentMargin.top - contentMargin.bottom};
    }
};

// Draw a StyleBoxFlat into `box`: soft shadow first (offset + spread), then the border colour as the
// outer rounded rect, then the fill as an inset rounded rect (so a border ring shows). `seg` controls
// corner smoothness. No-op layers are skipped (zero alpha / zero size).
inline void drawStyleBoxFlat(render::Renderer& r, const Rect& box, const StyleBoxFlat& s, int seg = 6) {
    if (s.shadow.a > 0.0f && (s.shadowSize > 0.0f || s.shadowOffset.x != 0.0f ||
                              s.shadowOffset.y != 0.0f)) {
        const Rect sh{box.x - s.shadowSize + s.shadowOffset.x,
                      box.y - s.shadowSize + s.shadowOffset.y, box.w + 2.0f * s.shadowSize,
                      box.h + 2.0f * s.shadowSize};
        const Corners sc{s.radius.tl + s.shadowSize, s.radius.tr + s.shadowSize,
                         s.radius.br + s.shadowSize, s.radius.bl + s.shadowSize};
        const auto poly = roundedRectPolygon(sh, sc, seg);
        r.drawConvexPolygon(poly.data(), static_cast<std::uint32_t>(poly.size()), s.shadow);
    }

    if (s.borderWidth > 0.0f && s.border.a > 0.0f) {
        const auto outer = roundedRectPolygon(box, s.radius, seg);
        r.drawConvexPolygon(outer.data(), static_cast<std::uint32_t>(outer.size()), s.border);
        const float bw = s.borderWidth;
        const Rect inner{box.x + bw, box.y + bw, box.w - 2.0f * bw, box.h - 2.0f * bw};
        auto shrink = [bw](float rad) { return rad - bw < 0.0f ? 0.0f : rad - bw; };
        const Corners ic{shrink(s.radius.tl), shrink(s.radius.tr), shrink(s.radius.br),
                         shrink(s.radius.bl)};
        const auto in = roundedRectPolygon(inner, ic, seg);
        r.drawConvexPolygon(in.data(), static_cast<std::uint32_t>(in.size()), s.bg);
    } else {
        const auto poly = roundedRectPolygon(box, s.radius, seg);
        r.drawConvexPolygon(poly.data(), static_cast<std::uint32_t>(poly.size()), s.bg);
    }
}

// A Theme resource: named StyleBoxFlats and Colors resolved by string key (Godot's convention is
// "Type/state", e.g. "Button/hover"). Lookups fall back to a registered default so a control always
// gets *something*; `styleBox(type, state)` additionally falls back from "type/state" to "type/normal".
class Theme {
public:
    void setStyleBox(const std::string& key, const StyleBoxFlat& s) { styles_[key] = s; }
    bool hasStyleBox(const std::string& key) const { return styles_.find(key) != styles_.end(); }

    const StyleBoxFlat& styleBox(const std::string& key) const {
        const auto it = styles_.find(key);
        return it != styles_.end() ? it->second : defaultStyle_;
    }

    // Resolve "type/state", falling back to "type/normal", then to the default style.
    const StyleBoxFlat& styleBox(const std::string& type, const std::string& state) const {
        const auto it = styles_.find(type + "/" + state);
        if (it != styles_.end()) {
            return it->second;
        }
        const auto base = styles_.find(type + "/normal");
        return base != styles_.end() ? base->second : defaultStyle_;
    }

    void setDefaultStyleBox(const StyleBoxFlat& s) { defaultStyle_ = s; }

    void setColor(const std::string& key, render::Color c) { colors_[key] = c; }
    bool hasColor(const std::string& key) const { return colors_.find(key) != colors_.end(); }
    render::Color color(const std::string& key, render::Color fallback = {}) const {
        const auto it = colors_.find(key);
        return it != colors_.end() ? it->second : fallback;
    }

    std::size_t styleCount() const { return styles_.size(); }

    // --- Theme type variations (Godot's `theme_type_variation` / theme type inheritance) ---
    // Registers that items missing on `type` are looked up on `base` next (and so on up the chain).
    // A variation named "FlatButton" based on "Button" inherits every Button item it doesn't override.
    void setTypeVariation(const std::string& type, const std::string& base) { typeBase_[type] = base; }
    std::string typeVariationBase(const std::string& type) const {
        const auto it = typeBase_.find(type);
        return it != typeBase_.end() ? it->second : std::string{};
    }

    // Typed setters store items under "type/name" so they participate in the inheritance chain.
    void setColor(const std::string& type, const std::string& name, render::Color c) {
        colors_[type + "/" + name] = c;
    }
    void setStyleBox(const std::string& type, const std::string& name, const StyleBoxFlat& s) {
        styles_[type + "/" + name] = s;
    }
    void setConstant(const std::string& type, const std::string& name, float v) {
        constants_[type + "/" + name] = v;
    }

    // Resolve a colour item by (type, name), walking the type-variation chain before the fallback.
    // Cycle-safe: a base that loops back on itself is visited at most once.
    render::Color themeColor(const std::string& type, const std::string& name,
                             render::Color fallback = {}) const {
        std::set<std::string> seen;
        for (std::string t = type; !t.empty() && seen.insert(t).second; t = typeVariationBase(t)) {
            const auto it = colors_.find(t + "/" + name);
            if (it != colors_.end()) {
                return it->second;
            }
        }
        return fallback;
    }
    bool hasThemeColor(const std::string& type, const std::string& name) const {
        std::set<std::string> seen;
        for (std::string t = type; !t.empty() && seen.insert(t).second; t = typeVariationBase(t)) {
            if (colors_.find(t + "/" + name) != colors_.end()) {
                return true;
            }
        }
        return false;
    }

    // Resolve a StyleBoxFlat by (type, name) through the chain; returns the default style on a miss.
    const StyleBoxFlat& themeStyleBox(const std::string& type, const std::string& name) const {
        std::set<std::string> seen;
        for (std::string t = type; !t.empty() && seen.insert(t).second; t = typeVariationBase(t)) {
            const auto it = styles_.find(t + "/" + name);
            if (it != styles_.end()) {
                return it->second;
            }
        }
        return defaultStyle_;
    }
    bool hasThemeStyleBox(const std::string& type, const std::string& name) const {
        std::set<std::string> seen;
        for (std::string t = type; !t.empty() && seen.insert(t).second; t = typeVariationBase(t)) {
            if (styles_.find(t + "/" + name) != styles_.end()) {
                return true;
            }
        }
        return false;
    }

    // Resolve a numeric constant by (type, name) through the chain.
    float themeConstant(const std::string& type, const std::string& name, float fallback = 0.0f) const {
        std::set<std::string> seen;
        for (std::string t = type; !t.empty() && seen.insert(t).second; t = typeVariationBase(t)) {
            const auto it = constants_.find(t + "/" + name);
            if (it != constants_.end()) {
                return it->second;
            }
        }
        return fallback;
    }
    bool hasThemeConstant(const std::string& type, const std::string& name) const {
        std::set<std::string> seen;
        for (std::string t = type; !t.empty() && seen.insert(t).second; t = typeVariationBase(t)) {
            if (constants_.find(t + "/" + name) != constants_.end()) {
                return true;
            }
        }
        return false;
    }

private:
    std::unordered_map<std::string, StyleBoxFlat> styles_;
    std::unordered_map<std::string, render::Color> colors_;
    std::unordered_map<std::string, float> constants_;
    std::unordered_map<std::string, std::string> typeBase_;
    StyleBoxFlat defaultStyle_;
};

// --- Per-control theme overrides (Godot's `add_theme_color_override` / `add_theme_stylebox_override` …) ---
// A control carries a small local override table that takes precedence over its theme. Items are keyed by
// name only (the control already knows its own type), matching Godot where an override wins regardless of
// the resolved theme type. Empty by default, so a control with no overrides falls straight through.
struct ThemeOverrides {
    std::unordered_map<std::string, render::Color> colors;
    std::unordered_map<std::string, StyleBoxFlat> styleBoxes;
    std::unordered_map<std::string, float> constants;

    void setColor(const std::string& name, render::Color c) { colors[name] = c; }
    bool hasColor(const std::string& name) const { return colors.find(name) != colors.end(); }
    void clearColor(const std::string& name) { colors.erase(name); }

    void setStyleBox(const std::string& name, const StyleBoxFlat& s) { styleBoxes[name] = s; }
    bool hasStyleBox(const std::string& name) const { return styleBoxes.find(name) != styleBoxes.end(); }
    void clearStyleBox(const std::string& name) { styleBoxes.erase(name); }

    void setConstant(const std::string& name, float v) { constants[name] = v; }
    bool hasConstant(const std::string& name) const { return constants.find(name) != constants.end(); }
    void clearConstant(const std::string& name) { constants.erase(name); }

    bool empty() const { return colors.empty() && styleBoxes.empty() && constants.empty(); }
};

// Resolve a theme item the way a Control does: a local override wins; otherwise walk the theme's
// type-variation chain; otherwise the supplied fallback.
inline render::Color resolveColor(const ThemeOverrides& ov, const Theme& theme, const std::string& type,
                                  const std::string& name, render::Color fallback = {}) {
    const auto it = ov.colors.find(name);
    return it != ov.colors.end() ? it->second : theme.themeColor(type, name, fallback);
}
inline StyleBoxFlat resolveStyleBox(const ThemeOverrides& ov, const Theme& theme, const std::string& type,
                                    const std::string& name) {
    const auto it = ov.styleBoxes.find(name);
    return it != ov.styleBoxes.end() ? it->second : theme.themeStyleBox(type, name);
}
inline float resolveConstant(const ThemeOverrides& ov, const Theme& theme, const std::string& type,
                             const std::string& name, float fallback = 0.0f) {
    const auto it = ov.constants.find(name);
    return it != ov.constants.end() ? it->second : theme.themeConstant(type, name, fallback);
}

} // namespace maz::ui
