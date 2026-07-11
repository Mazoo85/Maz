#pragma once

#include "maz/render/Renderer.hpp"

#include <cstdint>

namespace maz::ui {

// Bitmap font baked from a TTF via stb_truetype into a single atlas texture. Text is drawn as
// tinted glyph sprites through the Renderer's 2D API, so it works on any renderer backend and
// respects the active camera (use a pixel-space Camera2D for a screen-fixed HUD).
class Font {
public:
    // Bake `ttfPath` at `pixelHeight`. Uploads the atlas via renderer.createTexture.
    bool load(render::Renderer& renderer, const char* ttfPath, float pixelHeight);

    // Draw `text` with its top-left at (x, y) in the active camera's space. `\n` starts a new
    // line. `scale` multiplies the baked size.
    void drawText(render::Renderer& renderer, float x, float y, const char* text,
                  const render::Color& color, float scale = 1.0f) const;

    // Draw `text` horizontally centered on `cx` (single line; `y` is the top).
    void drawTextCentered(render::Renderer& renderer, float cx, float y, const char* text,
                          const render::Color& color, float scale = 1.0f) const;

    float textWidth(const char* text, float scale = 1.0f) const;
    float lineHeight(float scale = 1.0f) const { return m_pixelHeight * scale; }
    bool valid() const { return m_texture != render::kInvalidTexture; }

private:
    static constexpr int kFirst = 32; // space
    static constexpr int kCount = 96; // 32..127 inclusive of printable ASCII

    struct Glyph {
        float u0, v0, u1, v1;   // atlas UVs
        float xoff, yoff;       // pixel offset from pen position
        float xadvance;         // pen advance
        float w, h;             // glyph pixel size
    };

    render::TextureHandle m_texture = render::kInvalidTexture;
    float m_pixelHeight = 0.0f;
    float m_ascent = 0.0f; // baked pixels from top to baseline
    Glyph m_glyphs[kCount]{};
};

} // namespace maz::ui
