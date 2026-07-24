// tests/zomboid/sim.cpp — headless verification of the ZOMBOID shooter's whole simulation.
// The game's rules live in apps/zomboid/game.hpp as a maz::script program on a scene::SceneTree; this
// standalone test drives that simulation with no GPU/window and asserts the shooter behaves. Kept as its
// own fast-compiling target (separate from the giant unit suite) so game iterations verify in seconds.
#include "game.hpp" // apps/zomboid — the flagship game's logic (on the include path via CMake)

#include "maz/scene/SceneTree.hpp"
#include "maz/core/KeyValueStore.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

using maz::scene::SceneNode;
using maz::scene::SceneTree;
using maz::script::Value;

static int g_fail = 0;
#define CHECK(c) do{ if(!(c)){ std::printf("FAIL: %s (line %d)\n", #c, __LINE__); ++g_fail; } }while(0)

static Value* sField(SceneNode* n, const char* f) { return n->script().instance->findField(f); }
static double glob(SceneTree& t, const char* n) {
    const Value* v = t.scripts().vm().getGlobal(n);
    return v ? v->number : 0.0;
}
static int aliveZombies(SceneTree& t) {
    int c = 0;
    for (SceneNode* z : t.nodesInGroup("zombies"))
        if (z->script().instance->findField("alive")->boolean) ++c;
    return c;
}
static int activeBullets(SceneTree& t) {
    int c = 0;
    for (SceneNode* b : t.nodesInGroup("bullets"))
        if (b->script().instance->findField("active")->boolean) ++c;
    return c;
}
static int activeParticles(SceneTree& t) {
    int c = 0;
    for (SceneNode* p : t.nodesInGroup("particles"))
        if (p->script().instance->findField("active")->boolean) ++c;
    return c;
}
static int activeSpits(SceneTree& t) {
    int c = 0;
    for (SceneNode* s : t.nodesInGroup("spits"))
        if (s->script().instance->findField("active")->boolean) ++c;
    return c;
}
static int activePowerups(SceneTree& t) {
    int c = 0;
    for (SceneNode* p : t.nodesInGroup("powerups"))
        if (p->script().instance->findField("active")->boolean) ++c;
    return c;
}
static void setWeapon(SceneTree& t, SceneNode* s, int w) {
    Value self = s->script();
    std::vector<Value> a = {Value::fromNum(static_cast<double>(w))};
    t.scripts().vm().callOn(self, "set_weapon", a);
}

int main() {
    // Scene builds: survivor, dormant bullet + zombie pools, loot.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        CHECK(survivor != nullptr);
        CHECK(tree.scripts().vm().error().empty());
        CHECK((int)tree.nodesInGroup("bullets").size() == zomboid::kBulletPool);
        CHECK((int)tree.nodesInGroup("zombies").size() == zomboid::kZombiePool);
        CHECK((int)tree.nodesInGroup("loot").size() == zomboid::kLootCount);
    }

    // Director opens wave 1 immediately and revives pooled zombies.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        CHECK(aliveZombies(tree) == 0);
        tree.process(0.016);
        CHECK((int)glob(tree, "g_wave") == 1);
        CHECK(aliveZombies(tree) > 0);
    }

    // Shooting: aiming + firing pulls a bullet from the pool (pistol default).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;
        CHECK(activeBullets(tree) == 0);
        sField(survivor, "firing")->boolean = true;
        tree.process(0.016);
        CHECK(activeBullets(tree) >= 1);
        CHECK(sField(survivor, "shots")->number >= 1.0);
    }

    // Pistol cadence: ~6 shots/second (not one per frame).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        sField(survivor, "firing")->boolean = true;
        for (int i = 0; i < 60; ++i) tree.process(1.0 / 60.0);
        const double shots = sField(survivor, "shots")->number;
        CHECK(shots >= 5.0 && shots <= 8.0);
    }

    // Shotgun fires a burst of pellets in one shot; SMG fires far faster than the pistol.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        setWeapon(tree, survivor, 1); // shotgun
        CHECK((int)sField(survivor, "pellets")->number == 6);
        sField(survivor, "firing")->boolean = true;
        tree.process(1.0 / 60.0);
        CHECK(activeBullets(tree) >= 5); // a spread of pellets went out on the first shot
    }
    {
        // SMG vs pistol shot count over 1s.
        SceneTree a, b;
        SceneNode* sa = zomboid::buildScene(a);
        SceneNode* sb = zomboid::buildScene(b);
        sField(sa, "firing")->boolean = true;           // pistol
        setWeapon(b, sb, 2);                             // smg
        sField(sb, "firing")->boolean = true;
        for (int i = 0; i < 60; ++i) { a.process(1.0 / 60.0); b.process(1.0 / 60.0); }
        CHECK(sField(sb, "shots")->number > sField(sa, "shots")->number);
    }

    // Railgun (weapon 3): a piercing hitscan beam damages an entire line of zombies in one shot,
    // while sparing bodies off the beam.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        setWeapon(tree, survivor, 3);
        CHECK((int)sField(survivor, "weapon")->number == 3);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;

        SceneNode* zs[3] = {tree.findNode("Zombie0"), tree.findNode("Zombie1"), tree.findNode("Zombie2")};
        const double xs[3] = {8.0, 14.0, 20.0};
        for (int i = 0; i < 3; ++i) {
            Value zv = zs[i]->script();
            std::vector<Value> a = {Value::fromNum(xs[i]), Value::fromNum(0.0),
                                    Value::fromNum(100.0), Value::fromNum(0.0)}; // spawn_at(x,y,hp,spd)
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        }
        SceneNode* off = tree.findNode("Zombie3"); // parked well off the beam
        Value offv = off->script();
        std::vector<Value> ao = {Value::fromNum(14.0), Value::fromNum(10.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};
        tree.scripts().vm().callOn(offv, "spawn_at", ao);

        Value sv = survivor->script();
        std::vector<Value> none;
        tree.scripts().vm().callOn(sv, "do_shoot", none); // one railgun shot

        for (int i = 0; i < 3; ++i)
            CHECK(zs[i]->script().instance->findField("health")->number <= 60.0); // whole line pierced
        CHECK(off->script().instance->findField("health")->number == 100.0);      // off-beam spared
        CHECK(activeBullets(tree) >= 1);                                           // visual tracer flew
    }

    // A bullet kills a zombie: park one live zombie in the line of fire and shoot it.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* z0 = tree.findNode("Zombie0");
        Value zs = z0->script();
        std::vector<Value> args = {Value::fromNum(4.0), Value::fromNum(0.0), Value::fromNum(25.0),
                                   Value::fromNum(0.0)};
        tree.scripts().vm().callOn(zs, "spawn_at", args);
        CHECK(z0->script().instance->findField("alive")->boolean);
        const double kills0 = glob(tree, "g_kills");
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;
        sField(survivor, "firing")->boolean = true;
        bool died = false;
        for (int i = 0; i < 120 && !died; ++i) {
            tree.process(1.0 / 60.0);
            died = !z0->script().instance->findField("alive")->boolean;
        }
        CHECK(died);
        CHECK(glob(tree, "g_kills") > kills0);
        CHECK(glob(tree, "g_score") > 0.0);
    }

    // A cleared wave escalates to a bigger wave 2.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        tree.process(0.016);
        CHECK((int)glob(tree, "g_wave") == 1);
        const int wave1 = aliveZombies(tree);
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (z->script().instance->findField("alive")->boolean) {
                Value zs = z->script();
                std::vector<Value> dmg = {Value::fromNum(9999.0)};
                tree.scripts().vm().callOn(zs, "take_damage", dmg);
            }
        }
        CHECK(aliveZombies(tree) == 0);
        for (int i = 0; i < 240; ++i) tree.process(1.0 / 60.0);
        CHECK((int)glob(tree, "g_wave") == 2);
        CHECK(aliveZombies(tree) > wave1);
    }

    // Survival pressure: standing in the horde costs health.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        for (int i = 0; i < 300; ++i) tree.process(0.05);
        CHECK(sField(survivor, "health")->number < 100.0);
    }

    // Eating restores hunger and consumes a ration.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(5000.0, 5000.0);
        for (int i = 0; i < 40; ++i) tree.process(1.0);
        const double before = sField(survivor, "hunger")->number;
        const double food0 = sField(survivor, "food")->number;
        CHECK(before > 0.0);
        Value self = survivor->script();
        std::vector<Value> none;
        tree.scripts().vm().callOn(self, "eat", none);
        CHECK(sField(survivor, "hunger")->number < before);
        CHECK(sField(survivor, "food")->number == food0 - 1);
    }

    // Loot pickup collects exactly once.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* loot0 = tree.findNode("Loot0");
        const double food0 = sField(survivor, "food")->number;
        CHECK(!loot0->script().instance->findField("taken")->boolean);
        survivor->setPosition(loot0->x(), loot0->y());
        tree.process(0.016);
        CHECK(loot0->script().instance->findField("taken")->boolean);
        CHECK(sField(survivor, "food")->number == food0 + 1);
        tree.process(0.016);
        CHECK(sField(survivor, "food")->number == food0 + 1);
    }

    // Enemy variety: a boss wave (5) spawns a boss plus a mix of kinds with distinct stats.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* dir = tree.findNode("Director");
        Value ds = dir->script();
        std::vector<Value> a = {Value::fromNum(5.0)};
        tree.scripts().vm().callOn(ds, "start_wave", a); // force wave 5 directly

        bool hasBoss = false, hasRunner = false, hasBrute = false, hasWalker = false;
        double bossHp = 0.0, runnerSpd = 0.0, walkerSpd = 0.0, bruteHp = 0.0, walkerHp = 0.0;
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (!z->script().instance->findField("alive")->boolean) continue;
            const int k = (int)z->script().instance->findField("kind")->number;
            const double hp = z->script().instance->findField("health")->number;
            const double sp = z->script().instance->findField("speed")->number;
            if (k == 3) { hasBoss = true; bossHp = hp; }
            else if (k == 2) { hasBrute = true; bruteHp = hp; }
            else if (k == 1) { hasRunner = true; runnerSpd = sp; }
            else { hasWalker = true; walkerSpd = sp; walkerHp = hp; }
        }
        CHECK(hasBoss);                          // the boss leads every 5th wave
        CHECK(hasRunner && hasBrute && hasWalker); // a genuine mix
        CHECK(bossHp > 300.0);                   // the boss is a bullet sponge
        CHECK(bruteHp > walkerHp);               // brutes are tankier than walkers
        CHECK(runnerSpd > walkerSpd);            // runners outrun walkers
    }

    // A brute soaks more damage than a walker before dying (per-kind health matters).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> brute = {Value::fromNum(3.0), Value::fromNum(0.0), Value::fromNum(2.0),
                                    Value::fromNum(1.0)}; // spawn(x,y,kind=2,wave=1)
        tree.scripts().vm().callOn(zs, "spawn", brute);
        const double hp0 = z->script().instance->findField("health")->number;
        CHECK(hp0 >= 90.0); // brute wave-1 hp = 80 + 20
        std::vector<Value> dmg = {Value::fromNum(25.0)}; // one pistol hit
        tree.scripts().vm().callOn(zs, "take_damage", dmg);
        CHECK(z->script().instance->findField("alive")->boolean); // survives a single shot
        CHECK(z->script().instance->findField("health")->number < hp0);
        (void)survivor;
    }

    // Juice: killing a zombie sprays particles and kicks the screen shake, which then settle.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        CHECK((int)tree.nodesInGroup("particles").size() == zomboid::kParticlePool);
        CHECK(activeParticles(tree) == 0);
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> sa = {Value::fromNum(3.0), Value::fromNum(0.0), Value::fromNum(20.0),
                                 Value::fromNum(0.0)};
        tree.scripts().vm().callOn(zs, "spawn_at", sa);
        std::vector<Value> dmg = {Value::fromNum(9999.0)}; // lethal
        tree.scripts().vm().callOn(zs, "take_damage", dmg);
        CHECK(activeParticles(tree) > 0);       // sparks + blood emitted
        CHECK(glob(tree, "g_shake") > 0.0);     // the hit shook the screen
        // Everything eases back to rest within a couple of seconds.
        for (int i = 0; i < 150; ++i) tree.process(1.0 / 60.0);
        CHECK(activeParticles(tree) == 0);
        CHECK(glob(tree, "g_shake") < 0.01);
    }

    // Ammo: firing drains the magazine, then an auto-reload refills it from reserve.
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        CHECK(sField(s, "cur_ammo")->number == 12.0);   // pistol magazine
        const double res0 = (*sField(s, "reserves")->array)[0].number; // 48 spare
        sField(s, "firing")->boolean = true;
        bool sawReload = false;
        for (int i = 0; i < 180; ++i) { // 3 s of held fire — drains the mag and starts a reload
            tree.process(1.0 / 60.0);
            if (sField(s, "is_reloading")->boolean) sawReload = true;
        }
        CHECK(sawReload);                                // an empty mag triggered a reload
        sField(s, "firing")->boolean = false;
        for (int i = 0; i < 200; ++i) tree.process(1.0 / 60.0); // let it finish
        CHECK(sField(s, "cur_ammo")->number == 12.0);    // magazine refilled
        CHECK((*sField(s, "reserves")->array)[0].number < res0); // reserve was consumed
    }

    // Dry weapon: with an empty magazine AND empty reserve, exactly the last round fires and no more.
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        (*sField(s, "mags")->array)[0] = Value::fromNum(1.0);     // one round chambered
        (*sField(s, "reserves")->array)[0] = Value::fromNum(0.0); // nothing to reload
        sField(s, "firing")->boolean = true;
        for (int i = 0; i < 120; ++i) tree.process(1.0 / 60.0);
        CHECK(sField(s, "shots")->number == 1.0);        // fired once, then dry
        CHECK(sField(s, "cur_ammo")->number == 0.0);
        CHECK(sField(s, "is_reloading")->boolean == false); // can't reload from an empty reserve
    }

    // Loot is an ammo crate: collecting it tops up the reserve.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* loot0 = tree.findNode("Loot0");
        const double res0 = (*sField(survivor, "reserves")->array)[0].number;
        survivor->setPosition(loot0->x(), loot0->y());
        tree.process(0.016);
        CHECK((*sField(survivor, "reserves")->array)[0].number > res0);
    }

    // Upgrades: each apply_upgrade cycles a distinct boost (+dmg, +rate, +max health, +ammo).
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        const double dmg0 = sField(s, "damage")->number;      // pistol 25
        const double rate0 = sField(s, "fire_rate")->number;  // pistol 6
        const double hp0 = sField(s, "max_health")->number;   // 100
        const double res0 = (*sField(s, "reserves")->array)[0].number;
        Value self = s->script();
        std::vector<Value> none;
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k0: +damage
        CHECK(sField(s, "damage")->number > dmg0);
        CHECK(sField(s, "dmg_mult")->number > 1.0);
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k1: +fire rate
        CHECK(sField(s, "fire_rate")->number > rate0);
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k2: +max health (heal)
        CHECK(sField(s, "max_health")->number > hp0);
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k3: +ammo
        CHECK((*sField(s, "reserves")->array)[0].number > res0);
        CHECK((int)sField(s, "upgrades")->number == 4);
        const double crit0 = sField(s, "crit_chance")->number;
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k4: +crit chance
        CHECK(sField(s, "crit_chance")->number > crit0);
        CHECK((int)sField(s, "upgrades")->number == 5);
    }

    // Critical hits: a shot rolls for bonus damage; forcing the odds proves both branches.
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        Value self = s->script();
        std::vector<Value> none;
        const double dmg = sField(s, "damage")->number;
        const double cm = sField(s, "crit_mult")->number;

        sField(s, "crit_chance")->number = 1.0;   // always crit
        CHECK(tree.scripts().vm().callOn(self, "shot_damage", none).number == dmg * cm);
        sField(s, "crit_chance")->number = 0.0;   // never crit
        CHECK(tree.scripts().vm().callOn(self, "shot_damage", none).number == dmg);
    }

    // Progression is wired to waves: clearing wave 1 grants the first upgrade when wave 2 opens.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        tree.process(0.016);
        CHECK((int)glob(tree, "g_wave") == 1);
        CHECK((int)sField(survivor, "upgrades")->number == 0); // no upgrade yet on wave 1
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (z->script().instance->findField("alive")->boolean) {
                Value zs = z->script();
                std::vector<Value> dmg = {Value::fromNum(9999.0)};
                tree.scripts().vm().callOn(zs, "take_damage", dmg);
            }
        }
        for (int i = 0; i < 240; ++i) tree.process(1.0 / 60.0);
        CHECK((int)glob(tree, "g_wave") == 2);
        CHECK((int)sField(survivor, "upgrades")->number == 1); // wave 2 handed out an upgrade
    }

    // Grenades: throwing spends one and arms a grenade; the blast kills a whole cluster of zombies.
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        CHECK((int)tree.nodesInGroup("grenades").size() == zomboid::kGrenadePool);
        CHECK(sField(s, "grenades")->number == 3.0);
        sField(s, "aim_x")->number = 1.0;
        sField(s, "aim_y")->number = 0.0;
        Value self = s->script();
        std::vector<Value> none;
        tree.scripts().vm().callOn(self, "throw_grenade", none);
        CHECK(sField(s, "grenades")->number == 2.0); // one spent
        int armed = 0;
        for (SceneNode* g : tree.nodesInGroup("grenades"))
            if (g->script().instance->findField("active")->boolean) ++armed;
        CHECK(armed == 1);

        // Detonate a grenade in the middle of three zombies -> all three die from the blast.
        SceneNode* g0 = tree.findNode("Grenade0");
        g0->setPosition(50.0, 0.0);
        const char* names[3] = {"Zombie0", "Zombie1", "Zombie2"};
        const double pos[3][2] = {{50.0, 0.0}, {51.5, 0.0}, {49.0, 1.0}};
        for (int i = 0; i < 3; ++i) {
            SceneNode* z = tree.findNode(names[i]);
            Value zs = z->script();
            std::vector<Value> sa = {Value::fromNum(pos[i][0]), Value::fromNum(pos[i][1]),
                                     Value::fromNum(30.0), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zs, "spawn_at", sa);
        }
        const double kills0 = glob(tree, "g_kills");
        Value gs = g0->script();
        tree.scripts().vm().callOn(gs, "explode", none);
        for (int i = 0; i < 3; ++i) {
            SceneNode* z = tree.findNode(names[i]);
            CHECK(!z->script().instance->findField("alive")->boolean); // caught in the blast
        }
        CHECK(glob(tree, "g_kills") >= kills0 + 3.0);
    }

    // Out of grenades: throwing does nothing.
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        sField(s, "grenades")->number = 0.0;
        Value self = s->script();
        std::vector<Value> none;
        tree.scripts().vm().callOn(self, "throw_grenade", none);
        int armed = 0;
        for (SceneNode* g : tree.nodesInGroup("grenades"))
            if (g->script().instance->findField("active")->boolean) ++armed;
        CHECK(armed == 0);
    }

    // Loot refills a grenade too.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* loot0 = tree.findNode("Loot0");
        const double g0 = sField(survivor, "grenades")->number;
        survivor->setPosition(loot0->x(), loot0->y());
        tree.process(0.016);
        CHECK(sField(survivor, "grenades")->number == g0 + 1.0);
    }

    // Combo: consecutive kills build a score multiplier; score follows the hand-computed total.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        CHECK(glob(tree, "g_score") == 0.0);
        // Kill 5 dormant walkers back-to-back (no ticks between, so the combo can't decay).
        for (int i = 0; i < 5; ++i) {
            SceneNode* z = tree.findNode("Zombie" + std::to_string(i));
            Value zs = z->script();
            std::vector<Value> sa = {Value::fromNum(60.0 + i * 3), Value::fromNum(0.0),
                                     Value::fromNum(20.0), Value::fromNum(0.0)}; // walker, 20 hp
            tree.scripts().vm().callOn(zs, "spawn_at", sa);
            std::vector<Value> dmg = {Value::fromNum(9999.0)};
            tree.scripts().vm().callOn(zs, "take_damage", dmg);
        }
        // Kills 1-4 at x1 (score_value 10 each), the 5th at x2 (streak hits 5) => 40 + 20 = 60.
        CHECK((int)glob(tree, "g_combo") == 5);
        CHECK((int)glob(tree, "g_mult") == 2);
        CHECK(glob(tree, "g_score") == 60.0);
        // Stop killing: after the combo window the streak resets.
        for (int i = 0; i < 200; ++i) tree.process(1.0 / 60.0);
        CHECK((int)glob(tree, "g_combo") == 0);
        CHECK((int)glob(tree, "g_mult") == 1);
    }

    // High-score meta: beatsBest ranks runs, and the persistence round-trips through KeyValueStore.
    {
        CHECK(zomboid::beatsBest(3, 500, 2, 400));   // a higher score wins
        CHECK(zomboid::beatsBest(5, 400, 3, 400));   // equal score, deeper wave wins
        CHECK(!zomboid::beatsBest(2, 300, 3, 400));  // a worse run does not
        CHECK(!zomboid::beatsBest(2, 400, 2, 400));  // an identical run is not "better"

        const char* path = "zomboid_hs_test.ini";
        {
            maz::core::KeyValueStore w;
            w.load(path);
            w.set("best_wave", 7);
            w.set("best_score", 1234);
            CHECK(w.save());
        }
        {
            maz::core::KeyValueStore r;
            r.load(path);
            CHECK(r.getInt("best_wave", 0) == 7);       // survived a save/load cycle
            CHECK(r.getInt("best_score", 0) == 1234);
        }
        std::remove(path);
    }

    // Medkits: a dropped kit activates, heals the survivor on pickup (capped), and expires if ignored.
    auto activeMedkits = [](SceneTree& t) {
        int c = 0;
        for (SceneNode* m : t.nodesInGroup("medkits"))
            if (m->script().instance->findField("active")->boolean) ++c;
        return c;
    };
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        CHECK((int)tree.nodesInGroup("medkits").size() == zomboid::kMedkitPool);
        CHECK(activeMedkits(tree) == 0);

        // Drop a medkit far from a wounded survivor, then walk onto it: it heals and is consumed.
        sField(survivor, "health")->number = 50.0;
        survivor->setPosition(0.0, 0.0);
        std::vector<Value> at = {Value::fromNum(200.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_medkit", at);
        CHECK(activeMedkits(tree) == 1);
        // Move the survivor onto it and tick.
        SceneNode* kit = nullptr;
        for (SceneNode* m : tree.nodesInGroup("medkits"))
            if (m->script().instance->findField("active")->boolean) kit = m;
        survivor->setPosition(kit->x(), kit->y());
        tree.process(0.016);
        CHECK(sField(survivor, "health")->number > 50.0);  // healed
        CHECK(activeMedkits(tree) == 0);                    // consumed

        // Heal is capped at max health.
        sField(survivor, "health")->number = sField(survivor, "max_health")->number - 5.0;
        const double cap = sField(survivor, "max_health")->number;
        std::vector<Value> at2 = {Value::fromNum(survivor->x()), Value::fromNum(survivor->y())};
        tree.scripts().vm().call("drop_medkit", at2);
        tree.process(0.016);
        CHECK(sField(survivor, "health")->number == cap); // not over max

        // An ignored medkit expires.
        survivor->setPosition(0.0, 0.0);
        std::vector<Value> far = {Value::fromNum(500.0), Value::fromNum(500.0)};
        tree.scripts().vm().call("drop_medkit", far);
        CHECK(activeMedkits(tree) == 1);
        for (int i = 0; i < 900; ++i) tree.process(1.0 / 60.0); // > 12 s max_life
        CHECK(activeMedkits(tree) == 0);
    }

    // Pickup magnetism: a medkit near the survivor drifts toward them; a far one stays put.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        // Near kit (5 units, inside the 6-unit magnet, outside the 2.2 pickup range).
        std::vector<Value> nearAt = {Value::fromNum(5.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_medkit", nearAt);
        SceneNode* kit = nullptr;
        for (SceneNode* m : tree.nodesInGroup("medkits"))
            if (m->script().instance->findField("active")->boolean) kit = m;
        const double nx0 = kit->x();
        tree.process(1.0 / 60.0);
        CHECK(kit->script().instance->findField("active")->boolean); // not yet picked up
        CHECK(kit->x() < nx0);                                        // drifted toward the survivor
    }
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        std::vector<Value> farAt = {Value::fromNum(20.0), Value::fromNum(0.0)}; // outside magnet range
        tree.scripts().vm().call("drop_medkit", farAt);
        SceneNode* kit = nullptr;
        for (SceneNode* m : tree.nodesInGroup("medkits"))
            if (m->script().instance->findField("active")->boolean) kit = m;
        const double fx0 = kit->x();
        tree.process(1.0 / 60.0);
        CHECK(kit->x() == fx0); // stayed put — no magnet at 20 units
    }

    // Power-ups: rare pooled pickups grant a timed buff that reverts when it lapses.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        CHECK((int)tree.nodesInGroup("powerups").size() == zomboid::kPowerupPool);
        CHECK(activePowerups(tree) == 0);

        // Rapid-fire (kind 0): fire rate jumps while the buff is up, then falls back to base.
        survivor->setPosition(0.0, 0.0);
        const double baseRate = sField(survivor, "fire_rate")->number;
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_powerup", at);
        CHECK(activePowerups(tree) == 1);
        tree.process(0.016);                                    // walk onto it
        CHECK(activePowerups(tree) == 0);                       // consumed
        CHECK((int)sField(survivor, "buff_kind")->number == 0);
        CHECK(sField(survivor, "fire_rate")->number > baseRate * 1.5);
        sField(survivor, "health")->number = 100000.0;          // outlast the buff, ignore the horde
        for (int i = 0; i < 600; ++i) tree.process(1.0 / 60.0); // > 8 s buff window
        CHECK((int)sField(survivor, "buff_kind")->number == -1);
        const double fr = sField(survivor, "fire_rate")->number;
        CHECK(fr > baseRate - 0.01 && fr < baseRate + 0.01);    // reverted to base

        // Shield (kind 2): incoming damage is fully negated while active.
        survivor->setPosition(0.0, 0.0);
        std::vector<Value> at2 = {Value::fromNum(0.0), Value::fromNum(0.0), Value::fromNum(2.0)};
        tree.scripts().vm().call("drop_powerup", at2);
        tree.process(0.016);                                    // pick it up
        CHECK((int)sField(survivor, "buff_kind")->number == 2);
        sField(survivor, "health")->number = 100.0;
        Value sv = survivor->script();
        std::vector<Value> dmg = {Value::fromNum(50.0)};
        tree.scripts().vm().callOn(sv, "take_damage", dmg);
        CHECK(sField(survivor, "health")->number == 100.0);     // shield soaked it
    }

    // Adrenaline: dropping below 25% health surges the fire rate; healing back above it reverts.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        const double baseRate = sField(survivor, "fire_rate")->number;
        const double maxHp = sField(survivor, "max_health")->number;
        CHECK(!sField(survivor, "adrenaline")->boolean);

        sField(survivor, "health")->number = maxHp * 0.2; // critically wounded
        tree.process(0.016);
        CHECK(sField(survivor, "adrenaline")->boolean);
        CHECK(sField(survivor, "fire_rate")->number > baseRate * 1.4); // ~1.5x surge

        sField(survivor, "health")->number = maxHp * 0.9; // patched up
        tree.process(0.016);
        CHECK(!sField(survivor, "adrenaline")->boolean);
        const double fr = sField(survivor, "fire_rate")->number;
        CHECK(fr > baseRate - 0.01 && fr < baseRate + 0.01);         // back to base
    }

    // Supply crate: a dropped care package refills ammo + grenades and heals when collected.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        CHECK((int)tree.nodesInGroup("crates").size() == zomboid::kCratePool);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 50.0;
        const double nades0 = sField(survivor, "grenades")->number;
        const double res0 = (*sField(survivor, "reserves")->array)[0].number;

        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_crate", at);
        int active = 0;
        for (SceneNode* c : tree.nodesInGroup("crates"))
            if (c->script().instance->findField("active")->boolean) ++active;
        CHECK(active == 1);

        tree.process(1.0 / 60.0);   // survivor is on the crate → collected
        CHECK(sField(survivor, "grenades")->number == nades0 + 2);              // +2 grenades
        CHECK((*sField(survivor, "reserves")->array)[0].number > res0);         // ammo refilled
        CHECK(sField(survivor, "health")->number > 50.0);                       // healed
    }

    // Dodge-roll: bursts the survivor in a direction, grants i-frame invulnerability, then cools
    // down. Damage taken mid-roll is ignored; a second dodge is refused until the cooldown clears.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);
        CHECK(sField(survivor, "dash_cd")->number == 0.0);

        // Fire the dodge to the right (+x).
        std::vector<Value> dir = {Value::fromNum(1.0), Value::fromNum(0.0)};
        Value ok = tree.scripts().vm().callOn(sv, "dash", dir);
        CHECK(ok.boolean);                                    // dodge fired
        CHECK(sField(survivor, "iframes")->number > 0.0);     // invulnerable now
        CHECK(sField(survivor, "dash_cd")->number > 0.0);     // on cooldown

        // A second dodge is refused while cooling down.
        Value again = tree.scripts().vm().callOn(sv, "dash", dir);
        CHECK(!again.boolean);

        // Damage taken during i-frames is fully ignored.
        sField(survivor, "health")->number = 100.0;
        std::vector<Value> dmg = {Value::fromNum(40.0)};
        tree.scripts().vm().callOn(sv, "take_damage", dmg);
        CHECK(sField(survivor, "health")->number == 100.0);   // untouchable mid-roll

        // The burst carries the survivor to the right over a few frames.
        const double x0 = survivor->x();
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 15; ++i) tree.scripts().vm().callOn(sv, "_process", dtv);
        CHECK(survivor->x() > x0 + 1.0);                       // moved rightward

        // After the i-frames lapse, damage lands normally again.
        for (int i = 0; i < 40; ++i) tree.scripts().vm().callOn(sv, "_process", dtv);
        CHECK(sField(survivor, "iframes")->number <= 0.0);
        tree.scripts().vm().callOn(sv, "take_damage", dmg);
        CHECK(sField(survivor, "health")->number < 100.0);    // vulnerable again
    }

    // Melee shove: a free close-range swing that damages and knocks back adjacent zombies, then
    // goes on cooldown. Zombies out of range are untouched.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);

        // Park a tanky zombie point-blank (x=2) and another farZ away (x=40).
        SceneNode* nearZ = tree.findNode("Zombie0");
        SceneNode* farZ = tree.findNode("Zombie1");
        std::vector<Value> atNear = {Value::fromNum(2.0), Value::fromNum(0.0), Value::fromNum(300.0),
                                     Value::fromNum(0.0)};
        std::vector<Value> atFar = {Value::fromNum(40.0), Value::fromNum(0.0), Value::fromNum(300.0),
                                    Value::fromNum(0.0)};
        Value nearS = nearZ->script();
        Value farS = farZ->script();
        tree.scripts().vm().callOn(nearS, "spawn_at", atNear);
        tree.scripts().vm().callOn(farS, "spawn_at", atFar);
        const double nearHp0 = nearZ->script().instance->findField("health")->number;
        const double farHp0 = farZ->script().instance->findField("health")->number;
        const double farX0 = farZ->x();

        std::vector<Value> none;
        Value struck = tree.scripts().vm().callOn(sv, "melee", none);
        CHECK(struck.number == 1.0);                                          // only the nearZ zombie
        CHECK(nearZ->script().instance->findField("health")->number < nearHp0); // damaged
        CHECK(nearZ->x() > 2.0);                                                // knocked back (+x)
        CHECK(farZ->script().instance->findField("health")->number == farHp0);  // farZ one untouched
        CHECK(farZ->x() == farX0);
        CHECK(sField(survivor, "melee_cd")->number > 0.0);                     // on cooldown

        // A second swing during cooldown is refused (-1) and deals no further damage.
        const double nearHp1 = nearZ->script().instance->findField("health")->number;
        Value again = tree.scripts().vm().callOn(sv, "melee", none);
        CHECK(again.number == -1.0);
        CHECK(nearZ->script().instance->findField("health")->number == nearHp1);
    }

    // Proximity mine: deployed at the survivor's feet, arms after a safety delay, then detonates when
    // a zombie steps into trigger range — a heavy AoE blast. Consumes one from the stock.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);
        CHECK((int)tree.nodesInGroup("mines").size() == zomboid::kMinePool);
        const double stock0 = sField(survivor, "mines")->number;
        CHECK(stock0 >= 1.0);

        // Deploy a mine at the origin.
        std::vector<Value> none;
        Value placed = tree.scripts().vm().callOn(sv, "place_mine", none);
        CHECK(placed.boolean);
        CHECK(sField(survivor, "mines")->number == stock0 - 1.0);
        SceneNode* mine = tree.findNode("Mine0");
        CHECK(mine->script().instance->findField("active")->boolean);

        // Park a zombie right on the mine BEFORE it arms: the safety fuse means no early detonation.
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> at = {Value::fromNum(0.5), Value::fromNum(0.0), Value::fromNum(200.0),
                                 Value::fromNum(0.0)};
        tree.scripts().vm().callOn(zs, "spawn_at", at);
        const double zHp0 = z->script().instance->findField("health")->number;
        Value mv = mine->script();
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        tree.scripts().vm().callOn(mv, "_process", dtv);   // still arming
        CHECK(mine->script().instance->findField("active")->boolean);        // not yet blown
        CHECK(z->script().instance->findField("health")->number == zHp0);    // no early damage

        // Let the safety fuse elapse (~0.6s), then it detonates on the in-range zombie.
        for (int i = 0; i < 45; ++i) tree.scripts().vm().callOn(mv, "_process", dtv);
        CHECK(!mine->script().instance->findField("active")->boolean);       // detonated + recycled
        CHECK(z->script().instance->findField("health")->number < zHp0);     // caught in the blast

        // Collecting a supply crate replenishes a mine.
        survivor->setPosition(0.0, 0.0);
        std::vector<Value> catv = {Value::fromNum(0.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_crate", catv);
        const double stock1 = sField(survivor, "mines")->number;
        tree.process(1.0 / 60.0);   // survivor is on the crate → collected
        CHECK(sField(survivor, "mines")->number == stock1 + 1.0);
    }

    // Second wind: lethal damage is cancelled while a revive charge remains — the survivor bursts back
    // with half health, i-frames, and a crowd-clearing nova. Only the final (chargeless) hit is fatal.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);
        CHECK(sField(survivor, "revives")->number == 1.0);
        const double maxhp = sField(survivor, "max_health")->number;

        // Park a zombie in the nova radius so we can confirm the burst hits it.
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> at = {Value::fromNum(4.0), Value::fromNum(0.0), Value::fromNum(300.0),
                                 Value::fromNum(0.0)};
        tree.scripts().vm().callOn(zs, "spawn_at", at);
        const double zHp0 = z->script().instance->findField("health")->number;

        // A killing blow: cancelled by the revive instead of ending the run.
        std::vector<Value> big = {Value::fromNum(9999.0)};
        tree.scripts().vm().callOn(sv, "take_damage", big);
        CHECK(sField(survivor, "alive")->boolean);                        // survived
        CHECK(sField(survivor, "revives")->number == 0.0);               // charge spent
        CHECK(sField(survivor, "health")->number == maxhp * 0.5);        // back at half health
        CHECK(sField(survivor, "iframes")->number > 0.0);                // emergency i-frames
        CHECK(z->script().instance->findField("health")->number < zHp0); // nova hit the crowd

        // The nova's i-frames make the next blow harmless; clear them, then a chargeless killing blow
        // is now actually fatal.
        sField(survivor, "iframes")->number = 0.0;
        tree.scripts().vm().callOn(sv, "take_damage", big);
        CHECK(!sField(survivor, "alive")->boolean);                       // no charge left → dead
    }

    // Second wind is re-earned at the 50-kill milestone (capped at 3).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        sField(survivor, "revives")->number = 0.0;   // spend the starting charge
        std::vector<Value> none;
        for (int i = 0; i < 50; ++i) tree.scripts().vm().callOn(sv, "on_kill", none);
        CHECK(sField(survivor, "revives")->number == 1.0);   // milestone granted one back
    }

    // Splitter (kind 6): a mid-tier zombie that bursts into two fast runners when killed.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        CHECK(aliveZombies(tree) == 0);

        // Spawn a lone splitter and confirm its kind, then kill it in one blow.
        SceneNode* sp = tree.findNode("Zombie0");
        Value spv = sp->script();
        std::vector<Value> spawn = {Value::fromNum(20.0), Value::fromNum(0.0), Value::fromNum(6.0),
                                    Value::fromNum(3.0)}; // spawn(x,y,kind=6,wave=3)
        tree.scripts().vm().callOn(spv, "spawn", spawn);
        CHECK((int)sp->script().instance->findField("kind")->number == 6);
        CHECK(aliveZombies(tree) == 1);

        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        tree.scripts().vm().callOn(spv, "take_damage", lethal);
        CHECK(!sp->script().instance->findField("alive")->boolean);   // the splitter itself is dead

        // Its death left exactly two live kind-1 runners spawned near where it fell.
        int runners = 0;
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (!z->script().instance->findField("alive")->boolean) continue;
            if ((int)z->script().instance->findField("kind")->number == 1) ++runners;
        }
        CHECK(runners == 2);
        CHECK(aliveZombies(tree) == 2);   // only the two splitlings remain

        // The splitlings spawned near the splitter's position (within a small radius).
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (!z->script().instance->findField("alive")->boolean) continue;
            const double dx = z->x() - 20.0, dy = z->y() - 0.0;
            CHECK(dx * dx + dy * dy <= 16.0);   // within ~4 units of the split point
        }
    }

    // Auto-turret sentry: deployed from stock, it auto-fires at nearby zombies over its lifetime,
    // then powers down. Out-of-range zombies are ignored.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);
        CHECK((int)tree.nodesInGroup("sentries").size() == zomboid::kSentryPool);
        const double stock0 = sField(survivor, "sentries")->number;
        CHECK(stock0 >= 1.0);

        // Deploy a sentry at the origin.
        std::vector<Value> none;
        Value placed = tree.scripts().vm().callOn(sv, "place_sentry", none);
        CHECK(placed.boolean);
        CHECK(sField(survivor, "sentries")->number == stock0 - 1.0);
        SceneNode* sen = tree.findNode("Sentry0");
        CHECK(sen->script().instance->findField("active")->boolean);

        // A zombie in range takes fire; a far zombie stays untouched.
        SceneNode* zin = tree.findNode("Zombie0");
        SceneNode* zout = tree.findNode("Zombie1");
        std::vector<Value> atIn = {Value::fromNum(6.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                   Value::fromNum(0.0)};
        std::vector<Value> atOut = {Value::fromNum(60.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                    Value::fromNum(0.0)};
        Value zinS = zin->script();
        Value zoutS = zout->script();
        tree.scripts().vm().callOn(zinS, "spawn_at", atIn);
        tree.scripts().vm().callOn(zoutS, "spawn_at", atOut);
        const double inHp0 = zin->script().instance->findField("health")->number;
        const double outHp0 = zout->script().instance->findField("health")->number;

        // Drive the sentry directly for a second (isolated from zombie movement).
        Value senv = sen->script();
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 60; ++i) tree.scripts().vm().callOn(senv, "_process", dtv);
        CHECK(zin->script().instance->findField("health")->number < inHp0);    // in-range zombie shot
        CHECK(zout->script().instance->findField("health")->number == outHp0); // far zombie ignored

        // It powers down once its lifetime elapses (~12 s more).
        for (int i = 0; i < 60 * 13; ++i) tree.scripts().vm().callOn(senv, "_process", dtv);
        CHECK(!sen->script().instance->findField("active")->boolean);

        // A supply crate replenishes a sentry.
        survivor->setPosition(0.0, 0.0);
        const double stock1 = sField(survivor, "sentries")->number;
        std::vector<Value> catv = {Value::fromNum(0.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_crate", catv);
        tree.process(1.0 / 60.0);
        CHECK(sField(survivor, "sentries")->number == stock1 + 1.0);
    }

    // Burning status: an ignited zombie takes fire damage over time, then the fire burns out.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> at = {Value::fromNum(30.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                 Value::fromNum(0.0)}; // tanky, parked far from the survivor
        tree.scripts().vm().callOn(zs, "spawn_at", at);
        CHECK(z->script().instance->findField("burn_timer")->number == 0.0);

        // Ignite it: 3 seconds at 20 dps.
        std::vector<Value> ig = {Value::fromNum(3.0), Value::fromNum(20.0)};
        tree.scripts().vm().callOn(zs, "ignite", ig);
        CHECK(z->script().instance->findField("burn_timer")->number > 0.0);
        const double hp0 = z->script().instance->findField("health")->number;

        // Drive the zombie's own _process for ~1 s: fire ticks should chew its health.
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 60; ++i) tree.scripts().vm().callOn(zs, "_process", dtv);
        CHECK(z->script().instance->findField("health")->number < hp0);   // burned

        // After the burn duration lapses (~3 s total), the fire is out and stops damaging.
        for (int i = 0; i < 60 * 3; ++i) tree.scripts().vm().callOn(zs, "_process", dtv);
        CHECK(z->script().instance->findField("burn_timer")->number == 0.0);
        const double hpAfter = z->script().instance->findField("health")->number;
        for (int i = 0; i < 60; ++i) tree.scripts().vm().callOn(zs, "_process", dtv);
        CHECK(z->script().instance->findField("health")->number == hpAfter); // no more fire damage
    }

    // Incendiary exploder blast: a dying exploder ignites zombies caught in its blast.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        // A tanky neighbour parked right next to the exploder (out of the survivor's way).
        SceneNode* nb = tree.findNode("Zombie1");
        Value nbs = nb->script();
        std::vector<Value> nat = {Value::fromNum(50.0), Value::fromNum(0.0), Value::fromNum(400.0),
                                  Value::fromNum(0.0)};
        tree.scripts().vm().callOn(nbs, "spawn_at", nat);
        CHECK(nb->script().instance->findField("burn_timer")->number == 0.0);

        // An exploder at the neighbour's position (kind 4), then kill it to trigger its blast.
        SceneNode* ex = tree.findNode("Zombie0");
        Value exs = ex->script();
        std::vector<Value> espawn = {Value::fromNum(50.5), Value::fromNum(0.0), Value::fromNum(4.0),
                                     Value::fromNum(1.0)}; // spawn(x,y,kind=4,wave=1)
        tree.scripts().vm().callOn(exs, "spawn", espawn);
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        tree.scripts().vm().callOn(exs, "take_damage", lethal);   // detonate
        CHECK(nb->script().instance->findField("burn_timer")->number > 0.0); // neighbour set alight
    }

    // Exploder (kind 4): fast/fragile suicide bomber that blasts the survivor on death
    // only if they are close, so it must be shot from a distance.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();

        // Fragile + fast relative to a walker: wave-1 hp 25, speed 19.
        std::vector<Value> spawn = {Value::fromNum(2.0), Value::fromNum(0.0), Value::fromNum(4.0),
                                    Value::fromNum(1.0)}; // spawn(x=2,y=0,kind=4,wave=1)
        tree.scripts().vm().callOn(zs, "spawn", spawn);
        CHECK((int)z->script().instance->findField("kind")->number == 4);
        CHECK(z->script().instance->findField("health")->number <= 30.0);   // fragile
        CHECK(z->script().instance->findField("speed")->number > 15.0);     // faster than a walker

        // Dying next to the survivor detonates: AoE damage lands.
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        std::vector<Value> kill = {Value::fromNum(999.0)};
        tree.scripts().vm().callOn(zs, "take_damage", kill);
        CHECK(!z->script().instance->findField("alive")->boolean);
        CHECK(sField(survivor, "health")->number < 100.0);                  // blast hurt the survivor

        // A second exploder dying far away does NOT reach the survivor.
        SceneNode* z2 = tree.findNode("Zombie1");
        Value zs2 = z2->script();
        std::vector<Value> spawnFar = {Value::fromNum(200.0), Value::fromNum(0.0),
                                       Value::fromNum(4.0), Value::fromNum(1.0)};
        tree.scripts().vm().callOn(zs2, "spawn", spawnFar);
        const double hpBefore = sField(survivor, "health")->number;
        tree.scripts().vm().callOn(zs2, "take_damage", kill);
        CHECK(!z2->script().instance->findField("alive")->boolean);
        CHECK(sField(survivor, "health")->number == hpBefore);              // out of blast range
    }

    // Spitter (kind 5): a ranged zombie that halts at distance and lobs acid globs.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        CHECK((int)tree.nodesInGroup("spits").size() == zomboid::kSpitPool);
        CHECK(activeSpits(tree) == 0);

        // Spawn a spitter 10 units away — inside its ~13 spitting range.
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        std::vector<Value> spawn = {Value::fromNum(10.0), Value::fromNum(0.0), Value::fromNum(5.0),
                                    Value::fromNum(1.0)}; // spawn(x=10,y=0,kind=5,wave=1)
        tree.scripts().vm().callOn(zs, "spawn", spawn);
        CHECK((int)z->script().instance->findField("kind")->number == 5);

        // One tick: cooldown starts at 0, so it lobs a glob and holds its ground (doesn't rush in).
        tree.process(0.05);
        CHECK(activeSpits(tree) >= 1);   // a glob is airborne
        CHECK(z->x() > 5.0);             // the spitter kept its distance

        // Let the glob travel and splash on the stationary survivor.
        for (int i = 0; i < 120; ++i) tree.process(1.0 / 60.0);
        CHECK(sField(survivor, "health")->number < 100.0); // the acid hit landed
    }

    // The Director mixes exploders into later waves (wave 4+).
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* dir = tree.findNode("Director");
        Value ds = dir->script();
        std::vector<Value> a = {Value::fromNum(4.0)};
        tree.scripts().vm().callOn(ds, "start_wave", a); // force wave 4
        bool hasExploder = false;
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (!z->script().instance->findField("alive")->boolean) continue;
            if ((int)z->script().instance->findField("kind")->number == 4) hasExploder = true;
        }
        CHECK(hasExploder);
    }

    // Out-of-combat regeneration: after a delay without damage, health slowly recovers, capped at max.
    // Driven by calling the survivor's _process directly so the horde never interferes.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        const double maxHp = sField(survivor, "max_health")->number;
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};

        sField(survivor, "health")->number = 40.0;
        sField(survivor, "regen_timer")->number = 0.0;
        for (int i = 0; i < 120; ++i) tree.scripts().vm().callOn(sv, "_process", dtv); // 2 s < delay
        CHECK(sField(survivor, "health")->number <= 40.5);   // no heal yet inside the delay window

        for (int i = 0; i < 240; ++i) tree.scripts().vm().callOn(sv, "_process", dtv); // +4 s past delay
        CHECK(sField(survivor, "health")->number > 40.0);    // regenerated once the delay elapsed

        // Regen never overshoots max health.
        sField(survivor, "health")->number = maxHp - 2.0;
        sField(survivor, "regen_timer")->number = 10.0;
        for (int i = 0; i < 180; ++i) tree.scripts().vm().callOn(sv, "_process", dtv);
        CHECK(sField(survivor, "health")->number == maxHp);
    }

    // Knockback: a bullet shoves a light zombie along its travel; a heavy brute barely budges.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        (void)survivor;
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> park = {Value::fromNum(10.0), Value::fromNum(0.0), Value::fromNum(200.0),
                                   Value::fromNum(0.0)}; // walker, radius 1.0
        tree.scripts().vm().callOn(zs, "spawn_at", park);
        const double wx0 = z->x();
        std::vector<Value> kb = {Value::fromNum(1.0), Value::fromNum(0.0), Value::fromNum(0.6)};
        tree.scripts().vm().callOn(zs, "hit_knockback", kb);
        CHECK(z->x() > wx0 + 0.4);  // shoved along +x

        SceneNode* zb = tree.findNode("Zombie1");
        Value zbs = zb->script();
        std::vector<Value> brute = {Value::fromNum(10.0), Value::fromNum(0.0), Value::fromNum(2.0),
                                    Value::fromNum(1.0)}; // brute, radius 1.8
        tree.scripts().vm().callOn(zbs, "spawn", brute);
        const double bx0 = zb->x();
        tree.scripts().vm().callOn(zbs, "hit_knockback", kb);
        CHECK(zb->x() - bx0 < 0.3);  // heavy body resists
    }

    // Chill/slow: a slowed zombie crawls toward the survivor far less per tick than an unimpaired one.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> dtv = {Value::fromNum(0.1)};
        std::vector<Value> park = {Value::fromNum(10.0), Value::fromNum(0.0), Value::fromNum(999.0),
                                   Value::fromNum(15.0)};

        // Baseline advance over one tick.
        tree.scripts().vm().callOn(zs, "spawn_at", park);
        tree.scripts().vm().callOn(zs, "_process", dtv);
        const double moved1 = 10.0 - z->x();
        CHECK(moved1 > 0.0);

        // Same tick, but chilled first: it barely moves.
        tree.scripts().vm().callOn(zs, "spawn_at", park);
        std::vector<Value> dur = {Value::fromNum(2.0)};
        tree.scripts().vm().callOn(zs, "apply_slow", dur);
        tree.scripts().vm().callOn(zs, "_process", dtv);
        const double moved2 = 10.0 - z->x();
        CHECK(moved2 > 0.0);
        CHECK(moved2 < moved1 * 0.6); // chilled to ~40% speed
    }

    // Shatter: a chilled zombie takes extra damage from the same hit (freeze-then-shred synergy).
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* a = tree.findNode("Zombie0");
        SceneNode* b = tree.findNode("Zombie1");
        Value as = a->script();
        Value bs = b->script();
        std::vector<Value> park = {Value::fromNum(5.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                   Value::fromNum(0.0)};
        tree.scripts().vm().callOn(as, "spawn_at", park);
        tree.scripts().vm().callOn(bs, "spawn_at", park);
        std::vector<Value> dur = {Value::fromNum(2.0)};
        tree.scripts().vm().callOn(bs, "apply_slow", dur); // chill only b

        const double ah0 = a->script().instance->findField("health")->number;
        const double bh0 = b->script().instance->findField("health")->number;
        std::vector<Value> hit = {Value::fromNum(50.0)};
        tree.scripts().vm().callOn(as, "take_damage", hit);
        tree.scripts().vm().callOn(bs, "take_damage", hit);
        const double aLost = ah0 - a->script().instance->findField("health")->number;
        const double bLost = bh0 - b->script().instance->findField("health")->number;
        CHECK(bLost > aLost);              // chilled body took more
        CHECK(bLost > aLost * 1.4);        // ~1.5x shatter multiplier
    }

    // Elite ("champion") zombies: crowning one boosts its health and score and guarantees a medkit.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        (void)survivor;
        SceneNode* z = tree.findNode("Zombie0");
        Value zv = z->script();
        std::vector<Value> sp = {Value::fromNum(5.0), Value::fromNum(0.0), Value::fromNum(0.0),
                                 Value::fromNum(2.0)}; // spawn(x,y,kind=0,wave=2)
        tree.scripts().vm().callOn(zv, "spawn", sp);
        const double baseHp = z->script().instance->findField("health")->number;
        const double baseScore = z->script().instance->findField("score_value")->number;

        std::vector<Value> none;
        tree.scripts().vm().callOn(zv, "make_elite", none);
        CHECK(z->script().instance->findField("elite")->boolean);
        CHECK(z->script().instance->findField("health")->number > baseHp * 2.0);
        CHECK(z->script().instance->findField("score_value")->number > baseScore * 2.0);

        // Killing an elite always drops a medkit.
        CHECK(activeMedkits(tree) == 0);
        std::vector<Value> big = {Value::fromNum(9999.0)};
        tree.scripts().vm().callOn(zv, "take_damage", big);
        CHECK(!z->script().instance->findField("alive")->boolean);
        CHECK(activeMedkits(tree) >= 1); // guaranteed elite drop
    }

    // Exploder chain: its death blast also damages nearby non-exploder zombies (but spares distant ones).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0); // keep the player well clear of the blast
        SceneNode* ex = tree.findNode("Zombie0");
        Value exs = ex->script();
        std::vector<Value> sp = {Value::fromNum(0.0), Value::fromNum(0.0), Value::fromNum(4.0),
                                 Value::fromNum(1.0)}; // exploder at origin
        tree.scripts().vm().callOn(exs, "spawn", sp);

        SceneNode* near = tree.findNode("Zombie1");
        Value nears = near->script();
        std::vector<Value> np = {Value::fromNum(3.0), Value::fromNum(0.0), Value::fromNum(200.0),
                                 Value::fromNum(0.0)}; // tanky walker 3 units away
        tree.scripts().vm().callOn(nears, "spawn_at", np);
        SceneNode* far = tree.findNode("Zombie2");
        Value fars = far->script();
        std::vector<Value> fp = {Value::fromNum(20.0), Value::fromNum(0.0), Value::fromNum(200.0),
                                 Value::fromNum(0.0)}; // walker well outside the blast
        tree.scripts().vm().callOn(fars, "spawn_at", fp);

        const double nearHp0 = near->script().instance->findField("health")->number;
        const double farHp0 = far->script().instance->findField("health")->number;
        std::vector<Value> kill = {Value::fromNum(999.0)};
        tree.scripts().vm().callOn(exs, "take_damage", kill); // detonate
        CHECK(!ex->script().instance->findField("alive")->boolean);
        CHECK(near->script().instance->findField("health")->number < nearHp0); // caught in the chain
        CHECK(far->script().instance->findField("health")->number == farHp0);  // out of range, spared
    }

    // Boss ground slam (kind 3): a periodic shockwave hits a survivor within its radius even when
    // they are outside melee-bite range, but spares one standing well clear of it.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        SceneNode* boss = tree.findNode("Zombie0");
        Value bz = boss->script();
        std::vector<Value> sp = {Value::fromNum(6.0), Value::fromNum(0.0), Value::fromNum(3.0),
                                 Value::fromNum(1.0)}; // boss at dist 6 (inside slam 10, outside bite)
        tree.scripts().vm().callOn(bz, "spawn", sp);
        boss->script().instance->findField("slam_cd")->number = 0.01; // slam almost ready
        tree.process(0.02);
        CHECK(sField(survivor, "health")->number <= 75.0); // slam landed (~25), no bite at range 6
    }
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        SceneNode* boss = tree.findNode("Zombie0");
        Value bz = boss->script();
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0), Value::fromNum(3.0),
                                 Value::fromNum(1.0)}; // far outside the slam radius
        tree.scripts().vm().callOn(bz, "spawn", sp);
        boss->script().instance->findField("slam_cd")->number = 0.01;
        tree.process(0.02);
        CHECK(sField(survivor, "health")->number == 100.0); // out of slam range, unscathed
    }

    // Overcharge ultimate: kills fill the meter; a detonate wipes the field and resets the charge.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        CHECK(!sField(survivor, "ult_ready")->boolean);

        const int need = (int)sField(survivor, "ult_max")->number;
        std::vector<Value> one = {Value::fromNum(1.0)};
        for (int i = 0; i < need; ++i) tree.scripts().vm().callOn(sv, "add_ult", one);
        CHECK(sField(survivor, "ult_ready")->boolean);       // meter full

        // Park a cluster of zombies, then detonate.
        SceneNode* zn[5] = {tree.findNode("Zombie0"), tree.findNode("Zombie1"), tree.findNode("Zombie2"),
                            tree.findNode("Zombie3"), tree.findNode("Zombie4")};
        for (int i = 0; i < 5; ++i) {
            Value zv = zn[i]->script();
            std::vector<Value> a = {Value::fromNum(5.0 + i), Value::fromNum(0.0),
                                    Value::fromNum(30.0), Value::fromNum(0.0)}; // spawn_at
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        }
        CHECK(aliveZombies(tree) >= 5);
        std::vector<Value> none;
        tree.scripts().vm().callOn(sv, "detonate", none);
        CHECK(aliveZombies(tree) == 0);                      // field wiped
        CHECK(!sField(survivor, "ult_ready")->boolean);      // charge consumed
        CHECK(sField(survivor, "ult")->number == 0.0);
    }

    // Kill milestones: every 25th kill grants a bonus grenade and a small heal.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        const double nades0 = sField(survivor, "grenades")->number;
        sField(survivor, "health")->number = 50.0;
        std::vector<Value> none;

        for (int i = 0; i < 24; ++i) tree.scripts().vm().callOn(sv, "on_kill", none);
        CHECK(sField(survivor, "grenades")->number == nades0);   // no bonus before the milestone
        CHECK(sField(survivor, "health")->number == 50.0);

        tree.scripts().vm().callOn(sv, "on_kill", none);         // the 25th kill
        CHECK(sField(survivor, "grenades")->number == nades0 + 1); // bonus grenade
        CHECK(sField(survivor, "health")->number > 50.0);          // bonus heal
        CHECK((int)sField(survivor, "next_bonus")->number == 50);  // next milestone advanced
    }

    // Run stats: the survive timer advances while alive and bullet hits are counted (accuracy).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        CHECK(sField(survivor, "time_survived")->number == 0.0);
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 60; ++i) tree.scripts().vm().callOn(sv, "_process", dtv);
        const double t = sField(survivor, "time_survived")->number;
        CHECK(t > 0.9 && t < 1.1); // ~1 s of survival tracked

        // A bullet connecting increments the hit counter.
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> at = {Value::fromNum(6.0), Value::fromNum(0.0), Value::fromNum(200.0),
                                 Value::fromNum(0.0)}; // park a tanky target dead ahead
        tree.scripts().vm().callOn(zs, "spawn_at", at);
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;
        sField(survivor, "hits")->number = 0.0;
        sField(survivor, "firing")->boolean = true;
        for (int i = 0; i < 30 && sField(survivor, "hits")->number == 0.0; ++i) tree.process(1.0 / 60.0);
        CHECK(sField(survivor, "hits")->number >= 1.0); // a shot landed
    }

    if (g_fail == 0) {
        std::printf("zomboid_sim: OK — pools, waves, twin-stick fire, weapons, enemy variety, "
                    "impact juice, ammo + reload, grenades, wave upgrades, combo multiplier, "
                    "high-score persistence, medkits, exploders, spitters, power-ups, kills/score, "
                    "survival, loot.\n");
        return 0;
    }
    std::printf("zomboid_sim: %d failure(s).\n", g_fail);
    return 1;
}
