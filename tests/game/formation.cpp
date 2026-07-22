// tests/game/formation.cpp — verifies squad formations (game::formationSlots / formationPositions).
// Ground truths: counts honoured and non-positive counts empty; a Line is centred and abreast with the right
// gap; a Column is single-file receding behind; a Wedge tips at the leader and is left/right symmetric; a Box
// is a centred grid; a Circle is an even ring of the right radius; world placement rotates the local layout by
// the facing (an isometry — distances from the anchor are preserved) and translates to the anchor; a zero
// facing defaults to forward; spacing scales linearly. Checked against the layout definitions. Pure CPU.
#include "maz/game/Formation.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::game;
using maz::math::vec2;

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }
static float len(const vec2& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

int main() {
    // --- 1. Counts and degenerate inputs. ---
    {
        CHECK(formationSlots(FormationShape::Line, 0, 1.0f).empty(), "count 0 -> empty");
        CHECK(formationSlots(FormationShape::Box, -2, 1.0f).empty(), "negative count -> empty");
        const auto one = formationSlots(FormationShape::Wedge, 1, 2.0f);
        CHECK(one.size() == 1 && near(one[0].x, 0) && near(one[0].y, 0), "count 1 -> leader at origin");
    }

    // --- 2. Line: abreast, centred, correct gap. ---
    {
        const auto s = formationSlots(FormationShape::Line, 5, 2.0f);
        CHECK(s.size() == 5, "line has 5 slots");
        float sumx = 0;
        for (const vec2& p : s) { CHECK(near(p.y, 0), "line slot on the y=0 rank"); sumx += p.x; }
        CHECK(near(sumx, 0), "line is centred (offsets cancel)");
        CHECK(near(s[1].x - s[0].x, 2.0f), "line neighbour gap == spacing");
        CHECK(near(s[0].x, -4.0f) && near(s[4].x, 4.0f), "line spans -4..+4 for 5 @ 2");
    }

    // --- 3. Column: single file, receding, correct gap. ---
    {
        const auto s = formationSlots(FormationShape::Column, 4, 3.0f);
        for (const vec2& p : s) CHECK(near(p.x, 0), "column slot on the centreline");
        CHECK(near(s[0].y, 0) && near(s[1].y, -3.0f) && near(s[3].y, -9.0f), "column recedes by spacing");
    }

    // --- 4. Wedge: tip at leader, left/right symmetric. ---
    {
        const auto s = formationSlots(FormationShape::Wedge, 5, 2.0f);
        CHECK(near(s[0].x, 0) && near(s[0].y, 0), "wedge tip at the leader");
        // Slots 1 & 2 are the first rank: mirror images, one rank back.
        CHECK(near(s[1].x, -s[2].x) && near(s[1].y, s[2].y), "wedge first rank is mirrored");
        CHECK(near(s[1].y, -2.0f) && near(s[2].y, -2.0f), "wedge first rank one step back");
        CHECK(s[1].x < 0 && s[2].x > 0, "wedge fans left then right");
    }

    // --- 5. Box: centred grid. ---
    {
        const auto s = formationSlots(FormationShape::Box, 9, 2.0f); // 3x3
        CHECK(s.size() == 9, "box has 9 slots");
        float sumx = 0;
        for (const vec2& p : s) sumx += p.x;
        CHECK(near(sumx, 0), "box columns are centred");
        CHECK(near(s[0].y, 0) && near(s[3].y, -2.0f) && near(s[6].y, -4.0f), "box rows recede by spacing");
    }

    // --- 6. Circle: even ring of the right radius. ---
    {
        const int n = 8;
        const float spacing = 2.0f;
        const auto s = formationSlots(FormationShape::Circle, n, spacing);
        const float expectR = spacing * static_cast<float>(n) / (2.0f * 3.14159265358979f);
        for (const vec2& p : s) CHECK(near(len(p), expectR, 1e-3f), "circle slot at the ring radius");
        CHECK(near(s[0].x, expectR) && near(s[0].y, 0, 1e-3f), "circle slot 0 on +X");
    }

    // --- 7. World placement: facing up is identity; rotation is an isometry; +X facing rotates 90 degrees. ---
    {
        const vec2 anchor(10, 5);
        const auto local = formationSlots(FormationShape::Wedge, 5, 2.0f);
        const auto up = formationPositions(anchor, vec2(0, 1), FormationShape::Wedge, 5, 2.0f);
        for (std::size_t i = 0; i < local.size(); ++i)
            CHECK(near(up[i].x, anchor.x + local[i].x) && near(up[i].y, anchor.y + local[i].y),
                  "facing +Y == anchor + local");

        // Distances from the anchor are preserved for any facing (pure rotation + translation).
        const auto rot = formationPositions(anchor, vec2(1, 0), FormationShape::Wedge, 5, 2.0f);
        for (std::size_t i = 0; i < local.size(); ++i)
            CHECK(near(len(vec2(rot[i].x - anchor.x, rot[i].y - anchor.y)), len(local[i])),
                  "rotation preserves distance from the anchor");
        // Facing +X: local (x,0) maps to world offset (0,-x).
        const auto lineRot = formationPositions(anchor, vec2(1, 0), FormationShape::Line, 3, 2.0f);
        CHECK(near(lineRot[0].x, anchor.x) && near(lineRot[0].y, anchor.y + 2.0f),
              "line facing +X: first slot offset (0,-x) with x=-2 -> +2 in y");
    }

    // --- 8. Zero facing defaults to +Y; spacing scales. ---
    {
        const vec2 anchor(0, 0);
        const auto z = formationPositions(anchor, vec2(0, 0), FormationShape::Line, 3, 2.0f);
        const auto up = formationPositions(anchor, vec2(0, 1), FormationShape::Line, 3, 2.0f);
        for (std::size_t i = 0; i < z.size(); ++i)
            CHECK(near(z[i].x, up[i].x) && near(z[i].y, up[i].y), "zero facing defaults to +Y");

        const auto a = formationSlots(FormationShape::Line, 4, 1.0f);
        const auto b = formationSlots(FormationShape::Line, 4, 3.0f);
        CHECK(near(b[0].x, a[0].x * 3.0f), "spacing scales offsets linearly");
    }

    if (g_fail == 0) {
        std::printf("formation: OK — line/column/wedge/box/circle layouts + facing-rotated world placement.\n");
        return 0;
    }
    std::printf("formation: %d failure(s).\n", g_fail);
    return 1;
}
