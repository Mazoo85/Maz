// Maz Engine — "SETTING" (ui::layoutText, TextServer's decodeUtf8 / baseDirection / bidiRuns /
// lineBreakOpportunities, parseBBCode / stripBBCode, FontFallback, shapeGlyphs, GlyphCache —
// everything that happens between a string and a picture of that string)
// Drawing text is the last step and the easy one. Before it: deciding where the line breaks, which
// direction each part of it runs, which font owns each character, which pairs of letters merge or pull
// together, and what to keep in the atlas. All of it is arithmetic, so all of it is checkable — and the
// measuring function here makes every character exactly ten units wide, which turns "where does it
// wrap" from a judgement into a sum you can do in your head. LEFT: the same sentence in three box
// widths, a word too long to fit, and what alignment does and does not change. MIDDLE: mixed
// left-to-right and right-to-left text split into runs, where a line is allowed to break, and BBCode
// turned into styled spans — including a tag nobody defined, which must survive as text rather than
// being eaten. RIGHT: five runs across three fonts from one string, a ligature that replaces two
// glyphs with one narrower one, and an atlas filling up.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 697 headers.
#include "maz/ui/FontFallback.hpp"
#include "maz/ui/GlyphCache.hpp"
#include "maz/ui/RichText.hpp"
#include "maz/ui/TextLayout.hpp"
#include "maz/ui/TextServer.hpp"
#include "maz/ui/TextShaping.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 0) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

// Every character exactly ten units wide. A real font's measure is a table lookup; this one makes the
// wrap points arithmetic, so the panel can be checked rather than admired.
float tenWide(std::string_view s) { return static_cast<float>(s.size()) * 10.0f; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SETTING starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Setting";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- where the line breaks ------------------------------------------------------------------------
    const std::string prose = "The quick brown fox jumps over the lazy dog";
    struct Wrapped {
        float box = 0.0f;
        std::vector<ui::TextLine> lines;
        float widest = 0.0f;
    };
    std::vector<Wrapped> wrapped;
    for (float w : {200.0f, 120.0f, 80.0f}) {
        const ui::TextLayout l = ui::layoutText(prose, w, tenWide, 16.0f);
        wrapped.push_back(Wrapped{w, l.lines, l.width});
    }
    const ui::TextLayout longWord = ui::layoutText("a supercalifragilistic b", 80.0f, tenWide, 16.0f);
    struct Aligned {
        std::string what;
        std::vector<float> x;
        std::vector<std::string> text;
    };
    std::vector<Aligned> aligned;
    for (ui::TextAlign a : {ui::TextAlign::Left, ui::TextAlign::Center, ui::TextAlign::Right}) {
        const ui::TextLayout l = ui::layoutText("one\n\ntwo words", 120.0f, tenWide, 16.0f, a);
        Aligned row;
        row.what = a == ui::TextAlign::Left ? "left" : (a == ui::TextAlign::Center ? "centre" : "right");
        for (const ui::TextLine& ln : l.lines) {
            row.x.push_back(ln.x);
            row.text.push_back(ln.text);
        }
        aligned.push_back(row);
    }

    // ---- which way does it run -------------------------------------------------------------------------
    struct Directional {
        std::string what;
        std::size_t codepoints = 0;
        bool baseRtl = false;
        std::vector<ui::BidiRun> runs;
    };
    std::vector<Directional> directional;
    {
        struct Case {
            const char* what;
            const char* utf8;
        };
        const Case cases[] = {
            {"plain English", "hello world"},
            {"Arabic alone", "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7"},
            {"English, then Arabic", "price: \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7"},
            {"Arabic, then English", "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 ok"},
        };
        for (const Case& c : cases) {
            const std::u32string t = ui::decodeUtf8(c.utf8);
            directional.push_back(Directional{c.what, t.size(),
                                              ui::baseDirection(t) == ui::Direction::Rtl,
                                              ui::bidiRuns(t)});
        }
    }
    const std::string breakSubject = "hello world-wide web";
    const std::vector<std::size_t> breaks =
        ui::lineBreakOpportunities(ui::decodeUtf8(breakSubject));

    // ---- tags into spans --------------------------------------------------------------------------------
    const std::string marked =
        "plain [b]bold[/b] and [color=#ff4020]orange[/color] and [i]slanted[/i]";
    const std::vector<ui::RichSpan> spans = ui::parseBBCode(marked);
    const std::string stripped = ui::stripBBCode(marked);
    const std::string unknownTag = "a [notatag]b[/notatag] c";
    const std::string unknownKept = ui::stripBBCode(unknownTag);

    // ---- which font owns each character -----------------------------------------------------------------
    std::vector<ui::FontFallback::FontRun> fontRuns;
    std::size_t mixedLength = 0;
    int uncovered = 0;
    {
        ui::FontFallback chain;
        chain.addFont(1);
        chain.coverRange(1, 0x20, 0x7E); // latin
        chain.addFont(2);
        chain.coverRange(2, 0x600, 0x6FF); // arabic
        chain.addFont(3);
        chain.coverRange(3, 0x4E00, 0x9FFF); // cjk
        const std::u32string mixed =
            ui::decodeUtf8("hi \xd9\x85\xd8\xb1 \xe6\x97\xa5\xe6\x9c\xac ok");
        mixedLength = mixed.size();
        fontRuns = chain.runs(mixed);
        uncovered = chain.fontFor(U'✓'); // a tick nothing declared
    }

    // ---- ligatures and kerning ---------------------------------------------------------------------------
    std::size_t plainGlyphs = 0;
    float plainWidth = 0.0f;
    std::size_t ligatedGlyphs = 0;
    float ligatedWidth = 0.0f;
    std::vector<ui::ShapedGlyph> ligated;
    {
        ui::ShapingTable table;
        for (std::uint32_t g = 1; g <= 6; ++g) {
            table.advance[g] = 10.0f;
        }
        table.advance[100] = 14.0f; // the merged glyph, narrower than its two parts
        table.ligatures.push_back(ui::LigatureRule{{1u, 2u}, 100u});
        table.kern[(static_cast<std::uint64_t>(3) << 32) | 4u] = -3.0f;
        const std::vector<ui::ShapedGlyph> a = ui::shapeGlyphs({3u, 4u, 5u}, table);
        ligated = ui::shapeGlyphs({1u, 2u, 5u}, table);
        plainGlyphs = a.size();
        plainWidth = ui::shapedWidth(a);
        ligatedGlyphs = ligated.size();
        ligatedWidth = ui::shapedWidth(ligated);
    }

    // ---- the atlas ---------------------------------------------------------------------------------------
    struct CacheStep {
        std::string what;
        std::size_t cached = 0;
        int rasterCalls = 0;
        float occupancy = 0.0f;
    };
    std::vector<CacheStep> cacheSteps;
    {
        int rasterCalls = 0;
        ui::GlyphCache cache(256, 256, [&rasterCalls](const ui::GlyphKey& k) {
            ++rasterCalls;
            ui::GlyphBitmap b;
            b.metrics.width = 12 + static_cast<int>(k.codepoint % 5);
            b.metrics.height = 16;
            b.metrics.advance = static_cast<float>(b.metrics.width) + 1.0f;
            return b;
        });
        const std::string word = "hello";
        for (char c : word) {
            cache.get(ui::GlyphKey{1u, static_cast<std::uint32_t>(c), 16u});
        }
        cacheSteps.push_back(CacheStep{"after \"hello\"", cache.count(), rasterCalls, cache.occupancy()});
        for (char c : word) {
            cache.get(ui::GlyphKey{1u, static_cast<std::uint32_t>(c), 16u});
        }
        cacheSteps.push_back(CacheStep{"the same word again", cache.count(), rasterCalls, cache.occupancy()});
        for (char c : word) {
            cache.get(ui::GlyphKey{1u, static_cast<std::uint32_t>(c), 32u}); // a bigger size
        }
        cacheSteps.push_back(CacheStep{"...at a bigger size", cache.count(), rasterCalls, cache.occupancy()});
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.27f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SETTING", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "everything between a string and a picture of it — with every character "
                          "exactly ten wide, so the wrapping is a sum you can check",
                          kDim, 0.32f);

            auto cell = [&](float x, float y, const std::string& s, render::Color colour, float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };

            // ---- column 1 ----
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "WHERE THE LINE BREAKS", kHead, 0.34f);
            y += 28.0f;
            for (const Wrapped& w : wrapped) {
                cell(24.0f, y,
                     "a box " + num(static_cast<double>(w.box)) + " wide — " +
                         num(static_cast<double>(w.box / 10.0f)) + " characters, " +
                         std::to_string(w.lines.size()) + " lines",
                     kVal, 0.26f);
                y += 21.0f;
                for (const ui::TextLine& ln : w.lines) {
                    cell(40.0f, y, "\"" + ln.text + "\"", kText, sz);
                    cell(310.0f, y, num(static_cast<double>(ln.width)), kDim, sz);
                    y += 20.0f;
                }
                y += 5.0f;
            }
            y += 4.0f;
            cell(24.0f, y, "a word wider than the box is never cut:", kVal, 0.26f);
            y += 21.0f;
            for (const ui::TextLine& ln : longWord.lines) {
                cell(40.0f, y, "\"" + ln.text + "\"  " + num(static_cast<double>(ln.width)), kText, sz);
                y += 20.0f;
            }
            y += 8.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Greedy wrapping: take words until the next one would not fit. Nineteen "
                          "characters fit a box of 200 and twenty-five do not, so the fox line ends "
                          "where it does — and a word longer than the whole box gets a line to itself "
                          "rather than being broken, because a hyphen the author did not write is a "
                          "worse answer than a line that overflows.",
                          kDim, 0.25f);

            // ---- column 2 ----
            y = 98.0f;
            font.drawText(*renderer, 470.0f, y, "ALIGNMENT MOVES X, NOTHING ELSE", kHead, 0.33f);
            y += 28.0f;
            for (const Aligned& a : aligned) {
                cell(470.0f, y, a.what, kText, sz);
                float x = 560.0f;
                for (std::size_t i = 0; i < a.x.size(); ++i) {
                    cell(x, y, "\"" + a.text[i] + "\"@" + num(static_cast<double>(a.x[i])), kVal, 0.25f);
                    x += 105.0f;
                }
                y += 22.0f;
            }
            y += 6.0f;
            cell(470.0f, y, "the empty line between the two is still a line", kDim, 0.25f);
            y += 26.0f;

            font.drawText(*renderer, 470.0f, y, "WHICH WAY DOES IT RUN", kHead, 0.33f);
            y += 28.0f;
            for (const Directional& d : directional) {
                cell(470.0f, y, d.what, kText, sz);
                cell(660.0f, y, d.baseRtl ? "base rtl" : "base ltr", d.baseRtl ? kOk : kDim, sz);
                std::string runs;
                for (const ui::BidiRun& r : d.runs) {
                    runs += (runs.empty() ? "" : " ");
                    runs += std::string(r.rtl ? "rtl" : "ltr") + "x" + std::to_string(r.length);
                }
                cell(760.0f, y, runs, kVal, sz);
                y += 22.0f;
            }
            y += 6.0f;
            {
                std::string list;
                for (std::size_t b : breaks) {
                    list += (list.empty() ? "" : ", ") + std::to_string(b);
                }
                cell(470.0f, y, "\"" + breakSubject + "\" may break before: " + list, kDim, 0.25f);
            }
            y += 24.0f;
            font.drawText(*renderer, 470.0f, y,
                          "The space between the Arabic and the \"ok\" joins the right-to-left run, "
                          "not the left-to-right one — a neutral character takes the direction of what "
                          "came before it, which is what stops a space flipping a phrase inside out.",
                          kDim, 0.25f);

            y += 66.0f;
            font.drawText(*renderer, 470.0f, y, "TAGS INTO SPANS", kHead, 0.33f);
            y += 28.0f;
            for (const ui::RichSpan& s : spans) {
                std::string style;
                if (s.bold) style += "bold ";
                if (s.italic) style += "italic ";
                if (s.hasColor) {
                    style += "rgb " + num(static_cast<double>(s.r) * 255.0) + "," +
                             num(static_cast<double>(s.g) * 255.0) + "," +
                             num(static_cast<double>(s.b) * 255.0);
                }
                cell(470.0f, y, "\"" + s.text + "\"", kText, sz);
                cell(660.0f, y, style.empty() ? "plain" : style, style.empty() ? kDim : kVal, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(470.0f, y, "a tag nobody defined: " + unknownKept, kOk, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 470.0f, y,
                          "That last line is the one that matters. A parser that swallowed unknown "
                          "tags would quietly delete whatever a writer mistyped; this one hands it "
                          "back as text, so the mistake is visible instead of missing.",
                          kDim, 0.25f);

            // ---- column 3 ----
            y = 98.0f;
            font.drawText(*renderer, 950.0f, y, "WHICH FONT OWNS EACH LETTER", kHead, 0.33f);
            y += 28.0f;
            cell(950.0f, y,
                 std::to_string(mixedLength) + " characters of Latin, Arabic and Japanese -> " +
                     std::to_string(fontRuns.size()) + " runs",
                 kVal, 0.26f);
            y += 22.0f;
            {
                std::string runs;
                for (const ui::FontFallback::FontRun& r : fontRuns) {
                    runs += (runs.empty() ? "" : "  ");
                    runs += "font " + std::to_string(r.fontId) + " x" + std::to_string(r.length);
                }
                cell(950.0f, y, runs, kText, sz);
            }
            y += 22.0f;
            cell(950.0f, y,
                 "a character nothing covers -> " + std::to_string(uncovered) +
                     (uncovered == ui::FontFallback::kNone ? "  (no font, not font 0)" : ""),
                 uncovered == ui::FontFallback::kNone ? kOk : kDim, 0.25f);
            y += 26.0f;
            font.drawText(*renderer, 950.0f, y,
                          "One string, three fonts, five runs — and the Latin font is picked up again "
                          "for the space in the middle, because coverage is per character, not per "
                          "word. An uncovered character reports NO font rather than defaulting to the "
                          "first, so a missing glyph is a decision the caller makes.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 950.0f, y, "LETTERS THAT MERGE AND PULL", kHead, 0.33f);
            y += 28.0f;
            cell(950.0f, y,
                 "three glyphs, one kerned pair: " + std::to_string(plainGlyphs) + " out, width " +
                     num(static_cast<double>(plainWidth)),
                 kVal, sz);
            y += 22.0f;
            cell(950.0f, y,
                 "three glyphs, two ligate: " + std::to_string(ligatedGlyphs) + " out, width " +
                     num(static_cast<double>(ligatedWidth)),
                 kVal, sz);
            y += 22.0f;
            {
                std::string clusters;
                for (const ui::ShapedGlyph& g : ligated) {
                    clusters += (clusters.empty() ? "" : ", ");
                    clusters += "glyph " + std::to_string(g.glyph) + " from character " +
                                std::to_string(g.cluster);
                }
                cell(950.0f, y, clusters, kDim, 0.25f);
            }
            y += 26.0f;
            font.drawText(*renderer, 950.0f, y,
                          "Both would be 30 units wide unshaped. Kerning pulls a pair three closer "
                          "without changing the count; a ligature replaces two glyphs with one that is "
                          "narrower than their sum. The cluster numbers are what makes a caret still "
                          "work afterwards: the merged glyph still remembers it began at character "
                          "zero, so clicking it can still put the cursor between the two letters.",
                          kDim, 0.25f);

            y += 104.0f;
            font.drawText(*renderer, 950.0f, y, "AND WHAT THE ATLAS KEEPS", kHead, 0.33f);
            y += 28.0f;
            for (const CacheStep& c : cacheSteps) {
                cell(950.0f, y, c.what, kText, sz);
                cell(1130.0f, y, std::to_string(c.cached) + " glyphs", kVal, sz);
                cell(1235.0f, y, std::to_string(c.rasterCalls) + " rasterised", kVal, sz);
                y += 22.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 950.0f, y,
                          "\"hello\" is five letters and four rasterisations, because the second l is "
                          "already there — the cache pays for itself inside a single word. Asking for "
                          "the whole word again rasterises nothing at all. Asking at a different pixel "
                          "size rasterises all four again, because a glyph at 32 is not a glyph at 16 "
                          "scaled up, which is why the size is part of the key rather than an "
                          "afterthought.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 702.0f,
                          "None of this draws anything. It is where the words go, which way they face, "
                          "what owns them and what they cost — all of it settled before a single pixel "
                          "is touched, and all of it therefore testable without a screen.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SETTING shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
