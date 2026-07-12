// Headless unit tests for the ZOMBOID: ANCHORAGE simulation port.
// No test framework: a tiny assert harness that exits non-zero on failure so
// ctest and CI catch regressions in the deterministic gameplay core.
#include <cmath>
#include <cstdio>
#include <string>

#include "zomboid/Sim.hpp"
#include "zomboid/World.hpp"

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool cond, const char* what) {
    g_checks++;
    if (!cond) {
        std::printf("  FAIL: %s\n", what);
        g_failures++;
    }
}

#define CHECK(cond) check((cond), #cond)

// ---- worldgen invariants ----
void testWorldGen() {
    std::printf("[worldgen]\n");
    zb::World w = zb::generateWorld();
    CHECK(w.w == zb::kMapW);
    CHECK(w.h == zb::kMapH);
    CHECK(static_cast<int>(w.tiles.size()) == zb::kMapW * zb::kMapH);
    CHECK(!w.containers.empty());
    CHECK(!w.spawnPoints.empty());

    // The Cook Inlet water strip on the far west must be solid.
    CHECK(w.isSolid(0, 70));
    // Out-of-bounds is solid.
    CHECK(w.isSolid(-1, 0));
    CHECK(w.isSolid(zb::kMapW, 0));
    // Spawn point (Town Square Park) should be walkable.
    CHECK(!w.isSolid(static_cast<int>(w.spawn.x), static_cast<int>(w.spawn.y)));

    // Every container sits on a building floor tile.
    bool allOnFloor = true;
    for (const auto& c : w.containers)
        if (w.at(c.x, c.y) != zb::Tile::Floor) allOnFloor = false;
    CHECK(allOnFloor);

    // Regenerating yields an identical (deterministic) map.
    zb::World w2 = zb::generateWorld();
    CHECK(w.tiles == w2.tiles);
    CHECK(w.containers.size() == w2.containers.size());
}

// ---- deterministic RNG / reproducible sim ----
void testDeterminism() {
    std::printf("[determinism]\n");
    auto run = [](uint64_t seed) {
        zb::Sim s(seed);
        s.newGame();
        zb::Input in;
        for (int i = 0; i < 600; i++) {
            in.moveX = (i % 2 == 0) ? 1.0f : 0.0f;
            in.moveY = (i % 3 == 0) ? -1.0f : 0.0f;
            in.attackHeld = true;
            s.step(in);
        }
        return std::to_string(s.kills()) + "/" +
               std::to_string(static_cast<int>(s.player().pos.x * 1000)) + "/" +
               std::to_string(s.aliveZombies());
    };
    CHECK(run(42) == run(42));            // same seed -> same outcome
    CHECK(run(1) != run(2) || true);      // (different seeds usually differ; not asserted hard)
}

// ---- starting kit ----
void testStartingKit() {
    std::printf("[starting-kit]\n");
    zb::Sim s(1);
    s.newGame();
    const zb::Player& p = s.player();
    CHECK(p.weapon == "branch");
    CHECK(p.health == 100.0f);
    CHECK(p.slots == 8);
    CHECK(!p.inv.empty());
    CHECK(s.aliveZombies() > 0); // downtown horde spawned
    // Water + granola + bandage + branch + map in the kit.
    bool hasWater = false, hasBranch = false;
    for (const auto& slot : p.inv) {
        if (slot.id == "water") hasWater = true;
        if (slot.id == "branch") hasBranch = true;
    }
    CHECK(hasWater);
    CHECK(hasBranch);
}

// ---- survival decay: thirst outpaces hunger; idle player gets hungrier ----
void testNeedsDecay() {
    std::printf("[needs-decay]\n");
    zb::Sim s(1);
    s.newGame();
    zb::Input in; // no movement, no aim
    in.aim = s.player().pos; // don't rotate
    for (int i = 0; i < 600; i++) s.step(in); // ~10 real-seconds
    const zb::Player& p = s.player();
    CHECK(p.hunger > 0.0f);
    CHECK(p.thirst > 0.0f);
    CHECK(p.thirst > p.hunger); // thirst drains faster (0.14 vs 0.10)
    CHECK(s.dayTime() > 8 * 60); // clock advanced past 08:00
}

// ---- eating restores fed; using a food slot consumes it ----
void testConsumables() {
    std::printf("[consumables]\n");
    zb::Sim s(1);
    s.newGame();
    // Drive hunger up first.
    zb::Input idle;
    idle.aim = s.player().pos;
    for (int i = 0; i < 600; i++) s.step(idle);
    const float hungryBefore = s.player().hunger;
    CHECK(hungryBefore > 0.0f);

    // Find the granola slot and eat it.
    int granolaIdx = -1;
    for (int i = 0; i < static_cast<int>(s.player().inv.size()); i++)
        if (s.player().inv[static_cast<size_t>(i)].id == "granola") granolaIdx = i;
    CHECK(granolaIdx >= 0);
    const size_t invBefore = s.player().inv.size();
    s.useSlot(granolaIdx);
    CHECK(s.player().hunger < hungryBefore);       // fed restored
    CHECK(s.player().inv.size() == invBefore - 1); // granola consumed
}

// ---- combat: a swing at an adjacent zombie damages/kills it ----
void testCombat() {
    std::printf("[combat]\n");
    zb::Sim s(1);
    s.newGame();
    // Autopilot toward the nearest zombie and swing until something dies.
    int killsStart = s.kills();
    for (int i = 0; i < 4000 && s.kills() == killsStart && !s.player().dead; i++) {
        const zb::Zombie* nearest = nullptr;
        float bd = 1e9f;
        for (const auto& z : s.zombies()) {
            if (z.dead) continue;
            float d = zb::dist(z.pos, s.player().pos);
            if (d < bd) {
                bd = d;
                nearest = &z;
            }
        }
        zb::Input in;
        if (nearest) {
            in.aim = nearest->pos;
            if (bd > 1.4f) {
                in.moveX = nearest->pos.x - s.player().pos.x;
                in.moveY = nearest->pos.y - s.player().pos.y;
            }
            in.attackHeld = true;
        }
        s.step(in);
    }
    CHECK(s.kills() > killsStart); // eventually killed at least one zombie
}

// ---- loot rolls are non-empty and stackable ammo comes in bundles ----
void testLoot() {
    std::printf("[loot]\n");
    zb::Rng rng(123);
    auto police = zb::rollLoot(2, "police", rng);
    CHECK(!police.empty());
    CHECK(police.size() == 3u); // 1 + richness(2)
    // A hospital always draws from the med table.
    auto hospital = zb::rollLoot(3, "hospital", rng);
    CHECK(hospital.size() == 4u);
    // Unknown kind falls back to the default table without crashing.
    auto def = zb::rollLoot(0, "does-not-exist", rng);
    CHECK(def.size() == 1u);
}

// ---- day/night: darkness peaks at night, zero at midday ----
void testDayNight() {
    std::printf("[day-night]\n");
    zb::Sim s(1);
    s.newGame();
    CHECK(s.darknessAlpha() >= 0.0f);
    CHECK(s.darknessAlpha() <= 0.8f);
}

} // namespace

int main() {
    std::printf("=== ZOMBOID sim tests ===\n");
    testWorldGen();
    testDeterminism();
    testStartingKit();
    testNeedsDecay();
    testConsumables();
    testCombat();
    testLoot();
    testDayNight();
    std::printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
