#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace maz::ui {

// BBCode rich-text parser — Godot's RichTextLabel markup. Maz can draw a plain string (ui::Font) and wrap it
// (ui::layoutText, M134), but there was no way to mix styles WITHIN a string: bold a word, colour a phrase,
// enlarge a heading. Godot does this with BBCode — `[b]bold[/b]`, `[i]/[u]`, `[color=#ff0000]red[/color]`,
// `[size=32]big[/size]` — parsed into styled runs a label then lays out. This is that parser: `parseBBCode`
// turns a tagged string into a flat list of `RichSpan`s (each a substring + its resolved bold/italic/
// underline/colour/size), and `stripBBCode` returns the tags-removed plain text. It is deliberately
// renderer-independent (no Font/Color dependency) so it unit-tests headlessly; the app maps each span's
// attributes onto its own font draw. Tag handling is lenient like Godot: nested tags stack, an unclosed tag
// runs to the end, a stray close tag is ignored, `[lb]`/`[rb]` emit literal brackets, and an unrecognized
// tag is passed through as literal text rather than dropped.

struct RichSpan {
    std::string text;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool hasColor = false;
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    float sizePx = 0.0f; // 0 => inherit the label's default size
};

namespace detail {

// Map a BBCode colour value (`#rgb`, `#rrggbb`, `#rrggbbaa`, or a common name) to RGBA. Returns false if the
// value isn't a colour we recognize, so the caller can treat the whole tag as literal text.
inline bool parseRichColor(const std::string& v, float& r, float& g, float& b, float& a) {
    a = 1.0f;
    if (!v.empty() && v[0] == '#') {
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const std::string h = v.substr(1);
        auto byte2 = [&](std::size_t i) { return static_cast<float>(hexVal(h[i]) * 16 + hexVal(h[i + 1])) / 255.0f; };
        auto nib = [&](std::size_t i) { const int x = hexVal(h[i]); return static_cast<float>(x * 16 + x) / 255.0f; };
        for (char c : h) {
            if (hexVal(c) < 0) {
                return false;
            }
        }
        if (h.size() == 3) {
            r = nib(0); g = nib(1); b = nib(2);
            return true;
        }
        if (h.size() == 6) {
            r = byte2(0); g = byte2(2); b = byte2(4);
            return true;
        }
        if (h.size() == 8) {
            r = byte2(0); g = byte2(2); b = byte2(4); a = byte2(6);
            return true;
        }
        return false;
    }
    struct Named {
        const char* name;
        float r, g, b;
    };
    static const Named kNamed[] = {
        {"red", 1, 0, 0},      {"green", 0, 1, 0},     {"blue", 0, 0, 1},   {"white", 1, 1, 1},
        {"black", 0, 0, 0},    {"yellow", 1, 1, 0},    {"cyan", 0, 1, 1},   {"magenta", 1, 0, 1},
        {"orange", 1, 0.6f, 0}, {"gray", 0.5f, 0.5f, 0.5f}, {"grey", 0.5f, 0.5f, 0.5f},
    };
    for (const Named& n : kNamed) {
        if (v == n.name) {
            r = n.r; g = n.g; b = n.b;
            return true;
        }
    }
    return false;
}

} // namespace detail

// Parse a BBCode string into resolved style runs. Adjacent runs with identical style are coalesced, so the
// span list is minimal and predictable.
inline std::vector<RichSpan> parseBBCode(const std::string& src) {
    std::vector<RichSpan> spans;

    // Active nested state.
    int bold = 0, italic = 0, underline = 0;
    std::vector<std::array<float, 4>> colorStack; // r,g,b,a
    std::vector<float> sizeStack;
    std::string pending;

    auto flush = [&]() {
        if (pending.empty()) {
            return;
        }
        RichSpan s;
        s.text = pending;
        s.bold = bold > 0;
        s.italic = italic > 0;
        s.underline = underline > 0;
        if (!colorStack.empty()) {
            s.hasColor = true;
            s.r = colorStack.back()[0];
            s.g = colorStack.back()[1];
            s.b = colorStack.back()[2];
            s.a = colorStack.back()[3];
        }
        s.sizePx = sizeStack.empty() ? 0.0f : sizeStack.back();
        // Coalesce with the previous span if the resolved style is identical.
        if (!spans.empty()) {
            const RichSpan& p = spans.back();
            const bool same = p.bold == s.bold && p.italic == s.italic && p.underline == s.underline &&
                              p.hasColor == s.hasColor && p.r == s.r && p.g == s.g && p.b == s.b &&
                              p.a == s.a && p.sizePx == s.sizePx;
            if (same) {
                spans.back().text += s.text;
                pending.clear();
                return;
            }
        }
        spans.push_back(std::move(s));
        pending.clear();
    };

    const std::size_t n = src.size();
    for (std::size_t i = 0; i < n;) {
        if (src[i] != '[') {
            pending.push_back(src[i]);
            ++i;
            continue;
        }
        // Find the matching ']'.
        const std::size_t close = src.find(']', i);
        if (close == std::string::npos) {
            // No closing bracket — the rest is literal text.
            pending.append(src, i, std::string::npos);
            break;
        }
        const std::string tag = src.substr(i + 1, close - i - 1);
        const std::size_t next = close + 1;

        auto emitLiteral = [&]() { pending.append(src, i, close - i + 1); };

        if (tag == "b") {
            flush(); ++bold;
        } else if (tag == "/b") {
            flush(); if (bold > 0) --bold;
        } else if (tag == "i") {
            flush(); ++italic;
        } else if (tag == "/i") {
            flush(); if (italic > 0) --italic;
        } else if (tag == "u") {
            flush(); ++underline;
        } else if (tag == "/u") {
            flush(); if (underline > 0) --underline;
        } else if (tag == "lb") {
            pending.push_back('[');
        } else if (tag == "rb") {
            pending.push_back(']');
        } else if (tag == "/color") {
            flush(); if (!colorStack.empty()) colorStack.pop_back();
        } else if (tag == "/size") {
            flush(); if (!sizeStack.empty()) sizeStack.pop_back();
        } else if (tag.rfind("color=", 0) == 0) {
            float r, g, b, a;
            if (detail::parseRichColor(tag.substr(6), r, g, b, a)) {
                flush();
                colorStack.push_back({r, g, b, a});
            } else {
                emitLiteral(); // unrecognized colour → literal
            }
        } else if (tag.rfind("size=", 0) == 0) {
            const std::string num = tag.substr(5);
            char* end = nullptr;
            const float px = std::strtof(num.c_str(), &end);
            if (end != num.c_str() && px > 0.0f) {
                flush();
                sizeStack.push_back(px);
            } else {
                emitLiteral();
            }
        } else {
            emitLiteral(); // unknown tag → passthrough
        }
        i = next;
    }
    flush();
    return spans;
}

// Return the plain text with all recognized tags removed (literal-bracket and unknown-tag rules match
// parseBBCode).
inline std::string stripBBCode(const std::string& src) {
    std::string out;
    const std::vector<RichSpan> spans = parseBBCode(src);
    for (const RichSpan& s : spans) {
        out += s.text;
    }
    return out;
}

} // namespace maz::ui
