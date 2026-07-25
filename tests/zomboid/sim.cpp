// tests/zomboid/sim.cpp — headless verification of the ZOMBOID shooter's whole simulation.
// The game's rules live in apps/zomboid/game.hpp as a maz::script program on a scene::SceneTree; this
// standalone test drives that simulation with no GPU/window and asserts the shooter behaves. Kept as its
// own fast-compiling target (separate from the giant unit suite) so game iterations verify in seconds.
#include "game.hpp" // apps/zomboid — the flagship game's logic (on the include path via CMake)

#include "maz/scene/SceneTree.hpp"
#include "maz/core/KeyValueStore.hpp"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>
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
static int activeAcid(SceneTree& t) {
    int c = 0;
    for (SceneNode* a : t.nodesInGroup("acid"))
        if (a->script().instance->findField("active")->boolean) ++c;
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

    // Railgun pops barrels: a barrel straddling the beam detonates (like an ordinary bullet), while one
    // off the beam is untouched.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        setWeapon(tree, survivor, 3);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;
        auto& vm = tree.scripts().vm();

        SceneNode* onBeam = tree.findNode("Barrel0");
        SceneNode* offBeam = tree.findNode("Barrel1");
        Value obv = onBeam->script();
        Value ofv = offBeam->script();
        std::vector<Value> onAt = {Value::fromNum(12.0), Value::fromNum(0.0)};    // dead on the +x beam
        std::vector<Value> offAt = {Value::fromNum(12.0), Value::fromNum(10.0)};  // well off the beam
        vm.callOn(obv, "place", onAt);
        vm.callOn(ofv, "place", offAt);
        CHECK(sField(onBeam, "active")->boolean);
        CHECK(sField(offBeam, "active")->boolean);

        Value sv = survivor->script();
        std::vector<Value> none;
        vm.callOn(sv, "do_shoot", none);   // one railgun beam down +x
        CHECK(!sField(onBeam, "active")->boolean);   // barrel on the beam detonated
        CHECK(sField(offBeam, "active")->boolean);   // off-beam barrel intact
    }

    // Barrel blast respects i-frames: a survivor mid-dodge rides the explosion out completely — no damage
    // AND no knockback — matching the boss slam and brute. The blast's damage already honored i-frames, but
    // its fling did not, so a perfectly-dodged survivor took no damage yet was still hurled clear. Two runs
    // at a fixed range: without i-frames the blast both hurts and flings; with i-frames up neither lands.
    {
        // Baseline — no i-frames: the blast damages and flings the survivor.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        sField(survivor, "iframes")->number = 0.0;
        auto& vm = tree.scripts().vm();
        SceneNode* barrel = tree.findNode("Barrel0");
        Value bv = barrel->script();
        std::vector<Value> place = {Value::fromNum(3.0), Value::fromNum(0.0)};
        vm.callOn(bv, "place", place);                 // barrel 3 units away, inside blast_radius 7
        std::vector<Value> none;
        vm.callOn(bv, "explode", none);
        CHECK(sField(survivor, "health")->number < 100.0);   // took the half-damage blast
        CHECK(survivor->x() < -1.0);                          // flung away from the barrel (toward -x)
    }
    {
        // Dodging — i-frames up: the blast is fully ridden out.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        sField(survivor, "iframes")->number = 5.0;     // mid-dodge invulnerability
        auto& vm = tree.scripts().vm();
        SceneNode* barrel = tree.findNode("Barrel0");
        Value bv = barrel->script();
        std::vector<Value> place = {Value::fromNum(3.0), Value::fromNum(0.0)};
        vm.callOn(bv, "place", place);
        std::vector<Value> none;
        vm.callOn(bv, "explode", none);
        CHECK(sField(survivor, "health")->number == 100.0);  // no damage — untouchable mid-roll
        CHECK(survivor->x() == 0.0);                          // no knockback either — rode it out
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

    // Rations aren't squandered: eating while not hungry (hunger 0) is declined without spending a ration,
    // the same "no wasted resource" rule the shop applies to a heal at full health. A mistimed E-press
    // can't throw a meal away for zero benefit.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value self = survivor->script();
        std::vector<Value> none;
        sField(survivor, "hunger")->number = 0.0;   // freshly fed / not hungry
        sField(survivor, "food")->number = 3.0;
        Value r = tree.scripts().vm().callOn(self, "eat", none);
        CHECK(r.boolean == false);                          // declined
        CHECK(sField(survivor, "food")->number == 3.0);     // ...and the ration is kept
        CHECK(sField(survivor, "hunger")->number == 0.0);

        // But once genuinely hungry, the same press feeds normally and consumes one ration.
        sField(survivor, "hunger")->number = 60.0;
        Value r2 = tree.scripts().vm().callOn(self, "eat", none);
        CHECK(r2.boolean == true);
        CHECK(sField(survivor, "food")->number == 2.0);
        CHECK(sField(survivor, "hunger")->number == 20.0);  // 60 - 40
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

    // Spawn-table coverage: the Director picks each spawn's kind through a hand-tuned modulo priority
    // chain, so a careless reorder or a wrong wave-gate could silently SHADOW a kind and stop it ever
    // spawning — with no test to catch it. Force a deep non-boss wave (9): it gates every kind (the
    // Healer needs wave >= 9) and spawns enough bodies (base 4 + 9*2 = 22) to reach even the sparse
    // high-modulo slots (Healer at index 17, Warper at 15). Confirm the FULL non-boss roster appears and
    // the boss does not (bosses only lead every 5th wave).
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* dir = tree.findNode("Director");
        Value ds = dir->script();
        std::vector<Value> w9 = {Value::fromNum(9.0)};
        tree.scripts().vm().callOn(ds, "start_wave", w9);

        bool seen[14];
        for (int k = 0; k < 14; ++k) seen[k] = false;
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (!sField(z, "alive")->boolean) continue;
            const int k = (int)sField(z, "kind")->number;
            if (k >= 0 && k < 14) seen[k] = true;
        }
        // walker 0, runner 1, brute 2, exploder 4, spitter 5, splitter 6, summoner 7, armored 8,
        // leaper 9, bloater 10, screamer 11, healer 12, warper 13 — every non-boss kind is reachable.
        const int expected[] = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
        for (int e : expected) { CHECK(seen[e]); }
        CHECK(!seen[3]);   // no boss on a non-multiple-of-5 wave
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

    // Ammo: firing drains the magazine, then an auto-reload refills it from reserve. Shown on the SMG (a
    // depletable weapon); the pistol's reserve is infinite and is validated in its own test below.
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        setWeapon(tree, s, 2);                          // SMG
        CHECK(sField(s, "cur_ammo")->number == 30.0);   // SMG magazine
        const double res0 = (*sField(s, "reserves")->array)[2].number; // 90 spare
        sField(s, "firing")->boolean = true;
        bool sawReload = false;
        for (int i = 0; i < 300; ++i) { // held fire — drains the mag and starts a reload
            tree.process(1.0 / 60.0);
            if (sField(s, "is_reloading")->boolean) sawReload = true;
        }
        CHECK(sawReload);                                // an empty mag triggered a reload
        sField(s, "firing")->boolean = false;
        for (int i = 0; i < 200; ++i) tree.process(1.0 / 60.0); // let it finish
        CHECK(sField(s, "cur_ammo")->number > 0.0);      // magazine refilled from reserve
        CHECK((*sField(s, "reserves")->array)[2].number < res0); // reserve was consumed
    }

    // Dry weapon: a NON-pistol with an empty magazine AND empty reserve fires exactly its last round and
    // no more. (The pistol is exempt — it has an infinite reserve; see the next test.)
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);
        setWeapon(tree, s, 2);                                     // SMG: a depletable weapon
        (*sField(s, "mags")->array)[2] = Value::fromNum(1.0);     // one round chambered
        (*sField(s, "reserves")->array)[2] = Value::fromNum(0.0); // nothing to reload
        sField(s, "firing")->boolean = true;
        for (int i = 0; i < 120; ++i) tree.process(1.0 / 60.0);
        CHECK(sField(s, "shots")->number == 1.0);        // fired once, then dry
        CHECK(sField(s, "cur_ammo")->number == 0.0);
        CHECK(sField(s, "is_reloading")->boolean == false); // can't reload from an empty reserve
    }

    // Infinite-reserve sidearm: the pistol (weapon 0) reloads even from an empty reserve, so it can never
    // be left permanently dry — firing its last round auto-reloads and it keeps shooting.
    {
        SceneTree tree;
        SceneNode* s = zomboid::buildScene(tree);            // weapon 0 (pistol) is the default
        (*sField(s, "mags")->array)[0] = Value::fromNum(1.0);     // one round chambered
        (*sField(s, "reserves")->array)[0] = Value::fromNum(0.0); // reserve empty — pistol ignores it
        sField(s, "firing")->boolean = true;
        for (int i = 0; i < 240; ++i) tree.process(1.0 / 60.0);  // ~4s: fire, auto-reload, fire again
        CHECK(sField(s, "shots")->number > 1.0);                 // kept firing past the last chambered round
        // Never stranded: either rounds are chambered or a reload is mid-flight — never a dead weapon.
        CHECK((*sField(s, "mags")->array)[0].number > 0.0 || sField(s, "is_reloading")->boolean);
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

    // Upgrades: each apply_upgrade cycles a distinct boost (+dmg, +rate, +max health, +ammo,
    // +crit chance, +crit damage, +move speed, -dodge cooldown) on an eight-step loop.
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
        const double cmult0 = sField(s, "crit_mult")->number;    // 2.0 by default
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k5: +crit damage
        CHECK(sField(s, "crit_mult")->number == cmult0 + 0.25);
        CHECK((int)sField(s, "upgrades")->number == 6);
        const double move6 = sField(s, "move_mult")->number;    // 1.0 by default
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k6: +move speed
        CHECK(sField(s, "move_mult")->number > move6);
        CHECK((int)sField(s, "upgrades")->number == 7);
        const double dashCd7 = sField(s, "dash_cd_max")->number; // 4.0 by default
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k7: -dodge cooldown
        CHECK(sField(s, "dash_cd_max")->number < dashCd7);
        CHECK((int)sField(s, "upgrades")->number == 8);
        // The cycle wraps at eight: the ninth upgrade rolls back to +damage (k0).
        const double dmgMult8 = sField(s, "dmg_mult")->number;
        tree.scripts().vm().callOn(self, "apply_upgrade", none); // k0 again: +damage
        CHECK(sField(s, "dmg_mult")->number > dmgMult8);
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

    // Grenade concussive stun: a zombie that survives the blast is briefly staggered (rooted), so the
    // frag is crowd control as well as damage — a tanky body caught in it reels where it stands.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* g0 = tree.findNode("Grenade0");
        g0->setPosition(50.0, 0.0);
        SceneNode* tank = tree.findNode("Zombie0");
        Value zs = tank->script();
        std::vector<Value> sa = {Value::fromNum(50.0), Value::fromNum(0.0),
                                 Value::fromNum(500.0), Value::fromNum(0.0)};  // 500 hp — survives
        vm.callOn(zs, "spawn_at", sa);
        CHECK(sField(tank, "stagger_timer")->number == 0.0);   // steady before the blast
        std::vector<Value> none;
        Value gv = g0->script();
        vm.callOn(gv, "explode", none);
        CHECK(sField(tank, "alive")->boolean);                 // tanked the blast...
        CHECK(sField(tank, "stagger_timer")->number > 0.0);    // ...but got concussed (rooted)
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

        // newPersonalBest is the PERSISTENCE rule: best wave and best score save independently, so a run
        // that improves EITHER axis is a new best. This is the fix for the stale-best-wave bug — a run
        // that reaches a new deepest wave but scores below the all-time-best score still counts.
        CHECK(zomboid::newPersonalBest(12, 400, 10, 600));  // deeper wave, LOWER score → still a new best
        CHECK(zomboid::newPersonalBest(5, 700, 10, 600));   // shallower wave, higher score → new best
        CHECK(zomboid::newPersonalBest(11, 601, 10, 600));  // both improved → new best
        CHECK(!zomboid::newPersonalBest(10, 600, 10, 600)); // identical run → not a new best
        CHECK(!zomboid::newPersonalBest(8, 500, 10, 600));  // worse on both axes → not a new best
        // The bug this guards against: beatsBest (score-primary) would REJECT a new-deepest-wave run whose
        // score trailed the record, so gating the save on it left best_wave stale — newPersonalBest fixes it.
        CHECK(!zomboid::beatsBest(12, 400, 10, 600));       // (score-primary ranking says "not better"...)
        CHECK(zomboid::newPersonalBest(12, 400, 10, 600));  // (...but it IS a new deepest wave to persist)

        // Run rank: a composite of wave + kills + accuracy graded S..D, monotonic in each input.
        CHECK(std::string(zomboid::runRankLetter(zomboid::runRank(1, 0, 0))) == "D");   // a quick death
        CHECK(std::string(zomboid::runRankLetter(zomboid::runRank(15, 200, 90))) == "S"); // a great run
        // A deeper/cleaner run never grades lower than a worse one.
        CHECK(zomboid::runRank(10, 120, 80) >= zomboid::runRank(4, 40, 50));
        CHECK(zomboid::runRank(8, 100, 100) >= zomboid::runRank(8, 100, 40)); // accuracy only helps
        // Accuracy is clamped, so out-of-range values don't distort the grade.
        CHECK(zomboid::runRank(5, 50, 150) == zomboid::runRank(5, 50, 100));
        // The five tiers all map to distinct letters.
        CHECK(std::string(zomboid::runRankLetter(0)) == "D");
        CHECK(std::string(zomboid::runRankLetter(2)) == "B");
        CHECK(std::string(zomboid::runRankLetter(4)) == "S");

        // Mutator HUD effect lines: every active mutator (1-9) has a non-empty, distinct plain-language
        // effect string (shown under its codename), and "no mutator" / out-of-range map to empty.
        CHECK(std::string(zomboid::mutatorEffect(0)).empty());    // no mutator → no line
        CHECK(std::string(zomboid::mutatorEffect(10)).empty());   // out of range → no line
        std::set<std::string> effects;
        for (int m = 1; m <= 9; ++m) {
            std::string e = zomboid::mutatorEffect(m);
            CHECK(!e.empty());               // every real mutator explains itself
            effects.insert(e);
        }
        CHECK(effects.size() == 9);          // all nine effect lines are distinct

        // Power-up HUD names: every buff_kind (0-9) has a non-empty, distinct display name (shown with its
        // countdown on the HUD), while the idle state (-1) and out-of-range map to empty.
        CHECK(std::string(zomboid::powerupName(-1)).empty());   // no buff → no label
        CHECK(std::string(zomboid::powerupName(10)).empty());   // out of range → no label
        std::set<std::string> buffs;
        for (int b = 0; b <= 9; ++b) {
            std::string nm = zomboid::powerupName(b);
            CHECK(!nm.empty());              // every buff names itself
            buffs.insert(nm);
        }
        CHECK(buffs.size() == 10);           // all ten buff names are distinct

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

        // Heal is capped at max health — but the surplus is banked as bonus armor, never wasted.
        sField(survivor, "health")->number = sField(survivor, "max_health")->number - 5.0;
        sField(survivor, "armor")->number = 0.0;
        const double cap = sField(survivor, "max_health")->number;
        std::vector<Value> at2 = {Value::fromNum(survivor->x()), Value::fromNum(survivor->y())};
        tree.scripts().vm().call("drop_medkit", at2);
        tree.process(0.016);
        CHECK(sField(survivor, "health")->number == cap); // not over max
        CHECK(sField(survivor, "armor")->number == 35.0); // the 40-heal kit: 5 filled health, 35 -> armor

        // An ignored medkit expires. Drive the kit's own _process in isolation so the assertion is
        // about its lifetime alone, not the surrounding sim (which can drop fresh kits over 15 s).
        survivor->setPosition(0.0, 0.0);
        std::vector<Value> far = {Value::fromNum(500.0), Value::fromNum(500.0)};
        tree.scripts().vm().call("drop_medkit", far);
        CHECK(activeMedkits(tree) == 1);
        SceneNode* expiring = nullptr;
        for (SceneNode* m : tree.nodesInGroup("medkits"))
            if (m->script().instance->findField("active")->boolean) expiring = m;
        CHECK(expiring != nullptr);
        Value kv = expiring->script();
        std::vector<Value> dtk = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 900; ++i) tree.scripts().vm().callOn(kv, "_process", dtk); // > 12 s max_life
        CHECK(!expiring->script().instance->findField("active")->boolean);
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
    // Adrenaline medkit reach: a critically wounded survivor (last-stand adrenaline) pulls in a medkit
    // from twice the normal magnet range — the lifeline finds you in the scramble. A healthy survivor
    // leaves the same kit lying just outside the normal 6-unit magnet.
    {
        // Wounded case: kit at 9 units (outside the normal 6-unit magnet, inside the 12-unit adrenaline
        // reach) drifts toward a survivor under 25% health.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 15.0;      // 15/100 → critically wounded
        sField(survivor, "adrenaline")->boolean = true; // last-stand surge engaged
        std::vector<Value> at = {Value::fromNum(9.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_medkit", at);
        SceneNode* kit = nullptr;
        for (SceneNode* m : tree.nodesInGroup("medkits"))
            if (m->script().instance->findField("active")->boolean) kit = m;
        const double x0 = kit->x();
        tree.process(1.0 / 60.0);
        CHECK(sField(kit, "active")->boolean);   // not yet picked up at 9 units
        CHECK(kit->x() < x0);                     // the adrenaline reach pulled it in
    }
    {
        // Control: a full-health survivor leaves the same 9-unit kit untouched (normal magnet is 6).
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);   // full health, no adrenaline
        std::vector<Value> at = {Value::fromNum(9.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_medkit", at);
        SceneNode* kit = nullptr;
        for (SceneNode* m : tree.nodesInGroup("medkits"))
            if (m->script().instance->findField("active")->boolean) kit = m;
        const double x0 = kit->x();
        tree.process(1.0 / 60.0);
        CHECK(kit->x() == x0);   // stayed put — outside the normal magnet, no adrenaline reach
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

    // Last-stand dodge recharge: while critically wounded (adrenaline), the dodge cooldown recharges
    // faster, so the same elapsed time leaves a critical survivor with a readier roll than a healthy one.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Critical survivor: adrenaline on → the dodge cools down faster.
        SceneTree tLow;
        SceneNode* sLow = zomboid::buildScene(tLow);
        auto& vmL = tLow.scripts().vm();
        Value svL = sLow->script();
        sField(sLow, "health")->number = sField(sLow, "max_health")->number * 0.1;  // critically wounded
        sField(sLow, "dash_cd")->number = 4.0;
        for (int i = 0; i < 30; ++i) vmL.callOn(svL, "_process", dt);
        const double lowCd = sField(sLow, "dash_cd")->number;
        CHECK(sField(sLow, "adrenaline")->boolean);   // last-stand active

        // Healthy survivor: same starting cooldown, same frames, normal recharge.
        SceneTree tHi;
        SceneNode* sHi = zomboid::buildScene(tHi);
        auto& vmH = tHi.scripts().vm();
        Value svH = sHi->script();
        sField(sHi, "dash_cd")->number = 4.0;
        for (int i = 0; i < 30; ++i) vmH.callOn(svH, "_process", dt);
        const double hiCd = sField(sHi, "dash_cd")->number;
        CHECK(!sField(sHi, "adrenaline")->boolean);

        CHECK(lowCd < hiCd);   // the critical survivor's dodge recharged faster
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
        const double food0 = sField(survivor, "food")->number;

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
        CHECK(sField(survivor, "food")->number == food0 + 2.0);                 // +2 rations — the only
                                                                               // renewable food source, so
                                                                               // hunger stays answerable
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

    // A mine's blast is volatile enough to flash over caustic acid, just like a naked flame: a puddle
    // caught in the mine's blast radius combusts, while one well outside the radius is left untouched.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0);   // keep the survivor clear of the blast
        auto& vm = tree.scripts().vm();
        SceneNode* mine = tree.findNode("Mine0");
        Value mv = mine->script();
        std::vector<Value> origin = {Value::fromNum(0.0), Value::fromNum(0.0)};
        vm.callOn(mv, "arm", origin);                       // places the mine at the origin

        SceneNode* nearAcid = tree.findNode("Acid0");
        SceneNode* farAcid = tree.findNode("Acid1");
        Value nav = nearAcid->script();
        Value fav = farAcid->script();
        std::vector<Value> near = {Value::fromNum(3.0), Value::fromNum(0.0)};    // inside blast radius 6
        std::vector<Value> far = {Value::fromNum(30.0), Value::fromNum(0.0)};    // well outside
        vm.callOn(nav, "splat_at", near);
        vm.callOn(fav, "splat_at", far);
        CHECK(sField(nearAcid, "active")->boolean);
        CHECK(sField(farAcid, "active")->boolean);

        std::vector<Value> noargs;
        vm.callOn(mv, "detonate", noargs);              // set off the mine
        CHECK(!sField(nearAcid, "active")->boolean);    // flashed over by the blast
        CHECK(sField(farAcid, "active")->boolean);      // out of range — still a puddle
    }

    // Mine cluster-aware trigger: a single zombie merely clipping the trigger ring (but not point-blank)
    // does NOT waste the mine — it holds. A second zombie entering the ring makes a worthwhile cluster and
    // it detonates at once.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0);            // keep the survivor clear
        auto& vm = tree.scripts().vm();

        SceneNode* mineN = tree.findNode("Mine0");
        Value mv = mineN->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        vm.callOn(mv, "arm", at);
        sField(mineN, "arm_delay")->number = 0.0;        // skip the safety fuse

        // One zombie in the trigger ring (radius 3) but outside point-blank (1.5): the mine holds.
        Value z0 = tree.findNode("Zombie0")->script();
        std::vector<Value> s0 = {Value::fromNum(2.5), Value::fromNum(0.0), Value::fromNum(200.0),
                                 Value::fromNum(0.0)};   // spawn_at(x,y,hp,spd)
        vm.callOn(z0, "spawn_at", s0);
        vm.callOn(mv, "_process", dt);
        CHECK(sField(mineN, "active")->boolean);         // lone edge straggler — not wasted

        // A second zombie joins the ring → a cluster worth catching → it detonates now.
        Value z1 = tree.findNode("Zombie1")->script();
        std::vector<Value> s1 = {Value::fromNum(2.5), Value::fromNum(0.6), Value::fromNum(200.0),
                                 Value::fromNum(0.0)};
        vm.callOn(z1, "spawn_at", s1);
        const double z0hp = sField(tree.findNode("Zombie0"), "health")->number;
        vm.callOn(mv, "_process", dt);
        CHECK(!sField(mineN, "active")->boolean);         // cluster caught — detonated
        CHECK(sField(tree.findNode("Zombie0"), "health")->number < z0hp);  // both in the blast
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

    // Sentry threat-priority targeting: a sentry's magazine is scarce, so it focus-fires the highest-
    // threat zombie in range (via threat_of) rather than whatever body is merely nearest — a summoner or
    // healer outranks a walker even when the walker is closer. Among equal-threat targets, the nearest is
    // taken first. This locks in threat_of's hand-maintained priority table and the tie-break, the
    // sentry's headline behavior, which the range/lifetime test above does not exercise.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        std::vector<Value> none;
        vm.callOn(sv, "place_sentry", none);
        SceneNode* sen = tree.findNode("Sentry0");
        Value senv = sen->script();
        sen->setPosition(0.0, 0.0);
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};

        // A low-threat walker (kind 0) parked CLOSE, and a high-threat summoner (kind 7) parked FARTHER —
        // both well inside the sentry's 16-unit range.
        SceneNode* nearWalker = tree.findNode("Zombie0");
        SceneNode* farSummoner = tree.findNode("Zombie1");
        Value nwS = nearWalker->script();
        Value fsS = farSummoner->script();
        std::vector<Value> nearAt = {Value::fromNum(4.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                     Value::fromNum(0.0)};
        std::vector<Value> farAt = {Value::fromNum(11.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                    Value::fromNum(0.0)};
        vm.callOn(nwS, "spawn_at", nearAt);
        vm.callOn(fsS, "spawn_at", farAt);
        sField(nearWalker, "kind")->number = 0.0;   // walker  — threat 10
        sField(farSummoner, "kind")->number = 7.0;  // summoner — threat 80
        const double nearHp0 = sField(nearWalker, "health")->number;
        const double farHp0 = sField(farSummoner, "health")->number;

        for (int i = 0; i < 60; ++i) vm.callOn(senv, "_process", dtv);
        // The far summoner is bleeding despite the walker being closer; the near walker is untouched.
        CHECK(sField(farSummoner, "health")->number < farHp0);
        CHECK(sField(nearWalker, "health")->number == nearHp0);

        // Tie-break: with two EQUAL-threat walkers in range, the nearer one is shot first.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        Value sv2 = surv2->script();
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        vm2.callOn(sv2, "place_sentry", none);
        SceneNode* sen2 = t2.findNode("Sentry0");
        Value sen2v = sen2->script();
        sen2->setPosition(0.0, 0.0);
        SceneNode* nearW = t2.findNode("Zombie0");
        SceneNode* farW = t2.findNode("Zombie1");
        Value nwv = nearW->script();
        Value fwv = farW->script();
        std::vector<Value> nAt = {Value::fromNum(4.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                  Value::fromNum(0.0)};
        std::vector<Value> fAt = {Value::fromNum(11.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                  Value::fromNum(0.0)};
        vm2.callOn(nwv, "spawn_at", nAt);
        vm2.callOn(fwv, "spawn_at", fAt);
        sField(nearW, "kind")->number = 0.0;   // both plain walkers — identical threat
        sField(farW, "kind")->number = 0.0;
        const double nW0 = sField(nearW, "health")->number;
        const double fW0 = sField(farW, "health")->number;
        for (int i = 0; i < 60; ++i) vm2.callOn(sen2v, "_process", dtv);
        CHECK(sField(nearW, "health")->number < nW0);   // nearer of the like pair takes fire
        CHECK(sField(farW, "health")->number == fW0);   // farther like target waits its turn
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

    // Exploder blast sets off the environment: a dying exploder cooks off a barrel and flashes over a
    // caustic puddle in range — the same hard-blast chain the player's mines/grenades/barrels trigger —
    // while ones out of range are untouched.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0);   // keep the survivor clear of the blast
        auto& vm = tree.scripts().vm();

        SceneNode* ex = tree.findNode("Zombie0");
        Value exs = ex->script();
        std::vector<Value> espawn = {Value::fromNum(0.0), Value::fromNum(0.0), Value::fromNum(4.0),
                                     Value::fromNum(1.0)}; // exploder (kind 4) at the origin
        vm.callOn(exs, "spawn", espawn);

        SceneNode* nearBarrel = tree.findNode("Barrel0");
        SceneNode* farBarrel = tree.findNode("Barrel1");
        Value nbv = nearBarrel->script();
        Value fbv = farBarrel->script();
        std::vector<Value> nbAt = {Value::fromNum(3.0), Value::fromNum(0.0)};   // inside blast radius 5
        std::vector<Value> fbAt = {Value::fromNum(60.0), Value::fromNum(0.0)};  // well outside
        vm.callOn(nbv, "place", nbAt);
        vm.callOn(fbv, "place", fbAt);
        SceneNode* nearAcid = tree.findNode("Acid0");
        SceneNode* farAcid = tree.findNode("Acid1");
        Value nav = nearAcid->script();
        Value fav = farAcid->script();
        std::vector<Value> naAt = {Value::fromNum(0.0), Value::fromNum(3.0)};   // inside blast radius
        std::vector<Value> faAt = {Value::fromNum(0.0), Value::fromNum(60.0)};  // well outside
        vm.callOn(nav, "splat_at", naAt);
        vm.callOn(fav, "splat_at", faAt);
        CHECK(sField(nearBarrel, "active")->boolean);
        CHECK(sField(nearAcid, "active")->boolean);

        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        vm.callOn(exs, "take_damage", lethal);                     // detonate the exploder
        CHECK(!sField(nearBarrel, "active")->boolean);             // barrel cooked off
        CHECK(sField(farBarrel, "active")->boolean);               // out of range — intact
        CHECK(!sField(nearAcid, "active")->boolean);               // puddle flashed over
        CHECK(sField(farAcid, "active")->boolean);                 // out of range — still a puddle
    }

    // Molotov: thrown ahead of the survivor, it leaves a burning patch that ignites zombies standing
    // in it. Consumes one from the stock; a supply crate replenishes it.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "aim_x")->number = 1.0;   // aim +x → lands ~9 units to the right
        sField(survivor, "aim_y")->number = 0.0;
        CHECK((int)tree.nodesInGroup("fires").size() == zomboid::kFirePool);
        const double stock0 = sField(survivor, "molotovs")->number;
        CHECK(stock0 >= 1.0);

        // Park a tanky zombie at the landing point (x=9), out of biting range irrelevant here.
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> at = {Value::fromNum(9.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                 Value::fromNum(0.0)};
        tree.scripts().vm().callOn(zs, "spawn_at", at);

        // Throw the molotov.
        std::vector<Value> none;
        Value thrown = tree.scripts().vm().callOn(sv, "throw_molotov", none);
        CHECK(thrown.boolean);
        CHECK(sField(survivor, "molotovs")->number == stock0 - 1.0);
        int firesActive = 0;
        for (SceneNode* f : tree.nodesInGroup("fires"))
            if (f->script().instance->findField("active")->boolean) ++firesActive;
        CHECK(firesActive == 1);

        // Drive the fire patch for a moment: the zombie in it gets ignited, then burns.
        SceneNode* fire = tree.findNode("Fire0");
        Value fv = fire->script();
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        tree.scripts().vm().callOn(fv, "_process", dtv);
        CHECK(z->script().instance->findField("burn_timer")->number > 0.0);   // caught in the flames
        const double hp0 = z->script().instance->findField("health")->number;
        for (int i = 0; i < 60; ++i) tree.scripts().vm().callOn(zs, "_process", dtv);
        CHECK(z->script().instance->findField("health")->number < hp0);       // burned by the patch

        // The patch burns out after its lifetime (~5 s).
        for (int i = 0; i < 60 * 6; ++i) tree.scripts().vm().callOn(fv, "_process", dtv);
        CHECK(!fire->script().instance->findField("active")->boolean);

        // A supply crate replenishes a molotov.
        survivor->setPosition(0.0, 0.0);
        const double stock1 = sField(survivor, "molotovs")->number;
        std::vector<Value> catv = {Value::fromNum(0.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_crate", catv);
        tree.process(1.0 / 60.0);
        CHECK(sField(survivor, "molotovs")->number == stock1 + 1.0);
    }

    // Summoner (kind 7): a support zombie that periodically calls reinforcement walkers, bounded by a
    // finite budget so it can't spawn forever. Killing it stops the reinforcements.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        CHECK(aliveZombies(tree) == 0);

        // Spawn a lone summoner far from the survivor (so summons don't immediately reach it).
        SceneNode* sm = tree.findNode("Zombie0");
        Value smv = sm->script();
        std::vector<Value> spawn = {Value::fromNum(40.0), Value::fromNum(0.0), Value::fromNum(7.0),
                                    Value::fromNum(7.0)}; // spawn(x,y,kind=7,wave=7)
        tree.scripts().vm().callOn(smv, "spawn", spawn);
        CHECK((int)sm->script().instance->findField("kind")->number == 7);
        CHECK(sm->script().instance->findField("summon_budget")->number == 6.0);
        CHECK(aliveZombies(tree) == 1);

        // Drive the summoner's _process past its first summon timer (~4 s): a reinforcement appears.
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 60 * 5; ++i) tree.scripts().vm().callOn(smv, "_process", dtv);
        CHECK(aliveZombies(tree) >= 2);   // at least one walker summoned
        CHECK(sm->script().instance->findField("summon_budget")->number < 6.0); // budget consumed

        // Exhaust the budget: it stops summoning once spent (bounded, no infinite spawns).
        for (int i = 0; i < 60 * 30; ++i) tree.scripts().vm().callOn(smv, "_process", dtv);
        CHECK(sm->script().instance->findField("summon_budget")->number == 0.0);
        const int capped = aliveZombies(tree);
        for (int i = 0; i < 60 * 10; ++i) tree.scripts().vm().callOn(smv, "_process", dtv);
        CHECK(aliveZombies(tree) == capped);   // no more reinforcements after the budget is spent
    }

    // Summoner reinforcement scaling: an early (wave < 5) summoner calls fodder walkers, but a mid/late
    // summoner (wave 5+) calls faster runners instead, so it stays a threat deep into a run.
    {
        // Late summoner → runner reinforcement.
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* sm = tree.findNode("Zombie0");
        Value smv = sm->script();
        std::vector<Value> lateSpawn = {Value::fromNum(40.0), Value::fromNum(0.0),
                                        Value::fromNum(7.0), Value::fromNum(7.0)}; // wave 7 summoner
        vm.callOn(smv, "spawn", lateSpawn);
        std::vector<Value> one = {Value::fromNum(1.0)};
        vm.callOn(smv, "summon", one);
        CHECK(sField(tree.findNode("Zombie1"), "alive")->boolean);
        CHECK((int)sField(tree.findNode("Zombie1"), "kind")->number == 1);  // a runner, not a walker

        // Early summoner → walker reinforcement (control).
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        SceneNode* sm2 = t2.findNode("Zombie0");
        Value sm2v = sm2->script();
        std::vector<Value> earlySpawn = {Value::fromNum(40.0), Value::fromNum(0.0),
                                         Value::fromNum(7.0), Value::fromNum(1.0)}; // wave 1 summoner
        vm2.callOn(sm2v, "spawn", earlySpawn);
        vm2.callOn(sm2v, "summon", one);
        CHECK(sField(t2.findNode("Zombie1"), "alive")->boolean);
        CHECK((int)sField(t2.findNode("Zombie1"), "kind")->number == 0);   // a plain walker
    }

    // Ammo drop: a pooled ammo box refills the active weapon's reserve (and a little for the rest) on
    // pickup, and drifts toward a nearby survivor via magnetism.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        Value sv = survivor->script();
        survivor->setPosition(0.0, 0.0);
        CHECK((int)tree.nodesInGroup("ammo").size() == zomboid::kAmmoPool);

        // Drain the pistol's (weapon 0) reserve so the top-up is visible.
        (*sField(survivor, "reserves")->array)[0].number = 0.0;
        (*sField(survivor, "reserves")->array)[2].number = 0.0;

        // Drop an ammo box right on the survivor.
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_ammo", at);
        int active = 0;
        for (SceneNode* a : tree.nodesInGroup("ammo"))
            if (a->script().instance->findField("active")->boolean) ++active;
        CHECK(active == 1);

        tree.process(1.0 / 60.0);   // survivor is on the box → collected
        CHECK((*sField(survivor, "reserves")->array)[0].number > 0.0);  // active weapon topped up
        CHECK((*sField(survivor, "reserves")->array)[2].number > 0.0);  // others get a little too

        // Magnetism: a box dropped a few units away drifts toward the survivor over a second.
        std::vector<Value> at2 = {Value::fromNum(5.0), Value::fromNum(0.0)};
        tree.scripts().vm().call("drop_ammo", at2);
        SceneNode* box = nullptr;
        for (SceneNode* a : tree.nodesInGroup("ammo"))
            if (a->script().instance->findField("active")->boolean) box = a;
        CHECK(box != nullptr);
        const double bx0 = box->x();
        Value bv = box->script();
        std::vector<Value> dtv = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 20; ++i) tree.scripts().vm().callOn(bv, "_process", dtv);
        CHECK(box->x() < bx0);   // drifted toward the survivor at the origin
    }

    // Wave-clear bonus: clearing a wave (once one has started) awards a score bonus scaling with it,
    // and only once per wave.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        tree.process(1.0 / 60.0);                 // Director opens wave 1
        CHECK((int)glob(tree, "g_wave") == 1);
        CHECK(aliveZombies(tree) > 0);
        const double score0 = glob(tree, "g_score");

        // Take a hit so this is a normal (non-flawless) clear — the flawless bonus is covered elsewhere.
        Value pv = survivor->script();
        std::vector<Value> ph = {Value::fromNum(5.0)};
        tree.scripts().vm().callOn(pv, "take_damage", ph);

        // Wipe the field, then a step with the field empty pays the wave-1 bonus (1 * 50).
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            Value zv = z->script();
            std::vector<Value> big = {Value::fromNum(9999.0)};
            if (z->script().instance->findField("alive")->boolean)
                tree.scripts().vm().callOn(zv, "take_damage", big);
        }
        // Kills also add score, so measure the bonus via the Director's own field.
        SceneNode* dir = tree.findNode("Director");
        Value dv = dir->script();
        std::vector<Value> dt1 = {Value::fromNum(1.0 / 60.0)};
        tree.scripts().vm().callOn(dv, "_process", dt1);
        CHECK((int)dir->script().instance->findField("bonus_wave")->number == 1);
        CHECK((int)dir->script().instance->findField("last_bonus")->number == 50);
        const double afterBonus = glob(tree, "g_score");
        CHECK(afterBonus >= score0 + 50.0);

        // A second empty-field step does NOT pay again (bonus is once-per-wave).
        tree.scripts().vm().callOn(dv, "_process", dt1);
        CHECK(glob(tree, "g_score") == afterBonus);
    }

    // Day/night danger ramp. The horde's aggression must track the on-screen darkness: flat at 1.0
    // through the daylit first half, then climbing across the night half to ~1.7 at the darkest hour
    // before dawn (danger() scales both horde speed and bite). Regression guard: the original cosine
    // peaked at dusk in full daylight and eased off as the screen got darkest — the exact reverse of
    // the intent, which made deep night the SAFEST time. These checks pin the corrected ramp.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        const double dayLen = glob(tree, "g_day_len");
        Value* phase = const_cast<Value*>(vm.getGlobal("g_phase"));
        std::vector<Value> none;

        phase->number = 0.0;                       // midday / start of the daylit half
        const double dMidday = vm.call("danger", none).number;
        phase->number = dayLen * 0.25;             // still broad daylight
        const double dDay = vm.call("danger", none).number;
        phase->number = dayLen * 0.5;              // dusk: night is just beginning
        const double dDusk = vm.call("danger", none).number;
        phase->number = dayLen * 0.75;             // deep night
        const double dNight = vm.call("danger", none).number;
        phase->number = dayLen * 0.99;             // the darkest hour, right before dawn
        const double dDark = vm.call("danger", none).number;

        CHECK(dMidday > 0.99 && dMidday < 1.01);   // ~1.0 by day
        CHECK(dDay > 0.99 && dDay < 1.01);         // the entire first half stays calm daylight
        CHECK(dDusk > 0.99 && dDusk < 1.01);       // aggression only starts to climb once night falls
        CHECK(dNight > dDusk);                      // ...then rises monotonically through the night
        CHECK(dDark > dNight);
        CHECK(dDark > 1.68 && dDark < 1.71);       // ~1.7 at the darkest hour
        CHECK(dDark > dMidday);                     // core fix: the dead of night is deadlier than noon
        phase->number = 0.0;                        // restore
    }

    // Explosive barrels: live from the start, they detonate when shot (or chipped to zero hull),
    // blasting + igniting nearby zombies, and chain-react to neighbouring barrels.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        CHECK((int)tree.nodesInGroup("barrels").size() == zomboid::kBarrelPool);
        int liveBarrels = 0;
        for (SceneNode* b : tree.nodesInGroup("barrels"))
            if (b->script().instance->findField("active")->boolean) ++liveBarrels;
        CHECK(liveBarrels == zomboid::kBarrelPool);   // all start active

        // Move one barrel to a known spot, park a tanky zombie next to it, then detonate the barrel.
        SceneNode* barrel = tree.findNode("Barrel0");
        Value bv = barrel->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        tree.scripts().vm().callOn(bv, "place", at);

        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> zat = {Value::fromNum(3.0), Value::fromNum(0.0), Value::fromNum(500.0),
                                  Value::fromNum(0.0)};
        tree.scripts().vm().callOn(zs, "spawn_at", zat);
        const double zhp0 = z->script().instance->findField("health")->number;

        // A chipping hit that doesn't reach zero leaves it intact; a lethal hit pops it.
        std::vector<Value> small = {Value::fromNum(10.0)};
        tree.scripts().vm().callOn(bv, "take_damage", small);
        CHECK(barrel->script().instance->findField("active")->boolean);   // 30-10 > 0, still standing
        std::vector<Value> big = {Value::fromNum(99.0)};
        tree.scripts().vm().callOn(bv, "take_damage", big);
        CHECK(!barrel->script().instance->findField("active")->boolean);  // detonated

        // The nearby zombie was blasted and set alight.
        CHECK(z->script().instance->findField("health")->number < zhp0);
        CHECK(z->script().instance->findField("burn_timer")->number > 0.0);
    }

    // Barrels replenish between waves: pop every barrel, then start a new wave — a couple are restored so
    // the environmental-kill playstyle stays alive through an endless run instead of drying up for good.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();

        // Detonate every barrel — the arena is now bare.
        std::vector<Value> lethal = {Value::fromNum(999.0)};
        for (SceneNode* b : tree.nodesInGroup("barrels")) {
            Value bv = b->script();
            vm.callOn(bv, "take_damage", lethal);
        }
        int liveAfterClear = 0;
        for (SceneNode* b : tree.nodesInGroup("barrels"))
            if (sField(b, "active")->boolean) ++liveAfterClear;
        CHECK(liveAfterClear == 0);   // all spent

        // Start a new wave (wave 3): the Director restores a couple of barrels.
        SceneNode* dir = tree.findNode("Director");
        Value ds = dir->script();
        std::vector<Value> w3 = {Value::fromNum(3.0)};
        vm.callOn(ds, "start_wave", w3);
        int liveAfterWave = 0;
        for (SceneNode* b : tree.nodesInGroup("barrels"))
            if (sField(b, "active")->boolean) ++liveAfterWave;
        CHECK(liveAfterWave == 2);   // two barrels replenished for the new wave
    }

    // Armored zombie (kind 8): a shield soaks damage before health; only overflow past a broken
    // shield bleeds through, so it must be worn down before it can be killed.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> spawn = {Value::fromNum(5.0), Value::fromNum(0.0), Value::fromNum(8.0),
                                    Value::fromNum(1.0)}; // spawn(x,y,kind=8,wave=1)
        tree.scripts().vm().callOn(zs, "spawn", spawn);
        CHECK((int)z->script().instance->findField("kind")->number == 8);
        const double shield0 = z->script().instance->findField("shield")->number;
        const double hp0 = z->script().instance->findField("health")->number;
        CHECK(shield0 > 0.0);

        // A hit smaller than the shield is fully absorbed: shield drops, health untouched.
        std::vector<Value> hit = {Value::fromNum(20.0)};
        tree.scripts().vm().callOn(zs, "take_damage", hit);
        CHECK(z->script().instance->findField("shield")->number == shield0 - 20.0);
        CHECK(z->script().instance->findField("health")->number == hp0);   // shield ate it

        // A blow that exceeds the remaining shield breaks it and the overflow bleeds into health.
        const double remaining = z->script().instance->findField("shield")->number;
        std::vector<Value> big = {Value::fromNum(remaining + 5.0)};
        tree.scripts().vm().callOn(zs, "take_damage", big);
        CHECK(z->script().instance->findField("shield")->number == 0.0);      // broken
        CHECK(z->script().instance->findField("health")->number == hp0 - 5.0); // 5 overflow through

        // With the shield gone, further hits damage health directly.
        tree.scripts().vm().callOn(zs, "take_damage", hit);
        CHECK(z->script().instance->findField("health")->number == hp0 - 5.0 - 20.0);
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

    // Savage Horde (mutator 8) makes the exploder's blast hit 60% harder too. The exploder detonates for
    // a flat amount rather than biting for its (Savage-scaled) contact damage, so without this it would be
    // the one enemy a Savage wave left untouched. Measure the survivor's health drop from a point-blank
    // detonation with the mutator off vs on: the Savage blast must be 1.6x the baseline.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        Value* mut = const_cast<Value*>(vm.getGlobal("g_mutator"));

        std::vector<Value> spawn = {Value::fromNum(2.0), Value::fromNum(0.0), Value::fromNum(4.0),
                                    Value::fromNum(1.0)}; // exploder at (2,0), in blast range of the origin
        std::vector<Value> kill = {Value::fromNum(999.0)};

        // Baseline: no mutator.
        SceneNode* ez = tree.findNode("Zombie0");
        Value ezs = ez->script();
        mut->number = 0.0;
        vm.callOn(ezs, "spawn", spawn);
        sField(survivor, "health")->number = 100.0;
        sField(survivor, "iframes")->number = 0.0;
        vm.callOn(ezs, "take_damage", kill);
        const double baseHit = 100.0 - sField(survivor, "health")->number;   // == 35
        CHECK(baseHit > 0.0);

        // Savage: same setup, mutator on.
        SceneNode* ez2 = tree.findNode("Zombie1");
        Value ez2s = ez2->script();
        mut->number = 8.0;
        vm.callOn(ez2s, "spawn", spawn);
        sField(survivor, "health")->number = 100.0;
        sField(survivor, "iframes")->number = 0.0;
        vm.callOn(ez2s, "take_damage", kill);
        const double savageHit = 100.0 - sField(survivor, "health")->number; // == 56

        CHECK(std::abs(savageHit - baseHit * 1.6) < 1e-6);   // Savage blast is exactly 1.6x the baseline
        mut->number = 0.0;
    }

    // Bloodthirsty Horde (mutator 9): a zombie that bites the survivor SIPHONS life from the wound,
    // healing itself — so you can't win a Bloodthirsty wave by trading hits. Verify a wounded biter
    // gains health on a bite while the mutator is on, and a control with the mutator off does not.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        Value* mut = const_cast<Value*>(vm.getGlobal("g_mutator"));
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        SceneNode* z = tree.findNode("Zombie0");
        Value zv = z->script();
        std::vector<Value> at = {Value::fromNum(1.0), Value::fromNum(0.0), Value::fromNum(100.0),
                                 Value::fromNum(0.0)};   // a plain walker 1 unit away — inside bite range 1.2
        vm.callOn(zv, "spawn_at", at);
        sField(z, "kind")->number = 0.0;      // walker: no telegraph, bites on contact
        sField(z, "health")->number = 50.0;   // wounded, so a heal is visible (max stays 100)
        sField(z, "cooldown")->number = 0.0;   // ready to bite this frame

        mut->number = 9.0;                     // Bloodthirsty
        const double survHp0 = sField(survivor, "health")->number;
        vm.callOn(zv, "_process", dt);         // it bites
        CHECK(sField(survivor, "health")->number < survHp0);   // the bite still wounds the survivor
        CHECK(sField(z, "health")->number > 50.0);             // ...and the biter leeched health back

        // Control: mutator off → a bite heals the biter nothing.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_mutator"))->number = 0.0;
        SceneNode* z2 = t2.findNode("Zombie0");
        Value z2v = z2->script();
        vm2.callOn(z2v, "spawn_at", at);
        sField(z2, "kind")->number = 0.0;
        sField(z2, "health")->number = 50.0;
        sField(z2, "cooldown")->number = 0.0;
        vm2.callOn(z2v, "_process", dt);
        CHECK(sField(z2, "health")->number == 50.0);   // no mutator → no leech
        mut->number = 0.0;

        // Dodge denies the leech: a bite the survivor DODGES (i-frames up) draws no blood, so under
        // Bloodthirsty the biter heals nothing — dodge stays real counterplay, not a leak that still feeds
        // the horde. (Same for a Shield power-up soaking the bite; both are the take_damage no-op cases.)
        SceneTree t3;
        SceneNode* surv3 = zomboid::buildScene(t3);
        surv3->setPosition(0.0, 0.0);
        auto& vm3 = t3.scripts().vm();
        const_cast<Value*>(vm3.getGlobal("g_mutator"))->number = 9.0;
        SceneNode* z3 = t3.findNode("Zombie0");
        Value z3v = z3->script();
        vm3.callOn(z3v, "spawn_at", at);
        sField(z3, "kind")->number = 0.0;
        sField(z3, "health")->number = 50.0;
        sField(z3, "cooldown")->number = 0.0;
        sField(surv3, "iframes")->number = 1.0;   // mid dodge-roll — untouchable
        vm3.callOn(z3v, "_process", dt);
        CHECK(sField(z3, "health")->number == 50.0);   // dodged bite drew no blood → no leech
        const_cast<Value*>(vm3.getGlobal("g_mutator"))->number = 0.0;

        // Boss exemption: the boss is excluded from Bloodthirsty like every other horde-wide modifier
        // (its duel is self-contained). A boss that bites the survivor under Bloodthirsty still deals its
        // hit but leeches nothing.
        SceneTree t4;
        SceneNode* surv4 = zomboid::buildScene(t4);
        surv4->setPosition(0.0, 0.0);
        auto& vm4 = t4.scripts().vm();
        const_cast<Value*>(vm4.getGlobal("g_mutator"))->number = 9.0;
        SceneNode* boss = t4.findNode("Zombie0");
        Value bossv = boss->script();
        std::vector<Value> bs = {Value::fromNum(2.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(1.0)};   // boss (kind 3), wave 1
        vm4.callOn(bossv, "spawn", bs);
        boss->setPosition(2.0, 0.0);               // inside the boss's 2.5 attack range, not enraged
        sField(boss, "health")->number = 300.0;    // wounded, so any wrongful heal would show
        sField(boss, "cooldown")->number = 0.0;    // ready to bite
        sField(boss, "slam_cd")->number = 99.0;    // suppress the slam so only the bite happens
        sField(boss, "slam_warn")->number = 0.0;
        const double bossSurvHp0 = sField(surv4, "health")->number;
        vm4.callOn(bossv, "_process", dt);
        CHECK(sField(surv4, "health")->number < bossSurvHp0);   // the boss bit the survivor
        CHECK(sField(boss, "health")->number == 300.0);         // ...but leeched nothing (boss exempt)
        const_cast<Value*>(vm4.getGlobal("g_mutator"))->number = 0.0;
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

    // Spitter maintains its firing distance: rushed to point-blank (well inside its ~7.8 hold band), it
    // backpedals to reopen a gap rather than letting the survivor sit on top of it — the same
    // hold-distance behaviour as the summoner and the rest of the back line.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> spawn = {Value::fromNum(5.0), Value::fromNum(0.0), Value::fromNum(5.0),
                                    Value::fromNum(1.0)}; // spitter 5 units away — inside the hold band
        vm.callOn(zs, "spawn", spawn);
        const double x0 = z->x();
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 20; ++i) { vm.callOn(zs, "_process", dt); }
        CHECK(z->x() > x0);   // backpedaled away to reopen its firing gap
    }

    // Spitter silenced by control: a chilled spitter can't lob a glob (frozen back-liners are silenced
    // like the casters), and neither can a staggered one — but once the effect clears it spits again.
    {
        // Chill case.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> spawn = {Value::fromNum(10.0), Value::fromNum(0.0), Value::fromNum(5.0),
                                    Value::fromNum(1.0)};
        vm.callOn(zs, "spawn", spawn);
        std::vector<Value> chill = {Value::fromNum(3.0)};
        vm.callOn(zs, "apply_slow", chill);      // freeze it before it can lob
        for (int i = 0; i < 10; ++i) tree.process(1.0 / 60.0);
        CHECK(activeSpits(tree) == 0);           // chilled → silenced, no glob
        sField(z, "slow_timer")->number = 0.0;   // thaw
        tree.process(0.05);
        CHECK(activeSpits(tree) >= 1);           // spits again once the chill clears

        // Stagger case (fresh scene).
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* z2 = t2.findNode("Zombie0");
        Value z2s = z2->script();
        vm2.callOn(z2s, "spawn", spawn);
        std::vector<Value> stag = {Value::fromNum(0.5)};
        vm2.callOn(z2s, "stagger", stag);        // root it mid-throw
        t2.process(1.0 / 60.0);
        CHECK(activeSpits(t2) == 0);             // staggered → can't lob
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

    // Relentless mutator (7): the whole horde ignores knockback — a shove that would move a normal walker
    // leaves a Relentless one planted. Clearing the mutator restores normal knockback.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value* mut = const_cast<Value*>(vm.getGlobal("g_mutator"));
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> park = {Value::fromNum(10.0), Value::fromNum(0.0), Value::fromNum(200.0),
                                   Value::fromNum(0.0)}; // walker
        vm.callOn(zs, "spawn_at", park);
        std::vector<Value> kb = {Value::fromNum(1.0), Value::fromNum(0.0), Value::fromNum(6.0)};

        // Relentless: the shove does nothing.
        mut->number = 7.0;
        const double rx0 = z->x();
        vm.callOn(zs, "hit_knockback", kb);
        CHECK(z->x() == rx0);       // planted — no knockback

        // Cleared: the same shove moves it.
        mut->number = 0.0;
        vm.callOn(zs, "hit_knockback", kb);
        CHECK(z->x() > rx0 + 1.0);  // shoved along +x again
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
        const double baseSpeed = z->script().instance->findField("speed")->number;

        std::vector<Value> none;
        tree.scripts().vm().callOn(zv, "make_elite", none);
        CHECK(z->script().instance->findField("elite")->boolean);
        CHECK(z->script().instance->findField("health")->number > baseHp * 2.0);      // tankier
        CHECK(z->script().instance->findField("score_value")->number > baseScore * 2.0); // worth more
        CHECK(z->script().instance->findField("speed")->number > baseSpeed);          // and faster
        CHECK(z->script().instance->findField("speed")->number <= 30.0);              // ...but speed-capped

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
        sField(boss, "slam_cd")->number = 0.01; // slam almost ready
        std::vector<Value> dt1 = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 45; ++i) { tree.scripts().vm().callOn(bz, "_process", dt1); } // wind-up then land
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
        sField(boss, "slam_cd")->number = 0.01;
        std::vector<Value> dt2 = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 45; ++i) { tree.scripts().vm().callOn(bz, "_process", dt2); } // fires, can't reach
        CHECK(sField(survivor, "health")->number == 100.0); // out of slam range, unscathed
    }

    // Boss slam respects i-frames: a survivor mid-dodge rides the slam out UNTOUCHED — no damage AND no
    // knockback — exactly like a brute's blow. Two runs at a fixed distance (boss pinned so the range
    // stays put): without i-frames the slam both hurts and hurls; with i-frames up neither lands.
    {
        // Baseline — no i-frames: the slam damages and knocks the survivor back.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        sField(survivor, "iframes")->number = 0.0;
        auto& vm = tree.scripts().vm();
        SceneNode* boss = tree.findNode("Zombie0");
        Value bz = boss->script();
        std::vector<Value> sp = {Value::fromNum(6.0), Value::fromNum(0.0), Value::fromNum(3.0),
                                 Value::fromNum(1.0)}; // boss at dist 6 (inside slam radius, outside bite)
        vm.callOn(bz, "spawn", sp);
        sField(boss, "speed")->number = 0.0;      // pin it so the slam distance stays fixed at 6
        sField(boss, "slam_cd")->number = 0.01;   // slam almost ready
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 45; ++i) { vm.callOn(bz, "_process", dt); }
        CHECK(sField(survivor, "health")->number <= 75.0);   // took the ~25 slam
        CHECK(survivor->x() < -1.0);                          // hurled away from the boss (toward -x)
    }
    {
        // Dodging — i-frames up: the slam is fully ridden out. Only the boss is processed, so the
        // survivor's i-frames never tick down and stay up across the whole wind-up.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 100.0;
        sField(survivor, "iframes")->number = 5.0;   // mid-dodge invulnerability
        auto& vm = tree.scripts().vm();
        SceneNode* boss = tree.findNode("Zombie0");
        Value bz = boss->script();
        std::vector<Value> sp = {Value::fromNum(6.0), Value::fromNum(0.0), Value::fromNum(3.0),
                                 Value::fromNum(1.0)};
        vm.callOn(bz, "spawn", sp);
        sField(boss, "speed")->number = 0.0;
        sField(boss, "slam_cd")->number = 0.01;
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 45; ++i) { vm.callOn(bz, "_process", dt); }
        CHECK(sField(survivor, "health")->number == 100.0);  // no damage — untouchable mid-roll
        CHECK(survivor->x() == 0.0);                          // no knockback either — rode it out
    }

    // Leaper pounce-vector is divide-by-zero safe: if the survivor is standing exactly on the coiled
    // (rooted) leaper when its wind-up completes, distance is zero. Without a floor, (dx/dist) would be
    // NaN and blow the leaper's position/velocity to NaN. Drive the coil to completion at zero range and
    // confirm the leaper's leap velocity and position stay finite.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* leaper = tree.findNode("Zombie0");
        Value lv = leaper->script();
        std::vector<Value> sp = {Value::fromNum(0.0), Value::fromNum(0.0), Value::fromNum(9.0),
                                 Value::fromNum(3.0)}; // leaper (kind 9) spawned ON the survivor
        vm.callOn(lv, "spawn", sp);
        leaper->setPosition(0.0, 0.0);               // exact overlap → zero distance to the survivor
        sField(leaper, "leap_wind")->number = 0.001; // one tick from committing the pounce
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        vm.callOn(lv, "_process", dt);               // wind-up completes → pounce vector computed at dist 0
        CHECK(std::isfinite(sField(leaper, "leap_vx")->number));  // not NaN/inf
        CHECK(std::isfinite(sField(leaper, "leap_vy")->number));
        // Fly out the whole pounce; the leaper's position must stay finite the entire time.
        for (int i = 0; i < 30; ++i) { vm.callOn(lv, "_process", dt); }
        CHECK(std::isfinite(leaper->x()));
        CHECK(std::isfinite(leaper->y()));
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
        // A too-tough body (2000 hp) survives the 500-damage blast — and is left deep-frozen by it.
        SceneNode* tank = tree.findNode("Zombie5");
        Value tv = tank->script();
        std::vector<Value> ta = {Value::fromNum(6.0), Value::fromNum(0.0),
                                 Value::fromNum(2000.0), Value::fromNum(0.0)};   // spawn_at, 2000 hp
        tree.scripts().vm().callOn(tv, "spawn_at", ta);
        CHECK(sField(tank, "slow_timer")->number == 0.0);    // not chilled before the blast

        CHECK(aliveZombies(tree) >= 6);
        std::vector<Value> none;
        tree.scripts().vm().callOn(sv, "detonate", none);
        CHECK(aliveZombies(tree) == 1);                      // field wiped but the 2000-hp body survives
        CHECK(sField(tank, "alive")->boolean);
        CHECK(sField(tank, "slow_timer")->number > 0.0);     // ...left cryo-locked by the overcharge
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

    // Piercing-rounds power-up (kind 3): a single bullet punches through a line of zombies, hitting
    // each once, where a normal bullet would stop at the first. Drives the spawned bullet's _process
    // directly so the line stays put (spd 0) and the outcome is deterministic.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;

        // Grant the piercing power-up and confirm the flag latched.
        Value sv = survivor->script();
        std::vector<Value> pk = {Value::fromNum(3.0)};
        tree.scripts().vm().callOn(sv, "grant_powerup", pk);
        CHECK(sField(survivor, "pierce_shots")->boolean);

        // Three tanky zombies parked in a straight line ahead (hp 200 so one hit can't kill them).
        SceneNode* zs[3] = {tree.findNode("Zombie0"), tree.findNode("Zombie1"), tree.findNode("Zombie2")};
        const double xs[3] = {4.0, 6.0, 8.0};
        for (int i = 0; i < 3; ++i) {
            Value zv = zs[i]->script();
            std::vector<Value> a = {Value::fromNum(xs[i]), Value::fromNum(0.0),
                                    Value::fromNum(200.0), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        }

        // Fire one shot, then find the spawned bullet and confirm it carries pierces.
        std::vector<Value> none;
        tree.scripts().vm().callOn(sv, "do_shoot", none);
        SceneNode* bullet = nullptr;
        for (SceneNode* b : tree.nodesInGroup("bullets"))
            if (b->script().instance->findField("active")->boolean) { bullet = b; break; }
        CHECK(bullet != nullptr);
        CHECK((int)sField(bullet, "pierce_left")->number == 2);

        // Fly the bullet forward; it should chip all three (1 initial hit + 2 pierces) then expire.
        Value bv = bullet->script();
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 60; ++i) tree.scripts().vm().callOn(bv, "_process", dt);
        for (int i = 0; i < 3; ++i)
            CHECK(sField(zs[i], "health")->number < 200.0); // every zombie in the line was struck
        CHECK(!sField(bullet, "active")->boolean);          // pierces spent, bullet consumed

        // Contrast: without the power-up, one bullet stops at the first body.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        sField(surv2, "aim_x")->number = 1.0;
        sField(surv2, "aim_y")->number = 0.0;
        SceneNode* zn[3] = {t2.findNode("Zombie0"), t2.findNode("Zombie1"), t2.findNode("Zombie2")};
        for (int i = 0; i < 3; ++i) {
            Value zv = zn[i]->script();
            std::vector<Value> a = {Value::fromNum(xs[i]), Value::fromNum(0.0),
                                    Value::fromNum(200.0), Value::fromNum(0.0)};
            t2.scripts().vm().callOn(zv, "spawn_at", a);
        }
        Value sv2 = surv2->script();
        t2.scripts().vm().callOn(sv2, "do_shoot", none);
        SceneNode* b2 = nullptr;
        for (SceneNode* b : t2.nodesInGroup("bullets"))
            if (b->script().instance->findField("active")->boolean) { b2 = b; break; }
        CHECK(b2 != nullptr);
        Value b2v = b2->script();
        for (int i = 0; i < 60; ++i) t2.scripts().vm().callOn(b2v, "_process", dt);
        CHECK(sField(zn[0], "health")->number < 200.0);  // first zombie struck
        CHECK(sField(zn[1], "health")->number == 200.0); // second untouched — bullet stopped
        CHECK(sField(zn[2], "health")->number == 200.0); // third untouched
    }

    // Leaper (kind 9): between pounces it walks, but on a ready cooldown at mid-range it lunges — a
    // fast burst that covers far more ground per frame than its walk. Player parked at the origin and
    // only the leaper is stepped, so the motion is deterministic and one-dimensional (both stay on y=0).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        Value* phase = const_cast<Value*>(vm.getGlobal("g_phase"));
        phase->number = 0.0;                          // dawn → danger()==1.0, so walk speed is exactly 12

        SceneNode* leaper = tree.findNode("Zombie0");
        Value lv = leaper->script();
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(9.0), Value::fromNum(5.0)}; // spawn(x,y,kind=9,wave=5)
        vm.callOn(lv, "spawn", sp);
        CHECK((int)sField(leaper, "kind")->number == 9);
        CHECK((int)sField(leaper, "score_value")->number == 16);

        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        bool leaped = false;
        double maxLeapStep = 0.0;  // largest single-frame move made while mid-pounce
        double maxWalkStep = 0.0;  // largest single-frame move made while walking
        double px = leaper->x();
        for (int i = 0; i < 900 && leaper->x() > 2.0; ++i) {
            const bool midLeap = sField(leaper, "leaping")->number > 0.0;
            vm.callOn(lv, "_process", dt);
            const double step = px - leaper->x();     // travels toward the origin, so px > new x
            px = leaper->x();
            if (midLeap) { leaped = true; if (step > maxLeapStep) maxLeapStep = step; }
            else if (step > maxWalkStep) { maxWalkStep = step; }
        }
        const double walkPerFrame = 12.0 / 60.0;      // 0.20 units/frame at danger()==1.0
        CHECK(leaped);                                 // a pounce actually fired
        CHECK(maxWalkStep <= walkPerFrame + 0.001);    // walking never exceeds the base walk step
        CHECK(maxLeapStep > walkPerFrame * 1.8);       // the pounce is markedly faster than a walk
    }

    // Leaper interrupt: a stagger landed during the pre-pounce coil breaks the leap outright — the
    // leaper uncoils without ever leaving the ground. A leaper left undisturbed through the wind-up
    // does pounce (control), so the interrupt is what makes the difference.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        const_cast<Value*>(vm.getGlobal("g_phase"))->number = 0.0;
        SceneNode* leaper = tree.findNode("Zombie0");
        Value lv = leaper->script();
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(9.0), Value::fromNum(5.0)}; // spawn(x,y,kind=9,wave=5)
        vm.callOn(lv, "spawn", sp);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Drive until it enters the coil (leap_wind > 0), before any pounce fires.
        for (int i = 0; i < 900 && sField(leaper, "leap_wind")->number <= 0.0; ++i)
            vm.callOn(lv, "_process", dt);
        CHECK(sField(leaper, "leap_wind")->number > 0.0);   // coiled and telegraphing
        CHECK(sField(leaper, "leaping")->number == 0.0);    // not yet airborne

        // Stagger it mid-coil, then step once: the pounce is interrupted, not merely delayed.
        std::vector<Value> stag = {Value::fromNum(0.5)};
        vm.callOn(lv, "stagger", stag);
        vm.callOn(lv, "_process", dt);
        CHECK(sField(leaper, "leap_wind")->number == 0.0);  // coil broken
        CHECK(sField(leaper, "leaping")->number == 0.0);    // never left the ground

        // Control: an uninterrupted leaper does pounce.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_phase"))->number = 0.0;
        SceneNode* lp2 = t2.findNode("Zombie0");
        Value l2 = lp2->script();
        vm2.callOn(l2, "spawn", sp);
        bool leaped2 = false;
        for (int i = 0; i < 900 && !leaped2; ++i) {
            vm2.callOn(l2, "_process", dt);
            if (sField(lp2, "leaping")->number > 0.0) leaped2 = true;
        }
        CHECK(leaped2);   // left undisturbed, the pounce fires
    }

    // Leaper interrupt — chill variant: the coil-break at game.hpp is `stagger OR chill`, and the
    // stagger half is covered above. A chill (slow_timer) landed DURING the coil must break the pounce
    // just the same — freezing a coiled leaper uncoils it harmlessly, denying the leap. This exercises
    // the slow_timer half of that OR, a distinct code path from the stagger test.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        const_cast<Value*>(vm.getGlobal("g_phase"))->number = 0.0;
        SceneNode* leaper = tree.findNode("Zombie0");
        Value lv = leaper->script();
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(9.0), Value::fromNum(5.0)}; // spawn(x,y,kind=9,wave=5)
        vm.callOn(lv, "spawn", sp);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Drive until it enters the coil (leap_wind > 0), before any pounce fires.
        for (int i = 0; i < 900 && sField(leaper, "leap_wind")->number <= 0.0; ++i)
            vm.callOn(lv, "_process", dt);
        CHECK(sField(leaper, "leap_wind")->number > 0.0);   // coiled and telegraphing
        CHECK(sField(leaper, "leaping")->number == 0.0);    // not yet airborne

        // Chill it mid-coil, then step once: the pounce is interrupted, not merely delayed.
        std::vector<Value> chill = {Value::fromNum(0.5)};
        vm.callOn(lv, "apply_slow", chill);
        vm.callOn(lv, "_process", dt);
        CHECK(sField(leaper, "leap_wind")->number == 0.0);  // coil broken by the chill
        CHECK(sField(leaper, "leaping")->number == 0.0);    // never left the ground
    }

    // Warper interrupt: a stagger during the blink tell cancels the teleport outright (warp_warn resets,
    // no blink). An uninterrupted warper does blink toward the survivor (control).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        const_cast<Value*>(vm.getGlobal("g_phase"))->number = 0.0;
        SceneNode* warper = tree.findNode("Zombie0");
        Value wv = warper->script();
        std::vector<Value> sp = {Value::fromNum(40.0), Value::fromNum(0.0),
                                 Value::fromNum(13.0), Value::fromNum(5.0)}; // spawn(x,y,kind=13,wave=5)
        vm.callOn(wv, "spawn", sp);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 900 && sField(warper, "warp_warn")->number <= 0.0; ++i)
            vm.callOn(wv, "_process", dt);
        CHECK(sField(warper, "warp_warn")->number > 0.0);   // shimmering — charging a blink
        const double xBefore = warper->x();
        std::vector<Value> stag = {Value::fromNum(0.5)};
        vm.callOn(wv, "stagger", stag);
        vm.callOn(wv, "_process", dt);
        CHECK(sField(warper, "warp_warn")->number == 0.0);  // tell cancelled
        CHECK(warper->x() > xBefore - 2.0);                 // did NOT blink ~half the distance inward

        // Control: an uninterrupted warper blinks toward the survivor (a single-frame jump).
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_phase"))->number = 0.0;
        SceneNode* w2 = t2.findNode("Zombie0");
        Value w2v = w2->script();
        vm2.callOn(w2v, "spawn", sp);
        bool blinked = false;
        double prevx = w2->x();
        for (int i = 0; i < 900 && !blinked; ++i) {
            vm2.callOn(w2v, "_process", dt);
            if (prevx - w2->x() > 3.0) blinked = true;   // a large single-frame jump = a blink
            prevx = w2->x();
        }
        CHECK(blinked);
    }

    // Warper cryo counter: chill is a HARD counter to the warper (docs call it out). It works two ways —
    // a chill landed DURING the blink tell cancels the teleport outright (same as a stagger), and a
    // chilled warper off-cooldown cannot even BEGIN to charge a blink. Only the stagger interrupt was
    // covered above; this pins both chill paths.
    {
        // (a) Chill mid-tell cancels the blink.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        const_cast<Value*>(vm.getGlobal("g_phase"))->number = 0.0;
        SceneNode* warper = tree.findNode("Zombie0");
        Value wv = warper->script();
        std::vector<Value> sp = {Value::fromNum(40.0), Value::fromNum(0.0),
                                 Value::fromNum(13.0), Value::fromNum(5.0)};
        vm.callOn(wv, "spawn", sp);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 900 && sField(warper, "warp_warn")->number <= 0.0; ++i)
            vm.callOn(wv, "_process", dt);
        CHECK(sField(warper, "warp_warn")->number > 0.0);   // charging a blink
        const double xBefore = warper->x();
        sField(warper, "slow_timer")->number = 3.0;         // cryo lands on the shimmer
        vm.callOn(wv, "_process", dt);
        CHECK(sField(warper, "warp_warn")->number == 0.0);  // blink cancelled by the chill
        CHECK(warper->x() > xBefore - 2.0);                 // did NOT jump ~half the distance inward

        // (b) A chilled warper off-cooldown never even starts a tell — it's pinned in place.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_phase"))->number = 0.0;
        SceneNode* w2 = t2.findNode("Zombie0");
        Value w2v = w2->script();
        vm2.callOn(w2v, "spawn", sp);   // spawns at x=40, far enough to want to blink
        bool everCharged = false;
        for (int i = 0; i < 300; ++i) {
            sField(w2, "slow_timer")->number = 1.0;   // keep it frozen every frame
            vm2.callOn(w2v, "_process", dt);
            if (sField(w2, "warp_warn")->number > 0.0) everCharged = true;
        }
        CHECK(!everCharged);   // frozen the whole time → never charged a blink
    }

    // Summoner interrupt: a stagger during the call wind-up fizzles the reinforcement — matching the way a
    // stagger already breaks a leaper's coil and a warper's blink. Previously only a chill could interrupt
    // a caster; a melee shove / dash-strike now works too. An undisturbed summoner completes the call.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        const_cast<Value*>(vm.getGlobal("g_phase"))->number = 0.0;
        SceneNode* summoner = tree.findNode("Zombie0");
        Value sv = summoner->script();
        std::vector<Value> sp = {Value::fromNum(60.0), Value::fromNum(0.0),
                                 Value::fromNum(7.0), Value::fromNum(5.0)}; // spawn(x,y,kind=7,wave=5)
        vm.callOn(sv, "spawn", sp);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Drive until it starts a call (summon_warn > 0), before the reinforcement lands.
        for (int i = 0; i < 900 && sField(summoner, "summon_warn")->number <= 0.0; ++i)
            vm.callOn(sv, "_process", dt);
        CHECK(sField(summoner, "summon_warn")->number > 0.0);      // telegraphing a call
        const double budget0 = sField(summoner, "summon_budget")->number;

        // Stagger it mid-tell, then step once: the call fizzles and no reinforcement is spent.
        std::vector<Value> stag = {Value::fromNum(0.5)};
        vm.callOn(sv, "stagger", stag);
        vm.callOn(sv, "_process", dt);
        CHECK(sField(summoner, "summon_warn")->number == 0.0);     // call cancelled
        CHECK(sField(summoner, "summon_budget")->number == budget0); // nothing summoned

        // Control: an undisturbed summoner completes a call (spends a reinforcement from its budget).
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_phase"))->number = 0.0;
        SceneNode* s2 = t2.findNode("Zombie0");
        Value s2v = s2->script();
        vm2.callOn(s2v, "spawn", sp);
        const double budget2 = sField(s2, "summon_budget")->number;
        bool summoned = false;
        for (int i = 0; i < 900 && !summoned; ++i) {
            vm2.callOn(s2v, "_process", dt);
            if (sField(s2, "summon_budget")->number < budget2) summoned = true;
        }
        CHECK(summoned);   // left undisturbed, the call goes through
    }

    // Flamethrower (weapon 4): a short cone of fire in front of the survivor — every live zombie inside
    // the cone takes a little direct damage and is set alight; bodies behind, out of range, or off the
    // cone axis are spared. One do_shoot, then assert who burned.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        setWeapon(tree, survivor, 4);
        CHECK((int)sField(survivor, "weapon")->number == 4);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;

        SceneNode* inCone = tree.findNode("Zombie0");  // (6,0): ahead, in range, on axis
        SceneNode* behind = tree.findNode("Zombie1");  // (-6,0): directly behind
        SceneNode* farAway = tree.findNode("Zombie2"); // (30,0): ahead but well out of range
        SceneNode* offAxis = tree.findNode("Zombie3"); // (1,9): near but far off the cone axis
        const double px[4] = {6.0, -6.0, 30.0, 1.0};
        const double py[4] = {0.0, 0.0, 0.0, 9.0};
        SceneNode* zs[4] = {inCone, behind, farAway, offAxis};
        for (int i = 0; i < 4; ++i) {
            Value zv = zs[i]->script();
            std::vector<Value> a = {Value::fromNum(px[i]), Value::fromNum(py[i]),
                                    Value::fromNum(200.0), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        }

        Value sv = survivor->script();
        std::vector<Value> none;
        tree.scripts().vm().callOn(sv, "do_shoot", none);   // one flamethrower cone

        CHECK(sField(inCone, "health")->number < 200.0);    // scorched
        CHECK(sField(inCone, "burn_timer")->number > 0.0);  // and set alight
        CHECK(sField(behind, "health")->number == 200.0);   // behind the survivor — spared
        CHECK(sField(behind, "burn_timer")->number == 0.0);
        CHECK(sField(farAway, "health")->number == 200.0);  // out of range — spared
        CHECK(sField(farAway, "burn_timer")->number == 0.0);
        CHECK(sField(offAxis, "health")->number == 200.0);  // off the cone axis — spared
        CHECK(sField(offAxis, "burn_timer")->number == 0.0);
    }

    // Wave mutator: an active modifier reshapes the whole horde as it spawns — feral (1) is faster,
    // hulking (2) is tougher — measured against an unmutated baseline of the same kind and wave.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value* mut = const_cast<Value*>(vm.getGlobal("g_mutator"));

        SceneNode* base = tree.findNode("Zombie0");
        SceneNode* fast = tree.findNode("Zombie1");
        SceneNode* tough = tree.findNode("Zombie2");
        // spawn(x, y, kind, wave): a plain walker (kind 0) at wave 4 in each case.
        auto spawnWalker = [&](SceneNode* z) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(20.0), Value::fromNum(0.0),
                                    Value::fromNum(0.0), Value::fromNum(4.0)};
            vm.callOn(zv, "spawn", a);
        };

        mut->number = 0.0; spawnWalker(base);              // baseline, no mutator
        const double baseSpd = sField(base, "speed")->number;
        const double baseHp = sField(base, "health")->number;

        mut->number = 1.0; spawnWalker(fast);              // feral: faster
        CHECK(sField(fast, "speed")->number > baseSpd);
        CHECK(sField(fast, "health")->number == baseHp);   // feral leaves health alone

        mut->number = 2.0; spawnWalker(tough);             // hulking: tougher
        CHECK(sField(tough, "health")->number > baseHp);
        CHECK(sField(tough, "max_health")->number == sField(tough, "health")->number);
        CHECK(sField(tough, "speed")->number == baseSpd);  // hulking leaves speed alone

        mut->number = 0.0;                                 // reset so later logic is unaffected

        // The director rolls a mutator from wave 3 on, and none before it.
        SceneNode* dir = tree.findNode("Director");
        Value ds = dir->script();
        std::vector<Value> w2 = {Value::fromNum(2.0)};
        vm.callOn(ds, "start_wave", w2);
        CHECK((int)glob(tree, "g_mutator") == 0);          // no mutator before wave 3
        std::vector<Value> w6 = {Value::fromNum(6.0)};
        vm.callOn(ds, "start_wave", w6);
        const int rolled = (int)glob(tree, "g_mutator");
        CHECK(rolled >= 1 && rolled <= 3);                 // a valid modifier was rolled
    }

    // Savage Horde mutator (g_mutator == 8): every zombie bites 60% harder, but is no faster or tougher —
    // measured against an unmutated baseline of the same kind and wave.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value* mut = const_cast<Value*>(vm.getGlobal("g_mutator"));

        SceneNode* base = tree.findNode("Zombie0");
        SceneNode* savage = tree.findNode("Zombie1");
        auto spawnWalker = [&](SceneNode* z) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(20.0), Value::fromNum(0.0),
                                    Value::fromNum(0.0), Value::fromNum(4.0)};
            vm.callOn(zv, "spawn", a);
        };

        mut->number = 0.0; spawnWalker(base);              // baseline, no mutator
        const double baseDmg = sField(base, "damage")->number;
        const double baseSpd = sField(base, "speed")->number;
        const double baseHp = sField(base, "health")->number;

        mut->number = 8.0; spawnWalker(savage);            // savage: bites harder
        const double savDmg = sField(savage, "damage")->number;
        CHECK(std::abs(savDmg - baseDmg * 1.6) < 1e-6);    // exactly 1.6x the baseline bite
        CHECK(sField(savage, "speed")->number == baseSpd); // savage leaves speed alone
        CHECK(sField(savage, "health")->number == baseHp); // ...and health alone

        mut->number = 0.0;                                 // reset so later logic is unaffected

        // The director's roll spans the full mutator band (1..9, now including bloodthirsty) — confirm
        // every roll from wave 3 on lands in range and the clamp never emits an out-of-band value.
        SceneNode* dir = tree.findNode("Director");
        Value ds = dir->script();
        for (int w = 3; w <= 40; ++w) {
            std::vector<Value> wv = {Value::fromNum((double)w)};
            vm.callOn(ds, "start_wave", wv);
            const int m = (int)glob(tree, "g_mutator");
            CHECK(m >= 1 && m <= 9);                        // every roll lands in the valid 1..9 range
        }
    }

    // Overkill gib: a killing blow far larger than a zombie's full health bursts it in a shockwave that
    // chips nearby zombies; a merely-lethal blow does not. Neighbours outside the burst radius are safe.
    {
        // Overkill case: 100 damage onto a 20-hp body (100 >= 1.5*20) gibs and splashes the neighbour.
        SceneTree tree;
        zomboid::buildScene(tree);
        SceneNode* target = tree.findNode("Zombie0");
        SceneNode* near_ = tree.findNode("Zombie1");   // 3 units away — inside the radius-4 burst
        SceneNode* farZ = tree.findNode("Zombie2");    // 10 units away — outside the burst
        auto place = [&](SceneNode* z, double x, double hp) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(hp), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        };
        place(target, 0.0, 20.0);
        place(near_, 3.0, 100.0);
        place(farZ, 10.0, 100.0);
        Value tv = target->script();
        std::vector<Value> big = {Value::fromNum(100.0)};
        tree.scripts().vm().callOn(tv, "take_damage", big);
        CHECK(!sField(target, "alive")->boolean);            // the target died
        CHECK(sField(near_, "health")->number < 100.0);      // neighbour caught the gib shockwave
        CHECK(sField(farZ, "health")->number == 100.0);      // out of range — untouched

        // Control: a merely-lethal blow (exactly 20 onto 20 hp) kills without any overkill burst.
        SceneTree t2;
        zomboid::buildScene(t2);
        SceneNode* tgt2 = t2.findNode("Zombie0");
        SceneNode* nb2 = t2.findNode("Zombie1");
        auto place2 = [&](SceneNode* z, double x, double hp) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(hp), Value::fromNum(0.0)};
            t2.scripts().vm().callOn(zv, "spawn_at", a);
        };
        place2(tgt2, 0.0, 20.0);
        place2(nb2, 3.0, 100.0);
        Value tv2 = tgt2->script();
        std::vector<Value> exact = {Value::fromNum(20.0)};
        t2.scripts().vm().callOn(tv2, "take_damage", exact);
        CHECK(!sField(tgt2, "alive")->boolean);              // still dies
        CHECK(sField(nb2, "health")->number == 100.0);       // but no overkill splash
    }

    // Overkill scales with force: a monster hit throws a bigger gib shockwave than one that only just
    // tips a body over the overkill threshold. Same setup, same neighbour distance — only the killing
    // blow's size differs, and the neighbour loses proportionally more health to the bigger burst.
    {
        auto splashOnNeighbour = [](double killDamage) {
            SceneTree tree;
            zomboid::buildScene(tree);
            SceneNode* target = tree.findNode("Zombie0");
            SceneNode* nb = tree.findNode("Zombie1");         // 3 units away — inside every burst radius
            auto place = [&](SceneNode* z, double x, double hp) {
                Value zv = z->script();
                std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                        Value::fromNum(hp), Value::fromNum(0.0)};
                tree.scripts().vm().callOn(zv, "spawn_at", a);
            };
            place(target, 0.0, 20.0);                          // 20-hp body
            place(nb, 3.0, 100.0);
            Value tv = target->script();
            std::vector<Value> hit = {Value::fromNum(killDamage)};
            tree.scripts().vm().callOn(tv, "take_damage", hit);
            return 100.0 - sField(nb, "health")->number;       // damage the neighbour took from the burst
        };

        // Threshold kill (30 onto 20 hp → power 1.5): the smallest, base 25-damage burst.
        const double weak = splashOnNeighbour(30.0);
        CHECK(weak == 25.0);
        // Monster kill (100 onto 20 hp → power 5, capped): the biggest, 70-damage burst.
        const double strong = splashOnNeighbour(100.0);
        CHECK(strong == 70.0);
        CHECK(strong > weak);                                 // heavier hit → deadlier chain
    }

    // Boss enrage: a boss dropped below 35% health flips into a permanent rage — it speeds up (once),
    // and the flag latches so a second frame doesn't compound the speed boost.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* boss = tree.findNode("Zombie0");
        Value bv = boss->script();
        std::vector<Value> sp = {Value::fromNum(40.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(1.0)}; // spawn boss (kind 3), wave 1
        vm.callOn(bv, "spawn", sp);
        CHECK((int)sField(boss, "kind")->number == 3);
        CHECK(!sField(boss, "enraged")->boolean);          // not enraged at full health
        const double calmSpeed = sField(boss, "speed")->number;

        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        vm.callOn(bv, "_process", dt);                     // healthy: still calm
        CHECK(!sField(boss, "enraged")->boolean);

        // Wound it below the 35% threshold, then step: it should enrage and speed up.
        sField(boss, "health")->number = sField(boss, "max_health")->number * 0.3;
        vm.callOn(bv, "_process", dt);
        CHECK(sField(boss, "enraged")->boolean);
        const double rageSpeed = sField(boss, "speed")->number;
        CHECK(rageSpeed > calmSpeed);

        // The boost is one-shot: another frame must not multiply the speed again.
        vm.callOn(bv, "_process", dt);
        CHECK(sField(boss, "speed")->number == rageSpeed);
    }
    // Boss enrage-heal: once enraged, the boss slowly knits its wounds (2%/s) — but a burning, bleeding,
    // or chilled boss can't, so damage-over-time and cold shut the self-heal off, the same rule that
    // governs the Regenerator mutator and the Healer's mend.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        // Enraged and clean: it regenerates over a second of frames.
        SceneTree t1;
        SceneNode* s1 = zomboid::buildScene(t1);
        s1->setPosition(0.0, 0.0);
        auto& vm1 = t1.scripts().vm();
        SceneNode* b1 = t1.findNode("Zombie0");
        Value b1v = b1->script();
        std::vector<Value> sp = {Value::fromNum(40.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(1.0)};
        vm1.callOn(b1v, "spawn", sp);
        sField(b1, "health")->number = sField(b1, "max_health")->number * 0.3;  // below the 35% threshold
        vm1.callOn(b1v, "_process", dt);   // enrages this frame
        const double healed0 = sField(b1, "health")->number;
        for (int i = 0; i < 60; ++i) { vm1.callOn(b1v, "_process", dt); }  // 1s clean
        CHECK(sField(b1, "health")->number > healed0);   // enrage-heal knits wounds

        // Enraged but burning: the self-heal is shut off, so health does not climb.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* b2 = t2.findNode("Zombie0");
        Value b2v = b2->script();
        vm2.callOn(b2v, "spawn", sp);
        sField(b2, "health")->number = sField(b2, "max_health")->number * 0.3;
        vm2.callOn(b2v, "_process", dt);   // enrages
        const double burnBase = sField(b2, "health")->number;
        sField(b2, "burn_timer")->number = 5.0;   // keep it alight across the run
        for (int i = 0; i < 60; ++i) {
            sField(b2, "burn_timer")->number = 5.0;   // sustain the burn each frame (it ticks down/deals dmg)
            vm2.callOn(b2v, "_process", dt);
        }
        CHECK(sField(b2, "health")->number <= burnBase);   // burning → no enrage-heal (it only loses HP)

        // Enraged but BLEEDING: the same gate blocks the self-heal (bleed is DoT, holds the wound open).
        SceneTree t3;
        SceneNode* s3 = zomboid::buildScene(t3);
        s3->setPosition(0.0, 0.0);
        auto& vm3 = t3.scripts().vm();
        SceneNode* b3 = t3.findNode("Zombie0");
        Value b3v = b3->script();
        vm3.callOn(b3v, "spawn", sp);
        sField(b3, "health")->number = sField(b3, "max_health")->number * 0.3;
        vm3.callOn(b3v, "_process", dt);   // enrages
        sField(b3, "health")->number = sField(b3, "max_health")->number * 0.3;  // reset to a clean baseline
        const double bleedBase = sField(b3, "health")->number;
        for (int i = 0; i < 60; ++i) {
            sField(b3, "bleed_stacks")->number = 3.0;   // sustain a bleed the whole run
            sField(b3, "bleed_timer")->number = 3.0;    // ...keep the wound open so the stacks don't clear
            vm3.callOn(b3v, "_process", dt);
        }
        CHECK(sField(b3, "health")->number <= bleedBase);   // bleeding → no enrage-heal

        // Enraged but CHILLED: cold shuts the self-heal off too (cryo is a hard counter to the enrage).
        SceneTree t4;
        SceneNode* s4 = zomboid::buildScene(t4);
        s4->setPosition(0.0, 0.0);
        auto& vm4 = t4.scripts().vm();
        SceneNode* b4 = t4.findNode("Zombie0");
        Value b4v = b4->script();
        vm4.callOn(b4v, "spawn", sp);
        sField(b4, "health")->number = sField(b4, "max_health")->number * 0.3;
        vm4.callOn(b4v, "_process", dt);   // enrages
        const double chillBase = sField(b4, "health")->number;
        for (int i = 0; i < 60; ++i) {
            sField(b4, "slow_timer")->number = 2.0;   // keep it frozen the whole run
            vm4.callOn(b4v, "_process", dt);
        }
        CHECK(sField(b4, "health")->number <= chillBase);   // chilled → self-heal shut off (never climbs)
    }

    // Boss bounty: felling a boss (the wave leader) always drops a full care package — a guaranteed
    // medkit AND a guaranteed power-up — unlike an ordinary zombie whose drops are a rare dice roll.
    {
        auto bossMedkits = [](SceneTree& t) {
            int n = 0;
            for (SceneNode* m : t.nodesInGroup("medkits"))
                if (sField(m, "active")->boolean) ++n;
            return n;
        };
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* boss = tree.findNode("Zombie0");
        Value bv = boss->script();
        std::vector<Value> sp = {Value::fromNum(40.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(5.0)}; // boss (kind 3), wave 5
        vm.callOn(bv, "spawn", sp);
        CHECK((int)sField(boss, "kind")->number == 3);
        CHECK(bossMedkits(tree) == 0);                         // nothing dropped while it lives
        CHECK(activePowerups(tree) == 0);

        std::vector<Value> lethal = {Value::fromNum(999999.0)};
        vm.callOn(bv, "take_damage", lethal);
        CHECK(!sField(boss, "alive")->boolean);                // the boss fell
        CHECK(bossMedkits(tree) >= 1);                         // guaranteed medkit
        CHECK(activePowerups(tree) >= 1);                      // ...and a guaranteed power-up
    }

    // Cryo-nova power-up (kind 4): grabbing it instantly chills every live zombie on the field, buying
    // breathing room. Dormant pool slots (not alive) are left untouched.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();

        SceneNode* zs[3] = {tree.findNode("Zombie0"), tree.findNode("Zombie1"), tree.findNode("Zombie2")};
        const double xs[3] = {5.0, -8.0, 20.0};
        for (int i = 0; i < 3; ++i) {
            Value zv = zs[i]->script();
            std::vector<Value> a = {Value::fromNum(xs[i]), Value::fromNum(0.0),
                                    Value::fromNum(100.0), Value::fromNum(0.0)};
            vm.callOn(zv, "spawn_at", a);
            CHECK(sField(zs[i], "slow_timer")->number == 0.0);   // not chilled yet
        }
        SceneNode* dormant = tree.findNode("Zombie5");           // never spawned — stays dead
        CHECK(!sField(dormant, "alive")->boolean);

        Value sv = survivor->script();
        // Give the survivor a sustained buff first (Berserk = 8), then grab the one-shot Cryo Nova.
        std::vector<Value> berserk = {Value::fromNum(8.0)};
        vm.callOn(sv, "grant_powerup", berserk);
        CHECK((int)sField(survivor, "buff_kind")->number == 8);
        const double berserkFr = sField(survivor, "buff_fr")->number;
        CHECK(berserkFr > 1.0);                                   // Berserk raised the fire-rate multiplier

        std::vector<Value> pk = {Value::fromNum(4.0)};
        vm.callOn(sv, "grant_powerup", pk);
        // Cryo Nova is a ONE-SHOT: it chills the field but must NOT occupy the buff slot, so the active
        // Berserk survives untouched (previously the pickup overwrote buff_kind to 4 and wiped Berserk).
        CHECK((int)sField(survivor, "buff_kind")->number == 8);   // Berserk still active, not cancelled
        CHECK(sField(survivor, "buff_fr")->number == berserkFr);  // ...its multiplier intact
        for (int i = 0; i < 3; ++i)
            CHECK(sField(zs[i], "slow_timer")->number > 0.0);    // whole field chilled, any distance
        CHECK(sField(dormant, "slow_timer")->number == 0.0);     // a dormant slot is left alone
    }

    // Frost shatter: a chilled zombie killed while frozen bursts into an icy cloud that chills nearby
    // zombies (chain freeze), but a body killed while NOT chilled does no such thing.
    {
        // Chilled target: killing it chills an in-range neighbour but not a far one.
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* target = tree.findNode("Zombie0");
        SceneNode* near_ = tree.findNode("Zombie1");   // 3 units — inside the radius-4.5 shatter
        SceneNode* farZ = tree.findNode("Zombie2");    // 12 units — outside
        auto place = [&](SceneNode* z, double x, double hp) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(hp), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        };
        place(target, 0.0, 20.0);
        place(near_, 3.0, 100.0);
        place(farZ, 12.0, 100.0);
        Value tv = target->script();
        std::vector<Value> chill = {Value::fromNum(3.0)};
        vm.callOn(tv, "apply_slow", chill);            // freeze the target before it dies
        CHECK(sField(near_, "slow_timer")->number == 0.0);
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        vm.callOn(tv, "take_damage", lethal);
        CHECK(!sField(target, "alive")->boolean);
        CHECK(sField(near_, "slow_timer")->number > 0.0);   // caught the frost shatter
        CHECK(sField(farZ, "slow_timer")->number == 0.0);   // out of range — unchilled

        // Control: a target killed while NOT chilled shatters nothing.
        SceneTree t2;
        zomboid::buildScene(t2);
        SceneNode* tgt2 = t2.findNode("Zombie0");
        SceneNode* nb2 = t2.findNode("Zombie1");
        auto place2 = [&](SceneNode* z, double x, double hp) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(hp), Value::fromNum(0.0)};
            t2.scripts().vm().callOn(zv, "spawn_at", a);
        };
        place2(tgt2, 0.0, 20.0);
        place2(nb2, 3.0, 100.0);
        Value t2v = tgt2->script();
        t2.scripts().vm().callOn(t2v, "take_damage", lethal);   // killed unfrozen
        CHECK(sField(nb2, "slow_timer")->number == 0.0);        // no chain freeze
    }

    // Fire contagion: a zombie that dies while burning sets in-range neighbours alight (radius 4.5), but
    // a far one is untouched — and a target killed while NOT burning spreads no fire. The offensive
    // mirror of frost shatter.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* target = tree.findNode("Zombie0");
        SceneNode* near_ = tree.findNode("Zombie1");   // 3 units — inside the radius-4.5 spread
        SceneNode* farZ = tree.findNode("Zombie2");    // 12 units — outside
        auto place = [&](SceneNode* z, double x, double hp) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(hp), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        };
        place(target, 0.0, 20.0);
        place(near_, 3.0, 100.0);
        place(farZ, 12.0, 100.0);
        Value tv = target->script();
        std::vector<Value> torch = {Value::fromNum(2.0), Value::fromNum(12.0)};  // ignite(dur, dps)
        vm.callOn(tv, "ignite", torch);                // set the target alight before it dies
        CHECK(sField(near_, "burn_timer")->number == 0.0);
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        vm.callOn(tv, "take_damage", lethal);
        CHECK(!sField(target, "alive")->boolean);
        CHECK(sField(near_, "burn_timer")->number > 0.0);   // caught the spreading fire
        CHECK(sField(farZ, "burn_timer")->number == 0.0);   // out of range — never lit

        // Control: a target killed while NOT burning spreads no fire.
        SceneTree t2;
        zomboid::buildScene(t2);
        SceneNode* tgt2 = t2.findNode("Zombie0");
        SceneNode* nb2 = t2.findNode("Zombie1");
        auto place2 = [&](SceneNode* z, double x, double hp) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(hp), Value::fromNum(0.0)};
            t2.scripts().vm().callOn(zv, "spawn_at", a);
        };
        place2(tgt2, 0.0, 20.0);
        place2(nb2, 3.0, 100.0);
        Value t2v = tgt2->script();
        t2.scripts().vm().callOn(t2v, "take_damage", lethal);   // killed unlit
        CHECK(sField(nb2, "burn_timer")->number == 0.0);        // no fire spread
    }

    // Shoot down a spitter glob: a bullet that catches an in-flight acid glob destroys it clean (no
    // puddle) — ranged counterplay to the spitter's ranged threat.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0);   // keep the survivor clear
        auto& vm = tree.scripts().vm();
        SceneNode* spit = tree.findNode("Spit0");
        spit->setPosition(10.0, 0.0);
        sField(spit, "active")->boolean = true;      // an acid glob hanging in the air at (10,0)
        CHECK(sField(spit, "active")->boolean);
        CHECK(activeAcid(tree) == 0);

        SceneNode* bullet = tree.findNode("Bullet0");
        Value bv = bullet->script();
        std::vector<Value> shot = {Value::fromNum(0.0), Value::fromNum(0.0), Value::fromNum(1.0),
                                   Value::fromNum(0.0), Value::fromNum(70.0), Value::fromNum(25.0)};
        vm.callOn(bv, "fire", shot);                 // fire toward +x, straight at the glob
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 30 && sField(spit, "active")->boolean; ++i) vm.callOn(bv, "_process", dt);
        CHECK(sField(spit, "active")->boolean == false);   // glob shot out of the air
        CHECK(sField(bullet, "active")->boolean == false); // the round was spent knocking it down
        CHECK(activeAcid(tree) == 0);                      // destroyed clean — no puddle left behind
    }

    // Fire denies the split: a splitter (kind 6) killed while burning is incinerated and spawns no
    // runners, while an unlit splitter bursts into two.
    {
        auto aliveCount = [](SceneTree& t) {
            int c = 0;
            for (SceneNode* z : t.nodesInGroup("zombies"))
                if (sField(z, "alive")->boolean) ++c;
            return c;
        };
        std::vector<Value> spawn6 = {Value::fromNum(0.0), Value::fromNum(0.0), Value::fromNum(6.0),
                                     Value::fromNum(3.0)}; // spawn(x,y,kind=6,wave=3)
        std::vector<Value> lethal = {Value::fromNum(9999.0)};

        // Burning splitter: incinerated, no split.
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* sp = tree.findNode("Zombie0");
        Value spv = sp->script();
        vm.callOn(spv, "spawn", spawn6);
        CHECK(aliveCount(tree) == 1);
        std::vector<Value> torch = {Value::fromNum(2.0), Value::fromNum(12.0)};
        vm.callOn(spv, "ignite", torch);
        vm.callOn(spv, "take_damage", lethal);
        CHECK(!sField(sp, "alive")->boolean);
        CHECK(aliveCount(tree) == 0);   // burned before it could rupture — no runners

        // Control: an unlit splitter bursts into two runners.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        SceneNode* sp2 = t2.findNode("Zombie0");
        Value sp2v = sp2->script();
        vm2.callOn(sp2v, "spawn", spawn6);
        vm2.callOn(sp2v, "take_damage", lethal);
        CHECK(!sField(sp2, "alive")->boolean);
        CHECK(aliveCount(t2) == 2);     // burst into two runners
    }

    // Exploder is a suicide bomber: on reaching the survivor it detonates on contact (killing itself and
    // hurting the adjacent player) instead of a harmless bite.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        sField(survivor, "health")->number = 500.0;   // tanky enough to survive the blast for the assert
        auto& vm = tree.scripts().vm();
        const_cast<Value*>(vm.getGlobal("g_phase"))->number = 0.0;
        SceneNode* ex = tree.findNode("Zombie0");
        Value exv = ex->script();
        std::vector<Value> sp = {Value::fromNum(0.5), Value::fromNum(0.0),
                                 Value::fromNum(4.0), Value::fromNum(3.0)}; // spawn(x,y,kind=4,wave=3) on top of the survivor
        vm.callOn(exv, "spawn", sp);
        const double h0 = sField(survivor, "health")->number;
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 5 && sField(ex, "alive")->boolean; ++i) vm.callOn(exv, "_process", dt);
        CHECK(!sField(ex, "alive")->boolean);                    // detonated on contact (didn't just bite)
        CHECK(sField(survivor, "health")->number < h0);          // the blast caught the adjacent survivor
    }

    // Grenade flashes over acid: a caustic puddle in the frag's blast radius combusts, while one well
    // outside is untouched — completing the "any hard blast sets off volatile acid" rule (like the mine).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0);   // keep the survivor clear of the blast
        auto& vm = tree.scripts().vm();
        SceneNode* gren = tree.findNode("Grenade0");
        Value gv = gren->script();
        std::vector<Value> place = {Value::fromNum(0.0), Value::fromNum(0.0),
                                    Value::fromNum(1.0), Value::fromNum(0.0)}; // throw_at(x,y,dx,dy)
        vm.callOn(gv, "throw_at", place);
        gren->setPosition(0.0, 0.0);   // pin the blast centre at the origin

        SceneNode* nearAcid = tree.findNode("Acid0");
        SceneNode* farAcid = tree.findNode("Acid1");
        Value nav = nearAcid->script();
        Value fav = farAcid->script();
        std::vector<Value> near = {Value::fromNum(3.0), Value::fromNum(0.0)};    // inside blast radius 5
        std::vector<Value> far = {Value::fromNum(40.0), Value::fromNum(0.0)};    // well outside
        vm.callOn(nav, "splat_at", near);
        vm.callOn(fav, "splat_at", far);
        CHECK(sField(nearAcid, "active")->boolean);
        CHECK(sField(farAcid, "active")->boolean);

        std::vector<Value> noargs;
        vm.callOn(gv, "explode", noargs);
        CHECK(!sField(nearAcid, "active")->boolean);    // flashed over by the frag
        CHECK(sField(farAcid, "active")->boolean);      // out of range — still a puddle
    }

    // Acid flash-over pops barrels: lighting a caustic puddle detonates an explosive barrel caught in the
    // flash (combust radius = acid radius + 2), while a barrel well outside the flash is untouched — so a
    // puddle sitting on a barrel is a two-stage bomb, completing the "every violent combustion sets off a
    // barrel" rule (fire, blast, and now the acid flash-over).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0);   // keep the survivor clear of everything
        auto& vm = tree.scripts().vm();

        SceneNode* acid = tree.findNode("Acid0");
        Value av = acid->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};   // splat_at(x,y)
        vm.callOn(av, "splat_at", at);
        CHECK(sField(acid, "active")->boolean);

        SceneNode* nearBarrel = tree.findNode("Barrel0");
        SceneNode* farBarrel = tree.findNode("Barrel1");
        Value nbv = nearBarrel->script();
        Value fbv = farBarrel->script();
        std::vector<Value> nearPos = {Value::fromNum(3.0), Value::fromNum(0.0)};   // inside the flash-over
        std::vector<Value> farPos = {Value::fromNum(40.0), Value::fromNum(0.0)};   // well outside
        vm.callOn(nbv, "place", nearPos);
        vm.callOn(fbv, "place", farPos);
        CHECK(sField(nearBarrel, "active")->boolean);
        CHECK(sField(farBarrel, "active")->boolean);

        std::vector<Value> noargs;
        vm.callOn(av, "combust", noargs);
        CHECK(!sField(nearBarrel, "active")->boolean);   // barrel on the puddle cooked off by the flash
        CHECK(sField(farBarrel, "active")->boolean);      // out of range — still standing
    }

    // Melee execute: a melee swing finishes a badly-wounded (<30% health) non-boss outright and refunds
    // most of its cooldown; a healthy target takes only the normal swing and no refund.
    {
        // Execute case: a wounded brute is finished and the cooldown is refunded.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* brute = tree.findNode("Zombie0");
        Value bv = brute->script();
        std::vector<Value> sp = {Value::fromNum(1.0), Value::fromNum(0.0),
                                 Value::fromNum(2.0), Value::fromNum(5.0)}; // brute (kind 2) point-blank
        vm.callOn(bv, "spawn", sp);
        const double mhp = sField(brute, "max_health")->number;
        sField(brute, "health")->number = mhp * 0.2;           // wounded to 20%
        const double cdMax = sField(survivor, "melee_cd_max")->number;
        sField(survivor, "health")->number = 50.0;             // wounded, with room to heal
        Value sv = survivor->script();
        std::vector<Value> none;
        vm.callOn(sv, "melee", none);
        CHECK(!sField(brute, "alive")->boolean);               // executed outright
        CHECK(sField(survivor, "melee_cd")->number < cdMax);   // cooldown refunded
        CHECK(sField(survivor, "health")->number == 55.0);     // executioner's bloodthirst: +5 per finisher

        // No-execute case: a healthy brute survives the swing and the cooldown is not refunded.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* brute2 = t2.findNode("Zombie0");
        Value b2 = brute2->script();
        vm2.callOn(b2, "spawn", sp);
        const double full = sField(brute2, "health")->number;  // full brute health
        const double dmg = sField(surv2, "melee_dmg")->number;
        sField(surv2, "health")->number = 50.0;                // wounded, room to heal
        Value sv2 = surv2->script();
        vm2.callOn(sv2, "melee", none);
        CHECK(sField(brute2, "alive")->boolean);               // survives the swing
        CHECK(sField(brute2, "health")->number == full - dmg); // took a normal melee hit
        CHECK(sField(surv2, "melee_cd")->number == sField(surv2, "melee_cd_max")->number); // no refund
        CHECK(sField(surv2, "health")->number == 50.0);        // no execute → no bloodthirst heal
    }

    // Dash strike: the dodge-roll now shoulder-checks zombies it passes through — one shove each,
    // knocking them back and dealing dash damage — so a dash both escapes and clears a path.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* z0 = tree.findNode("Zombie0");
        Value zv = z0->script();
        std::vector<Value> at = {Value::fromNum(4.0), Value::fromNum(0.0), Value::fromNum(100.0),
                                 Value::fromNum(0.0)};   // a walker parked 4 units ahead
        vm.callOn(zv, "spawn_at", at);
        const double zx0 = z0->x();

        Value sv = survivor->script();
        std::vector<Value> dir = {Value::fromNum(1.0), Value::fromNum(0.0)}; // dash straight at it
        Value ok = vm.callOn(sv, "dash", dir);
        CHECK(ok.boolean);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 20; ++i) vm.callOn(sv, "_process", dt);   // ride out the dash burst
        CHECK(sField(z0, "health")->number == 60.0);   // struck exactly once for 40 (not per-frame)
        CHECK(z0->x() > zx0);                           // and knocked further along the dash
        CHECK(sField(z0, "stagger_timer")->number > 0.0);   // and briefly flinched by the shoulder-check
    }

    // Armor shatter: when a hit BREAKS the plate, it throws off a concussive burst that shoves and
    // staggers nearby zombies — but a hit the plate merely soaks (no break) does not.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();
        SceneNode* z = tree.findNode("Zombie0");
        Value zs = z->script();
        std::vector<Value> at = {Value::fromNum(2.0), Value::fromNum(0.0), Value::fromNum(100.0),
                                 Value::fromNum(0.0)};   // a light walker 2 units away (radius-5 burst)
        vm.callOn(zs, "spawn_at", at);

        // Soak case: a big plate eats a small hit without breaking — no shatter, zombie undisturbed.
        sField(survivor, "armor")->number = 100.0;
        std::vector<Value> small = {Value::fromNum(10.0)};
        vm.callOn(sv, "take_damage", small);
        CHECK(z->x() == 2.0);                                 // not shoved — the plate held
        CHECK(sField(z, "stagger_timer")->number == 0.0);

        // Break case: a hit that spends the plate detonates the shatter — zombie knocked back + staggered.
        sField(survivor, "armor")->number = 5.0;
        std::vector<Value> big = {Value::fromNum(50.0)};
        vm.callOn(sv, "take_damage", big);
        CHECK(sField(survivor, "armor")->number == 0.0);     // plate broke
        CHECK(z->x() > 2.0);                                  // flung outward by the shatter burst
        CHECK(sField(z, "stagger_timer")->number > 0.0);     // and staggered

        // Exact-depletion case: a hit landing EXACTLY on the plate's remaining charge still counts as
        // the breaking hit — the shatter must fire (not be silently skipped), and with the plate soaking
        // the full blow, no damage bleeds through to health. (Regression: the old `armor >= 0` early-out
        // treated an exact-zero plate as "fully absorbed" and never shattered — on that hit or any later
        // one, since armor was then 0.)
        z->setPosition(2.0, 0.0);                            // reset the target next to the survivor
        sField(z, "stagger_timer")->number = 0.0;
        sField(survivor, "armor")->number = 25.0;
        sField(survivor, "health")->number = 100.0;
        std::vector<Value> exact = {Value::fromNum(25.0)};   // damage == remaining plate, to the point
        vm.callOn(sv, "take_damage", exact);
        CHECK(sField(survivor, "armor")->number == 0.0);     // plate spent exactly
        CHECK(sField(survivor, "health")->number == 100.0);  // ...with zero bleed-through to health
        CHECK(z->x() > 2.0);                                 // and the shatter still fired
        CHECK(sField(z, "stagger_timer")->number > 0.0);
    }

    // Hunger pressure: while starving (hunger maxed), out-of-combat regen is SUPPRESSED, so health
    // drains and you must eat. Fed (hunger low), the same out-of-combat window heals instead.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Starving + wounded + out of combat + NO rations left: no regen — the starve drain wins.
        sField(survivor, "health")->number = 50.0;
        sField(survivor, "hunger")->number = 100.0;
        sField(survivor, "food")->number = 0.0;           // out of rations → true starvation
        sField(survivor, "regen_timer")->number = 10.0;   // already past the 5s out-of-combat window
        const double hStarve0 = sField(survivor, "health")->number;
        for (int i = 0; i < 30; ++i) vm.callOn(sv, "_process", dt);
        CHECK(sField(survivor, "health")->number < hStarve0);   // starving: drained, not healed

        // Fed + wounded + out of combat: the same window regenerates health.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        Value s2v = s2->script();
        sField(s2, "health")->number = 50.0;
        sField(s2, "hunger")->number = 0.0;
        sField(s2, "regen_timer")->number = 10.0;
        const double hFed0 = sField(s2, "health")->number;
        for (int i = 0; i < 30; ++i) vm2.callOn(s2v, "_process", dt);
        CHECK(sField(s2, "health")->number > hFed0);            // fed + out of combat: regen ticks up
    }

    // Starvation auto-feed: at max hunger you instinctively eat a carried ration rather than take
    // starvation damage — so the drain only bites once your food is truly gone. A ration is spent and
    // hunger drops; a survivor with no food takes the drain instead (covered above).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        sField(survivor, "health")->number = 50.0;
        sField(survivor, "hunger")->number = 100.0;
        sField(survivor, "food")->number = 2.0;            // rations in the pack
        sField(survivor, "regen_timer")->number = 0.0;     // keep out-of-combat regen out of the picture
        const double h0 = sField(survivor, "health")->number;

        vm.callOn(sv, "_process", dt);                     // one starving frame with food on hand
        CHECK(sField(survivor, "food")->number == 1.0);    // ate a ration instead of starving
        CHECK(sField(survivor, "hunger")->number < 100.0); // hunger relieved by the meal
        CHECK(sField(survivor, "health")->number == h0);   // no starvation damage taken
    }

    // Body armor: a depletable plate takes the hit first, and only the overflow past a spent plate
    // reaches health. Bought from the shop (kind 3) for cash.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();

        // Give a 50-point plate; a 30 hit is fully absorbed (health untouched, armor drops to 20).
        sField(survivor, "armor")->number = 50.0;
        const double h0 = sField(survivor, "health")->number;
        std::vector<Value> hit30 = {Value::fromNum(30.0)};
        vm.callOn(sv, "take_damage", hit30);
        CHECK(sField(survivor, "armor")->number == 20.0);
        CHECK(sField(survivor, "health")->number == h0);        // plate ate all of it

        // A 40 hit breaks the remaining 20 plate; 20 overflow reaches health.
        std::vector<Value> hit40 = {Value::fromNum(40.0)};
        vm.callOn(sv, "take_damage", hit40);
        CHECK(sField(survivor, "armor")->number == 0.0);        // plate spent
        CHECK(sField(survivor, "health")->number == h0 - 20.0); // only the overflow bled through

        // Buying armor (kind 3, cost 80) from the shop straps on a full plate.
        Value* cash = const_cast<Value*>(vm.getGlobal("g_cash"));
        cash->number = 100.0;
        sField(survivor, "armor")->number = 0.0;
        std::vector<Value> buyArmor = {Value::fromNum(3.0)};
        Value ok = vm.callOn(sv, "buy", buyArmor);
        CHECK(ok.boolean);
        CHECK(glob(tree, "g_cash") == 20.0);                    // 100 - 80
        CHECK(sField(survivor, "armor")->number == sField(survivor, "armor_max")->number);

        // Buying a field kit (kind 4, cost 70) restocks one mine, one sentry, and one molotov.
        cash->number = 100.0;
        const double mines0 = sField(survivor, "mines")->number;
        const double sentries0 = sField(survivor, "sentries")->number;
        const double molotovs0 = sField(survivor, "molotovs")->number;
        std::vector<Value> buyKit = {Value::fromNum(4.0)};
        Value kitOk = vm.callOn(sv, "buy", buyKit);
        CHECK(kitOk.boolean);
        CHECK(glob(tree, "g_cash") == 30.0);                    // 100 - 70
        CHECK(sField(survivor, "mines")->number == mines0 + 1.0);
        CHECK(sField(survivor, "sentries")->number == sentries0 + 1.0);
        CHECK(sField(survivor, "molotovs")->number == molotovs0 + 1.0);

        // No wasted salvage: a heal at full health and a plate when armor is already full are both
        // refused WITHOUT charging (return false, cash untouched). A genuine gain still goes through.
        cash->number = 100.0;
        sField(survivor, "health")->number = sField(survivor, "max_health")->number; // full HP
        std::vector<Value> buyHeal = {Value::fromNum(2.0)};
        CHECK(vm.callOn(sv, "buy", buyHeal).boolean == false);   // refused
        CHECK(glob(tree, "g_cash") == 100.0);                    // no charge

        sField(survivor, "armor")->number = sField(survivor, "armor_max")->number; // full plate
        std::vector<Value> buyArmor2 = {Value::fromNum(3.0)};
        CHECK(vm.callOn(sv, "buy", buyArmor2).boolean == false); // refused
        CHECK(glob(tree, "g_cash") == 100.0);                    // no charge

        // Control: a heal that actually restores health still costs cash.
        sField(survivor, "health")->number = 10.0;
        CHECK(vm.callOn(sv, "buy", buyHeal).boolean);            // a real heal goes through
        CHECK(glob(tree, "g_cash") == 40.0);                     // 100 - 60 (heal cost)

        // Buying and weapon-switching are INDEPENDENT operations. (The field-kit buy and the flamethrower
        // switch once shared key 5 in the host input layer, so one press did both; the keys are now split.
        // This locks the underlying invariant so no future change re-entangles them.) Buying a field kit
        // must not change the equipped weapon, and switching weapons must not spend cash.
        cash->number = 500.0;
        std::vector<Value> pistol = {Value::fromNum(0.0)};
        vm.callOn(sv, "set_weapon", pistol);
        CHECK((int)sField(survivor, "weapon")->number == 0);
        vm.callOn(sv, "buy", buyKit);                            // buy field kit (kind 4)
        CHECK((int)sField(survivor, "weapon")->number == 0);    // ...weapon unchanged by a shop purchase
        const double cashPreSwitch = glob(tree, "g_cash");
        std::vector<Value> flamer = {Value::fromNum(4.0)};
        vm.callOn(sv, "set_weapon", flamer);                     // switch to flamethrower (weapon 4)
        CHECK((int)sField(survivor, "weapon")->number == 4);
        CHECK(glob(tree, "g_cash") == cashPreSwitch);           // ...switching weapons spends no cash
    }

    // Salvage economy: kills bank cash, and buy() spends it on ammo/grenades/heals — succeeding when
    // the survivor can afford it, rejected (with no effect) when they can't.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value* cash = const_cast<Value*>(vm.getGlobal("g_cash"));

        // A kill banks cash.
        SceneNode* z0 = tree.findNode("Zombie0");
        Value zv = z0->script();
        std::vector<Value> at = {Value::fromNum(4.0), Value::fromNum(0.0), Value::fromNum(20.0),
                                 Value::fromNum(0.0)};
        vm.callOn(zv, "spawn_at", at);
        cash->number = 0.0;
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        vm.callOn(zv, "take_damage", lethal);
        CHECK(glob(tree, "g_cash") > 0.0);                     // the kill paid out salvage

        // Afford it: buying a grenade (cost 40) spends cash and adds a grenade.
        Value sv = survivor->script();
        cash->number = 100.0;
        const double nades0 = sField(survivor, "grenades")->number;
        std::vector<Value> buyNade = {Value::fromNum(1.0)};
        Value ok = vm.callOn(sv, "buy", buyNade);
        CHECK(ok.boolean);
        CHECK(glob(tree, "g_cash") == 60.0);                   // 100 - 40
        CHECK(sField(survivor, "grenades")->number == nades0 + 1.0);

        // Can't afford it: with 10 cash, a 60-cost heal is refused and nothing changes.
        cash->number = 10.0;
        sField(survivor, "health")->number = 50.0;             // wound them so a heal would be visible
        const double hp0 = sField(survivor, "health")->number;
        std::vector<Value> buyHeal = {Value::fromNum(2.0)};
        Value poor = vm.callOn(sv, "buy", buyHeal);
        CHECK(!poor.boolean);
        CHECK(glob(tree, "g_cash") == 10.0);                   // untouched
        CHECK(sField(survivor, "health")->number == hp0);      // no heal applied
    }

    // Shop no-waste on ammo: an ammo refill (kind 0) is declined for free while the pistol is equipped
    // (its reserve is bottomless, so the refill would do nothing), but goes through on a power weapon.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value* cash = const_cast<Value*>(vm.getGlobal("g_cash"));
        Value sv = survivor->script();
        std::vector<Value> buyAmmo = {Value::fromNum(0.0)};

        // Pistol equipped: the buy is refused without charging.
        setWeapon(tree, survivor, 0);
        cash->number = 100.0;
        Value onPistol = vm.callOn(sv, "buy", buyAmmo);
        CHECK(!onPistol.boolean);                               // declined — pistol reserve is infinite
        CHECK(glob(tree, "g_cash") == 100.0);                  // no salvage spent

        // SMG equipped: the buy lands, spending cash and topping up that weapon's reserve.
        setWeapon(tree, survivor, 2);
        const double res0 = (*sField(survivor, "reserves")->array)[2].number;
        Value onSmg = vm.callOn(sv, "buy", buyAmmo);
        CHECK(onSmg.boolean);                                   // a finite reserve is worth refilling
        CHECK(glob(tree, "g_cash") == 50.0);                  // 100 - 50
        CHECK((*sField(survivor, "reserves")->array)[2].number > res0);  // reserve grew
    }

    // Bloater (kind 10): a slow, tanky zombie that bursts into a lingering toxic cloud (an acid puddle)
    // when it dies — so a careless point-blank kill leaves the survivor standing in poison.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* bloater = tree.findNode("Zombie0");
        Value bv = bloater->script();
        std::vector<Value> sp = {Value::fromNum(20.0), Value::fromNum(0.0),
                                 Value::fromNum(10.0), Value::fromNum(3.0)}; // spawn(x,y,kind=10,wave=3)
        vm.callOn(bv, "spawn", sp);
        CHECK((int)sField(bloater, "kind")->number == 10);
        CHECK((int)sField(bloater, "score_value")->number == 30);  // high-value tank
        CHECK(sField(bloater, "radius")->number > 1.5);            // visibly fat
        CHECK(activeAcid(tree) == 0);                              // no cloud while alive

        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        vm.callOn(bv, "take_damage", lethal);
        CHECK(!sField(bloater, "alive")->boolean);                // it died
        CHECK(activeAcid(tree) >= 1);                             // ...and left a toxic cloud behind
    }

    // Bloater killed while burning: its volatile gas is already alight, so it erupts into a FIRE patch
    // instead of a toxic cloud — torching a bloater denies its poison and hands you a blaze. (A non-burning
    // bloater still leaves acid, covered above.)
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* bloater = tree.findNode("Zombie0");
        Value bv = bloater->script();
        std::vector<Value> sp = {Value::fromNum(20.0), Value::fromNum(0.0),
                                 Value::fromNum(10.0), Value::fromNum(3.0)}; // spawn(x,y,kind=10,wave=3)
        vm.callOn(bv, "spawn", sp);
        std::vector<Value> ign = {Value::fromNum(3.0), Value::fromNum(10.0)};  // ignite(dur, dps)
        vm.callOn(bv, "ignite", ign);
        CHECK(sField(bloater, "burn_timer")->number > 0.0);       // on fire when it dies

        auto activeFires = [](SceneTree& t) {
            int c = 0;
            for (SceneNode* f : t.nodesInGroup("fires"))
                if (f->script().instance->findField("active")->boolean) ++c;
            return c;
        };
        CHECK(activeFires(tree) == 0);
        CHECK(activeAcid(tree) == 0);

        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        vm.callOn(bv, "take_damage", lethal);
        CHECK(!sField(bloater, "alive")->boolean);                // it died burning
        CHECK(activeFires(tree) >= 1);                            // gas ignited → a fire patch, not poison
        CHECK(activeAcid(tree) == 0);                             // no toxic cloud when it burns
    }

    // Acid puddles: a spitter's glob leaves a caustic patch where it lands, and the survivor loses health
    // while standing in it — but is safe just outside the radius. Also: a spitter's spit spawns a puddle.
    {
        // DoT case: puddle right on the survivor chews their health over a few ticks.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* acid = tree.findNode("Acid0");
        Value av = acid->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};   // splat on the survivor
        vm.callOn(av, "splat_at", at);
        CHECK(sField(acid, "active")->boolean);
        const double h0 = sField(survivor, "health")->number;
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 90; ++i) vm.callOn(av, "_process", dt);   // ~1.5 s of standing in it
        CHECK(sField(survivor, "health")->number < h0);              // it ate away at the survivor

        // Safe case: a puddle far from the survivor never touches their health.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* acid2 = t2.findNode("Acid0");
        Value a2v = acid2->script();
        std::vector<Value> far = {Value::fromNum(40.0), Value::fromNum(0.0)};
        vm2.callOn(a2v, "splat_at", far);
        const double h2 = sField(surv2, "health")->number;
        for (int i = 0; i < 90; ++i) vm2.callOn(a2v, "_process", dt);
        CHECK(sField(surv2, "health")->number == h2);               // out of the puddle — unharmed

        // A spitter's glob leaves a puddle: launch one, land it, and a puddle should be active.
        SceneTree t3;
        SceneNode* surv3 = zomboid::buildScene(t3);
        surv3->setPosition(0.0, 0.0);
        auto& vm3 = t3.scripts().vm();
        SceneNode* spit = t3.findNode("Spit0");
        Value spv = spit->script();
        std::vector<Value> lp = {Value::fromNum(3.0), Value::fromNum(0.0),
                                 Value::fromNum(0.0), Value::fromNum(0.0)}; // launch(x,y,tx,ty)
        vm3.callOn(spv, "launch", lp);
        CHECK(activeAcid(t3) == 0);                                  // none yet, glob still in flight
        for (int i = 0; i < 30 && activeAcid(t3) == 0; ++i) vm3.callOn(spv, "_process", dt);
        CHECK(activeAcid(t3) >= 1);                                  // the glob landed and left acid
    }

    // Acid corrodes the horde too: a zombie standing in a caustic puddle is bogged down (slowed),
    // refreshed each tick, while a zombie just outside the radius is untouched. This makes a spitter's
    // own puddle a double-edged battlefield the survivor can kite the swarm through.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(100.0, 100.0);   // park the survivor well clear of the pool
        auto& vm = tree.scripts().vm();
        SceneNode* inside = tree.findNode("Zombie0");   // 2.0 units — inside the radius-3.2 pool
        SceneNode* outside = tree.findNode("Zombie1");  // 20.0 units — well outside
        auto place = [&](SceneNode* z, double x) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(100.0), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        };
        place(inside, 2.0);
        place(outside, 20.0);
        SceneNode* acid = tree.findNode("Acid0");
        Value av = acid->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        vm.callOn(av, "splat_at", at);
        CHECK(sField(inside, "slow_timer")->number == 0.0);    // neither bogged down yet
        CHECK(sField(outside, "slow_timer")->number == 0.0);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 5; ++i) vm.callOn(av, "_process", dt);   // enough to land one caustic tick
        CHECK(sField(inside, "slow_timer")->number > 0.0);     // corroded — bogged down by the sludge
        CHECK(sField(outside, "slow_timer")->number == 0.0);   // clear of the pool — unaffected
    }

    // Bleed / laceration: kinetic rounds open a bleeding wound that ticks damage over time. Stacks
    // build with sustained fire, cap at 5, and a body left alone keeps hemorrhaging until the wound
    // closes. A zombie that was never hit takes no bleed damage.
    {
        // DoT case: an applied bleed chews health over a couple of seconds of standing there.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* z = tree.findNode("Zombie0");
        Value zv = z->script();
        std::vector<Value> sp = {Value::fromNum(5.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};   // parked walker, hp 100
        vm.callOn(zv, "spawn_at", sp);
        std::vector<Value> b3 = {Value::fromNum(3.0)};
        vm.callOn(zv, "apply_bleed", b3);
        CHECK((int)sField(z, "bleed_stacks")->number == 3);
        CHECK(sField(z, "bleed_timer")->number > 0.0);
        const double h0 = sField(z, "health")->number;
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 120; ++i) vm.callOn(zv, "_process", dt);   // ~2 s
        CHECK(sField(z, "health")->number < h0);                        // bled for real

        // Cap: stacks never exceed 5 no matter how many hits land.
        std::vector<Value> b10 = {Value::fromNum(10.0)};
        vm.callOn(zv, "apply_bleed", b10);
        CHECK((int)sField(z, "bleed_stacks")->number == 5);

        // Control: a never-hit zombie loses no health from the same idle stepping.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* z2 = t2.findNode("Zombie0");
        Value z2v = z2->script();
        std::vector<Value> sp2 = {Value::fromNum(5.0), Value::fromNum(0.0),
                                  Value::fromNum(100.0), Value::fromNum(0.0)};
        vm2.callOn(z2v, "spawn_at", sp2);
        const double hc = sField(z2, "health")->number;
        for (int i = 0; i < 120; ++i) vm2.callOn(z2v, "_process", dt);
        CHECK(sField(z2, "health")->number == hc);                      // no wound, no bleed
        CHECK((int)sField(z2, "bleed_stacks")->number == 0);

        // Wiring: an actual bullet impact opens a bleed on the zombie it strikes.
        SceneTree t3;
        SceneNode* surv3 = zomboid::buildScene(t3);
        surv3->setPosition(0.0, 0.0);
        auto& vm3 = t3.scripts().vm();
        SceneNode* z3 = t3.findNode("Zombie0");
        Value z3v = z3->script();
        std::vector<Value> sp3 = {Value::fromNum(3.0), Value::fromNum(0.0),
                                  Value::fromNum(200.0), Value::fromNum(0.0)};   // tanky so it survives
        vm3.callOn(z3v, "spawn_at", sp3);
        sField(surv3, "aim_x")->number = 1.0;
        sField(surv3, "aim_y")->number = 0.0;
        Value s3v = surv3->script();
        std::vector<Value> none;
        vm3.callOn(s3v, "do_shoot", none);                              // one pistol round downrange
        for (int i = 0; i < 20 && (int)sField(z3, "bleed_stacks")->number == 0; ++i)
            t3.process(1.0 / 60.0);
        CHECK((int)sField(z3, "bleed_stacks")->number >= 1);            // the hit lacerated it
    }

    // Stagger / flinch: a heavy single blow (>=40% of full health) that doesn't kill roots the zombie
    // in place for a fraction of a second; a light hit doesn't; and a cooldown stops it being re-locked.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Heavy hit roots: a staggered walker makes no headway toward the survivor while flinching.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* z = tree.findNode("Zombie0");
        Value zv = z->script();
        std::vector<Value> sp = {Value::fromNum(10.0), Value::fromNum(0.0),
                                 Value::fromNum(300.0), Value::fromNum(20.0)};   // hp 300, speed 20
        vm.callOn(zv, "spawn_at", sp);
        std::vector<Value> heavy = {Value::fromNum(130.0)};   // 130 >= 40% of 300 → staggers
        vm.callOn(zv, "take_damage", heavy);
        CHECK(sField(z, "stagger_timer")->number > 0.0);
        const double xRooted = z->x();
        for (int i = 0; i < 10; ++i) vm.callOn(zv, "_process", dt);   // ~0.17 s, still flinching
        CHECK(sField(z, "stagger_timer")->number > 0.0);
        CHECK(z->x() == xRooted);                                     // never moved while rooted

        // Light hit doesn't stagger, and the zombie keeps advancing.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* z2 = t2.findNode("Zombie0");
        Value z2v = z2->script();
        std::vector<Value> sp2 = {Value::fromNum(10.0), Value::fromNum(0.0),
                                  Value::fromNum(300.0), Value::fromNum(20.0)};
        vm2.callOn(z2v, "spawn_at", sp2);
        std::vector<Value> light = {Value::fromNum(50.0)};    // 50 < 40% of 300 → no stagger
        vm2.callOn(z2v, "take_damage", light);
        CHECK(sField(z2, "stagger_timer")->number == 0.0);
        const double xStart = z2->x();
        for (int i = 0; i < 5; ++i) vm2.callOn(z2v, "_process", dt);
        CHECK(z2->x() < xStart);                                      // closed distance normally

        // Cooldown: a second heavy hit landed while still on cooldown does NOT re-stagger.
        SceneTree t3;
        SceneNode* surv3 = zomboid::buildScene(t3);
        surv3->setPosition(0.0, 0.0);
        auto& vm3 = t3.scripts().vm();
        SceneNode* z3 = t3.findNode("Zombie0");
        Value z3v = z3->script();
        std::vector<Value> sp3 = {Value::fromNum(10.0), Value::fromNum(0.0),
                                  Value::fromNum(300.0), Value::fromNum(20.0)};
        vm3.callOn(z3v, "spawn_at", sp3);
        vm3.callOn(z3v, "take_damage", heavy);                        // first stagger
        for (int i = 0; i < 30; ++i) vm3.callOn(z3v, "_process", dt); // ~0.5 s: flinch ends, cd still up
        CHECK(sField(z3, "stagger_timer")->number == 0.0);
        vm3.callOn(z3v, "take_damage", heavy);                        // heavy again, but on cooldown
        CHECK(sField(z3, "stagger_timer")->number == 0.0);            // blocked — no re-lock
        CHECK(sField(z3, "alive")->boolean);                          // 300-130-130 = 40, still up
    }

    // Vampiric power-up (kind 5): while active, each kinetic bullet hit leeches a little health back
    // to a wounded survivor — but only while the buff lasts, and never without it.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Leech case: buffed + wounded, a bullet hit heals the survivor.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* z = tree.findNode("Zombie0");
        Value zv = z->script();
        std::vector<Value> sp = {Value::fromNum(3.0), Value::fromNum(0.0),
                                 Value::fromNum(400.0), Value::fromNum(0.0)};   // tanky, survives the shot
        vm.callOn(zv, "spawn_at", sp);
        Value sv = survivor->script();
        std::vector<Value> vamp = {Value::fromNum(5.0)};
        vm.callOn(sv, "grant_powerup", vamp);
        CHECK((int)sField(survivor, "buff_kind")->number == 5);
        sField(survivor, "health")->number = 50.0;                 // wounded (max 100), room to heal
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;
        std::vector<Value> none;
        vm.callOn(sv, "do_shoot", none);
        for (int i = 0; i < 20 && sField(survivor, "health")->number == 50.0; ++i)
            tree.process(1.0 / 60.0);
        CHECK(sField(survivor, "health")->number > 50.0);          // the hit leeched health back

        // Control: no buff, the same wounded shot heals nothing.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* z2 = t2.findNode("Zombie0");
        Value z2v = z2->script();
        std::vector<Value> sp2 = {Value::fromNum(3.0), Value::fromNum(0.0),
                                  Value::fromNum(400.0), Value::fromNum(0.0)};
        vm2.callOn(z2v, "spawn_at", sp2);
        Value s2v = surv2->script();
        sField(surv2, "health")->number = 50.0;
        sField(surv2, "aim_x")->number = 1.0;
        sField(surv2, "aim_y")->number = 0.0;
        vm2.callOn(s2v, "do_shoot", none);
        for (int i = 0; i < 20; ++i) t2.process(1.0 / 60.0);
        CHECK(sField(surv2, "health")->number == 50.0);            // no buff → no leech

        // Expiry: once the buff timer runs out, lifesteal stops.
        SceneTree t3;
        SceneNode* surv3 = zomboid::buildScene(t3);
        surv3->setPosition(0.0, 0.0);
        auto& vm3 = t3.scripts().vm();
        Value s3v = surv3->script();
        vm3.callOn(s3v, "grant_powerup", vamp);
        sField(surv3, "buff_timer")->number = 0.0;                 // force the buff expired
        Value active = vm3.callOn(s3v, "lifesteal_active", none);
        CHECK(!active.boolean);
    }

    // Field Medic power-up (kind 9): a sustained heal-over-time. While active it steadily mends the
    // survivor (capped at full), even with no combat happening — distinct from Vampiric (heal on hit)
    // and the passive out-of-combat regen. Once the buff lapses, the trickle stops.
    {
        std::vector<Value> none;
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(5000.0, 5000.0);   // far from any zombie — isolate the heal-over-time
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();
        sField(survivor, "health")->number = 40.0;    // wounded (max 100)
        std::vector<Value> medic = {Value::fromNum(9.0)};
        vm.callOn(sv, "grant_powerup", medic);
        CHECK((int)sField(survivor, "buff_kind")->number == 9);
        const double h0 = sField(survivor, "health")->number;
        for (int i = 0; i < 60; ++i) vm.callOn(sv, "_process", dt);   // ~1s of Field Medic
        const double h1 = sField(survivor, "health")->number;
        CHECK(h1 > h0);                                // it steadily mended the survivor
        CHECK(h1 <= 100.0);                            // ...never past full

        // Never overheals: at full health the trickle adds nothing.
        sField(survivor, "health")->number = 100.0;
        sField(survivor, "buff_timer")->number = 5.0;   // keep the buff alive
        for (int i = 0; i < 30; ++i) vm.callOn(sv, "_process", dt);
        CHECK(sField(survivor, "health")->number == 100.0);

        // Expiry: once the buff lapses, the heal-over-time stops (a wounded survivor stays wounded).
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(5000.0, 5000.0);
        auto& vm2 = t2.scripts().vm();
        Value sv2 = surv2->script();
        sField(surv2, "health")->number = 40.0;
        sField(surv2, "hunger")->number = 100.0;   // starving + no rations suppresses the out-of-combat
        sField(surv2, "food")->number = 0.0;       // regen, so only the (expired) buff could raise health
        vm2.callOn(sv2, "grant_powerup", medic);
        sField(surv2, "buff_timer")->number = 0.0;   // force expired
        const double e0 = sField(surv2, "health")->number;
        for (int i = 0; i < 60; ++i) vm2.callOn(sv2, "_process", dt);
        CHECK(sField(surv2, "health")->number <= e0);   // no buff → no heal-over-time (never rises)
    }

    // Screamer (kind 11): a fragile support zombie that periodically shrieks, whipping nearby zombies
    // into a speed frenzy. A frenzied zombie covers more ground per second; a far one is untouched;
    // and silencing (killing) the screamer means its shriek never lands.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Frenzy case: the screamer shrieks and a nearby zombie speeds up (covers more ground).
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* screamer = tree.findNode("Zombie0");
        SceneNode* near_ = tree.findNode("Zombie1");   // 6 units from screamer — inside radius 15
        SceneNode* farZ = tree.findNode("Zombie2");    // 40 units — outside
        // Screamer spawned as kind 11 via the full spawn() so it runs its shriek logic.
        Value scv = screamer->script();
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(11.0), Value::fromNum(5.0)};   // spawn(x,y,kind,wave)
        vm.callOn(scv, "spawn", sp);
        CHECK((int)sField(screamer, "kind")->number == 11);
        auto park = [&](SceneNode* z, double x) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(100.0), Value::fromNum(10.0)};  // walker, speed 10
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        };
        park(near_, 24.0);   // 6 from the screamer at x=30
        park(farZ, -10.0);   // 40 from the screamer
        // Run enough frames for the screamer's 5 s cooldown to fire at least once (~6 s).
        for (int i = 0; i < 360; ++i) {
            vm.callOn(scv, "_process", dt);
        }
        CHECK(sField(near_, "frenzy_timer")->number > 0.0);   // caught the shriek
        CHECK(sField(farZ, "frenzy_timer")->number == 0.0);   // too far — untouched

        // A frenzied walker outruns an identical calm one over the same span.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* fz = t2.findNode("Zombie0");
        SceneNode* cz = t2.findNode("Zombie1");
        auto park2 = [&](SceneNode* z, double x) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(100.0), Value::fromNum(10.0)};
            t2.scripts().vm().callOn(zv, "spawn_at", a);
        };
        park2(fz, 40.0);
        park2(cz, 40.0);
        Value fzv = fz->script();
        std::vector<Value> fren = {Value::fromNum(2.0)};
        vm2.callOn(fzv, "apply_frenzy", fren);
        Value czv = cz->script();
        const double fx0 = fz->x();
        const double cx0 = cz->x();
        for (int i = 0; i < 30; ++i) {   // 0.5 s of chasing the survivor at the origin
            vm2.callOn(fzv, "_process", dt);
            vm2.callOn(czv, "_process", dt);
        }
        const double fzMoved = fx0 - fz->x();   // both close toward the origin (x decreasing)
        const double czMoved = cx0 - cz->x();
        CHECK(fzMoved > czMoved);   // the frenzied one covered more ground
    }
    // Back-line retreat: a Screamer and a Healer hold their distance — they back away from an approaching
    // survivor rather than shambling into melee, matching their documented back-line support role (like the
    // summoner). Cast cooldown is set high so we observe pure movement, not a rooted wind-up.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        // Screamer close to the survivor retreats: its x grows, moving away from the origin.
        SceneTree t1;
        SceneNode* s1 = zomboid::buildScene(t1);
        s1->setPosition(0.0, 0.0);
        auto& vm1 = t1.scripts().vm();
        SceneNode* scr = t1.findNode("Zombie0");
        Value scv = scr->script();
        std::vector<Value> sp11 = {Value::fromNum(5.0), Value::fromNum(0.0),
                                   Value::fromNum(11.0), Value::fromNum(5.0)};
        vm1.callOn(scv, "spawn", sp11);
        sField(scr, "cooldown")->number = 10.0;   // suppress a shriek wind-up so movement is pure retreat
        const double sx0 = scr->x();
        for (int i = 0; i < 30; ++i) { vm1.callOn(scv, "_process", dt); }
        CHECK(scr->x() > sx0);   // backed away from the survivor at the origin

        // Healer likewise holds its distance.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* heal = t2.findNode("Zombie0");
        Value hv = heal->script();
        std::vector<Value> sp12 = {Value::fromNum(5.0), Value::fromNum(0.0),
                                   Value::fromNum(12.0), Value::fromNum(5.0)};
        vm2.callOn(hv, "spawn", sp12);
        sField(heal, "cooldown")->number = 10.0;   // suppress a mend wind-up
        const double hx0 = heal->x();
        for (int i = 0; i < 30; ++i) { vm2.callOn(hv, "_process", dt); }
        CHECK(heal->x() > hx0);   // backed away
    }

    // Boss is immune to frenzy: a screamer's shriek can't stack a speed frenzy on the wave leader (which
    // is tuned by its own enrage), but a normal zombie is still whipped up.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        std::vector<Value> fren = {Value::fromNum(2.0)};

        SceneNode* boss = tree.findNode("Zombie0");
        Value bvv = boss->script();
        std::vector<Value> bs = {Value::fromNum(20.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(5.0)}; // boss (kind 3)
        vm.callOn(bvv, "spawn", bs);
        vm.callOn(bvv, "apply_frenzy", fren);
        CHECK(sField(boss, "frenzy_timer")->number == 0.0);   // boss shrugs off the frenzy

        SceneNode* walker = tree.findNode("Zombie1");
        Value wv2 = walker->script();
        std::vector<Value> ws = {Value::fromNum(20.0), Value::fromNum(0.0), Value::fromNum(100.0),
                                 Value::fromNum(0.0)};   // spawn_at → walker
        vm.callOn(wv2, "spawn_at", ws);
        vm.callOn(wv2, "apply_frenzy", fren);
        CHECK(sField(walker, "frenzy_timer")->number > 0.0); // a normal body is frenzied
    }

    // Healer (kind 12): a back-line medic that, on a cooldown, mends every wounded zombie inside a
    // radius by a chunk of their max health — capped at full, never past it — while a far zombie is
    // left to bleed. It never heals itself and never top-heals a zombie already at full.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();

        SceneNode* healer = tree.findNode("Zombie0");
        SceneNode* nearLow = tree.findNode("Zombie1");    // 6 units away, badly wounded
        SceneNode* nearHigh = tree.findNode("Zombie2");   // 6 units away, barely wounded (cap check)
        SceneNode* farZ = tree.findNode("Zombie3");       // 50 units away — outside radius 14

        Value hv = healer->script();
        std::vector<Value> hs = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(12.0), Value::fromNum(5.0)};   // spawn(x,y,kind,wave)
        vm.callOn(hv, "spawn", hs);
        CHECK((int)sField(healer, "kind")->number == 12);
        sField(healer, "speed")->number = 0.0;   // pin it in place so the heal radius stays fixed

        auto park = [&](SceneNode* z, double x, double hp) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(100.0), Value::fromNum(0.0)};  // walker, full=100
            tree.scripts().vm().callOn(zv, "spawn_at", a);
            sField(z, "health")->number = hp;   // wound it (max_health stays 100)
        };
        park(nearLow, 24.0, 20.0);    // 6 from the healer at x=30
        park(nearHigh, 36.0, 90.0);   // 6 from the healer, near full
        park(farZ, -20.0, 20.0);      // 50 from the healer

        // Drive only the healer: its cooldown starts at 0, so it begins a mend wind-up on the first
        // frame; ~0.6 s of telegraph later the heal pulse lands, then it sits on its 4 s cooldown. Run
        // 60 frames (1 s) so exactly one mend resolves. The parked patients don't move.
        for (int i = 0; i < 60; ++i) { vm.callOn(hv, "_process", dt); }

        CHECK(sField(nearLow, "health")->number == 45.0);    // 20 + 25% of 100
        CHECK(sField(nearHigh, "health")->number == 100.0);  // 90 + 25 clamped to max, not 115
        CHECK(sField(farZ, "health")->number == 20.0);       // out of range — untouched
    }

    // Healer can't mend a burning or bleeding body: damage-over-time holds the wound open against all
    // healing, so a patient you've set alight or lacerated is skipped by the mend even in range — while a
    // clean-wounded neighbour is patched. Torching the pack shuts the healer down, not just its own regen.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();

        SceneNode* healer = tree.findNode("Zombie0");
        Value hv = healer->script();
        std::vector<Value> hs = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(12.0), Value::fromNum(5.0)};
        vm.callOn(hv, "spawn", hs);
        sField(healer, "speed")->number = 0.0;   // pin it so the heal radius stays fixed

        auto park = [&](SceneNode* z, double x, double y) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(y),
                                    Value::fromNum(100.0), Value::fromNum(0.0)};  // walker, full=100
            tree.scripts().vm().callOn(zv, "spawn_at", a);
            sField(z, "health")->number = 20.0;   // wound it (max_health stays 100)
        };
        SceneNode* burning = tree.findNode("Zombie1");
        SceneNode* bleeding = tree.findNode("Zombie2");
        SceneNode* clean = tree.findNode("Zombie3");
        park(burning, 24.0, 0.0);    // 6 from the healer, in range
        park(bleeding, 36.0, 0.0);   // 6 from the healer, in range
        park(clean, 30.0, 12.0);     // 12 from the healer, in range
        sField(burning, "burn_timer")->number = 2.0;    // actively on fire
        sField(bleeding, "bleed_stacks")->number = 1.0; // actively bleeding

        // Drive only the healer (patients aren't processed, so their DoT doesn't tick — this isolates the
        // mend). One mend resolves within the first second.
        for (int i = 0; i < 60; ++i) { vm.callOn(hv, "_process", dt); }

        CHECK(sField(burning, "health")->number == 20.0);   // on fire → mend skips it
        CHECK(sField(bleeding, "health")->number == 20.0);  // bleeding → mend skips it
        CHECK(sField(clean, "health")->number == 45.0);     // clean wound → mended (20 + 25% of 100)
    }

    // Healer can't mend a CHILLED body either: cold holds the wound open exactly like burn/bleed, so the
    // mend's anti-heal rule is truly unified across damage-over-time AND cryo — matching the Regenerator
    // mutator and the boss enrage-heal, which both already halt on chill. A frozen patient in range is
    // skipped while a clean-wounded neighbour is patched. (This is a chilled PATIENT, not a chilled healer
    // — the caster here is unfrozen and casting normally; it's the target's frost that blocks the mend.)
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();

        SceneNode* healer = tree.findNode("Zombie0");
        Value hv = healer->script();
        std::vector<Value> hs = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(12.0), Value::fromNum(5.0)};
        vm.callOn(hv, "spawn", hs);
        sField(healer, "speed")->number = 0.0;   // pin it so the heal radius stays fixed

        auto park = [&](SceneNode* z, double x, double y) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(y),
                                    Value::fromNum(100.0), Value::fromNum(0.0)};  // walker, full=100
            tree.scripts().vm().callOn(zv, "spawn_at", a);
            sField(z, "health")->number = 20.0;   // wound it (max_health stays 100)
        };
        SceneNode* chilled = tree.findNode("Zombie1");
        SceneNode* clean = tree.findNode("Zombie2");
        park(chilled, 24.0, 0.0);    // 6 from the healer, in range
        park(clean, 30.0, 12.0);     // 12 from the healer, in range
        sField(chilled, "slow_timer")->number = 2.0;   // frozen — cold holds the wound open

        for (int i = 0; i < 60; ++i) { vm.callOn(hv, "_process", dt); }

        CHECK(sField(chilled, "health")->number == 20.0);   // chilled → mend skips it (unified with burn/bleed)
        CHECK(sField(clean, "health")->number == 45.0);     // clean wound → mended (20 + 25% of 100)
    }

    // Healer never mends the boss: the wave leader is exempt (like stagger/gib/overkill/Volatile), so a
    // healer beside a wounded boss can't refund its huge health bar — but it still mends a nearby walker.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();

        SceneNode* healer = tree.findNode("Zombie0");
        Value hv = healer->script();
        std::vector<Value> hs = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(12.0), Value::fromNum(5.0)};
        vm.callOn(hv, "spawn", hs);
        sField(healer, "speed")->number = 0.0;

        // A wounded boss parked next to the healer (dist 6, inside radius 14).
        SceneNode* boss = tree.findNode("Zombie1");
        Value bvv = boss->script();
        std::vector<Value> bs = {Value::fromNum(36.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(5.0)}; // boss (kind 3)
        vm.callOn(bvv, "spawn", bs);
        sField(boss, "speed")->number = 0.0;   // pin it so it stays in range
        const double bossMax = sField(boss, "max_health")->number;
        sField(boss, "health")->number = bossMax - 200.0;   // clearly wounded

        // A wounded walker also parked next to the healer (control — it does get mended).
        SceneNode* walker = tree.findNode("Zombie2");
        Value wv2 = walker->script();
        std::vector<Value> ws = {Value::fromNum(24.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)}; // spawn_at → walker, full 100
        vm.callOn(wv2, "spawn_at", ws);
        sField(walker, "health")->number = 20.0;

        for (int i = 0; i < 60; ++i) { vm.callOn(hv, "_process", dt); }

        CHECK(sField(boss, "health")->number == bossMax - 200.0);  // boss NOT mended — exempt
        CHECK(sField(walker, "health")->number > 20.0);            // the walker was mended
    }

    // Cryo silences the back line: a chilled healer can't mend — a frozen caster can't work its
    // ability, so cryo shuts it down until the chill wears off (same rule for screamer/summoner).
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* healer = tree.findNode("Zombie0");
        SceneNode* patient = tree.findNode("Zombie1");    // 6 units away, wounded

        Value hv = healer->script();
        std::vector<Value> hs = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(12.0), Value::fromNum(5.0)};
        vm.callOn(hv, "spawn", hs);
        sField(healer, "speed")->number = 0.0;            // pin it in place
        sField(healer, "slow_timer")->number = 2.0;       // ...and chill it — heal is silenced

        Value pv = patient->script();
        std::vector<Value> ps = {Value::fromNum(24.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};
        vm.callOn(pv, "spawn_at", ps);
        sField(patient, "health")->number = 20.0;         // badly wounded

        for (int i = 0; i < 10; ++i) { vm.callOn(hv, "_process", dt); }
        CHECK(sField(patient, "health")->number == 20.0); // no mend landed — the chill silenced it
    }

    // Combo-scaled salvage: cash per kill grows with the streak multiplier — the same zombie pays
    // more killed on a hot streak (×3) than cold (×1), rewarding sustained aggression.
    {
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        std::vector<Value> sp = {Value::fromNum(50.0), Value::fromNum(0.0),
                                 Value::fromNum(10.0), Value::fromNum(0.0)};  // walker, score 10

        // Cold kill: fresh combo (multiplier ×1) → base salvage only.
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        Value zv1 = t1.findNode("Zombie0")->script();
        vm1.callOn(zv1, "spawn_at", sp);
        vm1.callOn(zv1, "take_damage", lethal);
        CHECK((int)glob(t1, "g_mult") == 1);
        CHECK(glob(t1, "g_cash") == 7.0);     // 5 + int(10/4), ×1 multiplier, no combo bonus

        // Hot kill: preload the streak so this kill lands at ×3 → base + 100% bonus.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_combo"))->number = 12.0;   // → 13 after the kill, mult 3
        Value zv2 = t2.findNode("Zombie0")->script();
        vm2.callOn(zv2, "spawn_at", sp);
        const double c0 = glob(t2, "g_cash");
        vm2.callOn(zv2, "take_damage", lethal);
        CHECK((int)glob(t2, "g_mult") == 3);
        CHECK(glob(t2, "g_cash") - c0 == 14.0);   // salvage 7 + int(7*2/2) = 14
    }

    // Weak-point window: a staggered (flinching) zombie takes 40% more damage from a hit, rewarding
    // following a stagger — from a melee shove or dash-strike — with fire. A calm zombie takes the
    // hit at face value.
    {
        std::vector<Value> hit = {Value::fromNum(20.0)};
        std::vector<Value> stag = {Value::fromNum(1.0)};

        // Control: an un-staggered walker loses exactly the hit amount.
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        Value zv1 = t1.findNode("Zombie0")->script();
        std::vector<Value> sp = {Value::fromNum(50.0), Value::fromNum(0.0),
                                 Value::fromNum(200.0), Value::fromNum(0.0)};  // walker, 200 hp
        vm1.callOn(zv1, "spawn_at", sp);
        vm1.callOn(zv1, "take_damage", hit);
        CHECK(sField(t1.findNode("Zombie0"), "health")->number == 180.0);   // 200 - 20

        // Staggered: the same hit bites 40% deeper (28 instead of 20).
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        Value zv2 = t2.findNode("Zombie0")->script();
        vm2.callOn(zv2, "spawn_at", sp);
        vm2.callOn(zv2, "stagger", stag);                                   // flinch it first
        CHECK(sField(t2.findNode("Zombie0"), "stagger_timer")->number > 0.0);
        vm2.callOn(zv2, "take_damage", hit);
        CHECK(sField(t2.findNode("Zombie0"), "health")->number == 172.0);   // 200 - 20*1.4
    }

    // Bulwark mutator (g_mutator == 4): the whole horde spawns carrying a damage-absorbing shield,
    // even a plain walker — so a normally-shieldless zombie must be broken down first. Off, it has none.
    {
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(0.0), Value::fromNum(5.0)};  // spawn(x,y,walker,wave 5)

        // Bulwark on: a walker gets a shield of 15 + wave*2 = 25 at wave 5.
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        const_cast<Value*>(vm1.getGlobal("g_mutator"))->number = 4.0;
        Value zv1 = t1.findNode("Zombie0")->script();
        vm1.callOn(zv1, "spawn", sp);
        CHECK(sField(t1.findNode("Zombie0"), "shield")->number == 25.0);

        // Bulwark off: the same walker has no shield.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_mutator"))->number = 0.0;
        Value zv2 = t2.findNode("Zombie0")->script();
        vm2.callOn(zv2, "spawn", sp);
        CHECK(sField(t2.findNode("Zombie0"), "shield")->number == 0.0);
    }

    // Volatile Horde mutator (g_mutator == 5): every non-boss body ruptures into a caustic pool where
    // it falls, so the arena fills with hazard as the fight drags on. A plain walker — which normally
    // leaves nothing — drops a puddle when it dies under this mutator, but not with the mutator off.
    {
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(0.0), Value::fromNum(3.0)};  // walker at (30,0), wave 3
        std::vector<Value> lethal = {Value::fromNum(9999.0)};

        // Volatile on: killing the walker leaves a caustic puddle behind.
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        const_cast<Value*>(vm1.getGlobal("g_mutator"))->number = 5.0;
        Value zv1 = t1.findNode("Zombie0")->script();
        vm1.callOn(zv1, "spawn", sp);
        CHECK(activeAcid(t1) == 0);                     // no hazard while it's alive
        vm1.callOn(zv1, "take_damage", lethal);
        CHECK(!sField(t1.findNode("Zombie0"), "alive")->boolean);
        CHECK(activeAcid(t1) >= 1);                     // ...ruptured into a caustic pool on death

        // Volatile off: the same walker leaves nothing when it dies.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_mutator"))->number = 0.0;
        Value zv2b = t2.findNode("Zombie0")->script();
        vm2.callOn(zv2b, "spawn", sp);
        vm2.callOn(zv2b, "take_damage", lethal);
        CHECK(!sField(t2.findNode("Zombie0"), "alive")->boolean);
        CHECK(activeAcid(t2) == 0);                     // a plain walker leaves no hazard
    }

    // Regenerator Horde mutator (g_mutator == 6): every body knits its wounds back over time, so chip
    // damage bleeds away and you must commit real burst to a kill. Two hard counters: the regen halts
    // while the body is chilled (slowed), and it's exempt for bosses. Verify a wounded walker recovers
    // health under the mutator, stays flat with the mutator off, and stays flat while frozen.
    {
        std::vector<Value> sp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                 Value::fromNum(0.0), Value::fromNum(3.0)};  // walker at (30,0), wave 3
        std::vector<Value> hurt = {Value::fromNum(10.0)};
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> chill = {Value::fromNum(4.0)};

        // Regenerator on: a wounded walker heals over a second of ticks (but not past its max).
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        const_cast<Value*>(vm1.getGlobal("g_mutator"))->number = 6.0;
        SceneNode* z1 = t1.findNode("Zombie0");
        Value zv1 = z1->script();
        vm1.callOn(zv1, "spawn", sp);
        vm1.callOn(zv1, "take_damage", hurt);
        const double wounded = sField(z1, "health")->number;
        const double maxh = sField(z1, "max_health")->number;
        CHECK(wounded < maxh);                          // actually took a wound
        for (int i = 0; i < 60; ++i) { vm1.callOn(zv1, "_process", dt); }
        const double healed = sField(z1, "health")->number;
        CHECK(healed > wounded);                        // regenerated some health back
        CHECK(healed <= maxh + 0.001);                  // but never above its max

        // Regenerator off: the same wound never closes.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_mutator"))->number = 0.0;
        SceneNode* z2 = t2.findNode("Zombie0");
        Value zv2 = z2->script();
        vm2.callOn(zv2, "spawn", sp);
        vm2.callOn(zv2, "take_damage", hurt);
        const double wounded2 = sField(z2, "health")->number;
        for (int i = 0; i < 60; ++i) { vm2.callOn(zv2, "_process", dt); }
        CHECK(sField(z2, "health")->number == wounded2);   // no mutator → no self-heal

        // Chilled counter: under the mutator, a frozen body's regen is silenced — health stays flat.
        SceneTree t3;
        zomboid::buildScene(t3);
        auto& vm3 = t3.scripts().vm();
        const_cast<Value*>(vm3.getGlobal("g_mutator"))->number = 6.0;
        SceneNode* z3 = t3.findNode("Zombie0");
        Value zv3 = z3->script();
        vm3.callOn(zv3, "spawn", sp);
        vm3.callOn(zv3, "take_damage", hurt);
        vm3.callOn(zv3, "apply_slow", chill);              // freeze it
        const double wounded3 = sField(z3, "health")->number;
        for (int i = 0; i < 30; ++i) { vm3.callOn(zv3, "_process", dt); }  // 0.5s, still chilled
        CHECK(sField(z3, "health")->number == wounded3);   // frozen → no regen

        // Burning counter: a burning body can't knit its wounds — regen halts entirely, so even a LIGHT
        // burn (4 dps) beats the regen on a tanky wave-10 body (~6.3 hp/s of regen), which the old
        // out-damage-only rule couldn't. Health strictly falls while it cooks.
        std::vector<Value> bigSp = {Value::fromNum(30.0), Value::fromNum(0.0),
                                    Value::fromNum(0.0), Value::fromNum(10.0)};  // wave-10 walker (~105 hp)
        std::vector<Value> wound20 = {Value::fromNum(20.0)};
        std::vector<Value> lightBurn = {Value::fromNum(2.0), Value::fromNum(4.0)};  // ignite(dur, dps)
        SceneTree t4;
        zomboid::buildScene(t4);
        auto& vm4 = t4.scripts().vm();
        const_cast<Value*>(vm4.getGlobal("g_mutator"))->number = 6.0;
        SceneNode* z4 = t4.findNode("Zombie0");
        Value zv4 = z4->script();
        vm4.callOn(zv4, "spawn", bigSp);
        vm4.callOn(zv4, "take_damage", wound20);
        vm4.callOn(zv4, "ignite", lightBurn);
        const double wounded4 = sField(z4, "health")->number;
        for (int i = 0; i < 60; ++i) { vm4.callOn(zv4, "_process", dt); }  // 1s, still burning
        CHECK(sField(z4, "health")->number < wounded4);    // burning → regen off, net loss

        // Bleeding counter: same story for a single laceration stack (2.5 hp/s) on the tanky body —
        // regen halts while the wound is open, so health falls rather than climbing back.
        std::vector<Value> oneStack = {Value::fromNum(1.0)};
        SceneTree t5;
        zomboid::buildScene(t5);
        auto& vm5 = t5.scripts().vm();
        const_cast<Value*>(vm5.getGlobal("g_mutator"))->number = 6.0;
        SceneNode* z5 = t5.findNode("Zombie0");
        Value zv5 = z5->script();
        vm5.callOn(zv5, "spawn", bigSp);
        vm5.callOn(zv5, "take_damage", wound20);
        vm5.callOn(zv5, "apply_bleed", oneStack);
        const double wounded5 = sField(z5, "health")->number;
        for (int i = 0; i < 60; ++i) { vm5.callOn(zv5, "_process", dt); }  // 1s, still bleeding
        CHECK(sField(z5, "health")->number < wounded5);    // bleeding → regen off, net loss
    }

    // Fire ignites acid: a molotov's fire patch (or the flamethrower cone) touching a caustic puddle
    // flashes it over in one violent combustion — the puddle is consumed and any zombie in it takes a
    // burst of damage. Turns an enemy hazard (a spitter's acid, a Volatile-horde pool) into an offensive
    // tool. Verify overlapping fire combusts the pool and hurts a zombie standing in it; fire placed far
    // away leaves the pool untouched.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> at = {Value::fromNum(30.0), Value::fromNum(0.0)};
        // wave-8 walker so it survives the 30-damage flash and we can read the wound cleanly.
        std::vector<Value> sp = {Value::fromNum(31.0), Value::fromNum(0.0),
                                 Value::fromNum(0.0), Value::fromNum(8.0)};

        // Overlapping fire: the puddle combusts and the zombie in it is burned.
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        Value acid1 = t1.findNode("Acid0")->script();
        vm1.callOn(acid1, "splat_at", at);
        CHECK(sField(t1.findNode("Acid0"), "active")->boolean);   // puddle is live
        SceneNode* z1 = t1.findNode("Zombie0");
        Value zv1 = z1->script();
        vm1.callOn(zv1, "spawn", sp);
        const double hp1 = sField(z1, "health")->number;
        Value fire1 = t1.findNode("Fire0")->script();
        vm1.callOn(fire1, "ignite_ground", at);                   // fire right on the puddle
        vm1.callOn(fire1, "_process", dt);
        CHECK(!sField(t1.findNode("Acid0"), "active")->boolean);  // ...flashed over (consumed)
        CHECK(sField(z1, "health")->number < hp1);                // the zombie took the burst

        // Fire far from the puddle: no combustion, no harm.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        Value acid2 = t2.findNode("Acid0")->script();
        vm2.callOn(acid2, "splat_at", at);
        SceneNode* z2 = t2.findNode("Zombie0");
        Value zv2 = z2->script();
        vm2.callOn(zv2, "spawn", sp);
        const double hp2 = sField(z2, "health")->number;
        std::vector<Value> farAway = {Value::fromNum(90.0), Value::fromNum(90.0)};
        Value fire2 = t2.findNode("Fire0")->script();
        vm2.callOn(fire2, "ignite_ground", farAway);
        vm2.callOn(fire2, "_process", dt);
        CHECK(sField(t2.findNode("Acid0"), "active")->boolean);   // untouched puddle
        CHECK(sField(z2, "health")->number == hp2);               // zombie unharmed
    }

    // Barrel leaves fire: a popped explosive barrel spills burning fuel, leaving a lingering fire patch
    // where it stood — lasting area denial (and, being fire, it flashes over any acid it overlaps). No
    // fire burns at rest or from merely placing a barrel; only the blast lights one.
    {
        std::vector<Value> lethal = {Value::fromNum(999.0)};
        std::vector<Value> at = {Value::fromNum(40.0), Value::fromNum(0.0)};

        auto activeFires = [](SceneTree& t) {
            int c = 0;
            for (SceneNode* f : t.nodesInGroup("fires")) {
                if (f->script().instance->findField("active")->boolean) { c = c + 1; }
            }
            return c;
        };

        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        CHECK(activeFires(tree) == 0);                 // no ground fire at rest
        Value barrel = tree.findNode("Barrel0")->script();
        vm.callOn(barrel, "place", at);
        CHECK(activeFires(tree) == 0);                 // placing a barrel lights nothing
        vm.callOn(barrel, "take_damage", lethal);      // pop it
        CHECK(!sField(tree.findNode("Barrel0"), "active")->boolean);  // barrel is spent
        CHECK(activeFires(tree) >= 1);                 // ...and its rupture left a burning patch
    }

    // Between-wave upgrade cycle now includes a 7th pick: move speed. Cycling through a full round of
    // seven upgrades bumps the walk-speed multiplier exactly once (the k==6 pick), on top of the older
    // damage/rate/health/ammo/crit-chance/crit-damage picks — so mobility now grows over a long run.
    {
        std::vector<Value> none;
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();

        CHECK(sField(survivor, "move_mult")->number == 1.0);   // starts at base walk speed
        const double dmg0 = sField(survivor, "dmg_mult")->number;
        const double crit0 = sField(survivor, "crit_mult")->number;
        for (int i = 0; i < 8; ++i) { vm.callOn(sv, "apply_upgrade", none); }
        // One full cycle (eight picks): the move-speed pick landed once (+0.08), and the others fire too.
        CHECK(sField(survivor, "move_mult")->number > 1.0);
        CHECK(std::abs(sField(survivor, "move_mult")->number - 1.08) < 1e-9);
        CHECK(sField(survivor, "dmg_mult")->number > dmg0);    // +damage pick still applied
        CHECK(sField(survivor, "crit_mult")->number > crit0);  // +crit-damage pick still applied

        // A second full cycle stacks another speed increment.
        for (int i = 0; i < 8; ++i) { vm.callOn(sv, "apply_upgrade", none); }
        CHECK(std::abs(sField(survivor, "move_mult")->number - 1.16) < 1e-9);
    }

    // Shotgun point-blank knockback: a shotgun pellet (falloff) lands a heavy shove that fades with
    // travel, where a plain round gives only a light nudge — the shotgun's crowd-control identity.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> none;
        // fire(px, py, dirx, diry, speed, dmg): a bullet placed just behind a zombie at (10,0).
        std::vector<Value> shot = {Value::fromNum(8.0), Value::fromNum(0.0), Value::fromNum(1.0),
                                   Value::fromNum(0.0), Value::fromNum(70.0), Value::fromNum(5.0)};
        std::vector<Value> zsp = {Value::fromNum(10.0), Value::fromNum(0.0),
                                  Value::fromNum(0.0), Value::fromNum(8.0)};  // walker, wave 8

        // Method-level: a fresh pellet's shove is far stronger than a plain round's.
        SceneTree t0;
        zomboid::buildScene(t0);
        auto& vm0 = t0.scripts().vm();
        Value b0 = t0.findNode("Bullet0")->script();
        vm0.callOn(b0, "fire", shot);
        const double plainKnock = vm0.callOn(b0, "knock_strength", none).number;   // falloff off
        sField(t0.findNode("Bullet0"), "falloff")->boolean = true;
        const double pelletKnock = vm0.callOn(b0, "knock_strength", none).number;  // falloff on, fresh
        CHECK(std::abs(plainKnock - 0.6) < 1e-9);
        CHECK(std::abs(pelletKnock - 3.5) < 1e-9);
        CHECK(pelletKnock > plainKnock * 4.0);

        // End-to-end: a shotgun pellet bodily knocks the zombie back farther than a pistol round does.
        SceneTree tp;
        zomboid::buildScene(tp);
        auto& vmp = tp.scripts().vm();
        Value zp = tp.findNode("Zombie0")->script();
        vmp.callOn(zp, "spawn", zsp);
        Value bp = tp.findNode("Bullet0")->script();
        vmp.callOn(bp, "fire", shot);                       // pistol round (no falloff)
        vmp.callOn(bp, "_process", dt);
        const double pistolPush = tp.findNode("Zombie0")->x() - 10.0;

        SceneTree ts;
        zomboid::buildScene(ts);
        auto& vms = ts.scripts().vm();
        Value zs = ts.findNode("Zombie0")->script();
        vms.callOn(zs, "spawn", zsp);
        Value bs = ts.findNode("Bullet0")->script();
        vms.callOn(bs, "fire", shot);
        sField(ts.findNode("Bullet0"), "falloff")->boolean = true;  // shotgun pellet
        vms.callOn(bs, "_process", dt);
        const double shotgunPush = ts.findNode("Zombie0")->x() - 10.0;

        CHECK(pistolPush > 0.0);                 // both shove the zombie in the fire direction
        CHECK(shotgunPush > pistolPush * 3.0);   // but the point-blank pellet shoves far harder
    }

    // Combo charges the ultimate faster: a kill landed on a hot combo streak banks more Overcharge
    // meter than a kill at base multiplier (1x at ×1–2, 2x at ×3–4, 3x at ×5) — so chain-killing earns
    // the ultimate more often.
    {
        std::vector<Value> none;
        auto ultGainAtMult = [&](double mult) {
            SceneTree tree;
            SceneNode* survivor = zomboid::buildScene(tree);
            auto& vm = tree.scripts().vm();
            const_cast<Value*>(vm.getGlobal("g_mult"))->number = mult;
            sField(survivor, "ult")->number = 0.0;
            sField(survivor, "ult_ready")->boolean = false;
            Value sv = survivor->script();
            vm.callOn(sv, "on_kill", none);
            return sField(survivor, "ult")->number;
        };
        CHECK(ultGainAtMult(1.0) == 1.0);   // base streak: one charge
        CHECK(ultGainAtMult(3.0) == 2.0);   // mid streak: double
        CHECK(ultGainAtMult(5.0) == 3.0);   // max streak: triple
        CHECK(ultGainAtMult(5.0) > ultGainAtMult(1.0));
    }

    // Brute knockback-on-hit: a Brute (kind 2) that lands its blow doesn't just deal damage — it hurls
    // the survivor back, a real spacing threat. A walker's bite doesn't, and a dodging survivor
    // (i-frames up) rides out the blow without being moved.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        // Player displacement after one attack tick from a zombie of `kind`, with `iframes` set.
        auto pushFrom = [&](double kind, double iframes) {
            SceneTree tree;
            SceneNode* p = zomboid::buildScene(tree);
            p->setPosition(0.0, 0.0);
            auto& vm = tree.scripts().vm();
            sField(p, "iframes")->number = iframes;
            std::vector<Value> sp = {Value::fromNum(-1.0), Value::fromNum(0.0),
                                     Value::fromNum(kind), Value::fromNum(5.0)};  // at (-1,0), wave 5
            Value z = tree.findNode("Zombie0")->script();
            vm.callOn(z, "spawn", sp);
            sField(tree.findNode("Zombie0"), "cooldown")->number = 0.0;   // ready to strike
            const double x0 = p->x();
            vm.callOn(z, "_process", dt);
            return p->x() - x0;
        };
        CHECK(pushFrom(2.0, 0.0) > 3.0);      // brute hurls the survivor back (~4 units, +x)
        CHECK(pushFrom(0.0, 0.0) == 0.0);     // a walker's bite doesn't shove
        CHECK(pushFrom(2.0, 1.0) == 0.0);     // dodging (i-frames) rides the brute's blow out
    }

    // Exploder chain reaction: killing one exploder detonates it, and its blast chain-detonates other
    // exploders in range — a daisy-chain of blasts. Reaches even a third exploder out of the first's
    // radius but within the second's. An exploder well clear of the chain survives. A plain zombie in
    // the blast is caught (damaged) but doesn't chain.
    {
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        // Exploders at 0, 3, 6 on the x-axis (blast radius 5): 0 reaches 3 directly; 3 then reaches 6.
        auto spawnExploder = [&](SceneTree& t, const char* name, double x) {
            Value z = t.findNode(name)->script();
            std::vector<Value> sp = {Value::fromNum(x), Value::fromNum(0.0),
                                     Value::fromNum(4.0), Value::fromNum(5.0)};
            t.scripts().vm().callOn(z, "spawn", sp);
        };
        SceneTree tree;
        SceneNode* player = zomboid::buildScene(tree);
        player->setPosition(100.0, 100.0);   // well out of every blast
        auto& vm = tree.scripts().vm();
        spawnExploder(tree, "Zombie0", 0.0);
        spawnExploder(tree, "Zombie1", 3.0);
        spawnExploder(tree, "Zombie2", 6.0);
        spawnExploder(tree, "Zombie3", 40.0);   // far clear of the chain
        // A plain walker sitting next to the first exploder: caught by the blast, but not a chain link.
        Value w = tree.findNode("Zombie4")->script();
        std::vector<Value> wsp = {Value::fromNum(2.0), Value::fromNum(0.0),
                                  Value::fromNum(0.0), Value::fromNum(1.0)};  // low-hp walker
        vm.callOn(w, "spawn", wsp);

        Value first = tree.findNode("Zombie0")->script();
        vm.callOn(first, "take_damage", lethal);   // pop the first exploder

        CHECK(!sField(tree.findNode("Zombie0"), "alive")->boolean);  // detonated
        CHECK(!sField(tree.findNode("Zombie1"), "alive")->boolean);  // chained (in radius of #0)
        CHECK(!sField(tree.findNode("Zombie2"), "alive")->boolean);  // chained via #1 (out of #0's radius)
        CHECK(sField(tree.findNode("Zombie3"), "alive")->boolean);   // far exploder untouched
        CHECK(!sField(tree.findNode("Zombie4"), "alive")->boolean);  // walker caught in the blast

        // The distant exploder took no damage at all (full health).
        CHECK(sField(tree.findNode("Zombie3"), "health")->number ==
              sField(tree.findNode("Zombie3"), "max_health")->number);
    }

    // Mine cooks off barrels: a proximity mine's blast now detonates an explosive barrel in range, so
    // rigging a mine beside a barrel sets up a huge combined blast. A barrel well outside the mine's
    // radius is left standing.
    {
        std::vector<Value> at = {Value::fromNum(50.0), Value::fromNum(0.0)};

        SceneTree tree;
        SceneNode* player = zomboid::buildScene(tree);
        player->setPosition(100.0, 100.0);   // clear of the blast
        auto& vm = tree.scripts().vm();
        Value barrelNear = tree.findNode("Barrel0")->script();
        std::vector<Value> nearPos = {Value::fromNum(52.0), Value::fromNum(0.0)};   // within 6-unit radius
        vm.callOn(barrelNear, "place", nearPos);
        Value barrelFar = tree.findNode("Barrel1")->script();
        std::vector<Value> farPos = {Value::fromNum(90.0), Value::fromNum(0.0)};     // well clear
        vm.callOn(barrelFar, "place", farPos);

        Value mine = tree.findNode("Mine0")->script();
        vm.callOn(mine, "arm", at);
        sField(tree.findNode("Mine0"), "arm_delay")->number = 0.0;   // skip the safety fuse
        vm.callOn(mine, "detonate", std::vector<Value>{});

        CHECK(!sField(tree.findNode("Barrel0"), "active")->boolean);  // cooked off by the mine
        CHECK(sField(tree.findNode("Barrel1"), "active")->boolean);   // far barrel survives
    }

    // Sentry self-destruct: when a sentry powers down (lifetime expired), it goes out with a blast that
    // damages the zombies around it — rewarding aggressive placement. A zombie well clear is untouched.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();

        Value znear = tree.findNode("Zombie0")->script();
        std::vector<Value> nsp = {Value::fromNum(62.0), Value::fromNum(0.0),
                                  Value::fromNum(2.0), Value::fromNum(5.0)};  // brute at (62,0)
        vm.callOn(znear, "spawn", nsp);
        Value zfar = tree.findNode("Zombie1")->script();
        std::vector<Value> fsp = {Value::fromNum(80.0), Value::fromNum(0.0),
                                  Value::fromNum(2.0), Value::fromNum(5.0)};  // brute at (80,0), clear
        vm.callOn(zfar, "spawn", fsp);
        const double nearMax = sField(tree.findNode("Zombie0"), "max_health")->number;
        const double farHp0 = sField(tree.findNode("Zombie1"), "health")->number;

        Value sentry = tree.findNode("Sentry0")->script();
        std::vector<Value> at = {Value::fromNum(60.0), Value::fromNum(0.0)};
        vm.callOn(sentry, "deploy", at);
        sField(tree.findNode("Sentry0"), "life")->number = 0.0001;   // about to expire
        vm.callOn(sentry, "_process", dt);                           // ...triggers the self-destruct

        CHECK(!sField(tree.findNode("Sentry0"), "active")->boolean);            // powered down
        CHECK(sField(tree.findNode("Zombie0"), "health")->number < nearMax);    // caught in the blast
        CHECK(sField(tree.findNode("Zombie1"), "health")->number == farHp0);    // far zombie untouched
    }
    // Sentry blast sets off the environment: a sentry powering down cooks off an explosive barrel in its
    // self-destruct radius (and flashes over a caustic puddle), while a barrel well clear is untouched —
    // the same hard-blast chain barrels/mines/grenades/exploders trigger.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();

        Value nearBarrel = tree.findNode("Barrel0")->script();
        std::vector<Value> nb = {Value::fromNum(63.0), Value::fromNum(0.0)};  // 3 units from the sentry
        vm.callOn(nearBarrel, "place", nb);
        Value farBarrel = tree.findNode("Barrel1")->script();
        std::vector<Value> fb = {Value::fromNum(80.0), Value::fromNum(0.0)};  // 20 units — well clear
        vm.callOn(farBarrel, "place", fb);
        Value acid = tree.findNode("Acid0")->script();
        std::vector<Value> ac = {Value::fromNum(62.0), Value::fromNum(0.0)};  // puddle in blast range
        vm.callOn(acid, "splat_at", ac);
        CHECK(sField(tree.findNode("Barrel0"), "active")->boolean);
        CHECK(sField(tree.findNode("Barrel1"), "active")->boolean);
        CHECK(sField(tree.findNode("Acid0"), "active")->boolean);

        Value sentry = tree.findNode("Sentry0")->script();
        std::vector<Value> at = {Value::fromNum(60.0), Value::fromNum(0.0)};
        vm.callOn(sentry, "deploy", at);
        sField(tree.findNode("Sentry0"), "life")->number = 0.0001;   // about to expire
        vm.callOn(sentry, "_process", dt);                           // ...triggers the self-destruct

        CHECK(!sField(tree.findNode("Barrel0"), "active")->boolean);   // cooked off by the sentry blast
        CHECK(sField(tree.findNode("Barrel1"), "active")->boolean);    // out of range — intact
        CHECK(!sField(tree.findNode("Acid0"), "active")->boolean);     // puddle flashed over
    }

    // Sentry threat targeting: with a scarce magazine the turret focus-fires the biggest threat in range
    // (a boss) even when a lesser one (a walker) is closer, rather than plinking the nearest body.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();

        Value zwalk = tree.findNode("Zombie0")->script();
        std::vector<Value> wsp = {Value::fromNum(5.0), Value::fromNum(0.0),
                                  Value::fromNum(0.0), Value::fromNum(1.0)};   // walker at (5,0) — nearer
        vm.callOn(zwalk, "spawn", wsp);
        Value zboss = tree.findNode("Zombie1")->script();
        std::vector<Value> bsp = {Value::fromNum(10.0), Value::fromNum(0.0),
                                  Value::fromNum(3.0), Value::fromNum(1.0)};   // boss at (10,0) — farther
        vm.callOn(zboss, "spawn", bsp);
        const double walkMax = sField(tree.findNode("Zombie0"), "max_health")->number;
        const double bossHp0 = sField(tree.findNode("Zombie1"), "health")->number;

        Value sentry = tree.findNode("Sentry0")->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};    // both in range 16
        vm.callOn(sentry, "deploy", at);
        vm.callOn(sentry, "_process", dt);                                     // fires one bolt

        CHECK(sField(tree.findNode("Zombie1"), "health")->number < bossHp0);   // boss got the bolt
        CHECK(sField(tree.findNode("Zombie0"), "health")->number == walkMax);  // nearer walker ignored
    }

    // Active reload: tapping reload again during the tail window snaps the reload shut instantly and
    // grants a brief +30% damage surge. Tapping too early does nothing (no penalty). The surge lifts
    // shot damage while it lasts.
    {
        std::vector<Value> none;

        // Perfect timing: a second reload in the window finishes instantly + arms the damage surge.
        SceneTree t1;
        SceneNode* s1 = zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        Value sv1 = s1->script();
        sField(s1, "weapon")->number = 0.0;                       // pistol
        (*sField(s1, "mags")->array)[0].number = 0.0;             // empty magazine → reload is allowed
        vm1.callOn(sv1, "reload", none);                          // begin the reload
        CHECK(sField(s1, "reloading")->boolean);
        sField(s1, "reload_t")->number = 0.3;                     // 1.2s reload → 0.3 is in the window
        vm1.callOn(sv1, "reload", none);                          // active-reload tap
        CHECK(!sField(s1, "reloading")->boolean);                 // snapped shut instantly
        CHECK(sField(s1, "perfect_timer")->number > 0.0);         // surge armed
        CHECK((*sField(s1, "mags")->array)[0].number > 0.0);      // magazine refilled

        // Too early: a tap outside the window leaves the reload running and grants no surge.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        Value sv2 = s2->script();
        sField(s2, "weapon")->number = 0.0;
        (*sField(s2, "mags")->array)[0].number = 0.0;
        vm2.callOn(sv2, "reload", none);
        sField(s2, "reload_t")->number = 1.0;                     // well before the window
        vm2.callOn(sv2, "reload", none);
        CHECK(sField(s2, "reloading")->boolean);                  // still reloading
        CHECK(sField(s2, "perfect_timer")->number == 0.0);        // no surge

        // Too late: a tap in the final sliver (below the window's 12% lower bound) also misses — no
        // instant finish, no surge. This pins the window's LATE edge, the counterpart to the early one:
        // a mistap right before the reload completes doesn't sneak a free surge.
        SceneTree t2b;
        SceneNode* s2b = zomboid::buildScene(t2b);
        auto& vm2b = t2b.scripts().vm();
        Value sv2b = s2b->script();
        sField(s2b, "weapon")->number = 0.0;                      // pistol → 1.2s reload
        (*sField(s2b, "mags")->array)[0].number = 0.0;
        vm2b.callOn(sv2b, "reload", none);
        sField(s2b, "reload_t")->number = 0.05;                   // ~4% remaining — past the window's low end
        vm2b.callOn(sv2b, "reload", none);
        CHECK(sField(s2b, "reloading")->boolean);                 // still reloading (no instant snap)
        CHECK(sField(s2b, "perfect_timer")->number == 0.0);       // no surge for a too-late tap

        // The surge lifts shot damage ~30% (crit forced off for a deterministic read).
        SceneTree t3;
        SceneNode* s3 = zomboid::buildScene(t3);
        auto& vm3 = t3.scripts().vm();
        Value sv3 = s3->script();
        sField(s3, "crit_chance")->number = 0.0;
        sField(s3, "perfect_timer")->number = 0.0;
        const double baseDmg = vm3.callOn(sv3, "shot_damage", none).number;
        sField(s3, "perfect_timer")->number = 4.0;
        const double surgeDmg = vm3.callOn(sv3, "shot_damage", none).number;
        CHECK(surgeDmg > baseDmg);
        CHECK(std::abs(surgeDmg - baseDmg * 1.3) < 1e-6);
    }

    // Screamer telegraph: the screamer no longer shrieks instantly — it winds up with a tell first, so
    // the survivor gets a window to burst it or chill it. Verify the wind-up delays the frenzy, that the
    // shriek does land after it, and that a chill during the wind-up fizzles it.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> screamSp = {Value::fromNum(0.0), Value::fromNum(0.0),
                                       Value::fromNum(11.0), Value::fromNum(5.0)};  // screamer at origin
        std::vector<Value> walkSp = {Value::fromNum(5.0), Value::fromNum(0.0),
                                     Value::fromNum(0.0), Value::fromNum(5.0)};     // walker in radius 15
        std::vector<Value> chill = {Value::fromNum(4.0)};

        // Wind-up then shriek: the neighbour isn't frenzied on the tell tick, but is once it lands.
        SceneTree t1;
        SceneNode* p1 = zomboid::buildScene(t1);
        p1->setPosition(200.0, 200.0);
        auto& vm1 = t1.scripts().vm();
        Value scr1 = t1.findNode("Zombie0")->script();
        vm1.callOn(scr1, "spawn", screamSp);
        sField(t1.findNode("Zombie0"), "speed")->number = 0.0;     // pin it so distances stay fixed
        sField(t1.findNode("Zombie0"), "cooldown")->number = 0.0;  // ready to shriek
        Value wk1 = t1.findNode("Zombie1")->script();
        vm1.callOn(wk1, "spawn", walkSp);
        vm1.callOn(scr1, "_process", dt);                          // enters the wind-up
        CHECK(sField(t1.findNode("Zombie0"), "scream_warn")->number > 0.0);   // telegraphing
        CHECK(sField(t1.findNode("Zombie1"), "frenzy_timer")->number == 0.0); // not yet frenzied
        for (int i = 0; i < 50; ++i) { vm1.callOn(scr1, "_process", dt); }    // let the tell resolve
        CHECK(sField(t1.findNode("Zombie1"), "frenzy_timer")->number > 0.0);  // shriek landed

        // Chill during the wind-up fizzles the shriek: the neighbour is never frenzied.
        SceneTree t2;
        SceneNode* p2 = zomboid::buildScene(t2);
        p2->setPosition(200.0, 200.0);
        auto& vm2 = t2.scripts().vm();
        Value scr2 = t2.findNode("Zombie0")->script();
        vm2.callOn(scr2, "spawn", screamSp);
        sField(t2.findNode("Zombie0"), "speed")->number = 0.0;
        sField(t2.findNode("Zombie0"), "cooldown")->number = 0.0;
        Value wk2 = t2.findNode("Zombie1")->script();
        vm2.callOn(wk2, "spawn", walkSp);
        vm2.callOn(scr2, "_process", dt);                          // enters the wind-up
        CHECK(sField(t2.findNode("Zombie0"), "scream_warn")->number > 0.0);
        vm2.callOn(scr2, "apply_slow", chill);                     // chill it mid-tell
        for (int i = 0; i < 50; ++i) { vm2.callOn(scr2, "_process", dt); }
        CHECK(sField(t2.findNode("Zombie1"), "frenzy_timer")->number == 0.0); // shriek fizzled

    }

    // Melee swats acid globs: a well-timed shove bats an incoming spitter glob out of the air (within
    // melee reach), destroying it clean with no acid puddle. A glob out of reach sails on untouched.
    {
        std::vector<Value> none;
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        sField(survivor, "melee_cd")->number = 0.0;              // melee ready

        // A glob in reach (melee_range 4.5) and one well out of reach.
        Value near = tree.findNode("Spit0")->script();
        std::vector<Value> nearLaunch = {Value::fromNum(2.0), Value::fromNum(0.0),
                                         Value::fromNum(0.0), Value::fromNum(0.0)};
        vm.callOn(near, "launch", nearLaunch);
        Value far = tree.findNode("Spit1")->script();
        std::vector<Value> farLaunch = {Value::fromNum(30.0), Value::fromNum(0.0),
                                        Value::fromNum(0.0), Value::fromNum(0.0)};
        vm.callOn(far, "launch", farLaunch);
        CHECK(sField(tree.findNode("Spit0"), "active")->boolean);
        CHECK(sField(tree.findNode("Spit1"), "active")->boolean);

        const int acidBefore = activeAcid(tree);
        Value sv = survivor->script();
        vm.callOn(sv, "melee", none);

        CHECK(!sField(tree.findNode("Spit0"), "active")->boolean);   // batted out of the air
        CHECK(sField(tree.findNode("Spit1"), "active")->boolean);    // too far — sails on
        CHECK(activeAcid(tree) == acidBefore);                       // swatted glob leaves NO puddle
    }

    // Kill momentum feeds mobility: each kill shaves 0.3s off the dodge-roll cooldown, so chaining
    // kills keeps your escape ready. The refund clamps at 0 and never goes negative.
    {
        std::vector<Value> none;
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();

        sField(survivor, "dash_cd")->number = 2.0;              // mid-cooldown
        vm.callOn(sv, "on_kill", none);
        CHECK(std::abs(sField(survivor, "dash_cd")->number - 1.7) < 1e-9);   // shaved 0.3s

        // A kill with the dodge already ready doesn't push the cooldown negative.
        sField(survivor, "dash_cd")->number = 0.1;
        vm.callOn(sv, "on_kill", none);
        CHECK(sField(survivor, "dash_cd")->number == 0.0);      // clamped, not negative
    }

    // Healer telegraph: the healer now winds up with a tell before its mend pulse, so you get a window
    // to kill or chill it before it undoes your chip damage. Verify the wind-up delays the mend, that
    // the mend lands after it, and that a chill during the wind-up fizzles it.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> healSp = {Value::fromNum(0.0), Value::fromNum(0.0),
                                     Value::fromNum(12.0), Value::fromNum(5.0)};  // healer at origin
        std::vector<Value> woundSp = {Value::fromNum(5.0), Value::fromNum(0.0),
                                      Value::fromNum(2.0), Value::fromNum(5.0)};  // brute in radius 14
        std::vector<Value> wound = {Value::fromNum(50.0)};
        std::vector<Value> chill = {Value::fromNum(4.0)};

        // Wind-up then mend: the wounded neighbour isn't healed on the tell tick, but is once it lands.
        SceneTree t1;
        SceneNode* p1 = zomboid::buildScene(t1);
        p1->setPosition(200.0, 200.0);
        auto& vm1 = t1.scripts().vm();
        Value h1 = t1.findNode("Zombie0")->script();
        vm1.callOn(h1, "spawn", healSp);
        sField(t1.findNode("Zombie0"), "speed")->number = 0.0;
        sField(t1.findNode("Zombie0"), "cooldown")->number = 0.0;
        Value wnd1 = t1.findNode("Zombie1")->script();
        vm1.callOn(wnd1, "spawn", woundSp);
        vm1.callOn(wnd1, "take_damage", wound);
        const double hurt1 = sField(t1.findNode("Zombie1"), "health")->number;
        vm1.callOn(h1, "_process", dt);                          // enters the wind-up
        CHECK(sField(t1.findNode("Zombie0"), "mend_warn")->number > 0.0);
        CHECK(sField(t1.findNode("Zombie1"), "health")->number == hurt1);   // not healed yet
        for (int i = 0; i < 50; ++i) { vm1.callOn(h1, "_process", dt); }
        CHECK(sField(t1.findNode("Zombie1"), "health")->number > hurt1);    // mend landed

        // Chill during the wind-up fizzles the mend: the neighbour stays wounded.
        SceneTree t2;
        SceneNode* p2 = zomboid::buildScene(t2);
        p2->setPosition(200.0, 200.0);
        auto& vm2 = t2.scripts().vm();
        Value h2 = t2.findNode("Zombie0")->script();
        vm2.callOn(h2, "spawn", healSp);
        sField(t2.findNode("Zombie0"), "speed")->number = 0.0;
        sField(t2.findNode("Zombie0"), "cooldown")->number = 0.0;
        Value wnd2 = t2.findNode("Zombie1")->script();
        vm2.callOn(wnd2, "spawn", woundSp);
        vm2.callOn(wnd2, "take_damage", wound);
        const double hurt2 = sField(t2.findNode("Zombie1"), "health")->number;
        vm2.callOn(h2, "_process", dt);                          // enters the wind-up
        CHECK(sField(t2.findNode("Zombie0"), "mend_warn")->number > 0.0);
        vm2.callOn(h2, "apply_slow", chill);                     // chill it mid-tell
        for (int i = 0; i < 50; ++i) { vm2.callOn(h2, "_process", dt); }
        CHECK(sField(t2.findNode("Zombie1"), "health")->number == hurt2);   // mend fizzled

    }

    // Flamethrower ground-fire trail: firing the flamethrower lays a lingering fire patch mid-cone, on
    // a throttle so it doesn't spam. First shot lights a patch; an immediate second shot (throttle still
    // up) lays no new one.
    {
        std::vector<Value> fire = {Value::fromNum(1.0), Value::fromNum(0.0)};   // aim +x
        auto activeFires = [](SceneTree& t) {
            int c = 0;
            for (SceneNode* f : t.nodesInGroup("fires")) {
                if (f->script().instance->findField("active")->boolean) { c = c + 1; }
            }
            return c;
        };
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();
        sField(survivor, "flame_cd")->number = 0.0;             // trail ready

        CHECK(activeFires(tree) == 0);                          // no ground fire at rest
        vm.callOn(sv, "flamethrower_fire", fire);
        CHECK(activeFires(tree) == 1);                          // laid one patch mid-cone
        CHECK(sField(survivor, "flame_cd")->number > 0.0);      // throttle now armed
        vm.callOn(sv, "flamethrower_fire", fire);
        CHECK(activeFires(tree) == 1);                          // throttled — no second patch yet
    }

    // Summoner telegraph: the summoner now winds up with a tell before calling a reinforcement, so you
    // get a window to burst or chill it first. Verify the wind-up delays the summon, that the summon
    // lands after it, and that a chill during the wind-up fizzles it.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> sumSp = {Value::fromNum(0.0), Value::fromNum(0.0),
                                    Value::fromNum(7.0), Value::fromNum(5.0)};   // summoner at origin
        std::vector<Value> chill = {Value::fromNum(4.0)};
        auto aliveZombies = [](SceneTree& t) {
            int c = 0;
            for (SceneNode* z : t.nodesInGroup("zombies")) {
                if (z->script().instance->findField("alive")->boolean) { c = c + 1; }
            }
            return c;
        };

        // Wind-up then summon: no reinforcement on the tell tick, but one arrives once it lands.
        SceneTree t1;
        SceneNode* p1 = zomboid::buildScene(t1);
        p1->setPosition(200.0, 200.0);
        auto& vm1 = t1.scripts().vm();
        Value s1 = t1.findNode("Zombie0")->script();
        vm1.callOn(s1, "spawn", sumSp);
        sField(t1.findNode("Zombie0"), "speed")->number = 0.0;
        sField(t1.findNode("Zombie0"), "summon_cd")->number = 0.0;
        const int before1 = aliveZombies(t1);
        vm1.callOn(s1, "_process", dt);                          // enters the wind-up
        CHECK(sField(t1.findNode("Zombie0"), "summon_warn")->number > 0.0);
        CHECK(aliveZombies(t1) == before1);                      // no reinforcement yet
        for (int i = 0; i < 50; ++i) { vm1.callOn(s1, "_process", dt); }
        CHECK(aliveZombies(t1) > before1);                       // reinforcement arrived

        // Chill during the wind-up fizzles the call: no reinforcement.
        SceneTree t2;
        SceneNode* p2 = zomboid::buildScene(t2);
        p2->setPosition(200.0, 200.0);
        auto& vm2 = t2.scripts().vm();
        Value s2 = t2.findNode("Zombie0")->script();
        vm2.callOn(s2, "spawn", sumSp);
        sField(t2.findNode("Zombie0"), "speed")->number = 0.0;
        sField(t2.findNode("Zombie0"), "summon_cd")->number = 0.0;
        const int before2 = aliveZombies(t2);
        vm2.callOn(s2, "_process", dt);
        CHECK(sField(t2.findNode("Zombie0"), "summon_warn")->number > 0.0);
        vm2.callOn(s2, "apply_slow", chill);
        for (int i = 0; i < 50; ++i) { vm2.callOn(s2, "_process", dt); }
        CHECK(aliveZombies(t2) == before2);                      // fizzled — no reinforcement

    }

    // Railgun armor-piercing: the beam shears any shield clean off before biting into health, so it's
    // the counter to armored zombies and Bulwark waves. A zombie off the beam keeps its shield.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();

        // On-beam target: parked straight ahead (+x) with a shield in front of its health.
        SceneNode* onZ = tree.findNode("Zombie0");
        Value onv = onZ->script();
        std::vector<Value> sp = {Value::fromNum(10.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};  // walker at (10,0), 100 hp
        vm.callOn(onv, "spawn_at", sp);
        sField(onZ, "shield")->number = 40.0;

        // Off-beam control: parked to the side, well clear of the beam line.
        SceneNode* offZ = tree.findNode("Zombie1");
        Value offv = offZ->script();
        std::vector<Value> sp2 = {Value::fromNum(0.0), Value::fromNum(40.0),
                                  Value::fromNum(100.0), Value::fromNum(0.0)};
        vm.callOn(offv, "spawn_at", sp2);
        sField(offZ, "shield")->number = 40.0;

        Value sv = survivor->script();
        std::vector<Value> aim = {Value::fromNum(1.0), Value::fromNum(0.0)};  // fire straight down +x
        vm.callOn(sv, "railgun_fire", aim);

        CHECK(sField(onZ, "shield")->number == 0.0);        // slug sheared the plating off
        CHECK(sField(onZ, "health")->number < 100.0);       // and bit into health
        CHECK(sField(offZ, "shield")->number == 40.0);      // off the beam — plating intact
        CHECK(sField(offZ, "health")->number == 100.0);
    }

    // Last-stand adrenaline damage: when critically wounded the survivor's shots hit 30% harder, and
    // the flag flips on automatically once health drops to 25% of max.
    {
        std::vector<Value> none;
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();
        sField(survivor, "crit_chance")->number = 0.0;   // no crit rolls — isolate the adrenaline term
        const double base = sField(survivor, "damage")->number;

        // Healthy: no adrenaline, shot deals exactly base damage.
        CHECK(!sField(survivor, "adrenaline")->boolean);
        CHECK(vm.callOn(sv, "shot_damage", none).number == base);

        // Force the last-stand flag: shots now hit 30% harder.
        sField(survivor, "adrenaline")->boolean = true;
        CHECK(vm.callOn(sv, "shot_damage", none).number == base * 1.3);

        // Wiring: dropping to 25% health flips adrenaline on through the survivor's own update.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        Value s2v = s2->script();
        const double mh = sField(s2, "max_health")->number;
        sField(s2, "health")->number = mh * 0.25;
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        t2.scripts().vm().callOn(s2v, "_process", dt);
        CHECK(sField(s2, "adrenaline")->boolean);
    }

    // Last-stand grit: the desperation surge also cuts incoming damage by a quarter, so the low-health
    // comeback window is survivable, not a death spiral. Off the surge, the same hit lands in full.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();
        sField(survivor, "armor")->number = 0.0;      // no plate — measure health loss directly
        sField(survivor, "iframes")->number = 0.0;    // not mid-dodge
        std::vector<Value> hit = {Value::fromNum(20.0)};

        // Surge off: a 20-damage hit removes the full 20 health.
        sField(survivor, "adrenaline")->boolean = false;
        sField(survivor, "health")->number = 100.0;
        vm.callOn(sv, "take_damage", hit);
        CHECK(sField(survivor, "health")->number == 80.0);

        // Surge on: the same hit is cut to 15 (×0.75), so only 15 health is lost.
        sField(survivor, "adrenaline")->boolean = true;
        sField(survivor, "health")->number = 100.0;
        vm.callOn(sv, "take_damage", hit);
        CHECK(sField(survivor, "health")->number == 85.0);
    }

    // Wave-clear pickup vacuum: clearing a wave sweeps up any medkit or power-up still lying on the
    // field (out of walking range), so a cleared wave never strands a drop during the between-wave lull.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        tree.process(1.0 / 60.0);                        // wave 1 opens, zombies spawn
        CHECK((int)glob(tree, "g_wave") == 1);
        sField(survivor, "health")->number = 50.0;       // wounded, so a vacuumed medkit shows

        // Drop a medkit and a power-up far from the survivor — well out of pickup/magnet range.
        SceneNode* kit = tree.findNode("Medkit0");
        Value kv = kit->script();
        std::vector<Value> kp = {Value::fromNum(100.0), Value::fromNum(100.0)};
        vm.callOn(kv, "place", kp);
        SceneNode* pow = tree.findNode("Powerup0");
        Value pv = pow->script();
        std::vector<Value> pp = {Value::fromNum(100.0), Value::fromNum(100.0),
                                 Value::fromNum(0.0)};    // kind 0 = rapid fire
        vm.callOn(pv, "place", pp);
        // ...and an ammo box, likewise out of reach — it should be swept up too, not left to expire.
        setWeapon(tree, survivor, 1);   // shotgun equipped so an ammo refill isn't a pistol no-op
        SceneNode* box = tree.findNode("Ammo0");
        Value av = box->script();
        std::vector<Value> ap = {Value::fromNum(-100.0), Value::fromNum(100.0)};
        vm.callOn(av, "place", ap);
        const double res1before = (*sField(survivor, "reserves")->array)[1].number;
        CHECK(sField(kit, "active")->boolean);
        CHECK(sField(pow, "active")->boolean);
        CHECK(sField(box, "active")->boolean);

        // Kill the whole wave, then step once so the Director registers the clear and vacuums.
        for (SceneNode* z : tree.nodesInGroup("zombies")) {
            if (z->script().instance->findField("alive")->boolean) {
                Value zs = z->script();
                std::vector<Value> lethal = {Value::fromNum(9999.0)};
                vm.callOn(zs, "take_damage", lethal);
            }
        }
        tree.process(1.0 / 60.0);

        CHECK(!sField(kit, "active")->boolean);           // medkit swept up
        CHECK(!sField(pow, "active")->boolean);           // power-up swept up
        CHECK(!sField(box, "active")->boolean);           // ammo box swept up
        CHECK(sField(survivor, "health")->number > 50.0); // medkit healed on the way in
        CHECK(sField(survivor, "buff_timer")->number > 0.0); // power-up buff granted
        CHECK((*sField(survivor, "reserves")->array)[1].number > res1before); // ammo refilled on sweep
    }

    // Boss slam knockback: the boss's ground slam physically hurls a nearby survivor away from it, so
    // standing next to the boss is punished with a shove, not just chip damage.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(5.0, 0.0);                 // 5 units from the boss at the origin
        auto& vm = tree.scripts().vm();

        SceneNode* boss = tree.findNode("Zombie0");
        Value bv = boss->script();
        std::vector<Value> bs = {Value::fromNum(0.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(5.0)};  // spawn(x,y,boss,wave)
        vm.callOn(bv, "spawn", bs);
        CHECK((int)sField(boss, "kind")->number == 3);
        sField(boss, "slam_cd")->number = 0.0;           // slam becomes ready this step

        // First tick only starts the wind-up telegraph — the survivor is not hit yet.
        vm.callOn(bv, "_process", dt);
        CHECK(survivor->x() == 5.0);                      // not yet knocked back
        CHECK(sField(boss, "slam_warn")->number > 0.0);   // winding up

        // After the ~0.5s wind-up the slam lands and hurls the survivor clear (unit-vector *6 along +x).
        for (int i = 0; i < 40; ++i) { vm.callOn(bv, "_process", dt); }
        CHECK(survivor->x() > 10.0);
        CHECK(survivor->y() == 0.0);                      // pushed straight along the boss→survivor axis
    }

    // Explosive barrel is double-edged: a survivor caught in the blast takes half damage and is flung
    // clear, so hugging a barrel you shoot is punished; standing well clear is safe.
    {
        std::vector<Value> none;

        // In-blast case: survivor 2 units from a barrel at the origin — well inside blast radius 7.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(2.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* barrel = tree.findNode("Barrel0");
        Value bv = barrel->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        vm.callOn(bv, "place", at);                       // move it onto the origin, hp reset
        const double h0 = sField(survivor, "health")->number;
        vm.callOn(bv, "explode", none);
        CHECK(sField(survivor, "health")->number == h0 - 45.0);   // 90 blast * 0.5, no armor
        CHECK(survivor->x() > 2.0);                                // flung away from the barrel

        // Clear case: survivor far away takes nothing from the same blast.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(100.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* b2 = t2.findNode("Barrel0");
        Value b2v = b2->script();
        vm2.callOn(b2v, "place", at);
        const double h2 = sField(s2, "health")->number;
        vm2.callOn(b2v, "explode", none);
        CHECK(sField(s2, "health")->number == h2);        // out of range — untouched
        CHECK(s2->x() == 100.0);
    }

    // Combo grace scales with the streak: a hard-won high multiplier survives a longer idle gap than a
    // fresh streak (base 2.5s, +0.5s per multiplier step, up to 4.5s at x5).
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        auto setG = [](SceneTree& t, const char* n, double v) {
            const_cast<Value*>(t.scripts().vm().getGlobal(n))->number = v;
        };

        // High streak (x5) idle 3.0s: still inside its 4.5s window — combo survives.
        SceneTree t1;
        SceneNode* s1 = zomboid::buildScene(t1);
        setG(t1, "g_combo", 20.0); setG(t1, "g_mult", 5.0); setG(t1, "g_combo_timer", 3.0);
        Value s1v = s1->script();
        t1.scripts().vm().callOn(s1v, "_process", dt);
        CHECK((int)glob(t1, "g_combo") == 20);

        // Fresh streak (x1) idle the same 3.0s: past its 2.5s window — combo resets.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        setG(t2, "g_combo", 3.0); setG(t2, "g_mult", 1.0); setG(t2, "g_combo_timer", 3.0);
        Value s2v = s2->script();
        t2.scripts().vm().callOn(s2v, "_process", dt);
        CHECK((int)glob(t2, "g_combo") == 0);

        // High streak past even its extended 4.5s window: it finally resets too.
        SceneTree t3;
        SceneNode* s3 = zomboid::buildScene(t3);
        setG(t3, "g_combo", 20.0); setG(t3, "g_mult", 5.0); setG(t3, "g_combo_timer", 4.6);
        Value s3v = s3->script();
        t3.scripts().vm().callOn(s3v, "_process", dt);
        CHECK((int)glob(t3, "g_combo") == 0);
    }

    // Spitter acid puddle bogs the survivor down: standing in it sets the acid-slow timer (main.cpp
    // halves movement while it's up); standing clear leaves it at zero.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // In-puddle case: survivor at the puddle centre gets slowed.
        SceneTree t1;
        SceneNode* s1 = zomboid::buildScene(t1);
        s1->setPosition(0.0, 0.0);
        auto& vm1 = t1.scripts().vm();
        SceneNode* acid1 = t1.findNode("Acid0");
        Value a1 = acid1->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        vm1.callOn(a1, "splat_at", at);
        CHECK(sField(s1, "acid_slow")->number == 0.0);   // not yet
        vm1.callOn(a1, "_process", dt);                  // first tick lands the caustic burn
        CHECK(sField(s1, "acid_slow")->number > 0.0);    // now bogged down

        // Dodge-roll cleanse: a dash bursts the survivor free of the caustic bog, clearing the slow.
        Value s1v = s1->script();
        std::vector<Value> dir = {Value::fromNum(1.0), Value::fromNum(0.0)};
        CHECK(vm1.callOn(s1v, "dash", dir).boolean);     // the dash fires
        CHECK(sField(s1, "acid_slow")->number == 0.0);   // ...and shakes off the acid slow

        // Clear case: survivor well outside the puddle is untouched.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(100.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* acid2 = t2.findNode("Acid0");
        Value a2 = acid2->script();
        vm2.callOn(a2, "splat_at", at);
        vm2.callOn(a2, "_process", dt);
        CHECK(sField(s2, "acid_slow")->number == 0.0);   // out of the puddle — no slow
    }

    // Leaper pounce telegraph: a ready leaper at mid-range crouches (leap_wind) for a beat — rooted,
    // not yet flying — before it springs, giving the survivor a window to juke aside.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* leaper = tree.findNode("Zombie0");
        Value lz = leaper->script();
        std::vector<Value> sp = {Value::fromNum(10.0), Value::fromNum(0.0),
                                 Value::fromNum(9.0), Value::fromNum(5.0)};   // leaper at mid-range
        vm.callOn(lz, "spawn", sp);
        sField(leaper, "leap_cd")->number = 0.0;    // ready to pounce
        sField(leaper, "slow_timer")->number = 0.0;
        const double x0 = leaper->x();

        // One tick starts the coil: it winds up but hasn't sprung and hasn't moved.
        vm.callOn(lz, "_process", dt);
        CHECK(sField(leaper, "leap_wind")->number > 0.0);   // coiling
        CHECK(sField(leaper, "leaping")->number == 0.0);    // not airborne yet
        CHECK(leaper->x() == x0);                            // rooted during the tell

        // After the wind-up it commits the pounce (locks in a leap velocity and goes airborne).
        for (int i = 0; i < 25; ++i) { vm.callOn(lz, "_process", dt); }
        CHECK(sField(leaper, "leap_vx")->number != 0.0);    // sprang toward the survivor
        CHECK(leaper->x() < x0);                             // closed the gap (moved toward the origin)
    }

    // Warper (kind 13): a teleporter that telegraphs a blink (a brief rooted wind-up shimmer) then
    // phases a big chunk of the way to the survivor in a single instant — closing a gap no walker
    // could. With the cooldown still running it only shambles, barely moving.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Blink case: cooldown ready, 40 units out. First it winds up (rooted), then it phases in.
        SceneTree t1;
        SceneNode* surv1 = zomboid::buildScene(t1);
        surv1->setPosition(0.0, 0.0);
        auto& vm1 = t1.scripts().vm();
        SceneNode* w1 = t1.findNode("Zombie0");
        Value wz1 = w1->script();
        std::vector<Value> sp = {Value::fromNum(40.0), Value::fromNum(0.0),
                                 Value::fromNum(13.0), Value::fromNum(8.0)};   // warper (kind 13), wave 8
        vm1.callOn(wz1, "spawn", sp);
        CHECK((int)sField(w1, "kind")->number == 13);
        sField(w1, "warp_cd")->number = 0.0;              // ready to blink
        vm1.callOn(wz1, "_process", dt);
        CHECK(sField(w1, "warp_warn")->number > 0.0);      // telegraphing — charging the blink...
        CHECK(w1->x() > 39.0);                             // ...and rooted during the tell, no teleport
        for (int i = 0; i < 25; ++i) { vm1.callOn(wz1, "_process", dt); }   // wind-up elapses, it phases
        CHECK(w1->x() < 25.0);                             // teleported way in from 40...
        CHECK(w1->x() > 12.0);                             // ...to about the halfway point (then walks a bit)
        CHECK(sField(w1, "warp_cd")->number > 2.0);        // blink put the cooldown back on

        // No-blink case: cooldown not ready — the same warper only shambles a hair this tick.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* w2 = t2.findNode("Zombie0");
        Value wz2 = w2->script();
        vm2.callOn(wz2, "spawn", sp);
        sField(w2, "warp_cd")->number = 5.0;              // still on cooldown, no blink
        vm2.callOn(wz2, "_process", dt);
        CHECK(w2->x() > 39.0);                             // barely moved — a slow walk, no teleport

        // Chilled case: even with the cooldown ready, a frozen warper can't phase — cryo pins it.
        SceneTree t3;
        SceneNode* surv3 = zomboid::buildScene(t3);
        surv3->setPosition(0.0, 0.0);
        auto& vm3 = t3.scripts().vm();
        SceneNode* w3 = t3.findNode("Zombie0");
        Value wz3 = w3->script();
        vm3.callOn(wz3, "spawn", sp);
        sField(w3, "warp_cd")->number = 0.0;              // blink is off cooldown...
        sField(w3, "slow_timer")->number = 2.0;          // ...but it's chilled, so it can't phase
        vm3.callOn(wz3, "_process", dt);
        CHECK(w3->x() > 39.0);                             // stayed put — cryo hard-counters the blink
    }

    // Molotov fire cooks off barrels: an explosive barrel sitting in a burning patch is chipped by the
    // flames until it detonates, while a barrel well clear of the fire is left intact.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // In-fire case: barrel at the patch centre cooks off within a couple of seconds.
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        SceneNode* fire1 = t1.findNode("Fire0");
        Value f1 = fire1->script();
        std::vector<Value> at = {Value::fromNum(0.0), Value::fromNum(0.0)};
        vm1.callOn(f1, "ignite_ground", at);
        SceneNode* barrel1 = t1.findNode("Barrel0");
        Value b1 = barrel1->script();
        vm1.callOn(b1, "place", at);
        CHECK(sField(barrel1, "active")->boolean);
        for (int i = 0; i < 180; ++i) { vm1.callOn(f1, "_process", dt); }  // ~3s of burning
        CHECK(!sField(barrel1, "active")->boolean);       // cooked off and detonated

        // Clear case: a barrel outside the fire radius is untouched.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        SceneNode* fire2 = t2.findNode("Fire0");
        Value f2 = fire2->script();
        vm2.callOn(f2, "ignite_ground", at);
        SceneNode* barrel2 = t2.findNode("Barrel0");
        Value b2 = barrel2->script();
        std::vector<Value> far = {Value::fromNum(50.0), Value::fromNum(0.0)};
        vm2.callOn(b2, "place", far);
        for (int i = 0; i < 180; ++i) { vm2.callOn(f2, "_process", dt); }
        CHECK(sField(barrel2, "active")->boolean);        // well clear of the flames — intact
    }

    // Elite death shockwave: killing an elite champion releases a nova that knocks back and wounds
    // the surrounding crowd, clearing space; a zombie well clear of it is untouched.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();

        // Elite at the origin (make_elite triples score and multiplies health to 250).
        SceneNode* elite = tree.findNode("Zombie0");
        Value ev = elite->script();
        std::vector<Value> es = {Value::fromNum(0.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};   // walker, 100 hp
        vm.callOn(ev, "spawn_at", es);
        vm.callOn(ev, "make_elite", {});
        CHECK(sField(elite, "elite")->boolean);

        // A zombie 4 units away (inside the nova) and one 20 units away (clear of it).
        SceneNode* near_ = tree.findNode("Zombie1");
        Value nv = near_->script();
        std::vector<Value> np = {Value::fromNum(4.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};
        vm.callOn(nv, "spawn_at", np);
        SceneNode* far_ = tree.findNode("Zombie2");
        Value fv = far_->script();
        std::vector<Value> fp = {Value::fromNum(20.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};
        vm.callOn(fv, "spawn_at", fp);

        // Kill the elite with a hit big enough to drop it but under the overkill-gib threshold (1.5x
        // its 250 max), so only the elite death nova touches the neighbours.
        std::vector<Value> kill = {Value::fromNum(300.0)};
        vm.callOn(ev, "take_damage", kill);
        CHECK(!sField(elite, "alive")->boolean);

        CHECK(sField(near_, "health")->number == 70.0);   // nova dealt 30
        CHECK(near_->x() > 4.0);                           // and knocked it outward
        CHECK(sField(far_, "health")->number == 100.0);   // out of range — unscathed
        CHECK(far_->x() == 20.0);
    }

    // Flamethrower cooks barrels: a barrel in the flame cone detonates; one behind the survivor
    // (outside the cone) is untouched.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        sField(survivor, "crit_chance")->number = 0.0;   // steady base damage per lick

        SceneNode* front = tree.findNode("Barrel0");
        Value fbv = front->script();
        std::vector<Value> inCone = {Value::fromNum(5.0), Value::fromNum(0.0)};   // ahead, in the +x cone
        vm.callOn(fbv, "place", inCone);
        SceneNode* back = tree.findNode("Barrel1");
        Value bbv = back->script();
        std::vector<Value> away = {Value::fromNum(0.0), Value::fromNum(40.0)};    // off to the side, clear
        vm.callOn(bbv, "place", away);

        Value sv = survivor->script();
        std::vector<Value> aim = {Value::fromNum(1.0), Value::fromNum(0.0)};      // torch straight ahead
        for (int i = 0; i < 30; ++i) { vm.callOn(sv, "flamethrower_fire", aim); }

        CHECK(!sField(front, "active")->boolean);   // the flames cooked it off
        CHECK(sField(back, "active")->boolean);     // out of the cone — intact
    }

    // Grenade sets off barrels: a barrel inside the frag's blast detonates; one outside is spared.
    {
        std::vector<Value> none;
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* nade = tree.findNode("Grenade0");
        nade->setPosition(0.0, 0.0);   // detonate at the origin

        SceneNode* inBlast = tree.findNode("Barrel0");
        Value ib = inBlast->script();
        std::vector<Value> near_ = {Value::fromNum(3.0), Value::fromNum(0.0)};   // within blast radius 5
        vm.callOn(ib, "place", near_);
        SceneNode* outBlast = tree.findNode("Barrel1");
        Value ob = outBlast->script();
        std::vector<Value> far_ = {Value::fromNum(40.0), Value::fromNum(0.0)};
        vm.callOn(ob, "place", far_);

        Value gv = nade->script();
        vm.callOn(gv, "explode", none);

        CHECK(!sField(inBlast, "active")->boolean);   // frag set it off
        CHECK(sField(outBlast, "active")->boolean);   // out of the blast — spared
    }

    // Night salvage bonus: a kill after dusk banks 50% more cash than the same kill by day — reward
    // for surviving the deadlier hours.
    {
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        std::vector<Value> sp = {Value::fromNum(50.0), Value::fromNum(0.0),
                                 Value::fromNum(10.0), Value::fromNum(0.0)};  // walker, score 10

        // Day kill: base salvage of 7 (5 + int(10/4)) at multiplier x1.
        SceneTree t1;
        zomboid::buildScene(t1);
        auto& vm1 = t1.scripts().vm();
        Value z1 = t1.findNode("Zombie0")->script();
        vm1.callOn(z1, "spawn_at", sp);
        CHECK(!(bool)(glob(t1, "g_phase") >= 30.0));   // starts in daytime
        vm1.callOn(z1, "take_damage", lethal);
        CHECK(glob(t1, "g_cash") == 7.0);

        // Night kill: force the clock past dusk (g_phase >= half of g_day_len) → +50% salvage.
        SceneTree t2;
        zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        const_cast<Value*>(vm2.getGlobal("g_phase"))->number = 40.0;   // deep night (day_len 60)
        Value z2 = t2.findNode("Zombie0")->script();
        vm2.callOn(z2, "spawn_at", sp);
        vm2.callOn(z2, "take_damage", lethal);
        CHECK(glob(t2, "g_cash") == 10.0);             // 7 + int(7/2)
    }

    // Frost Field power-up (kind 7): a sustained aura that halves every zombie's movement while it's
    // active — a frosted zombie covers noticeably less ground than an un-frosted one over the same span.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> sp = {Value::fromNum(20.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(10.0)};  // walker, speed 10

        // Control: no buff — the walker closes the normal distance toward the survivor.
        SceneTree t1;
        SceneNode* s1 = zomboid::buildScene(t1);
        s1->setPosition(0.0, 0.0);
        auto& vm1 = t1.scripts().vm();
        Value z1 = t1.findNode("Zombie0")->script();
        vm1.callOn(z1, "spawn_at", sp);
        const double x1 = t1.findNode("Zombie0")->x();
        for (int i = 0; i < 30; ++i) { vm1.callOn(z1, "_process", dt); }
        const double moved1 = x1 - t1.findNode("Zombie0")->x();

        // Frosted: the survivor holds a Frost Field buff, so the same walker crawls.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        Value s2v = s2->script();
        std::vector<Value> grant = {Value::fromNum(7.0)};
        vm2.callOn(s2v, "grant_powerup", grant);
        CHECK(sField(s2, "buff_kind")->number == 7.0);
        Value z2 = t2.findNode("Zombie0")->script();
        vm2.callOn(z2, "spawn_at", sp);
        const double x2 = t2.findNode("Zombie0")->x();
        for (int i = 0; i < 30; ++i) { vm2.callOn(z2, "_process", dt); }
        const double moved2 = x2 - t2.findNode("Zombie0")->x();

        CHECK(moved2 < moved1);                // frosted crawls less ground
        CHECK(moved2 < moved1 * 0.6);          // roughly half speed (with margin)
    }
    // Frost Field confers the chill STATUS, not just a movement slow: while the aura is up every zombie's
    // slow_timer is refreshed, so bodies turn brittle, frost-shatter on death, and casters are shut off
    // exactly like a cryo nova — the behaviour the field is documented to have. Without the field the same
    // walker is never chilled by merely existing.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        std::vector<Value> sp = {Value::fromNum(20.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(10.0)};  // walker

        // Frosted: the survivor holds a Frost Field, so the walker gains the chill status.
        SceneTree t1;
        SceneNode* s1 = zomboid::buildScene(t1);
        s1->setPosition(0.0, 0.0);
        auto& vm1 = t1.scripts().vm();
        Value s1v = s1->script();
        std::vector<Value> grant = {Value::fromNum(7.0)};
        vm1.callOn(s1v, "grant_powerup", grant);
        Value z1 = t1.findNode("Zombie0")->script();
        vm1.callOn(z1, "spawn_at", sp);
        CHECK(sField(t1.findNode("Zombie0"), "slow_timer")->number == 0.0);  // not chilled at spawn
        vm1.callOn(z1, "_process", dt);
        CHECK(sField(t1.findNode("Zombie0"), "slow_timer")->number > 0.0);   // the field applied the chill

        // Control: no Frost Field — the walker is never chilled just by being processed.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        Value z2 = t2.findNode("Zombie0")->script();
        vm2.callOn(z2, "spawn_at", sp);
        vm2.callOn(z2, "_process", dt);
        CHECK(sField(t2.findNode("Zombie0"), "slow_timer")->number == 0.0);  // no field → no chill
    }

    // Flawless-wave bonus: clearing a wave without taking a hit doubles the clear bonus, pays cash,
    // and patches the survivor up; taking any hit during the wave forfeits all of that.
    {
        auto clearWave = [](SceneTree& t) {
            for (SceneNode* z : t.nodesInGroup("zombies")) {
                if (z->script().instance->findField("alive")->boolean) {
                    Value zs = z->script();
                    std::vector<Value> dmg = {Value::fromNum(9999.0)};
                    t.scripts().vm().callOn(zs, "take_damage", dmg);
                }
            }
        };

        // Flawless case: no hit taken → double bonus (+100 at wave 1), +25 cash, +health.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        tree.process(1.0 / 60.0);                       // wave 1 opens, zombies spawn on the ring
        CHECK((int)glob(tree, "g_wave") == 1);
        sField(survivor, "health")->number = 50.0;      // wounded, so the flawless patch-up shows
        clearWave(tree);                                // kill them all, untouched
        const double s0 = glob(tree, "g_score");
        const double c0 = glob(tree, "g_cash");
        tree.process(1.0 / 60.0);                       // director awards the clear + flawless bonus
        SceneNode* dir = tree.findNode("Director");
        CHECK(sField(dir, "last_clean")->boolean);      // flagged flawless
        CHECK(glob(tree, "g_score") - s0 == 100.0);     // base 50 + flawless 50
        CHECK(glob(tree, "g_cash") - c0 == 25.0);       // cash reward
        CHECK(sField(survivor, "health")->number > 55.0); // patched up (~60)

        // Hit case: take one hit during the wave → only the base bonus, no cash, not flagged clean.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        t2.process(1.0 / 60.0);
        Value s2 = surv2->script();
        std::vector<Value> hit = {Value::fromNum(5.0)};
        t2.scripts().vm().callOn(s2, "take_damage", hit);   // spoils the flawless run
        clearWave(t2);
        const double s0b = glob(t2, "g_score");
        const double c0b = glob(t2, "g_cash");
        t2.process(1.0 / 60.0);
        SceneNode* dir2 = t2.findNode("Director");
        CHECK(!sField(dir2, "last_clean")->boolean);     // not flawless
        CHECK(glob(t2, "g_score") - s0b == 50.0);        // base bonus only
        CHECK(glob(t2, "g_cash") - c0b == 0.0);          // no cash reward
    }

    // Flawless streak: consecutive no-hit waves pay escalating cash (25, 40, 55, ...), and taking a
    // hit on any wave resets the streak to zero. Driven straight through the director's clear logic.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* dir = tree.findNode("Director");
        Value dv = dir->script();
        Value* clean = const_cast<Value*>(vm.getGlobal("g_wave_clean"));
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Clear a flawless wave `w` (with `w-1` already paid) and return the cash it awarded.
        auto flawlessClear = [&](int w, bool noHit) {
            sField(dir, "wave")->number = static_cast<double>(w);
            sField(dir, "bonus_wave")->number = static_cast<double>(w - 1);
            sField(dir, "break_timer")->number = 3.0;   // keep the wave from advancing this step
            clean->boolean = noHit;
            const double before = glob(tree, "g_cash");
            vm.callOn(dv, "_process", dt);
            return glob(tree, "g_cash") - before;
        };

        CHECK(flawlessClear(1, true) == 25.0);            // 1st flawless: 25
        CHECK((int)sField(dir, "clean_streak")->number == 1);
        CHECK(flawlessClear(2, true) == 40.0);            // 2nd in a row: 40
        CHECK((int)sField(dir, "clean_streak")->number == 2);
        CHECK(flawlessClear(3, true) == 55.0);            // 3rd in a row: 55
        CHECK((int)sField(dir, "clean_streak")->number == 3);
        CHECK(flawlessClear(4, false) == 0.0);            // took a hit: no cash, streak resets
        CHECK((int)sField(dir, "clean_streak")->number == 0);
        CHECK(flawlessClear(5, true) == 25.0);            // streak restarts at the base reward
        CHECK((int)sField(dir, "clean_streak")->number == 1);
    }

    // Summoner (kind 7) kiting: it keeps its distance — backing away when the survivor closes in
    // rather than shambling into melee, and drifting in only when far away.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Retreat case: parked close (inside the keep-away range), it moves AWAY from the survivor.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* sum = tree.findNode("Zombie0");
        Value sv = sum->script();
        std::vector<Value> sp = {Value::fromNum(8.0), Value::fromNum(0.0),
                                 Value::fromNum(7.0), Value::fromNum(4.0)};   // spawn kind 7, wave 4
        vm.callOn(sv, "spawn", sp);
        CHECK((int)sField(sum, "kind")->number == 7);
        const double dNear0 = sum->x();                 // starts 8 units out (player at origin)
        for (int i = 0; i < 30; ++i) vm.callOn(sv, "_process", dt);  // 0.5 s (< 4 s summon delay)
        CHECK(sum->x() > dNear0);                        // backed away — distance grew

        // Drift-in case: parked well beyond the hold range, it closes some of the gap.
        SceneTree t2;
        SceneNode* s2 = zomboid::buildScene(t2);
        s2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* sum2 = t2.findNode("Zombie0");
        Value s2v = sum2->script();
        std::vector<Value> sp2 = {Value::fromNum(30.0), Value::fromNum(0.0),
                                  Value::fromNum(7.0), Value::fromNum(4.0)};   // 30 units — far
        vm2.callOn(s2v, "spawn", sp2);
        const double dFar0 = sum2->x();
        for (int i = 0; i < 30; ++i) vm2.callOn(s2v, "_process", dt);
        CHECK(sum2->x() < dFar0);                        // drifted inward toward the survivor
    }

    // Killstreak milestones: every 10th unbroken kill pays a +15 cash bounty (and every 20th heals);
    // the 9th kill pays nothing extra, the 10th trips the milestone exactly.
    {
        SceneTree tree;
        zomboid::buildScene(tree);           // no process() — keep the field static, combo won't decay
        auto& vm = tree.scripts().vm();
        // Park 10 identical walkers (score_value 10 → 7 cash each) and kill them one at a time.
        std::vector<SceneNode*> zs;
        for (int i = 0; i < 10; ++i) {
            SceneNode* z = tree.findNode("Zombie" + std::to_string(i));
            Value zv = z->script();
            std::vector<Value> sp = {Value::fromNum(20.0 * (i + 1)), Value::fromNum(0.0),
                                     Value::fromNum(10.0), Value::fromNum(0.0)};  // far apart: no
            vm.callOn(zv, "spawn_at", sp);                                        // overkill splash chain
            zs.push_back(z);
        }
        CHECK(glob(tree, "g_cash") == 0.0);
        CHECK(glob(tree, "g_streak_rewards") == 0.0);
        std::vector<Value> lethal = {Value::fromNum(9999.0)};
        for (int i = 0; i < 9; ++i) {         // nine kills: combo 9, no milestone yet
            Value zv = zs[static_cast<size_t>(i)]->script();
            vm.callOn(zv, "take_damage", lethal);
        }
        CHECK((int)glob(tree, "g_combo") == 9);
        CHECK(glob(tree, "g_streak_rewards") == 0.0);   // not tripped before the 10th
        // Salvage scales with the streak multiplier: kills 1-4 at ×1 (7 each), 5-9 at ×2 (10 each).
        CHECK(glob(tree, "g_cash") == 78.0);            // 4*7 + 5*10, no bounty
        Value zv10 = zs[9]->script();
        vm.callOn(zv10, "take_damage", lethal);          // the 10th trips the milestone
        CHECK((int)glob(tree, "g_combo") == 10);
        CHECK(glob(tree, "g_streak_rewards") == 1.0);
        CHECK(glob(tree, "g_cash") == 107.0);           // 78 + 14 (×3 salvage) + 15 milestone bounty
    }

    // Sentry ammo: the auto-turret carries a limited magazine and shuts down once it's dry — even
    // with a target still in range and time left on its lifetime clock, so placement is a real choice.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* sentry = tree.findNode("Sentry0");
        Value sv = sentry->script();
        std::vector<Value> dep = {Value::fromNum(0.0), Value::fromNum(0.0)};
        vm.callOn(sv, "deploy", dep);
        const int ammoMax = (int)sField(sentry, "ammo")->number;
        CHECK(ammoMax > 0);
        CHECK(sField(sentry, "active")->boolean);

        // One fat target parked in firing range but OUTSIDE the sentry's self-destruct blast radius
        // (6 units), so only the bolts count against it — the farewell blast doesn't skew the tally.
        SceneNode* dummy = tree.findNode("Zombie0");
        Value dv = dummy->script();
        std::vector<Value> sp = {Value::fromNum(10.0), Value::fromNum(0.0),
                                 Value::fromNum(100000.0), Value::fromNum(0.0)};
        vm.callOn(dv, "spawn_at", sp);
        const double dmg = sField(sentry, "damage")->number;
        const double hp0 = sField(dummy, "health")->number;

        // Run ~9 s — long enough to empty a 25-bolt magazine (25/3 s) but under the 12 s lifetime.
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        for (int i = 0; i < 540; ++i) vm.callOn(sv, "_process", dt);
        CHECK(!sField(sentry, "active")->boolean);              // shut down: out of bolts, not time
        CHECK(sField(sentry, "life")->number > 0.0);            // lifetime clock still had time left
        CHECK((int)sField(sentry, "ammo")->number == 0);        // magazine emptied
        // It fired exactly ammoMax bolts — the target lost exactly that much health, no more.
        CHECK(hp0 - sField(dummy, "health")->number == ammoMax * dmg);
    }

    // Shotgun point-blank falloff: a pellet (falloff flag) hits hardest fresh and fades toward 40%
    // over its flight; a plain round ignores travel entirely; and the shotgun tags its pellets.
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        std::vector<Value> none;

        SceneNode* b = tree.findNode("Bullet0");
        Value bv = b->script();
        sField(b, "damage")->number = 100.0;
        sField(b, "max_life")->number = 2.0;

        // Plain round: flat damage regardless of how long it's been flying.
        sField(b, "falloff")->boolean = false;
        sField(b, "life")->number = 0.5;
        CHECK(vm.callOn(bv, "effective_damage", none).number == 100.0);

        // Pellet fresh out of the barrel (life == max_life): full damage.
        sField(b, "falloff")->boolean = true;
        sField(b, "life")->number = 2.0;
        CHECK(vm.callOn(bv, "effective_damage", none).number == 100.0);
        // Half its life spent (frac 0.5): half damage.
        sField(b, "life")->number = 1.0;
        CHECK(vm.callOn(bv, "effective_damage", none).number == 50.0);
        // Nearly spent (frac 0.1): clamped to the 40% floor, not lower.
        sField(b, "life")->number = 0.2;
        CHECK(vm.callOn(bv, "effective_damage", none).number == 40.0);

        // Wiring: firing the shotgun (weapon 1) tags its pellets with falloff.
        setWeapon(tree, survivor, 1);
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;
        Value sv = survivor->script();
        vm.callOn(sv, "do_shoot", none);
        bool anyFalloff = false;
        for (SceneNode* pel : tree.nodesInGroup("bullets")) {
            if (sField(pel, "active")->boolean && sField(pel, "falloff")->boolean) anyFalloff = true;
        }
        CHECK(anyFalloff);   // shotgun pellets carry the falloff flag
    }

    // Molotov crowd control: a zombie standing in a fire patch not only burns but stumbles (a slow),
    // so the flames hold a lane; a zombie outside the patch is untouched.
    {
        SceneTree tree;
        zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* fire = tree.findNode("Fire0");
        Value fv = fire->script();
        std::vector<Value> ig = {Value::fromNum(0.0), Value::fromNum(0.0)};   // burn patch at origin
        vm.callOn(fv, "ignite_ground", ig);

        SceneNode* inFire = tree.findNode("Zombie0");   // stands in the flames
        SceneNode* outFire = tree.findNode("Zombie1");  // parked well clear
        auto park = [&](SceneNode* z, double x) {
            Value zv = z->script();
            std::vector<Value> a = {Value::fromNum(x), Value::fromNum(0.0),
                                    Value::fromNum(100.0), Value::fromNum(0.0)};
            tree.scripts().vm().callOn(zv, "spawn_at", a);
        };
        park(inFire, 1.0);    // inside the radius-5 patch
        park(outFire, 40.0);  // far outside
        CHECK(sField(inFire, "slow_timer")->number == 0.0);
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};
        vm.callOn(fv, "_process", dt);
        CHECK(sField(inFire, "slow_timer")->number > 0.0);    // chilled/stumbling in the flames
        CHECK(sField(inFire, "burn_timer")->number > 0.0);    // and alight
        CHECK(sField(outFire, "slow_timer")->number == 0.0);  // clear of the patch — untouched
    }

    // Melee shove reliably staggers: a heavy swing flinches even a tanky brute it can't threaten with
    // damage alone (a create-space button), while the boss shrugs the shove off (stays un-staggered).
    {
        // Brute case: a high-wave brute takes far less than the stagger damage-threshold from the
        // 55-damage swing, so the only way it flinches is the melee's own guaranteed stagger.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* brute = tree.findNode("Zombie0");
        Value bz = brute->script();
        std::vector<Value> sp = {Value::fromNum(1.0), Value::fromNum(0.0),
                                 Value::fromNum(2.0), Value::fromNum(10.0)};  // brute, wave 10: hp 280
        vm.callOn(bz, "spawn", sp);
        CHECK(sField(brute, "stagger_timer")->number == 0.0);
        Value sv = survivor->script();
        std::vector<Value> none;
        vm.callOn(sv, "melee", none);
        CHECK(sField(brute, "stagger_timer")->number > 0.0);   // the shove flinched it
        CHECK(sField(brute, "alive")->boolean);                // but didn't kill it

        // Boss case: the shove connects but the boss (kind 3) is immune to stagger.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        surv2->setPosition(0.0, 0.0);
        auto& vm2 = t2.scripts().vm();
        SceneNode* boss = t2.findNode("Zombie0");
        Value bv = boss->script();
        std::vector<Value> bsp = {Value::fromNum(1.0), Value::fromNum(0.0),
                                  Value::fromNum(3.0), Value::fromNum(1.0)};   // boss
        vm2.callOn(bv, "spawn", bsp);
        Value s2v = surv2->script();
        vm2.callOn(s2v, "melee", none);
        CHECK(sField(boss, "stagger_timer")->number == 0.0);   // boss shrugs off the shove
    }

    // Boss enrage phase-2 adds: once the boss rages (below 35% health) it periodically calls in a
    // pair of runners, up to a fixed reinforcement budget — then stops (no endless flood).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        survivor->setPosition(0.0, 0.0);
        auto& vm = tree.scripts().vm();
        SceneNode* boss = tree.findNode("Zombie0");
        Value bv = boss->script();
        std::vector<Value> sp = {Value::fromNum(10.0), Value::fromNum(0.0),
                                 Value::fromNum(3.0), Value::fromNum(3.0)};   // boss, wave 3
        vm.callOn(bv, "spawn", sp);
        CHECK(aliveZombies(tree) == 1);                       // just the boss on the field
        const double mh = sField(boss, "max_health")->number;
        sField(boss, "health")->number = mh * 0.3;           // drop it below the 35% rage threshold
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // ~5 s: enrage triggers and the first reinforcement wave (2 runners) lands.
        for (int i = 0; i < 300; ++i) vm.callOn(bv, "_process", dt);
        CHECK(sField(boss, "enraged")->boolean);
        CHECK(aliveZombies(tree) >= 3);                       // boss + its first pair of adds

        // Long run: it keeps calling until the budget is spent, then stops — a bounded flood.
        for (int i = 0; i < 2400; ++i) vm.callOn(bv, "_process", dt);
        CHECK((int)sField(boss, "summon_budget")->number == 0);   // reinforcement budget spent
        CHECK(aliveZombies(tree) == 9);                           // boss + 4 waves * 2 runners = 9, capped
    }

    // Ultimate = panic button: unleashing the charged overcharge clears the field AND grants a brief
    // invulnerability window, so it's safe mid-swarm. A not-ready ultimate does nothing (no free i-frames).
    {
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        SceneNode* z = tree.findNode("Zombie0");
        Value zv = z->script();
        std::vector<Value> sp = {Value::fromNum(5.0), Value::fromNum(0.0),
                                 Value::fromNum(100.0), Value::fromNum(0.0)};
        vm.callOn(zv, "spawn_at", sp);
        Value sv = survivor->script();
        std::vector<Value> none;
        sField(survivor, "iframes")->number = 0.0;
        sField(survivor, "ult_ready")->boolean = true;
        vm.callOn(sv, "detonate", none);
        CHECK(!sField(z, "alive")->boolean);                 // screen-wide blast wiped the field
        CHECK(sField(survivor, "iframes")->number >= 1.0);   // and left the survivor briefly untouchable
        CHECK(!sField(survivor, "ult_ready")->boolean);      // ultimate spent

        // Not charged: detonating grants nothing.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        auto& vm2 = t2.scripts().vm();
        Value s2v = surv2->script();
        sField(surv2, "iframes")->number = 0.0;
        sField(surv2, "ult_ready")->boolean = false;
        vm2.callOn(s2v, "detonate", none);
        CHECK(sField(surv2, "iframes")->number == 0.0);      // no charge → no free i-frames
    }

    // Overflow power-up (kind 6): while active, holding fire spends no ammo and never reloads — the
    // magazine stays full through a long burst; without the buff, sustained fire drains the magazine.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        // Overflow case: magazine never dips below full while firing.
        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        sField(survivor, "aim_x")->number = 1.0;
        sField(survivor, "aim_y")->number = 0.0;
        Value sv = survivor->script();
        std::vector<Value> six = {Value::fromNum(6.0)};
        vm.callOn(sv, "grant_powerup", six);
        tree.process(1.0 / 60.0);                       // sync cur_ammo after the top-up
        const double full = sField(survivor, "cur_ammo")->number;
        const double shots0 = sField(survivor, "shots")->number;
        sField(survivor, "firing")->boolean = true;
        double minAmmo = full;
        for (int i = 0; i < 180; ++i) {                 // 3 s of held fire
            tree.process(1.0 / 60.0);
            const double a = sField(survivor, "cur_ammo")->number;
            if (a < minAmmo) minAmmo = a;
        }
        CHECK(sField(survivor, "shots")->number > shots0);   // it actually fired a burst
        CHECK(minAmmo == full);                              // yet the magazine never dropped

        // Control: no buff — the same burst drains the magazine below full at some point.
        SceneTree t2;
        SceneNode* surv2 = zomboid::buildScene(t2);
        sField(surv2, "aim_x")->number = 1.0;
        sField(surv2, "aim_y")->number = 0.0;
        t2.process(1.0 / 60.0);
        const double full2 = sField(surv2, "cur_ammo")->number;
        sField(surv2, "firing")->boolean = true;
        double minAmmo2 = full2;
        for (int i = 0; i < 180; ++i) {
            t2.process(1.0 / 60.0);
            const double a = sField(surv2, "cur_ammo")->number;
            if (a < minAmmo2) minAmmo2 = a;
        }
        CHECK(minAmmo2 < full2);                             // normal fire spent rounds
    }

    // Berserk power-up (kind 8): a combined offensive surge — it raises BOTH the fire-rate and the
    // damage multiplier at once (rapid boosts only rate, double-damage only damage). Verify the grant
    // sets both buff multipliers above 1, tags the buff kind, and starts the timer; and that the
    // resulting fire rate genuinely outpaces the un-buffed baseline.
    {
        std::vector<Value> dt = {Value::fromNum(1.0 / 60.0)};

        SceneTree tree;
        SceneNode* survivor = zomboid::buildScene(tree);
        auto& vm = tree.scripts().vm();
        Value sv = survivor->script();

        // Baseline fire rate with no buff (after one process tick to settle derived stats).
        tree.process(1.0 / 60.0);
        const double baseFr = sField(survivor, "fire_rate")->number;

        std::vector<Value> eight = {Value::fromNum(8.0)};
        vm.callOn(sv, "grant_powerup", eight);
        CHECK((int)sField(survivor, "buff_kind")->number == 8);   // tagged berserk
        CHECK(sField(survivor, "buff_timer")->number > 0.0);      // timer running
        CHECK(sField(survivor, "buff_fr")->number > 1.0);         // fire-rate boosted
        CHECK(sField(survivor, "buff_dmg")->number > 1.0);        // AND damage boosted
        // Distinct from rapid (fr-only) and double-damage (dmg-only): both are lifted together.
        CHECK(sField(survivor, "buff_fr")->number == 1.7);
        CHECK(sField(survivor, "buff_dmg")->number == 1.7);

        // The buff flows through to the derived fire_rate — a berserker fires faster.
        tree.process(1.0 / 60.0);
        CHECK(sField(survivor, "fire_rate")->number > baseFr);
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
