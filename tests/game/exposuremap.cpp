// tests/game/exposuremap.cpp — verifies the cover/exposure map (game::buildExposureMap / ExposureMap).
// Ground truths: on an open grid every cell is seen by the single threat (exposure 1); a wall casts a SHADOW
// of covered (exposure 0) cells directly behind it while off-line cells stay exposed; a wall cell itself is 0;
// two threats that both see a cell give exposure 2; a threat on a wall or off the grid is skipped; empty grid
// and no-threats cases are safe; isCovered() reflects zero exposure. Pure CPU, deterministic.
#include "maz/game/ExposureMap.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::buildExposureMap;
using maz::game::ExposureMap;
using maz::game::ExposureCell;

int main() {
    // --- 1. Open grid, single threat: every cell exposed exactly once. ---
    {
        auto open = [](const ExposureCell&) { return false; };
        const ExposureMap m = buildExposureMap(7, 7, {ExposureCell(0, 3)}, open);
        bool allOne = true;
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 7; ++x) allOne = allOne && (m.at(x, y) == 1);
        CHECK(allOne, "open grid: every cell exposure 1");
        CHECK(!m.isCovered(5, 5), "open grid cell not covered");
    }

    // --- 2. A wall casts a shadow of covered cells behind it. ---
    {
        // Threat at (0,4); a single wall at (4,4). Cells on the same row past the wall are shadowed.
        auto blocked = [](const ExposureCell& c) { return c.x == 4 && c.y == 4; };
        const ExposureMap m = buildExposureMap(9, 9, {ExposureCell(0, 4)}, blocked);
        CHECK(m.at(3, 4) == 1, "cell before the wall is exposed");
        CHECK(m.at(5, 4) == 0 && m.at(8, 4) == 0, "cells directly behind the wall are covered");
        CHECK(m.isCovered(8, 4), "shadowed cell reports covered");
        CHECK(m.at(4, 4) == 0, "the wall cell itself stays 0");
        CHECK(m.at(8, 0) == 1, "an off-line cell still has clear line of sight");
    }

    // --- 3. Two threats both seeing a cell give exposure 2. ---
    {
        auto open = [](const ExposureCell&) { return false; };
        const ExposureMap m = buildExposureMap(9, 9, {ExposureCell(0, 0), ExposureCell(8, 8)}, open);
        CHECK(m.at(4, 4) == 2, "cell seen by two threats has exposure 2");
    }

    // --- 4. Threats on a wall or off the grid are skipped. ---
    {
        auto blocked = [](const ExposureCell& c) { return c.x == 2 && c.y == 2; };
        const ExposureMap onWall = buildExposureMap(5, 5, {ExposureCell(2, 2)}, blocked);
        bool allZero = true;
        for (int v : onWall.count) allZero = allZero && (v == 0);
        CHECK(allZero, "threat embedded in a wall contributes nothing");
        auto open = [](const ExposureCell&) { return false; };
        const ExposureMap oob = buildExposureMap(5, 5, {ExposureCell(99, 99)}, open);
        bool z = true;
        for (int v : oob.count) z = z && (v == 0);
        CHECK(z, "off-grid threat contributes nothing");
    }

    // --- 5. Degenerate: empty grid and no threats. ---
    {
        auto open = [](const ExposureCell&) { return false; };
        CHECK(buildExposureMap(0, 5, {ExposureCell(0, 0)}, open).count.empty(), "empty grid -> empty");
        const ExposureMap noThreats = buildExposureMap(4, 4, {}, open);
        bool z = true;
        for (int v : noThreats.count) z = z && (v == 0);
        CHECK(z && noThreats.isCovered(1, 1), "no threats -> everything covered");
    }

    if (g_fail == 0) {
        std::printf("exposure map: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
