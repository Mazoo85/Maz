#pragma once

#include "maz/render/Renderer.hpp"
#include "maz/ui/Font.hpp"

#include <cstdint>

namespace maz::ui {

// Screen-space rectangle (pixel coordinates, origin top-left).
struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

// Map a horizontal pointer position within `track` to a value in [minV, maxV], clamped to the ends.
inline float sliderValueFromX(const Rect& track, float pointerX, float minV, float maxV) {
    float t = track.w > 0.0f ? (pointerX - track.x) / track.w : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return minV + (maxV - minV) * t;
}

// A minimal immediate-mode (IMGUI-style) UI. Each frame you call begin() with the pointer state,
// then issue widget calls that both draw and return interaction; call end() to finish. Widgets are
// identified by a caller-supplied stable id so the context can track which one is hovered ("hot")
// and which is being pressed ("active") across frames — that's what makes a button only fire when
// the press and release happen on the same widget, and what lets a slider keep dragging even when
// the pointer briefly leaves the track.
//
// Drawing goes through the Renderer's 2D API in the active camera (use a pixel-space Camera2D for a
// screen-fixed menu). The draw calls are guarded so a Context with no renderer (or an inactive
// headless one) still runs all interaction logic — which is exactly how the unit tests exercise it
// without a GPU.
class Context {
public:
    // Bind the renderer + font used for drawing. `white` is a 1x1 white texture for solid quads.
    void init(render::Renderer& r, Font& font, render::TextureHandle white) {
        m_r = &r;
        m_font = &font;
        m_white = white;
    }

    // Start a frame: pointer position (pixels) and whether the primary button is currently held.
    void begin(float pointerX, float pointerY, bool pointerDown) {
        m_px = pointerX;
        m_py = pointerY;
        m_downPrev = m_down;
        m_down = pointerDown;
        m_hot = 0;
    }
    void end() {}

    // Colors (public so apps can theme the UI).
    render::Color colBg{0.14f, 0.16f, 0.22f, 0.92f};
    render::Color colItem{0.24f, 0.28f, 0.38f, 1.0f};
    render::Color colHot{0.32f, 0.38f, 0.52f, 1.0f};
    render::Color colActive{0.42f, 0.52f, 0.78f, 1.0f};
    render::Color colAccent{0.40f, 0.70f, 1.0f, 1.0f};
    render::Color colText{0.92f, 0.95f, 1.0f, 1.0f};

    void panel(const Rect& r, const render::Color& c) { quad(r, c); }

    void label(float x, float y, const char* text, float scale = 0.5f) {
        if (m_r && m_font) {
            m_font->drawText(*m_r, x, y, text, colText, scale);
        }
    }

    // A clickable button. Returns true on the frame the click completes (press + release on it).
    bool button(uint32_t id, const Rect& r, const char* text, float scale = 0.5f) {
        const bool inside = r.contains(m_px, m_py);
        if (inside) {
            m_hot = id;
        }
        bool clicked = false;
        if (m_active == id) {
            if (!m_down) {
                clicked = inside;
                m_active = 0;
            }
        } else if (inside && m_down && !m_downPrev) {
            m_active = id;
        }
        const render::Color& c = (m_active == id && inside) ? colActive : (m_hot == id ? colHot : colItem);
        quad(r, c);
        drawCenteredLabel(r, text, scale);
        return clicked;
    }

    // A checkbox-style toggle drawn as a box + label. Flips `value` on click; returns true if changed.
    bool toggle(uint32_t id, const Rect& box, const char* text, bool& value, float scale = 0.5f) {
        const bool inside = box.contains(m_px, m_py);
        if (inside) {
            m_hot = id;
        }
        bool changed = false;
        if (m_active == id) {
            if (!m_down) {
                if (inside) {
                    value = !value;
                    changed = true;
                }
                m_active = 0;
            }
        } else if (inside && m_down && !m_downPrev) {
            m_active = id;
        }
        quad(box, m_hot == id ? colHot : colItem);
        if (value) {
            const float pad = box.h * 0.25f;
            quad(Rect{box.x + pad, box.y + pad, box.w - 2 * pad, box.h - 2 * pad}, colAccent);
        }
        if (m_r && m_font) {
            m_font->drawText(*m_r, box.x + box.w + 8.0f, box.y + box.h * 0.5f - m_font->lineHeight(scale) * 0.5f,
                             text, colText, scale);
        }
        return changed;
    }

    // A horizontal slider. Dragging sets `value` in [minV, maxV]; returns true if it changed.
    bool slider(uint32_t id, const Rect& track, float& value, float minV, float maxV) {
        const bool inside = track.contains(m_px, m_py);
        if (inside) {
            m_hot = id;
        }
        bool changed = false;
        if (m_active == id) {
            const float nv = sliderValueFromX(track, m_px, minV, maxV);
            if (nv != value) {
                value = nv;
                changed = true;
            }
            if (!m_down) {
                m_active = 0;
            }
        } else if (inside && m_down && !m_downPrev) {
            m_active = id;
            const float nv = sliderValueFromX(track, m_px, minV, maxV);
            if (nv != value) {
                value = nv;
                changed = true;
            }
        }
        quad(track, colItem);
        // Fill + handle at the current value.
        const float span = maxV - minV;
        const float t = span != 0.0f ? (value - minV) / span : 0.0f;
        quad(Rect{track.x, track.y, track.w * t, track.h}, colAccent);
        const float hw = track.h * 0.5f;
        quad(Rect{track.x + track.w * t - hw, track.y - hw * 0.5f, hw * 2.0f, track.h + hw},
             (m_active == id || m_hot == id) ? colText : colHot);
        return changed;
    }

private:
    void quad(const Rect& r, const render::Color& c) {
        if (!m_r) {
            return;
        }
        render::SpriteDesc s;
        s.x = r.x;
        s.y = r.y;
        s.width = r.w;
        s.height = r.h;
        s.color = c;
        m_r->drawSprite(m_white, s);
    }
    void drawCenteredLabel(const Rect& r, const char* text, float scale) {
        if (m_r && m_font) {
            m_font->drawTextCentered(*m_r, r.x + r.w * 0.5f,
                                     r.y + r.h * 0.5f - m_font->lineHeight(scale) * 0.5f, text,
                                     colText, scale);
        }
    }

    render::Renderer* m_r = nullptr;
    Font* m_font = nullptr;
    render::TextureHandle m_white = render::kInvalidTexture;
    float m_px = 0.0f, m_py = 0.0f;
    bool m_down = false, m_downPrev = false;
    uint32_t m_hot = 0;    // widget under the pointer this frame
    uint32_t m_active = 0; // widget currently pressed (id captured on press)
};

} // namespace maz::ui
