// Maz Engine — "JOBS" (job-system / parallelism demo)
// Computes a Julia-set fractal twice at startup — once single-threaded, once via
// maz::core::JobSystem::parallelFor across worker threads — then displays the (identical) image and
// reports both timings and the measured speedup. Proves the engine can parallelize heavy data-
// parallel work; the pixels are deterministic, so only the *speed* differs. Run --headless/--frames N.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr int kW = 1024;
constexpr int kH = 576;
constexpr int kMaxIter = 256;

inline uint8_t toByte(float v) {
    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

// Compute one row of the Julia set into `px` (RGBA8). Pure per-row work: no shared writes across
// rows, so it parallelizes with zero synchronization.
void computeRow(int row, std::vector<uint8_t>& px, float cRe, float cIm) {
    const float aspect = static_cast<float>(kW) / static_cast<float>(kH);
    const float y = (static_cast<float>(row) / static_cast<float>(kH) - 0.5f) * 2.0f;
    for (int x = 0; x < kW; ++x) {
        float zr = (static_cast<float>(x) / static_cast<float>(kW) - 0.5f) * 2.0f * aspect * 1.4f;
        float zi = y * 1.4f;
        int iter = 0;
        while (iter < kMaxIter && zr * zr + zi * zi < 4.0f) {
            const float nzr = zr * zr - zi * zi + cRe;
            zi = 2.0f * zr * zi + cIm;
            zr = nzr;
            ++iter;
        }
        const size_t i = (static_cast<size_t>(row) * static_cast<size_t>(kW) +
                          static_cast<size_t>(x)) * 4;
        if (iter >= kMaxIter) {
            px[i] = px[i + 1] = px[i + 2] = 8; // inside the set: near-black
        } else {
            const float tt = static_cast<float>(iter) / static_cast<float>(kMaxIter);
            px[i] = toByte(0.5f + 0.5f * std::cos(6.2831853f * (tt * 3.0f + 0.0f)));
            px[i + 1] = toByte(0.5f + 0.5f * std::cos(6.2831853f * (tt * 3.0f + 0.33f)));
            px[i + 2] = toByte(0.5f + 0.5f * std::cos(6.2831853f * (tt * 3.0f + 0.66f)));
        }
        px[i + 3] = 255;
    }
}

double ms(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("JOBS (parallelism demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Job System";
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

    // A fixed Julia constant (a pretty region).
    const float cRe = -0.8f, cIm = 0.156f;

    core::JobSystem jobs;
    std::vector<uint8_t> single(static_cast<size_t>(kW) * kH * 4, 0);
    std::vector<uint8_t> multi(static_cast<size_t>(kW) * kH * 4, 0);

    // Single-threaded pass.
    const auto s0 = std::chrono::steady_clock::now();
    for (int row = 0; row < kH; ++row) {
        computeRow(row, single, cRe, cIm);
    }
    const auto s1 = std::chrono::steady_clock::now();

    // Multi-threaded pass: one job per chunk of rows.
    const auto m0 = std::chrono::steady_clock::now();
    jobs.parallelFor(0, static_cast<size_t>(kH),
                     [&](size_t row) { computeRow(static_cast<int>(row), multi, cRe, cIm); });
    const auto m1 = std::chrono::steady_clock::now();

    const double msSingle = ms(s0, s1);
    const double msMulti = ms(m0, m1);

    // Sanity: the two passes must produce identical pixels (parallelism must not change results).
    bool identical = single == multi;
    MAZ_LOG_INFO("JOBS %dx%d fractal: 1 thread %.1f ms, %u threads %.1f ms (%.2fx), identical=%d",
                 kW, kH, msSingle, jobs.workerCount(), msMulti,
                 msMulti > 0.0 ? msSingle / msMulti : 0.0, identical ? 1 : 0);

    render::TextureHandle tex =
        renderer->createTexture(static_cast<uint32_t>(kW), static_cast<uint32_t>(kH), multi.data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
        const float sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            // Static image; nothing to step. (The parallel work happened once at startup.)
        }

        renderer->setClearColor(render::Color{0.02f, 0.02f, 0.04f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Fill the window with the fractal (preserve aspect via letterbox width).
            render::SpriteDesc s;
            const float imgAspect = static_cast<float>(kW) / static_cast<float>(kH);
            float dw = sw, dh = sw / imgAspect;
            if (dh > sh) {
                dh = sh;
                dw = sh * imgAspect;
            }
            s.x = (sw - dw) * 0.5f;
            s.y = (sh - dh) * 0.5f;
            s.width = dw;
            s.height = dh;
            renderer->drawSprite(tex, s);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  JOB SYSTEM / parallelFor",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "%dx%d Julia  |  1 thread: %.0f ms  ->  %u threads: %.0f ms  =  %.2fx",
                          kW, kH, msSingle, jobs.workerCount(), msMulti,
                          msMulti > 0.0 ? msSingle / msMulti : 0.0);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.95f, 0.8f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("JOBS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
