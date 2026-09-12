// tests/game/fogofwar.cpp — verifies persistent tile visibility memory (game::FogOfWar).
// Ground truths: a fresh map is entirely Unseen; reveal() makes a tile Visible (and thus Explored); a new
// frame demotes last frame's Visible tiles to Explored (memory kept) while Unseen stays Unseen; a tile seen
// once and never again stays Explored forever (never reverts to Unseen); re-revealing lifts Explored back to
// Visible; revealCircle marks a disc; out-of-bounds is safe; exploreAll fills memory without going live;
// reset clears; and the visible/explored counts track the state. Checked against the three-state model.
#include "maz/game/FogOfWar.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::game::FogOfWar;
using maz::game::Visibility;

int main() {
    // --- 1. Fresh map is entirely Unseen. ---
    {
        FogOfWar fog(8, 6);
        CHECK(fog.width() == 8 && fog.height() == 6, "dimensions set");
        CHECK(fog.visibility(3, 3) == Visibility::Unseen, "fresh tile is Unseen");
        CHECK(!fog.isExplored(3, 3) && !fog.isVisible(3, 3), "fresh tile is neither explored nor visible");
        CHECK(fog.visibleCount() == 0 && fog.exploredCount() == 0, "nothing seen yet");
    }

    // --- 2. reveal makes a tile Visible + Explored. ---
    {
        FogOfWar fog(8, 6);
        fog.reveal(2, 2);
        CHECK(fog.visibility(2, 2) == Visibility::Visible, "revealed tile is Visible");
        CHECK(fog.isVisible(2, 2) && fog.isExplored(2, 2), "visible implies explored");
        CHECK(fog.visibleCount() == 1 && fog.exploredCount() == 1, "one visible, one explored");
    }

    // --- 3. beginFrame demotes Visible -> Explored; Unseen stays Unseen. ---
    {
        FogOfWar fog(8, 6);
        fog.reveal(2, 2);
        fog.beginFrame();
        CHECK(fog.visibility(2, 2) == Visibility::Explored, "last frame's visible tile becomes Explored");
        CHECK(fog.isExplored(2, 2) && !fog.isVisible(2, 2), "explored but no longer visible");
        CHECK(fog.visibility(5, 5) == Visibility::Unseen, "never-seen tile stays Unseen");
        CHECK(fog.visibleCount() == 0 && fog.exploredCount() == 1, "0 visible, 1 remembered");
    }

    // --- 4. A tile seen once, never again, stays Explored across many frames. ---
    {
        FogOfWar fog(8, 6);
        fog.reveal(2, 2);
        for (int f = 0; f < 10; ++f) {
            fog.beginFrame(); // never re-revealed
        }
        CHECK(fog.visibility(2, 2) == Visibility::Explored, "memory persists — never reverts to Unseen");
    }

    // --- 5. Re-revealing lifts an Explored tile back to Visible. ---
    {
        FogOfWar fog(8, 6);
        fog.reveal(2, 2);
        fog.beginFrame();
        fog.reveal(2, 2);
        CHECK(fog.visibility(2, 2) == Visibility::Visible, "re-revealed tile is Visible again");
    }

    // --- 6. revealCircle marks a disc; corners outside the radius are excluded. ---
    {
        FogOfWar fog(11, 11);
        fog.revealCircle(5, 5, 2);
        CHECK(fog.isVisible(5, 5) && fog.isVisible(5, 3) && fog.isVisible(7, 5), "centre and cardinal edges visible");
        CHECK(!fog.isVisible(7, 7), "diagonal corner (dist √8 > 2) is outside the disc");
        CHECK(fog.visibleCount() == 13, "radius-2 disc has 13 tiles");
    }

    // --- 7. Out-of-bounds is safe; exploreAll and reset. ---
    {
        FogOfWar fog(4, 4);
        fog.reveal(-1, 0);
        fog.reveal(0, 99);
        CHECK(fog.visibleCount() == 0, "out-of-bounds reveal ignored");
        CHECK(fog.visibility(99, 99) == Visibility::Unseen, "out-of-bounds query returns Unseen");

        fog.exploreAll();
        CHECK(fog.exploredCount() == 16 && fog.visibleCount() == 0, "exploreAll fills memory, none live");
        CHECK(fog.visibility(1, 1) == Visibility::Explored, "explored, not visible, after exploreAll");

        fog.reset();
        CHECK(fog.exploredCount() == 0, "reset clears all memory");
    }

    if (g_fail == 0) {
        std::printf("fogofwar: OK — Unseen/Explored/Visible states, frame demote, persistent memory, disc, reset.\n");
        return 0;
    }
    std::printf("fogofwar: %d failure(s).\n", g_fail);
    return 1;
}
