/* =============================================================================
 *  NEON CELLS  —  level generation
 *
 *  A level is a grid of rooms carved out of solid rock, wired together by
 *  corridors and one-way platform shafts, then populated. It is built from the
 *  run's seed alone, so the same seed always builds the same level.
 *
 *  The important promise this file keeps is REACHABILITY. A generator that can
 *  strand the exit behind a wall is a generator that ends runs unfairly, so
 *  every level is walked with a conservative model of what the player can
 *  actually do (walk, fall, jump about five tiles up and six across) and:
 *
 *    · the exit must be reachable from where the player spawns, or the level
 *      is thrown away and rebuilt;
 *    · every enemy, chest, scroll and shop is placed *only* on tiles the walk
 *      already proved reachable — so nothing is ever locked away.
 *
 *  cells/tests/cells-logic.test.js checks both of those over many seeds.
 * ========================================================================== */
(function (global) {
  'use strict';

  const RNG = global.CELLS_RNG || require('./rng.js');
  const CONTENT = global.CELLS_CONTENT || require('./content.js');

  /* Tile kinds. SOLID blocks everything; PLATFORM is one-way (you can jump up
   * through it and drop down with Down+Jump); SPIKE hurts but holds you up. */
  const T = { EMPTY: 0, SOLID: 1, PLATFORM: 2, SPIKE: 3 };

  const TILE = 16;     // px per tile
  const RW = 26;       // room cell width, in tiles
  const RH = 15;       // room cell height, in tiles

  /* What the reachability walk assumes the player can do. Deliberately meaner
   * than the real movement code, so anything it says is reachable really is. */
  const JUMP_UP = 4;       // tiles of height gained from a standing jump
  const JUMP_ACROSS = 5;   // tiles of horizontal travel in that jump
  const FALL_ACROSS = 5;   // tiles you can drift while falling
  const FALL_MAX = 26;     // how far down a fall is still survivable terrain

  /* ------------------------------------------------------------------ grid */
  function makeGrid(w, h, fill) {
    const tiles = new Uint8Array(w * h);
    if (fill) tiles.fill(fill);
    return { w: w, h: h, tiles: tiles };
  }

  function at(level, x, y) {
    if (x < 0 || y < 0 || x >= level.w || y >= level.h) return T.SOLID;
    return level.tiles[y * level.w + x];
  }

  function put(level, x, y, v) {
    if (x < 0 || y < 0 || x >= level.w || y >= level.h) return;
    level.tiles[y * level.w + x] = v;
  }

  function isBlocking(v) {
    return v === T.SOLID;
  }

  function isFloor(v) {
    return v === T.SOLID || v === T.PLATFORM || v === T.SPIKE;
  }

  function carveRect(level, x0, y0, x1, y1, v) {
    for (let y = y0; y <= y1; y++) {
      for (let x = x0; x <= x1; x++) put(level, x, y, v);
    }
  }

  /* ------------------------------------------------------- where you can be
   * A tile is standable if you fit in it (it and the tile above are open) and
   * something holds you up from below.
   */
  function standable(level, x, y) {
    return (
      at(level, x, y) === T.EMPTY &&
      at(level, x, y - 1) === T.EMPTY &&
      isFloor(at(level, x, y + 1))
    );
  }

  function openAt(level, x, y) {
    const v = at(level, x, y);
    return v === T.EMPTY || v === T.PLATFORM;
  }

  /* Body-height clearance, i.e. "the player fits here mid-move". */
  function clear(level, x, y) {
    return openAt(level, x, y) && openAt(level, x, y - 1);
  }

  function key(x, y) {
    return x + ',' + y;
  }

  /* ---------------------------------------------------------- reachability
   * Breadth-first over standable tiles, remembering how each one was reached so
   * the same walk can answer both "where can the player get to" and "by what
   * route" — the second is what the tests drive the real character controller
   * along, to prove the route is not merely theoretical.
   */
  function walkFrom(level, sx, sy) {
    const from = new Map();
    const start = snapToFloor(level, sx, sy);
    if (!start) return { start: null, from: from };

    const queue = [{ x: start.x, y: start.y, via: 'start' }];
    from.set(key(start.x, start.y), null);

    for (let head = 0; head < queue.length; head++) {
      const cur = queue[head];
      for (const next of neighbours(level, cur.x, cur.y)) {
        const k = key(next.x, next.y);
        if (from.has(k)) continue;
        from.set(k, { tile: cur, via: next.via });
        queue.push(next);
      }
    }
    return { start: start, from: from };
  }

  function reachableFrom(level, sx, sy) {
    return new Set(walkFrom(level, sx, sy).from.keys());
  }

  /* The shortest route the walk found, as a list of tiles, or null if there
   * is none. */
  function pathTo(level, sx, sy, tx, ty) {
    const walk = walkFrom(level, sx, sy);
    if (!walk.start) return null;
    const target = snapToFloor(level, tx, ty);
    if (!target) return null;

    if (!walk.from.has(key(target.x, target.y))) return null;

    const path = [];
    let cur = { x: target.x, y: target.y, via: 'arrive' };
    while (cur) {
      const step = walk.from.get(key(cur.x, cur.y));
      path.unshift({ x: cur.x, y: cur.y, via: step ? step.via : 'start' });
      cur = step ? step.tile : null;
    }
    return path;
  }

  /* Drop a point onto the nearest standable tile at or below it. */
  function snapToFloor(level, x, y) {
    for (let yy = y; yy < Math.min(level.h, y + FALL_MAX); yy++) {
      if (standable(level, x, yy)) return { x: x, y: yy };
    }
    for (let yy = y; yy >= 0; yy--) {
      if (standable(level, x, yy)) return { x: x, y: yy };
    }
    return null;
  }

  /* Each neighbour carries `via`: how the player gets there from here — 'walk',
   * 'fall' or 'jump'. A route is then something that can actually be executed,
   * not just a list of tiles that happen to be connected. */
  function neighbours(level, x, y) {
    const out = [];

    /* walk, and step one tile up or down */
    for (const dx of [-1, 1]) {
      for (const dy of [0, -1, 1]) {
        if (clear(level, x + dx, y + dy) && standable(level, x + dx, y + dy)) {
          out.push({ x: x + dx, y: y + dy, via: 'walk' });
        }
      }
    }

    /* walk off an edge and fall */
    for (const dir of [-1, 1]) {
      if (!clear(level, x + dir, y)) continue;
      for (let dx = 1; dx <= FALL_ACROSS; dx++) {
        const cx = x + dir * dx;
        if (!clear(level, cx, y)) break;
        for (let dy = 1; dy <= FALL_MAX; dy++) {
          const cy = y + dy;
          if (isBlocking(at(level, cx, cy))) break;
          if (standable(level, cx, cy)) {
            out.push({ x: cx, y: cy, via: 'fall' });
            break;
          }
        }
      }
    }

    /* jump: straight up through platforms, or up and across */
    for (let dy = 1; dy <= JUMP_UP; dy++) {
      if (isBlocking(at(level, x, y - dy - 1))) break; // head hits rock
      if (standable(level, x, y - dy)) out.push({ x: x, y: y - dy, via: 'jump' });

      for (const dir of [-1, 1]) {
        for (let dx = 1; dx <= JUMP_ACROSS; dx++) {
          const cx = x + dir * dx;
          const cy = y - dy;
          if (!clear(level, cx, cy)) break;
          if (standable(level, cx, cy)) out.push({ x: cx, y: cy, via: 'jump' });
        }
      }
    }

    return out;
  }

  /* --------------------------------------------------------- the room graph
   * A depth-first maze over the cols x rows grid gives a tree that touches
   * every room; a few extra links turn dead ends into loops, which is what
   * makes a level feel like a place rather than a corridor.
   */
  function roomGraph(rng, cols, rows) {
    const visited = new Set();
    const links = [];
    const stack = [{ x: rng.int(0, cols - 1), y: rows - 1 }];
    visited.add(key(stack[0].x, stack[0].y));
    const start = stack[0];

    while (stack.length) {
      const cur = stack[stack.length - 1];
      const options = rng.shuffle([
        { x: cur.x - 1, y: cur.y }, { x: cur.x + 1, y: cur.y },
        { x: cur.x, y: cur.y - 1 }, { x: cur.x, y: cur.y + 1 }
      ]).filter(function (c) {
        return c.x >= 0 && c.y >= 0 && c.x < cols && c.y < rows && !visited.has(key(c.x, c.y));
      });

      if (!options.length) {
        stack.pop();
        continue;
      }
      const next = options[0];
      visited.add(key(next.x, next.y));
      links.push({ a: cur, b: next });
      stack.push(next);
    }

    /* extra loops */
    const extra = Math.max(1, Math.round(cols * rows * 0.25));
    for (let i = 0; i < extra; i++) {
      const a = { x: rng.int(0, cols - 1), y: rng.int(0, rows - 1) };
      const b = rng.chance(0.5) ? { x: a.x + 1, y: a.y } : { x: a.x, y: a.y + 1 };
      if (b.x >= cols || b.y >= rows) continue;
      const exists = links.some(function (l) {
        return (l.a.x === a.x && l.a.y === a.y && l.b.x === b.x && l.b.y === b.y) ||
               (l.b.x === a.x && l.b.y === a.y && l.a.x === b.x && l.a.y === b.y);
      });
      if (!exists) links.push({ a: a, b: b });
    }

    return { start: start, links: links };
  }

  /* Room distance in links, so the exit can be put as far from the start as
   * the level allows — the long way round is the interesting way. */
  function roomDistances(cols, rows, links, start) {
    const adj = new Map();
    function add(a, b) {
      const k = key(a.x, a.y);
      if (!adj.has(k)) adj.set(k, []);
      adj.get(k).push(b);
    }
    for (const l of links) { add(l.a, l.b); add(l.b, l.a); }

    const dist = new Map();
    dist.set(key(start.x, start.y), 0);
    let frontier = [start];
    while (frontier.length) {
      const next = [];
      for (const cur of frontier) {
        const d = dist.get(key(cur.x, cur.y));
        for (const n of adj.get(key(cur.x, cur.y)) || []) {
          if (dist.has(key(n.x, n.y))) continue;
          dist.set(key(n.x, n.y), d + 1);
          next.push(n);
        }
      }
      frontier = next;
    }
    return dist;
  }

  /* ------------------------------------------------------------- carving */
  function floorRowOf(cy) {
    return cy * RH + RH - 1;   // the solid row you stand on
  }

  function carveRoom(level, rng, cx, cy) {
    const ox = cx * RW;
    const oy = cy * RH;
    carveRect(level, ox + 1, oy + 2, ox + RW - 2, oy + RH - 2, T.EMPTY);
    return {
      cx: cx, cy: cy,
      x0: ox + 1, y0: oy + 2, x1: ox + RW - 2, y1: oy + RH - 2,
      floorY: oy + RH - 2
    };
  }

  function carveHorizontalLink(level, a, b) {
    const left = a.cx < b.cx ? a : b;
    const ox = left.cx * RW;
    const floorY = floorRowOf(left.cy) - 1;
    carveRect(level, ox + RW - 2, floorY - 3, ox + RW + 1, floorY, T.EMPTY);
  }

  function carveVerticalLink(level, rng, upper, lower) {
    const ox = upper.cx * RW;
    const sx = ox + rng.int(3, RW - 9);
    const top = floorRowOf(upper.cy) - 1;
    const bottom = lower.cy * RH + 2;
    carveRect(level, sx, top, sx + 5, bottom, T.EMPTY);

    /* A ladder of one-way platforms every three tiles, running unbroken from
     * the lower room's floor up through the shaft and into the upper room —
     * three tiles is well inside a standing jump, so the shaft is always
     * climbable in both directions (drop back down with Down + Jump). */
    const lowFloor = floorRowOf(lower.cy) - 1;
    for (let y = lowFloor - 3; y >= top - 3; y -= 3) {
      carveRect(level, sx + 1, y, sx + 4, y, T.PLATFORM);
    }
  }

  /* Interior furniture: ledges to fight on, and the occasional spike pit. */
  function furnishRoom(level, rng, room, biome, isStartRoom) {
    const ledges = rng.int(2, 5);
    for (let i = 0; i < ledges; i++) {
      const w = rng.int(3, 8);
      const x = rng.int(room.x0 + 1, Math.max(room.x0 + 1, room.x1 - w - 1));
      const y = rng.int(room.y0 + 2, room.floorY - 3);
      let blocked = false;
      for (let xx = x; xx < x + w; xx++) {
        if (at(level, xx, y) !== T.EMPTY || at(level, xx, y - 1) !== T.EMPTY) blocked = true;
      }
      if (blocked) continue;
      carveRect(level, x, y, x + w - 1, y, T.PLATFORM);
    }

    if (!isStartRoom && biome.spikes && rng.chance(biome.spikes * 6)) {
      const w = rng.int(2, 5);
      const x = rng.int(room.x0 + 2, Math.max(room.x0 + 2, room.x1 - w - 2));
      carveRect(level, x, room.floorY + 1, x + w - 1, room.floorY + 1, T.SPIKE);
    }
  }

  /* ------------------------------------------------------------- populating */
  function standableTilesIn(level, room, reach) {
    const out = [];
    for (let y = room.y0; y <= room.floorY; y++) {
      for (let x = room.x0; x <= room.x1; x++) {
        if (!standable(level, x, y)) continue;
        if (at(level, x, y + 1) === T.SPIKE) continue;
        if (reach && !reach.has(key(x, y))) continue;
        out.push({ x: x, y: y });
      }
    }
    return out;
  }

  function toPx(tile) {
    return { x: tile.x * TILE + TILE / 2, y: tile.y * TILE + TILE };
  }

  /* --------------------------------------------------------------- biomes */
  function generate(biomeId, seed, opts) {
    const options = opts || {};
    const biome = CONTENT.BIOME[biomeId];
    if (!biome) throw new Error('unknown biome: ' + biomeId);
    if (biome.boss) return generateArena(biome, seed, options);

    /* Up to a handful of attempts; keep the first level whose exit is
     * reachable, which in practice is nearly always the first one. */
    let best = null;
    for (let attempt = 0; attempt < 12; attempt++) {
      const level = buildLevel(biome, seed + attempt * 7919, options);
      if (level.ok) return level;
      if (!best || level.reach.size > best.reach.size) best = level;
    }
    return best;
  }

  function buildLevel(biome, seed, options) {
    const rng = RNG.Rng(seed);
    const cols = biome.cols;
    const rows = biome.rows;
    const level = makeGrid(cols * RW, rows * RH, T.SOLID);
    level.tileSize = TILE;
    level.biome = biome;
    level.depth = biome.depth;
    level.seed = seed;
    level.objects = [];
    level.enemies = [];
    level.torches = [];

    const graph = roomGraph(rng, cols, rows);
    const rooms = [];
    const roomAt = new Map();
    for (let cy = 0; cy < rows; cy++) {
      for (let cx = 0; cx < cols; cx++) {
        const room = carveRoom(level, rng, cx, cy);
        rooms.push(room);
        roomAt.set(key(cx, cy), room);
      }
    }

    for (const link of graph.links) {
      const a = roomAt.get(key(link.a.x, link.a.y));
      const b = roomAt.get(key(link.b.x, link.b.y));
      if (a.cy === b.cy) carveHorizontalLink(level, a, b);
      else {
        const upper = a.cy < b.cy ? a : b;
        const lower = a.cy < b.cy ? b : a;
        carveVerticalLink(level, rng, upper, lower);
      }
    }

    const startRoom = roomAt.get(key(graph.start.x, graph.start.y));
    for (const room of rooms) furnishRoom(level, rng, room, biome, room === startRoom);

    /* Where the player lands: a tile that is standable and not over spikes,
     * as close as the room allows to its left-hand end. */
    const footing = standableTilesIn(level, startRoom, null);
    const want = { x: startRoom.x0 + 3, y: startRoom.floorY };
    let snapped = snapToFloor(level, want.x, want.y) || want;
    if (footing.length) {
      snapped = footing.reduce(function (best, tile) {
        const d = Math.abs(tile.x - want.x) + Math.abs(tile.y - want.y) * 2;
        const bd = Math.abs(best.x - want.x) + Math.abs(best.y - want.y) * 2;
        return d < bd ? tile : best;
      }, footing[0]);
    }
    level.spawnTile = snapped;
    level.spawn = toPx(snapped);

    const dist = roomDistances(cols, rows, graph.links, graph.start);
    const ranked = rooms.slice().sort(function (a, b) {
      return (dist.get(key(b.cx, b.cy)) || 0) - (dist.get(key(a.cx, a.cy)) || 0);
    });
    const exitRoom = ranked[0];

    level.reach = reachableFrom(level, snapped.x, snapped.y);
    level.rooms = rooms;

    /* The exit door sits on a reachable floor tile in the farthest room. */
    const exitSpots = standableTilesIn(level, exitRoom, level.reach);
    if (!exitSpots.length) {
      level.ok = false;
      return level;
    }
    const exitTile = exitSpots[exitSpots.length - 1];
    const exitPx = toPx(exitTile);
    level.exit = { x: exitPx.x, y: exitPx.y, w: 22, h: 34 };
    level.exitRoom = exitRoom;
    level.ok = true;

    /* The Collector waits by the exit, the way it does at the end of a
     * Dead Cells level: cells spent there are kept for every future run. */
    const collectorSpot = exitSpots.length > 6 ? exitSpots[exitSpots.length - 6] : exitSpots[0];
    level.objects.push({ type: 'collector', tile: collectorSpot, pos: toPx(collectorSpot), used: false });

    populate(level, rng, biome, rooms, startRoom, exitRoom, options);
    return level;
  }

  function populate(level, rng, biome, rooms, startRoom, exitRoom, options) {
    const depth = biome.depth;
    const bossCells = options.bossCells || 0;
    const hpScale = (1 + 0.26 * (depth - 1)) * (1 + 0.35 * bossCells);
    const dmgScale = (1 + 0.17 * (depth - 1)) * (1 + 0.25 * bossCells);

    /* room "slots": every reachable standing tile, room by room */
    const slots = new Map();
    for (const room of rooms) slots.set(room, standableTilesIn(level, room, level.reach));

    function take(room, away) {
      const list = slots.get(room) || [];
      const usable = list.filter(function (t) {
        if (!away) return true;
        return Math.abs(t.x - away.x) + Math.abs(t.y - away.y) > 5;
      });
      if (!usable.length) return null;
      const pick = rng.pick(usable);
      slots.set(room, list.filter(function (t) { return t !== pick; }));
      return pick;
    }

    /* ---- enemies: none in the room you spawn in, so you get a moment */
    const pool = biome.pool;
    for (const room of rooms) {
      if (room === startRoom) continue;
      let count = Math.round(rng.float(1.4, 3.4) * biome.density);
      if (room === exitRoom) count = Math.max(1, count - 1);
      for (let i = 0; i < count; i++) {
        const tile = take(room, null);
        if (!tile) break;
        const id = rng.pick(pool);
        const base = CONTENT.ENEMY[id];
        const elite = rng.chance(0.07 + 0.01 * depth);
        level.enemies.push({
          id: id,
          tile: tile,
          pos: toPx(tile),
          elite: elite,
          hp: Math.round(base.hp * hpScale * (elite ? 2.4 : 1)),
          dmg: Math.round(base.dmg * dmgScale * (elite ? 1.5 : 1))
        });
      }
    }

    /* One elite carries the key to the vault, if the level has one. */
    const vaultRoom = rooms.filter(function (r) { return r !== startRoom && r !== exitRoom; });
    if (vaultRoom.length && level.enemies.length) {
      const carriers = level.enemies.filter(function (e) { return e.elite; });
      const carrier = carriers.length ? rng.pick(carriers) : rng.pick(level.enemies);
      carrier.hasKey = true;
      const room = rng.pick(vaultRoom);
      const tile = take(room, null);
      if (tile) {
        level.objects.push({ type: 'vault', tile: tile, pos: toPx(tile), locked: true, opened: false });
      }
    }

    /* ---- scrolls: the stat upgrades the whole run is built out of */
    const scrollRooms = rng.shuffle(rooms.filter(function (r) { return r !== startRoom; }));
    for (let i = 0; i < (biome.scrolls || 0); i++) {
      const room = scrollRooms[i % scrollRooms.length];
      const tile = take(room, null);
      if (!tile) continue;
      level.objects.push({
        type: 'scroll',
        dual: rng.chance(0.45),
        tile: tile, pos: toPx(tile), taken: false
      });
    }

    /* ---- chests, cursed chests, food, shop */
    const chestRooms = rng.shuffle(rooms.filter(function (r) { return r !== startRoom; }));
    for (let i = 0; i < (biome.chests || 0); i++) {
      const room = chestRooms[i % chestRooms.length];
      const tile = take(room, null);
      if (!tile) continue;
      const cursed = rng.chance(0.18);
      level.objects.push({
        type: cursed ? 'cursed_chest' : 'chest',
        tile: tile, pos: toPx(tile), opened: false
      });
    }

    for (let i = 0; i < 2; i++) {
      const room = rng.pick(rooms);
      const tile = take(room, null);
      if (!tile) continue;
      level.objects.push({ type: 'food', tile: tile, pos: toPx(tile), taken: false });
    }

    if (biome.shop) {
      const room = rng.pick(rooms.filter(function (r) { return r !== startRoom && r !== exitRoom; }) || rooms);
      const tile = take(room, null);
      if (tile) level.objects.push({ type: 'shop', tile: tile, pos: toPx(tile), stock: null });
    }

    /* ---- the timed door: a vault that only opens if you were quick */
    const doorRooms = rooms.filter(function (r) { return r !== startRoom; });
    if (doorRooms.length) {
      const room = rng.pick(doorRooms);
      const tile = take(room, null);
      if (tile) {
        level.objects.push({ type: 'timed_chest', tile: tile, pos: toPx(tile), opened: false });
      }
    }
    level.timeLimit = 70 + 25 * (biome.cols * biome.rows);

    /* ---- torches: bracketed to rock, so they read as sconces rather than
     * lights hanging in mid-air */
    for (const room of rooms) {
      const wanted = rng.int(2, 4);
      let placed = 0;
      for (let tries = 0; tries < 40 && placed < wanted; tries++) {
        const x = rng.int(room.x0, room.x1);
        const y = rng.int(room.y0, room.floorY - 1);
        if (at(level, x, y) !== T.EMPTY) continue;
        const onWall =
          at(level, x - 1, y) === T.SOLID || at(level, x + 1, y) === T.SOLID ||
          at(level, x, y + 1) === T.SOLID || at(level, x, y - 1) === T.SOLID;
        if (!onWall) continue;
        level.torches.push({ x: x * TILE + TILE / 2, y: y * TILE + TILE / 2, seed: rng.next() * 6.28 });
        placed++;
      }
    }
  }

  /* ------------------------------------------------------------ boss arenas
   * One wide room, a floor you can always reach, and two ledges so the fight
   * has somewhere to go.
   */
  function generateArena(biome, seed, options) {
    const rng = RNG.Rng(seed);
    const w = 50;
    const h = 17;
    const level = makeGrid(w, h, T.SOLID);
    level.tileSize = TILE;
    level.biome = biome;
    level.depth = biome.depth;
    level.seed = seed;
    level.objects = [];
    level.enemies = [];
    level.torches = [];
    level.isArena = true;

    carveRect(level, 2, 3, w - 3, h - 3, T.EMPTY);
    carveRect(level, 6, h - 7, 13, h - 7, T.PLATFORM);
    carveRect(level, w - 14, h - 7, w - 7, h - 7, T.PLATFORM);
    carveRect(level, Math.floor(w / 2) - 5, h - 11, Math.floor(w / 2) + 4, h - 11, T.PLATFORM);
    carveRect(level, 3, h - 11, 8, h - 11, T.PLATFORM);
    carveRect(level, w - 9, h - 11, w - 4, h - 11, T.PLATFORM);

    const floorY = h - 3;
    const spawn = { x: 5, y: floorY };
    level.spawnTile = spawn;
    level.spawn = toPx(spawn);
    level.reach = reachableFrom(level, spawn.x, spawn.y);
    level.rooms = [{ cx: 0, cy: 0, x0: 2, y0: 3, x1: w - 3, y1: floorY, floorY: floorY }];

    const boss = CONTENT.BOSS[biome.boss];
    const bossCells = options.bossCells || 0;
    level.boss = {
      id: boss.id,
      pos: { x: (w - 12) * TILE, y: floorY * TILE + TILE },
      hp: Math.round(boss.hp * (1 + 0.4 * bossCells)),
      dmg: Math.round(boss.dmg * (1 + 0.25 * bossCells))
    };

    /* A door home, revealed once the boss is down. */
    const exitTile = { x: w - 5, y: floorY };
    const exitPx = toPx(exitTile);
    level.exit = { x: exitPx.x, y: exitPx.y, w: 22, h: 34, locked: true };
    level.exitRoom = level.rooms[0];
    level.timeLimit = 9999;
    level.ok = true;

    /* braziers along the back wall of the arena */
    for (let i = 0; i < 7; i++) {
      level.torches.push({
        x: (5 + i * 7) * TILE, y: (h - 3) * TILE - 2, seed: rng.next() * 6.28
      });
    }
    return level;
  }

  const API = {
    T: T,
    TILE: TILE,
    RW: RW,
    RH: RH,
    generate: generate,
    at: at,
    put: put,
    standable: standable,
    isFloor: isFloor,
    isBlocking: isBlocking,
    reachableFrom: reachableFrom,
    pathTo: pathTo,
    snapToFloor: snapToFloor,
    key: key
  };

  if (typeof module === 'object' && module.exports) module.exports = API;
  global.CELLS_LEVELGEN = API;
})(typeof globalThis !== 'undefined' ? globalThis : this);
