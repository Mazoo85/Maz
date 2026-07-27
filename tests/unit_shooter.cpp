// Unit tests for the DEAD SECTOR gameplay simulation (shooter::Sim) — pure logic, no GPU/display.
// Drives the sim with scripted input and asserts the wave/combat/pickup rules hold. Deterministic
// via the seeded RNG, so these checks are reproducible.

#include "Sim.hpp"

#include <cmath>
#include <cstdio>

using namespace shooter;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) ++g_failures;
}

// Advance the sim `steps` fixed steps with a constant input.
void run(Sim& sim, int steps, const Input& in) {
    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < steps; ++i) sim.update(dt, in);
}

} // namespace

int main() {
    const float dt = 1.0f / 60.0f;

    // --- Fresh state ---
    {
        Sim sim(1u);
        check(sim.state() == GameState::Playing, "starts playing");
        check(sim.wave() == 1, "starts on wave 1");
        check(sim.playerHp() == sim.playerMaxHp(), "starts at full health");
        check(sim.enemiesRemaining() > 0, "wave 1 has enemies to fight");
        check(sim.kills() == 0 && sim.score() == 0, "starts with zero kills/score");
    }

    // --- Zombies actually spawn and home in on the player ---
    {
        Sim sim(7u);
        Input idle;              // no movement, no firing
        idle.aiming = false;
        run(sim, 120, idle);     // 2 seconds
        check(!sim.zombies().empty(), "zombies spawn over time");
        // Record distance to player, step, and confirm at least one got closer.
        const Vec2 p = sim.playerPos();
        float before = 1e18f;
        for (const auto& z : sim.zombies()) {
            const float d = (z.pos.x - p.x) * (z.pos.x - p.x) + (z.pos.y - p.y) * (z.pos.y - p.y);
            before = std::fmin(before, d);
        }
        run(sim, 60, idle);
        const Vec2 p2 = sim.playerPos();
        float after = 1e18f;
        for (const auto& z : sim.zombies()) {
            const float d = (z.pos.x - p2.x) * (z.pos.x - p2.x) + (z.pos.y - p2.y) * (z.pos.y - p2.y);
            after = std::fmin(after, d);
        }
        check(after < before, "nearest zombie closes on the player");
    }

    // --- Firing spawns bullets and aim sets the facing angle ---
    {
        Sim sim(3u);
        Input in;
        in.aim = {1.0f, 0.0f};   // aim +X
        in.aiming = true;
        in.firing = true;
        sim.update(dt, in);
        check(!sim.bullets().empty(), "holding fire spawns bullets");
        check(std::fabs(sim.playerAngle()) < 0.01f, "aim +X -> angle ~0");
        const Vec2 b0 = sim.bullets().front().pos;
        sim.update(dt, in);
        // The first bullet should have travelled in +X.
        bool moved = false;
        for (const auto& b : sim.bullets()) {
            if (b.pos.x > b0.x + 1.0f) moved = true;
        }
        check(moved, "bullets travel along the aim direction");
    }

    // --- A stationary player surrounded by zombies eventually dies; restart recovers ---
    {
        Sim sim(9u);
        Input idle;
        idle.aiming = false;     // never shoot back
        int steps = 0;
        while (sim.state() == GameState::Playing && steps < 60 * 60) { // cap 60s
            sim.update(dt, idle);
            ++steps;
        }
        check(sim.state() == GameState::Dead, "unarmed player is overrun and dies");
        check(sim.playerHp() <= 0.0f, "death happens at zero health");

        Input restart;
        restart.restart = true;
        sim.update(dt, restart);
        check(sim.state() == GameState::Playing, "restart returns to playing");
        check(sim.wave() == 1 && sim.kills() == 0, "restart resets wave and kills");
        check(sim.playerHp() == sim.playerMaxHp(), "restart restores full health");
    }

    // --- Shooting a zombie damages it; enough shots kill it and raise the score ---
    {
        Sim sim(5u);
        Input wait;
        wait.aiming = false;
        // Let a zombie appear.
        int guard = 0;
        while (sim.zombies().empty() && guard < 600) { sim.update(dt, wait); ++guard; }
        check(!sim.zombies().empty(), "a zombie is present to shoot");
        const int startKills = sim.kills();
        // Aim at the nearest zombie and fire until the kill count rises.
        int fired = 0;
        while (sim.kills() == startKills && fired < 60 * 20) {
            Input in;
            const Vec2 p = sim.playerPos();
            Vec2 aim{1.0f, 0.0f};
            float best = 1e18f;
            for (const auto& z : sim.zombies()) {
                const float d = (z.pos.x - p.x) * (z.pos.x - p.x) + (z.pos.y - p.y) * (z.pos.y - p.y);
                if (d < best) { best = d; aim = {z.pos.x - p.x, z.pos.y - p.y}; }
            }
            in.aim = aim; in.aiming = true; in.firing = true;
            sim.update(dt, in);
            ++fired;
        }
        check(sim.kills() > startKills, "sustained fire kills a zombie");
        check(sim.score() > 0, "kills award score");
    }

    std::printf("unit_shooter: %s (%d failures)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
