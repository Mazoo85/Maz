#pragma once

#include "maz/math/Math.hpp" // math::vec2
#include "maz/render/Path.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// maz::render stroke font — text on a CPU image, with no GPU and no font file.
//
// The engine's only text was ui::Font, which bakes a TTF through stb_truetype and draws the glyphs as
// sprites through the Vulkan renderer. That is the right thing for a HUD and no use at all to anything
// headless: stb is linked PRIVATE to the engine, so a header cannot reach it, and a Renderer means a
// window. Anything that draws into a render::Image -- a generated thumbnail, a headless test's
// annotated output, a film frame rendered with no display attached -- had no way to put a word on it.
//
// This is a single-stroke ("engraver's") font: every glyph is a handful of polylines on a 1-em grid,
// stroked to a given weight. No hinting, no kerning pairs, no bitmaps to ship, no dependency, and it
// stays crisp at any size because it is geometry rather than pixels. It is legible and plain rather
// than beautiful -- an honest description, not modesty.
//
// The grid: x runs left to right from the pen, y runs DOWNWARD with the cap top at 0 and the baseline
// at 1. Lowercase x-height is 0.36; descenders reach 1.26. A whole string becomes ONE Path and is
// filled ONCE, the same trick that keeps a figure's nine limbs to a single fill.
namespace maz::render {

struct TextStyle {
    float size = 32.0f;     // cap height to baseline, in pixels
    float weight = 0.085f;  // stroke thickness as a fraction of `size`
    float tracking = 0.04f; // extra space between glyphs, as a fraction of `size`
};

namespace detail {

using Stroke = std::vector<math::vec2>;

struct Glyph {
    float advance = 0.6f;
    std::vector<Stroke> strokes;
};

// Append an elliptical arc. Angles are in the y-down convention: 0 points right, +PI/2 points DOWN.
inline void arcPts(Stroke& s, float cx, float cy, float rx, float ry, float a0, float a1, int steps) {
    for (int i = 0; i <= steps; ++i) {
        const float t = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(steps);
        s.push_back(math::vec2{cx + rx * std::cos(t), cy + ry * std::sin(t)});
    }
}

inline Stroke ring(float cx, float cy, float rx, float ry, int steps = 18) {
    Stroke s;
    arcPts(s, cx, cy, rx, ry, 0.0f, 6.28318531f, steps);
    return s;
}

// A dot is drawn as a degenerate one-point stroke: the round cap does the work.
inline Stroke dot(float x, float y) { return Stroke{math::vec2{x, y}}; }

inline const std::unordered_map<char, Glyph>& glyphs() {
    static const std::unordered_map<char, Glyph> kGlyphs = [] {
        std::unordered_map<char, Glyph> g;
        const float PI = 3.14159265f;

        // --- capitals (cap height 0 .. 1) ---
        g['A'] = {0.72f, {{{0.00f, 1.00f}, {0.31f, 0.00f}, {0.62f, 1.00f}}, {{0.11f, 0.66f}, {0.51f, 0.66f}}}};
        {
            Stroke top{{0.00f, 0.00f}, {0.32f, 0.00f}};
            arcPts(top, 0.32f, 0.25f, 0.22f, 0.25f, -PI / 2.0f, PI / 2.0f, 7);
            top.push_back({0.00f, 0.50f});
            Stroke bot{{0.00f, 0.50f}, {0.34f, 0.50f}};
            arcPts(bot, 0.34f, 0.75f, 0.24f, 0.25f, -PI / 2.0f, PI / 2.0f, 7);
            bot.push_back({0.00f, 1.00f});
            g['B'] = {0.72f, {{{0.00f, 0.00f}, {0.00f, 1.00f}}, top, bot}};
        }
        {
            Stroke c;
            arcPts(c, 0.33f, 0.50f, 0.31f, 0.50f, -0.30f * PI, -1.70f * PI, 16);
            g['C'] = {0.72f, {c}};
            Stroke gg = c;
            g['G'] = {0.76f, {gg, {{0.40f, 0.55f}, {0.64f, 0.55f}}, {{0.64f, 0.55f}, {0.64f, 0.92f}}}};
        }
        {
            Stroke d{{0.00f, 0.00f}, {0.26f, 0.00f}};
            arcPts(d, 0.26f, 0.50f, 0.36f, 0.50f, -PI / 2.0f, PI / 2.0f, 11);
            d.push_back({0.00f, 1.00f});
            g['D'] = {0.74f, {{{0.00f, 0.00f}, {0.00f, 1.00f}}, d}};
        }
        g['E'] = {0.68f, {{{0.58f, 0.00f}, {0.00f, 0.00f}, {0.00f, 1.00f}, {0.58f, 1.00f}}, {{0.00f, 0.50f}, {0.46f, 0.50f}}}};
        g['F'] = {0.66f, {{{0.58f, 0.00f}, {0.00f, 0.00f}, {0.00f, 1.00f}}, {{0.00f, 0.50f}, {0.44f, 0.50f}}}};
        g['H'] = {0.72f, {{{0.00f, 0.00f}, {0.00f, 1.00f}}, {{0.58f, 0.00f}, {0.58f, 1.00f}}, {{0.00f, 0.52f}, {0.58f, 0.52f}}}};
        g['I'] = {0.32f, {{{0.12f, 0.00f}, {0.12f, 1.00f}}}};
        {
            Stroke j{{0.46f, 0.00f}, {0.46f, 0.76f}};
            arcPts(j, 0.24f, 0.76f, 0.22f, 0.24f, 0.0f, PI, 8);
            g['J'] = {0.60f, {j}};
        }
        g['K'] = {0.70f, {{{0.00f, 0.00f}, {0.00f, 1.00f}}, {{0.56f, 0.00f}, {0.04f, 0.58f}}, {{0.20f, 0.42f}, {0.58f, 1.00f}}}};
        g['L'] = {0.62f, {{{0.00f, 0.00f}, {0.00f, 1.00f}, {0.54f, 1.00f}}}};
        g['M'] = {0.88f, {{{0.00f, 1.00f}, {0.00f, 0.00f}, {0.36f, 0.64f}, {0.72f, 0.00f}, {0.72f, 1.00f}}}};
        g['N'] = {0.74f, {{{0.00f, 1.00f}, {0.00f, 0.00f}, {0.60f, 1.00f}, {0.60f, 0.00f}}}};
        g['O'] = {0.76f, {ring(0.33f, 0.50f, 0.31f, 0.50f, 20)}};
        {
            Stroke p{{0.00f, 0.00f}, {0.32f, 0.00f}};
            arcPts(p, 0.32f, 0.28f, 0.24f, 0.28f, -PI / 2.0f, PI / 2.0f, 8);
            p.push_back({0.00f, 0.56f});
            g['P'] = {0.70f, {{{0.00f, 0.00f}, {0.00f, 1.00f}}, p}};
            g['R'] = {0.72f, {{{0.00f, 0.00f}, {0.00f, 1.00f}}, p, {{0.28f, 0.56f}, {0.60f, 1.00f}}}};
        }
        g['Q'] = {0.78f, {ring(0.33f, 0.50f, 0.31f, 0.50f, 20), {{0.38f, 0.70f}, {0.66f, 1.06f}}}};
        g['S'] = {0.68f, {{{0.56f, 0.17f}, {0.44f, 0.03f}, {0.20f, 0.01f}, {0.05f, 0.14f}, {0.06f, 0.34f},
                           {0.30f, 0.49f}, {0.54f, 0.62f}, {0.57f, 0.82f}, {0.43f, 0.98f}, {0.18f, 0.99f},
                           {0.03f, 0.85f}}}};
        g['T'] = {0.64f, {{{0.00f, 0.00f}, {0.60f, 0.00f}}, {{0.30f, 0.00f}, {0.30f, 1.00f}}}};
        {
            Stroke u{{0.00f, 0.00f}, {0.00f, 0.70f}};
            arcPts(u, 0.30f, 0.70f, 0.30f, 0.30f, PI, 0.0f, 10);
            u.push_back({0.60f, 0.00f});
            g['U'] = {0.74f, {u}};
        }
        g['V'] = {0.72f, {{{0.00f, 0.00f}, {0.30f, 1.00f}, {0.60f, 0.00f}}}};
        g['W'] = {1.00f, {{{0.00f, 0.00f}, {0.19f, 1.00f}, {0.42f, 0.32f}, {0.65f, 1.00f}, {0.84f, 0.00f}}}};
        g['X'] = {0.70f, {{{0.00f, 0.00f}, {0.58f, 1.00f}}, {{0.58f, 0.00f}, {0.00f, 1.00f}}}};
        g['Y'] = {0.68f, {{{0.00f, 0.00f}, {0.29f, 0.52f}, {0.58f, 0.00f}}, {{0.29f, 0.52f}, {0.29f, 1.00f}}}};
        g['Z'] = {0.68f, {{{0.00f, 0.00f}, {0.58f, 0.00f}, {0.00f, 1.00f}, {0.58f, 1.00f}}}};

        // --- digits ---
        g['0'] = {0.70f, {ring(0.29f, 0.50f, 0.27f, 0.50f, 20)}};
        g['1'] = {0.52f, {{{0.06f, 0.20f}, {0.26f, 0.02f}, {0.26f, 1.00f}}, {{0.06f, 1.00f}, {0.46f, 1.00f}}}};
        g['2'] = {0.68f, {{{0.04f, 0.22f}, {0.14f, 0.05f}, {0.38f, 0.01f}, {0.55f, 0.17f}, {0.50f, 0.40f},
                           {0.04f, 1.00f}, {0.58f, 1.00f}}}};
        g['3'] = {0.68f, {{{0.04f, 0.14f}, {0.22f, 0.02f}, {0.46f, 0.06f}, {0.52f, 0.26f}, {0.30f, 0.46f}},
                          {{0.30f, 0.46f}, {0.54f, 0.60f}, {0.52f, 0.86f}, {0.30f, 0.99f}, {0.06f, 0.90f}}}};
        g['4'] = {0.70f, {{{0.44f, 1.00f}, {0.44f, 0.00f}, {0.02f, 0.70f}, {0.60f, 0.70f}}}};
        g['5'] = {0.68f, {{{0.52f, 0.02f}, {0.12f, 0.02f}, {0.08f, 0.42f}},
                          {{0.08f, 0.42f}, {0.30f, 0.35f}, {0.52f, 0.47f}, {0.55f, 0.75f}, {0.34f, 0.98f},
                           {0.07f, 0.92f}}}};
        g['6'] = {0.68f, {ring(0.31f, 0.70f, 0.27f, 0.28f, 16),
                          {{0.52f, 0.05f}, {0.28f, 0.10f}, {0.10f, 0.32f}, {0.04f, 0.66f}}}};
        g['7'] = {0.64f, {{{0.02f, 0.02f}, {0.58f, 0.02f}, {0.24f, 1.00f}}}};
        g['8'] = {0.70f, {ring(0.31f, 0.26f, 0.25f, 0.26f, 16), ring(0.31f, 0.74f, 0.29f, 0.26f, 16)}};
        g['9'] = {0.68f, {ring(0.31f, 0.30f, 0.27f, 0.28f, 16),
                          {{0.58f, 0.34f}, {0.54f, 0.68f}, {0.36f, 0.90f}, {0.10f, 0.97f}}}};

        // --- lowercase (x-height top 0.36, baseline 1, descender 1.26) ---
        g['a'] = {0.62f, {ring(0.27f, 0.68f, 0.25f, 0.32f, 16), {{0.52f, 0.40f}, {0.52f, 1.00f}}}};
        g['b'] = {0.64f, {{{0.02f, 0.00f}, {0.02f, 1.00f}}, ring(0.30f, 0.68f, 0.28f, 0.32f, 16)}};
        {
            Stroke c;
            arcPts(c, 0.30f, 0.68f, 0.26f, 0.32f, -0.28f * PI, -1.72f * PI, 13);
            g['c'] = {0.58f, {c}};
        }
        g['d'] = {0.64f, {{{0.56f, 0.00f}, {0.56f, 1.00f}}, ring(0.28f, 0.68f, 0.26f, 0.32f, 16)}};
        {
            Stroke e;
            arcPts(e, 0.29f, 0.68f, 0.27f, 0.32f, 0.0f, -1.75f * PI, 15);
            g['e'] = {0.60f, {{{0.03f, 0.64f}, {0.56f, 0.64f}}, e}};
        }
        g['f'] = {0.40f, {{{0.42f, 0.06f}, {0.30f, 0.00f}, {0.19f, 0.12f}, {0.19f, 1.00f}},
                          {{0.02f, 0.40f}, {0.42f, 0.40f}}}};
        g['g'] = {0.64f, {ring(0.28f, 0.68f, 0.26f, 0.32f, 16),
                          {{0.54f, 0.38f}, {0.54f, 1.08f}, {0.40f, 1.24f}, {0.14f, 1.22f}}}};
        g['h'] = {0.62f, {{{0.02f, 0.00f}, {0.02f, 1.00f}},
                          {{0.02f, 0.56f}, {0.18f, 0.38f}, {0.40f, 0.38f}, {0.52f, 0.54f}, {0.52f, 1.00f}}}};
        g['i'] = {0.28f, {{{0.10f, 0.36f}, {0.10f, 1.00f}}, dot(0.10f, 0.14f)}};
        {
            Stroke j{{0.26f, 0.36f}, {0.26f, 1.06f}};
            arcPts(j, 0.08f, 1.06f, 0.18f, 0.20f, 0.0f, 0.9f * PI, 6);
            g['j'] = {0.30f, {j, dot(0.26f, 0.14f)}};
        }
        g['k'] = {0.58f, {{{0.02f, 0.00f}, {0.02f, 1.00f}}, {{0.46f, 0.38f}, {0.06f, 0.74f}},
                          {{0.20f, 0.62f}, {0.48f, 1.00f}}}};
        g['l'] = {0.28f, {{{0.10f, 0.00f}, {0.10f, 1.00f}}}};
        g['m'] = {0.94f, {{{0.02f, 0.38f}, {0.02f, 1.00f}},
                          {{0.02f, 0.54f}, {0.14f, 0.38f}, {0.30f, 0.38f}, {0.40f, 0.54f}, {0.40f, 1.00f}},
                          {{0.40f, 0.54f}, {0.52f, 0.38f}, {0.68f, 0.38f}, {0.78f, 0.54f}, {0.78f, 1.00f}}}};
        g['n'] = {0.62f, {{{0.02f, 0.38f}, {0.02f, 1.00f}},
                          {{0.02f, 0.54f}, {0.16f, 0.38f}, {0.38f, 0.38f}, {0.50f, 0.54f}, {0.50f, 1.00f}}}};
        g['o'] = {0.62f, {ring(0.29f, 0.68f, 0.27f, 0.32f, 16)}};
        g['p'] = {0.64f, {{{0.02f, 0.38f}, {0.02f, 1.26f}}, ring(0.30f, 0.68f, 0.28f, 0.32f, 16)}};
        g['q'] = {0.64f, {{{0.56f, 0.38f}, {0.56f, 1.26f}}, ring(0.28f, 0.68f, 0.26f, 0.32f, 16)}};
        g['r'] = {0.42f, {{{0.02f, 0.38f}, {0.02f, 1.00f}}, {{0.02f, 0.56f}, {0.16f, 0.40f}, {0.38f, 0.37f}}}};
        g['s'] = {0.54f, {{{0.46f, 0.45f}, {0.32f, 0.36f}, {0.12f, 0.38f}, {0.04f, 0.52f}, {0.18f, 0.64f},
                           {0.40f, 0.70f}, {0.48f, 0.84f}, {0.36f, 0.98f}, {0.14f, 0.99f}, {0.02f, 0.90f}}}};
        g['t'] = {0.42f, {{{0.18f, 0.10f}, {0.18f, 0.86f}, {0.30f, 1.00f}, {0.44f, 0.96f}},
                          {{0.02f, 0.38f}, {0.40f, 0.38f}}}};
        g['u'] = {0.62f, {{{0.02f, 0.38f}, {0.02f, 0.82f}, {0.14f, 0.98f}, {0.36f, 0.98f}, {0.50f, 0.80f},
                           {0.50f, 0.38f}}, {{0.50f, 0.74f}, {0.50f, 1.00f}}}};
        g['v'] = {0.58f, {{{0.02f, 0.38f}, {0.27f, 1.00f}, {0.52f, 0.38f}}}};
        g['w'] = {0.84f, {{{0.02f, 0.38f}, {0.18f, 1.00f}, {0.36f, 0.54f}, {0.54f, 1.00f}, {0.70f, 0.38f}}}};
        g['x'] = {0.56f, {{{0.02f, 0.38f}, {0.50f, 1.00f}}, {{0.50f, 0.38f}, {0.02f, 1.00f}}}};
        g['y'] = {0.58f, {{{0.02f, 0.38f}, {0.29f, 0.96f}}, {{0.54f, 0.38f}, {0.16f, 1.24f}}}};
        g['z'] = {0.56f, {{{0.02f, 0.38f}, {0.50f, 0.38f}, {0.02f, 1.00f}, {0.52f, 1.00f}}}};

        // --- space and punctuation ---
        g[' '] = {0.34f, {}};
        g['.'] = {0.28f, {dot(0.10f, 0.96f)}};
        g[','] = {0.28f, {{{0.14f, 0.88f}, {0.12f, 1.02f}, {0.02f, 1.14f}}}};
        g[':'] = {0.28f, {dot(0.10f, 0.52f), dot(0.10f, 0.96f)}};
        g[';'] = {0.28f, {dot(0.10f, 0.52f), {{0.14f, 0.88f}, {0.12f, 1.02f}, {0.02f, 1.14f}}}};
        g['\''] = {0.24f, {{{0.09f, 0.02f}, {0.05f, 0.28f}}}};
        g['"'] = {0.38f, {{{0.09f, 0.02f}, {0.05f, 0.28f}}, {{0.24f, 0.02f}, {0.20f, 0.28f}}}};
        g['!'] = {0.28f, {{{0.10f, 0.02f}, {0.10f, 0.68f}}, dot(0.10f, 0.96f)}};
        g['?'] = {0.60f, {{{0.04f, 0.20f}, {0.14f, 0.03f}, {0.38f, 0.02f}, {0.50f, 0.18f}, {0.44f, 0.36f},
                           {0.28f, 0.50f}, {0.28f, 0.68f}}, dot(0.28f, 0.96f)}};
        g['-'] = {0.50f, {{{0.05f, 0.60f}, {0.45f, 0.60f}}}};
        g['+'] = {0.58f, {{{0.06f, 0.55f}, {0.52f, 0.55f}}, {{0.29f, 0.32f}, {0.29f, 0.78f}}}};
        g['_'] = {0.60f, {{{0.02f, 1.08f}, {0.56f, 1.08f}}}};
        g['('] = {0.36f, {{{0.28f, -0.06f}, {0.10f, 0.24f}, {0.06f, 0.54f}, {0.12f, 0.84f}, {0.28f, 1.12f}}}};
        g[')'] = {0.36f, {{{0.06f, -0.06f}, {0.24f, 0.24f}, {0.28f, 0.54f}, {0.22f, 0.84f}, {0.06f, 1.12f}}}};
        g['/'] = {0.52f, {{{0.02f, 1.06f}, {0.46f, -0.06f}}}};
        g['&'] = {0.76f, {{{0.60f, 1.00f}, {0.14f, 0.44f}, {0.20f, 0.10f}, {0.42f, 0.10f}, {0.44f, 0.34f},
                           {0.06f, 0.68f}, {0.12f, 0.96f}, {0.38f, 0.98f}, {0.62f, 0.72f}}}};
        return g;
    }();
    return kGlyphs;
}

// Unknown characters fall back to a space, so a caption with an emoji in it still lays out and still
// renders the words around it, rather than vanishing or drawing a row of boxes.
inline const Glyph& glyphFor(char c) {
    const auto& table = glyphs();
    const auto it = table.find(c);
    return it == table.end() ? table.at(' ') : it->second;
}

// Read one character from a UTF-8 string, advancing `i` past it, and fold it to a key this font has.
//
// The font is a one-byte table, but the films are not one-byte text: a subcaption reads "Drama . 3
// min" with a MIDDLE DOT in it, screenplay dialogue picks up curly quotes and em dashes, and a name
// can carry an accent. Walking the string byte by byte would turn each of those into two or three
// stray spaces. So the handful that actually occur are folded to their plain equivalents, accented
// Latin letters are folded to their base letter, and anything else becomes one space rather than one
// space per byte.
inline char nextGlyphKey(const std::string& text, std::size_t& i) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    if (c < 0x80u) {
        ++i;
        return static_cast<char>(c);
    }
    // How many bytes this sequence claims, so that `i` lands on the next character either way.
    std::size_t len = 1;
    if ((c & 0xE0u) == 0xC0u) {
        len = 2;
    } else if ((c & 0xF0u) == 0xE0u) {
        len = 3;
    } else if ((c & 0xF8u) == 0xF0u) {
        len = 4;
    }
    if (i + len > text.size()) {
        len = 1;
    }
    std::uint32_t cp = 0;
    if (len == 2) {
        cp = ((static_cast<std::uint32_t>(c) & 0x1Fu) << 6) |
             (static_cast<std::uint32_t>(static_cast<unsigned char>(text[i + 1])) & 0x3Fu);
    } else if (len == 3) {
        cp = ((static_cast<std::uint32_t>(c) & 0x0Fu) << 12) |
             ((static_cast<std::uint32_t>(static_cast<unsigned char>(text[i + 1])) & 0x3Fu) << 6) |
             (static_cast<std::uint32_t>(static_cast<unsigned char>(text[i + 2])) & 0x3Fu);
    }
    i += len;
    switch (cp) {
        case 0x00B7: case 0x2022: return '.';  // middle dot, bullet
        case 0x2018: case 0x2019: return '\''; // curly single quotes
        case 0x201C: case 0x201D: return '"';  // curly double quotes
        case 0x2013: case 0x2014: case 0x2212: return '-'; // en dash, em dash, minus
        case 0x2026: return '.';               // ellipsis (one dot: better than three spaces)
        default: break;
    }
    // Fold the Latin-1 accented letters to their base letter rather than dropping the word.
    if (cp >= 0x00C0 && cp <= 0x00FF) {
        static const char* kFold =
            "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYPsaaaaaaaceeeeiiiidnooooo/ouuuuypy";
        return kFold[cp - 0x00C0];
    }
    return ' ';
}

// Stroke one polyline into `out` as quads with round joins and caps. A single point becomes a dot.
inline void strokePolyline(Path& out, const std::vector<math::vec2>& pts, float halfWidth) {
    if (pts.empty()) {
        return;
    }
    // The round joins must be wound the SAME way as the quads below, or the nonzero rule cancels them
    // where they overlap -- which is at every corner of every letter -- and the text comes out riddled
    // with holes. The quads run p0+n, p1+n, p1-n, p0-n, which is the reverse of an ellipse's default
    // direction, so the joins are flipped to match. This is the same hazard that would cut a figure's
    // head out of its shoulders, and it is just as invisible until you look at the pixels.
    for (const math::vec2& p : pts) {
        out.ellipse(p.x, p.y, halfWidth, halfWidth, 0.0f, /*reversed*/ true);
    }
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const float dx = pts[i + 1].x - pts[i].x;
        const float dy = pts[i + 1].y - pts[i].y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f) {
            continue;
        }
        const float nx = -dy / len * halfWidth;
        const float ny = dx / len * halfWidth;
        out.moveTo(pts[i].x + nx, pts[i].y + ny);
        out.lineTo(pts[i + 1].x + nx, pts[i + 1].y + ny);
        out.lineTo(pts[i + 1].x - nx, pts[i + 1].y - ny);
        out.lineTo(pts[i].x - nx, pts[i].y - ny);
        out.close();
    }
}

} // namespace detail

// How wide `text` is, in pixels, at this style.
inline float textWidth(const std::string& text, const TextStyle& style) {
    float w = 0.0f;
    std::size_t i = 0;
    while (i < text.size()) {
        w += detail::glyphFor(detail::nextGlyphKey(text, i)).advance + style.tracking;
    }
    return w > 0.0f ? (w - style.tracking) * style.size : 0.0f;
}

// The whole string as ONE path, with the pen starting at (x, baselineY). Fill it with the NONZERO
// rule: every stroke quad and every round join is wound the same way, so they union instead of
// cancelling where they overlap -- which they do at every corner of every letter.
inline Path textPath(const std::string& text, float x, float baselineY, const TextStyle& style) {
    Path path;
    const float s = style.size;
    const float half = style.weight * s * 0.5f;
    float pen = x;
    std::size_t i = 0;
    while (i < text.size()) {
        const detail::Glyph& glyph = detail::glyphFor(detail::nextGlyphKey(text, i));
        for (const auto& stroke : glyph.strokes) {
            std::vector<math::vec2> scaled;
            scaled.reserve(stroke.size());
            for (const math::vec2& p : stroke) {
                scaled.push_back(math::vec2{pen + p.x * s, baselineY - s + p.y * s});
            }
            detail::strokePolyline(path, scaled, half);
        }
        pen += (glyph.advance + style.tracking) * s;
    }
    return path;
}

// The same, centred horizontally on `cx`.
inline Path textPathCentred(const std::string& text, float cx, float baselineY,
                            const TextStyle& style) {
    return textPath(text, cx - textWidth(text, style) * 0.5f, baselineY, style);
}

} // namespace maz::render
