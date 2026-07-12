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
#include <string>
#include <vector>

#include "zomboid/Sim.hpp"
#include "zomboid/render/SoftRenderer.hpp"

#include "Png.hpp"

namespace {

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? static_cast<size_t>(n) : 0);
    const size_t got = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t wrote = data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return wrote == data.size();
}

} // namespace

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
    std::string renderPath;    // if set, write a PNG frame after the run
    bool renderMap = false;    // whole-world overview instead of the game viewport
    int imgW = 960, imgH = 720;
    int tilePx = 16;
    std::string loadPath;      // restore a save before running
    std::string savePath;      // write a save after running
    std::string screen;        // title | pause | death — render a menu screen

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            ticks = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = static_cast<uint64_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--quiet") == 0) {
            quiet = true;
        } else if (std::strcmp(argv[i], "--render") == 0 && i + 1 < argc) {
            renderPath = argv[++i];
        } else if (std::strcmp(argv[i], "--map") == 0) {
            renderMap = true;
        } else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            imgW = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            imgH = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--tile") == 0 && i + 1 < argc) {
            tilePx = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--load") == 0 && i + 1 < argc) {
            loadPath = argv[++i];
        } else if (std::strcmp(argv[i], "--save") == 0 && i + 1 < argc) {
            savePath = argv[++i];
        } else if (std::strcmp(argv[i], "--screen") == 0 && i + 1 < argc) {
            screen = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("usage: zomboid [--ticks N] [--seed S] [--quiet]\n"
                        "               [--load in.sav] [--save out.sav]\n"
                        "               [--render out.png [--map] [--width W] [--height H] "
                        "[--tile PX]]\n");
            return 0;
        }
    }

    zb::Sim sim(seed);
    sim.newGame();

    if (!loadPath.empty()) {
        std::vector<uint8_t> blob;
        if (!readFile(loadPath, blob) || !sim.loadState(blob)) {
            std::printf("ERROR: could not load save %s\n", loadPath.c_str());
            return 1;
        }
        if (!quiet) std::printf("loaded save %s\n", loadPath.c_str());
    }

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

    if (!savePath.empty()) {
        if (writeFile(savePath, sim.saveState())) {
            std::printf("saved -> %s\n", savePath.c_str());
        } else {
            std::printf("ERROR: could not write save %s\n", savePath.c_str());
            return 1;
        }
    }

    if (!renderPath.empty()) {
        zb::Framebuffer fb(imgW, imgH);
        if (screen == "title") {
            zb::renderTitleScreen(fb, 0.4f); // phase where PRESS START is lit
        } else if (screen == "pause") {
            zb::RenderOptions opts;
            opts.tilePx = tilePx;
            zb::renderScene(sim, fb, opts);
            zb::renderPauseScreen(fb);
        } else if (screen == "death") {
            zb::RenderOptions opts;
            opts.tilePx = tilePx;
            zb::renderScene(sim, fb, opts);
            zb::renderDeathScreen(fb, sim.day(), sim.kills());
        } else if (renderMap) {
            zb::renderWorldMap(sim, fb);
        } else {
            zb::RenderOptions opts;
            opts.tilePx = tilePx;
            zb::renderScene(sim, fb, opts);
        }
        if (zbpng::write(renderPath, imgW, imgH, fb.pixels())) {
            std::printf("wrote %dx%d frame -> %s\n", imgW, imgH, renderPath.c_str());
        } else {
            std::printf("ERROR: could not write %s\n", renderPath.c_str());
            return 1;
        }
    }
    return 0;
}
