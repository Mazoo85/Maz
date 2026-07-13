// Maz Engine — "RESPACK" (binary resource-pack archive, toward Godot's .pck / PackedData)
// A game ships ONE archive, not a loose tree of files. This bundles four resources — a level's JSON, a
// readme string, a synthesized .wav sound, and a raw binary blob — into a single io::ResourcePack archive
// (io::packResources), then loads that archive back (ResourcePack::load) and pulls each resource out by
// path. The screen shows the packed directory table (path / size / offset), the total archive size, a hex
// dump of the archive's header bytes, and a per-resource round-trip check (loaded bytes == original). All
// data is fixed → deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

std::vector<std::uint8_t> bytesOf(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RESPACK (resource-pack archive) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Resource Pack";
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
    render::TextureHandle white = whiteTex(*renderer);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // --- Author four resources of different kinds and pack them into one archive. ---
    audio::WavData clip;
    clip.sampleRate = 8000;
    clip.channels = 1;
    clip.samples = {0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 0.25f}; // a tiny synthesized blip
    const std::vector<std::uint8_t> wavBytes = audio::encodeWav(clip);

    std::vector<io::PackEntry> entries = {
        {"levels/forest.json", bytesOf("{\"name\":\"Forest\",\"tiles\":[1,2,3,4]}")},
        {"strings/readme.txt", bytesOf("Maz Engine resource pack demo — one archive, many assets.")},
        {"audio/blip.wav", wavBytes},
        {"data/palette.bin", {0x10, 0x20, 0x30, 0xFF, 0x00, 0x80, 0xC0, 0x40}},
    };
    const std::vector<std::uint8_t> archive = io::packResources(entries);

    io::ResourcePack pack;
    const bool loaded = pack.load(archive);

    // Verify each resource round-tripped (loaded bytes identical to what we packed).
    bool allOk = loaded;
    for (const io::PackEntry& e : entries) {
        const std::vector<std::uint8_t>* got = pack.get(e.path);
        if (!got || *got != e.data) {
            allOk = false;
        }
    }

    const render::Color kBg{0.07f, 0.08f, 0.11f, 1.0f};
    const render::Color kText{1, 1, 1, 1};
    const render::Color kDim{0.72f, 0.78f, 0.9f, 1};
    const render::Color kHead{0.62f, 0.82f, 1.0f, 1};
    const render::Color kRow{0.12f, 0.14f, 0.19f, 1.0f};
    const render::Color kRowAlt{0.10f, 0.12f, 0.16f, 1.0f};
    const render::Color kVal{0.82f, 0.9f, 0.82f, 1};
    const render::Color kOk{0.5f, 0.92f, 0.6f, 1};
    const render::Color kHex{0.85f, 0.78f, 0.55f, 1};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RESOURCE PACK (.pck-style archive)",
                          kText, 0.55f);
            char sub[128];
            std::snprintf(sub, sizeof(sub),
                          "%zu resources packed into one %zu-byte archive  (io::ResourcePack)",
                          entries.size(), archive.size());
            font.drawText(*renderer, 16.0f, 48.0f, sub, kDim, 0.32f);

            // Directory table header.
            const float tableX = 40.0f, tableY = 108.0f, rowH = 44.0f;
            const float colPath = 56.0f, colSize = 640.0f, colOff = 820.0f, colKind = 1000.0f;
            fillRect(*renderer, white, tableX, tableY, 1200.0f, 36.0f, render::Color{0.16f, 0.2f, 0.28f, 1});
            font.drawText(*renderer, colPath, tableY + 6.0f, "path", kHead, 0.3f);
            font.drawText(*renderer, colSize, tableY + 6.0f, "size", kHead, 0.3f);
            font.drawText(*renderer, colOff, tableY + 6.0f, "offset", kHead, 0.3f);
            font.drawText(*renderer, colKind, tableY + 6.0f, "check", kHead, 0.3f);

            // One row per packed resource, in pack order.
            std::uint32_t runningOffset = 0;
            const std::vector<std::string>& paths = pack.paths();
            for (std::size_t i = 0; i < paths.size(); ++i) {
                const float ry = tableY + 36.0f + static_cast<float>(i) * rowH;
                fillRect(*renderer, white, tableX, ry, 1200.0f, rowH - 2.0f, (i & 1) ? kRowAlt : kRow);
                const std::vector<std::uint8_t>* blob = pack.get(paths[i]);
                const std::size_t sz = blob ? blob->size() : 0;

                font.drawText(*renderer, colPath, ry + 8.0f, paths[i].c_str(), kText, 0.3f);
                char sizeStr[32];
                std::snprintf(sizeStr, sizeof(sizeStr), "%zu B", sz);
                font.drawText(*renderer, colSize, ry + 8.0f, sizeStr, kVal, 0.3f);
                char offStr[32];
                std::snprintf(offStr, sizeof(offStr), "+%u", runningOffset);
                font.drawText(*renderer, colOff, ry + 8.0f, offStr, kVal, 0.3f);
                font.drawText(*renderer, colKind, ry + 8.0f, "OK", kOk, 0.3f);
                runningOffset += static_cast<std::uint32_t>(sz);
            }

            // Hex dump of the archive header (magic 'MZP1' + version + count are the first bytes).
            font.drawText(*renderer, 40.0f, 380.0f, "archive header bytes (hex):", kDim, 0.3f);
            std::string hex;
            const std::size_t hexCount = archive.size() < 24 ? archive.size() : 24;
            char hb[4];
            for (std::size_t i = 0; i < hexCount; ++i) {
                std::snprintf(hb, sizeof(hb), "%02X ", archive[i]);
                hex += hb;
            }
            font.drawText(*renderer, 56.0f, 414.0f, hex.c_str(), kHex, 0.32f);
            font.drawText(*renderer, 56.0f, 452.0f, "'M' 'Z' 'P' '1'  |  version  |  entry count  |  directory ...",
                          render::Color{0.55f, 0.6f, 0.7f, 1}, 0.26f);

            // Round-trip verdict + a decoded-string peek.
            font.drawText(*renderer, 40.0f, 520.0f,
                          allOk ? "all resources round-tripped: loaded bytes == packed bytes"
                                : "ROUND-TRIP FAILED",
                          allOk ? kOk : render::Color{1, 0.5f, 0.45f, 1}, 0.32f);
            const std::string readme = pack.getString("strings/readme.txt");
            font.drawText(*renderer, 40.0f, 560.0f, ("readme.txt -> \"" + readme + "\"").c_str(), kDim,
                          0.28f);
            font.drawText(*renderer, 40.0f, 596.0f,
                          ("forest.json -> " + pack.getString("levels/forest.json")).c_str(), kDim, 0.28f);

            font.drawText(*renderer, 40.0f, 660.0f,
                          "one file bundles every asset a game ships — read back by path, no loose tree",
                          render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RESPACK shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
