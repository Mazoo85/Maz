// Maz Engine — "STRTABLE" (string interning, toward Godot's StringName)
// A game refers to the same names over and over (tags, signals, actions). This demo INTERNS a stream of
// repeated name references into a core::StringTable: each incoming name is turned into a small integer id
// (repeats collapse to the same id), and the unique names collect into a pool with their ids + FNV hashes.
// The left panel is the raw reference stream (name -> #id); the right panel is the deduplicated pool. All
// static → deterministic, golden-stable. Run --headless / --frames N for CI.

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
    MAZ_LOG_INFO("STRTABLE (string interning) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — String Interning";
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
        font.load(*renderer, fontPath.c_str(), 32.0f);
    }

    // A stream of tag references with repeats — the raw names a scene throws at the table.
    const std::vector<std::string> stream = {"player", "enemy",  "enemy", "pickup", "player",
                                             "boss",   "pickup", "enemy", "player", "exit_door"};

    // Intern them ONCE: each ref resolves to an id; the pool dedups.
    core::StringTable table;
    std::vector<core::StringId> refIds;
    refIds.reserve(stream.size());
    for (const std::string& s : stream) {
        refIds.push_back(table.intern(s));
    }

    const render::Color kBg{0.07f, 0.08f, 0.11f, 1.0f};
    const render::Color kPanel{0.13f, 0.15f, 0.20f, 1.0f};
    const render::Color kChip{0.24f, 0.42f, 0.62f, 1.0f};
    const render::Color kChipNew{0.30f, 0.62f, 0.40f, 1.0f};
    const render::Color kText{0.90f, 0.93f, 1.0f, 1.0f};
    const render::Color kDim{0.66f, 0.72f, 0.84f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  STRING INTERNING (StringName)", kText,
                          0.62f);
            char sub[128];
            std::snprintf(sub, sizeof(sub), "%zu name references collapse to %zu unique interned ids",
                          stream.size(), table.size());
            font.drawText(*renderer, 16.0f, 46.0f, sub, kDim, 0.36f);

            // Left panel — the raw reference stream, each name tagged with the id it resolved to.
            fillBox(*renderer, 30.0f, 96.0f, 560.0f, 590.0f, kPanel);
            font.drawText(*renderer, 50.0f, 112.0f, "REFERENCE STREAM  (name  ->  #id)", kDim, 0.4f);
            for (std::size_t i = 0; i < stream.size(); ++i) {
                const float y = 158.0f + static_cast<float>(i) * 50.0f;
                // A chip shows the id; the first time an id appears it's green ("new"), else blue.
                bool firstUse = true;
                for (std::size_t j = 0; j < i; ++j) {
                    if (refIds[j] == refIds[i]) {
                        firstUse = false;
                        break;
                    }
                }
                fillBox(*renderer, 50.0f, y, 54.0f, 36.0f, firstUse ? kChipNew : kChip);
                char idtxt[16];
                std::snprintf(idtxt, sizeof(idtxt), "%u", refIds[i].value);
                font.drawText(*renderer, 64.0f, y + 4.0f, idtxt, kText, 0.42f);
                font.drawText(*renderer, 124.0f, y + 4.0f, stream[i].c_str(), kText, 0.42f);
                if (firstUse) {
                    font.drawText(*renderer, 300.0f, y + 8.0f, "new", kChipNew, 0.32f);
                }
            }

            // Right panel — the deduplicated pool: id -> text + FNV hash.
            fillBox(*renderer, 620.0f, 96.0f, 630.0f, 590.0f, kPanel);
            font.drawText(*renderer, 640.0f, 112.0f, "INTERNED POOL  (#id  name  fnv)", kDim, 0.4f);
            for (std::size_t i = 0; i < table.size(); ++i) {
                const core::StringId id{static_cast<uint32_t>(i)};
                const float y = 158.0f + static_cast<float>(i) * 50.0f;
                fillBox(*renderer, 640.0f, y, 54.0f, 36.0f, kChip);
                char idtxt[16];
                std::snprintf(idtxt, sizeof(idtxt), "%u", id.value);
                font.drawText(*renderer, 654.0f, y + 4.0f, idtxt, kText, 0.42f);
                const std::string& name = table.str(id);
                font.drawText(*renderer, 714.0f, y + 4.0f, name.empty() ? "(empty)" : name.c_str(),
                              kText, 0.42f);
                char htxt[24];
                std::snprintf(htxt, sizeof(htxt), "0x%08X", table.hash(id));
                font.drawText(*renderer, 1000.0f, y + 8.0f, htxt, kDim, 0.32f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("STRTABLE shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
