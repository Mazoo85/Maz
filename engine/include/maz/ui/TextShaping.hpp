#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// maz::ui text shaping — the OpenType GSUB/GPOS step that turns a run of glyph ids into POSITIONED
// glyphs, the piece complex scripts (and good Latin typography) need beyond the existing word-wrap
// (ui::layoutText) and bidi/line-break analysis (TextServer.hpp). Three data-driven features model
// the OpenType tables real fonts carry:
//   * Ligature substitution (GSUB LookupType 4): a contiguous run of glyphs (f, i) collapses to one
//     ligature glyph (fi). Longest match wins, and source-character clusters are merged so hit-testing
//     and caret placement still map back to the original text.
//   * Pair kerning (GPOS LookupType 2): an adjustment to the advance between two specific glyphs
//     (the classic "AV" tuck-in).
//   * Mark-to-base attachment (GPOS LookupType 4): a zero-advance combining mark (an accent, an
//     Arabic/Indic vowel sign) is offset so ITS anchor point coincides with the base glyph's anchor
//     point — the heart of "mark positioning".
// This is the shaping DATA + engine (what HarfBuzz evaluates), decoupled from any specific font blob:
// you feed it a ShapingTable (which a font's cmap/GSUB/GPOS would populate) and a glyph run, and it
// returns advances and x/y offsets. Pure CPU + integer/float math, so it unit-tests headlessly. This
// is Godot's TextServer shaping layer.
namespace maz::ui {

// One output glyph after shaping: which glyph to draw, how far to advance the pen afterward, and a
// per-glyph offset (used by marks and cursive attachment). `cluster` is the index of the source
// character this glyph derives from (the smallest, when several characters merged into a ligature).
struct ShapedGlyph {
    uint32_t glyph = 0;
    float xAdvance = 0.0f;
    float xOffset = 0.0f;
    float yOffset = 0.0f;
    uint32_t cluster = 0;
};

// An OpenType anchor point, in font units relative to the glyph's pen origin.
struct ShapeAnchor {
    float x = 0.0f;
    float y = 0.0f;
};

// A GSUB LigatureSubst rule: an ordered component sequence that collapses to one ligature glyph.
struct LigatureRule {
    std::vector<uint32_t> components; // length >= 2
    uint32_t ligature = 0;
};

// The shaping data for one font/script/language: default advances, ligatures, kerning pairs, the set
// of combining marks (which carry zero advance), and mark/base anchor points. In a real pipeline a
// font loader fills this from the font's GSUB/GPOS tables; here it is plain data so it is fully
// testable and also usable as a hand-authored shaping table for bitmap/atlas fonts.
struct ShapingTable {
    std::unordered_map<uint32_t, float> advance;      // default x-advance per glyph
    std::vector<LigatureRule> ligatures;              // GSUB type 4 (longest match wins)
    std::unordered_map<uint64_t, float> kern;         // GPOS type 2: key = pack(left,right) -> delta
    std::unordered_set<uint32_t> marks;               // combining marks (zero advance)
    std::unordered_map<uint32_t, ShapeAnchor> baseAnchor; // GPOS type 4: base attach point
    std::unordered_map<uint32_t, ShapeAnchor> markAnchor; // GPOS type 4: mark attach point

    float advanceOf(uint32_t g) const {
        const auto it = advance.find(g);
        return it != advance.end() ? it->second : 0.0f;
    }
    bool isMark(uint32_t g) const { return marks.count(g) != 0; }

    static uint64_t kernKey(uint32_t left, uint32_t right) {
        return (static_cast<uint64_t>(left) << 32) | static_cast<uint64_t>(right);
    }
    float kernBetween(uint32_t left, uint32_t right) const {
        const auto it = kern.find(kernKey(left, right));
        return it != kern.end() ? it->second : 0.0f;
    }
};

// Shape a run of glyph ids (post-cmap, single script/direction) into positioned glyphs.
// Substitution runs first (left-to-right, longest ligature match), then positioning (kerning between
// adjacent base glyphs, mark-to-base attachment for combining marks).
inline std::vector<ShapedGlyph> shapeGlyphs(const std::vector<uint32_t>& glyphs,
                                            const ShapingTable& table) {
    // --- Pass 1: GSUB ligature substitution, carrying source clusters. ---
    struct Item {
        uint32_t glyph;
        uint32_t cluster;
    };
    std::vector<Item> items;
    items.reserve(glyphs.size());
    for (size_t i = 0; i < glyphs.size(); ++i) {
        items.push_back(Item{glyphs[i], static_cast<uint32_t>(i)});
    }

    std::vector<Item> subst;
    subst.reserve(items.size());
    size_t i = 0;
    while (i < items.size()) {
        const LigatureRule* best = nullptr;
        size_t bestLen = 0;
        for (const LigatureRule& rule : table.ligatures) {
            const size_t n = rule.components.size();
            if (n < 2 || n <= bestLen || i + n > items.size()) {
                continue;
            }
            bool ok = true;
            for (size_t k = 0; k < n; ++k) {
                if (items[i + k].glyph != rule.components[k]) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                best = &rule;
                bestLen = n;
            }
        }
        if (best != nullptr) {
            // Merge: the ligature inherits the earliest (smallest) component cluster.
            subst.push_back(Item{best->ligature, items[i].cluster});
            i += bestLen;
        } else {
            subst.push_back(items[i]);
            ++i;
        }
    }

    // --- Pass 2: GPOS positioning (kerning + mark-to-base). ---
    std::vector<ShapedGlyph> out;
    out.reserve(subst.size());
    bool haveBase = false;
    size_t lastBase = 0; // index into `out` of the most recent base (non-mark) glyph

    for (const Item& it : subst) {
        ShapedGlyph sg;
        sg.glyph = it.glyph;
        sg.cluster = it.cluster;

        if (table.isMark(it.glyph)) {
            sg.xAdvance = 0.0f;
            if (haveBase) {
                const uint32_t baseGlyph = out[lastBase].glyph;
                const auto ba = table.baseAnchor.find(baseGlyph);
                const auto ma = table.markAnchor.find(it.glyph);
                if (ba != table.baseAnchor.end() && ma != table.markAnchor.end()) {
                    // Place the mark so its anchor coincides with the base's anchor. The pen sits at
                    // base_origin + base_advance; we want mark_origin + markAnchor + offset to equal
                    // base_origin + baseAnchor, hence the offset below.
                    sg.xOffset = ba->second.x - out[lastBase].xAdvance - ma->second.x;
                    sg.yOffset = ba->second.y - ma->second.y;
                }
            }
            out.push_back(sg);
        } else {
            sg.xAdvance = table.advanceOf(it.glyph);
            if (haveBase) {
                // Kern against the previous base by adjusting that base's advance.
                out[lastBase].xAdvance += table.kernBetween(out[lastBase].glyph, it.glyph);
            }
            out.push_back(sg);
            lastBase = out.size() - 1;
            haveBase = true;
        }
    }
    return out;
}

// Convenience: treat Unicode code points as glyph ids directly (useful for atlas/bitmap fonts and for
// testing). Real font shaping maps code points through the font's cmap first; this is that identity.
inline std::vector<ShapedGlyph> shapeCodepoints(const std::u32string& text, const ShapingTable& table) {
    std::vector<uint32_t> glyphs;
    glyphs.reserve(text.size());
    for (const char32_t c : text) {
        glyphs.push_back(static_cast<uint32_t>(c));
    }
    return shapeGlyphs(glyphs, table);
}

// Total advance width of a shaped run (sum of x-advances; marks contribute 0).
inline float shapedWidth(const std::vector<ShapedGlyph>& run) {
    float w = 0.0f;
    for (const ShapedGlyph& g : run) {
        w += g.xAdvance;
    }
    return w;
}

} // namespace maz::ui
