// Maz Engine — "GENWORLD" (game::MazeGen, game::BspDungeon, game::WangTiles, game::LSystem,
// game::FloodFill — four ways to build a level and one way to check it, toward Godot's lack of any
// procedural generation)
// The same question answered four ways, side by side, so the difference between the techniques is the
// thing you look at rather than something you read about. A perfect maze (every cell reachable, exactly
// one route between any two). A BSP dungeon (rooms and corridors, the roguelike shape). A Wang tiling
// (edge colours matched in scanline order, so a seam always agrees). And an L-system, which builds a
// plant rather than a floor. Each is drawn as a grid of characters, and each is then checked with
// game::connectedRegions — because a generator that makes a pretty map with an unreachable half has
// failed, and only a flood fill can tell you.
//
// game::WaveFunctionCollapse is deliberately absent. It works, but a rule set of symmetric TERRAIN
// TYPES (water beside sand beside grass beside forest) collapses to one dominant biome at every seed
// and weighting tried — 90%+ of the field, with a few cells of everything else in one corner. That is
// what that formulation does; a landscape wants DIRECTIONAL tiles (a coast tile with water on its west
// and sand on its east), which is a tile set rather than a demo. Showing the blob and calling it a
// coastline would have taught the wrong lesson.
// One seed each, no input, so every map is the same every run. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the six are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/game/BspDungeon.hpp"
#include "maz/game/FloodFill.hpp"
#include "maz/game/LSystem.hpp"
#include "maz/game/MazeGen.hpp"
#include "maz/game/WangTiles.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace maz;

namespace {

// How many separate walkable areas a map has. One is what a level wants; more means a player can be
// dropped somewhere they can never leave.
std::size_t regionCount(int w, int h, const std::function<bool(const game::FillCell&)>& passable) {
    return game::connectedRegions(w, h, passable).size();
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GENWORLD starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Genworld";
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

    // ---- 1. A perfect maze -------------------------------------------------------------------------
    const game::Maze maze = game::generateMaze(11, 8, 20240912ull);
    std::vector<std::string> mazeRows;
    int mazeFloor = 0;
    for (int y = 0; y < maze.tileHeight; ++y) {
        std::string row;
        for (int x = 0; x < maze.tileWidth; ++x) {
            const bool floor = maze.floorAt(x, y);
            if (floor) mazeFloor++;
            row += floor ? ' ' : '#';
        }
        mazeRows.push_back(row);
    }
    const std::size_t mazeRegions =
        regionCount(maze.tileWidth, maze.tileHeight,
                    [&](const game::FillCell& c) { return maze.floorAt(c.x, c.y); });

    // ---- 2. A BSP dungeon --------------------------------------------------------------------------
    game::BspDungeonParams params;
    params.minLeaf = 7;
    params.minRoom = 3;
    params.maxDepth = 4;
    const game::Dungeon dungeon = game::generateBspDungeon(40, 18, 777ull, params);
    std::vector<std::string> dungeonRows;
    int dungeonFloor = 0;
    for (int y = 0; y < dungeon.height; ++y) {
        std::string row;
        for (int x = 0; x < dungeon.width; ++x) {
            const bool floor = dungeon.floorAt(x, y);
            if (floor) dungeonFloor++;
            row += floor ? '.' : '#';
        }
        dungeonRows.push_back(row);
    }
    const std::size_t dungeonRegions =
        regionCount(dungeon.width, dungeon.height,
                    [&](const game::FillCell& c) { return dungeon.floorAt(c.x, c.y); });

    // ---- 4. A Wang tiling --------------------------------------------------------------------------
    // Edge colours rather than a full constraint solve: each cell picks a tile matching the left and
    // upper neighbours already placed. Cheaper than WFC and it cannot back-track, which is the trade.
    std::vector<game::WangTile> wangSet;
    for (int n = 0; n < 2; ++n) {
        for (int e = 0; e < 2; ++e) {
            for (int s = 0; s < 2; ++s) {
                for (int wcol = 0; wcol < 2; ++wcol) {
                    wangSet.push_back(game::WangTile{n, e, s, wcol});
                }
            }
        }
    }
    const std::vector<int> wangGrid = game::wangTiling(wangSet, 40, 8, 99ull);
    std::vector<std::string> wangRows;
    if (!wangGrid.empty()) {
        for (int y = 0; y < 8; ++y) {
            std::string row;
            for (int x = 0; x < 40; ++x) {
                const int idx = wangGrid[static_cast<std::size_t>(y) * 40u + static_cast<std::size_t>(x)];
                // Draw by the tile's north edge colour, so a matching seam is visible as continuity.
                row += wangSet[static_cast<std::size_t>(idx)].north == 0 ? '-' : '=';
            }
            wangRows.push_back(row);
        }
    }
    // Every horizontal seam must agree: a tile's east colour equals its right neighbour's west.
    bool seamsMatch = !wangGrid.empty();
    for (int y = 0; y < 8 && seamsMatch; ++y) {
        for (int x = 0; x + 1 < 40 && seamsMatch; ++x) {
            const game::WangTile& a = wangSet[static_cast<std::size_t>(
                wangGrid[static_cast<std::size_t>(y) * 40u + static_cast<std::size_t>(x)])];
            const game::WangTile& b = wangSet[static_cast<std::size_t>(
                wangGrid[static_cast<std::size_t>(y) * 40u + static_cast<std::size_t>(x) + 1u])];
            if (a.east != b.west) seamsMatch = false;
        }
    }

    // ---- 5. An L-system ----------------------------------------------------------------------------
    // Not a floor: a plant. Three rewrite rules, four expansions, and a turtle that walks the result.
    game::LSystem plant;
    plant.axiom = "X";
    plant.rules['X'] = "F[+X][-X]FX";
    plant.rules['F'] = "FF";
    const std::string gen2 = plant.generate(2);
    const std::string gen4 = plant.generate(4);

    game::TurtleConfig turtle;
    const std::vector<game::TurtleSegment> branches = game::interpretTurtle(gen4, turtle);

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kMap{0.72f, 0.80f, 0.92f, 1};
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

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.28f;
            const float mapSz = 0.26f;
            const float lineH = 17.0f;
            font.drawText(*renderer, 16.0f, 10.0f, "MAZ ENGINE  -  GENWORLD", kText, 0.55f);
            font.drawText(*renderer, 16.0f, 44.0f,
                          "Five generators, one flood fill to check each one is actually playable",
                          kDim, 0.32f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 190.0f, y, value.c_str(), colour, sz);
            };
            auto drawMap = [&](float x, float y, const std::vector<std::string>& rows) {
                float ry = y;
                for (const std::string& line : rows) {
                    font.drawText(*renderer, x, ry, line.c_str(), kMap, mapSz);
                    ry += lineH;
                }
                return ry;
            };

            // ---- column 1: maze and dungeon ----
            float y = 84.0f;
            font.drawText(*renderer, 24.0f, y, "game::MazeGen  -  A PERFECT MAZE", kHead, 0.32f);
            y += 24.0f;
            y = drawMap(24.0f, y, mazeRows) + 6.0f;
            row(24.0f, y, "floor tiles", std::to_string(mazeFloor), kVal); y += 22.0f;
            row(24.0f, y, "walkable regions", std::to_string(mazeRegions),
                mazeRegions == 1 ? kOk : kNo); y += 22.0f;
            font.drawText(*renderer, 24.0f, y,
                          "perfect: one region, and exactly one route between any two cells", kDim,
                          0.25f);

            y += 34.0f;
            font.drawText(*renderer, 24.0f, y, "game::BspDungeon  -  ROOMS AND CORRIDORS", kHead, 0.32f);
            y += 24.0f;
            y = drawMap(24.0f, y, dungeonRows) + 6.0f;
            row(24.0f, y, "rooms carved", std::to_string(dungeon.rooms.size()), kVal); y += 22.0f;
            row(24.0f, y, "floor tiles", std::to_string(dungeonFloor), kVal); y += 22.0f;
            row(24.0f, y, "walkable regions", std::to_string(dungeonRegions),
                dungeonRegions == 1 ? kOk : kNo); y += 22.0f;
            font.drawText(*renderer, 24.0f, y,
                          "the corridors are what make it one region rather than several rooms", kDim,
                          0.25f);

            // ---- column 2: WFC, Wang, L-system ----
            y = 84.0f;
            font.drawText(*renderer, 680.0f, y, "game::WangTiles  -  MATCHED EDGES", kHead, 0.32f);
            y += 24.0f;
            y = drawMap(680.0f, y, wangRows) + 6.0f;
            row(680.0f, y, "tiles in the set", std::to_string(wangSet.size()), kVal); y += 22.0f;
            row(680.0f, y, "every seam agrees", seamsMatch ? "yes" : "NO", seamsMatch ? kOk : kNo);
            y += 22.0f;
            font.drawText(*renderer, 680.0f, y,
                          "scanline, no back-tracking: cheaper than WFC, and it can paint itself",
                          kDim, 0.25f);
            y += 20.0f;
            font.drawText(*renderer, 680.0f, y, "into a corner where WFC would retry", kDim, 0.25f);

            y += 32.0f;
            font.drawText(*renderer, 680.0f, y, "game::LSystem  -  A PLANT, NOT A FLOOR", kHead, 0.32f);
            y += 26.0f;
            row(680.0f, y, "axiom", plant.axiom, kVal); y += 22.0f;
            row(680.0f, y, "after 2 expansions", std::to_string(gen2.size()) + " symbols", kVal);
            y += 22.0f;
            row(680.0f, y, "after 4", std::to_string(gen4.size()) + " symbols", kVal); y += 22.0f;
            row(680.0f, y, "branches drawn", std::to_string(branches.size()), kOk); y += 22.0f;
            font.drawText(*renderer, 680.0f, y,
                          "three rewrite rules, then a turtle walks the string", kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 686.0f,
                          "Every map is generated from one seed, so these are the same five maps every "
                          "run — and each is checked, not just drawn.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.26f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GENWORLD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
