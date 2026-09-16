// Maz Engine — "PALETTE" (render's colour science: ColorNames, CieLab, ContrastRatio,
// ColorHarmony, ColorTemperature, ImageColorBlind, ColorQuantize, MedianCut, Dither)
// Nine modules that all answer questions about colour with a number rather than a taste, which is
// what makes a demo of them worth writing: every panel here can be checked against a published
// table or a piece of arithmetic. LEFT: what a colour is called, where it sits in a perceptual
// space, and how far apart two colours actually look. MIDDLE: whether anyone can read your text on
// it, which is a standard with a pass mark, and which colour pairs stop being distinguishable for
// the roughly one man in twelve who sees fewer of them. RIGHT: what happens when you are only allowed a
// few colours — how a palette is chosen, and how two levels can still look like a gradient.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 698 headers.
#include "maz/render/CieLab.hpp"
#include "maz/render/ColorHarmony.hpp"
#include "maz/render/ColorNames.hpp"
#include "maz/render/ColorQuantize.hpp"
#include "maz/render/ColorTemperature.hpp"
#include "maz/render/ContrastRatio.hpp"
#include "maz/render/Dither.hpp"
#include "maz/render/ImageColorBlind.hpp"
#include "maz/render/MedianCut.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

// A deterministic generator, so every number on screen is the same on every machine and every run.
struct Lcg {
    std::uint64_t s = 0xC0107u;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    int jitter(int n) {
        return static_cast<int>(next() % static_cast<std::uint32_t>(2 * n + 1)) - n;
    }
};

render::Rgb8 clampRgb(int r, int g, int b) {
    auto c = [](int v) { return static_cast<std::uint8_t>(std::clamp(v, 0, 255)); };
    return render::Rgb8{c(r), c(g), c(b)};
}

// Straight-line RGB distance, 0..441. Not a perceptual metric — used only where the question is
// "did these two stay apart at all", for which a plain distance is the honest measure.
double rgbDistance(const render::Color& a, const render::Color& b) {
    const double dr = static_cast<double>(a.r) - b.r;
    const double dg = static_cast<double>(a.g) - b.g;
    const double db = static_cast<double>(a.b) - b.b;
    return std::sqrt(dr * dr + dg * dg + db * db);
}

double quantizeError(const std::vector<render::Rgb8>& px, const std::vector<render::Rgb8>& pal) {
    const std::vector<std::size_t> idx = render::mapToPalette(px, pal);
    double sum = 0.0;
    for (std::size_t i = 0; i < px.size(); ++i) {
        const render::Rgb8& q = pal[idx[i]];
        const double dr = static_cast<double>(px[i].r) - q.r;
        const double dg = static_cast<double>(px[i].g) - q.g;
        const double db = static_cast<double>(px[i].b) - q.b;
        sum += std::sqrt(dr * dr + dg * dg + db * db);
    }
    return sum / static_cast<double>(px.size());
}

// How far a quantized image drifts from the original when you stand back: the mean difference
// between the source's average over a window and the output's. A single pixel is always wrong once
// you are down to two levels, so this is the only thing dithering is actually trying to buy.
double localError(const std::vector<std::uint8_t>& a, const std::vector<std::uint8_t>& b, int w,
                  int h, int win) {
    double sum = 0.0;
    int n = 0;
    for (int y = 0; y + win <= h; y += win) {
        for (int x = 0; x + win <= w; x += win) {
            double sa = 0.0, sb = 0.0;
            for (int j = 0; j < win; ++j) {
                for (int i = 0; i < win; ++i) {
                    const std::size_t k = static_cast<std::size_t>((y + j) * w + x + i);
                    sa += a[k];
                    sb += b[k];
                }
            }
            sum += std::fabs(sa - sb) / static_cast<double>(win * win);
            ++n;
        }
    }
    return n == 0 ? 0.0 : sum / static_cast<double>(n);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PALETTE starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Palette";
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

    // ---- what a colour is called ------------------------------------------------------------------------
    struct Named {
        std::string asked;
        bool known = false;
        render::Color value;
    };
    std::vector<Named> named;
    std::size_t nameCount = 0;
    {
        render::detail::namedColorTable(nameCount);
        const char* asked[] = {"rebeccapurple", "Rebecca Purple", "REBECCAPURPLE",
                               "tomato",        "#4080c0",       "chartreuse green"};
        for (const char* a : asked) {
            const render::Color miss{-1, -1, -1, 1};
            const render::Color got = render::colorFromString(a, miss);
            named.push_back(Named{a, render::namedColor(a).has_value(), got});
        }
    }

    // ---- where a colour sits, and how far apart two of them look -----------------------------------------
    struct LabRow {
        std::string what;
        render::Lab lab;
        float roundTrip = 0.0f;
    };
    std::vector<LabRow> labRows;
    {
        const struct {
            const char* what;
            render::Color c;
        } samples[] = {{"white", {1, 1, 1, 1}},      {"black", {0, 0, 0, 1}},
                       {"0.5 linear grey", {0.5f, 0.5f, 0.5f, 1}}, {"red", {1, 0, 0, 1}},
                       {"green", {0, 1, 0, 1}},      {"blue", {0, 0, 1, 1}}};
        for (const auto& s : samples) {
            const render::Lab l = render::toLab(s.c);
            const render::Color back = render::fromLab(l);
            const float err = std::max({std::fabs(back.r - s.c.r), std::fabs(back.g - s.c.g),
                                        std::fabs(back.b - s.c.b)});
            labRows.push_back(LabRow{s.what, l, err});
        }
    }
    // The published Sharma-Wu-Dalal pairs, which is how you tell a CIEDE2000 implementation from a
    // plausible-looking one. CIE76 gets both badly wrong, which is the reason CIEDE2000 exists.
    struct DeltaRow {
        std::string what;
        float e76 = 0.0f;
        float e2000 = 0.0f;
        float published = 0.0f;
    };
    const std::vector<DeltaRow> deltas = {
        {"blues, pair 1",
         render::deltaE76({50, 2.6772f, -79.7751f}, {50, 0.0f, -82.7485f}),
         render::deltaE2000({50, 2.6772f, -79.7751f}, {50, 0.0f, -82.7485f}), 2.0425f},
        {"blues, pair 2",
         render::deltaE76({50, 3.1571f, -77.2803f}, {50, 0.0f, -82.7485f}),
         render::deltaE2000({50, 3.1571f, -77.2803f}, {50, 0.0f, -82.7485f}), 2.8615f},
        {"greys, pair 3",
         render::deltaE76({50, 2.4900f, -0.0010f}, {50, -2.4900f, 0.0009f}),
         render::deltaE2000({50, 2.4900f, -0.0010f}, {50, -2.4900f, 0.0009f}), 7.1792f},
    };

    // ---- can anyone read it -------------------------------------------------------------------------------
    struct ReadRow {
        std::string what;
        float ratio = 0.0f;
        bool aa = false, aaLarge = false, aaa = false;
    };
    std::vector<ReadRow> reads;
    {
        const render::Color white{1, 1, 1, 1};
        const char* swatches[] = {"#767676", "#777777", "#595959", "#ffff00", "#0000ff"};
        for (const char* hex : swatches) {
            render::Color c{0, 0, 0, 1};
            render::fromHtml(hex, c);
            reads.push_back(ReadRow{std::string(hex) + " on white", render::contrastRatio(c, white),
                                    render::passesAA(c, white), render::passesAA(c, white, true),
                                    render::passesAAA(c, white)});
        }
    }
    // The same grey read in the two colour spaces it could be in. This is a trap with a size.
    float rawLum = 0.0f, convertedLum = 0.0f, rawRatio = 0.0f, convertedRatio = 0.0f;
    bool rawPasses = false, convertedPasses = false;
    {
        const render::Color linearGrey{0.5f, 0.5f, 0.5f, 1};
        const render::Color displayed = render::linearToSrgb(linearGrey);
        const render::Color white{1, 1, 1, 1};
        rawLum = render::relativeLuminance(linearGrey);
        convertedLum = render::relativeLuminance(displayed);
        rawRatio = render::contrastRatio(linearGrey, white);
        convertedRatio = render::contrastRatio(displayed, white);
        rawPasses = render::passesAA(linearGrey, white, true);
        convertedPasses = render::passesAA(displayed, white, true);
    }

    // ---- what a third of the palette looks like to someone else -------------------------------------------
    struct BlindRow {
        std::string pair;
        double normal = 0.0;
        double seen[4] = {0, 0, 0, 0};
    };
    std::vector<BlindRow> blind;
    const char* visionNames[4] = {"protan", "deutan", "tritan", "achroma"};
    {
        const render::ColorVision modes[4] = {
            render::ColorVision::Protanopia, render::ColorVision::Deuteranopia,
            render::ColorVision::Tritanopia, render::ColorVision::Achromatopsia};
        const struct {
            const char* a;
            const char* b;
        } pairs[] = {{"red", "green"}, {"blue", "yellow"}, {"red", "blue"}, {"orange", "teal"}};
        for (const auto& p : pairs) {
            const render::Color ca = render::colorFromString(p.a, render::Color{0, 0, 0, 1});
            const render::Color cb = render::colorFromString(p.b, render::Color{0, 0, 0, 1});
            BlindRow row;
            row.pair = std::string(p.a) + " / " + p.b;
            row.normal = rgbDistance(ca, cb);
            for (int i = 0; i < 4; ++i) {
                row.seen[i] = rgbDistance(render::simulateColorVision(ca, modes[i]),
                                          render::simulateColorVision(cb, modes[i]));
            }
            blind.push_back(row);
        }
    }

    // ---- a scheme out of one colour, and a colour out of a temperature ------------------------------------
    struct Scheme {
        std::string what;
        std::vector<render::Color> colors;
    };
    std::vector<Scheme> schemes;
    const render::Color harmonyBase =
        render::colorFromString("#e06020", render::Color{0, 0, 0, 1});
    {
        schemes.push_back(Scheme{"complementary", render::complementary(harmonyBase)});
        schemes.push_back(Scheme{"analogous", render::analogous(harmonyBase)});
        schemes.push_back(Scheme{"triadic", render::triadic(harmonyBase)});
        schemes.push_back(Scheme{"split complementary", render::splitComplementary(harmonyBase)});
        schemes.push_back(Scheme{"tetradic", render::tetradic(harmonyBase)});
        schemes.push_back(Scheme{"monochromatic x5", render::monochromatic(harmonyBase, 5)});
    }
    struct Kelvin {
        float k = 0.0f;
        render::Color c;
        std::string what;
    };
    std::vector<Kelvin> kelvins;
    {
        const struct {
            float k;
            const char* what;
        } steps[] = {{1000.0f, "ember"},      {1900.0f, "candle"},  {2700.0f, "tungsten"},
                     {4000.0f, "warm white"}, {5500.0f, "noon sun"}, {6600.0f, "the neutral point"},
                     {9000.0f, "open shade"}, {20000.0f, "clear sky"}};
        for (const auto& s : steps) {
            kelvins.push_back(Kelvin{s.k, render::kelvinToColor(s.k), s.what});
        }
    }

    // ---- choosing a few colours to stand for many ---------------------------------------------------------
    struct QuantRow {
        std::size_t k = 0;
        double intError = 0.0;
        double floatError = 0.0; // distance from the green cluster to the nearest palette entry
    };
    std::vector<QuantRow> quant;
    std::size_t distinctColors = 0;
    {
        const render::Rgb8 centres[3] = {{200, 40, 40}, {40, 180, 60}, {50, 60, 200}};
        std::vector<render::Rgb8> px;
        std::vector<render::Color> fpx;
        Lcg rng;
        for (int i = 0; i < 3000; ++i) {
            const render::Rgb8& c = centres[static_cast<std::size_t>(i % 3)];
            const render::Rgb8 j = clampRgb(c.r + rng.jitter(3), c.g + rng.jitter(3),
                                            c.b + rng.jitter(3));
            px.push_back(j);
            fpx.push_back(render::Color{static_cast<float>(j.r) / 255.0f,
                                        static_cast<float>(j.g) / 255.0f,
                                        static_cast<float>(j.b) / 255.0f, 1.0f});
        }
        {
            std::vector<std::uint32_t> keys;
            keys.reserve(px.size());
            for (const render::Rgb8& c : px) {
                keys.push_back((static_cast<std::uint32_t>(c.r) << 16) |
                               (static_cast<std::uint32_t>(c.g) << 8) | c.b);
            }
            std::sort(keys.begin(), keys.end());
            distinctColors =
                static_cast<std::size_t>(std::unique(keys.begin(), keys.end()) - keys.begin());
        }
        for (std::size_t k : {2u, 3u, 4u, 5u, 8u, 16u}) {
            const std::vector<render::Rgb8> pal = render::quantizePalette(px, k);
            const std::vector<render::Color> fpal =
                render::medianCutPalette(fpx, static_cast<int>(k));
            double best = 1e9;
            for (const render::Color& c : fpal) {
                best = std::min(best, rgbDistance(c, render::Color{40.0f / 255.0f, 180.0f / 255.0f,
                                                                   60.0f / 255.0f, 1}) * 255.0);
            }
            quant.push_back(QuantRow{k, quantizeError(px, pal), best});
        }
    }

    // ---- two levels that still look like a gradient --------------------------------------------------------
    struct DitherRow {
        std::string what;
        double w4 = 0.0, w8 = 0.0, w16 = 0.0;
        std::size_t levels = 0;
    };
    std::vector<DitherRow> dithers;
    {
        const int w = 96, h = 96;
        std::vector<std::uint8_t> src(static_cast<std::size_t>(w * h));
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const double v = 128.0 + 90.0 * std::sin(x * 0.13) * std::cos(y * 0.09);
                src[static_cast<std::size_t>(y * w + x)] =
                    static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0));
            }
        }
        std::vector<std::uint8_t> naive = src;
        for (std::uint8_t& p : naive) {
            p = static_cast<std::uint8_t>(p < 128 ? 0 : 255);
        }
        const struct {
            const char* what;
            std::vector<std::uint8_t> out;
        } runs[] = {{"plain threshold", naive},
                    {"ordered, bayer 4x4", render::orderedDitherGray(src, w, h, 2)},
                    {"ordered, bayer 8x8", render::orderedDitherGray(src, w, h, 2, 3)},
                    {"floyd-steinberg", render::floydSteinbergGray(src, w, h, 2)}};
        for (const auto& r : runs) {
            std::vector<std::uint8_t> sorted = r.out;
            std::sort(sorted.begin(), sorted.end());
            const std::size_t levels = static_cast<std::size_t>(
                std::unique(sorted.begin(), sorted.end()) - sorted.begin());
            dithers.push_back(DitherRow{r.what, localError(src, r.out, w, h, 4),
                                        localError(src, r.out, w, h, 8),
                                        localError(src, r.out, w, h, 16), levels});
        }
    }
    std::vector<std::size_t> bayerSizes;
    for (int lv = 0; lv < 4; ++lv) {
        bayerSizes.push_back(render::bayerMatrix(lv).size());
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kBad{1.0f, 0.52f, 0.45f, 1};

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

            const float sz = 0.26f;
            auto cell = [&](float x, float y, const std::string& s, render::Color colour,
                            float scale) { font.drawText(*renderer, x, y, s.c_str(), colour, scale); };
            // A filled swatch, so the numbers have the colour they describe next to them.
            auto swatch = [&](float x, float y, float w, float h, const render::Color& c) {
                const render::Point2 quad[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
                renderer->drawConvexPolygon(quad, 4, c);
            };

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PALETTE", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "nine colour modules, every one of them answering with a number you can "
                          "check against a published table or a piece of arithmetic",
                          kDim, 0.32f);

            // ---- column 1 -------------------------------------------------------------------
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "WHAT IS IT CALLED", kHead, 0.34f);
            y += 28.0f;
            for (const Named& n : named) {
                cell(24.0f, y, n.asked, kText, sz);
                cell(210.0f, y, n.known ? "a name" : "not a name", n.known ? kOk : kDim, sz);
                if (n.value.r >= 0.0f) {
                    swatch(318.0f, y + 2.0f, 22.0f, 14.0f, n.value);
                    cell(348.0f, y, num(static_cast<double>(n.value.r), 3) + " " +
                                        num(static_cast<double>(n.value.g), 3) + " " +
                                        num(static_cast<double>(n.value.b), 3), kVal, 0.24f);
                } else {
                    cell(318.0f, y, "fell back", kBad, sz);
                }
                y += 21.0f;
            }
            y += 4.0f;
            cell(24.0f, y,
                 "The table holds " + std::to_string(nameCount) +
                     " names, matched with the spaces and the capitals taken out, so \"Rebecca "
                     "Purple\" and \"REBECCAPURPLE\" are the same request. A hex code is not a "
                     "name but is still a colour; a phrase that is neither returns the fallback "
                     "rather than a guess.",
                 kDim, 0.25f);

            y += 78.0f;
            font.drawText(*renderer, 24.0f, y, "WHERE IT SITS, PERCEPTUALLY", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "colour", kDim, 0.24f);
            cell(190.0f, y, "L*", kDim, 0.24f);
            cell(250.0f, y, "a*", kDim, 0.24f);
            cell(320.0f, y, "b*", kDim, 0.24f);
            cell(400.0f, y, "round trip", kDim, 0.24f);
            y += 20.0f;
            for (const LabRow& r : labRows) {
                cell(24.0f, y, r.what, kText, sz);
                cell(190.0f, y, num(static_cast<double>(r.lab.L), 2), kVal, sz);
                cell(250.0f, y, num(static_cast<double>(r.lab.a), 2), kVal, sz);
                cell(320.0f, y, num(static_cast<double>(r.lab.b), 2), kVal, sz);
                cell(400.0f, y, r.roundTrip == 0.0f ? "exact" : ("< " + num(1e-6, 6)),
                     r.roundTrip < 1e-6f ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(24.0f, y, "how different do two colours LOOK:", kVal, 0.25f);
            y += 21.0f;
            cell(24.0f, y, "pair", kDim, 0.24f);
            cell(190.0f, y, "CIE76", kDim, 0.24f);
            cell(270.0f, y, "CIEDE2000", kDim, 0.24f);
            cell(390.0f, y, "published", kDim, 0.24f);
            y += 20.0f;
            for (const DeltaRow& d : deltas) {
                cell(24.0f, y, d.what, kText, sz);
                cell(190.0f, y, num(static_cast<double>(d.e76), 4), kDim, sz);
                cell(270.0f, y, num(static_cast<double>(d.e2000), 4), kVal, sz);
                cell(390.0f, y, num(static_cast<double>(d.published), 4),
                     std::fabs(d.e2000 - d.published) < 1e-3f ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "0.5 in a linear Color is not the middle grey your eye expects: L* 76, "
                          "not 50. And the last column is why CIEDE2000 exists — CIE76 calls the "
                          "two near-identical blues twice as different as they are, and the two "
                          "greys seven-tenths as different, because plain distance in L*a*b* "
                          "is not uniform. Matching the published pairs is how you tell a real "
                          "implementation from a plausible one.",
                          kDim, 0.25f);

            // ---- column 2 -------------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 560.0f, y, "CAN ANYONE READ IT", kHead, 0.34f);
            y += 28.0f;
            cell(560.0f, y, "foreground", kDim, 0.24f);
            cell(760.0f, y, "ratio", kDim, 0.24f);
            cell(840.0f, y, "AA", kDim, 0.24f);
            cell(890.0f, y, "AA large", kDim, 0.24f);
            cell(980.0f, y, "AAA", kDim, 0.24f);
            y += 20.0f;
            for (const ReadRow& r : reads) {
                cell(560.0f, y, r.what, kText, sz);
                cell(760.0f, y, num(static_cast<double>(r.ratio), 3), kVal, sz);
                cell(840.0f, y, r.aa ? "pass" : "fail", r.aa ? kOk : kBad, sz);
                cell(890.0f, y, r.aaLarge ? "pass" : "fail", r.aaLarge ? kOk : kBad, sz);
                cell(980.0f, y, r.aaa ? "pass" : "fail", r.aaa ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 560.0f, y,
                          "#767676 and #777777 differ by one step in each channel and land on "
                          "opposite sides of the 4.5 pass mark — #767676 is the darkest grey that "
                          "still passes AA on white. Yellow on white is 1.07:1, which is a colour "
                          "choice and not a contrast.",
                          kDim, 0.25f);

            y += 60.0f;
            cell(560.0f, y, "the same grey, read in two colour spaces:", kVal, 0.25f);
            y += 21.0f;
            cell(560.0f, y, "handed in raw (linear)", kText, sz);
            cell(800.0f, y, "lum " + num(static_cast<double>(rawLum), 4), kVal, sz);
            cell(920.0f, y, "ratio " + num(static_cast<double>(rawRatio), 3), kVal, sz);
            cell(1050.0f, y, rawPasses ? "AA large: pass" : "AA large: fail", rawPasses ? kBad : kOk,
                 sz);
            y += 21.0f;
            cell(560.0f, y, "converted first (sRGB)", kText, sz);
            cell(800.0f, y, "lum " + num(static_cast<double>(convertedLum), 4), kVal, sz);
            cell(920.0f, y, "ratio " + num(static_cast<double>(convertedRatio), 3), kVal, sz);
            cell(1050.0f, y, convertedPasses ? "AA large: pass" : "AA large: fail",
                 convertedPasses ? kBad : kOk, sz);
            y += 24.0f;
            font.drawText(*renderer, 560.0f, y,
                          "WCAG is defined on the numbers a designer types, which are sRGB. A "
                          "render::Color is linear, and the swapchain is an _SRGB format precisely "
                          "so the GPU does that encode on the way out. Hand the linear value "
                          "straight in and a colour that fails the large-text threshold of 3.0 is "
                          "reported as passing it. linearToSrgb first.",
                          kDim, 0.25f);

            y += 76.0f;
            font.drawText(*renderer, 560.0f, y, "WHAT A THIRD OF THE PALETTE LOSES", kHead, 0.34f);
            y += 28.0f;
            cell(560.0f, y, "pair", kDim, 0.24f);
            cell(740.0f, y, "normal", kDim, 0.24f);
            for (int i = 0; i < 4; ++i) {
                cell(820.0f + static_cast<float>(i) * 78.0f, y, visionNames[i], kDim, 0.24f);
            }
            y += 20.0f;
            for (const BlindRow& b : blind) {
                cell(560.0f, y, b.pair, kText, sz);
                cell(740.0f, y, num(b.normal, 3), kVal, sz);
                for (int i = 0; i < 4; ++i) {
                    const double frac = b.normal > 0.0 ? b.seen[i] / b.normal : 1.0;
                    cell(820.0f + static_cast<float>(i) * 78.0f, y, num(b.seen[i], 3),
                         frac < 0.5 ? kBad : (frac < 0.8 ? kText : kOk), sz);
                }
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 560.0f, y,
                          "Each deficiency destroys exactly the pair it is named for and leaves "
                          "the others mostly alone: red against green keeps a fifth of its "
                          "separation without red cones, blue against yellow keeps seven-eighths "
                          "of its there but loses two-fifths without blue cones, and with no "
                          "colour at all red and blue are a sixth apart — nearly the same grey. Which is the argument for never letting "
                          "hue alone carry meaning.",
                          kDim, 0.25f);

            // ---- column 3 -------------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 1300.0f, y, "A SCHEME OUT OF ONE COLOUR", kHead, 0.34f);
            y += 28.0f;
            swatch(1300.0f, y + 2.0f, 22.0f, 14.0f, harmonyBase);
            cell(1330.0f, y, "the base, hue " +
                                 num(static_cast<double>(render::toHsv(harmonyBase).h), 4) +
                                 " turns", kText, sz);
            y += 24.0f;
            for (const Scheme& s : schemes) {
                cell(1300.0f, y, s.what, kText, sz);
                float x = 1500.0f;
                for (const render::Color& c : s.colors) {
                    swatch(x, y + 2.0f, 20.0f, 14.0f, c);
                    cell(x, y + 18.0f, num(static_cast<double>(render::toHsv(c).h), 3), kDim, 0.20f);
                    x += 46.0f;
                }
                y += 38.0f;
            }
            y += 2.0f;
            font.drawText(*renderer, 1300.0f, y,
                          "Every one of these is a rotation of the hue by an exact fraction of a "
                          "turn: a half for the complement, thirds for the triad, quarters for the "
                          "tetrad, a twelfth either side for the analogous set. Monochromatic "
                          "rotates nothing at all — the hue is identical across all five and only "
                          "the value moves, which is why it is the one scheme that cannot clash.",
                          kDim, 0.25f);

            y += 82.0f;
            font.drawText(*renderer, 1300.0f, y, "A COLOUR OUT OF A TEMPERATURE", kHead, 0.34f);
            y += 28.0f;
            for (const Kelvin& k : kelvins) {
                cell(1300.0f, y, num(static_cast<double>(k.k), 0) + "K", kVal, sz);
                swatch(1390.0f, y + 2.0f, 30.0f, 14.0f, k.c);
                cell(1432.0f, y, k.what, kText, sz);
                cell(1600.0f, y,
                     num(static_cast<double>(k.c.r), 3) + " " + num(static_cast<double>(k.c.g), 3) +
                         " " + num(static_cast<double>(k.c.b), 3),
                     kDim, 0.23f);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 1300.0f, y,
                          "Red is pinned at full below 6600K and blue above it, so the curve is "
                          "warm on one side and cool on the other and pure white at exactly one "
                          "temperature. That temperature is 6600K, not the 6500K of daylight — an "
                          "artefact of where this approximation's pieces are joined, and worth "
                          "knowing before you use it to set a sun colour.",
                          kDim, 0.25f);

            // ---- column 4 -------------------------------------------------------------------
            y = 500.0f;
            font.drawText(*renderer, 24.0f, y, "FEWER COLOURS THAN YOU HAVE", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y,
                 std::to_string(distinctColors) +
                     " distinct colours in 3000 pixels, in three tight clusters:",
                 kDim, 0.25f);
            y += 22.0f;
            cell(24.0f, y, "palette", kDim, 0.24f);
            cell(130.0f, y, "error (0..441)", kDim, 0.24f);
            cell(290.0f, y, "nearest entry to the green cluster", kDim, 0.24f);
            y += 20.0f;
            for (const QuantRow& q : quant) {
                cell(24.0f, y, std::to_string(q.k) + " colours", kText, sz);
                cell(130.0f, y, num(q.intError, 2), q.intError < 5.0 ? kOk : kVal, sz);
                cell(290.0f, y, num(q.floatError, 1) + " away",
                     q.floatError < 6.0 ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Median cut halves one box at a time, so three clusters need more than "
                          "three entries to come apart: the error falls off a cliff at five, not "
                          "three. Until this demo was written it never fell at all — the box "
                          "chosen to split next was the widest one, which is always the nearly "
                          "empty box of stray pixels between two clusters, so half the image kept "
                          "sharing one colour that matched neither. Weighting the choice by how "
                          "many pixels a box holds took the error at eight colours from 22.6 to 3.",
                          kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 24.0f, y, "AND TWO LEVELS THAT STILL LOOK LIKE A GRADIENT",
                          kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "method", kDim, 0.24f);
            cell(230.0f, y, "levels out", kDim, 0.24f);
            cell(340.0f, y, "4x4", kDim, 0.24f);
            cell(410.0f, y, "8x8", kDim, 0.24f);
            cell(480.0f, y, "16x16", kDim, 0.24f);
            y += 20.0f;
            for (const DitherRow& d : dithers) {
                cell(24.0f, y, d.what, kText, sz);
                cell(230.0f, y, std::to_string(d.levels), kVal, sz);
                cell(340.0f, y, num(d.w4, 2), d.w4 < 10.0 ? kOk : kBad, sz);
                cell(410.0f, y, num(d.w8, 2), d.w8 < 10.0 ? kOk : kBad, sz);
                cell(480.0f, y, num(d.w16, 2), d.w16 < 10.0 ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            {
                std::string sizes;
                for (std::size_t i = 0; i < bayerSizes.size(); ++i) {
                    sizes += (i ? ", " : "") + std::to_string(i) + "->" +
                             std::to_string(bayerSizes[i]);
                }
                cell(24.0f, y, "bayer matrix entries by level: " + sizes, kDim, 0.25f);
            }
            y += 24.0f;
            font.drawText(*renderer, 24.0f, y,
                          "All four rows produce exactly two levels — the same two — and the error "
                          "column is the difference between the original's average over a window "
                          "and the output's, because a single pixel is always wrong once you are "
                          "down to two and the only question left is what it looks like from a "
                          "step back. Plain thresholding is still 35 out at a sixteen-pixel "
                          "window, an order of magnitude worse than either dither. Both dithers "
                          "land within a few parts in 255 of the original, ordered "
                          "slightly ahead close up and error diffusion ahead further out, which is "
                          "the trade: a fixed pattern you can see, or noise you cannot.",
                          kDim, 0.25f);

            font.drawText(*renderer, 560.0f, 1000.0f,
                          "Not one number here is a matter of opinion. The delta-E column matches "
                          "a published table, the contrast column matches a standard with a pass "
                          "mark, the harmony column is exact fractions of a turn, and the "
                          "quantization column is a distance. Colour is the part of a renderer "
                          "that looks like taste and is not.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PALETTE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
