// Maz Engine — "RING" (core::RingBuffer, toward Godot's RingBuffer)
// A ring buffer is the container behind rolling histories and bounded queues. This demo drives two:
//   * a FRAME-TIME HISTORY (capacity 96): 140 deterministic frame samples are pushed, so the buffer holds
//     the most recent 96 (the older 44 were evicted). They plot as a scrolling bar graph — newest on the
//     right — coloured green/amber/red against a 16.6 ms budget line, with the rolling average drawn across.
//   * an INPUT BUFFER (capacity 8): a longer sequence of button presses is pushed; the buffer keeps only
//     the last 8 as labelled chips (a fighting game's move buffer), the rest overwritten.
// Both are pure core::RingBuffer; the app only reads them back with toVector()/at(). Deterministic fills ->
// golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color col) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, col);
}

// Deterministic synthetic frame time (ms) for sample i: a smooth base wave + hashed jitter + rare hitches.
float frameMs(int i) {
    const float base = 13.2f + 2.8f * std::sin(static_cast<float>(i) * 0.17f) +
                       1.6f * std::sin(static_cast<float>(i) * 0.41f + 1.0f);
    const std::uint32_t h = static_cast<std::uint32_t>(i) * 2654435761u;
    const float jitter = (static_cast<float>((h >> 8) & 0xFFFFu) / 65535.0f - 0.5f) * 3.2f;
    const float hitch = (i % 37 == 0) ? 11.0f : ((i % 19 == 0) ? 5.0f : 0.0f);
    return base + jitter + hitch;
}

render::Color barColor(float ms) {
    if (ms > 22.0f) {
        return render::Color{0.9f, 0.32f, 0.32f, 1.0f}; // red — a hitch
    }
    if (ms > 16.6f) {
        return render::Color{0.95f, 0.75f, 0.3f, 1.0f}; // amber — over budget
    }
    return render::Color{0.42f, 0.82f, 0.5f, 1.0f}; // green — under budget
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RING (core::RingBuffer) starting");

    // Frame-time history: push 140 samples into a 96-slot ring; it keeps the last 96.
    core::RingBuffer<float> frames(96);
    for (int i = 0; i < 140; ++i) {
        frames.push(frameMs(i));
    }
    // Rolling average over the live window.
    float sum = 0.0f, lo = 1e9f, hi = -1e9f;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const float v = frames.at(i);
        sum += v;
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    const float avg = frames.size() ? sum / static_cast<float>(frames.size()) : 0.0f;

    // Input buffer: push a long sequence of button codes into an 8-slot ring; it keeps the last 8.
    // Codes: 0..3 = arrows, 4 = A, 5 = B, 6 = X, 7 = Y.
    core::RingBuffer<int> inputs(8);
    const int seq[] = {0, 2, 1, 4, 3, 5, 0, 1, 6, 2, 3, 7, 4, 0, 5, 1};
    for (int code : seq) {
        inputs.push(code);
    }
    const char* labels[] = {"<", ">", "^", "v", "A", "B", "X", "Y"};

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — RingBuffer";
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

    const render::Color label{0.82f, 0.86f, 0.94f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.07f, 0.1f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RINGBUFFER",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "a fixed-capacity circular buffer - rolling history + bounded queue "
                          "(core::RingBuffer)",
                          label, 0.4f);

            // --- Frame-time history graph ---
            font.drawText(*renderer, 60.0f, 96.0f,
                          "frame-time history (capacity 96, 140 pushed -> oldest 44 evicted)",
                          render::Color{0.7f, 0.8f, 1.0f, 1}, 0.38f);

            const float gx = 60.0f, gTop = 130.0f, gBase = 520.0f;
            const float perMs = 13.0f; // pixels per millisecond
            const float bw = 7.6f;
            // Backdrop.
            fillRect(*renderer, gx - 6.0f, gTop - 6.0f, static_cast<float>(frames.size()) * bw + 12.0f,
                     gBase - gTop + 30.0f, render::Color{0.11f, 0.12f, 0.16f, 1.0f});

            // Budget line at 16.6 ms (dashed).
            const float budgetY = gBase - 16.6f * perMs;
            for (float x = gx; x < gx + static_cast<float>(frames.size()) * bw; x += 14.0f) {
                fillRect(*renderer, x, budgetY, 7.0f, 1.5f, render::Color{0.9f, 0.6f, 0.3f, 0.7f});
            }
            font.drawText(*renderer, gx + static_cast<float>(frames.size()) * bw + 6.0f, budgetY - 8.0f,
                          "16.6ms", render::Color{0.9f, 0.7f, 0.4f, 1}, 0.3f);

            // Bars (oldest -> newest, left -> right).
            for (std::size_t i = 0; i < frames.size(); ++i) {
                const float v = frames.at(i);
                const float bh = v * perMs;
                const float x = gx + static_cast<float>(i) * bw;
                fillRect(*renderer, x, gBase - bh, bw - 1.4f, bh, barColor(v));
            }

            // Rolling-average line.
            const float avgY = gBase - avg * perMs;
            fillRect(*renderer, gx, avgY, static_cast<float>(frames.size()) * bw, 1.8f,
                     render::Color{0.6f, 0.8f, 1.0f, 0.9f});
            font.drawText(*renderer, gx + static_cast<float>(frames.size()) * bw + 6.0f, avgY - 8.0f,
                          "avg", render::Color{0.7f, 0.85f, 1.0f, 1}, 0.3f);

            // Stats readout.
            char stat[128];
            std::snprintf(stat, sizeof(stat), "min %.1f   avg %.1f   max %.1f  (ms)", static_cast<double>(lo),
                          static_cast<double>(avg), static_cast<double>(hi));
            font.drawText(*renderer, gx, gBase + 12.0f, stat, label, 0.34f);

            // --- Input buffer chips ---
            font.drawText(*renderer, 60.0f, 590.0f, "input buffer (capacity 8) - last 8 presses, older overwritten",
                          render::Color{0.7f, 0.8f, 1.0f, 1}, 0.38f);
            const float cx = 60.0f, cy = 626.0f, cw = 58.0f, ch = 48.0f, gap = 10.0f;
            const auto in = inputs.toVector();
            for (std::size_t i = 0; i < in.size(); ++i) {
                const float x = cx + static_cast<float>(i) * (cw + gap);
                const bool newest = (i + 1 == in.size());
                fillRect(*renderer, x, cy, cw, ch,
                         newest ? render::Color{0.22f, 0.34f, 0.54f, 1.0f}
                                : render::Color{0.16f, 0.18f, 0.24f, 1.0f});
                fillRect(*renderer, x, cy, cw, 3.0f,
                         newest ? render::Color{0.45f, 0.65f, 0.95f, 1.0f}
                                : render::Color{0.3f, 0.34f, 0.42f, 1.0f});
                font.drawText(*renderer, x + 20.0f, cy + 12.0f, labels[in[i] & 7],
                              render::Color{0.9f, 0.93f, 1.0f, 1}, 0.5f);
            }
            font.drawText(*renderer, cx, cy + ch + 10.0f, "oldest -> newest", label, 0.3f);

            // Side note.
            const char* notes[] = {
                "* push() always succeeds: once full it",
                "  overwrites the OLDEST slot, so the buffer",
                "  is a sliding window of the last N items.",
                "",
                "* at(0) is the oldest, at(size-1) the newest,",
                "  no matter where the data physically wraps.",
                "",
                "* pushBack()/popFront() make it a bounded",
                "  FIFO queue instead (reject-when-full).",
            };
            for (std::size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); ++i) {
                font.drawText(*renderer, 880.0f, 96.0f + static_cast<float>(i) * 22.0f, notes[i],
                              render::Color{0.72f, 0.76f, 0.85f, 1}, 0.3f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RING shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
