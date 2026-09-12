/*
 * NEON CELLS — logic tests.
 *
 *   node cells/tests/cells-logic.test.js
 *
 * No browser and no dependencies: the level generator, the combat maths, the
 * content tables and the save file are all plain modules.
 *
 * The promises worth testing in a roguelite are the ones that, when broken,
 * silently ruin runs:
 *   · the same seed always builds the same level;
 *   · the exit is always reachable, and nothing is ever placed where the player
 *     cannot get to it;
 *   · a scroll always makes your weapons hit harder, and by how much;
 *   · every enemy, weapon and skill the content tables mention really exists.
 */
'use strict';

const path = require('path');
const RNG = require(path.join(__dirname, '..', 'js', 'rng.js'));
const CONTENT = require(path.join(__dirname, '..', 'js', 'content.js'));
const LG = require(path.join(__dirname, '..', 'js', 'levelgen.js'));
const CB = require(path.join(__dirname, '..', 'js', 'combat.js'));
const META = require(path.join(__dirname, '..', 'js', 'meta.js'));

let passed = 0;
const failures = [];

function test(name, fn) {
  try {
    fn();
    passed++;
    console.log('  ok   ' + name);
  } catch (e) {
    failures.push(name + ' — ' + e.message);
    console.log('  FAIL ' + name + ' — ' + e.message);
  }
}

function assert(cond, message) {
  if (!cond) throw new Error(message || 'assertion failed');
}

function eq(actual, expected, message) {
  if (actual !== expected) {
    throw new Error((message || 'values differ') + ': got ' + JSON.stringify(actual) +
      ', expected ' + JSON.stringify(expected));
  }
}

function near(actual, expected, tolerance, message) {
  if (Math.abs(actual - expected) > (tolerance || 0.001)) {
    throw new Error((message || 'values differ') + ': got ' + actual + ', expected ~' + expected);
  }
}

const NORMAL_BIOMES = CONTENT.BIOMES.filter((b) => !b.boss).map((b) => b.id);
const BOSS_BIOMES = CONTENT.BIOMES.filter((b) => b.boss).map((b) => b.id);

/* ------------------------------------------------------------------ random */
console.log('\nSEEDED RANDOMNESS');

test('the same seed gives the same sequence', () => {
  const a = RNG.Rng(1234);
  const b = RNG.Rng(1234);
  for (let i = 0; i < 50; i++) eq(a.next(), b.next(), 'draw ' + i);
});

test('different seeds diverge', () => {
  const a = RNG.Rng(1);
  const b = RNG.Rng(2);
  let same = 0;
  for (let i = 0; i < 50; i++) if (a.next() === b.next()) same++;
  assert(same === 0, 'two seeds produced ' + same + ' identical draws');
});

test('int() covers both ends of its range and never leaves it', () => {
  const rng = RNG.Rng(7);
  const seen = new Set();
  for (let i = 0; i < 600; i++) {
    const v = rng.int(1, 4);
    assert(v >= 1 && v <= 4, 'out of range: ' + v);
    seen.add(v);
  }
  eq(seen.size, 4, 'values seen');
});

test('a string seed works as well as a number', () => {
  const a = RNG.Rng('run-one');
  const b = RNG.Rng('run-one');
  eq(a.next(), b.next());
  assert(RNG.Rng('run-two').next() !== a.next(), 'different strings should differ');
});

test('weighted() never returns a zero-weight entry', () => {
  const rng = RNG.Rng(9);
  const items = [{ w: 0, id: 'never' }, { w: 5, id: 'yes' }];
  for (let i = 0; i < 200; i++) {
    eq(rng.weighted(items, (it) => it.w).id, 'yes');
  }
});

/* ----------------------------------------------------------------- content */
console.log('\nTHE CONTENT TABLES');

test('every weapon has a colour that exists and a kind the game can swing', () => {
  const kinds = ['swing', 'thrust', 'slam', 'shoot', 'shield'];
  for (const w of CONTENT.WEAPONS) {
    assert(CONTENT.COLORS[w.color], w.id + ' has unknown colour ' + w.color);
    assert(kinds.indexOf(w.kind) !== -1, w.id + ' has unknown kind ' + w.kind);
    assert(w.dmg > 0 && w.rate > 0, w.id + ' must do damage at some speed');
    if (w.kind === 'shoot') assert(w.proj && w.proj.speed > 0, w.id + ' fires nothing');
    else assert(w.reach > 0, w.id + ' has no reach');
    if (w.tier > 0) assert(w.cost > 0, w.id + ' is unlockable but free');
  }
});

test('every status a weapon or skill inflicts is a real status', () => {
  const check = (list, owner) => {
    for (const s of list || []) {
      assert(CONTENT.STATUSES[s.status], owner + ' inflicts unknown status ' + s.status);
    }
  };
  for (const w of CONTENT.WEAPONS) check(w.onHit, w.id);
  for (const a of CONTENT.AFFIXES) check(a.onHit, a.id);
  for (const s of CONTENT.SKILLS) if (s.status) check([s.status], s.id);
});

test('every crit condition is one the combat code understands', () => {
  const known = ['backstab', 'airborne', 'close', 'bleeding', 'frozen', 'rooted', 'stunned'];
  for (const w of CONTENT.WEAPONS) {
    if (!w.crit) continue;
    assert(known.indexOf(w.crit.when) !== -1, w.id + ' crits on unknown condition ' + w.crit.when);
    assert(w.crit.mult > 1, w.id + ' crit does not actually crit');
  }
});

test('every biome spawns enemies that exist, and every boss biome names a real boss', () => {
  for (const b of CONTENT.BIOMES) {
    if (b.boss) {
      assert(CONTENT.BOSS[b.boss], b.id + ' names unknown boss ' + b.boss);
      continue;
    }
    assert(b.pool && b.pool.length, b.id + ' has no enemies');
    for (const id of b.pool) assert(CONTENT.ENEMY[id], b.id + ' spawns unknown enemy ' + id);
    assert(b.cols > 0 && b.rows > 0, b.id + ' has no room grid');
    assert(b.palette && b.palette.rock, b.id + ' has no palette');
  }
});

test('every enemy has a behaviour the entity code implements', () => {
  const ai = ['walker', 'archer', 'flyer', 'shielder', 'bomber', 'caster', 'slammer'];
  for (const e of CONTENT.ENEMIES) {
    assert(ai.indexOf(e.ai) !== -1, e.id + ' wants unknown behaviour ' + e.ai);
    assert(e.hp > 0 && e.dmg > 0 && e.w > 0 && e.h > 0, e.id + ' is not a viable body');
  }
});

test('the biomes are in increasing order of depth and end on a boss', () => {
  let last = 0;
  for (const b of CONTENT.BIOMES) {
    assert(b.depth > last, b.id + ' does not go deeper than the one before it');
    last = b.depth;
  }
  assert(CONTENT.BIOMES[CONTENT.BIOMES.length - 1].boss, 'the run must end on a boss');
});

test('every unlockable is a weapon or skill that exists, with a price', () => {
  for (const u of CONTENT.unlockables()) {
    const item = u.kind === 'weapon' ? CONTENT.WEAPON[u.id] : CONTENT.SKILL[u.id];
    assert(item, u.id + ' is unlockable but does not exist');
    assert(u.cost > 0, u.id + ' costs nothing');
    assert(u.name && u.desc, u.id + ' has nothing to show the player');
  }
  assert(CONTENT.unlockables().length >= 6, 'there should be a meaningful number of blueprints');
});

test('mutations are spread across all three colours', () => {
  const byColor = {};
  for (const m of CONTENT.MUTATIONS) {
    assert(CONTENT.COLORS[m.color], m.id + ' has unknown colour');
    assert(m.desc, m.id + ' has no description');
    byColor[m.color] = (byColor[m.color] || 0) + 1;
  }
  for (const c of CONTENT.COLOR_IDS) assert(byColor[c] >= 2, c + ' has too few mutations');
});

test('the starting pool can equip a player with no unlocks at all', () => {
  const starters = CONTENT.WEAPONS.filter((w) => w.tier === 0);
  const skills = CONTENT.SKILLS.filter((s) => s.tier === 0);
  assert(starters.length >= 4, 'too few starting weapons');
  assert(skills.length >= 2, 'too few starting skills');
  assert(CONTENT.WEAPON.rusty_sword.tier === 0, 'the starting sword must always be available');
});

/* ------------------------------------------------------------- level generation */
console.log('\nLEVEL GENERATION');

test('the same seed builds exactly the same level', () => {
  for (const id of NORMAL_BIOMES) {
    const a = LG.generate(id, 4242, { bossCells: 0 });
    const b = LG.generate(id, 4242, { bossCells: 0 });
    eq(a.w, b.w, id + ' width');
    eq(a.h, b.h, id + ' height');
    for (let i = 0; i < a.tiles.length; i++) {
      if (a.tiles[i] !== b.tiles[i]) throw new Error(id + ' differs at tile ' + i);
    }
    eq(a.enemies.length, b.enemies.length, id + ' enemy count');
    eq(JSON.stringify(a.spawnTile), JSON.stringify(b.spawnTile), id + ' spawn');
  }
});

test('a different seed builds a different level', () => {
  const a = LG.generate('promenade', 1, {});
  const b = LG.generate('promenade', 2, {});
  let diff = 0;
  for (let i = 0; i < a.tiles.length; i++) if (a.tiles[i] !== b.tiles[i]) diff++;
  assert(diff > 100, 'two seeds produced nearly the same level (' + diff + ' tiles differ)');
});

test('the exit is reachable from the spawn, in every biome, over many seeds', () => {
  for (const id of NORMAL_BIOMES) {
    for (let s = 0; s < 25; s++) {
      const level = LG.generate(id, 9000 + s * 13, {});
      assert(level.ok, id + ' seed ' + s + ' gave up on generating');
      const tile = { x: Math.floor(level.exit.x / LG.TILE), y: Math.floor((level.exit.y - 1) / LG.TILE) };
      assert(
        level.reach.has(LG.key(tile.x, tile.y)),
        id + ' seed ' + s + ': the exit is walled off'
      );
    }
  }
});

test('nothing is ever placed where the player cannot reach it', () => {
  for (const id of NORMAL_BIOMES) {
    for (let s = 0; s < 12; s++) {
      const level = LG.generate(id, 300 + s * 7, {});
      for (const o of level.objects) {
        assert(
          level.reach.has(LG.key(o.tile.x, o.tile.y)),
          id + ' seed ' + s + ': ' + o.type + ' is out of reach'
        );
      }
      for (const e of level.enemies) {
        assert(
          level.reach.has(LG.key(e.tile.x, e.tile.y)),
          id + ' seed ' + s + ': ' + e.id + ' is out of reach'
        );
      }
    }
  }
});

test('the player never spawns on spikes, and always on solid footing', () => {
  for (const id of NORMAL_BIOMES) {
    for (let s = 0; s < 20; s++) {
      const level = LG.generate(id, 77 + s, {});
      const t = level.spawnTile;
      assert(LG.standable(level, t.x, t.y), id + ' seed ' + s + ': spawn has no floor');
      assert(LG.at(level, t.x, t.y + 1) !== LG.T.SPIKE, id + ' seed ' + s + ': spawn is on spikes');
    }
  }
});

test('every level has a way out, a Collector, and something to fight', () => {
  for (const id of NORMAL_BIOMES) {
    const level = LG.generate(id, 555, {});
    assert(level.exit, id + ' has no exit');
    assert(level.objects.some((o) => o.type === 'collector'), id + ' has no Collector');
    assert(level.enemies.length >= 3, id + ' is empty (' + level.enemies.length + ' enemies)');
    assert(level.rooms.length === level.biome.cols * level.biome.rows, id + ' lost a room');
  }
});

test('the room you spawn in is left clear of enemies', () => {
  for (let s = 0; s < 15; s++) {
    const level = LG.generate('ramparts', 1200 + s, {});
    const start = level.rooms.find((r) => {
      return level.spawnTile.x >= r.x0 && level.spawnTile.x <= r.x1 &&
             level.spawnTile.y >= r.y0 && level.spawnTile.y <= r.floorY;
    });
    assert(start, 'seed ' + s + ': the spawn is not in any room');
    for (const e of level.enemies) {
      const inside = e.tile.x >= start.x0 && e.tile.x <= start.x1 &&
                     e.tile.y >= start.y0 && e.tile.y <= start.floorY;
      assert(!inside, 'seed ' + s + ': ' + e.id + ' is waiting in the spawn room');
    }
  }
});

test('deeper biomes send tougher enemies', () => {
  const shallow = LG.generate('quarters', 31, {});
  const deep = LG.generate('keep', 31, {});
  const avg = (level) => level.enemies.reduce((a, e) => a + e.hp, 0) / level.enemies.length;
  assert(avg(deep) > avg(shallow) * 1.3, 'the last biome is not meaningfully harder');
});

test('boss cells make a run harder still', () => {
  const plain = LG.generate('ossuary', 88, { bossCells: 0 });
  const hard = LG.generate('ossuary', 88, { bossCells: 2 });
  const avg = (level) => level.enemies.reduce((a, e) => a + e.hp, 0) / level.enemies.length;
  assert(avg(hard) > avg(plain), 'boss cells did not raise enemy health');
});

test('a boss arena has a boss, a sealed exit, and floor to fight on', () => {
  for (const id of BOSS_BIOMES) {
    const level = LG.generate(id, 11, {});
    assert(level.isArena, id + ' is not an arena');
    assert(level.boss && CONTENT.BOSS[level.boss.id], id + ' has no boss');
    assert(level.exit.locked, id + ' lets you walk straight past the boss');
    assert(level.reach.size > 30, id + ' has nowhere to stand (' + level.reach.size + ' tiles)');
    assert(level.enemies.length === 0, id + ' should start empty but for the boss');
  }
});

test('one-way platforms are used, so levels have vertical routes', () => {
  const level = LG.generate('sewers', 404, {});
  let platforms = 0;
  for (let i = 0; i < level.tiles.length; i++) if (level.tiles[i] === LG.T.PLATFORM) platforms++;
  assert(platforms > 40, 'only ' + platforms + ' platform tiles — the level is probably flat');
});

test('a room is never nothing but shooters', () => {
  /* Crossing the gap has to be possible: a room of pure ranged enemies means
   * taking fire the whole way in with no way to answer it. */
  const rng = RNG.Rng(31337);
  for (const biome of CONTENT.BIOMES) {
    if (biome.boss) continue;
    for (let count = 1; count <= 5; count++) {
      for (let t = 0; t < 40; t++) {
        const lineup = LG.composeRoom(rng, biome, count);
        eq(lineup.length, count, biome.id + ' room size');
        const ranged = lineup.filter(LG.isRangedEnemy).length;
        assert(
          ranged <= Math.max(1, Math.floor(count / 2)),
          biome.id + ': ' + ranged + ' of ' + count + ' are shooters (' + lineup.join(', ') + ')'
        );
        for (const id of lineup) assert(CONTENT.ENEMY[id], biome.id + ' picked unknown enemy ' + id);
      }
    }
  }
});

test('a generated level obeys the same rule', () => {
  for (const id of NORMAL_BIOMES) {
    const level = LG.generate(id, 2468, {});
    const byRoom = new Map();
    for (const spawn of level.enemies) {
      const room = level.rooms.find(function (r) {
        return spawn.tile.x >= r.x0 && spawn.tile.x <= r.x1 && spawn.tile.y >= r.y0 && spawn.tile.y <= r.floorY;
      });
      if (!room) continue;
      if (!byRoom.has(room)) byRoom.set(room, []);
      byRoom.get(room).push(spawn.id);
    }
    for (const [, lineup] of byRoom) {
      const ranged = lineup.filter(LG.isRangedEnemy).length;
      assert(
        ranged <= Math.max(1, Math.floor(lineup.length / 2)),
        id + ': a room holds ' + ranged + ' shooters out of ' + lineup.length
      );
    }
  }
});

test('the timed vault closes on a clock the player can beat', () => {
  const level = LG.generate('promenade', 606, {});
  assert(level.timeLimit > 60, 'the timed vault gives no time at all');
  assert(level.objects.some((o) => o.type === 'timed_chest'), 'no timed vault was placed');
});

/* -------------------------------------------------- walking it for real
 * Everything above is a model of the player. This drives the ACTUAL character
 * controller — the same gravity, jump arc, one-way platforms and tile collision
 * the game runs — from the spawn to the exit of real generated levels, using a
 * simple bot that re-plans from wherever it is, several times a second.
 *
 * Two things are being checked, and only one of them is about the bot:
 *
 *   · the controller really can cross these levels. The bot is crude, so it is
 *     not expected to manage every seed, but a clear majority must land;
 *   · it is NEVER stranded — from everywhere it ends up, including after a bad
 *     fall, a route to the exit still exists. That is the property that stops a
 *     run being unwinnable, and it is checked strictly.
 */
console.log('\nWALKING A LEVEL WITH THE REAL CHARACTER CONTROLLER');

const EN = require(path.join(__dirname, '..', 'js', 'entities.js'));

const IDLE_INPUT = {
  left: false, right: false, down: false, jump: false, jumpHeld: false, roll: false,
  atk1: false, atk2: false, atk1Held: false, atk2Held: false,
  skill1: false, skill2: false, flask: false, interact: false
};

function emptyWorld(level) {
  return {
    level: level,
    player: null,
    enemies: [], boss: null,
    projectiles: [], drops: [], particles: [], rings: [], slashes: [], texts: [],
    turrets: [], traps: [], orbs: [],
    time: 0, runTime: 0, shake: 0, freeze: 0, kills: 0, events: []
  };
}

function walkToExit(level, maxSeconds) {
  const world = emptyWorld(level);
  const player = EN.makePlayer(level, {
    stats: { brutality: 1, tactics: 1, survival: 1 },
    mutations: [],
    weapons: [CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none), null],
    skills: [null, null]
  });
  /* This measures getting there, not surviving spikes: a stalled bot would sit
   * in a spike pit, which tells us nothing about whether the level is crossable. */
  player.maxHp = 5000;
  player.hp = 5000;
  world.player = player;

  const exitTile = {
    x: Math.floor(level.exit.x / LG.TILE),
    y: Math.floor((level.exit.y - 1) / LG.TILE)
  };
  const midX = (t) => t.x * LG.TILE + LG.TILE / 2;
  const footY = (t) => t.y * LG.TILE + LG.TILE;
  const tileUnder = () => ({ x: Math.floor(player.x / LG.TILE), y: Math.floor((player.y - 1) / LG.TILE) });

  let plan = null;
  let hold = 0;
  let replanIn = 0;
  let stranded = 0;
  let bestPlan = Infinity;
  let sinceBest = 0;

  const frames = Math.round(maxSeconds * 60);
  for (let i = 0; i < frames; i++) {
    if (Math.hypot(player.x - level.exit.x, (player.y - 8) - (level.exit.y - 16)) < 24) {
      return { arrived: true, seconds: i / 60, stranded: stranded };
    }

    /* re-plan from where we actually are, not from where we meant to be */
    if (!plan || (replanIn <= 0 && player.onGround)) {
      const me = tileUnder();
      const route = LG.pathTo(level, me.x, me.y, exitTile.x, exitTile.y);
      if (route) plan = route;
      else stranded++;
      replanIn = 8;
    }
    replanIn--;
    if (!plan) return { arrived: false, stranded: stranded, reason: 'no route from the spawn at all' };

    const here = plan[0];
    const step = plan[Math.min(1, plan.length - 1)];
    const input = Object.assign({}, IDLE_INPUT);

    if (player.onGround) {
      if (step.via === 'jump') {
        const straightUp = step.x === here.x;
        const dx = midX(here) - player.x;
        if (straightUp && Math.abs(dx) > 3) {
          /* line up under a ladder before jumping, or you sail past it */
          if (dx > 0) input.right = true;
          else input.left = true;
        } else {
          input.jump = true;
          hold = here.y - step.y > 2 ? 14 : 8;
        }
      } else {
        const dx = midX(step) - player.x;
        if (dx > 2) input.right = true;
        else if (dx < -2) input.left = true;
        /* the route goes straight down: drop through the platform */
        if (footY(step) - player.y > 10 && Math.abs(dx) < 10) {
          input.down = true;
          input.jump = true;
        }
        const dir = input.right ? 1 : input.left ? -1 : 0;
        if (dir && LG.at(level, Math.floor((player.x + dir * 10) / LG.TILE), Math.floor((player.y - 8) / LG.TILE)) === LG.T.SOLID) {
          input.jump = true;
          hold = 10;
        }
      }
    } else {
      /* in the air: steer towards the landing spot, and brake past it */
      const dx = midX(step) - player.x;
      if (dx > 4) input.right = true;
      else if (dx < -4) input.left = true;
    }
    if (hold > 0) {
      input.jumpHeld = true;
      hold--;
    }

    EN.updatePlayer(world, input, 1 / 60);
    world.time += 1 / 60;
    if (player.dead) return { arrived: false, stranded: stranded, reason: 'died at ' + tileUnder().x + ',' + tileUnder().y };

    if (plan.length < bestPlan) {
      bestPlan = plan.length;
      sinceBest = 0;
    } else if (++sinceBest > 60 * 20) {
      return { arrived: false, stranded: stranded, reason: 'stopped making progress at ' + tileUnder().x + ',' + tileUnder().y };
    }
  }
  return { arrived: false, stranded: stranded, reason: 'ran out of time at ' + tileUnder().x + ',' + tileUnder().y };
}

test('the real character controller can cross a generated level', () => {
  const results = [];
  let arrived = 0;
  let attempts = 0;
  let slowest = 0;

  for (const id of NORMAL_BIOMES) {
    for (let s = 0; s < 3; s++) {
      const level = LG.generate(id, 60600 + s * 31, {});
      const result = walkToExit(level, 150);
      attempts++;
      if (result.arrived) {
        arrived++;
        slowest = Math.max(slowest, result.seconds);
      } else {
        results.push(id + ' seed ' + s + ': ' + result.reason);
      }
    }
  }

  /* The bot is deliberately simple, so it is held to a clear majority rather
   * than perfection; anything much below this means the terrain itself, not the
   * steering, has stopped being crossable. */
  assert(
    arrived >= Math.ceil(attempts * 0.75),
    'only ' + arrived + ' of ' + attempts + ' levels were crossed — ' + results.join(' | ')
  );
  assert(slowest < 120, 'the slowest crossing took ' + slowest.toFixed(0) + 's, which is too far to walk');
});

test('the player is never stranded somewhere with no route to the exit', () => {
  let stranded = 0;
  for (const id of NORMAL_BIOMES) {
    for (let s = 0; s < 3; s++) {
      const level = LG.generate(id, 777 + s * 97, {});
      stranded += walkToExit(level, 90).stranded;
    }
  }
  eq(stranded, 0, 'times the player had nowhere left to go');
});

test('a jump clears more than the generator assumes it can', () => {
  /* The generator promises reachability on the basis of a four-tile jump. If the
   * real jump ever got weaker than that, every level would start lying. */
  const height = (EN.P.jump * EN.P.jump) / (2 * EN.GRAVITY);
  assert(height > 4 * LG.TILE, 'a jump only clears ' + (height / LG.TILE).toFixed(2) + ' tiles');
});

/* ----------------------------------------------------------------- combat */
console.log('\nCOMBAT MATHS');

const rng = RNG.Rng(5);

function player(stats, mutations, extra) {
  return Object.assign({
    stats: Object.assign({ brutality: 1, tactics: 1, survival: 1 }, stats || {}),
    mutations: mutations || [],
    hp: 100, maxHp: 100, time: 100, status: {}
  }, extra || {});
}

test('a weapon at one scroll does its base damage', () => {
  const w = CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none);
  eq(CB.hitDamage(w, player(), {}).dmg, CONTENT.WEAPON.rusty_sword.dmg);
});

test('scrolls of the weapon\'s own colour scale it, and others do not', () => {
  const sword = CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none); // brutality
  const base = CB.hitDamage(sword, player(), {}).dmg;
  const scaled = CB.hitDamage(sword, player({ brutality: 6 }), {}).dmg;
  const wrong = CB.hitDamage(sword, player({ tactics: 6 }), {}).dmg;
  near(scaled / base, 1 + CB.SCALE_PER_POINT * 5, 0.05, 'five scrolls of the right colour');
  eq(wrong, base, 'tactics scrolls must not scale a brutality weapon');
});

test('health comes from survival', () => {
  const low = CB.maxHealth({ survival: 1 }, []);
  const high = CB.maxHealth({ survival: 6 }, []);
  assert(high > low, 'survival did not raise health');
  near(high - low, 11 * 5, 1, 'health per survival scroll');
});

test('a backstab with daggers triples the damage, from the front it does not', () => {
  const d = CB.makeWeapon(CONTENT.WEAPON.twin_daggers, CONTENT.AFFIX.none);
  const front = CB.hitDamage(d, player(), { behind: false }).dmg;
  const back = CB.hitDamage(d, player(), { behind: true }).dmg;
  near(back / front, 3, 0.2, 'backstab multiplier');
});

test('a crit that needs a status only lands when the target has it', () => {
  const blood = CB.makeWeapon(CONTENT.WEAPON.blood_sword, CONTENT.AFFIX.none);
  const clean = { status: {} };
  const bleeding = { status: { bleed: { t: 2, stacks: 1 } } };
  assert(!CB.critApplies(blood, { target: clean }), 'crit on a clean target');
  assert(CB.critApplies(blood, { target: bleeding }), 'no crit on a bleeding target');
});

test('affixes change the weapon they roll on', () => {
  const plain = CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none);
  const heavy = CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.heavy);
  const swift = CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.swift);
  assert(heavy.dmg > plain.dmg && heavy.rate > plain.rate, 'Heavy should hit harder and slower');
  assert(swift.rate < plain.rate, 'Swift should swing faster');
  assert(heavy.name.indexOf('Heavy') === 0, 'the affix should show in the name');
});

test('a rolled weapon is always usable', () => {
  for (let i = 0; i < 200; i++) {
    const w = CB.rollWeapon(rng, rng.pick(CONTENT.WEAPONS));
    assert(w.dmg > 0 && w.rate > 0, 'rolled an unusable weapon: ' + w.name);
    assert(CONTENT.COLORS[w.color], 'rolled an uncoloured weapon');
  }
});

test('Vengeance only pays out when you are nearly dead', () => {
  const muts = [CONTENT.MUTATION.vengeance];
  const healthy = player({}, muts, { hp: 100, maxHp: 100 });
  const dying = player({}, muts, { hp: 20, maxHp: 100 });
  eq(CB.playerDamageMultiplier(healthy), 1);
  near(CB.playerDamageMultiplier(dying), 1.6, 0.001);
});

test('Tranquillity pays out only after a quiet spell', () => {
  const muts = [CONTENT.MUTATION.tranquil];
  const justHit = player({}, muts, { time: 10, lastHurt: 8 });
  const calm = player({}, muts, { time: 10, lastHurt: 1 });
  eq(CB.playerDamageMultiplier(justHit), 1);
  near(CB.playerDamageMultiplier(calm), 1.45, 0.001);
});

test('Combo pays out just after you connect', () => {
  const muts = [CONTENT.MUTATION.combo];
  near(CB.playerDamageMultiplier(player({}, muts, { time: 10, lastHitLanded: 9 })), 1.25, 0.001);
  eq(CB.playerDamageMultiplier(player({}, muts, { time: 10, lastHitLanded: 2 })), 1);
});

test('Frenzy speeds your swing up and stacks', () => {
  const w = CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none);
  const calm = CB.attackInterval(w, player());
  const frenzied = CB.attackInterval(w, player({}, [], { frenzy: 4 }));
  assert(frenzied < calm, 'frenzy did not speed up attacks');
});

test('armour reduces damage, and a curse makes anything lethal', () => {
  const plain = player();
  const armoured = player({}, [CONTENT.MUTATION.armour]);
  assert(CB.incomingDamage(armoured, 100, 'melee') < CB.incomingDamage(plain, 100, 'melee'));
  const soldier = player({}, [CONTENT.MUTATION.soldier]);
  near(CB.incomingDamage(soldier, 100, 'trap'), 50, 1, 'spikes against a soldier');
  eq(CB.incomingDamage(soldier, 100, 'melee'), 100, 'melee against a soldier');
  const cursed = player({}, [], { cursed: true });
  assert(CB.incomingDamage(cursed, 1, 'melee') >= 9999, 'a curse must be lethal');
});

test('Gastronomy makes food heal more', () => {
  assert(CB.healAmount(player({}, [CONTENT.MUTATION.gastronomy]), 100) > CB.healAmount(player(), 100));
});

test('statuses tick damage over time and then expire', () => {
  const target = { status: {} };
  CB.applyStatus(target, 'bleed', 2);
  let total = 0;
  for (let i = 0; i < 180; i++) total += CB.tickStatuses(target, 1 / 60, {}).damage;
  near(total, CONTENT.STATUSES.bleed.dps * 2, 1, 'two seconds of bleeding');
  assert(!CB.hasStatus(target, 'bleed'), 'bleeding should have worn off');
});

test('Open Wounds makes bleeding and burning hurt more', () => {
  const bleed = (opts) => {
    const t = { status: {} };
    CB.applyStatus(t, 'bleed', 1);
    let total = 0;
    for (let i = 0; i < 60; i++) total += CB.tickStatuses(t, 1 / 60, opts).damage;
    return total;
  };
  near(bleed({ openWounds: true }) / bleed({}), 1.5, 0.05);
});

test('bleeding stacks but freezing does not', () => {
  const t = { status: {} };
  CB.applyStatus(t, 'bleed', 3);
  CB.applyStatus(t, 'bleed', 3);
  eq(t.status.bleed.stacks, 2, 'bleed stacks');
  CB.applyStatus(t, 'frozen', 2);
  CB.applyStatus(t, 'frozen', 2);
  eq(t.status.frozen.stacks, 1, 'frozen stacks');
});

test('being frozen, rooted or stunned stops you moving', () => {
  for (const id of ['frozen', 'rooted', 'stun']) {
    const t = { status: {} };
    CB.applyStatus(t, id, 1);
    eq(CB.statusSpeedMultiplier(t), 0, id + ' should hold the target still');
  }
  eq(CB.statusSpeedMultiplier({ status: {} }), 1, 'an unaffected target moves freely');
});

test('damage over time does not tick once per frame', () => {
  /* A status arrives a fraction of a point at a time, sixty times a second.
   * Rounding each of those fractions up to a whole point made poison twelve
   * times deadlier than its own numbers claim — and it killed reference runs
   * outright, which is how it was found. */
  const level = LG.generate('bridge', 12, {});
  level.boss = null;
  const world = emptyWorld(level);
  const player = EN.makePlayer(level, {
    stats: { brutality: 1, tactics: 1, survival: 1 }, mutations: [],
    weapons: [CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none), null], skills: [null, null]
  });
  world.player = player;
  player.invuln = 0;

  const before = player.hp;
  CB.applyStatus(player, 'poison', 2);
  for (let i = 0; i < 120; i++) EN.updatePlayer(world, Object.assign({}, IDLE_INPUT), 1 / 60);

  const lost = before - player.hp;
  const expected = CONTENT.STATUSES.poison.dps * 2;
  assert(
    Math.abs(lost - expected) < 2,
    'two seconds of poison took ' + lost.toFixed(1) + ' health, but poison is ' +
      CONTENT.STATUSES.poison.dps + ' a second'
  );
  assert(!player.dead, 'a single poison should not be lethal to a full-health player');
});

test('enemy numbers rise with depth and with boss cells', () => {
  const def = CONTENT.ENEMY.zombie;
  const d1 = CB.enemyStats(def, 1, 0);
  const d5 = CB.enemyStats(def, 5, 0);
  const d5bc = CB.enemyStats(def, 5, 2);
  eq(d1.hp, def.hp, 'depth 1 is the baseline');
  assert(d5.hp > d1.hp && d5.dmg > d1.dmg, 'depth did not scale enemies');
  assert(d5bc.hp > d5.hp, 'boss cells did not scale enemies');
});

/* ------------------------------------------------------------- difficulty
 * A fight, simulated. A reference player geared the way a run would be geared
 * at that depth is dropped into a room of that biome's enemies and plays it
 * straight: close, swing, and roll through anything winding up. The point is
 * not that the numbers are perfect — only that the curve stays sane, so a
 * balance change that makes a biome unsurvivable cannot land quietly.
 */
console.log('\nTHE DIFFICULTY CURVE');

function scrollsBefore(index) {
  let n = 0;
  for (let i = 0; i < index; i++) n += CONTENT.BIOMES[i].scrolls || 0;
  return n;
}

/* the usual shape of a run: most scrolls into your weapon's colour */
function buildFor(index, color) {
  const total = scrollsBefore(index);
  const main = Math.round(total * 0.7);
  const stats = { brutality: 1, tactics: 1, survival: 1 };
  stats[color] += main;
  stats.survival += total - main;
  return stats;
}

/* What a run actually looks like at a given depth: a weapon, the skill its
 * colour is built around, and one mutation per biome already cleared. A naked
 * sword with nothing else is not a representative player by the third biome —
 * the game hands you all three of those on the way down. */
function kitFor(biomeIndex, color) {
  const skill = CONTENT.SKILLS.filter(function (s) { return s.tier === 0 && s.color === color; })[0] ||
                CONTENT.SKILL.shockwave;
  const mutations = CONTENT.MUTATIONS
    .filter(function (m) { return m.color === color; })
    .slice(0, Math.max(0, Math.min(3, biomeIndex)));
  return { skill: biomeIndex >= 1 ? skill : null, mutations: mutations };
}

function simulateRoom(biomeIndex, weaponId, seed, count, maxSeconds) {
  const biome = CONTENT.BIOMES[biomeIndex];
  const rng = RNG.Rng(seed);

  /* Enemy behaviour rolls dice — attack timing, leaps, patrol direction. Point
   * those dice at a seeded source for the duration of the fight, so the same
   * seed always plays out the same way and these numbers can be compared
   * between runs and between changes. */
  const dice = RNG.Rng(seed ^ 0x5bf03635);
  EN.setRandom(dice.next);
  CB.setRandom(dice.next);
  const level = LG.generate('bridge', seed, {});   // a flat arena to fight in
  level.boss = null;

  const world = emptyWorld(level);
  const weapon = CB.makeWeapon(CONTENT.WEAPON[weaponId], CONTENT.AFFIX.none);
  const kit = kitFor(biomeIndex, weapon.color);
  const player = EN.makePlayer(level, {
    stats: buildFor(biomeIndex, weapon.color),
    mutations: kit.mutations,
    weapons: [weapon, null],
    skills: [kit.skill, null],
    flasks: 2
  });
  world.player = player;
  player.x = level.spawn.x + 40;
  player.y = level.spawn.y;

  for (const id of LG.composeRoom(rng, biome, count)) {
    const def = CONTENT.ENEMY[id];
    const stats = CB.enemyStats(def, biome.depth, 0);
    const enemy = EN.makeEnemy({
      id: id,
      pos: { x: player.x + 150 + world.enemies.length * 60, y: level.spawn.y },
      hp: stats.hp, dmg: stats.dmg
    });
    enemy.aggro = true;
    world.enemies.push(enemy);
  }

  function done(result) {
    EN.setRandom(null);
    CB.setRandom(null);
    return result;
  }

  let worstHp = player.hp;
  const frames = Math.round(maxSeconds * 60);
  for (let i = 0; i < frames; i++) {
    const alive = world.enemies.filter(function (e) { return !e.dead; });
    if (!alive.length) {
      return done({ cleared: true, seconds: i / 60, hpLeft: player.hp / player.maxHp, worst: worstHp / player.maxHp });
    }

    let target = alive[0];
    let best = Infinity;
    for (const e of alive) {
      const d = Math.hypot(e.x - player.x, e.y - player.y);
      if (d < best) { best = d; target = e; }
    }

    const input = Object.assign({}, IDLE_INPUT);
    const dir = Math.sign(target.x - player.x) || 1;
    const threatened = alive.some(function (e) {
      return e.state === 'windup' && Math.hypot(e.x - player.x, e.y - player.y) < 46;
    });

    if (threatened && player.rollCd <= 0) {
      input.roll = true;
      if (dir > 0) input.right = true; else input.left = true;
    } else if (best > weapon.reach * 0.8) {
      if (dir > 0) input.right = true; else input.left = true;
    } else {
      input.atk1 = true;
      input.atk1Held = true;
    }
    if (kit.skill && player.skillCd[0] <= 0 && best < 140) input.skill1 = true;
    if (player.hp < player.maxHp * 0.35 && player.flasks > 0) input.flask = true;

    EN.updatePlayer(world, input, 1 / 60);
    EN.updateEnemies(world, 1 / 60);
    EN.updateProjectiles(world, 1 / 60);
    EN.updateEffects(world, 1 / 60);
    world.time += 1 / 60;

    worstHp = Math.min(worstHp, player.hp);
    if (player.dead) return done({ cleared: false, seconds: i / 60, hpLeft: 0, worst: 0, died: true });
  }
  return done({ cleared: false, seconds: maxSeconds, hpLeft: player.hp / player.maxHp, worst: worstHp / player.maxHp, timeout: true });
}

test('a reference player can clear a room in every biome', () => {
  const trouble = [];
  for (let b = 0; b < CONTENT.BIOMES.length; b++) {
    const biome = CONTENT.BIOMES[b];
    if (biome.boss) continue;
    let cleared = 0;
    let slowest = 0;
    const tries = 5;
    for (let t = 0; t < tries; t++) {
      const result = simulateRoom(b, 'rusty_sword', 900 + t * 37, 3, 45);
      if (result.cleared) {
        cleared++;
        slowest = Math.max(slowest, result.seconds);
      } else {
        trouble.push(biome.id + ' seed ' + t + (result.died ? ': died' : ': ran out of time'));
      }
    }
    /* A clear majority, not every time: enemy behaviour is not seeded, and a
     * roguelite that never kills you is not a roguelite. What this catches is a
     * biome that has stopped being survivable. */
    assert(cleared >= 3, biome.id + ' was cleared only ' + cleared + ' times in ' + tries + ' — ' + trouble.join(', '));
    assert(slowest < 40, biome.id + ' took ' + slowest.toFixed(0) + 's to clear one room');
  }
});

test('no biome is a health sink', () => {
  /* The median of several fights, not the worst: one unlucky death is a
   * roguelite working as intended, and a test that failed on it would only get
   * muted. What must not happen is a biome that eats most of your health as a
   * matter of course. */
  for (let b = 0; b < CONTENT.BIOMES.length; b++) {
    const biome = CONTENT.BIOMES[b];
    if (biome.boss) continue;
    const lost = [];
    /* enough fights that the median is steady — enemy AI is not seeded */
    for (let t = 0; t < 9; t++) {
      lost.push(1 - simulateRoom(b, 'rusty_sword', 900 + t * 37, 3, 45).worst);
    }
    lost.sort(function (a, c) { return a - c; });
    const median = lost[Math.floor(lost.length / 2)];
    assert(
      median < 0.75,
      biome.id + ' costs ' + Math.round(median * 100) + '% of your health for one room, typically'
    );
  }
});

test('the run gets harder the deeper it goes', () => {
  /* Asserted on the scaling itself rather than on a simulated fight: a bot that
   * dodges every telegraphed blow perfectly makes slow, heavy enemies look
   * easy, which says more about the bot than about the game. */
  const zombie = CONTENT.ENEMY.zombie;
  let last = null;
  for (const biome of CONTENT.BIOMES) {
    const stats = CB.enemyStats(zombie, biome.depth, 0);
    if (last) {
      assert(stats.hp > last.hp, biome.id + ' enemies are no tougher than the biome before');
      assert(stats.dmg > last.dmg, biome.id + ' enemies hit no harder than the biome before');
    }
    last = stats;
  }

  /* and the last biome must ask a great deal more than the first */
  const first = CB.enemyStats(zombie, CONTENT.BIOMES[0].depth, 0);
  const deepest = CB.enemyStats(zombie, CONTENT.BIOMES[CONTENT.BIOMES.length - 1].depth, 0);
  assert(deepest.hp > first.hp * 2, 'the deepest enemies are not even twice as tough');
  assert(deepest.dmg > first.dmg * 2, 'the deepest enemies do not even hit twice as hard');

  /* one formula, in one place: the level generator must agree with it */
  const level = LG.generate('ramparts', 4242, {});
  const biome = CONTENT.BIOME.ramparts;
  for (const spawn of level.enemies) {
    if (spawn.elite) continue;
    const expected = CB.enemyStats(CONTENT.ENEMY[spawn.id], biome.depth, 0);
    eq(spawn.hp, expected.hp, spawn.id + ' health as placed in a level');
    eq(spawn.dmg, expected.dmg, spawn.id + ' damage as placed in a level');
  }
});

/* A boss fight, simulated the way the room fights are: read the telegraph, get
 * out of the way, close and hit it. What is being checked is that both bosses
 * are beatable by an ordinary build, that neither falls over in a few seconds,
 * and that a Boss Cell really does make the next run harder. */
function simulateBoss(biomeIndex, weaponId, seed, bossCells, maxSeconds) {
  const dice = RNG.Rng(seed ^ 0x5bf03635);
  EN.setRandom(dice.next);
  CB.setRandom(dice.next);

  const biome = CONTENT.BIOMES[biomeIndex];
  const level = LG.generate(biome.id, seed, { bossCells: bossCells });
  const world = emptyWorld(level);
  world.bossCells = bossCells;

  const weapon = CB.makeWeapon(CONTENT.WEAPON[weaponId], CONTENT.AFFIX.none);
  const kit = kitFor(biomeIndex, weapon.color);
  const player = EN.makePlayer(level, {
    stats: buildFor(biomeIndex, weapon.color),
    mutations: kit.mutations,
    weapons: [weapon, null],
    skills: [kit.skill, null],
    flasks: 3
  });
  world.player = player;
  world.boss = EN.makeBoss(world, level);

  function done(result) {
    EN.setRandom(null);
    CB.setRandom(null);
    return result;
  }

  let sawStagger = false;
  const frames = Math.round(maxSeconds * 60);
  for (let i = 0; i < frames; i++) {
    const boss = world.boss;
    if (boss.vulnerable) sawStagger = true;
    if (boss.dead) return done({ won: true, seconds: i / 60, sawStagger: sawStagger });

    const input = Object.assign({}, IDLE_INPUT);
    const dist = Math.hypot(boss.x - player.x, boss.y - player.y);
    const dir = Math.sign(boss.x - player.x) || 1;
    const winding = boss.state === 'telegraph' || boss.state === 'charge' || boss.state === 'dash';
    const incoming = world.projectiles.some(function (pr) {
      return pr.from === 'enemy' && Math.hypot(pr.x - player.x, pr.y - (player.y - 10)) < 40;
    });

    if (((winding && dist < 90) || incoming) && player.rollCd <= 0) {
      input.roll = true;
      if (dir > 0) input.left = true; else input.right = true;
    } else if (dist > weapon.reach * 0.8 + boss.w / 2) {
      if (dir > 0) input.right = true; else input.left = true;
      if (boss.state === 'slam' && dist < 70 && player.onGround) {
        input.jump = true;
        input.jumpHeld = true;
      }
    } else {
      input.atk1 = true;
      input.atk1Held = true;
    }
    if (kit.skill && player.skillCd[0] <= 0 && dist < 140) input.skill1 = true;
    if (player.hp < player.maxHp * 0.4 && player.flasks > 0) input.flask = true;

    EN.updatePlayer(world, input, 1 / 60);
    EN.updateEnemies(world, 1 / 60);
    EN.updateProjectiles(world, 1 / 60);
    EN.updateEffects(world, 1 / 60);
    world.time += 1 / 60;

    if (player.dead) return done({ won: false, seconds: i / 60, sawStagger: sawStagger, died: true });
  }
  return done({ won: false, seconds: maxSeconds, sawStagger: sawStagger, timeout: true });
}

test('both bosses can be beaten by an ordinary build, and neither is a pushover', () => {
  for (let b = 0; b < CONTENT.BIOMES.length; b++) {
    const biome = CONTENT.BIOMES[b];
    if (!biome.boss) continue;

    let won = 0;
    let quickest = Infinity;
    let staggered = false;
    const tries = 3;
    for (let t = 0; t < tries; t++) {
      const result = simulateBoss(b, 'war_hammer', 700 + t * 53, 0, 90);
      if (result.won) {
        won++;
        quickest = Math.min(quickest, result.seconds);
      }
      if (result.sawStagger) staggered = true;
    }

    assert(won >= 2, biome.boss + ' was beaten only ' + won + ' times in ' + tries);
    assert(quickest > 5, biome.boss + ' died in ' + quickest.toFixed(1) + 's — that is not a boss fight');
    assert(staggered, biome.boss + ' never left an opening to punish');
  }
});

test('a Boss Cell really does make the next run harder', () => {
  const index = CONTENT.BIOMES.findIndex(function (b) { return b.boss === 'warden'; });
  const time = function (cells) {
    let total = 0;
    for (let t = 0; t < 3; t++) total += simulateBoss(index, 'war_hammer', 700 + t * 53, cells, 90).seconds;
    return total / 3;
  };
  const plain = time(0);
  const hard = time(2);
  assert(hard > plain, 'two Boss Cells made the fight take ' + hard.toFixed(1) + 's against ' + plain.toFixed(1) + 's');
});

test('a staggered boss takes more damage, and only while it is open', () => {
  const index = CONTENT.BIOMES.findIndex(function (b) { return b.boss === 'warden'; });
  const level = LG.generate(CONTENT.BIOMES[index].id, 3, {});
  const world = emptyWorld(level);
  world.player = EN.makePlayer(level, {
    stats: { brutality: 1, tactics: 1, survival: 1 }, mutations: [],
    weapons: [CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none), null], skills: [null, null]
  });
  const boss = EN.makeBoss(world, level);
  world.boss = boss;

  const before = boss.hp;
  EN.damageEnemy(world, boss, 100, { silent: true });
  const normal = before - boss.hp;

  boss.vulnerable = true;
  const openBefore = boss.hp;
  EN.damageEnemy(world, boss, 100, { silent: true });
  const open = openBefore - boss.hp;

  assert(open > normal, 'a staggered boss took ' + open + ' where a guarded one took ' + normal);
  eq(Math.round(open / normal * 10) / 10, 1.6, 'the stagger multiplier');
});

test('a shieldbearer can be broken through as well as gone around', () => {
  const level = LG.generate('bridge', 5, {});
  level.boss = null;
  const world = emptyWorld(level);
  const player = EN.makePlayer(level, {
    stats: { brutality: 1, tactics: 1, survival: 1 }, mutations: [],
    weapons: [CB.makeWeapon(CONTENT.WEAPON.rusty_sword, CONTENT.AFFIX.none), null], skills: [null, null]
  });
  world.player = player;

  const def = CONTENT.ENEMY.shielder;
  const enemy = EN.makeEnemy({ id: 'shielder', pos: { x: 300, y: level.spawn.y }, hp: def.hp, dmg: def.dmg });
  enemy.facing = -1;                 // facing the attacker, shield up
  world.enemies.push(enemy);

  assert(!enemy.shieldBroken, 'a shieldbearer starts behind its shield');
  const hpBefore = enemy.hp;
  EN.damageEnemy(world, enemy, 10, { knockDir: 1, silent: true });
  assert(enemy.hp > hpBefore - 10, 'a frontal hit should mostly be blocked');

  /* keep at it and the shield gives out */
  for (let i = 0; i < 40 && !enemy.shieldBroken; i++) {
    EN.damageEnemy(world, enemy, 10, { knockDir: 1, silent: true });
  }
  assert(enemy.shieldBroken, 'the shield never broke');
  assert(CB.hasStatus(enemy, 'stun'), 'breaking a shield should stagger its owner');

  const before = enemy.hp;
  EN.damageEnemy(world, enemy, 10, { knockDir: 1, silent: true });
  eq(Math.round(before - enemy.hp), 10, 'damage after the shield breaks');

  /* and a hit from behind was never blocked in the first place */
  const other = EN.makeEnemy({ id: 'shielder', pos: { x: 300, y: level.spawn.y }, hp: def.hp, dmg: def.dmg });
  other.facing = 1;
  world.enemies.push(other);
  const backBefore = other.hp;
  EN.damageEnemy(world, other, 10, { knockDir: 1, behind: true, silent: true });
  eq(Math.round(backBefore - other.hp), 10, 'a backstab ignores the shield');
});

test('a shot leans onto what is in front of you, within a narrow cone', () => {
  const level = LG.generate('bridge', 6, {});
  level.boss = null;
  const world = emptyWorld(level);
  world.player = EN.makePlayer(level, {
    stats: { brutality: 1, tactics: 1, survival: 1 }, mutations: [],
    weapons: [CB.makeWeapon(CONTENT.WEAPON.bow, CONTENT.AFFIX.none), null], skills: [null, null]
  });

  const straight = EN.aimAngle(world, 100, 100, 1, 280, 0.5);
  eq(straight, 0, 'with nothing in front, a shot goes straight ahead');

  /* a bat hovering above head height: a flat shot could never reach it */
  const bat = EN.makeEnemy({ id: 'bat', pos: { x: 200, y: 70 }, hp: 10, dmg: 1 });
  world.enemies.push(bat);
  const lifted = EN.aimAngle(world, 100, 100, 1, 280, 0.5);
  assert(lifted < -0.05, 'the shot did not lift towards the bat (angle ' + lifted.toFixed(2) + ')');
  assert(Math.abs(lifted) <= 0.5, 'assist must stay inside the cone, not turn into aimbotting');

  /* something behind you is not a target */
  world.enemies.length = 0;
  world.enemies.push(EN.makeEnemy({ id: 'bat', pos: { x: 20, y: 70 }, hp: 10, dmg: 1 }));
  eq(EN.aimAngle(world, 100, 100, 1, 280, 0.5), 0, 'a shot must not curve backwards');
});

/* ------------------------------------------------------------------- save */
console.log('\nWHAT SURVIVES DEATH');

test('a fresh save starts empty', () => {
  const state = META.reset();
  eq(state.cells, 0);
  eq(state.unlocked.length, 0);
  eq(state.bossCells, 0);
});

test('cells bank, and blueprints cost what they say', () => {
  META.reset();
  META.bankCells(100);
  eq(META.load().cells, 100);
  const entry = CONTENT.unlockables()[0];
  assert(META.unlock(entry.id, entry.cost), 'the purchase should have gone through');
  eq(META.load().cells, 100 - entry.cost, 'cells were not deducted');
  assert(META.isUnlocked(entry.id), 'the blueprint was not recorded');
  assert(!META.unlock(entry.id, entry.cost), 'buying the same blueprint twice');
});

test('you cannot buy what you cannot afford', () => {
  META.reset();
  META.bankCells(5);
  const entry = CONTENT.unlockables()[0];
  assert(!META.unlock(entry.id, entry.cost), 'bought a blueprint without the cells');
  eq(META.load().cells, 5, 'cells should be untouched');
});

test('a win records a boss cell, a loss records the depth', () => {
  META.reset();
  META.recordRun({ won: false, kills: 10, depth: 3, biome: 'Toxic Sewers', time: 400 });
  let state = META.load();
  eq(state.runs, 1);
  eq(state.bestDepth, 3);
  eq(state.bossCells, 0);
  state = META.recordRun({ won: true, kills: 90, depth: 8, biome: 'Throne Room', time: 1500, bossCells: 0 });
  eq(state.wins, 1);
  eq(state.bossCells, 1, 'winning should earn a boss cell');
  eq(state.bestTime, 1500);
  eq(state.kills, 100, 'kills accumulate across runs');
});

test('boss cells do not run away with themselves', () => {
  META.reset();
  for (let i = 0; i < 12; i++) {
    META.recordRun({ won: true, kills: 1, depth: 8, time: 100, bossCells: META.load().bossCells });
  }
  assert(META.load().bossCells <= 5, 'boss cells should cap');
});

/* ----------------------------------------------------------------- report */
console.log('');
if (failures.length) {
  console.error(`✖ ${failures.length} of ${passed + failures.length} checks failed:\n`);
  for (const f of failures) console.error('  - ' + f);
  console.error('');
  process.exit(1);
}
console.log(`✓ all ${passed} checks passed\n`);
