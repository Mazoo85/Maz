// Unit tests for maz::ai — the 2D grid A* pathfinder (GridMap + findPath).
// Exercises a straight run, start==goal, blocked start/goal, an unreachable
// grid, a detour around a wall, an open diagonal (8-conn) run, corner-cutting
// refusal, and Manhattan optimality. Because equal-cost ties can yield different
// but equally optimal cell sequences, assertions target COST + endpoint +
// step-legality (via a local validPath), never a specific cell list. Pure C++,
// no GPU/display.

#include "maz/ai/AStarGrid.hpp"

#include <cstdio>
#include <vector>

using namespace maz::ai;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

// Validates that 'p' is a legal path from s to goal on map m: non-empty, correct
// endpoints, every cell walkable, every consecutive pair a single legal step
// (orthogonal, or diagonal only when diag AND the corner-cutting rule holds), and
// no immediate cell repeats.
bool validPath(const GridMap& m, const std::vector<GridCoord>& p, GridCoord s, GridCoord goal, bool diag) {
    if (p.empty()) { return false; }
    if (p.front() != s || p.back() != goal) { return false; }
    for (const GridCoord& c : p) {
        if (!m.walkable(c)) { return false; }
    }
    for (std::size_t i = 1; i < p.size(); ++i) {
        const GridCoord a = p[i - 1];
        const GridCoord b = p[i];
        const std::int32_t dx = b.x >= a.x ? b.x - a.x : a.x - b.x;
        const std::int32_t dy = b.y >= a.y ? b.y - a.y : a.y - b.y;
        if (dx == 0 && dy == 0) { return false; }             // immediate repeat
        if (dx > 1 || dy > 1) { return false; }               // not a single step
        const bool diagonal = dx == 1 && dy == 1;
        if (diagonal) {
            if (!diag) { return false; }                      // diagonal not allowed
            // Corner-cutting rule: both shared orthogonal cells must be walkable.
            if (!m.walkable(GridCoord{ b.x, a.y }) || !m.walkable(GridCoord{ a.x, b.y })) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

int main() {
    // --- STRAIGHT LINE (4-conn) ---------------------------------------------
    {
        GridMap m(5, 1);
        const GridCoord s{ 0, 0 };
        const GridCoord goal{ 4, 0 };
        const std::vector<GridCoord> p = findPath(m, s, goal, false);
        check(validPath(m, p, s, goal, false), "straight line is a valid path");
        check(pathCost(p, false) == 40, "straight line cost == 40");
        check(p.size() == 5, "straight line has 5 cells");
    }

    // --- START == GOAL -------------------------------------------------------
    {
        GridMap m(5, 5);
        const GridCoord s{ 2, 2 };
        const std::vector<GridCoord> p = findPath(m, s, s, false);
        check(p.size() == 1 && p.front() == s, "start==goal is a single-cell path");
        check(pathCost(p, false) == 0, "start==goal cost == 0");
    }

    // --- BLOCKED START OR GOAL ----------------------------------------------
    {
        GridMap m(5, 5);
        const GridCoord s{ 0, 0 };
        const GridCoord goal{ 4, 4 };
        m.setBlocked(goal, true);
        check(findPath(m, s, goal, false).empty(), "blocked goal -> empty path");

        GridMap m2(5, 5);
        m2.setBlocked(s, true);
        check(findPath(m2, s, goal, false).empty(), "blocked start -> empty path");
    }

    // --- UNREACHABLE (grid split by a full column) --------------------------
    {
        GridMap m(3, 3);
        m.setBlocked(GridCoord{ 1, 0 }, true);
        m.setBlocked(GridCoord{ 1, 1 }, true);
        m.setBlocked(GridCoord{ 1, 2 }, true);
        check(findPath(m, GridCoord{ 0, 0 }, GridCoord{ 2, 0 }, false).empty(),
              "fully walled column -> unreachable -> empty");
    }

    // --- DETOUR AROUND A WALL (4-conn) --------------------------------------
    {
        GridMap m(3, 3);
        m.setBlocked(GridCoord{ 1, 0 }, true);  // wall with a gap at (1,2)
        m.setBlocked(GridCoord{ 1, 1 }, true);
        const GridCoord s{ 0, 0 };
        const GridCoord goal{ 2, 0 };
        const std::vector<GridCoord> p = findPath(m, s, goal, false);
        // Optimal detour: (0,0)->(0,1)->(0,2)->(1,2)->(2,2)->(2,1)->(2,0) = 6*10.
        check(validPath(m, p, s, goal, false), "detour is a valid path");
        check(pathCost(p, false) == 60, "detour cost == 60");
    }

    // --- DIAGONAL OPEN (8-conn) ---------------------------------------------
    {
        GridMap m(4, 4);
        const GridCoord s{ 0, 0 };
        const GridCoord goal{ 3, 3 };
        const std::vector<GridCoord> p = findPath(m, s, goal, true);
        check(validPath(m, p, s, goal, true), "open diagonal is a valid path");
        check(pathCost(p, true) == 42, "open diagonal cost == 42 (3*14)");
        check(p.size() == 4, "open diagonal has 4 cells");
    }

    // --- CORNER-CUTTING FORBIDDEN (8-conn) ----------------------------------
    {
        GridMap m(2, 2);
        m.setBlocked(GridCoord{ 1, 0 }, true);
        m.setBlocked(GridCoord{ 0, 1 }, true);
        // The only diagonal (0,0)->(1,1) is a corner-cut between two blocked
        // cells, and no orthogonal route exists -> must be empty.
        check(findPath(m, GridCoord{ 0, 0 }, GridCoord{ 1, 1 }, true).empty(),
              "corner-cutting forbidden -> empty path");
    }

    // --- OPTIMALITY (4-conn Manhattan) --------------------------------------
    {
        GridMap m(6, 6);
        const GridCoord s{ 0, 0 };
        const GridCoord goal{ 3, 2 };
        const std::vector<GridCoord> p = findPath(m, s, goal, false);
        check(validPath(m, p, s, goal, false), "optimal open path is valid");
        check(pathCost(p, false) == 50, "optimal Manhattan cost == 50 ((3+2)*10)");
    }

    // --- EXPLICIT MULTI-CELL CONTIGUITY -------------------------------------
    {
        GridMap m(6, 6);
        const GridCoord s{ 1, 1 };
        const GridCoord goal{ 4, 3 };
        const std::vector<GridCoord> p = findPath(m, s, goal, false);
        check(p.size() > 1 && validPath(m, p, s, goal, false),
              "multi-cell open path is contiguous and valid");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
