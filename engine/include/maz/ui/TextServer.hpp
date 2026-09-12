#pragma once

#include <cstdint>
#include <string>
#include <vector>

// maz::ui text server (subset) — the CPU text-analysis Godot's TextServer performs before shaping:
// UTF-8 decoding, base-direction detection, bidirectional run segmentation, and line-break
// opportunities. Real complex-script *shaping* (glyph substitution/positioning, HarfBuzz-class) and the
// full Unicode Bidirectional Algorithm (UAX #9 with explicit embeddings/isolates and weak-type
// resolution) are a much larger effort and remain; this implements the widely-needed subset: which
// direction a paragraph runs, splitting mixed LTR/RTL text into runs, and where a line may wrap
// (UAX #14 subset — after spaces/hyphens, mandatory at newlines, between CJK ideographs). Pure, header-
// only, deterministic — unit-tests exactly.
namespace maz::ui {

// Minimal UTF-8 -> codepoints. Malformed bytes become U+FFFD and are skipped over safely.
inline std::u32string decodeUtf8(const std::string& s) {
    std::u32string out;
    std::size_t i = 0;
    const std::size_t n = s.size();
    auto cont = [&](std::size_t k) {
        return k < n && (static_cast<unsigned char>(s[k]) & 0xC0u) == 0x80u;
    };
    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        if (c < 0x80u) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0u) == 0xC0u && cont(i + 1)) {
            cp = (static_cast<char32_t>(c & 0x1Fu) << 6) |
                 static_cast<char32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3Fu);
            i += 2;
        } else if ((c & 0xF0u) == 0xE0u && cont(i + 1) && cont(i + 2)) {
            cp = (static_cast<char32_t>(c & 0x0Fu) << 12) |
                 (static_cast<char32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) |
                 static_cast<char32_t>(static_cast<unsigned char>(s[i + 2]) & 0x3Fu);
            i += 3;
        } else if ((c & 0xF8u) == 0xF0u && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
            cp = (static_cast<char32_t>(c & 0x07u) << 18) |
                 (static_cast<char32_t>(static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 12) |
                 (static_cast<char32_t>(static_cast<unsigned char>(s[i + 2]) & 0x3Fu) << 6) |
                 static_cast<char32_t>(static_cast<unsigned char>(s[i + 3]) & 0x3Fu);
            i += 4;
        } else {
            cp = 0xFFFD;
            i += 1;
        }
        out.push_back(cp);
    }
    return out;
}

// Strong right-to-left character (Hebrew / Arabic blocks, incl. Arabic presentation forms).
inline bool isRtl(char32_t c) {
    return (c >= 0x0590 && c <= 0x05FF) ||  // Hebrew
           (c >= 0x0600 && c <= 0x06FF) ||  // Arabic
           (c >= 0x0700 && c <= 0x074F) ||  // Syriac
           (c >= 0x0750 && c <= 0x077F) ||  // Arabic Supplement
           (c >= 0xFB1D && c <= 0xFB4F) ||  // Hebrew presentation forms
           (c >= 0xFB50 && c <= 0xFDFF) ||  // Arabic presentation forms-A
           (c >= 0xFE70 && c <= 0xFEFF);    // Arabic presentation forms-B
}

// Strong left-to-right character (ASCII letters + common LTR letter blocks below Hebrew).
inline bool isStrongLtr(char32_t c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
        return true;
    }
    return c >= 0x00C0 && c < 0x0590; // Latin-1 letters, Latin Ext, Greek, Cyrillic, ...
}

// A CJK ideograph (used for break-anywhere line breaking).
inline bool isIdeograph(char32_t c) {
    return (c >= 0x4E00 && c <= 0x9FFF) ||  // CJK Unified Ideographs
           (c >= 0x3400 && c <= 0x4DBF) ||  // Extension A
           (c >= 0xF900 && c <= 0xFAFF);    // Compatibility Ideographs
}

enum class Direction { Ltr, Rtl };

// Paragraph base direction from the first strong character (UAX #9 rule P2/P3). Defaults to LTR.
inline Direction baseDirection(const std::u32string& text) {
    for (char32_t c : text) {
        if (isRtl(c)) {
            return Direction::Rtl;
        }
        if (isStrongLtr(c)) {
            return Direction::Ltr;
        }
    }
    return Direction::Ltr;
}

struct BidiRun {
    std::size_t start = 0;
    std::size_t length = 0;
    bool rtl = false;
};

// Segment text into directional runs. Simplified UAX #9: each character resolves to a strong
// direction (its own if strong, else inherited from the previous resolved character, else the base),
// and consecutive same-direction characters coalesce into a run. Enough for mixed LTR/RTL paragraphs;
// explicit embeddings/isolates and weak-type resolution are not modeled.
inline std::vector<BidiRun> bidiRuns(const std::u32string& text) {
    std::vector<BidiRun> runs;
    if (text.empty()) {
        return runs;
    }
    const bool baseRtl = baseDirection(text) == Direction::Rtl;
    bool prev = baseRtl;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char32_t c = text[i];
        bool dir;
        if (isRtl(c)) {
            dir = true;
        } else if (isStrongLtr(c)) {
            dir = false;
        } else {
            dir = prev; // neutral inherits the previous resolved direction
        }
        prev = dir;
        if (!runs.empty() && runs.back().rtl == dir) {
            runs.back().length += 1;
        } else {
            runs.push_back(BidiRun{i, 1, dir});
        }
    }
    return runs;
}

// Indices i (1..n) at which a line may break BEFORE position i. Rules (UAX #14 subset): after a space
// or tab, after a hyphen, always after a mandatory break ('\n'), and between two CJK ideographs. The
// end of the text (n) is always a break opportunity. `mandatory` (optional) receives the subset that
// are forced breaks (newlines).
inline std::vector<std::size_t> lineBreakOpportunities(const std::u32string& text,
                                                       std::vector<std::size_t>* mandatory = nullptr) {
    std::vector<std::size_t> ops;
    if (mandatory) {
        mandatory->clear();
    }
    const std::size_t n = text.size();
    for (std::size_t i = 1; i <= n; ++i) {
        const char32_t prev = text[i - 1];
        bool op = false;
        bool forced = false;
        if (prev == U'\n') {
            op = true;
            forced = true;
        } else if (i == n) {
            op = true; // end of text
        } else if (prev == U' ' || prev == U'\t') {
            // break after a space, but not before another space (collapse the run to one opportunity)
            op = text[i] != U' ' && text[i] != U'\t';
        } else if (prev == U'-') {
            op = true;
        } else if (isIdeograph(prev) && isIdeograph(text[i])) {
            op = true;
        }
        if (op) {
            ops.push_back(i);
            if (forced && mandatory) {
                mandatory->push_back(i);
            }
        }
    }
    return ops;
}

} // namespace maz::ui
