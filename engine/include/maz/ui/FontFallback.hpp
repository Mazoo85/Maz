#pragma once

#include <cstddef>
#include <string>
#include <vector>

// maz::ui font-fallback chain — the "which font can draw this character?" resolver behind mixed-script
// text. Godot lets a Font carry an ordered list of fallback fonts and picks, per glyph, the first that
// has the character; a UI drawing Latin + Cyrillic + emoji in one string leans on exactly this. Maz's
// Font atlas covers ASCII, so anything beyond it needs a fallback chain. This is the reusable core: an
// ordered set of fonts (by int id), each with the Unicode ranges it covers, plus `fontFor(codepoint)`
// (first covering font in priority order, or the default) and `runs(text)` which splits a UTF-32 string
// into contiguous runs that resolve to the same font — the unit a shaper/renderer draws in one pass.
// Deterministic, std-only, GPU-free, so it unit-tests headlessly; wire real font coverage into it later.
namespace maz::ui {

class FontFallback {
public:
    static constexpr int kNone = -1;

    struct CoverageRange {
        char32_t lo;
        char32_t hi; // inclusive
    };
    struct FontRun {
        int fontId;
        std::size_t start;
        std::size_t length;
    };

    // Register a font at the end of the priority chain (earlier = higher priority). Re-adding an existing
    // id is ignored (its coverage is kept).
    void addFont(int id) {
        if (indexOf(id) < 0) m_fonts.push_back(FontEntry{id, {}});
    }

    // Declare that font `id` covers the inclusive codepoint range [lo, hi]. Creates the font if needed.
    void coverRange(int id, char32_t lo, char32_t hi) {
        if (lo > hi) return;
        int idx = indexOf(id);
        if (idx < 0) {
            m_fonts.push_back(FontEntry{id, {}});
            idx = static_cast<int>(m_fonts.size()) - 1;
        }
        m_fonts[static_cast<std::size_t>(idx)].ranges.push_back(CoverageRange{lo, hi});
    }
    // Convenience for a single codepoint.
    void coverCodepoint(int id, char32_t cp) { coverRange(id, cp, cp); }

    // The font used when no font in the chain covers a codepoint (default: kNone).
    void setDefaultFont(int id) { m_default = id; }
    int defaultFont() const { return m_default; }

    bool hasFont(int id) const { return indexOf(id) >= 0; }
    std::size_t fontCount() const { return m_fonts.size(); }

    // Does font `id` cover codepoint `cp`?
    bool covers(int id, char32_t cp) const {
        const int idx = indexOf(id);
        if (idx < 0) return false;
        return coversEntry(m_fonts[static_cast<std::size_t>(idx)], cp);
    }

    // The first font in priority order that covers `cp`, or the default font if none do.
    int fontFor(char32_t cp) const {
        for (const FontEntry& f : m_fonts) {
            if (coversEntry(f, cp)) return f.id;
        }
        return m_default;
    }

    // Split `text` into contiguous runs that each resolve to one font.
    std::vector<FontRun> runs(const std::u32string& text) const {
        std::vector<FontRun> out;
        if (text.empty()) return out;
        int cur = fontFor(text[0]);
        std::size_t start = 0;
        for (std::size_t i = 1; i < text.size(); ++i) {
            const int f = fontFor(text[i]);
            if (f != cur) {
                out.push_back(FontRun{cur, start, i - start});
                cur = f;
                start = i;
            }
        }
        out.push_back(FontRun{cur, start, text.size() - start});
        return out;
    }

    void removeFont(int id) {
        const int idx = indexOf(id);
        if (idx >= 0) m_fonts.erase(m_fonts.begin() + idx);
    }
    void clear() {
        m_fonts.clear();
        m_default = kNone;
    }

private:
    struct FontEntry {
        int id;
        std::vector<CoverageRange> ranges;
    };

    int indexOf(int id) const {
        for (std::size_t i = 0; i < m_fonts.size(); ++i)
            if (m_fonts[i].id == id) return static_cast<int>(i);
        return -1;
    }
    static bool coversEntry(const FontEntry& f, char32_t cp) {
        for (const CoverageRange& r : f.ranges)
            if (cp >= r.lo && cp <= r.hi) return true;
        return false;
    }

    std::vector<FontEntry> m_fonts;
    int m_default = kNone;
};

} // namespace maz::ui
