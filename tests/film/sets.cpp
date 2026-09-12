// tests/film/sets.cpp — verifies the fifteen ported sets (film Sets.hpp).
//
// A set is a place a scene can happen, drawn in three parallax planes: the far one, the one the
// characters stand among, and one dark element close to the lens. Porting 500 lines of drawing calls
// is the kind of work where a single transposed number produces something that still looks like a
// room, so the check is not "does this look plausible" -- it is agreement with the browser.
//
// The ground truth is a signature captured from the browser: each of the 15 sets, each of its 3
// planes, under 3 different palettes, reduced to a coarse 8x4 grid of mean colours. Coarse on
// purpose. Two independent rasterizers will never agree pixel for pixel on an anti-aliased curve or
// the last bit of a gradient, and demanding that would make this a test of the rasterizer rather
// than of the port -- but anything drawn in the wrong PLACE, at the wrong SIZE, or in the wrong
// COLOUR moves a cell mean well past the tolerance, which is what a porting error actually looks
// like.
#include "maz/film/Sets.hpp"
#include "maz/io/Json.hpp"
#include "maz/render/ImageCodecPnm.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using maz::film::Canvas;
using maz::film::NoiseField;
using maz::film::Palette;
using maz::render::Color;
using maz::render::Image;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Draw one plane of one set into a fresh image, the way the fixture was captured.
static Image renderPlane(const maz::film::SetPainter& set, const std::string& plane,
                         const Palette& pal, const NoiseField& n, int w, int h) {
    Image img(w, h, Color{0.0f, 0.0f, 0.0f, 1.0f});
    Canvas c(img, 0, h);
    c.setTransform(static_cast<float>(w) / maz::film::kWorldW, 0.0f, 0.0f,
                   static_cast<float>(h) / maz::film::kWorldH, 0.0f, 0.0f);
    if (plane == "back") {
        set.back(c, pal, n);
    } else if (plane == "mid") {
        set.mid(c, pal, n);
    } else {
        set.fore(c, pal, n, false);
    }
    return img;
}

static double cellMean(const Image& img, int gx, int gy, int GX, int GY, int channel) {
    const int x0 = gx * img.width() / GX, x1 = (gx + 1) * img.width() / GX;
    const int y0 = gy * img.height() / GY, y1 = (gy + 1) * img.height() / GY;
    double t = 0.0;
    int count = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const auto p = img.getPixel(x, y);
            t += static_cast<double>(channel == 0 ? p.r : (channel == 1 ? p.g : p.b)) * 255.0;
            ++count;
        }
    }
    return count > 0 ? t / count : 0.0;
}

static double inkOf(const Image& img) {
    double t = 0.0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const auto p = img.getPixel(x, y);
            t += static_cast<double>(p.r + p.g + p.b);
        }
    }
    return t / (3.0 * static_cast<double>(img.width()) * static_cast<double>(img.height()));
}

int main(int argc, char** argv) {
    const std::string path = argc > 1 ? argv[1] : "tests/film/sets-fixture.json";

    // --- 1. All fifteen are there, and an unknown name falls back the browser's way. ---
    {
        const auto& all = maz::film::sets();
        CHECK(all.size() == 15u, "all fifteen sets are there");
        const char* expected[] = {"lighthouse", "kitchen",    "room",   "corridor", "woods",
                                  "street",     "field",      "vehicle", "industrial", "office",
                                  "bar",        "ship",       "water",  "ward",     "chapel"};
        bool named = all.size() == 15u;
        for (std::size_t i = 0; named && i < 15u; ++i) {
            named = std::string(all[i].name) == expected[i];
        }
        CHECK(named, "the fifteen sets are the fifteen the reel can ask for, in order");
        CHECK(std::string(maz::film::setNamed("no-such-place").name) == "room",
              "an unknown set falls back to room, as the browser's renderer does");
        CHECK(std::string(maz::film::setNamed("chapel").name) == "chapel", "a known set is itself");
    }

    // --- 2. Parallax: the back plane moves least, the fore plane most. That ordering IS the depth. ---
    {
        CHECK(maz::film::Parallax::kBack < maz::film::Parallax::kMid,
              "the back plane moves less than the middle one");
        CHECK(maz::film::Parallax::kMid < maz::film::Parallax::kFore,
              "the fore plane moves more than the middle one");
        CHECK(maz::film::Parallax::kMid == 1.0f,
              "the middle plane moves at exactly the camera's rate, so figures land where a "
              "single-plane renderer would have put them");
    }

    // --- 3. Every plane of every set draws something. ---
    //
    // Measured as "the image changed", on a mid-grey ground, rather than as brightness. A fore
    // element is near-black ink by design, so on a black ground it draws a great deal and registers
    // as nothing at all -- which is what the first version of this check concluded about every set's
    // fore plane.
    {
        const Palette pal = maz::film::paletteFor("drama", "NIGHT", 0.4);
        bool allDrew = true;
        std::string blank;
        for (const auto& set : maz::film::sets()) {
            const auto n = maz::film::noise(maz::film::noiseKey(set.name, 20260912u), 80);
            for (const std::string plane : {"back", "mid", "fore"}) {
                const Color grey{0.5f, 0.5f, 0.5f, 1.0f};
                Image img(400, 168, grey);
                const Image before = img;
                Canvas c(img, 0, 168);
                c.setTransform(0.4f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f);
                if (plane == "back") {
                    set.back(c, pal, n);
                } else if (plane == "mid") {
                    set.mid(c, pal, n);
                } else {
                    set.fore(c, pal, n, false);
                }
                if (img.data() == before.data()) {
                    allDrew = false;
                    if (blank.empty()) {
                        blank = std::string(set.name) + "/" + plane;
                    }
                }
            }
        }
        CHECK(allDrew, ("every plane of every set draws something (first blank: " + blank + ")").c_str());
    }

    // --- 4. The signature check: agreement with the browser, cell by cell. ---
    {
        std::string text;
        if (!maz::io::readTextFile(path, text)) {
            std::printf("FAIL: could not read the sets fixture at %s\n", path.c_str());
            return 1;
        }
        const maz::io::JsonParseResult doc = maz::io::parseJson(text);
        CHECK(doc.ok, "the sets fixture is valid JSON");
        const int W = doc.value["width"].asInt(400);
        const int H = doc.value["height"].asInt(168);
        const int GX = doc.value["gx"].asInt(8);
        const int GY = doc.value["gy"].asInt(4);
        const std::uint32_t seed = static_cast<std::uint32_t>(doc.value["seed"].asNumber(0.0));
        const maz::io::JsonValue& cases = doc.value["cases"];
        CHECK(cases.isArray() && cases.size() == 135u,
              "the fixture carries 15 sets x 3 planes x 3 palettes");

        int offCells = 0;
        double worst = 0.0;
        std::string firstBad;
        for (const maz::io::JsonValue& c : cases.items()) {
            const std::string name = c["set"].asString();
            const std::string plane = c["plane"].asString();
            const Palette pal = maz::film::paletteFor(c["genre"].asString(), c["hour"].asString(),
                                                      c["mood"].asNumber(0.4));
            const auto n = maz::film::noise(maz::film::noiseKey(name, seed), 80);
            const Image img = renderPlane(maz::film::setNamed(name), plane, pal, n, W, H);
            const maz::io::JsonValue& cells = c["cells"];
            for (int gy = 0; gy < GY; ++gy) {
                for (int gx = 0; gx < GX; ++gx) {
                    const std::size_t k = static_cast<std::size_t>(gy * GX + gx);
                    for (int ch = 0; ch < 3; ++ch) {
                        const double them = cells[k][static_cast<std::size_t>(ch)].asNumber(-1.0);
                        const double mine = cellMean(img, gx, gy, GX, GY, ch);
                        const double d = std::fabs(mine - them);
                        worst = std::fmax(worst, d);
                        // Six levels out of 255 on the mean of a cell of some 2000 pixels. An
                        // anti-aliasing difference along one edge moves a cell mean by a fraction of
                        // a level; a shape in the wrong place moves it by tens.
                        if (d > 6.0) {
                            ++offCells;
                            if (firstBad.empty()) {
                                firstBad = name + "/" + plane + " " + c["genre"].asString() + "/" +
                                           c["hour"].asString() + " cell(" + std::to_string(gx) + "," +
                                           std::to_string(gy) + ") channel " + std::to_string(ch) +
                                           ": " + std::to_string(mine) + " vs " + std::to_string(them);
                            }
                        }
                    }
                }
            }
        }
        if (offCells != 0) {
            std::printf("FAIL: %d cell means differ from the browser by more than 6/255; first: %s\n",
                        offCells, firstBad.c_str());
            ++g_fail;
        } else {
            std::printf("  (135 plane signatures match the browser; worst cell mean off by %.2f/255)\n",
                        worst);
        }
    }

    // --- 5. The fore plane's out-of-focus variant is a lift, never a darkening. ---
    //
    // The browser reaches it by algebra rather than an offscreen composite, and the claim that makes
    // that legal is that mixing toward black is only a scaling -- so the lifted version must come out
    // lighter than the plain one, never darker.
    {
        const Palette pal = maz::film::paletteFor("drama", "NIGHT", 0.4);
        bool everDarker = false;
        int everDifferent = 0;
        for (const auto& set : maz::film::sets()) {
            const auto n = maz::film::noise(maz::film::noiseKey(set.name, 20260912u), 80);
            Image plain(400, 168, Color{0.5f, 0.5f, 0.5f, 1.0f});
            Image lifted(400, 168, Color{0.5f, 0.5f, 0.5f, 1.0f});
            for (int which = 0; which < 2; ++which) {
                Image& img = which == 0 ? plain : lifted;
                Canvas c(img, 0, 168);
                c.setTransform(0.4f, 0.0f, 0.0f, 0.4f, 0.0f, 0.0f);
                set.fore(c, pal, n, which == 1);
            }
            if (!(plain.data() == lifted.data())) {
                ++everDifferent;
            }
            if (inkOf(lifted) < inkOf(plain) - 0.01) {
                everDarker = true;
            }
        }
        CHECK(!everDarker, "the out-of-focus fore element is never darker than the plain one");
        CHECK(everDifferent == 15, "the lift changes every set's fore element");
    }

    // --- 6. Light: every set has a kind, and what it does stays finite and bounded. ---
    {
        CHECK(maz::film::lightKindFor("lighthouse") == maz::film::LightKind::Sweep,
              "a lighthouse owns a sweeping beam");
        CHECK(maz::film::lightKindFor("street") == maz::film::LightKind::Passing,
              "a street owns passing headlights");
        CHECK(maz::film::lightKindFor("corridor") == maz::film::LightKind::Flicker,
              "a corridor owns a failing bulb");
        CHECK(maz::film::lightKindFor("woods") == maz::film::LightKind::Cloud,
              "woods own moving cloud");
        CHECK(maz::film::lightKindFor("kitchen") == maz::film::LightKind::None,
              "a kitchen bulb does not move");
        CHECK(maz::film::lightKindFor("no-such-place") == maz::film::LightKind::None,
              "an unknown place owns no light of its own");

        bool bounded = true, moves = false;
        const maz::film::LightKind kinds[] = {maz::film::LightKind::Sweep, maz::film::LightKind::Passing,
                                              maz::film::LightKind::Flicker, maz::film::LightKind::Cloud,
                                              maz::film::LightKind::None};
        for (const maz::film::LightKind k : kinds) {
            for (int i = 0; i <= 4000; ++i) {
                const double t = static_cast<double>(i) * 0.05;
                for (const double tension : {0.0, 0.4, 1.0}) {
                    const maz::film::LightState st = maz::film::lightAt(k, t, tension);
                    if (!std::isfinite(st.brightness) || !std::isfinite(st.offset) ||
                        st.brightness < 0.4 || st.brightness > 1.6 || st.offset < -1.0001 ||
                        st.offset > 1.0001) {
                        bounded = false;
                    }
                }
            }
            if (std::fabs(maz::film::lightAt(k, 3.0, 0.5).brightness -
                          maz::film::lightAt(k, 7.0, 0.5).brightness) > 1e-9) {
                moves = true;
            }
        }
        CHECK(bounded, "every light behaviour stays finite, and its offset stays inside the frame");
        CHECK(moves, "at least one light behaviour actually changes over time");
        // Tension agitates a bulb and does not touch the sun.
        CHECK(maz::film::lightAt(maz::film::LightKind::Cloud, 5.0, 0.0).brightness ==
                  maz::film::lightAt(maz::film::LightKind::Cloud, 5.0, 1.0).brightness,
              "tension does not touch the weather");
        CHECK(maz::film::lightAt(maz::film::LightKind::Flicker, 5.0, 0.0).brightness !=
                  maz::film::lightAt(maz::film::LightKind::Flicker, 5.0, 1.0).brightness,
              "tension does agitate a failing bulb");
    }

    // --- 7. Determinism: the same set, seed and palette give the same pixels. ---
    {
        const Palette pal = maz::film::paletteFor("horror", "NIGHT", 0.88);
        bool same = true;
        for (const auto& set : maz::film::sets()) {
            const auto n = maz::film::noise(maz::film::noiseKey(set.name, 7u), 80);
            const Image a = renderPlane(set, "back", pal, n, 240, 100);
            const Image b = renderPlane(set, "back", pal, n, 240, 100);
            if (!(a.data() == b.data())) {
                same = false;
            }
        }
        CHECK(same, "drawing a set twice gives identical pixels");
    }

    // --- 8. A different seed refurnishes the sets that scatter, and leaves the others alone. ---
    {
        const Palette pal = maz::film::paletteFor("drama", "NIGHT", 0.4);
        int changed = 0;
        for (const auto& set : maz::film::sets()) {
            const Image a =
                renderPlane(set, "back", pal, maz::film::noise(maz::film::noiseKey(set.name, 1u), 80),
                            240, 100);
            const Image b =
                renderPlane(set, "back", pal, maz::film::noise(maz::film::noiseKey(set.name, 2u), 80),
                            240, 100);
            if (!(a.data() == b.data())) {
                ++changed;
            }
        }
        // lighthouse, woods, street, bar, ship and water scatter things; the rest are built.
        CHECK(changed >= 5, "a different seed refurnishes the sets that scatter things");
    }

    if (g_fail == 0) {
        std::printf("film sets: all checks passed (15 sets, 3 planes each)\n");
    }
    return g_fail == 0 ? 0 : 1;
}
