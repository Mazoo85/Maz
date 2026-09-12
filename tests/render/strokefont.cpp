// tests/render/strokefont.cpp — verifies CPU text (render StrokeFont.hpp).
//
// The engine's only text was ui::Font, which bakes a TTF through stb_truetype and draws glyph sprites
// through the Vulkan renderer -- no use to anything headless. This draws text as geometry into a
// render::Image with no GPU and no font file.
//
// The check that matters is SOLIDITY. A glyph is stroke quads plus a round join at every vertex, all
// unioned into one path and filled once under the nonzero rule. If the joins are wound opposite to the
// quads they CANCEL where they overlap -- at every corner of every letter -- and the text comes out
// riddled with holes. Nothing in the geometry says which happened; the first version of this font had
// exactly that bug and it was found by looking at a rendered specimen, so it is pinned here on pixels.
#include "maz/render/PathFill.hpp"
#include "maz/render/StrokeFont.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using maz::render::Color;
using maz::render::Image;
using maz::render::Path;
using maz::render::TextStyle;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static const Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};
static const Color kBlack{0.0f, 0.0f, 0.0f, 1.0f};

int main() {
    const TextStyle style{40.0f, 0.09f, 0.05f};

    // --- 1. Measuring. ---
    {
        CHECK(maz::render::textWidth("", style) == 0.0f, "empty text is zero wide");
        const float one = maz::render::textWidth("M", style);
        const float two = maz::render::textWidth("MM", style);
        CHECK(one > 0.0f, "a letter has width");
        CHECK(two > one, "two letters are wider than one");
        TextStyle big = style;
        big.size = 80.0f;
        CHECK(std::fabs(maz::render::textWidth("HELLO", big) -
                        2.0f * maz::render::textWidth("HELLO", style)) < 0.01f,
              "doubling the size doubles the width");
        CHECK(maz::render::textWidth("i", style) < maz::render::textWidth("m", style),
              "an i is narrower than an m");
    }

    // --- 2. Every printable character lays down ink, and a space does not. ---
    {
        bool allInk = true;
        std::string bare;
        for (char c = 33; c < 127; ++c) {
            const std::string s(1, c);
            Image img(120, 120, kBlack);
            Path p = maz::render::textPath(s, 20.0f, 90.0f, style);
            maz::render::fillPath(img, p, kWhite);
            double total = 0.0;
            for (int y = 0; y < 120; ++y) {
                for (int x = 0; x < 120; ++x) total += static_cast<double>(img.getPixel(x, y).r);
            }
            // Unknown characters fall back to a space, which is legitimately blank.
            const bool known = maz::render::detail::glyphs().count(c) != 0;
            if (known && total < 5.0) {
                allInk = false;
                if (bare.empty()) bare = s;
            }
        }
        CHECK(allInk, ("every known character draws something (first blank: " + bare + ")").c_str());

        Image img(120, 120, kBlack);
        maz::render::fillPath(img, maz::render::textPath(" ", 20.0f, 90.0f, style), kWhite);
        double total = 0.0;
        for (int y = 0; y < 120; ++y) {
            for (int x = 0; x < 120; ++x) total += static_cast<double>(img.getPixel(x, y).r);
        }
        CHECK(total < 0.01, "a space draws nothing");
    }

    // --- 3. Solidity: every joint of every glyph is filled, not cancelled to a hole. ---
    //
    // A glyph's round joins sit exactly on its polyline vertices, overlapping the stroke quads that
    // meet there. If the two are wound against each other the nonzero rule punches a hole at precisely
    // these points -- so these are precisely the points to check.
    {
        const float size = 200.0f;
        const TextStyle big{size, 0.09f, 0.05f};
        bool solid = true;
        std::string bad;
        for (const auto& entry : maz::render::detail::glyphs()) {
            const char c = entry.first;
            if (c == ' ') {
                continue;
            }
            Image img(500, 500, kBlack);
            const std::string s(1, c);
            maz::render::fillPath(img, maz::render::textPath(s, 100.0f, 320.0f, big), kWhite);
            for (const auto& stroke : entry.second.strokes) {
                for (const auto& pt : stroke) {
                    const int px = static_cast<int>(std::lround(100.0f + pt.x * size));
                    const int py = static_cast<int>(std::lround(320.0f - size + pt.y * size));
                    if (img.getPixel(px, py).r < 0.95f) {
                        solid = false;
                        if (bad.empty()) {
                            bad = std::string("'") + c + "' at (" + std::to_string(px) + "," +
                                  std::to_string(py) + ") reads " +
                                  std::to_string(img.getPixel(px, py).r);
                        }
                    }
                }
            }
        }
        CHECK(solid, ("every joint of every glyph is solid, not a hole; first: " + bad).c_str());
    }

    // --- 4. A letter with a counter still has its counter. ---
    //
    // Solidity must not have been bought by filling everything in: an O has to stay a ring.
    {
        const TextStyle big{200.0f, 0.09f, 0.05f};
        Image img(400, 400, kBlack);
        maz::render::fillPath(img, maz::render::textPath("O", 60.0f, 320.0f, big), kWhite);
        // The centre of the O, which is the middle of its bowl.
        CHECK(img.getPixel(60 + 66, 320 - 100).r < 0.05f, "the inside of an O is still empty");
    }

    // --- 5. UTF-8 characters the films actually use are folded, not spat out as stray spaces. ---
    {
        // A middle dot is two bytes; walking bytes would make it two spaces and shift the line.
        const float dotted = maz::render::textWidth("Drama \xC2\xB7 3 min", style);
        const float plain = maz::render::textWidth("Drama . 3 min", style);
        CHECK(std::fabs(dotted - plain) < 0.01f, "a middle dot measures as one character, not two");

        const float curly = maz::render::textWidth("\xE2\x80\x9CYes\xE2\x80\x9D", style);
        const float straight = maz::render::textWidth("\"Yes\"", style);
        CHECK(std::fabs(curly - straight) < 0.01f, "curly quotes measure as one character each");

        const float dash = maz::render::textWidth("a\xE2\x80\x94""b", style);
        const float hyphen = maz::render::textWidth("a-b", style);
        CHECK(std::fabs(dash - hyphen) < 0.01f, "an em dash measures as one character");

        const float accented = maz::render::textWidth("caf\xC3\xA9", style);
        const float folded = maz::render::textWidth("cafe", style);
        CHECK(std::fabs(accented - folded) < 0.01f, "an accented letter folds to its base letter");

        // A truncated sequence at the end must not read past the string.
        const float truncated = maz::render::textWidth("ok\xE2\x80", style);
        CHECK(truncated > 0.0f, "a truncated UTF-8 tail does not run off the end of the string");
    }

    // --- 6. Centring puts the middle of the text on the mark. ---
    {
        const std::string s = "CENTRED";
        const float w = maz::render::textWidth(s, style);
        const Path a = maz::render::textPathCentred(s, 300.0f, 100.0f, style);
        maz::math::vec2 lo{}, hi{};
        a.bounds(lo, hi);
        const float middle = (lo.x + hi.x) * 0.5f;
        CHECK(std::fabs(middle - 300.0f) < w * 0.05f, "centred text is centred on its mark");
    }

    // --- 7. The same text always gives the same path. ---
    {
        const Path a = maz::render::textPath("Repeatable.", 10.0f, 50.0f, style);
        const Path b = maz::render::textPath("Repeatable.", 10.0f, 50.0f, style);
        bool same = a.contours().size() == b.contours().size();
        for (std::size_t i = 0; same && i < a.contours().size(); ++i) {
            same = a.contours()[i].size() == b.contours()[i].size();
            for (std::size_t j = 0; same && j < a.contours()[i].size(); ++j) {
                same = a.contours()[i][j].x == b.contours()[i][j].x &&
                       a.contours()[i][j].y == b.contours()[i][j].y;
            }
        }
        CHECK(same, "the same text always gives the same path");
    }

    // --- 8. Text sits on its baseline. ---
    {
        const Path p = maz::render::textPath("HELLO", 0.0f, 100.0f, style);
        maz::math::vec2 lo{}, hi{};
        p.bounds(lo, hi);
        // Capitals sit between the cap line and the baseline, allowing for the stroke's half width.
        CHECK(hi.y < 100.0f + style.size * style.weight, "capitals rest on the baseline");
        CHECK(lo.y > 100.0f - style.size * 1.15f, "capitals do not rise above the cap line");
    }

    if (g_fail == 0) {
        std::printf("stroke font: all checks passed (%zu glyphs)\n",
                    maz::render::detail::glyphs().size());
    }
    return g_fail == 0 ? 0 : 1;
}
