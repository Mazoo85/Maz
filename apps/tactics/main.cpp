// Maz Engine — "TACTICS" (game::DijkstraMap, game::InfluenceMap, game::FieldOfView, game::ViewCone,
// game::FogOfWar, game::JumpPointSearch, game::ThetaStar, game::TurnOrder, game::UtilityAI —
// the layers a tactics AI thinks in, toward Godot's lack of any of them)
// One 34x16 battlefield, read nine ways, because that is how a tactics AI actually works: not one
// clever algorithm but a stack of cheap maps consulted in order. LEFT: a Dijkstra map — the step
// distance from every tile to the nearest goal, which is both the pursuit route (walk downhill) and,
// re-read, the escape route; and an influence map, where two sides push outward and the contested line
// is wherever they cancel. MIDDLE: what a guard can actually see — shadow-cast field of view, narrowed
// by a facing cone, and the fog that remembers where you have been. RIGHT: two pathfinders over the
// identical map, so the difference is visible rather than described — jump-point search returns grid
// steps, Theta* returns a line that cuts corners; then an initiative order and a utility AI choosing
// what to do with its turn. Fixed map, fixed positions, no input, so the picture is the test.
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the nine are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/game/DijkstraMap.hpp"
#include "maz/game/FieldOfView.hpp"
#include "maz/game/FogOfWar.hpp"
#include "maz/game/InfluenceMap.hpp"
#include "maz/game/JumpPointSearch.hpp"
#include "maz/game/ThetaStar.hpp"
#include "maz/game/TurnOrder.hpp"
#include "maz/game/UtilityAI.hpp"
#include "maz/game/ViewCone.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr int kW = 34;
constexpr int kH = 16;

// A fixed battlefield: a wall down the middle with two gaps, and a block of cover.
// '#' is solid, everything else is open.
const char* kMap[kH] = {
    "..................................",
    "..........#############...........",
    "..........#.................#.....",
    "..........#.....#####.......#.....",
    "..........#.....#...........#.....",
    "................#...........#.....",
    "..........#.....#...........#.....",
    "..........#.....#####.......#.....",
    "..........#.................#.....",
    "..........#############.....#.....",
    "............................#.....",
    "....######..................#.....",
    "....#....#..................#.....",
    "....#....#########################",
    "....#.............................",
    "..................................",
};

bool solidAt(int x, int y) {
    if (x < 0 || y < 0 || x >= kW || y >= kH) return true;
    return kMap[y][x] == '#';
}

std::string num(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TACTICS starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Tactics";
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

    const game::MapCell player{2, 14};
    const game::MapCell guard{24, 4};
    const game::MapCell objective{20, 5};

    // ---- 1. A Dijkstra map: the pursuit route, and the escape route ----------------------------------
    const auto blocked = [](const game::MapCell& c) { return solidAt(c.x, c.y); };
    const game::DijkstraMap toPlayer = game::buildDijkstraMap(kW, kH, {player}, blocked);
    const game::DijkstraMap away = game::makeFleeMap(toPlayer, blocked);

    // Walking downhill from the guard IS the pursuit. No path is stored anywhere.
    std::vector<game::MapCell> chase;
    {
        game::MapCell at = guard;
        for (int step = 0; step < 200; ++step) {
            chase.push_back(at);
            const game::MapCell next = toPlayer.descend(at);
            if (next.x == at.x && next.y == at.y) break;   // arrived, or walled in
            at = next;
        }
    }
    const bool chaseArrives = !chase.empty() && chase.back().x == player.x && chase.back().y == player.y;
    const int guardDistance = toPlayer.at(guard.x, guard.y);
    const int unreachableCells = [&] {
        int n = 0;
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                if (!solidAt(x, y) && toPlayer.at(x, y) >= game::DijkstraMap::kUnreachable) n++;
            }
        }
        return n;
    }();
    // One step of flight from a tile next to the guard: the flee map sends it the other way.
    const game::MapCell cornered{25, 4};
    const game::MapCell fleeStep = away.descend(cornered);
    const bool fleeMovesAway = toPlayer.at(fleeStep.x, fleeStep.y) >= toPlayer.at(cornered.x, cornered.y);

    // ---- 2. An influence map: where the front line is ------------------------------------------------
    game::InfluenceMap control(kW, kH);
    control.addSource(player.x, player.y, 1.0f);
    control.addSource(2, 12, 0.7f);
    control.addSource(guard.x, guard.y, -1.0f);
    control.addSource(24, 8, -0.8f);
    for (int i = 0; i < 16; ++i) control.propagate(0.98f, 0.42f);

    // Classified against the field's OWN peak, not an absolute number. Influence falls off with
    // distance by design, so a fixed threshold answers "is this tile near a source" rather than
    // "who holds this tile" — at 0.02 absolute, this map read as 453 contested tiles and nothing
    // held by anyone, which is not what the picture shows at all.
    float peak = 0.0f;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const float a = std::fabs(control.at(x, y));
            if (a > peak) peak = a;
        }
    }
    const float edge = peak * 0.05f;
    int held = 0, lost = 0, contested = 0;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            if (solidAt(x, y)) continue;
            const float v = control.at(x, y);
            if (v > edge) held++;
            else if (v < -edge) lost++;
            else contested++;
        }
    }
    // The tile a reinforcement should be sent to: the steepest pull among CONTESTED tiles, which is
    // where the two sides actually meet. Both halves of that were learned by measuring rather than
    // reasoning. A hand-picked objective sat inside the sealed inner room, where the gradient is
    // exactly zero. And the steepest pull on the whole map is not the front line at all — it is the
    // tile next to a source, because that is where a falling-off field falls off fastest.
    game::MapCell frontLine{0, 0};
    math::vec2 push(0.0f, 0.0f);
    float strongest = 0.0f;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            if (solidAt(x, y)) continue;
            if (std::fabs(control.at(x, y)) > edge) continue;   // not contested: someone holds it
            const math::vec2 g = control.gradient(x, y);
            const float mag = std::sqrt(g.x * g.x + g.y * g.y);
            if (mag > strongest) {
                strongest = mag;
                push = g;
                frontLine = game::MapCell(x, y);
            }
        }
    }

    // ---- 3. What the guard can see -------------------------------------------------------------------
    const auto opaque = [](const game::FovCell& c) { return solidAt(c.x, c.y); };
    const std::vector<game::FovCell> seen = game::computeFov(guard, 9, opaque);
    std::set<std::pair<int, int>> visible;
    for (const game::FovCell& c : seen) visible.insert({c.x, c.y});

    // A guard does not see behind itself: the cone narrows the FOV to what it is facing.
    const math::vec2 facing(-1.0f, 1.0f);          // looking down and to the left
    const float halfAngle = 0.9f;                  // ~51 degrees either side
    int inCone = 0;
    for (const game::FovCell& c : seen) {
        if (game::inViewCone2D(math::vec2(static_cast<float>(guard.x), static_cast<float>(guard.y)),
                               facing, math::vec2(static_cast<float>(c.x), static_cast<float>(c.y)),
                               halfAngle, 12.0f)) {
            inCone++;
        }
    }
    const game::ViewSample onPlayer = game::sampleViewCone2D(
        math::vec2(static_cast<float>(guard.x), static_cast<float>(guard.y)), facing,
        math::vec2(static_cast<float>(player.x), static_cast<float>(player.y)), halfAngle, 12.0f);

    // ---- 4. Fog that remembers ------------------------------------------------------------------------
    game::FogOfWar fog(kW, kH);
    // Three places the player has stood, in order. Only the last is lit now.
    const game::MapCell walked[3] = {{2, 14}, {6, 14}, {14, 14}};
    int exploredAfterFirst = 0;
    for (int i = 0; i < 3; ++i) {
        fog.beginFrame();
        fog.revealCircle(walked[i].x, walked[i].y, 5);
        if (i == 0) {
            for (int y = 0; y < kH; ++y) for (int x = 0; x < kW; ++x) if (fog.isExplored(x, y)) exploredAfterFirst++;
        }
    }
    int litNow = 0, rememberedNow = 0, neverSeen = 0;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            if (fog.isVisible(x, y)) litNow++;
            else if (fog.isExplored(x, y)) rememberedNow++;
            else neverSeen++;
        }
    }

    // ---- 5. Two pathfinders over the identical map ----------------------------------------------------
    game::JumpPointSearch jps;
    jps.setSize(kW, kH);
    std::vector<std::uint8_t> wallBits(static_cast<std::size_t>(kW) * static_cast<std::size_t>(kH), 0);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const bool s = solidAt(x, y);
            jps.setSolid(x, y, s);
            wallBits[static_cast<std::size_t>(y) * static_cast<std::size_t>(kW) +
                     static_cast<std::size_t>(x)] = s ? 1u : 0u;
        }
    }
    const std::vector<math::Vector2i> jpsPath =
        jps.findPath(math::Vector2i(player.x, player.y), math::Vector2i(guard.x, guard.y));
    const game::ThetaPath theta =
        game::thetaStar(wallBits, kW, kH, player.x, player.y, guard.x, guard.y);

    // Theta* returns corners, not steps: the count is the point of the comparison.
    const std::size_t jpsPoints = jpsPath.size();
    const std::size_t thetaPoints = theta.waypoints.size();

    // ---- 6. Whose turn, and what they do with it ------------------------------------------------------
    game::TurnOrder order;
    order.addCombatant(1, 14.0);   // the player
    order.addCombatant(2, 9.5);    // the guard
    order.addCombatant(3, 17.0);   // a scout
    order.addCombatant(4, 9.5);    // a second guard, tied on initiative
    order.start();
    std::vector<int> firstRound;
    for (std::size_t i = 0; i < order.combatantCount(); ++i) {
        firstRound.push_back(order.current());
        order.advance();
    }
    const int roundAfterOneLap = order.round();

    // A guard at 30% health, with the player close and its own side losing ground, weighs three
    // options. Each consideration is a normalised game value through a response curve; the action
    // with the highest product wins.
    auto consideration = [](float value, game::ResponseCurveType type, float slope, float exponent) {
        game::Consideration c;
        c.input = value;
        c.curve.type = type;
        c.curve.slope = slope;
        c.curve.exponent = exponent;
        return c;
    };
    const float health = 0.30f;
    const float enemyClose = 0.85f;
    const float losingGround = 0.70f;

    std::vector<game::UtilityAction> options(3);
    // Attack: wants the enemy close, and does not care much about its own health.
    options[0].considerations = {
        consideration(enemyClose, game::ResponseCurveType::Linear, 1.0f, 1.0f),
        consideration(health, game::ResponseCurveType::Linear, 0.6f, 1.0f)};
    // Retreat: wants low health, and wants the enemy close enough to be a threat.
    options[1].considerations = {
        consideration(1.0f - health, game::ResponseCurveType::Linear, 1.0f, 1.0f),
        consideration(enemyClose, game::ResponseCurveType::Linear, 1.0f, 1.0f)};
    // Call for help: wants to be losing ground, and to still be alive enough to shout.
    options[2].considerations = {
        consideration(losingGround, game::ResponseCurveType::Linear, 1.0f, 1.0f),
        consideration(health, game::ResponseCurveType::Linear, 1.0f, 1.0f)};

    float bestScore = 0.0f;
    const int chosen = game::selectBestAction(options, &bestScore);
    const char* actionNames[3] = {"attack", "retreat", "call for help"};
    const float scores[3] = {options[0].score(), options[1].score(), options[2].score()};

    // ---- The map, drawn once, with every layer that has something to say about a tile ------------------
    auto mapRows = [&](int layer) {
        std::vector<std::string> rows;
        for (int y = 0; y < kH; ++y) {
            std::string row;
            for (int x = 0; x < kW; ++x) {
                if (solidAt(x, y)) { row += '#'; continue; }
                if (layer == 0) {                                   // Dijkstra: distance banding
                    const int d = toPlayer.at(x, y);
                    if (d >= game::DijkstraMap::kUnreachable) row += '?';
                    else if (d == 0) row += '@';
                    else if (d < 8) row += '.';
                    else if (d < 16) row += ':';
                    else if (d < 26) row += '-';
                    else row += '=';
                } else if (layer == 1) {                            // influence: who holds it
                    const float v = control.at(x, y);
                    if (v > peak * 0.35f) row += 'O';
                    else if (v > edge) row += 'o';
                    else if (v < -peak * 0.35f) row += 'X';
                    else if (v < -edge) row += 'x';
                    else row += ' ';
                } else {                                            // sight: cone, fov, fog
                    const bool v = visible.count({x, y}) > 0;
                    const bool cone = v && game::inViewCone2D(
                        math::vec2(static_cast<float>(guard.x), static_cast<float>(guard.y)), facing,
                        math::vec2(static_cast<float>(x), static_cast<float>(y)), halfAngle, 12.0f);
                    if (x == guard.x && y == guard.y) row += 'G';
                    else if (cone) row += '!';
                    else if (v) row += '*';
                    else if (fog.isVisible(x, y)) row += '+';
                    else if (fog.isExplored(x, y)) row += '.';
                    else row += ' ';
                }
            }
            rows.push_back(row);
        }
        return rows;
    };
    const std::vector<std::string> distanceRows = mapRows(0);
    const std::vector<std::string> controlRows = mapRows(1);
    const std::vector<std::string> sightRows = mapRows(2);

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kMap{0.70f, 0.78f, 0.90f, 1};
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

            const float sz = 0.27f;
            const float mapSz = 0.25f;
            const float lineH = 15.0f;
            font.drawText(*renderer, 16.0f, 8.0f, "MAZ ENGINE  -  TACTICS", kText, 0.5f);
            font.drawText(*renderer, 16.0f, 40.0f,
                          "One battlefield, read nine ways — a tactics AI is a stack of cheap maps, "
                          "not one clever algorithm", kDim, 0.30f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 200.0f, y, value.c_str(), colour, sz);
            };
            auto drawMap = [&](float x, float y, const std::vector<std::string>& rows) {
                float ry = y;
                for (const std::string& line : rows) {
                    font.drawText(*renderer, x, ry, line.c_str(), kMap, mapSz);
                    ry += lineH;
                }
                return ry;
            };

            // ---- column 1: distance and control ----
            float y = 74.0f;
            font.drawText(*renderer, 20.0f, y, "game::DijkstraMap  -  DISTANCE TO THE PLAYER", kHead, 0.30f);
            y += 20.0f;
            y = drawMap(20.0f, y, distanceRows) + 4.0f;
            row(20.0f, y, "guard is", std::to_string(guardDistance) + " steps away", kVal); y += 20.0f;
            row(20.0f, y, "walking downhill", chaseArrives ? "reaches the player in " +
                              std::to_string(chase.size() - 1) + " steps" : "STUCK",
                chaseArrives ? kOk : kNo); y += 20.0f;
            row(20.0f, y, "unreachable tiles", std::to_string(unreachableCells), kVal); y += 20.0f;
            row(20.0f, y, "flee map sends it", fleeMovesAway ? "further away" : "THE WRONG WAY",
                fleeMovesAway ? kOk : kNo); y += 22.0f;
            font.drawText(*renderer, 20.0f, y,
                          "no path is stored: the map IS the route, for every tile at once", kDim, 0.24f);

            y += 30.0f;
            font.drawText(*renderer, 20.0f, y, "game::InfluenceMap  -  WHO HOLDS WHAT", kHead, 0.30f);
            y += 20.0f;
            y = drawMap(20.0f, y, controlRows) + 4.0f;
            row(20.0f, y, "held / lost / contested", std::to_string(held) + " / " +
                              std::to_string(lost) + " / " + std::to_string(contested), kVal); y += 20.0f;
            row(20.0f, y, "strongest pull at", std::to_string(frontLine.x) + "," +
                              std::to_string(frontLine.y), kVal); y += 20.0f;
            // Three decimals: this far from a source the field is weak, and at two the numbers
            // round to "-0.00, 0.00", which reads as broken rather than as small.
            row(20.0f, y, "pulling", num(push.x, 3) + ", " + num(push.y, 3), kVal);
            y += 22.0f;
            font.drawText(*renderer, 20.0f, y,
                          "the front line is wherever the two sides cancel, not a line anyone drew",
                          kDim, 0.24f);

            // ---- column 2: sight ----
            y = 74.0f;
            font.drawText(*renderer, 620.0f, y, "FieldOfView + ViewCone + FogOfWar", kHead, 0.30f);
            y += 20.0f;
            y = drawMap(620.0f, y, sightRows) + 4.0f;
            row(620.0f, y, "tiles in line of sight", std::to_string(seen.size()), kVal); y += 20.0f;
            row(620.0f, y, "of those, in the cone", std::to_string(inCone), kOk); y += 20.0f;
            row(620.0f, y, "player in the cone", onPlayer.inCone ? "SEEN" : "not seen",
                onPlayer.inCone ? kNo : kOk); y += 20.0f;
            row(620.0f, y, "angle off the facing", num(onPlayer.angle * 57.2958, 0) + " degrees", kVal);
            y += 20.0f;
            row(620.0f, y, "distance", num(onPlayer.distance, 1) + " tiles", kVal); y += 22.0f;
            font.drawText(*renderer, 620.0f, y,
                          "G guard   ! in the cone   * visible but behind it   + lit   . remembered",
                          kDim, 0.24f);
            y += 20.0f;
            font.drawText(*renderer, 620.0f, y,
                          "a wall is visible but does not reveal what stands behind it", kDim, 0.24f);

            y += 28.0f;
            font.drawText(*renderer, 620.0f, y, "game::FogOfWar  -  AFTER THREE MOVES", kHead, 0.30f);
            y += 22.0f;
            row(620.0f, y, "lit now", std::to_string(litNow), kVal); y += 20.0f;
            row(620.0f, y, "remembered", std::to_string(rememberedNow), kVal); y += 20.0f;
            row(620.0f, y, "never seen", std::to_string(neverSeen), kDim); y += 20.0f;
            row(620.0f, y, "explored after one move", std::to_string(exploredAfterFirst), kVal);
            y += 22.0f;
            font.drawText(*renderer, 620.0f, y,
                          "explored never shrinks; visible is rebuilt every frame", kDim, 0.24f);

            // ---- column 3: paths, turns, decisions ----
            y = 74.0f;
            font.drawText(*renderer, 1180.0f, y, "TWO PATHFINDERS, ONE MAP", kHead, 0.30f);
            y += 24.0f;
            row(1180.0f, y, "jump point search", std::to_string(jpsPoints) + " grid steps", kVal);
            y += 20.0f;
            row(1180.0f, y, "Theta*", std::to_string(thetaPoints) + " waypoints", kOk); y += 20.0f;
            row(1180.0f, y, "Theta* found a route", theta.found ? "yes" : "no",
                theta.found ? kOk : kNo); y += 22.0f;
            font.drawText(*renderer, 1180.0f, y, "same start, same goal, same walls:", kDim, 0.24f);
            y += 18.0f;
            font.drawText(*renderer, 1180.0f, y, "one returns every step, the other", kDim, 0.24f);
            y += 18.0f;
            font.drawText(*renderer, 1180.0f, y, "only the corners — which is what a", kDim, 0.24f);
            y += 18.0f;
            font.drawText(*renderer, 1180.0f, y, "unit should actually walk", kDim, 0.24f);

            y += 32.0f;
            font.drawText(*renderer, 1180.0f, y, "game::TurnOrder", kHead, 0.30f);
            y += 24.0f;
            {
                std::string names;
                for (std::size_t i = 0; i < firstRound.size(); ++i) {
                    if (i) names += " -> ";
                    names += std::to_string(firstRound[i]);
                }
                row(1180.0f, y, "first round", names, kVal); y += 20.0f;
            }
            row(1180.0f, y, "round after one lap", std::to_string(roundAfterOneLap), kVal); y += 22.0f;
            font.drawText(*renderer, 1180.0f, y, "highest initiative first; a tie keeps", kDim, 0.24f);
            y += 18.0f;
            font.drawText(*renderer, 1180.0f, y, "the order they were added", kDim, 0.24f);

            y += 32.0f;
            font.drawText(*renderer, 1180.0f, y, "game::UtilityAI  -  A GUARD DECIDES", kHead, 0.30f);
            y += 24.0f;
            row(1180.0f, y, "health", num(health * 100.0, 0) + "%", kNo); y += 20.0f;
            row(1180.0f, y, "enemy close", num(enemyClose * 100.0, 0) + "%", kVal); y += 20.0f;
            row(1180.0f, y, "losing ground", num(losingGround * 100.0, 0) + "%", kVal); y += 24.0f;
            for (int i = 0; i < 3; ++i) {
                const bool win = i == chosen;
                font.drawText(*renderer, 1180.0f, y, actionNames[i], win ? kOk : kDim, sz);
                font.drawText(*renderer, 1380.0f, y, num(scores[i]).c_str(), win ? kOk : kDim, sz);
                if (win) font.drawText(*renderer, 1450.0f, y, "<- chosen", kOk, sz);
                y += 20.0f;
            }
            y += 4.0f;
            font.drawText(*renderer, 1180.0f, y, "no rules, no tree: each option scores", kDim, 0.24f);
            y += 18.0f;
            font.drawText(*renderer, 1180.0f, y, "itself and the highest wins, so a new", kDim, 0.24f);
            y += 18.0f;
            font.drawText(*renderer, 1180.0f, y, "option changes nothing else", kDim, 0.24f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TACTICS shutting down, guard chose %s (renderer %s)", actionNames[chosen],
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
