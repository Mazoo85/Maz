// tests/ui/font.cpp — verifies ui::Font (TTF baking + text layout) and ui::DebugOverlay.
//
// Both are implemented in .cpp files and both talk to a render::Renderer, which is exactly why
// they had no test: there was no Renderer to give them without a GPU. There is now —
// tests/support/RecordingRenderer.hpp is a Renderer that draws nothing and records every call —
// so the real ui/Font.cpp and ui/DebugOverlay.cpp run here, unmodified, against a real TTF.
//
// Ground truths:
//   * a missing file and a refused texture upload both fail the load and leave valid() false;
//   * the baked atlas is square RGBA, white with alpha = glyph coverage (that is what makes the
//     sprite tint set the text colour), and it is uploaded exactly once per load;
//   * a size too large for the default 512x512 atlas grows the atlas instead of silently baking
//     half the alphabet -- every printable character draws and measures at every size;
//   * textWidth sums per-glyph advances, scales linearly, takes the widest line of a multi-line
//     string, and ignores bytes outside the baked 32..127 range;
//   * drawText emits one sprite per *visible* glyph — a space advances the pen and draws nothing;
//   * drawing the same text at a different position translates every sprite by exactly that much,
//     and '\n' translates by exactly lineHeight();
//   * drawTextCentered shifts by exactly half the measured width;
//   * scale multiplies each sprite's rectangle and leaves its UVs alone (same atlas cell);
//   * DebugOverlay draws nothing until enabled, smooths the frame time by the documented
//     0.9/0.1 blend, ignores non-positive deltas, and reads its counts from renderStats().
//
// The overlay assertions are exact because the sprites are decoded back into text: every glyph
// has a unique atlas cell, so a UV rectangle identifies a character. That turns "it drew 25
// sprites" into "it printed FPS 60 FRAME 16.67 ms", which is the thing worth checking.
#include "maz/ui/DebugOverlay.hpp"
#include "maz/ui/Font.hpp"

#include "../support/RecordingRenderer.hpp"

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::Color;
using maz::testing::RecordingRenderer;
using maz::ui::DebugOverlay;
using maz::ui::Font;

static bool nearf(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) < eps; }

static const Color kWhite{1.0f, 1.0f, 1.0f, 1.0f};

// --- decoding sprites back into text ---------------------------------------------------------

// A sprite's position is its pen position plus the glyph's own bearing, so comparing a sprite in
// a long string against a pen position is comparing two different things. Drawing each character
// on its own at the origin records that bearing, which makes the comparisons exact.
struct Solo {
    char ch = '?';
    float x = 0.0f; // sprite x when drawn alone at x = 0, i.e. the glyph's x bearing
    float y = 0.0f; // sprite y when drawn alone at y = 0, i.e. ascent + the glyph's y bearing
};

// Each baked glyph occupies its own cell in the atlas, so its UV rectangle is a unique key.
using CellMap = std::map<std::pair<float, float>, Solo>;

static CellMap buildCellMap(const Font& font) {
    CellMap cells;
    RecordingRenderer r;
    for (int c = 33; c < 127; ++c) {
        const char text[2] = {static_cast<char>(c), '\0'};
        r.clear();
        font.drawText(r, 0.0f, 0.0f, text, kWhite);
        if (r.sprites.size() == 1) {
            const auto& d = r.sprites[0].desc;
            cells[{d.uvMinX, d.uvMinY}] = Solo{static_cast<char>(c), d.x, d.y};
        }
    }
    return cells;
}

// The non-space characters of whatever the recorded sprites spell, in draw order.
static std::string decode(const RecordingRenderer& r, const CellMap& cells) {
    std::string out;
    for (const auto& s : r.sprites) {
        const auto it = cells.find({s.desc.uvMinX, s.desc.uvMinY});
        out.push_back(it == cells.end() ? '?' : it->second.ch);
    }
    return out;
}

// --- tests ------------------------------------------------------------------------------------

static void testLoadFailures(const char* ttfPath) {
    RecordingRenderer r;

    Font missing;
    CHECK(!missing.valid(), "a fresh Font is not valid");
    CHECK(!missing.load(r, "does/not/exist.ttf", 32.0f), "loading a missing file fails");
    CHECK(!missing.valid(), "a failed load leaves the Font invalid");
    CHECK(r.textures.empty(), "a failed read uploads nothing");

    // Drawing through an unloaded Font is a no-op, not a crash.
    missing.drawText(r, 0.0f, 0.0f, "hello", kWhite);
    CHECK(r.sprites.empty(), "an unloaded Font draws nothing");
    CHECK(nearf(missing.textWidth("hello"), 0.0f), "an unloaded Font measures zero");

    // A renderer that refuses the upload fails the load too.
    RecordingRenderer refuses;
    refuses.textureCreationFails = true;
    Font rejected;
    CHECK(!rejected.load(refuses, ttfPath, 32.0f), "a refused texture upload fails the load");
    CHECK(!rejected.valid(), "a refused upload leaves the Font invalid");
}

static void testAtlas(const Font& font, const RecordingRenderer& loadRenderer) {
    CHECK(font.valid(), "the font loaded");
    CHECK(loadRenderer.textures.size() == 1, "one load uploads exactly one atlas");
    if (loadRenderer.textures.size() != 1) {
        return;
    }
    const auto& atlas = loadRenderer.textures[0];
    CHECK(atlas.width == 512 && atlas.height == 512, "32 px bakes into the default 512x512 atlas");
    CHECK(atlas.pixels.size() == 512u * 512u * 4u, "the atlas is RGBA");

    bool allWhite = true;
    bool sawOpaque = false, sawClear = false;
    for (std::size_t i = 0; i + 3 < atlas.pixels.size(); i += 4) {
        if (atlas.pixels[i] != 255 || atlas.pixels[i + 1] != 255 || atlas.pixels[i + 2] != 255) {
            allWhite = false;
        }
        if (atlas.pixels[i + 3] == 255) sawOpaque = true;
        if (atlas.pixels[i + 3] == 0) sawClear = true;
    }
    // White RGB + coverage in alpha is the contract that lets drawSprite's tint set the colour.
    CHECK(allWhite, "every atlas texel is white; only alpha varies");
    CHECK(sawOpaque, "the atlas has fully covered texels (glyph interiors)");
    CHECK(sawClear, "the atlas has empty texels (the gaps between glyphs)");
}

static void testMeasuring(const Font& font) {
    CHECK(nearf(font.lineHeight(1.0f), 32.0f), "lineHeight is the baked pixel height");
    CHECK(nearf(font.lineHeight(2.5f), 80.0f), "lineHeight scales");

    CHECK(nearf(font.textWidth(nullptr), 0.0f), "null text measures zero");
    CHECK(nearf(font.textWidth(""), 0.0f), "empty text measures zero");

    const float a = font.textWidth("a");
    const float b = font.textWidth("b");
    CHECK(a > 0.0f && b > 0.0f, "letters have a positive advance");
    CHECK(nearf(font.textWidth("ab"), a + b), "textWidth sums advances (no kerning in Font)");

    const float wide = font.textWidth("the quick brown fox");
    CHECK(nearf(font.textWidth("the quick brown fox", 3.0f), wide * 3.0f),
          "textWidth is linear in scale");

    // A multi-line string measures its widest line, whichever line that is.
    CHECK(nearf(font.textWidth("ab\nthe quick brown fox"), wide), "multi-line takes the widest line");
    CHECK(nearf(font.textWidth("the quick brown fox\nab"), wide), "...even when it comes first");
    CHECK(nearf(font.textWidth("\n"), 0.0f), "a lone newline measures zero");

    // Only 32..127 is baked; anything else contributes nothing rather than indexing out of range.
    CHECK(nearf(font.textWidth("a\x01\x1f" "b"), a + b), "control bytes are ignored");
    CHECK(nearf(font.textWidth("a\xc3\xa9" "b"), a + b), "bytes above 127 are ignored");
}

static void testLayout(const Font& font) {
    RecordingRenderer r;

    font.drawText(r, 10.0f, 20.0f, nullptr, kWhite);
    CHECK(r.sprites.empty(), "null text draws nothing");
    r.clear();

    // A space advances the pen without drawing: "a b" is two sprites, not three.
    font.drawText(r, 0.0f, 0.0f, "a b", kWhite);
    CHECK(r.sprites.size() == 2, "a space draws no sprite");
    if (r.sprites.size() == 2) {
        // The 'b' sprite sits at the pen position after "a " plus b's own x bearing, which is
        // what drawing 'b' alone at x = 0 measures.
        RecordingRenderer solo;
        font.drawText(solo, 0.0f, 0.0f, "b", kWhite);
        CHECK(!solo.sprites.empty() &&
              nearf(r.sprites[1].desc.x, font.textWidth("a ") + solo.sprites[0].desc.x, 1e-3f),
              "...but it does advance the pen by the space's advance");
    }
    const maz::render::TextureHandle atlas = r.sprites.empty()
        ? maz::render::kInvalidTexture : r.sprites[0].texture;
    CHECK(atlas != maz::render::kInvalidTexture, "glyphs draw from the atlas texture");
    for (const auto& s : r.sprites) {
        CHECK(s.texture == atlas, "every glyph comes from the same atlas");
        CHECK(s.desc.uvMinX >= 0.0f && s.desc.uvMaxX <= 1.0f, "u stays in 0..1");
        CHECK(s.desc.uvMinY >= 0.0f && s.desc.uvMaxY <= 1.0f, "v stays in 0..1");
        CHECK(s.desc.uvMaxX > s.desc.uvMinX && s.desc.uvMaxY > s.desc.uvMinY,
              "a drawn glyph has a non-empty atlas cell");
        CHECK(s.desc.width > 0.0f && s.desc.height > 0.0f, "a drawn glyph has a non-empty rect");
    }

    // Moving the text moves every sprite by exactly that much, and nothing else changes.
    RecordingRenderer moved;
    font.drawText(moved, 137.0f, -45.5f, "a b", kWhite);
    CHECK(moved.sprites.size() == r.sprites.size(), "position does not change the glyph count");
    for (std::size_t i = 0; i < moved.sprites.size() && i < r.sprites.size(); ++i) {
        CHECK(nearf(moved.sprites[i].desc.x - r.sprites[i].desc.x, 137.0f, 1e-3f),
              "drawing is translated in x");
        CHECK(nearf(moved.sprites[i].desc.y - r.sprites[i].desc.y, -45.5f, 1e-3f),
              "drawing is translated in y");
        CHECK(nearf(moved.sprites[i].desc.width, r.sprites[i].desc.width),
              "position does not change glyph size");
    }

    // '\n' returns x to the start and drops y by exactly one line.
    RecordingRenderer wrapped;
    font.drawText(wrapped, 0.0f, 0.0f, "ab\nab", kWhite);
    CHECK(wrapped.sprites.size() == 4, "two lines of two glyphs draw four sprites");
    if (wrapped.sprites.size() == 4) {
        for (int i = 0; i < 2; ++i) {
            const auto& top = wrapped.sprites[static_cast<std::size_t>(i)];
            const auto& bot = wrapped.sprites[static_cast<std::size_t>(i) + 2];
            CHECK(nearf(bot.desc.x, top.desc.x), "a new line returns the pen to the left margin");
            CHECK(nearf(bot.desc.y - top.desc.y, font.lineHeight(1.0f), 1e-3f),
                  "a new line drops by exactly lineHeight");
        }
    }

    // Centering is a shift of exactly half the measured width.
    RecordingRenderer left, centered;
    const float half = font.textWidth("a b") * 0.5f;
    font.drawText(left, 500.0f - half, 0.0f, "a b", kWhite);
    font.drawTextCentered(centered, 500.0f, 0.0f, "a b", kWhite);
    CHECK(centered.sprites.size() == left.sprites.size(), "centering draws the same glyphs");
    for (std::size_t i = 0; i < centered.sprites.size() && i < left.sprites.size(); ++i) {
        CHECK(nearf(centered.sprites[i].desc.x, left.sprites[i].desc.x, 1e-3f),
              "drawTextCentered centres on the measured width");
    }

    // Scale multiplies the rectangle and leaves the atlas cell alone.
    RecordingRenderer big;
    font.drawText(big, 0.0f, 0.0f, "a b", kWhite, 2.0f);
    CHECK(big.sprites.size() == r.sprites.size(), "scale does not change the glyph count");
    for (std::size_t i = 0; i < big.sprites.size() && i < r.sprites.size(); ++i) {
        CHECK(nearf(big.sprites[i].desc.width, r.sprites[i].desc.width * 2.0f, 1e-3f),
              "scale multiplies glyph width");
        CHECK(nearf(big.sprites[i].desc.height, r.sprites[i].desc.height * 2.0f, 1e-3f),
              "scale multiplies glyph height");
        CHECK(nearf(big.sprites[i].desc.uvMinX, r.sprites[i].desc.uvMinX),
              "scale re-uses the same atlas cell");
    }

    // Capitals drawn with their top-left at y sit inside the line box below it.
    RecordingRenderer caps;
    font.drawText(caps, 0.0f, 0.0f, "AZ", kWhite);
    for (const auto& s : caps.sprites) {
        CHECK(s.desc.y >= 0.0f, "a capital sits below the text's top edge");
        CHECK(s.desc.y + s.desc.height <= font.lineHeight(1.0f) + 1e-3f,
              "a capital sits inside the line box");
    }

    // The tint is passed straight through, unmodified.
    RecordingRenderer tinted;
    const Color plum{0.4f, 0.1f, 0.7f, 0.5f};
    font.drawText(tinted, 0.0f, 0.0f, "a", plum);
    CHECK(tinted.sprites.size() == 1, "one glyph, one sprite");
    if (!tinted.sprites.empty()) {
        const Color& c = tinted.sprites[0].desc.color;
        CHECK(nearf(c.r, plum.r) && nearf(c.g, plum.g) && nearf(c.b, plum.b) && nearf(c.a, plum.a),
              "the colour reaches the sprite untouched");
    }
}

static void testOverlay(const Font& font, const CellMap& cells) {
    RecordingRenderer r;
    DebugOverlay overlay;

    CHECK(!overlay.enabled(), "the overlay is off by default");
    overlay.draw(r, const_cast<Font&>(font), 0.0f, 0.0f);
    CHECK(r.sprites.empty(), "a disabled overlay draws nothing");

    overlay.toggle();
    CHECK(overlay.enabled(), "toggle turns it on");
    overlay.toggle();
    CHECK(!overlay.enabled(), "toggle turns it off again");
    overlay.setEnabled(true);
    CHECK(overlay.enabled(), "setEnabled turns it on");

    // Non-positive deltas are ignored, so a stalled or mis-measured frame cannot poison the
    // readout. The overlay starts at 1/60 s, so it should still read exactly 60 FPS.
    r.stats.meshDraws = 7;
    r.stats.culled = 3;
    r.stats.particles = 5;
    r.stats.sprites = 11;
    overlay.update(0.0);
    overlay.update(-1.0);
    r.clear();
    overlay.draw(r, const_cast<Font&>(font), 0.0f, 0.0f);
    CHECK(decode(r, cells) == "FPS60FRAME16.67msMESH7(culled3)PART5SPR11",
          "the overlay prints 60 FPS / 16.67 ms and the renderer's own counts");
    if (decode(r, cells) != "FPS60FRAME16.67msMESH7(culled3)PART5SPR11") {
        std::printf("  decoded: %s\n", decode(r, cells).c_str());
    }

    // The overlay's own colour, on every glyph.
    for (const auto& s : r.sprites) {
        CHECK(nearf(s.desc.color.r, 0.6f) && nearf(s.desc.color.g, 1.0f) &&
              nearf(s.desc.color.b, 0.7f) && nearf(s.desc.color.a, 1.0f),
              "the overlay draws in its own colour");
    }

    // Two lines, the second exactly 22 px below the first. The line break is where the pen jumps
    // back to the left margin; subtracting each glyph's own bearing turns two sprite positions
    // into the two baselines, so this compares like with like whatever characters are printed.
    std::size_t wrap = 0;
    for (std::size_t i = 1; i < r.sprites.size(); ++i) {
        if (r.sprites[i].desc.x < r.sprites[i - 1].desc.x) {
            wrap = i;
            break;
        }
    }
    CHECK(wrap != 0, "the overlay draws a second line");
    if (wrap != 0) {
        const auto first = cells.find({r.sprites[0].desc.uvMinX, r.sprites[0].desc.uvMinY});
        const auto second = cells.find({r.sprites[wrap].desc.uvMinX, r.sprites[wrap].desc.uvMinY});
        CHECK(first != cells.end() && second != cells.end(), "both line starts decode to a glyph");
        if (first != cells.end() && second != cells.end()) {
            const float gap = (r.sprites[wrap].desc.y - second->second.y) -
                              (r.sprites[0].desc.y - first->second.y);
            CHECK(nearf(gap, 22.0f, 1e-3f), "the second overlay line sits exactly 22 px lower");
        }
    }

    // One frame at 1/30 s blends as 0.9*(1/60) + 0.1*(1/30) = 0.0183333 s -> 18.33 ms, 54.5 FPS,
    // which "%4.0f" rounds to 55. That pins the smoothing constant, not just its direction.
    overlay.update(1.0 / 30.0);
    r.clear();
    overlay.draw(r, const_cast<Font&>(font), 0.0f, 0.0f);
    const std::string blended = decode(r, cells);
    CHECK(blended.rfind("FPS55FRAME18.33ms", 0) == 0,
          "one slow frame blends 90/10 into the readout");
    if (blended.rfind("FPS55FRAME18.33ms", 0) != 0) {
        std::printf("  decoded: %s\n", blended.c_str());
    }

    // Held at 1/30 s the readout converges to it rather than drifting past.
    for (int i = 0; i < 400; ++i) {
        overlay.update(1.0 / 30.0);
    }
    r.clear();
    overlay.draw(r, const_cast<Font&>(font), 0.0f, 0.0f);
    const std::string settled = decode(r, cells);
    CHECK(settled.rfind("FPS30FRAME33.33ms", 0) == 0,
          "a sustained 30 FPS settles on 30 FPS / 33.33 ms");
    if (settled.rfind("FPS30FRAME33.33ms", 0) != 0) {
        std::printf("  decoded: %s\n", settled.c_str());
    }

    // Disabling it again silences it, whatever it has been fed.
    overlay.setEnabled(false);
    r.clear();
    overlay.draw(r, const_cast<Font&>(font), 0.0f, 0.0f);
    CHECK(r.sprites.empty(), "disabling silences the overlay again");
}

// A 512x512 atlas holds 32..127 up to about 64 px. Past that, stbtt_BakeFontBitmap bakes only
// the characters that fit and reports the shortfall as a NEGATIVE return, which used to be read
// as success: load() returned true, the glyphs that did not fit kept a zero-size cell and a zero
// advance, and the font silently dropped letters. At 96 px DejaVu Sans lost 15 of 94 printable
// characters and textWidth("Hello") was 0.0 -- so every drawTextCentered call at that size
// centred on nothing. The atlas now grows until the whole range fits.
static void testLargeSizes(const char* ttfPath) {
    for (const float height : {32.0f, 48.0f, 64.0f, 96.0f, 128.0f, 192.0f}) {
        RecordingRenderer r;
        Font font;
        char msg[96];
        std::snprintf(msg, sizeof(msg), "%.0f px loads", static_cast<double>(height));
        if (!font.load(r, ttfPath, height)) {
            std::printf("FAIL: %s\n", msg);
            ++g_fail;
            continue;
        }

        int drawn = 0, measured = 0;
        for (int c = 33; c < 127; ++c) {
            const char text[2] = {static_cast<char>(c), '\0'};
            r.clear();
            font.drawText(r, 0.0f, 0.0f, text, kWhite);
            if (!r.sprites.empty()) ++drawn;
            if (font.textWidth(text) > 0.0f) ++measured;
        }
        std::snprintf(msg, sizeof(msg), "%.0f px draws all 94 printable glyphs (got %d)",
                      static_cast<double>(height), drawn);
        CHECK(drawn == 94, msg);
        std::snprintf(msg, sizeof(msg), "%.0f px measures all 94 printable glyphs (got %d)",
                      static_cast<double>(height), measured);
        CHECK(measured == 94, msg);

        // The sanity check that used to fail outright: a real word has a real width.
        std::snprintf(msg, sizeof(msg), "%.0f px measures \"Hello\" as wider than nothing",
                      static_cast<double>(height));
        CHECK(font.textWidth("Hello") > height, msg);

        // Whatever size the atlas grew to, it is still square and still RGBA.
        CHECK(r.textures.empty(), "clear() emptied the recorder between probes");
        RecordingRenderer fresh;
        Font again;
        CHECK(again.load(fresh, ttfPath, height), "the same size loads again");
        CHECK(fresh.textures.size() == 1, "one load, one atlas, whatever its size");
        if (fresh.textures.size() == 1) {
            const auto& a = fresh.textures[0];
            CHECK(a.width == a.height, "the grown atlas stays square");
            CHECK(a.width >= 512 && a.width <= 4096, "the atlas stays within its bounds");
            CHECK(a.pixels.size() == static_cast<std::size_t>(a.width) * a.height * 4,
                  "the grown atlas is RGBA");
        }
    }
}

int main(int argc, char** argv) {
    const char* ttfPath = argc > 1 ? argv[1] : "assets/fonts/DejaVuSans.ttf";

    testLoadFailures(ttfPath);

    RecordingRenderer loadRenderer;
    Font font;
    if (!font.load(loadRenderer, ttfPath, 32.0f)) {
        std::printf("FAIL: could not load '%s'\n", ttfPath);
        return 1;
    }
    testAtlas(font, loadRenderer);
    testMeasuring(font);
    testLayout(font);
    testOverlay(font, buildCellMap(font));
    testLargeSizes(ttfPath);

    if (g_fail == 0) {
        std::printf("font/overlay tests passed\n");
    } else {
        std::printf("%d font/overlay checks failed\n", g_fail);
    }
    return g_fail == 0 ? 0 : 1;
}
