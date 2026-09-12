#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace maz::ui {

// Text layout — word-wrapping + alignment for multi-line paragraphs (Godot's Label autowrap + align).
// The Font renderer can draw a single line and measure its width, but it has no notion of FITTING text
// into a box: breaking a paragraph across lines at word boundaries so it doesn't overflow, and aligning
// each line left / center / right within the box. That's what every dialog box, tooltip, description
// pane, and subtitle needs. This is a pure algorithm — it takes a MEASURE callback (so it has no
// renderer/Font dependency and unit-tests headlessly) and returns positioned lines the caller then draws
// with one Font::drawText per line.

enum class TextAlign { Left, Center, Right };

struct TextLine {
    std::string text;
    float x = 0.0f;     // left offset within the box, per the chosen alignment
    float y = 0.0f;     // top offset (line index * lineHeight)
    float width = 0.0f; // measured pixel width of this line
};

struct TextLayout {
    std::vector<TextLine> lines;
    float width = 0.0f;  // widest line
    float height = 0.0f; // lines.size() * lineHeight
};

namespace detail {

// Greedy-wrap one paragraph (no embedded '\n') into lines no wider than maxWidth. A single word wider
// than maxWidth is placed alone on its line (no mid-word breaking). An empty paragraph yields one empty
// line so blank lines are preserved.
inline void wrapParagraph(std::string_view para, float maxWidth,
                          const std::function<float(std::string_view)>& measure,
                          std::vector<std::string>& outLines) {
    // Split into whitespace-separated words.
    std::vector<std::string> words;
    std::size_t i = 0;
    while (i < para.size()) {
        while (i < para.size() && para[i] == ' ') {
            ++i;
        }
        const std::size_t start = i;
        while (i < para.size() && para[i] != ' ') {
            ++i;
        }
        if (i > start) {
            words.emplace_back(para.substr(start, i - start));
        }
    }
    if (words.empty()) {
        outLines.emplace_back(); // preserve a blank line
        return;
    }

    std::string cur;
    for (const std::string& w : words) {
        if (cur.empty()) {
            cur = w;
            continue;
        }
        const std::string candidate = cur + " " + w;
        if (measure(candidate) <= maxWidth) {
            cur = candidate;
        } else {
            outLines.push_back(cur);
            cur = w; // an over-long single word still starts its own line here
        }
    }
    if (!cur.empty()) {
        outLines.push_back(cur);
    }
}

} // namespace detail

// Lay out `text` into `maxWidth`, wrapping at word boundaries and honoring explicit '\n' as hard breaks.
// `measure(s)` returns the pixel width of `s`; `lineHeight` is the vertical advance per line. Each line's
// x is set from `align`. The returned width is the widest line and height is lines*lineHeight.
inline TextLayout layoutText(std::string_view text, float maxWidth,
                             const std::function<float(std::string_view)>& measure, float lineHeight,
                             TextAlign align = TextAlign::Left) {
    // Split on '\n' into paragraphs (hard breaks), then word-wrap each.
    std::vector<std::string> lineStrings;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            detail::wrapParagraph(text.substr(start, i - start), maxWidth, measure, lineStrings);
            start = i + 1;
        }
    }

    TextLayout out;
    out.lines.reserve(lineStrings.size());
    float maxLineW = 0.0f;
    for (std::size_t li = 0; li < lineStrings.size(); ++li) {
        TextLine line;
        line.text = lineStrings[li];
        line.width = measure(line.text);
        line.y = static_cast<float>(li) * lineHeight;
        switch (align) {
            case TextAlign::Left: line.x = 0.0f; break;
            case TextAlign::Center: line.x = (maxWidth - line.width) * 0.5f; break;
            case TextAlign::Right: line.x = maxWidth - line.width; break;
        }
        if (line.width > maxLineW) {
            maxLineW = line.width;
        }
        out.lines.push_back(std::move(line));
    }
    out.width = maxLineW;
    out.height = static_cast<float>(lineStrings.size()) * lineHeight;
    return out;
}

} // namespace maz::ui
