#pragma once

#include "maz/render/ColorOps.hpp" // render::Color, fromHsv/toHsv, fromHtml/toHtml
#include "maz/render/Renderer.hpp" // render::Color {r,g,b,a}

#include <algorithm>
#include <string>
#include <vector>

// maz::ui ColorPicker — the interactive model behind Godot's ColorPicker control (the colour math
// itself lives in render::ColorOps, added earlier). A ColorPicker is more than an RGBA value: it
// edits colour in HSV space and must keep the *hue* (and saturation) stable while the user drags a
// value/saturation slider to an extreme — otherwise dragging brightness to black and back would
// scramble the hue. So this stores H, S, V, A as the source of truth (matching Godot, whose picker
// keeps `h`/`s` when a colour becomes grey/black), and derives RGB/hex on demand. It also carries
// the picker's editing state: an alpha-editing toggle, user preset swatches, and a capped
// most-recent list. Pure, header-only, deterministic — unit-tests exactly; the widget layer draws
// the wheel/sliders and calls these setters.
namespace maz::ui {

// Slider layout mode (Godot ColorMode). Purely which sliders the widget shows; the stored colour is
// identical across modes. RAW allows RGB channels above 1.0 (HDR); RGB/HSV clamp to [0,1].
enum class ColorPickerMode { Rgb, Hsv, Raw };

class ColorPicker {
  public:
    ColorPicker() = default;
    explicit ColorPicker(const render::Color& c) { setColor(c); }

    // --- The edited colour ------------------------------------------------------------------
    render::Color color() const {
        render::Color c = render::fromHsv(h_, s_, v_, editAlpha_ ? a_ : 1.0f);
        return c;
    }

    // Set from an RGBA colour. Decompose to HSV, but preserve hue when the colour is grey and
    // preserve hue+saturation when it is black — so a later value/saturation drag restores it.
    void setColor(const render::Color& c) {
        const render::Hsv hsv = render::toHsv(c);
        if (hsv.v < kEps) {              // black: keep hue + saturation, only value drops to 0
            v_ = 0.0f;
        } else if (hsv.s < kEps) {       // grey: keep hue, saturation is genuinely 0
            s_ = 0.0f;
            v_ = hsv.v;
        } else {
            h_ = hsv.h;
            s_ = hsv.s;
            v_ = hsv.v;
        }
        a_ = editAlpha_ ? std::clamp(c.a, 0.0f, 1.0f) : 1.0f;
    }

    // --- HSV editing (the source of truth; hue is trivially stable) --------------------------
    void setHue(float h) { h_ = h - std::floor(h); }             // wrap into [0,1)
    void setSaturation(float s) { s_ = std::clamp(s, 0.0f, 1.0f); }
    void setValue(float v) { v_ = std::clamp(v, 0.0f, 1.0f); }
    void setHsv(float h, float s, float v) {
        setHue(h);
        setSaturation(s);
        setValue(v);
    }
    float hue() const { return h_; }
    float saturation() const { return s_; }
    float value() const { return v_; }

    // --- RGB channel editing (derives HSV, preserving hue on grey/black) ---------------------
    void setR(float r) {
        render::Color c = color();
        c.r = clampChannel(r);
        setColor(c);
    }
    void setG(float g) {
        render::Color c = color();
        c.g = clampChannel(g);
        setColor(c);
    }
    void setB(float b) {
        render::Color c = color();
        c.b = clampChannel(b);
        setColor(c);
    }

    // --- Alpha ------------------------------------------------------------------------------
    void setAlpha(float a) { a_ = editAlpha_ ? std::clamp(a, 0.0f, 1.0f) : 1.0f; }
    float alpha() const { return editAlpha_ ? a_ : 1.0f; }

    // When alpha editing is off, the colour is always fully opaque and hex omits the alpha byte.
    void setEditAlpha(bool on) {
        editAlpha_ = on;
        if (!on) {
            a_ = 1.0f;
        }
    }
    bool editAlpha() const { return editAlpha_; }

    // --- Hex I/O ----------------------------------------------------------------------------
    // Parse "#rrggbb" / "#rrggbbaa" (with-or-without '#', shorthand allowed). Returns false and
    // leaves the colour unchanged on a malformed string.
    bool setHex(const std::string& text) {
        render::Color c;
        if (!render::fromHtml(text, c)) {
            return false;
        }
        setColor(c);
        return true;
    }
    std::string hex() const { return render::toHtml(color(), editAlpha_); }

    // --- Mode (which sliders the widget shows) ----------------------------------------------
    void setMode(ColorPickerMode m) { mode_ = m; }
    ColorPickerMode mode() const { return mode_; }

    // --- Preset swatches (Godot add_preset / erase_preset) ----------------------------------
    // Append a preset; a duplicate colour is moved to the end rather than added twice.
    void addPreset(const render::Color& c) {
        erasePreset(c);
        presets_.push_back(c);
    }
    void erasePreset(const render::Color& c) {
        presets_.erase(std::remove_if(presets_.begin(), presets_.end(),
                                      [&](const render::Color& p) { return sameColor(p, c); }),
                       presets_.end());
    }
    const std::vector<render::Color>& presets() const { return presets_; }
    std::size_t presetCount() const { return presets_.size(); }

    // --- Recent colours (Godot add_recent_preset) -------------------------------------------
    // Most-recent first, de-duplicated, capped at maxRecent (the oldest is dropped past the cap).
    void addRecent(const render::Color& c) {
        recent_.erase(std::remove_if(recent_.begin(), recent_.end(),
                                     [&](const render::Color& p) { return sameColor(p, c); }),
                      recent_.end());
        recent_.insert(recent_.begin(), c);
        if (recent_.size() > maxRecent_) {
            recent_.resize(maxRecent_);
        }
    }
    const std::vector<render::Color>& recent() const { return recent_; }
    void setMaxRecent(std::size_t n) {
        maxRecent_ = n;
        if (recent_.size() > maxRecent_) {
            recent_.resize(maxRecent_);
        }
    }
    std::size_t maxRecent() const { return maxRecent_; }

  private:
    static constexpr float kEps = 1e-6f;

    static bool sameColor(const render::Color& a, const render::Color& b) {
        const float e = 1.0f / 512.0f; // within an 8-bit quantisation step -> the "same" swatch
        return std::fabs(a.r - b.r) < e && std::fabs(a.g - b.g) < e && std::fabs(a.b - b.b) < e &&
               std::fabs(a.a - b.a) < e;
    }
    float clampChannel(float x) const {
        return mode_ == ColorPickerMode::Raw ? std::max(x, 0.0f) : std::clamp(x, 0.0f, 1.0f);
    }

    float h_ = 0.0f;
    float s_ = 0.0f;
    float v_ = 0.0f;
    float a_ = 1.0f;
    bool editAlpha_ = true;
    ColorPickerMode mode_ = ColorPickerMode::Rgb;
    std::vector<render::Color> presets_;
    std::vector<render::Color> recent_;
    std::size_t maxRecent_ = 20;
};

} // namespace maz::ui
