// Maz Engine — "TELEMETRY" (core::RunningStats, core::RunningMedian, core::MovingAverage,
// core::P2Quantile, core::Histogram, core::HyperLogLog, core::CountMinSketch, core::BloomFilter,
// core::ReservoirSampler — measuring a stream you cannot keep, toward Godot's lack of any of it)
// Nine modules answering questions about the same 20,000 frame times, and the reason they all exist is
// the thing this demo is for: every one of them answers in fixed memory, without storing the stream.
// LEFT: the exact summary — mean, spread, worst frame — from core::RunningStats in a handful of doubles,
// beside a windowed median and moving average that forget on purpose, because a game cares what the last
// second looked like and not what happened at startup. MIDDLE: the shape, as a histogram, and the 99th
// percentile from core::P2Quantile, which is the number that decides whether a game feels smooth —
// computed in five doubles rather than by sorting 20,000 samples. RIGHT: the sketches, each shown next to
// the exact answer it approximates, so the error is visible rather than claimed. Fixed seed, no input.
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the nine are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/core/BloomFilter.hpp"
#include "maz/core/CountMinSketch.hpp"
#include "maz/core/Histogram.hpp"
#include "maz/core/HyperLogLog.hpp"
#include "maz/core/MovingAverage.hpp"
#include "maz/core/P2Quantile.hpp"
#include "maz/core/Pcg32.hpp"
#include "maz/core/ReservoirSampler.hpp"
#include "maz/core/RunningMedian.hpp"
#include "maz/core/RunningStats.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TELEMETRY starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Telemetry";
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

    // ---- The stream: 20,000 frame times in milliseconds -----------------------------------------------
    // Mostly near 16.7ms, with the occasional spike a real game has — a shader compile, a GC pause, a
    // chunk load. The spikes are the whole reason percentiles matter: they barely move the mean.
    core::Pcg32 rng(90210u, 11u);
    const int frames = 20000;

    core::RunningStats stats;
    core::RunningMedian median(120);            // two seconds at 60fps
    core::MovingAverage<double> recent(120);
    core::P2Quantile p99(0.99);
    core::Histogram shape(10.0, 40.0, 12);

    core::HyperLogLog distinct(12);
    core::CountMinSketch frequencies(1024, 4);
    core::BloomFilter<std::string> seenAssets(8192, 5);
    core::ReservoirSampler<double> keep(8, rng);

    // The exact answers, kept only so the approximations can be scored against them.
    std::vector<double> everySample;
    everySample.reserve(static_cast<std::size_t>(frames));
    std::set<std::string> exactDistinct;
    std::size_t exactCellarCount = 0;

    for (int i = 0; i < frames; ++i) {
        // A frame time: a base near 16.7ms, plus jitter, plus a rare spike.
        const double jitter = (static_cast<double>(rng.nextBounded(1000)) / 1000.0 - 0.5) * 2.4;
        const bool spike = rng.nextBounded(200) == 0;
        const double ms = 16.7 + jitter + (spike ? 12.0 + static_cast<double>(rng.nextBounded(900)) / 100.0
                                                 : 0.0);

        stats.push(ms);
        median.push(ms);
        recent.push(ms);
        p99.push(ms);
        shape.add(ms);
        keep.offer(ms);
        everySample.push_back(ms);

        // Alongside the timings, the things a session touches: which room, which asset.
        const char* rooms[] = {"cellar", "hall", "attic", "yard", "vault"};
        const std::string room = rooms[rng.nextBounded(5)];
        frequencies.add(room);
        if (room == "cellar") {
            exactCellarCount++;
        }

        const std::string asset = "tex_" + std::to_string(rng.nextBounded(700));
        distinct.add(asset);
        exactDistinct.insert(asset);
        seenAssets.add(asset);
    }

    // Exact answers for the ones with an approximation beside them.
    std::vector<double> sorted = everySample;
    std::sort(sorted.begin(), sorted.end());
    const double exactP99 = sorted[static_cast<std::size_t>(0.99 * static_cast<double>(sorted.size()))];
    const double exactMedian = sorted[sorted.size() / 2];
    const double p99Error = 100.0 * std::abs(p99.value() - exactP99) / exactP99;
    const double distinctError = 100.0 * std::abs(distinct.estimate() -
                                                  static_cast<double>(exactDistinct.size())) /
                                 static_cast<double>(exactDistinct.size());
    const std::uint64_t cellarEstimate = frequencies.estimate("cellar");

    // A Bloom filter never says "no" wrongly. It sometimes says "maybe" wrongly, and that is the trade.
    int falsePositives = 0;
    const int probes = 2000;
    for (int i = 0; i < probes; ++i) {
        const std::string neverSeen = "sfx_" + std::to_string(i);   // a different prefix entirely
        if (seenAssets.possiblyContains(neverSeen)) {
            falsePositives++;
        }
    }
    bool missedAnything = false;
    for (const std::string& a : exactDistinct) {
        if (!seenAssets.possiblyContains(a)) {
            missedAnything = true;                                   // must never happen
        }
    }
    const double falsePositiveRate = 100.0 * static_cast<double>(falsePositives) /
                                     static_cast<double>(probes);

    // What each of them costs, which is the reason to use them at all.
    const std::size_t exactBytes = everySample.size() * sizeof(double);
    const std::size_t hllBytes = distinct.registerCount();
    const std::size_t cmsBytes = frequencies.width() * frequencies.depth() * sizeof(std::uint64_t);
    const std::size_t bloomBytes = seenAssets.numBits() / 8;

    std::vector<std::string> histogramLines;
    for (std::size_t i = 0; i < shape.binCount(); ++i) {
        const double pct = shape.frequency(i) * 100.0;
        const int bars = static_cast<int>(pct / 2.0 + 0.5);
        // One decimal, not zero: the bins are 2.5ms wide, and rounding the edges to whole
        // numbers made consecutive bins read as "12-15" and "15-18" — overlapping labels for
        // bins that do not overlap.
        histogramLines.push_back(num(shape.binLow(i), 1) + "-" + num(shape.binHigh(i), 1) + "ms  " +
                                 std::string(static_cast<std::size_t>(bars < 0 ? 0 : bars), '#') + " " +
                                 num(pct, 1) + "%");
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kNo{1.0f, 0.48f, 0.42f, 1};

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

            const float sz = 0.29f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TELEMETRY", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "20,000 frame times, nine ways to measure them — none of which keeps the stream",
                          kDim, 0.34f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 235.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1: the exact summary, and the forgetful ones ----
            float y = 106.0f;
            font.drawText(*renderer, 24.0f, y, "core::RunningStats  -  EXACT", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "frames", std::to_string(stats.count()), kText); y += 25.0f;
            row(24.0f, y, "mean", num(stats.mean()) + " ms", kVal); y += 25.0f;
            row(24.0f, y, "std deviation", num(stats.stddev()) + " ms", kVal); y += 25.0f;
            row(24.0f, y, "best / worst", num(stats.min()) + " / " + num(stats.max()), kVal); y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "Welford's method: exact mean and variance in four doubles, one pass", kDim,
                          0.26f);

            y += 40.0f;
            font.drawText(*renderer, 24.0f, y, "THE LAST TWO SECONDS", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "windowed median", num(median.median()) + " ms", kVal); y += 25.0f;
            row(24.0f, y, "moving average", num(recent.average()) + " ms", kVal); y += 25.0f;
            row(24.0f, y, "window low / high", num(recent.min()) + " / " + num(recent.max()), kVal);
            y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "these forget on purpose — a game cares about now, not about startup", kDim,
                          0.26f);

            y += 40.0f;
            font.drawText(*renderer, 24.0f, y, "core::ReservoirSampler", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "kept", std::to_string(keep.size()) + " of " + std::to_string(keep.seen()),
                kVal); y += 25.0f;
            font.drawText(*renderer, 24.0f, y,
                          "eight samples, each equally likely, from a stream of unknown length", kDim,
                          0.26f);

            // ---- column 2: the shape, and the number that matters ----
            y = 106.0f;
            font.drawText(*renderer, 520.0f, y, "core::Histogram  -  THE SHAPE", kHead, 0.35f);
            y += 32.0f;
            for (const std::string& line : histogramLines) {
                font.drawText(*renderer, 520.0f, y, line.c_str(), kText, 0.27f);
                y += 22.0f;
            }
            y += 8.0f;
            row(520.0f, y, "fullest bin", num(shape.binCenter(shape.modeBin()), 1) + " ms", kVal);
            y += 25.0f;
            row(520.0f, y, "over 40 ms", std::to_string(shape.above()), kVal); y += 32.0f;

            font.drawText(*renderer, 520.0f, y, "core::P2Quantile  -  99th PERCENTILE", kHead, 0.35f);
            y += 34.0f;
            row(520.0f, y, "estimated", num(p99.value()) + " ms", kVal); y += 25.0f;
            row(520.0f, y, "exact (sorted)", num(exactP99) + " ms", kText); y += 25.0f;
            row(520.0f, y, "error", num(p99Error) + "%", p99Error < 5.0 ? kOk : kNo); y += 25.0f;
            row(520.0f, y, "median, for scale", num(exactMedian) + " ms", kDim); y += 30.0f;
            font.drawText(*renderer, 520.0f, y,
                          "five doubles instead of sorting 20,000 — and it is the 99th, not the mean,",
                          kDim, 0.26f);
            y += 22.0f;
            font.drawText(*renderer, 520.0f, y, "that decides whether a game feels smooth", kDim, 0.26f);

            // ---- column 3: the sketches, each against the truth ----
            y = 106.0f;
            font.drawText(*renderer, 1010.0f, y, "core::HyperLogLog", kHead, 0.35f);
            y += 34.0f;
            row(1010.0f, y, "distinct assets", num(distinct.estimate(), 0), kVal); y += 25.0f;
            row(1010.0f, y, "exact", std::to_string(exactDistinct.size()), kText); y += 25.0f;
            row(1010.0f, y, "error", num(distinctError) + "%", distinctError < 5.0 ? kOk : kNo);
            y += 25.0f;
            row(1010.0f, y, "memory", std::to_string(hllBytes) + " B", kOk); y += 32.0f;

            font.drawText(*renderer, 1010.0f, y, "core::CountMinSketch", kHead, 0.35f);
            y += 34.0f;
            row(1010.0f, y, "\"cellar\" visits", std::to_string(cellarEstimate), kVal); y += 25.0f;
            row(1010.0f, y, "exact", std::to_string(exactCellarCount), kText); y += 25.0f;
            row(1010.0f, y, "memory", std::to_string(cmsBytes / 1024) + " KB", kVal); y += 30.0f;
            font.drawText(*renderer, 1010.0f, y, "never undercounts; may overcount on a collision",
                          kDim, 0.26f);

            y += 36.0f;
            font.drawText(*renderer, 1010.0f, y, "core::BloomFilter", kHead, 0.35f);
            y += 34.0f;
            row(1010.0f, y, "missed a real asset", missedAnything ? "YES" : "never",
                missedAnything ? kNo : kOk); y += 25.0f;
            row(1010.0f, y, "false \"maybe\" rate", num(falsePositiveRate) + "%", kVal); y += 25.0f;
            row(1010.0f, y, "memory", std::to_string(bloomBytes) + " B", kOk); y += 30.0f;
            font.drawText(*renderer, 1010.0f, y, "\"no\" is always true; \"maybe\" sometimes is not",
                          kDim, 0.26f);

            font.drawText(*renderer, 24.0f, 664.0f,
                          ("Keeping all 20,000 samples costs " + std::to_string(exactBytes / 1024) +
                           " KB and grows forever. Every measurement on this screen was made in fixed "
                           "memory, in one pass, and each approximation is shown beside the truth it "
                           "approximates.").c_str(),
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TELEMETRY shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
