/* =============================================================================
 *  ZOMBOID: ANCHORAGE
 *  A Project Zomboid–style survival game rendered as a 1990s SEGA arcade title,
 *  drenched in neon. Set in a tile replica of downtown/midtown Anchorage, AK.
 *
 *  Controls:  WASD / Arrows move · Mouse aim · LMB / Space attack
 *             E loot/interact · 1-8 use inventory slot · R reload · M map
 *             Tab inventory · F flashlight · P pause · L mute
 * ========================================================================== */
(function (global) {
  'use strict';

  const A = global.ANCHORAGE;
  const T = A.T;
  const WG = global.WORLDGEN;
  const ITEMS = global.ITEMS;
  const LOOT = global.LOOT;
  const AUDIO = global.AUDIO;

  // ---------- Neon palette (SEGA-ish) ----------
  const NEON = {
    pink:   '#ff2d95',
    cyan:   '#05d9e8',
    purple: '#9d00ff',
    green:  '#39ff14',
    yellow: '#ffe600',
    orange: '#ff7b00',
    blue:   '#2d6bff',
    red:    '#ff1744',
  };

  const TILE = 28;                 // px per tile at zoom 1
  let canvas, ctx2d, W, H;
  let last = 0, acc = 0;
  const STEP = 1 / 60;

  // ---------- Game state machine ----------
  const STATE = { BOOT: 'BOOT', TITLE: 'TITLE', PLAY: 'PLAY', PAUSE: 'PAUSE', DEAD: 'DEAD' };
  let state = STATE.BOOT;
  let bootT = 0;

  // ---------- World ----------
  let world = null;
  let cam = { x: 0, y: 0, zoom: 1 };

  // ---------- Input ----------
  const keys = {};
  const mouse = { x: 0, y: 0, down: false, wx: 0, wy: 0 };

  // ---------- Entities ----------
  let player = null;
  let zombies = [];
  let bullets = [];
  let corpses = [];
  let floatTexts = [];
  let particles = [];

  // ---------- Meta ----------
  let dayTime = 8 * 60;            // minutes; start 08:00
  const DAY_LEN = 24 * 60;
  let dayCount = 1;
  let timeScale = 28;             // game-minutes per real second
  let kills = 0;
  let showInventory = false;
  let showMap = false;
  let messages = [];
  let waveTimer = 0;

  // =====================================================================
  //  INITIALISATION
  // =====================================================================
  function init() {
    canvas = document.getElementById('game');
    ctx2d = canvas.getContext('2d');
    resize();
    global.addEventListener('resize', resize);

    document.addEventListener('keydown', onKeyDown);
    document.addEventListener('keyup', (e) => { keys[e.key.toLowerCase()] = false; });
    canvas.addEventListener('mousemove', onMouseMove);
    canvas.addEventListener('mousedown', (e) => { mouse.down = true; onClick(e); });
    canvas.addEventListener('mouseup', () => { mouse.down = false; });
    canvas.addEventListener('contextmenu', (e) => e.preventDefault());

    AUDIO.init();
    requestAnimationFrame(loop);
  }

  function resize() {
    W = canvas.width = global.innerWidth;
    H = canvas.height = global.innerHeight;
  }

  function startNewGame() {
    world = WG.generate();
    player = {
      x: world.spawn.x + 0.5, y: world.spawn.y + 0.5,
      dir: 0, speed: 4.6, radius: 0.35,
      health: 100, hunger: 0, thirst: 0, fatigue: 0, mood: 70,
      infection: 0, infected: false,
      attackCd: 0, attackImmune: 0, inv: [], slots: 8,
      weapon: 'fists', flashlight: false,
      hurtFlash: 0, dead: false,
    };
    // starting kit
    addItem('branch', 1);
    addItem('water', 1);
    addItem('granola', 1);
    addItem('bandage', 2);
    addItem('map', 1);
    player.weapon = 'branch';

    zombies = []; bullets = []; corpses = []; floatTexts = []; particles = [];
    dayTime = 8 * 60; dayCount = 1; kills = 0; waveTimer = 0;
    messages = [];
    pushMsg('Welcome to ANCHORAGE. Survive.', NEON.cyan);
    pushMsg('The dead walk on 5th Avenue...', NEON.pink);

    // initial horde near downtown
    for (let i = 0; i < 40; i++) spawnZombie(true);
    state = STATE.PLAY;
    AUDIO.startMusic();
  }

  // =====================================================================
  //  INPUT HANDLERS
  // =====================================================================
  function onKeyDown(e) {
    const k = e.key.toLowerCase();
    keys[k] = true;
    AUDIO.resume();

    if (state === STATE.BOOT) { state = STATE.TITLE; return; }
    if (state === STATE.TITLE) {
      if (k === 'enter' || k === ' ') { AUDIO.SFX.start(); startNewGame(); }
      return;
    }
    if (state === STATE.DEAD) {
      if (k === 'enter' || k === ' ') { state = STATE.TITLE; }
      return;
    }

    if (k === 'tab') { e.preventDefault(); showInventory = !showInventory; AUDIO.SFX.select(); }
    if (k === 'm') { showMap = !showMap; AUDIO.SFX.select(); }
    if (k === 'p') { state = state === STATE.PAUSE ? STATE.PLAY : STATE.PAUSE; }
    if (k === 'f') { player.flashlight = !player.flashlight; AUDIO.SFX.select(); }
    if (k === 'l') { AUDIO.toggle(); }
    if (k === 'e') tryInteract();
    if (k === 'r') reload();
    if (k >= '1' && k <= '8') useSlot(parseInt(k, 10) - 1);
    if (k === ' ') { e.preventDefault(); attack(); }
  }

  function onMouseMove(e) {
    const r = canvas.getBoundingClientRect();
    mouse.x = e.clientX - r.left;
    mouse.y = e.clientY - r.top;
  }

  function onClick(e) {
    AUDIO.resume();
    if (state === STATE.BOOT) { state = STATE.TITLE; return; }
    if (state === STATE.TITLE) { AUDIO.SFX.start(); startNewGame(); return; }
    if (state === STATE.DEAD) { state = STATE.TITLE; return; }
    if (state === STATE.PLAY) attack();
  }

  // =====================================================================
  //  INVENTORY
  // =====================================================================
  function addItem(id, qty) {
    qty = qty || 1;
    const base = ITEMS[id];
    if (base.stack) {
      const ex = player.inv.find((s) => s.id === id);
      if (ex) { ex.qty += qty; return true; }
    }
    if (player.inv.length >= player.slots) {
      pushMsg('Inventory full!', NEON.red);
      return false;
    }
    const entry = { id, qty };
    if (base.type === 'weapon') entry.durab = base.durab;
    player.inv.push(entry);
    return true;
  }

  function removeOne(idx) {
    const s = player.inv[idx];
    if (!s) return;
    s.qty--;
    if (s.qty <= 0) player.inv.splice(idx, 1);
  }

  function useSlot(idx) {
    const s = player.inv[idx];
    if (!s) return;
    const base = ITEMS[s.id];
    if (base.type === 'weapon') {
      player.weapon = s.id;
      pushMsg('Equipped ' + base.name, NEON.yellow);
      AUDIO.SFX.select();
      return;
    }
    if (base.type === 'food') {
      player.hunger = Math.max(0, player.hunger - (base.hunger || 0));
      if (base.mood) player.mood = clamp(player.mood + base.mood, 0, 100);
      AUDIO.SFX.eat(); pushMsg('Ate ' + base.name, NEON.green); removeOne(idx); return;
    }
    if (base.type === 'drink') {
      player.thirst = Math.max(0, player.thirst - (base.thirst || 0));
      if (base.fatigue) player.fatigue = Math.max(0, player.fatigue + base.fatigue);
      if (base.mood) player.mood = clamp(player.mood + base.mood, 0, 100);
      AUDIO.SFX.drink(); pushMsg('Drank ' + base.name, NEON.cyan); removeOne(idx); return;
    }
    if (base.type === 'med') {
      player.health = clamp(player.health + (base.heal || 0), 0, 100);
      if (base.cureInfection && player.infected) {
        player.infected = false; player.infection = 0;
        pushMsg('Infection cured!', NEON.green);
      }
      AUDIO.SFX.pickup(); pushMsg('Used ' + base.name, NEON.green); removeOne(idx); return;
    }
    if (s.id === 'flashlight') { player.flashlight = !player.flashlight; AUDIO.SFX.select(); }
    if (s.id === 'map') { showMap = !showMap; AUDIO.SFX.select(); }
  }

  function reload() {
    const base = ITEMS[player.weapon];
    if (!base || !base.ranged) return;
    const ammo = player.inv.find((s) => s.id === base.ammo);
    if (ammo) { pushMsg('Reloaded ' + base.name, NEON.yellow); AUDIO.SFX.select(); }
    else pushMsg('No ammo for ' + base.name, NEON.red);
  }

  // =====================================================================
  //  INTERACTION / LOOTING
  // =====================================================================
  function tryInteract() {
    const px = Math.floor(player.x), py = Math.floor(player.y);
    let best = null, bd = 2.2;
    for (const c of world.containers) {
      if (c.opened) continue;
      const d = Math.hypot(c.x + 0.5 - player.x, c.y + 0.5 - player.y);
      if (d < bd) { bd = d; best = c; }
    }
    if (best) {
      best.opened = true;
      const kind = (A.BUILDINGS.find((b) => b.name === best.name) || {}).kind || 'default';
      const drops = LOOT.rollLoot(best.loot, kind);
      AUDIO.SFX.open();
      if (drops.length === 0) { pushMsg('Empty...', '#888'); return; }
      drops.forEach((d) => {
        if (addItem(d.id, d.qty)) {
          floatText(player.x, player.y - 0.5, '+' + (d.qty > 1 ? d.qty + ' ' : '') + ITEMS[d.id].name, NEON.green);
        }
      });
      AUDIO.SFX.pickup();
      pushMsg('Looted ' + best.name, NEON.cyan);
      return;
    }
    pushMsg('Nothing to interact with', '#888');
  }

  // =====================================================================
  //  COMBAT
  // =====================================================================
  function attack() {
    if (state !== STATE.PLAY || player.attackCd > 0) return;
    const base = ITEMS[player.weapon] || ITEMS.fists;
    player.attackCd = base.speed;

    if (base.ranged) {
      const ammo = player.inv.find((s) => s.id === base.ammo);
      if (!ammo) { pushMsg('Click! No ammo (R to reload)', NEON.red); AUDIO.SFX.select(); player.attackCd = 0.2; return; }
      ammo.qty--; if (ammo.qty <= 0) player.inv.splice(player.inv.indexOf(ammo), 1);
      const ang = player.dir;
      bullets.push({
        x: player.x, y: player.y,
        vx: Math.cos(ang) * 22, vy: Math.sin(ang) * 22,
        dmg: base.dmg, life: base.range / 22 * 1.4,
        spread: base.name.indexOf('Mossberg') >= 0,
      });
      if (base.name.indexOf('Mossberg') >= 0) {
        AUDIO.SFX.shotgun();
        for (let s = -2; s <= 2; s++) {
          const a2 = ang + s * 0.12;
          bullets.push({ x: player.x, y: player.y, vx: Math.cos(a2) * 20, vy: Math.sin(a2) * 20, dmg: base.dmg * 0.5, life: 0.28 });
        }
        kick(8);
      } else { AUDIO.SFX.gun(); kick(4); }
      muzzleFlash();
      return;
    }

    // melee arc
    AUDIO.SFX.swing();
    const ang = player.dir;
    let hitAny = false;
    for (const z of zombies) {
      if (z.dead) continue;
      const dx = z.x - player.x, dy = z.y - player.y;
      const d = Math.hypot(dx, dy);
      if (d > base.range + 0.4) continue;
      const za = Math.atan2(dy, dx);
      let diff = Math.abs(normAng(za - ang));
      if (diff < 1.1) {
        damageZombie(z, base.dmg, ang);
        hitAny = true;
      }
    }
    if (hitAny) {
      AUDIO.SFX.hit();
      kick(2);
      const w = player.inv.find((s) => s.id === player.weapon);
      if (w && w.durab !== undefined && base.durab !== Infinity) {
        w.durab--;
        if (w.durab <= 0) {
          pushMsg(base.name + ' broke!', NEON.red);
          player.inv.splice(player.inv.indexOf(w), 1);
          player.weapon = 'fists';
        }
      }
    }
  }

  function damageZombie(z, dmg, ang) {
    z.health -= dmg;
    z.knock = 0.25;
    z.kx = Math.cos(ang); z.ky = Math.sin(ang);
    spawnBlood(z.x, z.y);
    floatText(z.x, z.y - 0.4, Math.round(dmg), NEON.yellow);
    if (z.health <= 0) killZombie(z);
  }

  function killZombie(z) {
    z.dead = true;
    kills++;
    corpses.push({ x: z.x, y: z.y, t: 0 });
    spawnBlood(z.x, z.y, 14);
    AUDIO.SFX.zgroan();
  }

  function muzzleFlash() {
    for (let i = 0; i < 6; i++) {
      const a = player.dir + (Math.random() - 0.5) * 0.5;
      particles.push({ x: player.x + Math.cos(player.dir) * 0.5, y: player.y + Math.sin(player.dir) * 0.5,
        vx: Math.cos(a) * 6, vy: Math.sin(a) * 6, life: 0.12, col: NEON.yellow, r: 3 });
    }
  }

  function spawnBlood(x, y, n) {
    n = n || 6;
    for (let i = 0; i < n; i++) {
      const a = Math.random() * Math.PI * 2, s = Math.random() * 4;
      particles.push({ x, y, vx: Math.cos(a) * s, vy: Math.sin(a) * s, life: 0.3 + Math.random() * 0.3, col: NEON.red, r: 2 + Math.random() * 2 });
    }
  }

  let shake = 0;
  function kick(n) { shake = Math.min(14, shake + n); }

  // =====================================================================
  //  ZOMBIES
  // =====================================================================
  function spawnZombie(downtown) {
    let sp;
    if (downtown && Math.random() < 0.7) {
      // cluster around downtown core
      sp = { x: 30 + Math.random() * 60, y: 24 + Math.random() * 40 };
    } else {
      sp = world.spawnPoints[Math.floor(Math.random() * world.spawnPoints.length)];
      sp = { x: sp.x + (Math.random() - 0.5) * 3, y: sp.y + (Math.random() - 0.5) * 3 };
    }
    if (WG.isSolid(world.tiles, Math.floor(sp.x), Math.floor(sp.y))) return;
    // don't spawn on top of the player
    if (player && Math.hypot(sp.x - player.x, sp.y - player.y) < 12) return;
    const tough = Math.random();
    zombies.push({
      x: sp.x, y: sp.y, dir: Math.random() * Math.PI * 2,
      health: 30 + tough * 50, max: 30 + tough * 50,
      speed: 1.1 + Math.random() * 1.4 + (tough > 0.85 ? 2.2 : 0), // sprinters
      sprinter: tough > 0.85,
      radius: 0.35, dead: false, knock: 0, kx: 0, ky: 0,
      groan: Math.random() * 6, wander: Math.random() * Math.PI * 2, wt: 0,
      tint: tough > 0.85 ? NEON.pink : (Math.random() < 0.5 ? '#4a7c3a' : '#5a6b4a'),
    });
  }

  function updateZombies(dt) {
    for (const z of zombies) {
      if (z.dead) continue;
      z.groan -= dt;
      if (z.groan <= 0) { z.groan = 4 + Math.random() * 8; if (dist(z, player) < 14) AUDIO.SFX.zgroan(); }

      let tx, ty;
      const dp = dist(z, player);
      const sees = dp < 11 || (player.flashlight && dp < 16);
      if (sees) {
        tx = player.x; ty = player.y;
      } else {
        z.wt -= dt;
        if (z.wt <= 0) { z.wander = Math.random() * Math.PI * 2; z.wt = 1 + Math.random() * 3; }
        tx = z.x + Math.cos(z.wander); ty = z.y + Math.sin(z.wander);
      }
      let ang = Math.atan2(ty - z.y, tx - z.x);
      z.dir = ang;
      const spd = (sees ? z.speed : z.speed * 0.45) * (player.flashlight && z.sprinter ? 1.2 : 1);

      let mvx = Math.cos(ang) * spd * dt;
      let mvy = Math.sin(ang) * spd * dt;
      if (z.knock > 0) {
        mvx += z.kx * z.knock * 6 * dt; mvy += z.ky * z.knock * 6 * dt;
        z.knock -= dt;
      }
      moveEntity(z, mvx, mvy);

      // attack player
      if (dp < 0.7 && player.attackImmune <= 0) {
        hurtPlayer(0.18 + (z.sprinter ? 0.12 : 0), z);
      }
    }
    // cull dead occasionally to keep arrays small
    if (zombies.length > 400) zombies = zombies.filter((z) => !z.dead);
  }

  function hurtPlayer(dmgPerHit, z) {
    player.health -= dmgPerHit;
    player.hurtFlash = 0.25;
    player.attackImmune = 0.5;
    if (Math.random() < 0.12) {                 // chance of infection bite
      if (!player.infected) { player.infected = true; pushMsg('You were bitten! Infection rising...', NEON.red); }
    }
    AUDIO.SFX.hurt();
    kick(3);
    if (player.health <= 0) die();
  }

  // =====================================================================
  //  BULLETS
  // =====================================================================
  function updateBullets(dt) {
    for (const b of bullets) {
      b.x += b.vx * dt; b.y += b.vy * dt; b.life -= dt;
      if (WG.isSolid(world.tiles, Math.floor(b.x), Math.floor(b.y))) { b.life = 0; continue; }
      for (const z of zombies) {
        if (z.dead) continue;
        if (Math.hypot(z.x - b.x, z.y - b.y) < 0.5) {
          damageZombie(z, b.dmg, Math.atan2(b.vy, b.vx));
          b.life = 0; break;
        }
      }
    }
    bullets = bullets.filter((b) => b.life > 0);
  }

  // =====================================================================
  //  MOVEMENT & COLLISION
  // =====================================================================
  function moveEntity(e, mvx, mvy) {
    const nx = e.x + mvx;
    if (!collides(nx, e.y, e.radius)) e.x = nx;
    const ny = e.y + mvy;
    if (!collides(e.x, ny, e.radius)) e.y = ny;
  }

  function collides(x, y, r) {
    for (let oy = -1; oy <= 1; oy++) {
      for (let ox = -1; ox <= 1; ox++) {
        const tx = Math.floor(x + ox * r), ty = Math.floor(y + oy * r);
        if (WG.isSolid(world.tiles, tx, ty)) {
          // closest point check
          const cx = clamp(x, tx, tx + 1), cy = clamp(y, ty, ty + 1);
          if (Math.hypot(x - cx, y - cy) < r) return true;
        }
      }
    }
    return false;
  }

  // =====================================================================
  //  PLAYER UPDATE
  // =====================================================================
  function updatePlayer(dt) {
    let mx = 0, my = 0;
    if (keys['w'] || keys['arrowup']) my -= 1;
    if (keys['s'] || keys['arrowdown']) my += 1;
    if (keys['a'] || keys['arrowleft']) mx -= 1;
    if (keys['d'] || keys['arrowright']) mx += 1;
    if (mx || my) {
      const l = Math.hypot(mx, my);
      const fatiguePenalty = player.fatigue > 70 ? 0.6 : 1;
      const sp = player.speed * fatiguePenalty * dt;
      moveEntity(player, (mx / l) * sp, (my / l) * sp);
      player.fatigue = Math.min(100, player.fatigue + dt * 0.6);
    }

    // aim toward mouse
    mouse.wx = cam.x + mouse.x / cam.zoom;
    mouse.wy = cam.y + mouse.y / cam.zoom;
    player.dir = Math.atan2(mouse.wy / TILE - player.y, mouse.wx / TILE - player.x);

    if (mouse.down) attack();

    player.attackCd = Math.max(0, player.attackCd - dt);
    player.attackImmune = Math.max(0, (player.attackImmune || 0) - dt);
    player.hurtFlash = Math.max(0, player.hurtFlash - dt);

    // survival decay (scaled to game time)
    const gm = dt * timeScale;       // game-minutes elapsed
    player.hunger = Math.min(100, player.hunger + gm * 0.10);
    player.thirst = Math.min(100, player.thirst + gm * 0.14);
    player.fatigue = Math.min(100, player.fatigue + gm * 0.05);

    // consequences
    if (player.hunger >= 100 || player.thirst >= 100) player.health -= dt * 1.2;
    if (player.fatigue >= 100) player.mood = Math.max(0, player.mood - dt * 2);
    if (player.infected) {
      player.infection = Math.min(100, player.infection + dt * 0.7);
      player.health -= dt * (0.3 + player.infection / 120);
      if (player.infection >= 100) { pushMsg('The infection takes you...', NEON.red); }
    }
    // slow natural regen when well-fed & rested
    if (player.hunger < 60 && player.thirst < 60 && !player.infected && player.health < 100) {
      player.health = Math.min(100, player.health + dt * 0.4);
    }
    player.mood = clamp(player.mood + (player.health > 50 ? dt * 0.1 : -dt * 0.2), 0, 100);

    if (player.health <= 0) die();
  }

  function die() {
    if (player.dead) return;
    player.dead = true;
    state = STATE.DEAD;
    AUDIO.SFX.death();
    AUDIO.stopMusic();
  }

  // =====================================================================
  //  WORLD / TIME UPDATE
  // =====================================================================
  function updateWorld(dt) {
    dayTime += dt * timeScale;
    if (dayTime >= DAY_LEN) { dayTime -= DAY_LEN; dayCount++; pushMsg('Day ' + dayCount + ' in Anchorage', NEON.cyan); }

    // night spawns more & faster waves
    const isNight = dayTime < 6 * 60 || dayTime > 21 * 60;
    waveTimer -= dt;
    if (waveTimer <= 0) {
      waveTimer = isNight ? 1.4 : 3.5;
      const alive = zombies.reduce((s, z) => s + (z.dead ? 0 : 1), 0);
      const cap = 60 + dayCount * 25;
      if (alive < cap) {
        const n = isNight ? 4 : 2;
        for (let i = 0; i < n; i++) spawnZombie(false);
      }
    }

    // update particles
    for (const p of particles) { p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt; p.vx *= 0.9; p.vy *= 0.9; }
    particles = particles.filter((p) => p.life > 0);
    for (const c of corpses) c.t += dt;
    if (corpses.length > 120) corpses.splice(0, corpses.length - 120);
    for (const f of floatTexts) { f.y -= dt * 1.2; f.life -= dt; }
    floatTexts = floatTexts.filter((f) => f.life > 0);
    for (const m of messages) m.life -= dt;
    messages = messages.filter((m) => m.life > 0);

    shake *= 0.86;
  }

  function darknessAlpha() {
    // 0 = full day, ~0.78 = deep night
    const t = dayTime;
    let d;
    if (t < 4 * 60) d = 0.78;
    else if (t < 7 * 60) d = lerp(0.78, 0, (t - 4 * 60) / (3 * 60));
    else if (t < 19 * 60) d = 0;
    else if (t < 22 * 60) d = lerp(0, 0.78, (t - 19 * 60) / (3 * 60));
    else d = 0.78;
    return d;
  }

  // =====================================================================
  //  MAIN LOOP
  // =====================================================================
  function loop(ts) {
    const dt = Math.min(0.05, (ts - last) / 1000 || 0);
    last = ts;
    bootT += dt;

    if (state === STATE.PLAY) {
      acc += dt;
      while (acc >= STEP) {
        updatePlayer(STEP);
        updateZombies(STEP);
        updateBullets(STEP);
        updateWorld(STEP);
        acc -= STEP;
      }
      updateCamera();
    } else if (state === STATE.DEAD || state === STATE.PAUSE) {
      // freeze sim, keep particles drifting a touch
    }

    render();
    requestAnimationFrame(loop);
  }

  function updateCamera() {
    cam.zoom = 1;
    const targetX = player.x * TILE - W / 2;
    const targetY = player.y * TILE - H / 2;
    cam.x += (targetX - cam.x) * 0.12;
    cam.y += (targetY - cam.y) * 0.12;
  }

  // =====================================================================
  //  RENDER
  // =====================================================================
  const tileColors = {
    [T.GRASS]:   '#16361f',
    [T.STREET]:  '#1c1f26',
    [T.SIDEWALK]:'#33363f',
    [T.FLOOR]:   '#241b2e',
    [T.WALL]:    '#3a2350',
    [T.DOOR]:    '#0b3a3f',
    [T.WATER]:   '#0a1a3a',
    [T.TREE]:    '#0f2a16',
    [T.LOT]:     '#202028',
    [T.RAIL]:    '#2a2a30',
  };

  function render() {
    ctx2d.fillStyle = '#05060a';
    ctx2d.fillRect(0, 0, W, H);

    if (state === STATE.BOOT) { renderBoot(); applyCRT(); return; }
    if (state === STATE.TITLE) { renderTitle(); applyCRT(); return; }

    ctx2d.save();
    const sx = (Math.random() - 0.5) * shake;
    const sy = (Math.random() - 0.5) * shake;
    ctx2d.translate(-cam.x + sx, -cam.y + sy);

    renderTiles();
    renderCorpses();
    renderContainers();
    renderParticles(true);
    renderZombies();
    renderPlayer();
    renderBullets();
    renderParticles(false);
    renderBuildingLabels();
    renderFloatTexts();
    ctx2d.restore();

    renderLighting();
    renderHUD();
    if (showMap) renderBigMap();
    if (showInventory) renderInventory();
    if (state === STATE.PAUSE) renderPauseOverlay();
    if (state === STATE.DEAD) renderDeath();
    applyCRT();
  }

  function worldToScreenVisible() {
    const x0 = Math.floor(cam.x / TILE) - 1;
    const y0 = Math.floor(cam.y / TILE) - 1;
    const x1 = x0 + Math.ceil(W / TILE) + 2;
    const y1 = y0 + Math.ceil(H / TILE) + 2;
    return { x0: Math.max(0, x0), y0: Math.max(0, y0), x1: Math.min(world.W, x1), y1: Math.min(world.H, y1) };
  }

  function renderTiles() {
    const v = worldToScreenVisible();
    const tnow = bootT;
    for (let y = v.y0; y < v.y1; y++) {
      for (let x = v.x0; x < v.x1; x++) {
        const t = world.tiles[y][x];
        const px = x * TILE, py = y * TILE;
        ctx2d.fillStyle = tileColors[t] || '#101010';
        ctx2d.fillRect(px, py, TILE, TILE);

        // neon detailing
        if (t === T.STREET) {
          // road center dashes
          ctx2d.fillStyle = 'rgba(255,230,0,0.10)';
          if ((x + y) % 2 === 0) ctx2d.fillRect(px + TILE / 2 - 1, py + 4, 2, TILE - 8);
        } else if (t === T.SIDEWALK) {
          ctx2d.strokeStyle = 'rgba(5,217,232,0.10)';
          ctx2d.strokeRect(px + 0.5, py + 0.5, TILE - 1, TILE - 1);
        } else if (t === T.WALL) {
          ctx2d.strokeStyle = NEON.purple;
          ctx2d.globalAlpha = 0.5;
          ctx2d.strokeRect(px + 1, py + 1, TILE - 2, TILE - 2);
          ctx2d.globalAlpha = 1;
        } else if (t === T.DOOR) {
          const g = 0.5 + 0.5 * Math.sin(tnow * 4 + x);
          ctx2d.fillStyle = NEON.green;
          ctx2d.globalAlpha = 0.3 + g * 0.4;
          ctx2d.fillRect(px + 4, py + 4, TILE - 8, TILE - 8);
          ctx2d.globalAlpha = 1;
        } else if (t === T.WATER) {
          ctx2d.fillStyle = 'rgba(45,107,255,0.14)';
          const w = Math.sin(tnow * 2 + x * 0.6 + y) * 2;
          ctx2d.fillRect(px, py + TILE / 2 + w, TILE, 2);
        } else if (t === T.TREE) {
          ctx2d.fillStyle = '#1c5a2a';
          ctx2d.beginPath();
          ctx2d.arc(px + TILE / 2, py + TILE / 2, TILE * 0.42, 0, Math.PI * 2);
          ctx2d.fill();
          ctx2d.fillStyle = 'rgba(57,255,20,0.18)';
          ctx2d.beginPath();
          ctx2d.arc(px + TILE / 2, py + TILE / 2, TILE * 0.30, 0, Math.PI * 2);
          ctx2d.fill();
        } else if (t === T.RAIL) {
          ctx2d.strokeStyle = 'rgba(255,123,0,0.4)';
          ctx2d.lineWidth = 1;
          ctx2d.beginPath();
          ctx2d.moveTo(px, py + 8); ctx2d.lineTo(px + TILE, py + 8);
          ctx2d.moveTo(px, py + TILE - 8); ctx2d.lineTo(px + TILE, py + TILE - 8);
          ctx2d.stroke();
        }
      }
    }
  }

  function renderContainers() {
    for (const c of world.containers) {
      const px = c.x * TILE, py = c.y * TILE;
      if (px < cam.x - TILE || px > cam.x + W || py < cam.y - TILE || py > cam.y + H) continue;
      ctx2d.fillStyle = c.opened ? '#3a3a44' : NEON.orange;
      if (!c.opened) {
        ctx2d.shadowColor = NEON.orange; ctx2d.shadowBlur = 8;
      }
      ctx2d.fillRect(px + 7, py + 9, TILE - 14, TILE - 14);
      ctx2d.shadowBlur = 0;
      ctx2d.strokeStyle = c.opened ? '#222' : '#fff';
      ctx2d.strokeRect(px + 7, py + 9, TILE - 14, TILE - 14);
    }
  }

  function renderCorpses() {
    for (const c of corpses) {
      const px = c.x * TILE, py = c.y * TILE;
      ctx2d.globalAlpha = Math.max(0.15, 1 - c.t / 60);
      ctx2d.fillStyle = '#5a1020';
      ctx2d.beginPath();
      ctx2d.ellipse(px, py, TILE * 0.5, TILE * 0.35, 0, 0, Math.PI * 2);
      ctx2d.fill();
      ctx2d.globalAlpha = 1;
    }
  }

  function blockSprite(px, py, size, body, outline, glow) {
    if (glow) { ctx2d.shadowColor = glow; ctx2d.shadowBlur = 10; }
    ctx2d.fillStyle = body;
    ctx2d.fillRect(px - size / 2, py - size / 2, size, size);
    ctx2d.shadowBlur = 0;
    ctx2d.strokeStyle = outline;
    ctx2d.lineWidth = 2;
    ctx2d.strokeRect(px - size / 2, py - size / 2, size, size);
  }

  function renderZombies() {
    const v = worldToScreenVisible();
    for (const z of zombies) {
      if (z.dead) continue;
      if (z.x < v.x0 - 1 || z.x > v.x1 + 1 || z.y < v.y0 - 1 || z.y > v.y1 + 1) continue;
      const px = z.x * TILE, py = z.y * TILE;
      const sz = TILE * 0.62;
      blockSprite(px, py, sz, z.tint, '#0a0a0a', z.sprinter ? NEON.pink : null);
      // eyes (neon)
      ctx2d.fillStyle = z.sprinter ? NEON.yellow : NEON.green;
      const ex = Math.cos(z.dir) * 4, ey = Math.sin(z.dir) * 4;
      ctx2d.fillRect(px + ex - 4, py + ey - 3, 3, 3);
      ctx2d.fillRect(px + ex + 1, py + ey - 3, 3, 3);
      // health bar if hurt
      if (z.health < z.max) {
        ctx2d.fillStyle = '#000'; ctx2d.fillRect(px - sz / 2, py - sz / 2 - 6, sz, 3);
        ctx2d.fillStyle = NEON.red; ctx2d.fillRect(px - sz / 2, py - sz / 2 - 6, sz * (z.health / z.max), 3);
      }
    }
  }

  function renderPlayer() {
    const px = player.x * TILE, py = player.y * TILE;
    // flashlight cone
    if (player.flashlight) {
      ctx2d.save();
      const grad = ctx2d.createRadialGradient(px, py, 10, px, py, TILE * 7);
      grad.addColorStop(0, 'rgba(255,255,200,0.18)');
      grad.addColorStop(1, 'rgba(255,255,200,0)');
      ctx2d.fillStyle = grad;
      ctx2d.beginPath();
      ctx2d.moveTo(px, py);
      ctx2d.arc(px, py, TILE * 7, player.dir - 0.5, player.dir + 0.5);
      ctx2d.closePath(); ctx2d.fill();
      ctx2d.restore();
    }
    const sz = TILE * 0.6;
    const flash = player.hurtFlash > 0 ? NEON.red : NEON.cyan;
    blockSprite(px, py, sz, '#11212b', flash, flash);
    // facing weapon line
    const base = ITEMS[player.weapon] || ITEMS.fists;
    ctx2d.strokeStyle = base.ranged ? NEON.yellow : NEON.cyan;
    ctx2d.lineWidth = 3;
    ctx2d.beginPath();
    ctx2d.moveTo(px, py);
    ctx2d.lineTo(px + Math.cos(player.dir) * TILE * 0.7, py + Math.sin(player.dir) * TILE * 0.7);
    ctx2d.stroke();
    // attack arc flourish
    if (player.attackCd > base.speed * 0.6 && !base.ranged) {
      ctx2d.strokeStyle = 'rgba(5,217,232,0.6)';
      ctx2d.lineWidth = 4;
      ctx2d.beginPath();
      ctx2d.arc(px, py, base.range * TILE, player.dir - 1.1, player.dir + 1.1);
      ctx2d.stroke();
    }
  }

  function renderBullets() {
    for (const b of bullets) {
      const px = b.x * TILE, py = b.y * TILE;
      ctx2d.strokeStyle = NEON.yellow;
      ctx2d.lineWidth = 3;
      ctx2d.shadowColor = NEON.yellow; ctx2d.shadowBlur = 8;
      ctx2d.beginPath();
      ctx2d.moveTo(px, py);
      ctx2d.lineTo(px - b.vx * TILE * 0.03, py - b.vy * TILE * 0.03);
      ctx2d.stroke();
      ctx2d.shadowBlur = 0;
    }
  }

  function renderParticles(under) {
    for (const p of particles) {
      const isBlood = p.col === NEON.red;
      if (under !== isBlood) continue;
      const px = p.x * TILE, py = p.y * TILE;
      ctx2d.globalAlpha = Math.max(0, p.life * 2);
      ctx2d.fillStyle = p.col;
      ctx2d.fillRect(px - p.r / 2, py - p.r / 2, p.r, p.r);
      ctx2d.globalAlpha = 1;
    }
  }

  function renderBuildingLabels() {
    ctx2d.textAlign = 'center';
    for (const lab of world.labels) {
      if (lab.kind !== 'building') continue;
      const px = lab.tx * TILE, py = lab.ty * TILE;
      if (px < cam.x - 200 || px > cam.x + W + 200 || py < cam.y - 100 || py > cam.y + H + 100) continue;
      ctx2d.font = 'bold 11px "Courier New", monospace';
      ctx2d.fillStyle = '#000';
      ctx2d.fillText(lab.text, px + 1, py + 1);
      ctx2d.fillStyle = NEON.cyan;
      ctx2d.shadowColor = NEON.cyan; ctx2d.shadowBlur = 6;
      ctx2d.fillText(lab.text, px, py);
      ctx2d.shadowBlur = 0;
    }
    // street labels (smaller, pink)
    for (const lab of world.labels) {
      if (lab.kind !== 'street') continue;
      const px = lab.tx * TILE, py = lab.ty * TILE;
      if (px < cam.x - 200 || px > cam.x + W + 200 || py < cam.y - 100 || py > cam.y + H + 100) continue;
      ctx2d.save();
      ctx2d.translate(px, py);
      if (lab.vertical) ctx2d.rotate(-Math.PI / 2);
      ctx2d.font = 'bold 9px "Courier New", monospace';
      ctx2d.fillStyle = NEON.pink;
      ctx2d.globalAlpha = 0.7;
      ctx2d.fillText(lab.text.toUpperCase(), 0, 0);
      ctx2d.restore();
      ctx2d.globalAlpha = 1;
    }
  }

  function renderFloatTexts() {
    ctx2d.textAlign = 'center';
    for (const f of floatTexts) {
      ctx2d.font = 'bold 13px "Courier New", monospace';
      ctx2d.globalAlpha = Math.min(1, f.life * 2);
      ctx2d.fillStyle = '#000'; ctx2d.fillText(f.text, f.x * TILE + 1, f.y * TILE + 1);
      ctx2d.fillStyle = f.col; ctx2d.fillText(f.text, f.x * TILE, f.y * TILE);
      ctx2d.globalAlpha = 1;
    }
  }

  // ---------- Lighting / night ----------
  function renderLighting() {
    const d = darknessAlpha();
    if (d <= 0.01) return;
    ctx2d.save();
    ctx2d.fillStyle = `rgba(2,4,20,${d})`;
    ctx2d.fillRect(0, 0, W, H);
    // neon city glow punching through the dark (additive)
    ctx2d.globalCompositeOperation = 'lighter';
    const px = player.x * TILE - cam.x, py = player.y * TILE - cam.y;
    const pr = player.flashlight ? TILE * 8 : TILE * 4;
    const g = ctx2d.createRadialGradient(px, py, 8, px, py, pr);
    g.addColorStop(0, `rgba(5,217,232,${0.5 * d})`);
    g.addColorStop(1, 'rgba(5,217,232,0)');
    ctx2d.fillStyle = g;
    ctx2d.beginPath(); ctx2d.arc(px, py, pr, 0, Math.PI * 2); ctx2d.fill();

    // doorway glows
    for (const lab of world.labels) {
      if (lab.kind !== 'building') continue;
    }
    ctx2d.restore();
  }

  // =====================================================================
  //  HUD
  // =====================================================================
  function bar(x, y, w, h, val, col, label) {
    ctx2d.fillStyle = 'rgba(0,0,0,0.6)';
    ctx2d.fillRect(x - 2, y - 2, w + 4, h + 4);
    ctx2d.fillStyle = '#15151c';
    ctx2d.fillRect(x, y, w, h);
    ctx2d.fillStyle = col;
    ctx2d.shadowColor = col; ctx2d.shadowBlur = 8;
    ctx2d.fillRect(x, y, w * clamp(val / 100, 0, 1), h);
    ctx2d.shadowBlur = 0;
    ctx2d.strokeStyle = col; ctx2d.globalAlpha = 0.6;
    ctx2d.strokeRect(x, y, w, h); ctx2d.globalAlpha = 1;
    ctx2d.fillStyle = '#fff';
    ctx2d.font = 'bold 10px "Courier New", monospace';
    ctx2d.textAlign = 'left';
    ctx2d.fillText(label, x + 4, y + h - 3);
  }

  function renderHUD() {
    // top-left stat stack
    const x = 16, w = 180, h = 16, gap = 6;
    let y = 16;
    bar(x, y, w, h, player.health, player.hurtFlash > 0 ? '#fff' : NEON.green, 'HEALTH'); y += h + gap;
    bar(x, y, w, h, 100 - player.hunger, NEON.orange, 'FED'); y += h + gap;
    bar(x, y, w, h, 100 - player.thirst, NEON.cyan, 'HYDRO'); y += h + gap;
    bar(x, y, w, h, 100 - player.fatigue, NEON.purple, 'ENERGY'); y += h + gap;
    bar(x, y, w, h, player.mood, NEON.pink, 'MOOD'); y += h + gap;
    if (player.infected) {
      bar(x, y, w, h, player.infection, NEON.red, 'INFECTION'); y += h + gap;
    }

    // top-right clock + day + kills
    const hh = Math.floor(dayTime / 60), mm = Math.floor(dayTime % 60);
    const tstr = String(hh).padStart(2, '0') + ':' + String(mm).padStart(2, '0');
    panelText([
      ['DAY ' + dayCount, NEON.cyan],
      [tstr, NEON.yellow],
      ['KILLS ' + kills, NEON.pink],
      ['Z ALIVE ' + zombies.reduce((s, z) => s + (z.dead ? 0 : 1), 0), NEON.green],
    ], W - 150, 16);

    // equipped weapon + ammo
    const base = ITEMS[player.weapon] || ITEMS.fists;
    let wtxt = base.icon + ' ' + base.name;
    if (base.ranged) {
      const ammo = player.inv.find((s) => s.id === base.ammo);
      wtxt += '  [' + (ammo ? ammo.qty : 0) + ']';
    }
    ctx2d.font = 'bold 14px "Courier New", monospace';
    ctx2d.textAlign = 'center';
    ctx2d.fillStyle = '#000'; ctx2d.fillText(wtxt, W / 2 + 1, H - 22);
    ctx2d.fillStyle = NEON.yellow;
    ctx2d.shadowColor = NEON.yellow; ctx2d.shadowBlur = 8;
    ctx2d.fillText(wtxt, W / 2, H - 23);
    ctx2d.shadowBlur = 0;

    // hotbar
    renderHotbar();

    // messages (bottom-left log)
    ctx2d.textAlign = 'left';
    let my = H - 30;
    for (let i = messages.length - 1; i >= 0 && i >= messages.length - 5; i--) {
      const m = messages[i];
      ctx2d.globalAlpha = clamp(m.life, 0, 1);
      ctx2d.font = '12px "Courier New", monospace';
      ctx2d.fillStyle = '#000'; ctx2d.fillText(m.text, 17, my + 1);
      ctx2d.fillStyle = m.col; ctx2d.fillText(m.text, 16, my);
      ctx2d.globalAlpha = 1;
      my -= 16;
    }

    // controls hint
    ctx2d.textAlign = 'right';
    ctx2d.font = '10px "Courier New", monospace';
    ctx2d.fillStyle = 'rgba(255,255,255,0.4)';
    ctx2d.fillText('WASD move · MOUSE aim · CLICK/SPACE attack · E loot · 1-8 use · R reload · M map · TAB bag · F light · P pause', W - 12, H - 8);
  }

  function panelText(lines, x, y) {
    ctx2d.textAlign = 'left';
    let yy = y;
    for (const [txt, col] of lines) {
      ctx2d.font = 'bold 13px "Courier New", monospace';
      ctx2d.fillStyle = '#000'; ctx2d.fillText(txt, x + 1, yy + 1);
      ctx2d.fillStyle = col;
      ctx2d.shadowColor = col; ctx2d.shadowBlur = 6;
      ctx2d.fillText(txt, x, yy);
      ctx2d.shadowBlur = 0;
      yy += 18;
    }
  }

  function renderHotbar() {
    const n = player.slots;
    const bw = 46, gap = 6;
    const totalW = n * bw + (n - 1) * gap;
    const x0 = W / 2 - totalW / 2;
    const y = H - 64;
    for (let i = 0; i < n; i++) {
      const x = x0 + i * (bw + gap);
      const s = player.inv[i];
      ctx2d.fillStyle = 'rgba(0,0,0,0.55)';
      ctx2d.fillRect(x, y, bw, bw);
      const equipped = s && s.id === player.weapon;
      ctx2d.strokeStyle = equipped ? NEON.yellow : 'rgba(5,217,232,0.5)';
      ctx2d.lineWidth = equipped ? 3 : 1;
      ctx2d.strokeRect(x, y, bw, bw);
      // slot number
      ctx2d.fillStyle = 'rgba(255,255,255,0.45)';
      ctx2d.font = '9px "Courier New", monospace';
      ctx2d.textAlign = 'left';
      ctx2d.fillText(String(i + 1), x + 3, y + 11);
      if (s) {
        const base = ITEMS[s.id];
        ctx2d.font = '20px sans-serif';
        ctx2d.textAlign = 'center';
        ctx2d.fillStyle = '#fff';
        ctx2d.fillText(base.icon, x + bw / 2, y + bw / 2 + 7);
        if (s.qty > 1) {
          ctx2d.font = 'bold 11px "Courier New", monospace';
          ctx2d.fillStyle = NEON.yellow;
          ctx2d.textAlign = 'right';
          ctx2d.fillText('x' + s.qty, x + bw - 3, y + bw - 4);
        }
        if (base.type === 'weapon' && s.durab !== undefined && base.durab !== Infinity) {
          ctx2d.fillStyle = '#222'; ctx2d.fillRect(x + 3, y + bw - 5, bw - 6, 3);
          ctx2d.fillStyle = NEON.green; ctx2d.fillRect(x + 3, y + bw - 5, (bw - 6) * clamp(s.durab / base.durab, 0, 1), 3);
        }
      }
    }
  }

  function renderInventory() {
    overlayBox(W / 2 - 230, 90, 460, 360, 'INVENTORY');
    let y = 150;
    ctx2d.textAlign = 'left';
    if (player.inv.length === 0) {
      ctx2d.fillStyle = '#888'; ctx2d.font = '14px "Courier New", monospace';
      ctx2d.fillText('Empty. Loot orange crates with E.', W / 2 - 200, y);
    }
    player.inv.forEach((s, i) => {
      const base = ITEMS[s.id];
      const row = W / 2 - 210;
      ctx2d.font = '16px "Courier New", monospace';
      ctx2d.fillStyle = s.id === player.weapon ? NEON.yellow : '#fff';
      ctx2d.fillText(`${i + 1}. ${base.icon}  ${base.name}` + (s.qty > 1 ? `  x${s.qty}` : ''), row, y);
      ctx2d.fillStyle = NEON.cyan; ctx2d.font = '11px "Courier New", monospace';
      let desc = base.type.toUpperCase();
      if (base.dmg) desc += ` · DMG ${base.dmg}`;
      if (base.hunger) desc += ` · +${base.hunger} FED`;
      if (base.thirst) desc += ` · +${base.thirst} HYDRO`;
      if (base.heal) desc += ` · +${base.heal} HP`;
      ctx2d.fillText(desc, row + 240, y);
      y += 30;
    });
    ctx2d.fillStyle = '#888'; ctx2d.textAlign = 'center';
    ctx2d.font = '11px "Courier New", monospace';
    ctx2d.fillText('Press number to use/equip · TAB to close', W / 2, 430);
  }

  function renderBigMap() {
    const mw = Math.min(W - 80, 760), mh = Math.min(H - 80, 680);
    const mx = W / 2 - mw / 2, my = H / 2 - mh / 2;
    overlayBox(mx, my, mw, mh, 'ANCHORAGE — TACTICAL MAP');
    const sx = (mw - 40) / world.W, sy = (mh - 80) / world.H;
    const ox = mx + 20, oy = my + 50;
    // tiles (downsampled)
    for (let y = 0; y < world.H; y += 1) {
      for (let x = 0; x < world.W; x += 1) {
        const t = world.tiles[y][x];
        let c = null;
        if (t === T.STREET) c = '#222';
        else if (t === T.WATER) c = '#0a1a3a';
        else if (t === T.WALL || t === T.FLOOR) c = '#3a2350';
        else if (t === T.TREE) c = '#123';
        if (c) { ctx2d.fillStyle = c; ctx2d.fillRect(ox + x * sx, oy + y * sy, sx + 0.6, sy + 0.6); }
      }
    }
    // building labels
    ctx2d.font = '8px "Courier New", monospace';
    ctx2d.textAlign = 'center';
    for (const b of A.BUILDINGS) {
      const cx = ox + (b.x + b.w / 2) * sx, cy = oy + (b.y + b.h / 2) * sy;
      ctx2d.fillStyle = NEON.cyan; ctx2d.globalAlpha = 0.85;
      ctx2d.fillRect(ox + b.x * sx, oy + b.y * sy, b.w * sx, b.h * sy);
      ctx2d.globalAlpha = 1;
    }
    // player marker
    ctx2d.fillStyle = NEON.yellow;
    ctx2d.shadowColor = NEON.yellow; ctx2d.shadowBlur = 10;
    ctx2d.beginPath();
    ctx2d.arc(ox + player.x * sx, oy + player.y * sy, 4, 0, Math.PI * 2);
    ctx2d.fill(); ctx2d.shadowBlur = 0;
    // zombie blips
    ctx2d.fillStyle = NEON.red;
    for (const z of zombies) { if (!z.dead) ctx2d.fillRect(ox + z.x * sx - 1, oy + z.y * sy - 1, 2, 2); }
    ctx2d.fillStyle = '#888'; ctx2d.textAlign = 'center'; ctx2d.font = '11px "Courier New", monospace';
    ctx2d.fillText('M to close', W / 2, my + mh - 14);
  }

  function overlayBox(x, y, w, h, title) {
    ctx2d.fillStyle = 'rgba(6,8,20,0.92)';
    ctx2d.fillRect(x, y, w, h);
    ctx2d.strokeStyle = NEON.pink; ctx2d.lineWidth = 2;
    ctx2d.shadowColor = NEON.pink; ctx2d.shadowBlur = 12;
    ctx2d.strokeRect(x, y, w, h);
    ctx2d.shadowBlur = 0;
    ctx2d.fillStyle = NEON.cyan;
    ctx2d.font = 'bold 18px "Courier New", monospace';
    ctx2d.textAlign = 'center';
    ctx2d.shadowColor = NEON.cyan; ctx2d.shadowBlur = 8;
    ctx2d.fillText(title, x + w / 2, y + 30);
    ctx2d.shadowBlur = 0;
  }

  function renderPauseOverlay() {
    ctx2d.fillStyle = 'rgba(0,0,0,0.6)';
    ctx2d.fillRect(0, 0, W, H);
    neonTitle('PAUSED', W / 2, H / 2, 60, NEON.cyan);
    ctx2d.textAlign = 'center';
    ctx2d.font = '16px "Courier New", monospace';
    ctx2d.fillStyle = '#fff';
    ctx2d.fillText('Press P to resume', W / 2, H / 2 + 50);
  }

  function renderDeath() {
    ctx2d.fillStyle = 'rgba(40,0,10,0.7)';
    ctx2d.fillRect(0, 0, W, H);
    neonTitle('YOU DIED', W / 2, H / 2 - 30, 72, NEON.red);
    ctx2d.textAlign = 'center';
    ctx2d.font = 'bold 18px "Courier New", monospace';
    ctx2d.fillStyle = NEON.cyan;
    ctx2d.fillText(`Survived ${dayCount} day${dayCount > 1 ? 's' : ''} · ${kills} kills in Anchorage`, W / 2, H / 2 + 30);
    ctx2d.fillStyle = '#fff'; ctx2d.font = '14px "Courier New", monospace';
    ctx2d.fillText('Press ENTER to return to title', W / 2, H / 2 + 64);
  }

  // =====================================================================
  //  BOOT (SEGA-style) + TITLE
  // =====================================================================
  function renderBoot() {
    // SEGA-style "presents" screen
    const t = bootT;
    ctx2d.fillStyle = '#05060a';
    ctx2d.fillRect(0, 0, W, H);
    if (t > 0.4 && t < 0.5) AUDIO.SFX.sega();
    const appear = clamp((t - 0.2) / 0.6, 0, 1);
    ctx2d.save();
    ctx2d.globalAlpha = appear;
    neonTitle('NEON GENESIS ARCADE', W / 2, H / 2 - 40, 38, NEON.cyan);
    ctx2d.font = 'italic bold 20px "Courier New", monospace';
    ctx2d.textAlign = 'center';
    ctx2d.fillStyle = NEON.pink;
    ctx2d.shadowColor = NEON.pink; ctx2d.shadowBlur = 12;
    ctx2d.fillText('p r e s e n t s', W / 2, H / 2 + 10);
    ctx2d.shadowBlur = 0;
    ctx2d.restore();
    if (t > 2.4) {
      ctx2d.globalAlpha = (Math.sin(t * 5) + 1) / 2;
      ctx2d.fillStyle = '#fff'; ctx2d.font = '14px "Courier New", monospace';
      ctx2d.fillText('press any key', W / 2, H - 80);
      ctx2d.globalAlpha = 1;
    }
    if (t > 5) state = STATE.TITLE;
  }

  let titleStars = null;
  function renderTitle() {
    const t = bootT;
    // synthwave gradient sky
    const g = ctx2d.createLinearGradient(0, 0, 0, H);
    g.addColorStop(0, '#10021f');
    g.addColorStop(0.5, '#3a0a4a');
    g.addColorStop(0.5, '#ff2d95');
    g.addColorStop(1, '#05060a');
    ctx2d.fillStyle = g; ctx2d.fillRect(0, 0, W, H);

    // neon sun
    ctx2d.save();
    ctx2d.beginPath();
    ctx2d.arc(W / 2, H * 0.42, 120, 0, Math.PI * 2);
    ctx2d.clip();
    const sg = ctx2d.createLinearGradient(0, H * 0.42 - 120, 0, H * 0.42 + 120);
    sg.addColorStop(0, NEON.yellow); sg.addColorStop(1, NEON.orange);
    ctx2d.fillStyle = sg; ctx2d.fillRect(W / 2 - 120, H * 0.42 - 120, 240, 240);
    // sun scanlines
    ctx2d.fillStyle = '#3a0a4a';
    for (let i = 0; i < 12; i++) {
      const yy = H * 0.42 + 20 + i * (i * 1.4);
      ctx2d.fillRect(W / 2 - 120, yy, 240, 4 + i);
    }
    ctx2d.restore();

    // perspective neon grid (lower half)
    ctx2d.strokeStyle = 'rgba(5,217,232,0.5)';
    ctx2d.lineWidth = 1;
    const hor = H * 0.5;
    for (let i = -10; i <= 10; i++) {
      ctx2d.beginPath();
      ctx2d.moveTo(W / 2 + i * 60, hor);
      ctx2d.lineTo(W / 2 + i * 600, H);
      ctx2d.stroke();
    }
    for (let i = 0; i < 14; i++) {
      const yy = hor + Math.pow(i / 14, 2) * (H - hor) + ((t * 60) % ((H - hor) / 14));
      ctx2d.beginPath(); ctx2d.moveTo(0, yy); ctx2d.lineTo(W, yy); ctx2d.stroke();
    }

    // title
    neonTitle('ZOMBOID', W / 2, H * 0.30, Math.min(120, W / 8), NEON.pink, NEON.cyan);
    ctx2d.font = `bold ${Math.min(34, W / 28)}px "Courier New", monospace`;
    ctx2d.textAlign = 'center';
    ctx2d.fillStyle = NEON.cyan;
    ctx2d.shadowColor = NEON.cyan; ctx2d.shadowBlur = 14;
    ctx2d.fillText('A N C H O R A G E', W / 2, H * 0.30 + 60);
    ctx2d.shadowBlur = 0;

    // blink prompt
    if (Math.sin(t * 4) > 0) {
      ctx2d.font = 'bold 22px "Courier New", monospace';
      ctx2d.fillStyle = NEON.yellow;
      ctx2d.shadowColor = NEON.yellow; ctx2d.shadowBlur = 12;
      ctx2d.fillText('PRESS START  ▶', W / 2, H * 0.82);
      ctx2d.shadowBlur = 0;
    }
    ctx2d.font = '12px "Courier New", monospace';
    ctx2d.fillStyle = 'rgba(255,255,255,0.6)';
    ctx2d.fillText('© 1994 NEON GENESIS ARCADE · 16-BIT SURVIVAL HORROR · ENTER / CLICK TO PLAY', W / 2, H - 30);
  }

  function neonTitle(text, x, y, size, col1, col2) {
    ctx2d.textAlign = 'center';
    ctx2d.font = `bold ${size}px "Arial Black", "Courier New", sans-serif`;
    // drop shadow stack for chunky 16-bit look
    ctx2d.fillStyle = '#0a0010';
    for (let i = 6; i > 0; i--) ctx2d.fillText(text, x + i, y + i);
    ctx2d.lineWidth = 3;
    ctx2d.strokeStyle = col2 || col1;
    ctx2d.shadowColor = col1; ctx2d.shadowBlur = 24;
    ctx2d.strokeText(text, x, y);
    ctx2d.fillStyle = col1;
    ctx2d.fillText(text, x, y);
    ctx2d.shadowBlur = 0;
  }

  // ---------- CRT / scanline post fx ----------
  let scanCanvas = null;
  function applyCRT() {
    // scanlines
    ctx2d.save();
    ctx2d.globalAlpha = 0.10;
    ctx2d.fillStyle = '#000';
    for (let y = 0; y < H; y += 3) ctx2d.fillRect(0, y, W, 1);
    ctx2d.restore();
    // vignette
    const vg = ctx2d.createRadialGradient(W / 2, H / 2, Math.min(W, H) * 0.35, W / 2, H / 2, Math.max(W, H) * 0.75);
    vg.addColorStop(0, 'rgba(0,0,0,0)');
    vg.addColorStop(1, 'rgba(0,0,0,0.55)');
    ctx2d.fillStyle = vg;
    ctx2d.fillRect(0, 0, W, H);
  }

  // =====================================================================
  //  HELPERS
  // =====================================================================
  function pushMsg(text, col) { messages.push({ text, col: col || '#fff', life: 6 }); if (messages.length > 30) messages.shift(); }
  function floatText(x, y, text, col) { floatTexts.push({ x, y, text: String(text), col, life: 0.9 }); }
  var clamp = MazUtil.clamp;
  var lerp = MazUtil.lerp;
  function dist(a, b) { return Math.hypot(a.x - b.x, a.y - b.y); }
  function normAng(a) { while (a > Math.PI) a -= Math.PI * 2; while (a < -Math.PI) a += Math.PI * 2; return a; }

  // boot
  global.addEventListener('load', init);
})(typeof window !== 'undefined' ? window : this);
