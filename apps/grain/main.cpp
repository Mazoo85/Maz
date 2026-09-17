// Maz Engine — "GRAIN" (core's noise and sampling: ValueNoise, SimplexNoise, WorleyNoise,
// CellularNoise, CurlNoise, PoissonDisk, Halton, SpaceFilling, RandomDistributions, ShuffleBag,
// WeightedReservoir)
// Eleven ways of being random on purpose. Randomness is the hardest thing in an engine to look at
// and judge — every one of these produces something that LOOKS fine, so the only honest test is a
// property that must hold and a number that says whether it did. A curl field must have zero
// divergence. A Poisson disk must never put two points closer than its radius. A shuffle bag must
// deal every outcome exactly as often as it was weighted. LEFT: noise, and what "smooth" and
// "repeatable" actually mean. MIDDLE: points placed better than chance would place them. RIGHT:
// distributions and draws, measured against their own theory.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 699 headers.
#include "maz/core/CellularNoise.hpp"
#include "maz/core/CurlNoise.hpp"
#include "maz/core/Halton.hpp"
#include "maz/core/Pcg32.hpp"
#include "maz/core/PoissonDisk.hpp"
#include "maz/core/RandomDistributions.hpp"
#include "maz/core/ShuffleBag.hpp"
#include "maz/core/SimplexNoise.hpp"
#include "maz/core/SpaceFilling.hpp"
#include "maz/core/ValueNoise.hpp"
#include "maz/core/WeightedReservoir.hpp"
#include "maz/core/WorleyNoise.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 4) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string sci(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1e", v);
    return buf;
}

std::string drift(double v) { return v == 0.0 ? std::string("exact") : sci(v); }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GRAIN starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Grain";
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

    // ---- noise: bounded, repeatable, and smooth ---------------------------------------------------
    double vnLo = 1e30, vnHi = -1e30, vnStep = 0.0, vnLattice = 0.0, vnSameSeed = 0.0,
           vnOtherSeed = 0.0;
    struct MeanRow {
        int cells = 0;
        double mean = 0.0;
    };
    std::vector<MeanRow> vnMeans;
    double sxLo = 1e30, sxHi = -1e30, sxMean = 0.0;
    std::vector<std::pair<int, double>> fbmPeaks;
    {
        const core::ValueNoise vn(7u);
        for (int i = 0; i < 400; ++i) {
            for (int j = 0; j < 400; ++j) {
                const float x = static_cast<float>(i) * 0.031f;
                const float y = static_cast<float>(j) * 0.029f;
                const float v = vn.value2(x, y);
                vnLo = std::min(vnLo, static_cast<double>(v));
                vnHi = std::max(vnHi, static_cast<double>(v));
                vnStep = std::max(vnStep,
                                  std::fabs(static_cast<double>(vn.value2(x + 0.001f, y)) - v));
            }
        }
        for (int ix = -20; ix <= 20; ++ix) {
            for (int iy = -20; iy <= 20; ++iy) {
                vnLattice = std::max(
                    vnLattice, std::fabs(static_cast<double>(
                                             vn.value2(static_cast<float>(ix), static_cast<float>(iy))) -
                                         vn.lattice(ix, iy)));
            }
        }
        const core::ValueNoise same(7u), other(8u);
        for (int i = 0; i < 10000; ++i) {
            const float x = static_cast<float>(i) * 0.013f, y = static_cast<float>(i) * 0.007f;
            vnSameSeed = std::max(
                vnSameSeed, std::fabs(static_cast<double>(vn.value2(x, y)) - same.value2(x, y)));
            vnOtherSeed = std::max(
                vnOtherSeed, std::fabs(static_cast<double>(vn.value2(x, y)) - other.value2(x, y)));
        }
        // The mean only looks biased until you sample enough of the lattice.
        for (int cells : {12, 50, 200, 800}) {
            double s = 0.0;
            const int steps = 600;
            for (int i = 0; i < steps; ++i) {
                for (int j = 0; j < steps; ++j) {
                    s += vn.value2(static_cast<float>(i) * static_cast<float>(cells) / steps,
                                   static_cast<float>(j) * static_cast<float>(cells) / steps);
                }
            }
            vnMeans.push_back(MeanRow{cells, s / (steps * steps)});
        }

        double sum = 0.0;
        for (int i = 0; i < 400; ++i) {
            for (int j = 0; j < 400; ++j) {
                const float v = core::simplex2D(static_cast<float>(i) * 0.031f,
                                                static_cast<float>(j) * 0.029f, 7u);
                sxLo = std::min(sxLo, static_cast<double>(v));
                sxHi = std::max(sxHi, static_cast<double>(v));
                sum += v;
            }
        }
        sxMean = sum / (400.0 * 400.0);
        for (int oct : {1, 2, 4, 8}) {
            double peak = 0.0;
            for (int i = 0; i < 200; ++i) {
                for (int j = 0; j < 200; ++j) {
                    peak = std::max(peak,
                                    std::fabs(static_cast<double>(core::simplexFbm2D(
                                        static_cast<float>(i) * 0.03f,
                                        static_cast<float>(j) * 0.03f, oct))));
                }
            }
            fbmPeaks.emplace_back(oct, peak);
        }
    }

    // ---- cells, and the distance to the nearest one -------------------------------------------------
    int worleyOutOfOrder = 0;
    double worleyMaxF1 = 0.0, cellF1 = 0.0, cellF2 = 0.0;
    {
        const core::WorleyNoise wn(7u);
        for (int i = 0; i < 300; ++i) {
            for (int j = 0; j < 300; ++j) {
                const std::pair<float, float> p =
                    wn.f1f2(static_cast<float>(i) * 0.037f, static_cast<float>(j) * 0.041f);
                if (p.second < p.first) {
                    ++worleyOutOfOrder;
                }
                worleyMaxF1 = std::max(worleyMaxF1, static_cast<double>(p.first));
            }
        }
        const core::CellularSample cs = core::worley2D(1.5f, 2.5f, 7u);
        cellF1 = cs.f1;
        cellF2 = cs.f2;
    }

    // ---- curl noise: the property is exact, the measurement is not ------------------------------------
    struct CurlRow {
        double h = 0.0;
        double divergence = 0.0;
        double cosine = 0.0;
    };
    std::vector<CurlRow> curls;
    {
        const core::CurlNoise cn(7u, 1.0e-3f);
        for (float h : {1.0e-1f, 1.0e-2f, 1.0e-3f, 1.0e-4f}) {
            double worstDiv = 0.0, worstCos = 0.0;
            for (int i = 0; i < 120; ++i) {
                for (int j = 0; j < 120; ++j) {
                    const float x = static_cast<float>(i) * 0.05f;
                    const float y = static_cast<float>(j) * 0.05f;
                    const std::pair<float, float> cx = cn.curl2(x + h, y), cxm = cn.curl2(x - h, y);
                    const std::pair<float, float> cy = cn.curl2(x, y + h), cym = cn.curl2(x, y - h);
                    const double div =
                        static_cast<double>(cx.first - cxm.first) / (2.0 * h) +
                        static_cast<double>(cy.second - cym.second) / (2.0 * h);
                    worstDiv = std::max(worstDiv, std::fabs(div));
                    const std::pair<float, float> g = cn.gradient2(x, y);
                    const std::pair<float, float> c = cn.curl2(x, y);
                    const double mag = std::hypot(static_cast<double>(g.first), g.second) *
                                       std::hypot(static_cast<double>(c.first), c.second);
                    if (mag > 1e-9) {
                        worstCos = std::max(worstCos,
                                            std::fabs(static_cast<double>(g.first) * c.first +
                                                      static_cast<double>(g.second) * c.second) /
                                                mag);
                    }
                }
            }
            curls.push_back(CurlRow{static_cast<double>(h), worstDiv, worstCos});
        }
    }

    // ---- points placed better than chance would place them ---------------------------------------------
    struct DiskRow {
        double radius = 0.0;
        std::size_t points = 0;
        double closest = 0.0;
        int violations = 0;
        double hexIdeal = 0.0;
    };
    std::vector<DiskRow> disks;
    bool diskRepeatable = false;
    {
        for (float r : {0.05f, 0.1f, 0.2f}) {
            const std::vector<math::vec2> pts =
                core::poissonDiskSample(math::vec2(0, 0), math::vec2(1, 1), r, 42u);
            double closest = 1e30;
            int violations = 0;
            for (std::size_t i = 0; i < pts.size(); ++i) {
                for (std::size_t j = i + 1; j < pts.size(); ++j) {
                    const double d = std::hypot(static_cast<double>(pts[i].x) - pts[j].x,
                                                static_cast<double>(pts[i].y) - pts[j].y);
                    closest = std::min(closest, d);
                    if (d < static_cast<double>(r) - 1e-6) {
                        ++violations;
                    }
                }
            }
            // How many a perfect hexagonal packing of the same radius would fit in a unit square.
            const double ideal =
                1.0 / (static_cast<double>(r) * static_cast<double>(r) * std::sqrt(3.0) / 2.0);
            disks.push_back(DiskRow{r, pts.size(), closest, violations, ideal});
        }
        const std::vector<math::vec2> a =
            core::poissonDiskSample(math::vec2(0, 0), math::vec2(1, 1), 0.1f, 42u);
        const std::vector<math::vec2> b =
            core::poissonDiskSample(math::vec2(0, 0), math::vec2(1, 1), 0.1f, 42u);
        diskRepeatable =
            a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                               [](const math::vec2& p, const math::vec2& q) {
                                                   return p.x == q.x && p.y == q.y;
                                               });
    }

    struct HaltonRow {
        int count = 0;
        double halton = 0.0;
        double random = 0.0;
    };
    std::vector<HaltonRow> haltons;
    std::vector<double> haltonFirst;
    {
        // The largest gap between the share of points inside a box anchored at the origin and that
        // box's area. Low for a well-spread set, high for one that clumps.
        auto discrepancy = [](const std::vector<std::pair<float, float>>& p) {
            double worst = 0.0;
            for (int i = 1; i <= 20; ++i) {
                for (int j = 1; j <= 20; ++j) {
                    const double bx = i / 20.0, by = j / 20.0;
                    std::size_t inside = 0;
                    for (const auto& q : p) {
                        if (q.first < bx && q.second < by) {
                            ++inside;
                        }
                    }
                    worst = std::max(worst, std::fabs(static_cast<double>(inside) /
                                                          static_cast<double>(p.size()) -
                                                      bx * by));
                }
            }
            return worst;
        };
        for (int count : {64, 256, 1024}) {
            std::vector<std::pair<float, float>> h, r;
            core::Pcg32 rng(9u, 1u);
            for (int i = 1; i <= count; ++i) {
                h.push_back(core::halton2D(static_cast<std::uint32_t>(i)));
            }
            for (int i = 0; i < count; ++i) {
                r.emplace_back(rng.nextFloat(), rng.nextFloat());
            }
            haltons.push_back(HaltonRow{count, discrepancy(h), discrepancy(r)});
        }
        core::HaltonSequence seq(2u);
        for (int i = 0; i < 8; ++i) {
            haltonFirst.push_back(seq.next());
        }
    }

    // ---- two ways to walk a grid ------------------------------------------------------------------------
    int mortonRoundTripBad = 0, hilbertDupes = 0, hilbertJumps = 0, hilbertRoundTripBad = 0,
        mortonJumps = 0;
    {
        constexpr std::uint32_t kSide = 64;
        for (std::uint32_t x = 0; x < kSide; ++x) {
            for (std::uint32_t y = 0; y < kSide; ++y) {
                const std::pair<std::uint32_t, std::uint32_t> d =
                    core::mortonDecode2(core::mortonEncode2(x, y));
                if (d.first != x || d.second != y) {
                    ++mortonRoundTripBad;
                }
            }
        }
        std::vector<int> seen(static_cast<std::size_t>(kSide) * kSide, 0);
        std::uint32_t px = 0, py = 0;
        for (std::uint64_t d = 0; d < std::uint64_t(kSide) * kSide; ++d) {
            std::uint32_t x = 0, y = 0;
            core::hilbertD2XY(kSide, d, x, y);
            if (seen[static_cast<std::size_t>(y) * kSide + x]++ != 0) {
                ++hilbertDupes;
            }
            if (core::hilbertXY2D(kSide, x, y) != d) {
                ++hilbertRoundTripBad;
            }
            if (d > 0 && (std::abs(static_cast<int>(x) - static_cast<int>(px)) +
                          std::abs(static_cast<int>(y) - static_cast<int>(py))) != 1) {
                ++hilbertJumps;
            }
            px = x;
            py = y;
        }
        std::uint32_t mx = 0, my = 0;
        for (std::uint64_t d = 0; d < std::uint64_t(kSide) * kSide; ++d) {
            const std::pair<std::uint32_t, std::uint32_t> p = core::mortonDecode2(d);
            if (d > 0 && (std::abs(static_cast<int>(p.first) - static_cast<int>(mx)) +
                          std::abs(static_cast<int>(p.second) - static_cast<int>(my))) != 1) {
                ++mortonJumps;
            }
            mx = p.first;
            my = p.second;
        }
    }

    // ---- distributions against their own theory ------------------------------------------------------------
    struct DistRow {
        std::string what;
        double mean = 0.0, meanExact = 0.0, variance = 0.0, varianceExact = 0.0;
    };
    std::vector<DistRow> dists;
    int geometricMin = 0;
    {
        core::Pcg32 rng(3u, 1u);
        auto u = [&rng] { return static_cast<double>(rng.nextFloat()); };
        const int n = 400000;
        {
            double s = 0.0, s2 = 0.0;
            for (int i = 0; i < n; ++i) {
                const double v = core::exponential(u, 2.5);
                s += v;
                s2 += v * v;
            }
            const double m = s / n;
            dists.push_back(DistRow{"exponential, rate 2.5", m, 1.0 / 2.5, s2 / n - m * m,
                                    1.0 / (2.5 * 2.5)});
        }
        {
            double s = 0.0, s2 = 0.0;
            for (int i = 0; i < n; ++i) {
                const double v = core::poisson(u, 4.0);
                s += v;
                s2 += v * v;
            }
            const double m = s / n;
            dists.push_back(DistRow{"poisson, lambda 4", m, 4.0, s2 / n - m * m, 4.0});
        }
        {
            double s = 0.0, s2 = 0.0;
            geometricMin = 1 << 30;
            for (int i = 0; i < n; ++i) {
                const int v = core::geometric(u, 0.2);
                s += v;
                s2 += static_cast<double>(v) * v;
                geometricMin = std::min(geometricMin, v);
            }
            const double m = s / n;
            dists.push_back(DistRow{"geometric, p 0.2", m, 5.0, s2 / n - m * m, 0.8 / (0.2 * 0.2)});
        }
    }

    // ---- draws that are fair over a cycle, and draws from a stream -------------------------------------------
    int bagInsideRepeats = 0, bagBoundaryRepeats = 0, bagCycles = 5000;
    std::size_t bagPerCycle = 0;
    std::map<int, int> bagTally;
    bool bagCyclesExact = true;
    {
        core::ShuffleBag<int> bag;
        for (int i = 0; i < 4; ++i) {
            bag.add(i, i == 0 ? 3 : 1); // three copies of item 0, one each of the rest
        }
        core::Pcg32 rng(5u, 1u);
        bagPerCycle = bag.totalCount();
        int last = -1;
        for (int c = 0; c < bagCycles; ++c) {
            std::map<int, int> inCycle;
            for (std::size_t i = 0; i < bagPerCycle; ++i) {
                const int v = bag.next(rng);
                if (v == last) {
                    if (i == 0) {
                        ++bagBoundaryRepeats;
                    } else {
                        ++bagInsideRepeats;
                    }
                }
                last = v;
                ++bagTally[v];
                ++inCycle[v];
            }
            if (inCycle[0] != 3 || inCycle[1] != 1 || inCycle[2] != 1 || inCycle[3] != 1) {
                bagCyclesExact = false;
            }
        }
    }

    struct ReservoirRow {
        double weight = 0.0, exact = 0.0, measured = 0.0;
    };
    std::vector<ReservoirRow> reservoir;
    double reservoirWorst = 0.0;
    const int kReservoirTrials = 4000, kReservoirItems = 1000, kReservoirKeep = 10;
    {
        std::vector<double> w(kReservoirItems);
        for (int i = 0; i < kReservoirItems; ++i) {
            w[static_cast<std::size_t>(i)] = 1.0 + static_cast<double>(i % 10);
        }
        const double total = std::accumulate(w.begin(), w.end(), 0.0);
        std::vector<int> chosen(kReservoirItems, 0);
        for (int t = 0; t < kReservoirTrials; ++t) {
            core::WeightedReservoir<int> r{static_cast<std::size_t>(kReservoirKeep),
                                           static_cast<std::uint64_t>(t)};
            for (int i = 0; i < kReservoirItems; ++i) {
                r.add(i, w[static_cast<std::size_t>(i)]);
            }
            for (const int id : r.sample()) {
                ++chosen[static_cast<std::size_t>(id)];
            }
        }
        for (int cls = 0; cls < 10; ++cls) {
            double got = 0.0, want = 0.0;
            for (int i = cls; i < kReservoirItems; i += 10) {
                got += chosen[static_cast<std::size_t>(i)];
                want += w[static_cast<std::size_t>(i)];
            }
            const double gotShare = got / (static_cast<double>(kReservoirTrials) * kReservoirKeep);
            const double wantShare = want / total;
            reservoirWorst = std::max(reservoirWorst, std::fabs(gotShare - wantShare));
            reservoir.push_back(ReservoirRow{1.0 + cls, wantShare, gotShare});
        }
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
                            float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  GRAIN", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "eleven ways of being random on purpose — all of which look fine, so all "
                          "of which are judged here by a property that had to hold",
                          kDim, 0.32f);

            // ---- column 1 ----------------------------------------------------------------
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "NOISE: BOUNDED, SMOOTH, REPEATABLE", kHead, 0.34f);
            y += 28.0f;
            cell(24.0f, y, "ValueNoise over 160000 samples", kVal, 0.26f);
            y += 22.0f;
            cell(44.0f, y, "range", kText, sz);
            cell(300.0f, y, num(vnLo) + " to " + num(vnHi),
                 (vnLo >= -1.0 && vnHi <= 1.0) ? kOk : kBad, sz);
            y += 21.0f;
            cell(44.0f, y, "at integer coordinates it equals lattice()", kText, sz);
            cell(400.0f, y, drift(vnLattice), vnLattice == 0.0 ? kOk : kBad, sz);
            y += 21.0f;
            cell(44.0f, y, "a step of 0.001 changes it by at most", kText, sz);
            cell(400.0f, y, num(vnStep, 5), kVal, sz);
            y += 21.0f;
            cell(44.0f, y, "same seed twice", kText, sz);
            cell(300.0f, y, drift(vnSameSeed), vnSameSeed == 0.0 ? kOk : kBad, sz);
            cell(400.0f, y, "a different seed: " + num(vnOtherSeed) + " apart", kVal, sz);
            y += 26.0f;
            cell(24.0f, y, "and its mean, as the sampled area grows:", kVal, 0.26f);
            y += 22.0f;
            for (const MeanRow& m : vnMeans) {
                cell(44.0f, y, "about " + std::to_string(m.cells) + " by " +
                                   std::to_string(m.cells) + " lattice cells",
                     kText, sz);
                cell(400.0f, y, num(m.mean, 5), std::fabs(m.mean) < 0.02 ? kOk : kDim, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "The first of those four is why this panel exists. Twelve cells give a "
                          "mean of +0.063 and it looks like a bias in the generator; it is a small "
                          "sample of a lattice, and it walks to -0.0006 by the time eight hundred "
                          "cells are averaged. A noise function is not wrong because one patch of "
                          "it leans — that is what noise is.",
                          kDim, 0.25f);

            y += 84.0f;
            cell(24.0f, y, "SimplexNoise over 160000 samples", kVal, 0.26f);
            y += 22.0f;
            cell(44.0f, y, "range " + num(sxLo) + " to " + num(sxHi) + ", mean " + num(sxMean, 5),
                 kVal, sz);
            y += 21.0f;
            for (const auto& f : fbmPeaks) {
                cell(44.0f, y,
                     "fbm with " + std::to_string(f.first) +
                         (f.first == 1 ? " octave:  peak " : " octaves: peak ") + num(f.second),
                     kText, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Each octave is half the amplitude of the one before, and the function "
                          "divides by the total amplitude it used, so the result stays inside "
                          "minus one to one however many octaves you ask for — the caller never "
                          "normalises. What the falling peak shows is something the normalisation "
                          "does not guarantee: the octaves do not line up their extremes, so the "
                          "sum never reaches the bound it is allowed, and it reaches it less the "
                          "more octaves there are.",
                          kDim, 0.25f);

            y += 76.0f;
            cell(24.0f, y, "WorleyNoise over 90000 samples", kVal, 0.26f);
            y += 22.0f;
            cell(44.0f, y, "the second-nearest was closer than the nearest", kText, sz);
            cell(430.0f, y, std::to_string(worleyOutOfOrder) + " times",
                 worleyOutOfOrder == 0 ? kOk : kBad, sz);
            y += 21.0f;
            cell(44.0f, y, "largest distance to any nearest feature point", kText, sz);
            cell(430.0f, y, num(worleyMaxF1), kVal, sz);
            y += 21.0f;
            cell(44.0f, y,
                 "CellularNoise at (1.5, 2.5): F1 " + num(cellF1) + ", F2 " + num(cellF2), kText, sz);

            // ---- column 2 ----------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 900.0f, y, "A FLOW WITH NOWHERE TO LEAK", kHead, 0.34f);
            y += 28.0f;
            cell(900.0f, y, "step h", kDim, 0.24f);
            cell(1010.0f, y, "worst measured divergence", kDim, 0.24f);
            cell(1310.0f, y, "worst cosine with the gradient", kDim, 0.24f);
            y += 20.0f;
            for (const CurlRow& c : curls) {
                cell(900.0f, y, sci(c.h), kText, sz);
                cell(1010.0f, y, sci(c.divergence), c.divergence < 1e-3 ? kOk : kBad, sz);
                cell(1310.0f, y, drift(c.cosine), c.cosine == 0.0 ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 900.0f, y,
                          "A curl field is the perpendicular of a gradient, so it cannot have a "
                          "source or a sink — smoke carried by it never piles up or vanishes. The "
                          "right-hand column is that property and it is exactly zero at every step "
                          "size, because it is construction rather than approximation. The "
                          "left-hand column is only a MEASUREMENT of it, and it tells a second "
                          "story: the error falls from 3e-01 to 9e-05 as the step shrinks and then "
                          "jumps to 1.2 at 1e-04, where subtracting two nearly equal floats loses "
                          "every digit that mattered. Truncation shrinking and round-off growing, "
                          "crossing in plain sight — and neither of them a fault in the field.",
                          kDim, 0.25f);

            y += 120.0f;
            font.drawText(*renderer, 900.0f, y, "POINTS PLACED BETTER THAN CHANCE", kHead, 0.34f);
            y += 28.0f;
            cell(900.0f, y, "radius", kDim, 0.24f);
            cell(990.0f, y, "points", kDim, 0.24f);
            cell(1080.0f, y, "closest pair", kDim, 0.24f);
            cell(1210.0f, y, "closer than the radius", kDim, 0.24f);
            cell(1420.0f, y, "against perfect packing", kDim, 0.24f);
            y += 20.0f;
            for (const DiskRow& d : disks) {
                cell(900.0f, y, num(d.radius, 2), kText, sz);
                cell(990.0f, y, std::to_string(d.points), kVal, sz);
                cell(1080.0f, y, num(d.closest), d.closest >= d.radius ? kOk : kBad, sz);
                cell(1210.0f, y, std::to_string(d.violations), d.violations == 0 ? kOk : kBad, sz);
                cell(1420.0f, y,
                     num(100.0 * static_cast<double>(d.points) / d.hexIdeal, 0) + "% of " +
                         num(d.hexIdeal, 0),
                     kVal, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(900.0f, y,
                 std::string("the same seed gives the same set of points: ") +
                     (diskRepeatable ? "yes" : "no"),
                 diskRepeatable ? kOk : kBad, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 900.0f, y,
                          "The minimum distance is a guarantee, not a tendency: not one pair out of "
                          "thousands is closer than the radius, at any of the three. What it does "
                          "not promise is density — around three fifths of what a perfect "
                          "hexagonal packing would fit, which is the price of the points being "
                          "irregular rather than a lattice. That irregularity is the whole reason "
                          "to use it for trees or grass.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 900.0f, y, "AND POINTS THAT FILL IN THE GAPS", kHead, 0.34f);
            y += 28.0f;
            cell(900.0f, y, "points", kDim, 0.24f);
            cell(1010.0f, y, "halton", kDim, 0.24f);
            cell(1130.0f, y, "plain random", kDim, 0.24f);
            cell(1290.0f, y, "tighter by", kDim, 0.24f);
            y += 20.0f;
            for (const HaltonRow& h : haltons) {
                cell(900.0f, y, std::to_string(h.count), kText, sz);
                cell(1010.0f, y, num(h.halton, 5), kVal, sz);
                cell(1130.0f, y, num(h.random, 5), kDim, sz);
                cell(1290.0f, y, num(h.random / h.halton, 1) + "x",
                     h.random > h.halton ? kOk : kBad, sz);
                y += 21.0f;
            }
            y += 6.0f;
            {
                std::string seq;
                for (std::size_t i = 0; i < haltonFirst.size(); ++i) {
                    seq += (i ? "  " : "") + num(haltonFirst[i], 4);
                }
                cell(900.0f, y, "base 2, in order: " + seq, kText, 0.24f);
            }
            y += 24.0f;
            font.drawText(*renderer, 900.0f, y,
                          "Read that sequence and you can see the rule: a half, then the middle of "
                          "each half, then the middle of each quarter. Every new point lands in the "
                          "largest gap left, so the set is well spread at every count and not only "
                          "at the end — which is what lets a renderer stop sampling whenever it "
                          "likes. The advantage over plain random GROWS with the count, from three "
                          "times to ten, because random clumps worse the more of it you draw.",
                          kDim, 0.25f);

            // ---- column 3 ----------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 2100.0f, y, "TWO WAYS TO WALK A GRID", kHead, 0.34f);
            y += 28.0f;
            cell(2100.0f, y, "over a 64 by 64 grid, 4096 cells:", kVal, 0.26f);
            y += 22.0f;
            cell(2120.0f, y, "morton: encode then decode, wrong", kText, sz);
            cell(2480.0f, y, std::to_string(mortonRoundTripBad),
                 mortonRoundTripBad == 0 ? kOk : kBad, sz);
            y += 21.0f;
            cell(2120.0f, y, "hilbert: cells visited twice", kText, sz);
            cell(2480.0f, y, std::to_string(hilbertDupes), hilbertDupes == 0 ? kOk : kBad, sz);
            y += 21.0f;
            cell(2120.0f, y, "hilbert: round trips wrong", kText, sz);
            cell(2480.0f, y, std::to_string(hilbertRoundTripBad),
                 hilbertRoundTripBad == 0 ? kOk : kBad, sz);
            y += 21.0f;
            cell(2120.0f, y, "hilbert: steps that were not to a neighbour", kText, sz);
            cell(2480.0f, y, std::to_string(hilbertJumps), hilbertJumps == 0 ? kOk : kBad, sz);
            y += 21.0f;
            cell(2120.0f, y, "morton: steps that were not to a neighbour", kText, sz);
            cell(2480.0f, y, std::to_string(mortonJumps), kVal, sz);
            y += 24.0f;
            font.drawText(*renderer, 2100.0f, y,
                          "Both visit all 4096 cells once and both undo perfectly. The difference "
                          "is the last two lines. A Hilbert curve never leaves the cell it is "
                          "standing on except to step to a neighbour — not once in 4095 moves — so "
                          "consecutive indices are always physically close, which is why it is the "
                          "better key for a texture or a spatial cache. Morton jumps 2047 times, "
                          "and is kept anyway because its index is a few shifts rather than a loop.",
                          kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 2100.0f, y, "DISTRIBUTIONS AGAINST THEIR OWN THEORY", kHead,
                          0.34f);
            y += 28.0f;
            cell(2100.0f, y, "400000 draws each", kDim, 0.24f);
            cell(2320.0f, y, "mean", kDim, 0.24f);
            cell(2420.0f, y, "exact", kDim, 0.24f);
            cell(2510.0f, y, "variance", kDim, 0.24f);
            cell(2620.0f, y, "exact", kDim, 0.24f);
            y += 20.0f;
            for (const DistRow& d : dists) {
                const bool meanOk = std::fabs(d.mean - d.meanExact) < 0.02 * d.meanExact + 0.01;
                const bool varOk =
                    std::fabs(d.variance - d.varianceExact) < 0.02 * d.varianceExact + 0.01;
                cell(2100.0f, y, d.what, kText, sz);
                cell(2320.0f, y, num(d.mean, 4), meanOk ? kOk : kBad, sz);
                cell(2420.0f, y, num(d.meanExact, 2), kDim, sz);
                cell(2510.0f, y, num(d.variance, 4), varOk ? kOk : kBad, sz);
                cell(2620.0f, y, num(d.varianceExact, 2), kDim, sz);
                y += 21.0f;
            }
            y += 6.0f;
            cell(2100.0f, y,
                 "the smallest geometric draw was " + std::to_string(geometricMin) +
                     " — it counts trials, so it can never be zero",
                 geometricMin == 1 ? kOk : kBad, 0.25f);
            y += 24.0f;
            font.drawText(*renderer, 2100.0f, y,
                          "Checking a mean is not enough: a generator that returned the mean every "
                          "time would pass it. The variance column is what separates a "
                          "distribution from a constant, and Poisson is the giveaway — its mean and "
                          "its variance are the same number in theory, and both come back as four.",
                          kDim, 0.25f);

            y += 76.0f;
            font.drawText(*renderer, 2100.0f, y, "DRAWS THAT HAVE TO BE FAIR", kHead, 0.34f);
            y += 28.0f;
            {
                std::string tally;
                for (const auto& kv : bagTally) {
                    tally += " " + std::to_string(kv.first) + ":" + std::to_string(kv.second);
                }
                cell(2100.0f, y,
                     "ShuffleBag, " + std::to_string(bagCycles) + " cycles of " +
                         std::to_string(bagPerCycle) + " (three copies of item 0)",
                     kVal, 0.26f);
                y += 22.0f;
                cell(2120.0f, y, "every cycle dealt exactly 3, 1, 1, 1", kText, sz);
                cell(2520.0f, y, bagCyclesExact ? "yes" : "NO", bagCyclesExact ? kOk : kBad, sz);
                y += 21.0f;
                cell(2120.0f, y, "totals:" + tally, kVal, sz);
                y += 21.0f;
                cell(2120.0f, y, "repeats across a cycle boundary", kText, sz);
                cell(2520.0f, y,
                     std::to_string(bagBoundaryRepeats) + " of " + std::to_string(bagCycles - 1),
                     bagBoundaryRepeats == 0 ? kOk : kBad, sz);
                y += 21.0f;
                cell(2120.0f, y, "repeats inside a cycle", kText, sz);
                cell(2520.0f, y, std::to_string(bagInsideRepeats), kDim, sz);
            }
            y += 26.0f;
            font.drawText(*renderer, 2100.0f, y,
                          "Those last two lines are one promise and not two. A shuffle bag stops "
                          "the run of bad luck a dice roll allows, but only where it said it would: "
                          "never the same outcome side by side across a refill, which is the case "
                          "that would look broken. Inside a cycle item 0 holds three of the six "
                          "tickets, so it lands twice in a row often, and that is the bag being "
                          "fair rather than the bag failing.",
                          kDim, 0.25f);

            y += 84.0f;
            cell(2100.0f, y,
                 "WeightedReservoir: " + std::to_string(kReservoirItems) + " items streamed past, " +
                     std::to_string(kReservoirKeep) + " kept, " + std::to_string(kReservoirTrials) +
                     " runs",
                 kVal, 0.26f);
            y += 22.0f;
            cell(2100.0f, y, "weight", kDim, 0.24f);
            cell(2200.0f, y, "exact share", kDim, 0.24f);
            cell(2340.0f, y, "measured", kDim, 0.24f);
            y += 20.0f;
            for (const ReservoirRow& r : reservoir) {
                cell(2100.0f, y, num(r.weight, 1), kText, 0.24f);
                cell(2200.0f, y, num(r.exact, 5), kDim, 0.24f);
                cell(2340.0f, y, num(r.measured, 5), kVal, 0.24f);
                y += 19.0f;
            }
            cell(2100.0f, y, "worst deviation " + num(reservoirWorst, 5),
                 reservoirWorst < 0.01 ? kOk : kBad, 0.25f);

            font.drawText(*renderer, 24.0f, 1040.0f,
                          "None of this could be checked by looking at it. Noise that leans, points "
                          "that clump, a bag that favours one outcome — all of them look exactly "
                          "like the correct thing. The only way to know is to name the property "
                          "that has to hold and count how often it did not.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GRAIN shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
