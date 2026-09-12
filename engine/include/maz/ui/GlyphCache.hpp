#pragma once

#include "maz/render/AtlasPacker.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>

// maz::ui dynamic-font glyph cache — the size-keyed glyph atlas Godot's FontFile maintains for dynamic
// (TrueType/OpenType) fonts. A font has no fixed bitmaps: each (glyph, pixel-size) is rasterized on
// first use and cached in a texture atlas, so the same character at the same size is rasterized once and
// thereafter is a cheap lookup. This models that cache independently of the rasterizer (which is
// injected), so it unit-tests headlessly: keying by font+codepoint+size, packing new glyphs into the
// atlas via the skyline packer, returning the atlas rect + metrics, and — when the atlas fills — evicting
// everything and starting over (glyphs re-rasterize lazily on next access). Wiring a real stb_truetype
// rasterizer + GPU atlas texture on top is the usual font path; this is the caching/layout brain.
namespace maz::ui {

// Identifies one rasterized glyph: which font, which Unicode codepoint, at which pixel size.
struct GlyphKey {
    uint32_t font = 0;
    uint32_t codepoint = 0;
    uint32_t pixelSize = 0;

    bool operator==(const GlyphKey& o) const {
        return font == o.font && codepoint == o.codepoint && pixelSize == o.pixelSize;
    }
    // Pack into a stable 64-bit id: font(16) | size(16) | codepoint(32... low 32 kept).
    uint64_t id() const {
        return (static_cast<uint64_t>(font & 0xFFFFu) << 48) |
               (static_cast<uint64_t>(pixelSize & 0xFFFFu) << 32) |
               static_cast<uint64_t>(codepoint);
    }
};

struct GlyphMetrics {
    int width = 0;    // bitmap width in px
    int height = 0;   // bitmap height in px
    float advance = 0.0f;  // pen advance after this glyph
    float bearingX = 0.0f; // left side bearing
    float bearingY = 0.0f; // top side bearing (baseline to top)
};

// What a rasterizer returns for a requested glyph: its bitmap size + metrics (pixels live wherever the
// caller wants; the cache only needs dimensions to lay it out).
struct GlyphBitmap {
    GlyphMetrics metrics;
};

// A cached glyph: where it sits in the atlas + its metrics.
struct CachedGlyph {
    render::Placement rect; // x,y,w,h in the atlas
    GlyphMetrics metrics;
    bool valid = false;
};

class GlyphCache {
  public:
    // Rasterizer: given a key, produce the glyph's bitmap size + metrics. Called once per (glyph,size)
    // between evictions.
    using Rasterizer = std::function<GlyphBitmap(const GlyphKey&)>;

    GlyphCache(int atlasWidth, int atlasHeight, Rasterizer rasterizer, int padding = 1)
        : m_packer(atlasWidth, atlasHeight), m_raster(std::move(rasterizer)),
          m_pad(padding < 0 ? 0 : padding) {}

    int atlasWidth() const { return m_packer.width(); }
    int atlasHeight() const { return m_packer.height(); }

    // Fetch a glyph, rasterizing + packing it on a miss. Returns nullptr only if the glyph cannot fit in
    // an empty atlas (too large for the page). The returned pointer is valid until the next eviction.
    const CachedGlyph* get(const GlyphKey& key) {
        const uint64_t k = key.id();
        auto it = m_entries.find(k);
        if (it != m_entries.end()) {
            ++m_hits;
            return &it->second;
        }
        ++m_misses;
        const CachedGlyph* placed = rasterizeAndPack(key, k);
        if (placed) {
            return placed;
        }
        // Atlas is full: evict everything and retry once in the fresh atlas.
        evictAll();
        return rasterizeAndPack(key, k);
    }

    bool contains(const GlyphKey& key) const { return m_entries.count(key.id()) != 0; }
    std::size_t count() const { return m_entries.size(); }

    // Drop all cached glyphs and reset the atlas (glyphs re-rasterize on next access).
    void clear() { evictAll(); }

    // Stats.
    long hits() const { return m_hits; }
    long misses() const { return m_misses; }
    long rasterizations() const { return m_rasterizations; }
    long evictions() const { return m_evictions; }
    float occupancy() const { return m_packer.occupancy(); }

  private:
    const CachedGlyph* rasterizeAndPack(const GlyphKey& key, uint64_t k) {
        const GlyphBitmap bmp = m_raster(key);
        ++m_rasterizations;
        const int w = bmp.metrics.width + m_pad;
        const int h = bmp.metrics.height + m_pad;
        render::Placement p = m_packer.insert(w > 0 ? w : 1, h > 0 ? h : 1);
        if (!p.placed) {
            return nullptr;
        }
        CachedGlyph g;
        g.rect = p;
        g.rect.w = bmp.metrics.width; // report the true glyph rect (without the padding gutter)
        g.rect.h = bmp.metrics.height;
        g.metrics = bmp.metrics;
        g.valid = true;
        auto res = m_entries.emplace(k, g);
        return &res.first->second;
    }

    void evictAll() {
        if (!m_entries.empty()) {
            ++m_evictions;
        }
        m_entries.clear();
        m_packer.reset();
    }

    render::AtlasPacker m_packer;
    Rasterizer m_raster;
    int m_pad;
    std::unordered_map<uint64_t, CachedGlyph> m_entries;
    long m_hits = 0;
    long m_misses = 0;
    long m_rasterizations = 0;
    long m_evictions = 0;
};

} // namespace maz::ui
