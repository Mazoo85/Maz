// ZOMBOID: ANCHORAGE — headless native driver.
//
// Runs the ported deterministic simulation with a simple built-in "autopilot"
// (aim at the nearest zombie, back away, swing) so the gameplay core can be
// exercised without a window, GPU, or audio. This is the native Maz port's
// proof-of-life; a windowed renderer bridges onto the same Sim later.
//
//   zomboid --ticks 3600 --seed 7      # run 60 game-seconds, print a summary
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#include "zomboid/Sim.hpp"

namespace {

// Pick the nearest living zombie so the autopilot has something to aim at.
const zb::Zombie* nearestZombie(const zb::Sim& sim) {
    const zb::Zombie* best = nullptr;
    float bd = std::numeric_limits<float>::max();
    for (const auto& z : sim.zombies()) {
        if (z.dead) continue;
        const float d = zb::dist(z.pos, sim.player().pos);
        if (d < bd) {
            bd = d;
            best = &z;
        }
    }
    return best;
}

} // namespace

int main(int argc, char** argv) {
    int ticks = 1800; // 30 game-seconds at 60 Hz
    uint64_t seed = 1;
    bool quiet = false;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            ticks = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint64_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("usage: zomboid [--ticks N] [--seed S] [--quiet]\n");
            return 0;
        }
    }

    zb::Sim sim(seed);
    sim.newGame();

    if (!quiet) {
        std::printf("ZOMBOID: ANCHORAGE — headless sim (seed=%llu)\n",
                    static_cast<unsigned long long>(seed));
        std::printf("world %dx%d  containers=%zu  zombies=%d\n", sim.world().w, sim.world().h,
                    sim.world().containers.size(), sim.aliveZombies());
    }

    int meleeSwings = 0;
    for (int t = 0; t < ticks && !sim.player().dead; t++) {
        zb::Input in;
        const zb::Zombie* z = nearestZombie(sim);
        if (z) {
            in.aim = z->pos;
            const float d = zb::dist(z->pos, sim.player().pos);
            // Back away from very close threats, otherwise hold ground and swing.
            if (d < 1.2f) {
                in.moveX = sim.player().pos.x - z->pos.x;
                in.moveY = sim.player().pos.y - z->pos.y;
            }
            if (d < 1.8f) {
                in.attackHeld = true;
                meleeSwings++;
            }
        }
        // Periodically try to loot whatever is underfoot.
        if (t % 120 == 0) in.interact = true;
        sim.step(in);
    }

    const zb::Player& p = sim.player();
    std::printf("--- after %d ticks (%.1f game-min) ---\n", ticks,
                static_cast<double>(ticks) * static_cast<double>(zb::kStep) *
                    static_cast<double>(zb::kTimeScale));
    std::printf("day %d  %02d:%02d  %s\n", sim.day(), static_cast<int>(sim.dayTime()) / 60,
                static_cast<int>(sim.dayTime()) % 60, sim.isNight() ? "night" : "day");
    std::printf("health=%.1f fed=%.0f hydro=%.0f energy=%.0f mood=%.0f%s\n", p.health,
                100.0f - p.hunger, 100.0f - p.thirst, 100.0f - p.fatigue, p.mood,
                p.infected ? "  [INFECTED]" : "");
    std::printf("kills=%d  swings=%d  z_alive=%d  inv=%zu/%d  weapon=%s%s\n", sim.kills(),
                meleeSwings, sim.aliveZombies(), p.inv.size(), p.slots, p.weapon.c_str(),
                p.dead ? "  *** DEAD ***" : "");
    return 0;
}
