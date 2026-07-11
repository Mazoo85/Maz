#include "maz/ui/Font.hpp"

#include "maz/core/Log.hpp"

// stb_truetype is third-party; silence its warnings for the include only.
#define STB_TRUETYPE_IMPLEMENTATION
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
#include <stb_truetype.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <vector>

namespace maz::ui {

namespace {

std::vector<uint8_t> readBinary(const char* path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f) {
        return {};
    }
    const auto size = static_cast<size_t>(f.tellg());
    std::vector<uint8_t> buffer(size);
    f.seekg(0);
    f.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));
    return buffer;
}

} // namespace

bool Font::load(render::Renderer& renderer, const char* ttfPath, float pixelHeight) {
    std::vector<uint8_t> ttf = readBinary(ttfPath);
    if (ttf.empty()) {
        MAZ_LOG_ERROR("Font: could not read '%s'", ttfPath);
        return false;
    }
    m_pixelHeight = pixelHeight;

    // Bake the printable ASCII range into a coverage atlas.
    const int atlasW = 512;
    const int atlasH = 512;
    std::vector<uint8_t> coverage(static_cast<size_t>(atlasW) * atlasH, 0);
    stbtt_bakedchar baked[kCount];
    const int result = stbtt_BakeFontBitmap(ttf.data(), 0, pixelHeight, coverage.data(), atlasW,
                                            atlasH, kFirst, kCount, baked);
    if (result == 0) {
        MAZ_LOG_ERROR("Font: stbtt_BakeFontBitmap failed for '%s'", ttfPath);
        return false;
    }

    // Ascent (top-to-baseline) so callers can anchor text by its top-left.
    stbtt_fontinfo info;
    if (stbtt_InitFont(&info, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0))) {
        const float sc = stbtt_ScaleForPixelHeight(&info, pixelHeight);
        int asc = 0, desc = 0, gap = 0;
        stbtt_GetFontVMetrics(&info, &asc, &desc, &gap);
        m_ascent = static_cast<float>(asc) * sc;
    } else {
        m_ascent = pixelHeight * 0.8f;
    }

    // Expand coverage -> RGBA (white, alpha = coverage) so drawSprite tint sets the text color
    // and the alpha-blend pipeline composites it.
    std::vector<uint8_t> rgba(static_cast<size_t>(atlasW) * atlasH * 4);
    for (size_t i = 0; i < coverage.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = coverage[i];
    }
    m_texture = renderer.createTexture(static_cast<uint32_t>(atlasW),
                                       static_cast<uint32_t>(atlasH), rgba.data());
    if (m_texture == render::kInvalidTexture) {
        return false;
    }

    const float fw = static_cast<float>(atlasW);
    const float fh = static_cast<float>(atlasH);
    for (int i = 0; i < kCount; ++i) {
        const stbtt_bakedchar& b = baked[i];
        Glyph& g = m_glyphs[i];
        g.u0 = static_cast<float>(b.x0) / fw;
        g.v0 = static_cast<float>(b.y0) / fh;
        g.u1 = static_cast<float>(b.x1) / fw;
        g.v1 = static_cast<float>(b.y1) / fh;
        g.xoff = b.xoff;
        g.yoff = b.yoff;
        g.xadvance = b.xadvance;
        g.w = static_cast<float>(b.x1 - b.x0);
        g.h = static_cast<float>(b.y1 - b.y0);
    }
    return true;
}

void Font::drawText(render::Renderer& renderer, float x, float y, const char* text,
                    const render::Color& color, float scale) const {
    if (m_texture == render::kInvalidTexture || text == nullptr) {
        return;
    }
    float cursorX = x;
    float baseline = y + m_ascent * scale;
    for (const char* p = text; *p; ++p) {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if (ch == '\n') {
            cursorX = x;
            baseline += lineHeight(scale);
            continue;
        }
        if (ch < kFirst || ch >= kFirst + kCount) {
            continue;
        }
        const Glyph& g = m_glyphs[ch - kFirst];
        if (g.w > 0.0f && g.h > 0.0f) {
            render::SpriteDesc s;
            s.x = cursorX + g.xoff * scale;
            s.y = baseline + g.yoff * scale;
            s.width = g.w * scale;
            s.height = g.h * scale;
            s.uvMinX = g.u0;
            s.uvMinY = g.v0;
            s.uvMaxX = g.u1;
            s.uvMaxY = g.v1;
            s.color = color;
            renderer.drawSprite(m_texture, s);
        }
        cursorX += g.xadvance * scale;
    }
}

float Font::textWidth(const char* text, float scale) const {
    if (text == nullptr) {
        return 0.0f;
    }
    float width = 0.0f, maxWidth = 0.0f;
    for (const char* p = text; *p; ++p) {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if (ch == '\n') {
            maxWidth = width > maxWidth ? width : maxWidth;
            width = 0.0f;
            continue;
        }
        if (ch < kFirst || ch >= kFirst + kCount) {
            continue;
        }
        width += m_glyphs[ch - kFirst].xadvance * scale;
    }
    return width > maxWidth ? width : maxWidth;
}

} // namespace maz::ui
