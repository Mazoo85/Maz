// Maz Engine — "ASSETS" (async streaming loader, toward Godot's ResourceLoader threaded API)
//
// A loading screen that streams a batch of assets on background threads via core::AssetServer and
// shows them arrive live: a grid of tiles flips from grey ("loading") to its decoded colour as each
// asset's worker job finishes and is harvested on the game thread by poll(). A big progress bar
// tracks server.progress() (loaded/total); when every asset is in, it reads READY. Each "decode"
// sleeps a fixed, index-derived time so the fill animates deterministically and settles at 100%
// within a couple of seconds — golden-stable. This is the runtime shape of Godot's
// load_threaded_request / get_status / get: request now, keep rendering, finalize on the main
// thread. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using namespace maz;

namespace {

void quad(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

// A decoded "asset": a colour and a byte size. In a real game this would be pixels or vertices.
struct Asset {
    render::Color color;
    int bytes;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ASSETS (async streaming loader) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Async Asset Streaming";
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

    // Background worker pool + the async asset server on top of it. The loader simulates a decode:
    // it sleeps an index-derived time (so the grid fills in a stable order) and returns a colour.
    core::JobSystem jobs;
    auto loader = [](const std::string& path) -> Asset {
        // Derive a deterministic index + colour from the path suffix ("tile_NN").
        int idx = 0;
        for (char c : path) {
            if (c >= '0' && c <= '9') {
                idx = idx * 10 + (c - '0');
            }
        }
        // Stagger completion: later tiles take a little longer, capped so the screen settles fast.
        std::this_thread::sleep_for(std::chrono::milliseconds(80 + (idx % 12) * 60));
        const float h = static_cast<float>(idx) / 24.0f; // hue-ish sweep across the batch
        const render::Color c{0.35f + 0.6f * (1.0f - h), 0.4f + 0.5f * h, 0.85f - 0.4f * h, 1.0f};
        return Asset{c, 1024 + idx * 137};
    };

    core::AssetServer<Asset> server(jobs, loader);

    // Kick off the whole batch at once — non-blocking; workers decode in parallel.
    constexpr int kCount = 24;
    std::vector<core::AssetId> ids;
    ids.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        char name[32];
        std::snprintf(name, sizeof(name), "tile_%02d", i);
        ids.push_back(server.request(name));
    }

    const int cols = 6;
    const float gridX = 340.0f, gridY = 150.0f;
    const float cell = 96.0f, pad = 12.0f;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        // Harvest any finished decode jobs on the game thread (where a GPU upload would happen).
        server.poll();
        const core::AssetServer<Asset>::Progress prog = server.progress();

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ASYNC ASSET STREAMING",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "request() enqueues decode jobs on worker threads; poll() finalizes them "
                          "on the game thread — the frame never blocks",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.32f);

            // Asset grid: grey while loading, decoded colour once ready.
            for (int i = 0; i < kCount; ++i) {
                // ids is a vector, so its subscript is size_t while i is an int loop counter.
                // Widened once here; i is 0..kCount-1, so the value is unchanged.
                const size_t idx = static_cast<size_t>(i);
                const int cx = i % cols;
                const int cy = i / cols;
                const float x = gridX + static_cast<float>(cx) * (cell + pad);
                const float y = gridY + static_cast<float>(cy) * (cell + pad);
                const bool loaded = server.status(ids[idx]) == core::AssetStatus::Loaded;
                quad(*renderer, x - 2, y - 2, cell + 4, cell + 4,
                     render::Color{0.24f, 0.27f, 0.33f, 1.0f});
                if (loaded) {
                    quad(*renderer, x, y, cell, cell, server.tryGet(ids[idx])->color);
                } else {
                    quad(*renderer, x, y, cell, cell, render::Color{0.15f, 0.16f, 0.20f, 1.0f});
                    font.drawText(*renderer, x + 28, y + 34, "...",
                                  render::Color{0.5f, 0.54f, 0.62f, 1}, 0.4f);
                }
            }

            // Progress bar for the whole batch.
            const float barX = gridX, barY = 560.0f, barW = 6 * cell + 5 * pad, barH = 34.0f;
            const float frac =
                prog.total > 0 ? static_cast<float>(prog.loaded) / static_cast<float>(prog.total)
                               : 0.0f;
            quad(*renderer, barX - 2, barY - 2, barW + 4, barH + 4,
                 render::Color{0.28f, 0.31f, 0.38f, 1.0f});
            quad(*renderer, barX, barY, barW, barH, render::Color{0.14f, 0.15f, 0.19f, 1.0f});
            const bool done = prog.loaded == prog.total;
            const render::Color fillCol = done ? render::Color{0.45f, 0.9f, 0.55f, 1.0f}
                                               : render::Color{0.45f, 0.72f, 1.0f, 1.0f};
            if (frac > 0.001f) {
                quad(*renderer, barX, barY, barW * frac, barH, fillCol);
            }

            char label[64];
            std::snprintf(label, sizeof(label), "%s  %d / %d", done ? "READY" : "STREAMING",
                          prog.loaded, prog.total);
            font.drawText(*renderer, barX, barY + barH + 16.0f, label,
                          render::Color{0.9f, 0.93f, 1.0f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ASSETS shutting down: %d/%d loaded (renderer %s)", server.progress().loaded,
                 server.progress().total, renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
