/*
 * ZOMBOID: ANCHORAGE — logic tests.
 *
 *   node zomboid/tests/zomboid-logic.test.js
 *
 * No dependencies and no browser. The game's data and world layers — the
 * Anchorage map, the generator that bakes it into tiles, and the item and loot
 * tables — are plain scripts that only need a `window` to hang their globals
 * on, so they are run here in a vm sandbox with a stub one. That keeps the game
 * files exactly as the page loads them: no export lines added for the tests'
 * benefit, nothing that can drift between what is tested and what is shipped.
 *
 * game.js is not loaded. It wants a document, a canvas and an audio context,
 * and it is covered where it belongs — scripts/smoke-site.cjs drives it in a
 * real Chromium and proves it paints.
 *
 * What these hold onto is the kind of break nobody notices until they are
 * playing: a building standing on the spot the player spawns, a door in a wall
 * that is not there, a loot table naming an item that was renamed.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');

const JS = path.join(__dirname, '..', 'js');

/* Load the game's own scripts into one sandbox, exactly as the page does. */
const sandbox = { console };
sandbox.window = sandbox;
sandbox.globalThis = sandbox;
vm.createContext(sandbox);
for (const file of ['world.js', 'worldgen.js', 'items.js']) {
  vm.runInContext(fs.readFileSync(path.join(JS, file), 'utf8'), sandbox, { filename: file });
}

const A = sandbox.ANCHORAGE;
const WORLDGEN = sandbox.WORLDGEN;
const ITEMS = sandbox.ITEMS;
const LOOT = sandbox.LOOT;
const T = A.T;

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

/* ------------------------------------------------------------- the map data */
console.log('\nDOWNTOWN ANCHORAGE');

test('every script exposed what the game expects to find', () => {
  for (const [name, value] of Object.entries({ ANCHORAGE: A, WORLDGEN, ITEMS, LOOT })) {
    assert(value, `${name} is missing — the page would throw on load`);
  }
});

test('the map has a size and a full set of tile codes', () => {
  assert(A.MAP_W > 0 && A.MAP_H > 0, 'the map has no size');
  const required = ['GRASS', 'STREET', 'SIDEWALK', 'FLOOR', 'WALL', 'DOOR', 'WATER', 'TREE', 'LOT', 'RAIL'];
  const missing = required.filter((k) => typeof T[k] !== 'number');
  eq(missing.join(', '), '', 'tile codes the generator uses but the data does not define');
});

test('every building stands inside the map', () => {
  A.BUILDINGS.forEach((b) => {
    assert(b.name, 'a building has no name');
    assert(b.w > 0 && b.h > 0, `${b.name} has no footprint`);
    assert(b.x >= 0 && b.y >= 0 && b.x + b.w <= A.MAP_W && b.y + b.h <= A.MAP_H,
      `${b.name} hangs off the edge of the map`);
  });
});

test('every door is on its own building, and on a wall', () => {
  // A door is placed relative to the building's corner. Out of range, it
  // vanishes; in the middle, it opens onto the floor and the wall stays sealed.
  A.BUILDINGS.filter((b) => b.door && b.kind !== 'park').forEach((b) => {
    const [dx, dy] = b.door;
    assert(dx >= 0 && dy >= 0 && dx < b.w && dy < b.h,
      `${b.name}'s door is outside the building`);
    const onEdge = dx === 0 || dy === 0 || dx === b.w - 1 || dy === b.h - 1;
    assert(onEdge, `${b.name}'s door is in the middle of the floor, not in a wall`);
  });
});

test('no two buildings overlap', () => {
  const clashes = [];
  for (let i = 0; i < A.BUILDINGS.length; i++) {
    for (let j = i + 1; j < A.BUILDINGS.length; j++) {
      const a = A.BUILDINGS[i], b = A.BUILDINGS[j];
      if (a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h) {
        clashes.push(`${a.name} / ${b.name}`);
      }
    }
  }
  eq(clashes.join('; '), '', 'these buildings are standing on each other');
});

test('every avenue and street runs inside the map', () => {
  A.AVENUES.forEach((av) => {
    assert(av.name, 'an avenue has no name');
    assert(av.row >= 1 && av.row + 2 < A.MAP_H, `${av.name} runs off the map`);
  });
  A.STREETS.forEach((st) => {
    assert(st.name, 'a street has no name');
    assert(st.col >= 1 && st.col + 2 < A.MAP_W, `${st.name} runs off the map`);
  });
});

/* ----------------------------------------------------------- baking the city */
console.log('\nBAKING THE CITY');

const world = WORLDGEN.generate();

test('the generated grid is exactly the size the map declares', () => {
  eq(world.W, A.MAP_W);
  eq(world.H, A.MAP_H);
  eq(world.tiles.length, A.MAP_H, 'wrong number of rows');
  world.tiles.forEach((row, y) => eq(row.length, A.MAP_W, `row ${y} is the wrong width`));
});

test('every tile is a code the renderer knows', () => {
  const known = new Set(Object.values(T));
  for (let y = 0; y < world.H; y++) {
    for (let x = 0; x < world.W; x++) {
      assert(known.has(world.tiles[y][x]), `unknown tile ${world.tiles[y][x]} at ${x},${y}`);
    }
  }
});

test('generating twice gives the identical city', () => {
  // The generator takes no seed and must not reach for Math.random: the city is
  // a fixed place, and two players must be able to talk about the same corner.
  const again = WORLDGEN.generate();
  eq(JSON.stringify(again.tiles), JSON.stringify(world.tiles), 'the map changed between runs');
  eq(JSON.stringify(again.containers), JSON.stringify(world.containers), 'the loot moved');
});

test('the player does not spawn inside a wall, the water or a tree', () => {
  // The spawn is a hard-coded corner of Town Square Park. Move a building onto
  // it and the game starts with the player stuck; nothing else would say so.
  const { x, y } = world.spawn;
  assert(x >= 0 && y >= 0 && x < world.W && y < world.H, 'the spawn is off the map');
  assert(!WORLDGEN.isSolid(world.tiles, x, y),
    `the spawn at ${x},${y} is on tile ${world.tiles[y][x]}, which is solid`);
});

test('every building with a door has one you can walk through', () => {
  A.BUILDINGS.filter((b) => b.door && b.kind !== 'park').forEach((b) => {
    const dx = b.x + b.door[0], dy = b.y + b.door[1];
    eq(world.tiles[dy][dx], T.DOOR, `${b.name}'s doorway did not become a door tile`);
    assert(!WORLDGEN.isSolid(world.tiles, dx, dy), `${b.name}'s door is solid`);
  });
});

test('every loot container sits on a floor tile inside a building', () => {
  assert(world.containers.length > 0, 'the city has nothing to loot');
  world.containers.forEach((c) => {
    eq(world.tiles[c.y][c.x], T.FLOOR, `${c.name}'s container at ${c.x},${c.y} is not on a floor`);
    assert(c.loot >= 0 && c.loot <= 3, `${c.name} has an out-of-range loot richness: ${c.loot}`);
    eq(c.opened, false, 'a container starts already opened');
  });
});

test('every zombie spawn point is on the map', () => {
  assert(world.spawnPoints.length > 0, 'nothing can spawn');
  world.spawnPoints.forEach((p) => {
    assert(p.x >= 0 && p.y >= 0 && p.x < world.W && p.y < world.H,
      `a spawn point at ${p.x},${p.y} is off the map`);
  });
});

test('every label points somewhere on the map', () => {
  world.labels.forEach((l) => {
    assert(l.text, 'a label has no text');
    assert(l.tx >= -1 && l.tx <= world.W && l.ty >= -1 && l.ty <= world.H,
      `"${l.text}" is drawn off the map at ${l.tx},${l.ty}`);
  });
});

/* ------------------------------------------------------------- what is solid */
console.log('\nWHAT YOU CAN WALK THROUGH');

test('everything off the map is solid', () => {
  for (const [x, y] of [[-1, 0], [0, -1], [world.W, 0], [0, world.H], [-5, -5]]) {
    assert(WORLDGEN.isSolid(world.tiles, x, y), `${x},${y} is off the map but not solid`);
  }
});

test('walls, water and trees stop you; streets, floors and doors do not', () => {
  const grid = [[T.WALL, T.WATER, T.TREE], [T.STREET, T.SIDEWALK, T.FLOOR], [T.DOOR, T.GRASS, T.LOT]];
  for (let x = 0; x < 3; x++) assert(WORLDGEN.isSolid(grid, x, 0), `tile ${grid[0][x]} should block`);
  for (let y = 1; y < 3; y++) {
    for (let x = 0; x < 3; x++) assert(!WORLDGEN.isSolid(grid, x, y), `tile ${grid[y][x]} should not block`);
  }
});

/* -------------------------------------------------------------------- loot */
console.log('\nITEMS AND LOOT');

test('every item is complete enough to show and to use', () => {
  const kinds = new Set(['weapon', 'food', 'drink', 'med', 'misc']);
  for (const [id, item] of Object.entries(ITEMS)) {
    assert(item.name, `${id} has no name`);
    assert(kinds.has(item.type), `${id} has an unknown type: ${item.type}`);
    assert(item.icon, `${id} has no icon, so the inventory would show a blank`);
  }
});

test('every weapon can actually be swung', () => {
  for (const [id, item] of Object.entries(ITEMS)) {
    if (item.type !== 'weapon') continue;
    assert(item.dmg > 0, `${id} does no damage`);
    assert(item.range > 0, `${id} has no reach`);
    assert(item.speed > 0, `${id} never lands`);
    assert(item.durab > 0, `${id} is broken before it is picked up`);
  }
});

test('every loot table names items that exist', () => {
  // The check that earns this file. Rename an item and the table that drops it
  // keeps the old id; rollLoot then reads `undefined.stack` and the game throws
  // the first time someone opens that kind of building.
  const unknown = [];
  for (const kind of ['grocery', 'food', 'bar', 'gas', 'hospital', 'police',
                      'hardware', 'outdoor', 'mall', 'hotel', 'default']) {
    for (const [id] of LOOT.tableFor(kind)) {
      if (!ITEMS[id]) unknown.push(`${kind}:${id}`);
    }
  }
  eq(unknown.join(', '), '', 'loot tables naming items that do not exist');
});

test('every loot weight is a positive number', () => {
  for (const kind of ['grocery', 'police', 'default']) {
    for (const [id, weight] of LOOT.tableFor(kind)) {
      assert(typeof weight === 'number' && weight > 0, `${kind}:${id} has weight ${weight}`);
    }
  }
});

test('every building kind in the city has a loot table of its own', () => {
  // A kind with no table silently falls back to `default`, which is not wrong
  // but means a hospital loots like a bus shelter. Worth knowing about.
  const missing = [...new Set(A.BUILDINGS.map((b) => b.kind).filter(Boolean))]
    .filter((kind) => kind !== 'park' && LOOT.tableFor(kind) === LOOT.tableFor('nope-not-a-kind'));
  eq(missing.join(', '), '', 'building kinds falling back to the default loot table');
});

test('rolling loot yields real items in the right quantity', () => {
  for (const kind of ['grocery', 'police', 'hospital', 'default']) {
    for (let richness = 0; richness <= 3; richness++) {
      for (let i = 0; i < 40; i++) {
        const drops = LOOT.rollLoot(richness, kind);
        eq(drops.length, richness + 1, `${kind} at richness ${richness} dropped the wrong number`);
        drops.forEach((d) => {
          assert(ITEMS[d.id], `${kind} dropped an item that does not exist: ${d.id}`);
          assert(d.qty >= 1, `${d.id} came in a quantity of ${d.qty}`);
        });
      }
    }
  }
});

test('an unknown building kind falls back to the default table rather than failing', () => {
  const drops = LOOT.rollLoot(1, 'a kind nobody built');
  eq(drops.length, 2);
  drops.forEach((d) => assert(ITEMS[d.id], `fallback dropped a phantom item: ${d.id}`));
});

/* ------------------------------------------------------------------ result */
console.log('');
if (failures.length) {
  console.log('✗ ' + failures.length + ' failed, ' + passed + ' passed\n');
  failures.forEach((f) => console.log('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
