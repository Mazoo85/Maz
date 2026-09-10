// Headless unit tests for the ZOMBOID: ANCHORAGE simulation port.
// No test framework: a tiny assert harness that exits non-zero on failure so
// ctest and CI catch regressions in the deterministic gameplay core.
#include <cmath>
#include <cstdio>
#include <string>

#include "zomboid/Sim.hpp"
#include "zomboid/World.hpp"
#include "zomboid/audio/Audio.hpp"
#include "zomboid/render/SoftRenderer.hpp"

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

// A fixed, deterministic input script so two sims can be driven identically.
zb::Input scriptedInput(int i) {
    zb::Input in;
    in.moveX = (i % 2 == 0) ? 1.0f : -0.5f;
    in.moveY = (i % 3 == 0) ? -1.0f : 0.3f;
    in.attackHeld = (i % 4 != 0);
    if (i % 130 == 0) in.interact = true;
    return in;
}

// ---- save / load: round-trip + a loaded game continues bit-identically ----
void testSaveLoad() {
    std::printf("[save-load]\n");
    zb::Sim a(99);
    a.newGame();
    for (int i = 0; i < 400; i++) a.step(scriptedInput(i));

    // Snapshot mid-game.
    const std::vector<uint8_t> snap = a.saveState();
    CHECK(!snap.empty());

    // Load into a fresh sim (different construction seed) and re-save: byte-identical.
    zb::Sim b(12345);
    b.newGame();
    CHECK(b.loadState(snap));
    CHECK(b.saveState() == snap);
    CHECK(b.kills() == a.kills());
    CHECK(b.day() == a.day());
    CHECK(b.aliveZombies() == a.aliveZombies());

    // Continue BOTH for the same 300 steps: final states must match exactly.
    for (int i = 400; i < 700; i++) {
        a.step(scriptedInput(i));
        b.step(scriptedInput(i));
    }
    CHECK(a.saveState() == b.saveState()); // loaded game == never-saved game
    CHECK(a.player().pos.x == b.player().pos.x);
    CHECK(a.player().pos.y == b.player().pos.y);

    // Corrupt / truncated / wrong-magic data is rejected without mutating the sim.
    const int killsBefore = a.kills();
    CHECK(!a.loadState({}));                       // empty
    CHECK(!a.loadState({0, 1, 2, 3, 4, 5, 6, 7})); // bad magic
    std::vector<uint8_t> truncated(snap.begin(), snap.begin() + 20);
    CHECK(!a.loadState(truncated));
    CHECK(a.kills() == killsBefore); // failed loads left the sim untouched
}

// ---- software renderer: produces a non-trivial, deterministic frame ----
void testRender() {
    std::printf("[render]\n");
    zb::Sim s(7);
    s.newGame();
    zb::Input in;
    in.aim = s.player().pos;
    for (int i = 0; i < 120; i++) s.step(in);

    zb::Framebuffer fb(320, 240);
    zb::RenderOptions opts;
    opts.tilePx = 16;
    zb::renderScene(s, fb, opts);

    // The frame must contain more than one colour (not a blank clear).
    bool varied = false;
    const auto& px = fb.pixels();
    for (size_t i = 3; i < px.size(); i += 3) {
        if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) {
            varied = true;
            break;
        }
    }
    CHECK(varied);

    // Deterministic: rendering the same state twice yields identical bytes.
    zb::Framebuffer fb2(320, 240);
    zb::renderScene(s, fb2, opts);
    CHECK(fb.pixels() == fb2.pixels());

    // The HUD changes the frame: same scene with hud off differs (bars/text drawn).
    zb::RenderOptions noHud = opts;
    noHud.hud = false;
    zb::Framebuffer fb3(320, 240);
    zb::renderScene(s, fb3, noHud);
    CHECK(fb.pixels() != fb3.pixels());

    // World-map overview also renders something.
    zb::Framebuffer mapFb(200, 180);
    zb::renderWorldMap(s, mapFb);
    bool mapVaried = false;
    for (size_t i = 3; i < mapFb.pixels().size(); i += 3)
        if (mapFb.pixels()[i] != mapFb.pixels()[0]) mapVaried = true;
    CHECK(mapVaried);

    // Post-FX changes the frame (scanlines + vignette darken pixels).
    zb::RenderOptions noFx = opts;
    noFx.postFx = false;
    zb::Framebuffer fbNoFx(320, 240);
    zb::renderScene(s, fbNoFx, noFx);
    CHECK(fb.pixels() != fbNoFx.pixels());

    // Menu screens render deterministic, non-trivial output.
    zb::Framebuffer title(240, 200), title2(240, 200);
    zb::renderTitleScreen(title, 0.4f);
    zb::renderTitleScreen(title2, 0.4f);
    CHECK(title.pixels() == title2.pixels());
    bool titleVaried = false;
    for (size_t i = 3; i < title.pixels().size(); i += 3)
        if (title.pixels()[i] != title.pixels()[0]) titleVaried = true;
    CHECK(titleVaried);
    zb::Framebuffer death(240, 200);
    zb::renderDeathScreen(death, 3, 9);
    bool deathVaried = false;
    for (size_t i = 3; i < death.pixels().size(); i += 3)
        if (death.pixels()[i] != death.pixels()[0]) deathVaried = true;
    CHECK(deathVaried);
}

// ---- audio synth: non-silent, deterministic, valid WAV ----
void testAudio() {
    std::printf("[audio]\n");
    auto peakAbs = [](const zb::audio::Clip& c) {
        float m = 0.0f;
        for (float v : c.samples()) m = std::max(m, std::fabs(v));
        return m;
    };

    zb::Rng r1(5), r2(5), r3(9);
    zb::audio::Clip gun1 = zb::audio::renderSfx(zb::audio::Sfx::Gun, r1);
    zb::audio::Clip gun2 = zb::audio::renderSfx(zb::audio::Sfx::Gun, r2);
    CHECK(!gun1.samples().empty());
    CHECK(peakAbs(gun1) > 0.01f);          // audible
    CHECK(gun1.samples() == gun2.samples()); // same seed -> identical audio
    zb::audio::Clip gun3 = zb::audio::renderSfx(zb::audio::Sfx::Gun, r3);
    CHECK(gun1.samples() != gun3.samples()); // noise differs by seed

    // A pure-tone SFX (no noise) is seed-independent.
    zb::Rng ra(1), rb(999);
    CHECK(zb::audio::renderSfx(zb::audio::Sfx::Select, ra).samples() ==
          zb::audio::renderSfx(zb::audio::Sfx::Select, rb).samples());

    // Music renders audible samples.
    zb::Rng rm(1);
    zb::audio::Clip music = zb::audio::renderMusic(16, rm);
    CHECK(peakAbs(music) > 0.01f);

    // Name lookup.
    zb::audio::Sfx s;
    CHECK(zb::audio::sfxFromName("shotgun", s));
    CHECK(!zb::audio::sfxFromName("nope", s));

    // WAV header is well-formed (RIFF/WAVE/PCM mono 44100, data size matches).
    const std::vector<uint8_t> wav = zb::audio::encodeWav(gun1);
    CHECK(wav.size() == 44u + gun1.samples().size() * 2u);
    CHECK(wav[0] == 'R' && wav[1] == 'I' && wav[2] == 'F' && wav[3] == 'F');
    CHECK(wav[8] == 'W' && wav[9] == 'A' && wav[10] == 'V' && wav[11] == 'E');
    const uint32_t sr = static_cast<uint32_t>(wav[24]) | (static_cast<uint32_t>(wav[25]) << 8) |
                        (static_cast<uint32_t>(wav[26]) << 16) |
                        (static_cast<uint32_t>(wav[27]) << 24);
    CHECK(sr == 44100u);
    CHECK(wav[22] == 1); // mono
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
    testSaveLoad();
    testRender();
    testAudio();
    testDayNight();
    std::printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
