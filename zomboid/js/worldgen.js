/* =============================================================================
 *  ZOMBOID: ANCHORAGE  —  WORLD GENERATOR
 *  Bakes ANCHORAGE data into a tile grid + collision map + loot containers.
 * ========================================================================== */
(function (global) {
  'use strict';

  const A = global.ANCHORAGE;
  const T = A.T;

  function makeGrid(w, h, fill) {
    const g = new Array(h);
    for (let y = 0; y < h; y++) {
      g[y] = new Array(w).fill(fill);
    }
    return g;
  }

  function inBounds(x, y) {
    return x >= 0 && y >= 0 && x < A.MAP_W && y < A.MAP_H;
  }

  function generate() {
    const W = A.MAP_W, H = A.MAP_H;
    const tiles = makeGrid(W, H, T.GRASS);
    const labels = [];           // floating street/building labels
    const containers = [];       // lootable spots {x,y,loot,name,opened}
    const spawnPoints = [];

    // --- Cook Inlet water on the far west, Chugach foothills (trees) east ---
    for (let y = 0; y < H; y++) {
      for (let x = 0; x < 4; x++) tiles[y][x] = T.WATER;
      // ragged shoreline
      if ((y * 7) % 5 === 0 && inBounds(4, y)) tiles[y][4] = T.WATER;
    }
    // Ship Creek strip across the north
    for (let x = 4; x < W; x++) {
      tiles[2][x] = T.WATER;
      tiles[3][x] = T.WATER;
    }
    // Chugach treeline / greenbelt on the east edge
    for (let y = 0; y < H; y++) {
      for (let x = W - 6; x < W; x++) {
        if ((x * 3 + y * 5) % 4 !== 0) tiles[y][x] = T.TREE;
        else tiles[y][x] = T.GRASS;
      }
    }

    // --- Avenues (horizontal streets) ---
    A.AVENUES.forEach((av) => {
      const r = av.row;
      for (let x = 5; x < W - 6; x++) {
        if (inBounds(x, r - 1)) tiles[r - 1][x] = T.SIDEWALK;
        if (inBounds(x, r))     tiles[r][x]     = T.STREET;
        if (inBounds(x, r + 1)) tiles[r + 1][x] = T.STREET;
        if (inBounds(x, r + 2)) tiles[r + 2][x] = T.SIDEWALK;
      }
      labels.push({ text: av.name, tx: 8, ty: r, kind: 'street' });
      labels.push({ text: av.name, tx: Math.floor(W * 0.62), ty: r, kind: 'street' });
    });

    // Ship Creek railyard
    for (let x = 5; x < W - 6; x++) {
      tiles[8][x] = T.RAIL;
      tiles[9][x] = T.RAIL;
    }

    // --- Streets (vertical) ---
    A.STREETS.forEach((st) => {
      const c = st.col;
      for (let y = 5; y < H - 2; y++) {
        if (inBounds(c - 1, y)) tiles[y][c - 1] = T.SIDEWALK;
        if (inBounds(c, y))     tiles[y][c]     = T.STREET;
        if (inBounds(c + 1, y)) tiles[y][c + 1] = T.STREET;
        if (inBounds(c + 2, y)) tiles[y][c + 2] = T.SIDEWALK;
      }
      labels.push({ text: st.name, tx: c, ty: 12, kind: 'street', vertical: true });
      labels.push({ text: st.name, tx: c, ty: Math.floor(H * 0.55), kind: 'street', vertical: true });
    });

    // --- Buildings ---
    A.BUILDINGS.forEach((b) => {
      const isPark = b.kind === 'park';
      for (let yy = 0; yy < b.h; yy++) {
        for (let xx = 0; xx < b.w; xx++) {
          const gx = b.x + xx, gy = b.y + yy;
          if (!inBounds(gx, gy)) continue;
          const edge = xx === 0 || yy === 0 || xx === b.w - 1 || yy === b.h - 1;
          if (isPark) {
            tiles[gy][gx] = (xx + yy) % 3 === 0 ? T.TREE : T.GRASS;
          } else if (edge) {
            tiles[gy][gx] = T.WALL;
          } else {
            tiles[gy][gx] = T.FLOOR;
          }
        }
      }
      // door
      if (!isPark && b.door) {
        const dx = b.x + b.door[0], dy = b.y + b.door[1];
        if (inBounds(dx, dy)) tiles[dy][dx] = T.DOOR;
        // sidewalk apron + parking lot in front
        for (let k = 1; k <= 2; k++) {
          const ay = dy + k;
          if (inBounds(dx, ay) && tiles[ay][dx] === T.GRASS) tiles[ay][dx] = T.LOT;
        }
      }
      // loot containers scattered inside, count scales with loot richness
      const nContainers = b.loot + 1;
      for (let i = 0; i < nContainers; i++) {
        const cx = b.x + 1 + ((i * 3 + 1) % Math.max(1, b.w - 2));
        const cy = b.y + 1 + ((i * 2 + 1) % Math.max(1, b.h - 2));
        if (inBounds(cx, cy) && tiles[cy][cx] === T.FLOOR) {
          containers.push({ x: cx, y: cy, loot: b.loot, name: b.name, opened: false });
        }
      }
      labels.push({ text: b.name, tx: b.x + b.w / 2, ty: b.y - 0.4, kind: 'building' });
    });

    // --- Player spawn: Town Square Park area on 5th & C ---
    const spawn = { x: 49, y: 36 };

    // --- Zombie spawn points: doorways + intersections ---
    A.BUILDINGS.forEach((b) => {
      if (b.door) spawnPoints.push({ x: b.x + b.door[0], y: b.y + b.door[1] + 1 });
    });
    A.AVENUES.forEach((av) => A.STREETS.forEach((st) => {
      spawnPoints.push({ x: st.col, y: av.row });
    }));

    return { tiles, labels, containers, spawnPoints, spawn, W, H };
  }

  function isSolid(tiles, x, y) {
    if (x < 0 || y < 0 || x >= A.MAP_W || y >= A.MAP_H) return true;
    const t = tiles[y][x];
    return t === T.WALL || t === T.WATER || t === T.TREE;
  }

  global.WORLDGEN = { generate, isSolid };
})(typeof window !== 'undefined' ? window : this);
