// Maz Engine — "SLOTMAP" (generational-handle slot-map, toward Godot's RID / stable handles)
// A live core::SlotMap<char> is driven through a scripted sequence — insert A,B,C; free B; insert D (which
// REUSES B's slot with a bumped generation) — and the result is drawn: the slot array (occupied vs free,
// each with its generation) and the handle table (which handles are still LIVE vs STALE). The teaching
// point is visible: handle hB went STALE because its slot was recycled by D at a new generation, so a
// dangling reference is safely detected. All static → deterministic, golden-stable. Run --headless.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillBox(render::Renderer& r, float x, float y, float w, float h, render::Color col) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SLOTMAP (generational handles) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Slot Map";
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

    // Drive a real SlotMap through the scripted sequence.
    core::SlotMap<char> map;
    const core::SlotHandle hA = map.insert('A');
    const core::SlotHandle hB = map.insert('B');
    const core::SlotHandle hC = map.insert('C');
    map.erase(hB);
    const core::SlotHandle hD = map.insert('D'); // reuses slot hB.index with a bumped generation

    // Snapshot the slot array for display: capacity() slots, live ones filled from forEach.
    struct SlotView {
        bool live = false;
        char value = 0;
        uint32_t gen = 0;
    };
    std::vector<SlotView> slots(map.capacity());
    map.forEach([&](core::SlotHandle h, char& v) {
        slots[h.index] = SlotView{true, v, h.generation};
    });

    struct HandleRow {
        const char* name;
        core::SlotHandle h;
    };
    const HandleRow handles[4] = {{"hA", hA}, {"hB", hB}, {"hC", hC}, {"hD", hD}};

    const render::Color kBg{0.08f, 0.09f, 0.12f, 1.0f};
    const render::Color kPanel{0.14f, 0.16f, 0.22f, 1.0f};
    const render::Color kLive{0.28f, 0.55f, 0.42f, 1.0f};
    const render::Color kFree{0.20f, 0.22f, 0.28f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1.0f};
    const render::Color kDim{0.66f, 0.72f, 0.84f, 1.0f};
    const render::Color kGreen{0.45f, 0.9f, 0.55f, 1.0f};
    const render::Color kRed{0.95f, 0.45f, 0.42f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SLOT MAP (generational handles / RID)",
                          kText, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "insert A,B,C  ->  free B  ->  insert D (reuses B's slot, bumps generation)",
                          kDim, 0.34f);

            // Slot array.
            font.drawText(*renderer, 30.0f, 110.0f, "SLOTS", kDim, 0.44f);
            for (std::size_t i = 0; i < slots.size(); ++i) {
                const float x = 40.0f + static_cast<float>(i) * 180.0f;
                const float y = 156.0f;
                const SlotView& s = slots[i];
                fillBox(*renderer, x, y, 150.0f, 150.0f, s.live ? kLive : kFree);
                char idx[40];
                std::snprintf(idx, sizeof(idx), "slot %zu", i);
                font.drawText(*renderer, x + 14.0f, y + 10.0f, idx, kDim, 0.34f);
                if (s.live) {
                    char val[4] = {s.value, 0, 0, 0};
                    font.drawText(*renderer, x + 58.0f, y + 52.0f, val, kText, 0.9f);
                    char g[16];
                    std::snprintf(g, sizeof(g), "gen %u", s.gen);
                    font.drawText(*renderer, x + 40.0f, y + 118.0f, g, kText, 0.32f);
                } else {
                    font.drawText(*renderer, x + 48.0f, y + 60.0f, "free", kDim, 0.4f);
                }
            }

            // Handle table.
            fillBox(*renderer, 40.0f, 380.0f, 1200.0f, 250.0f, kPanel);
            font.drawText(*renderer, 60.0f, 396.0f, "HANDLES  (name  ->  slot / gen  :  status)", kDim,
                          0.4f);
            for (int i = 0; i < 4; ++i) {
                const HandleRow& hr = handles[i];
                const float y = 448.0f + static_cast<float>(i) * 42.0f;
                const bool live = map.contains(hr.h);
                char line[96];
                if (live) {
                    std::snprintf(line, sizeof(line), "%s  ->  slot %u / gen %u  :  LIVE  '%c'", hr.name,
                                  hr.h.index, hr.h.generation, *map.get(hr.h));
                } else {
                    std::snprintf(line, sizeof(line), "%s  ->  slot %u / gen %u  :  STALE", hr.name,
                                  hr.h.index, hr.h.generation);
                }
                font.drawText(*renderer, 60.0f, y, line, live ? kGreen : kRed, 0.4f);
            }

            font.drawText(*renderer, 40.0f, 656.0f,
                          "hB is STALE: its slot was recycled by D at a new generation (dangling ref caught)",
                          kDim, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SLOTMAP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
