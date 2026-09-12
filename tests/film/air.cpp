// tests/film/air.cpp — verifies the air and the film stock (film Air.hpp) and the object glyphs
// (film Glyphs.hpp).
//
// Two lookup tables and four drawing passes, all of which sit between the audience and the picture:
// what hangs in the air, the light leaking across the frame, the vignette, the grain -- and, for the
// one shot in a film with no people in it, the object the story turns on.
//
// The two tables are pure functions of a few strings, so they are checked exhaustively against the
// browser: all 600 combinations of genre, hour and set for the weather, and 75 words for the glyph
// -- including the ones where the ORDER of the patterns decides the answer, which is the part of a
// regex-to-substring port that can quietly go wrong.
#include "maz/film/Air.hpp"
#include "maz/film/Glyphs.hpp"
#include "maz/io/Json.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using maz::film::Canvas;
using maz::film::Palette;
using maz::film::Weather;
using maz::render::Color;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static double meanOf(const Image& img, int x0, int y0, int x1, int y1) {
    double t = 0.0;
    int n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const auto p = img.getPixel(x, y);
            t += static_cast<double>(p.r + p.g + p.b) / 3.0;
            ++n;
        }
    }
    return n > 0 ? t / n : 0.0;
}

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "tests/film/air-fixture.json";
    std::string text;
    if (!maz::io::readTextFile(path, text)) {
        std::printf("FAIL: could not read the air fixture at %s\n", path.c_str());
        return 1;
    }
    const maz::io::JsonParseResult doc = maz::io::parseJson(text);
    CHECK(doc.ok, "the air fixture is valid JSON");

    // --- 1. What hangs in the air: all 600 combinations, against the browser. ---
    {
        const maz::io::JsonValue& cases = doc.value["weather"];
        CHECK(cases.size() == 600u, "the fixture carries every genre x hour x set");
        int wrong = 0;
        std::string firstBad;
        for (const maz::io::JsonValue& c : cases.items()) {
            const std::string want = c["kind"].asString();
            const std::string got = maz::film::weatherName(maz::film::weatherFor(
                c["genre"].asString(), c["hour"].asString(), c["set"].asString()));
            if (got != want) {
                ++wrong;
                if (firstBad.empty()) {
                    firstBad = c["genre"].asString() + "/" + c["hour"].asString() + "/" +
                               c["set"].asString() + ": got " + got + ", browser says " + want;
                }
            }
        }
        if (wrong != 0) {
            std::printf("FAIL: %d weather choices differ from the browser; first: %s\n", wrong,
                        firstBad.c_str());
            ++g_fail;
        }
        // And the property behind the table: a rainstorm indoors is a mistake, not a mood.
        bool indoorsDry = true;
        const char* genres[] = {"drama", "thriller", "horror", "comedy",  "romance",
                                "scifi", "mystery",  "fantasy", "heist",  "western"};
        const char* hours[] = {"NIGHT", "DAWN", "DAY", "DUSK"};
        const char* inside[] = {"lighthouse", "kitchen", "room",  "corridor", "vehicle",
                                "industrial", "office",  "bar",   "ship",     "ward",
                                "chapel"};
        for (const char* g : genres) {
            for (const char* h : hours) {
                for (const char* s : inside) {
                    const Weather w = maz::film::weatherFor(g, h, s);
                    if (w == Weather::Rain || w == Weather::Fog) {
                        indoorsDry = false;
                    }
                }
            }
        }
        CHECK(indoorsDry, "it never rains or fogs indoors");
    }

    // --- 2. Which object a word picks: 75 words, against the browser. ---
    {
        const maz::io::JsonValue& cases = doc.value["glyphs"];
        CHECK(cases.size() == 75u, "the fixture carries all 75 object words");
        int wrong = 0;
        std::string firstBad;
        for (const maz::io::JsonValue& c : cases.items()) {
            const std::string word = c["word"].asString();
            const std::string want = c["glyph"].asString();
            const std::string got = maz::film::glyphFor(word).name;
            if (got != want) {
                ++wrong;
                if (firstBad.empty()) {
                    firstBad = "\"" + word + "\": got " + got + ", browser says " + want;
                }
            }
        }
        if (wrong != 0) {
            std::printf("FAIL: %d object words pick a different shape than the browser; first: %s\n",
                        wrong, firstBad.c_str());
            ++g_fail;
        }
        // The ordering trap, called out directly: "film reel" contains `tape`'s "film" and "reel",
        // and the browser checks letter's list (which has "file") first -- so the answer depends on
        // the order the lists are tried in, not just on their contents.
        CHECK(std::string(maz::film::glyphFor("film reel").name) == "tape",
              "\"film reel\" is a tape, which only holds if the lists are tried in order");
        CHECK(std::string(maz::film::glyphFor("xyzzy").name) == "box",
              "a word matching nothing is still a thing in a box");
        CHECK(std::string(maz::film::glyphFor("").name) == "box", "no word at all is a box");
        CHECK(std::string(maz::film::glyphFor("THE LETTER").name) == "letter",
              "the match is case-insensitive");
        CHECK(maz::film::glyphs().size() == 10u, "all ten shapes are there");
    }

    // --- 3. Every glyph draws something, and the same thing twice. ---
    {
        const Palette pal = maz::film::paletteFor("drama", "NIGHT", 0.4);
        bool allDrew = true, allSame = true;
        std::string blank;
        for (const auto& g : maz::film::glyphs()) {
            Image a(400, 300, Color{0.5f, 0.5f, 0.5f, 1.0f});
            Image b(400, 300, Color{0.5f, 0.5f, 0.5f, 1.0f});
            const Image before = a;
            for (Image* img : {&a, &b}) {
                Canvas c(*img, 0, 300);
                c.setTransform(0.6f, 0.0f, 0.0f, 0.6f, 200.0f, 150.0f);
                g.draw(c, pal);
            }
            if (a.data() == before.data()) {
                allDrew = false;
                if (blank.empty()) blank = g.name;
            }
            if (!(a.data() == b.data())) {
                allSame = false;
            }
        }
        CHECK(allDrew, ("every object shape draws something (first blank: " + blank + ")").c_str());
        CHECK(allSame, "an object shape draws the same thing twice");
    }

    // --- 4. Weather draws, and `none` does not. ---
    {
        const Palette pal = maz::film::paletteFor("drama", "NIGHT", 0.4);
        const Weather kinds[] = {Weather::Rain, Weather::Dust,    Weather::Fog,
                                 Weather::Haze, Weather::Shimmer, Weather::Embers};
        bool allDrew = true;
        std::string blank;
        for (const Weather k : kinds) {
            Image img(480, 200, Color{0.1f, 0.1f, 0.12f, 1.0f});
            const Image before = img;
            Canvas c(img, 0, 200);
            c.setTransform(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            maz::film::drawWeather(c, k, pal, 3.0, 42u, 480.0f, 200.0f);
            if (img.data() == before.data()) {
                allDrew = false;
                if (blank.empty()) blank = maz::film::weatherName(k);
            }
        }
        CHECK(allDrew, ("every kind of weather draws something (first blank: " + blank + ")").c_str());

        Image none(480, 200, Color{0.1f, 0.1f, 0.12f, 1.0f});
        const Image beforeNone = none;
        Canvas c(none, 0, 200);
        c.setTransform(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        maz::film::drawWeather(c, Weather::None, pal, 3.0, 42u, 480.0f, 200.0f);
        CHECK(none.data() == beforeNone.data(), "clear air draws nothing at all");
    }

    // --- 5. Weather moves with the clock, and repeats for the same clock. ---
    {
        const Palette pal = maz::film::paletteFor("thriller", "NIGHT", 0.5);
        auto render = [&](double t, std::uint32_t seed) {
            Image img(480, 200, Color{0.0f, 0.0f, 0.0f, 1.0f});
            Canvas c(img, 0, 200);
            c.setTransform(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            maz::film::drawWeather(c, Weather::Rain, pal, t, seed, 480.0f, 200.0f);
            return img;
        };
        CHECK(render(2.0, 7u).data() == render(2.0, 7u).data(),
              "the same moment gives the same rain");
        CHECK(!(render(2.0, 7u).data() == render(2.4, 7u).data()), "rain falls as the clock runs");
        CHECK(!(render(2.0, 7u).data() == render(2.0, 8u).data()),
              "a different film's rain falls differently");
    }

    // --- 6. The vignette is dark at the corners, clear in the middle, deeper under tension. ---
    {
        auto vignetted = [](double mood) {
            Image img(400, 200, Color{1.0f, 1.0f, 1.0f, 1.0f});
            maz::film::drawVignette(img, maz::film::paletteFor("drama", "NIGHT", mood), 0, 200);
            return img;
        };
        const Image calm = vignetted(0.1);
        CHECK(meanOf(calm, 180, 90, 220, 110) > 0.9, "the middle of the frame is left alone");
        CHECK(meanOf(calm, 0, 0, 40, 30) < 0.75, "the corners are pulled down");
        CHECK(meanOf(calm, 0, 0, 40, 30) < meanOf(calm, 180, 90, 220, 110),
              "the corners are darker than the middle");
        const Image tense = vignetted(0.95);
        CHECK(meanOf(tense, 0, 0, 40, 30) < meanOf(calm, 0, 0, 40, 30),
              "a scene in crisis gets a deeper vignette");
    }

    // --- 7. The light leak follows the light. ---
    {
        const Palette pal = maz::film::paletteFor("drama", "NIGHT", 0.4);
        auto leaked = [&](double offset) {
            Image img(400, 200, Color{0.0f, 0.0f, 0.0f, 1.0f});
            Canvas c(img, 0, 200);
            c.setTransform(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            maz::film::drawLightLeak(c, pal, offset, 1.0, 400.0f, 200.0f);
            return img;
        };
        const Image left = leaked(-1.0);
        const Image right = leaked(1.0);
        // The leak starts at its own end of the frame, so its bright end moves with the light.
        CHECK(meanOf(left, 0, 90, 60, 110) != meanOf(right, 0, 90, 60, 110),
              "moving the light moves the leak");
        const Image bright = [&] {
            Image img(400, 200, Color{0.0f, 0.0f, 0.0f, 1.0f});
            Canvas c(img, 0, 200);
            c.setTransform(1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
            maz::film::drawLightLeak(c, pal, 0.0, 1.5, 400.0f, 200.0f);
            return img;
        }();
        const Image dim = leaked(0.0);
        CHECK(meanOf(bright, 0, 0, 400, 200) > meanOf(dim, 0, 0, 400, 200),
              "a brighter light leaks more");
    }

    // --- 8. Grain: seeded, stepped twelve times a second, and gentle. ---
    {
        CHECK(maz::film::grainTile().size() == 128u * 128u, "the grain tile is 128 pixels square");
        bool inRange = true;
        for (const std::uint8_t v : maz::film::grainTile()) {
            if (v < 110 || v > 200) {
                inRange = false;
            }
        }
        CHECK(inRange, "the grain stays in its stated range");

        auto grained = [](double t) {
            Image img(200, 100, Color{0.5f, 0.5f, 0.5f, 1.0f});
            maz::film::drawGrain(img, t, 0, 100);
            return img;
        };
        // Twelve steps a second: two moments inside one step are identical, and two in different
        // steps are not. This is what stops every frame of a recording being different.
        CHECK(grained(1.0).data() == grained(1.0 + 1.0 / 48.0).data(),
              "two moments inside one grain step look the same");
        CHECK(!(grained(1.0).data() == grained(1.0 + 1.0 / 6.0).data()),
              "the grain moves on to the next step");
        // Gentle: it is a texture, not a curtain.
        const Image g = grained(1.0);
        CHECK(std::fabs(meanOf(g, 0, 0, 200, 100) - 0.5) < 0.08,
              "the grain barely moves the overall brightness");
    }

    if (g_fail == 0) {
        std::printf("film air: all checks passed (600 weather cases, 75 object words)\n");
    }
    return g_fail == 0 ? 0 : 1;
}
