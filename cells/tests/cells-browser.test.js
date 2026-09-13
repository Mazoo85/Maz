/*
 * NEON CELLS — the game, driven in a real browser.
 *
 * The logic suite proves the maths and the level generator; this one proves the
 * game runs: the page boots with no errors, a run starts, the player actually
 * moves and swings and hurts things, every biome in the run (boss arenas
 * included) builds and draws, every menu the game can put in front of you opens
 * and draws, and dying ends the run properly.
 *
 * A game is exactly the kind of thing that can pass a unit test and still throw
 * on frame two, so this walks real frames with real input.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node cells/tests/cells-browser.test.js
 *
 * Set CHROMIUM_PATH to point at a Chromium build if Playwright can't find one.
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');

let chromium = null;
for (const spec of [
  'playwright',
  'playwright-core',
  path.join(ROOT, 'music', 'tests', 'node_modules', 'playwright'),
  path.join(ROOT, 'node_modules', 'playwright')
]) {
  try {
    chromium = require(spec).chromium;
    break;
  } catch (e) {
    /* try the next one */
  }
}
if (!chromium) {
  console.error('Playwright is not installed. Run: npm --prefix music/tests install');
  process.exit(2);
}

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8215;
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.md': 'text/plain' };

const server = http.createServer((req, res) => {
  let p = decodeURIComponent(req.url.split('?')[0].split('#')[0]);
  if (p.endsWith('/')) p += 'index.html';
  const file = path.join(ROOT, p);
  if (!file.startsWith(ROOT) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404);
    res.end('not found');
    return;
  }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
  res.end(fs.readFileSync(file));
});

let failures = 0;
function check(cond, msg) {
  console.log((cond ? '  ok   ' : '  FAIL ') + msg);
  if (!cond) failures++;
}

function launchOptions() {
  const opts = { args: ['--no-sandbox', '--use-gl=swiftshader', '--mute-audio'] };
  for (const c of [
    process.env.CHROMIUM_PATH,
    '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
    '/opt/pw-browsers/chromium/chrome-linux/chrome'
  ]) {
    if (c && fs.existsSync(c)) {
      opts.executablePath = c;
      break;
    }
  }
  return opts;
}

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const base = `http://127.0.0.1:${PORT}`;
  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 1280, height: 720 } });

  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => {
    if (m.type() === 'error') problems.push('console: ' + m.text());
  });
  page.on('requestfailed', (r) => problems.push('failed request: ' + r.url()));

  /* Every frame of the game is driven through requestAnimationFrame, so this
   * just waits real time and lets it run. */
  const play = (ms) => page.waitForTimeout(ms);

  try {
    console.log('\nBOOT');
    await page.goto(base + '/cells/', { waitUntil: 'load' });
    await play(700);

    check(problems.length === 0, 'boots with no errors' + (problems.length ? ' — ' + problems.join('; ') : ''));
    check((await page.title()).indexOf('NEON CELLS') === 0, 'page is NEON CELLS');
    check(await page.evaluate(() => !!window.NEON_CELLS), 'the game exposes its test hooks');
    check(await page.evaluate(() => window.NEON_CELLS.state() === 'TITLE'), 'starts on the title screen');
    check(
      await page.evaluate(() => {
        const c = document.getElementById('game');
        return c.width > 100 && c.height > 60;
      }),
      'canvas is sized to the window'
    );
    check(
      await page.evaluate(() => {
        const c = document.getElementById('game');
        const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
        let lit = 0;
        for (let i = 0; i < d.length; i += 4000) if (d[i] + d[i + 1] + d[i + 2] > 40) lit++;
        return lit > 20;
      }),
      'title screen actually draws something'
    );

    console.log('\nA RUN');
    await page.evaluate(() => window.NEON_CELLS.startRun());
    await play(400);
    const started = await page.evaluate(() => {
      const w = window.NEON_CELLS.world();
      return {
        state: window.NEON_CELLS.state(),
        enemies: w.enemies.length,
        hp: w.player.hp,
        maxHp: w.player.maxHp,
        weapon: w.player.weapons[0] && w.player.weapons[0].name,
        tiles: w.level.w + 'x' + w.level.h,
        biome: w.level.biome.id
      };
    });
    check(started.state === 'INTRO' || started.state === 'PLAY', 'a run starts in the first biome (' + started.biome + ')');
    check(started.enemies > 0, 'the level is populated (' + started.enemies + ' enemies)');
    check(started.hp === started.maxHp && started.hp > 40, 'the player starts at full health (' + started.hp + ')');
    check(!!started.weapon, 'the player starts holding something (' + started.weapon + ')');

    /* --- actually play: run right, jump, roll, swing */
    console.log('\nPLAYING');
    const before = await page.evaluate(() => {
      const p = window.NEON_CELLS.world().player;
      return { x: p.x, y: p.y };
    });
    await page.keyboard.down('d');
    await play(900);
    await page.keyboard.press('Space');
    await play(300);
    await page.keyboard.press('j');
    await play(200);
    await page.keyboard.press('Shift');
    await play(400);
    await page.keyboard.up('d');
    await play(200);
    const after = await page.evaluate(() => {
      const p = window.NEON_CELLS.world().player;
      return { x: p.x, y: p.y, dead: p.dead, frames: window.NEON_CELLS.world().time };
    });
    check(Math.abs(after.x - before.x) > 40, 'holding a direction moves the player (' + Math.round(after.x - before.x) + 'px)');
    check(after.frames > 1, 'the world clock is running (' + after.frames.toFixed(1) + 's simulated)');
    check(!after.dead, 'the player survived a walk in the first room');
    check(problems.length === 0, 'no errors while playing' + (problems.length ? ' — ' + problems.join('; ') : ''));

    /* --- swinging at something must hurt it */
    console.log('\nCOMBAT');
    const combat = await page.evaluate(async () => {
      const NC = window.NEON_CELLS;
      const w = NC.world();
      const p = w.player;
      const e = w.enemies.find((x) => !x.dead);
      /* stand next to it and swing until it notices */
      p.x = e.x - 16;
      p.y = e.y;
      p.facing = 1;
      const hpBefore = e.hp;
      for (let i = 0; i < 40; i++) {
        NC.press('KeyJ');
        await new Promise((r) => requestAnimationFrame(r));
      }
      return { hpBefore: hpBefore, hpAfter: e.hp, dead: e.dead, kills: w.kills };
    });
    check(combat.hpAfter < combat.hpBefore || combat.dead, 'a swing damages an enemy (' + combat.hpBefore + ' → ' + combat.hpAfter + ')');

    /* --- skills, where the run has one */
    const skills = await page.evaluate(async () => {
      const NC = window.NEON_CELLS;
      const w = NC.world();
      w.player.skills[0] = window.CELLS_CONTENT.SKILL.grenade;
      w.player.skillCd[0] = 0;
      NC.press('KeyU');
      for (let i = 0; i < 8; i++) await new Promise((r) => requestAnimationFrame(r));
      return { projectiles: w.projectiles.length, cd: w.player.skillCd[0] };
    });
    check(skills.cd > 0, 'using a skill puts it on cooldown');

    /* --- every biome has to build and draw, boss arenas included */
    console.log('\nEVERY BIOME');
    for (let i = 0; i < 8; i++) {
      const info = await page.evaluate(async (index) => {
        const NC = window.NEON_CELLS;
        NC.enterBiome(index);
        NC.setState('PLAY');
        for (let f = 0; f < 30; f++) await new Promise((r) => requestAnimationFrame(r));
        const w = NC.world();
        return {
          id: w.level.biome.id,
          ok: !!w.level.ok,
          boss: !!w.boss,
          enemies: w.enemies.length,
          hasExit: !!w.level.exit,
          dead: w.player.dead
        };
      }, i);
      check(
        info.ok && info.hasExit && !info.dead,
        'biome ' + (i + 1) + ' (' + info.id + ') builds, draws and has a way out' +
          (info.boss ? ' — boss arena' : ' — ' + info.enemies + ' enemies')
      );
    }
    check(problems.length === 0, 'no errors across every biome' + (problems.length ? ' — ' + problems.join('; ') : ''));

    /* --- the bosses have to fight, not just stand there */
    console.log('\nBOSSES');
    for (const index of [5, 7]) {
      const fight = await page.evaluate(async (idx) => {
        const NC = window.NEON_CELLS;
        NC.enterBiome(idx);
        NC.setState('PLAY');
        const w = NC.world();
        w.player.x = w.boss.x - 60;
        /* this check is about the boss's moveset, not about surviving it */
        w.player.maxHp = 100000;
        w.player.hp = 100000;
        const states = new Set();
        for (let f = 0; f < 400; f++) {
          states.add(w.boss.state);
          await new Promise((r) => requestAnimationFrame(r));
        }
        return { id: w.boss.id, states: [...states], hp: w.boss.hp, alive: !w.boss.dead };
      }, index);
      check(
        fight.states.length > 2,
        fight.id + ' works through its moves (' + fight.states.join(', ') + ')'
      );
    }

    /* --- killing the boss has to open the door home */
    const bossDeath = await page.evaluate(async () => {
      const NC = window.NEON_CELLS;
      NC.enterBiome(5);
      NC.setState('PLAY');
      const w = NC.world();
      const lockedBefore = !!w.level.exit.locked;
      window.CELLS_ENTITIES.damageEnemy(w, w.boss, 99999, {});
      for (let f = 0; f < 10; f++) await new Promise((r) => requestAnimationFrame(r));
      return { lockedBefore: lockedBefore, lockedAfter: !!w.level.exit.locked, drops: w.drops.length };
    });
    check(bossDeath.lockedBefore && !bossDeath.lockedAfter, 'the exit unseals when the boss dies');
    check(bossDeath.drops > 0, 'the boss drops cells');

    /* --- every screen the game can show, opened the way the game opens it */
    console.log('\nSCREENS');

    /* a biome with a shop and scrolls in it, so the real doors can be used */
    await page.evaluate(async () => {
      window.NEON_CELLS.startRun();
      window.NEON_CELLS.enterBiome(1);
      window.NEON_CELLS.setState('PLAY');
      for (let f = 0; f < 4; f++) await new Promise((r) => requestAnimationFrame(r));
    });

    /* Walking up to a thing and pressing E is the only way in, so that is what
     * this does — a screen that cannot be reached that way is broken. */
    async function openViaObject(type) {
      return page.evaluate(async (wanted) => {
        const NC = window.NEON_CELLS;
        const w = NC.world();
        const o = w.level.objects.find((x) => x.type === wanted && !x.taken && !x.opened);
        if (!o) return { reached: false, state: NC.state() };
        w.player.x = o.pos.x;
        w.player.y = o.pos.y;
        w.player.vx = 0;
        await new Promise((r) => requestAnimationFrame(r));   // let findNearby see it
        NC.press('KeyE');
        for (let f = 0; f < 4; f++) await new Promise((r) => requestAnimationFrame(r));
        return { reached: true, state: NC.state(), near: NC.nearby() && NC.nearby().type };
      }, type);
    }

    function litSamples() {
      return page.evaluate(() => {
        const c = document.getElementById('game');
        const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
        let lit = 0;
        for (let i = 0; i < d.length; i += 400) if (d[i] + d[i + 1] + d[i + 2] > 30) lit++;
        return lit;
      });
    }

    for (const [type, expected] of [['scroll', 'SCROLL'], ['shop', 'SHOP'], ['collector', 'COLLECTOR']]) {
      const opened = await openViaObject(type);
      const lit = await litSamples();
      check(opened.state === expected, 'pressing E at a ' + type + ' opens ' + expected + ' (got ' + opened.state + ')');
      check(lit > 60, expected + ' draws (' + lit + ' lit samples)');
      await page.evaluate(() => window.NEON_CELLS.setState('PLAY'));
    }

    /* the mutation offer is what a finished biome hands you */
    const mutate = await page.evaluate(async () => {
      const NC = window.NEON_CELLS;
      NC.nextBiome();
      for (let f = 0; f < 4; f++) await new Promise((r) => requestAnimationFrame(r));
      return { state: NC.state() };
    });
    check(mutate.state === 'MUTATE', 'clearing a biome offers a mutation (got ' + mutate.state + ')');
    check((await litSamples()) > 60, 'MUTATE draws');

    /* taking one has to carry it into the next biome */
    const taken = await page.evaluate(async () => {
      const NC = window.NEON_CELLS;
      NC.press('Space');
      for (let f = 0; f < 6; f++) await new Promise((r) => requestAnimationFrame(r));
      const w = NC.world();
      return { state: NC.state(), mutations: w.player.mutations.length, biome: w.level.biome.id };
    });
    check(taken.mutations === 1, 'the mutation is kept (' + taken.mutations + ')');
    check(taken.state === 'INTRO' || taken.state === 'PLAY', 'and the next biome starts (' + taken.biome + ')');

    /* the screens that are just screens */
    for (const target of ['PAUSE', 'DEAD', 'VICTORY', 'HELP', 'TITLE']) {
      await page.evaluate(async (s) => {
        window.NEON_CELLS.setState(s);
        for (let f = 0; f < 4; f++) await new Promise((r) => requestAnimationFrame(r));
      }, target);
      const lit = await litSamples();
      check(lit > 60, target + ' draws (' + lit + ' lit samples)');
    }
    check(problems.length === 0, 'no errors opening every screen' + (problems.length ? ' — ' + problems.join('; ') : ''));

    /* --- death ends the run and banks what it should */
    console.log('\nDEATH');
    const death = await page.evaluate(async () => {
      const NC = window.NEON_CELLS;
      NC.enterBiome(0);
      NC.setState('PLAY');
      const w = NC.world();
      w.player.cells = 40;
      window.CELLS_ENTITIES.hurtPlayer(w, 99999, 'test', null);
      for (let f = 0; f < 10; f++) await new Promise((r) => requestAnimationFrame(r));
      const saved = JSON.parse(localStorage.getItem('neon-cells-save-v1') || '{}');
      return { state: NC.state(), banked: NC.run().banked, runs: saved.runs, cells: saved.cells };
    });
    check(death.state === 'DEAD', 'running out of health ends the run');
    check(death.banked === 20, 'half the carried cells are banked (' + death.banked + ' of 40)');
    check(death.runs >= 1, 'the run is recorded in the save (' + death.runs + ' runs)');

    /* --- a controller has to drive the whole game, menus included */
    console.log('\nON A CONTROLLER');
    const pad = await browser.newPage({ viewport: { width: 1280, height: 720 } });
    const padProblems = [];
    pad.on('pageerror', (e) => padProblems.push('uncaught: ' + e.message));
    pad.on('console', (m) => {
      if (m.type() === 'error') padProblems.push('console: ' + m.text());
    });

    /* Chromium has no real pad attached, so stand one up before the page loads.
     * Standard mapping: 0=A 1=B 2=X 3=Y 4=LB 5=RB 6=LT 7=RT 9=START,
     * 12-15 = d-pad, axes[0]/[1] = left stick. */
    await pad.addInitScript(() => {
      const fake = {
        id: 'Test Controller (STANDARD GAMEPAD)',
        index: 0,
        connected: true,
        mapping: 'standard',
        timestamp: 0,
        axes: [0, 0, 0, 0],
        buttons: Array.from({ length: 17 }, () => ({ pressed: false, touched: false, value: 0 }))
      };
      window.__pad = fake;
      navigator.getGamepads = () => [fake];
      window.__padPress = (i, down) => {
        fake.buttons[i].pressed = !!down;
        fake.buttons[i].value = down ? 1 : 0;
        fake.timestamp = performance.now();
      };
      window.__padStick = (x, y) => {
        fake.axes[0] = x;
        fake.axes[1] = y;
        fake.timestamp = performance.now();
      };
    });

    await pad.goto(base + '/cells/', { waitUntil: 'load' });
    await pad.waitForTimeout(400);

    /* A on the title starts a run, which also proves menus read the pad */
    await pad.evaluate(async () => {
      window.__padPress(0, true);
      await new Promise((r) => requestAnimationFrame(r));
      await new Promise((r) => requestAnimationFrame(r));
      window.__padPress(0, false);
      for (let f = 0; f < 4; f++) await new Promise((r) => requestAnimationFrame(r));
    });
    const padStarted = await pad.evaluate(() => window.NEON_CELLS.state());
    check(padStarted === 'INTRO' || padStarted === 'PLAY', 'A on the title starts a run (state ' + padStarted + ')');

    await pad.evaluate(() => window.NEON_CELLS.setState('PLAY'));

    /* the left stick moves, and so does the d-pad */
    const stick = await pad.evaluate(async () => {
      const p = window.NEON_CELLS.world().player;
      const x0 = p.x;
      window.__padStick(1, 0);
      for (let f = 0; f < 40; f++) await new Promise((r) => requestAnimationFrame(r));
      const moved = p.x - x0;
      window.__padStick(0, 0);
      const x1 = p.x;
      window.__padPress(14, true);           // d-pad left
      for (let f = 0; f < 40; f++) await new Promise((r) => requestAnimationFrame(r));
      window.__padPress(14, false);
      return { stick: moved, dpad: p.x - x1 };
    });
    check(stick.stick > 30, 'the left stick moves the player right (' + Math.round(stick.stick) + 'px)');
    check(stick.dpad < -20, 'the d-pad moves the player left (' + Math.round(stick.dpad) + 'px)');

    /* A small stick nudge inside the dead zone must not creep.
     *
     * The player is held invulnerable for the measurement, which is not
     * tidiness: this runs inside a live level, and an enemy landing a hit
     * knocks the player sideways. That is the game working. Measured over 150
     * probes, every drift this check ever reported came with a hit taken and
     * nine health lost, and every probe with no hit moved the player 0.0px —
     * so without this the check reports the combat system as a fault in the
     * stick, at whatever rate the level happens to put an enemy in reach. */
    const deadzone = await pad.evaluate(async () => {
      const p = window.NEON_CELLS.world().player;
      const invuln = p.invuln;
      p.vx = 0;
      p.invuln = 999;
      const x0 = p.x;
      const hp0 = p.hp;
      window.__padStick(0.2, 0);
      for (let f = 0; f < 30; f++) await new Promise((r) => requestAnimationFrame(r));
      window.__padStick(0, 0);
      const moved = Math.abs(p.x - x0);
      const hurt = hp0 - p.hp;
      p.invuln = invuln;
      return { moved: moved, hurt: hurt };
    });
    check(deadzone.moved < 3, 'a stick inside the dead zone does not drift (' +
      deadzone.moved.toFixed(1) + 'px' +
      (deadzone.hurt > 0 ? `, but it took ${deadzone.hurt} damage mid-measurement — ` +
        'that is knockback, not drift' : '') + ')');

    /* A jumps, B rolls, X swings */
    const moves = await pad.evaluate(async (buttons) => {
      const NC = window.NEON_CELLS;
      const p = NC.world().player;
      const out = {};
      for (const [name, index] of Object.entries(buttons)) {
        /* put the player back on the floor between tries */
        for (let f = 0; f < 40 && !p.onGround; f++) await new Promise((r) => requestAnimationFrame(r));
        p.vy = 0;
        p.rollTimer = 0;
        p.rollCd = 0;
        p.attackTimer = 0;
        p.swing = null;
        window.__padPress(index, true);
        let seen = false;
        for (let f = 0; f < 8; f++) {
          await new Promise((r) => requestAnimationFrame(r));
          if (name === 'jump' && p.vy < -100) seen = true;
          if (name === 'roll' && p.rollTimer > 0) seen = true;
          if (name === 'attack' && p.swing) seen = true;
        }
        window.__padPress(index, false);
        out[name] = seen;
        await new Promise((r) => requestAnimationFrame(r));
      }
      return out;
    }, { jump: 0, roll: 1, attack: 2 });
    check(moves.jump, 'A jumps');
    check(moves.roll, 'B rolls');
    check(moves.attack, 'X swings the left-hand weapon');

    /* the skill buttons fire the skills */
    const padSkill = await pad.evaluate(async () => {
      const w = window.NEON_CELLS.world();
      w.player.skills[0] = window.CELLS_CONTENT.SKILL.grenade;
      w.player.skillCd[0] = 0;
      window.__padPress(4, true);            // LB
      for (let f = 0; f < 6; f++) await new Promise((r) => requestAnimationFrame(r));
      window.__padPress(4, false);
      return w.player.skillCd[0];
    });
    check(padSkill > 0, 'LB uses the first skill');

    /* START pauses, and the pad drives the pause menu */
    const paused = await pad.evaluate(async () => {
      const NC = window.NEON_CELLS;
      window.__padPress(9, true);
      await new Promise((r) => requestAnimationFrame(r));
      window.__padPress(9, false);
      for (let f = 0; f < 3; f++) await new Promise((r) => requestAnimationFrame(r));
      const state = NC.state();
      window.__padPress(13, true);           // d-pad down moves the highlight
      await new Promise((r) => requestAnimationFrame(r));
      window.__padPress(13, false);
      for (let f = 0; f < 3; f++) await new Promise((r) => requestAnimationFrame(r));
      return { state: state, still: NC.state() };
    });
    check(paused.state === 'PAUSE', 'START pauses the game');
    check(paused.still === 'PAUSE', 'and the d-pad moves through the menu without leaving it');

    /* the on-screen hints follow the controller */
    const hints = await pad.evaluate(async () => {
      const NC = window.NEON_CELLS;
      NC.setState('PLAY');
      for (let f = 0; f < 3; f++) await new Promise((r) => requestAnimationFrame(r));
      /* read the hint the HUD would print for the left-hand weapon */
      return NC.hint('atk1') + '/' + NC.hint('skill1') + '/' + NC.hint('interact');
    });
    check(hints === 'X/LB/LT', 'the HUD labels switch to controller buttons (' + hints + ')');

    check(padProblems.length === 0, 'no errors on a controller' + (padProblems.length ? ' — ' + padProblems.join('; ') : ''));
    await pad.close();

    /* --- a phone-shaped window must lay out and take touches */
    console.log('\nON A PHONE');
    const phone = await browser.newPage({
      viewport: { width: 844, height: 390 },
      hasTouch: true,
      isMobile: true
    });
    const phoneProblems = [];
    phone.on('pageerror', (e) => phoneProblems.push('uncaught: ' + e.message));
    phone.on('console', (m) => {
      if (m.type() === 'error') phoneProblems.push('console: ' + m.text());
    });
    await phone.goto(base + '/cells/', { waitUntil: 'load' });
    await phone.waitForTimeout(500);
    await phone.evaluate(() => window.NEON_CELLS.startRun());
    await phone.waitForTimeout(300);
    await phone.touchscreen.tap(700, 330);      // the attack button corner
    await phone.waitForTimeout(200);
    await phone.touchscreen.tap(150, 300);      // the move zone
    await phone.waitForTimeout(400);
    /* a phone has no Esc key, so pausing has to be reachable by thumb */
    await phone.waitForFunction(() => window.NEON_CELLS.state() === 'PLAY', null, { timeout: 5000 });
    const pauseButton = await phone.evaluate(() => {
      const b = window.NEON_CELLS.touchButtons().find((x) => x.name === 'pause');
      const c = document.getElementById('game');
      return b ? { x: (b.x / c.width) * window.innerWidth, y: (b.y / c.height) * window.innerHeight } : null;
    });
    check(!!pauseButton, 'there is a pause button on a touch screen');
    if (pauseButton) {
      await phone.touchscreen.tap(pauseButton.x, pauseButton.y);
      await phone.waitForFunction(() => window.NEON_CELLS.state() === 'PAUSE', null, { timeout: 5000 })
        .catch(function () { /* reported by the check below */ });
      check(await phone.evaluate(() => window.NEON_CELLS.state()) === 'PAUSE', 'tapping it pauses the game');

      /* Resuming is watched over many frames rather than sampled once after a
       * fixed wait. It used to be sampled, and so it only caught the bug it was
       * there to catch when the timing happened to land right: the game would
       * resume and then pause itself straight back one frame later, because the
       * press that resumed it was still sitting there to be read again. Half a
       * second of frames sees that; a single look at 250ms is a coin toss. */
      async function watchFrames(ms) {
        return phone.evaluate((limit) => new Promise(function (done) {
          const seen = [];
          const started = performance.now();
          (function watch() {
            seen.push(window.NEON_CELLS.state());
            if (performance.now() - started > limit) { done(seen); return; }
            requestAnimationFrame(watch);
          })();
        }), ms);
      }

      /* Pausing and resuming several times over, because the failure this
       * guards against is a race and one attempt is one roll of the dice.
       * Somebody playing taps this button dozens of times a session, so once
       * in a while is still a game that stops responding to its own pause
       * button. */
      let bounce = null;
      let stuck = 0;
      for (let go = 0; go < 5 && !bounce; go++) {
        await phone.touchscreen.tap(pauseButton.x, pauseButton.y);
        const seen = await watchFrames(320);
        const ended = seen[seen.length - 1];
        if (ended !== 'PLAY') {
          if (seen.indexOf('PLAY') >= 0) {
            bounce = seen.filter((v, i, a) => v !== a[i - 1]).join(' -> ');
          } else { stuck++; }
        }
        if (go < 4) {
          await phone.touchscreen.tap(pauseButton.x, pauseButton.y);
          await phone.waitForFunction(() => window.NEON_CELLS.state() === 'PAUSE',
            null, { timeout: 5000 }).catch(function () {});
        }
      }
      check(!bounce && !stuck, 'and tapping it again resumes, five times over' +
        (bounce ? ' — it resumed and then paused itself again: ' + bounce : '') +
        (stuck ? ` — ${stuck} tap(s) did not resume at all` : ''));
    }

    check(phoneProblems.length === 0, 'plays on a phone-sized screen' + (phoneProblems.length ? ' — ' + phoneProblems.join('; ') : ''));
    check(
      await phone.evaluate(() => {
        const c = document.getElementById('game');
        return c.width <= 844 && c.width > 300;
      }),
      'the view is scaled sensibly on a small screen'
    );
    await phone.close();
  } catch (e) {
    check(false, 'the suite itself threw — ' + e.message);
  } finally {
    await browser.close();
    server.close();
  }

  console.log('');
  if (failures) {
    console.error(`✖ ${failures} check${failures === 1 ? '' : 's'} failed\n`);
    process.exit(1);
  }
  console.log('✓ NEON CELLS runs\n');
})();
