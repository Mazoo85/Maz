// Maz Engine — "LEVEL" (on-disk JSON level pipeline)
// The whole level is an editable file on disk: assets/levels/arena.json describes a tile grid (rows
// of single-char tile codes), a color palette per code, which codes are solid, and a list of pickup
// positions. At startup the app reads that file with io::parseJsonFile, builds a game::Tilemap +
// pickups from it, and renders it top-down. It also round-trips the parsed level back out to the
// save directory with io::writeJsonFile, proving the format saves as well as loads. This is the
// payoff of the JSON work: real, human-editable content loaded from disk — not baked into code.
// The pickup pulse is a pure function of the fixed-step clock, so the render is golden-stable.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
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

render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.9f ? (1.0f - d) / 0.1f : 1.0f);
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

// A palette maps a tile code (0..9) to an RGB color; -1 alpha marks "no entry" (unused codes).
struct Palette {
    render::Color colors[10];
    bool present[10] = {};
};

struct Pickup {
    int tx, ty;
};

struct Level {
    std::string name = "(unnamed)";
    float tileSize = 40.0f;
    game::Tilemap map;
    Palette palette;
    std::vector<Pickup> pickups;
    bool loaded = false;
};

// Build a Level from a parsed JSON document. Missing/malformed fields degrade to sensible defaults
// rather than failing, so a partial file still yields a drawable (if sparse) level.
Level buildLevel(const io::JsonValue& doc) {
    Level lv;
    lv.name = doc["name"].asString("(unnamed)");
    lv.tileSize = doc["tileSize"].asFloat(40.0f);

    // Palette: an object whose keys are tile-code digits.
    for (const auto& kv : doc["palette"].fields().items) {
        if (kv.first.size() != 1 || kv.first[0] < '0' || kv.first[0] > '9') continue;
        const int code = kv.first[0] - '0';
        lv.palette.colors[code] = render::Color{kv.second[0].asFloat(0.5f),
                                                kv.second[1].asFloat(0.5f),
                                                kv.second[2].asFloat(0.5f), 1.0f};
        lv.palette.present[code] = true;
    }

    // Tile grid: an array of equal-length strings, one per row.
    const io::JsonValue& rows = doc["tiles"];
    const uint32_t h = static_cast<uint32_t>(rows.size());
    uint32_t w = 0;
    for (const io::JsonValue& row : rows.items())
        w = std::max(w, static_cast<uint32_t>(row.asString().size()));
    lv.map.resize(w, h, 0);
    lv.map.setTileSize(lv.tileSize);
    for (uint32_t y = 0; y < h; ++y) {
        const std::string& row = rows[y].asString();
        for (uint32_t x = 0; x < row.size(); ++x) {
            const char ch = row[x];
            if (ch >= '0' && ch <= '9')
                lv.map.set(static_cast<int>(x), static_cast<int>(y),
                           static_cast<game::TileId>(ch - '0'));
        }
    }

    // Solidity: an array of tile codes that block movement.
    for (const io::JsonValue& s : doc["solid"].items())
        lv.map.setSolid(static_cast<game::TileId>(s.asInt()), true);

    // Pickups: an array of { x, y } tile coordinates.
    for (const io::JsonValue& p : doc["pickups"].items())
        lv.pickups.push_back({p["x"].asInt(), p["y"].asInt()});

    lv.loaded = true;
    return lv;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LEVEL (on-disk JSON level) starting");

    // --- Load the level file from disk -----------------------------------------------------------
    const char* base = SDL_GetBasePath();
    const std::string levelPath =
        (base ? std::string(base) : std::string()) + "assets/levels/arena.json";
    auto parsed = io::parseJsonFile(levelPath);
    if (!parsed.ok) {
        MAZ_LOG_ERROR("failed to load level %s: %s", levelPath.c_str(), parsed.error.c_str());
    }
    Level level = buildLevel(parsed.value);
    MAZ_LOG_INFO("loaded level '%s': %ux%u tiles, %zu pickups", level.name.c_str(),
                 level.map.width(), level.map.height(), level.pickups.size());

    // Round-trip the parsed level back out to the save directory — proves the format saves too.
    if (parsed.ok) {
        const std::string savePath = platform::prefPath("MazEngine", "level", "arena.saved.json");
        if (io::writeJsonFile(savePath, parsed.value, 2))
            MAZ_LOG_INFO("round-tripped level to %s", savePath.c_str());
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — On-Disk JSON Level";
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
    render::TextureHandle disc = discTex(*renderer, 48);

    ui::Font font;
    {
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    float t = 0.0f;
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
            t += static_cast<float>(clock.fixedDelta());
        }

        renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Center the grid on screen.
            const float ts = level.tileSize;
            const float gridW = static_cast<float>(level.map.width()) * ts;
            const float gridH = static_cast<float>(level.map.height()) * ts;
            const float ox = (sw - gridW) * 0.5f;
            const float oy = (sh - gridH) * 0.5f + 12.0f;

            // Tiles.
            for (uint32_t y = 0; y < level.map.height(); ++y) {
                for (uint32_t x = 0; x < level.map.width(); ++x) {
                    const game::TileId code = level.map.at(static_cast<int>(x), static_cast<int>(y));
                    if (code >= 10 || !level.palette.present[code]) continue;
                    render::SpriteDesc d;
                    d.x = ox + static_cast<float>(x) * ts;
                    d.y = oy + static_cast<float>(y) * ts;
                    d.width = ts - 2.0f;
                    d.height = ts - 2.0f;
                    d.color = level.palette.colors[code];
                    renderer->drawSprite(white, d);
                }
            }

            // Pickups: pulsing gold discs at tile centers.
            const float pulse = 0.75f + 0.25f * std::sin(t * 4.0f);
            for (const Pickup& p : level.pickups) {
                const float size = ts * 0.5f * pulse;
                render::SpriteDesc d;
                d.x = ox + (static_cast<float>(p.tx) + 0.5f) * ts - size * 0.5f;
                d.y = oy + (static_cast<float>(p.ty) + 0.5f) * ts - size * 0.5f;
                d.width = size;
                d.height = size;
                d.color = render::Color{1.0f, 0.82f, 0.30f, 1.0f};
                renderer->drawSprite(disc, d);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ON-DISK JSON LEVEL",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "loaded '%s' from arena.json  -  %ux%u tiles, %zu pickups",
                          level.name.c_str(), level.map.width(), level.map.height(),
                          level.pickups.size());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LEVEL shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
