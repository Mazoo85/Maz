// tests/zomboid/sim.cpp — headless verification of the ZOMBOID shooter's whole simulation.
// The game's rules live in apps/zomboid/game.hpp as a maz::script program on a scene::SceneTree; this
// standalone test drives that simulation with no GPU/window and asserts the shooter behaves. Kept as its
// own fast-compiling target (separate from the giant unit suite) so game iterations verify in seconds.
#include "game.hpp" // apps/zomboid — the flagship game's logic (on the include path via CMake)

#include "maz/scene/SceneTree.hpp"

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

    if (g_fail == 0) {
        std::printf("zomboid_sim: OK — pools, waves, twin-stick fire, weapons, enemy variety, "
                    "impact juice, ammo + reload, kills/score, survival, loot.\n");
        return 0;
    }
    std::printf("zomboid_sim: %d failure(s).\n", g_fail);
    return 1;
}
