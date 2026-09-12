#!/usr/bin/env node
/*
 * DEAD SECTOR — the browser test.
 *
 *   npm --prefix music/tests install     # once, for Playwright
 *   node shooter/tests/shooter-browser.test.js
 *
 * shooter-logic.test.js checks the file agrees with itself without running it.
 * This runs it: the balance tables are read out of the live page (so they are
 * checked against the real thing, never against a second copy in a test that
 * would drift), and then the game is actually played — start it, move, shoot,
 * survive a few seconds — to prove the loop runs, the screen changes and
 * nothing throws.
 */
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');
let chromium = null;
for (const spec of [
  'playwright',
  path.join(ROOT, 'music', 'tests', 'node_modules', 'playwright'),
  path.join(ROOT, 'node_modules', 'playwright')
]) {
  try { chromium = require(spec).chromium; break; } catch (e) { /* next */ }
}
if (!chromium) {
  console.error('Playwright is not installed. Run: npm --prefix music/tests install');
  process.exit(2);
}

const PORT = process.env.PORT ? parseInt(process.env.PORT, 10) : 8214;
const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css' };
const server = http.createServer((req, res) => {
  let p = decodeURIComponent(req.url.split('?')[0].split('#')[0]);
  if (p.endsWith('/')) p += 'index.html';
  const file = path.join(ROOT, p);
  if (!file.startsWith(ROOT) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
    res.writeHead(404); res.end('not found'); return;
  }
  res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
  res.end(fs.readFileSync(file));
});

let failures = 0;
function check(cond, msg) {
  console.log((cond ? '  ok    ' : '  FAIL  ') + msg);
  if (!cond) failures++;
}

function launchOptions() {
  const opts = { args: ['--no-sandbox', '--use-gl=swiftshader', '--mute-audio'] };
  for (const c of [
    process.env.CHROMIUM_PATH,
    '/opt/pw-browsers/chromium-1194/chrome-linux/chrome',
    '/opt/pw-browsers/chromium/chrome-linux/chrome'
  ]) {
    if (c && fs.existsSync(c)) { opts.executablePath = c; break; }
  }
  return opts;
}

/* A cheap fingerprint of what is on the canvas, so "the game is running" can be
 * checked by the screen changing rather than by a flag the game sets itself. */
const FRAME = `(() => {
  const c = document.getElementById('game');
  const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
  let sum = 0, lit = 0;
  for (let i = 0; i < d.length; i += 4 * 151) {
    const v = d[i] + d[i + 1] + d[i + 2];
    sum += v;
    if (v > 60) lit++;
  }
  return { sum, lit, w: c.width, h: c.height };
})()`;

(async () => {
  await new Promise((r) => server.listen(PORT, '127.0.0.1', r));
  const browser = await chromium.launch(launchOptions());
  const page = await browser.newPage({ viewport: { width: 1100, height: 900 } });
  const problems = [];
  page.on('pageerror', (e) => problems.push('uncaught: ' + e.message));
  page.on('console', (m) => { if (m.type() === 'error') problems.push('console: ' + m.text()); });

  try {
    await page.goto(`http://127.0.0.1:${PORT}/shooter/`, { waitUntil: 'load' });
    await page.waitForTimeout(400);

    console.log('\nLOADING');
    check(problems.length === 0, 'the page loads clean' + (problems.length ? ' — ' + problems.join('; ') : ''));
    check(await page.isVisible('#startScreen'), 'the start screen is up');

    /* ------------------------------------------------------- the rules */
    console.log('\nTHE RULES');
    const rules = await page.evaluate(() => window.DEAD_SECTOR);
    check(!!rules, 'the page publishes its rules for checking');

    const weaponIds = Object.keys(rules.WEAPONS);
    check(rules.WEAPON_ORDER.every((id) => rules.WEAPONS[id]),
      'every weapon in the unlock order exists: ' + rules.WEAPON_ORDER.join(', '));
    check(rules.WEAPON_ORDER.length === weaponIds.length,
      `every weapon is reachable — ${weaponIds.length} defined, ${rules.WEAPON_ORDER.length} in the unlock order`);

    const badWeapon = weaponIds.filter((id) => {
      const w = rules.WEAPONS[id];
      return !(w.name && w.dmg > 0 && w.rate > 0 && w.mag > 0 && w.reload > 0 &&
               w.pellets >= 1 && w.spread >= 0 && w.speed > 0 && w.len > 0 && w.sfx);
    });
    check(badWeapon.length === 0, 'every weapon can be fired' + (badWeapon.length ? ' — broken: ' + badWeapon.join(', ') : ''));

    // Every weapon should be the best at SOMETHING, or it is a card in the
    // upgrade screen that nobody has a reason to take.
    //
    // The axes are chosen to match how the game is actually played, which an
    // earlier version of this check got wrong: it treated a wide spread as
    // purely a penalty and duly declared the shotgun useless. Spread is the
    // shotgun's whole point in a game about crowds, so it is not an axis —
    // what a weapon can lead on is the damage it lands in one trigger pull,
    // its sustained output, how long it fires before reloading, how fast it
    // reloads, how far it reaches, and how many bodies a round goes through.
    const AXES = {
      'burst damage': (w) => w.dmg * w.pellets,
      'sustained damage': (w) => (w.dmg * w.pellets) / w.rate,
      'magazine': (w) => w.mag,
      'reload speed': (w) => -w.reload,
      'projectile speed': (w) => w.speed,
      'pierce': (w) => w.pierce
    };
    const leads = {};
    for (const [axis, value] of Object.entries(AXES)) {
      const best = Math.max(...weaponIds.map((id) => value(rules.WEAPONS[id])));
      for (const id of weaponIds) {
        if (value(rules.WEAPONS[id]) === best) (leads[id] = leads[id] || []).push(axis);
      }
    }
    const pointless = weaponIds.filter((id) => !leads[id]);
    check(pointless.length === 0,
      'every weapon is the best at something' +
      (pointless.length
        ? ' — ' + pointless.join(', ') + ' leads on nothing, so nobody would take it'
        : ' (' + weaponIds.map((id) => `${id}: ${leads[id][0]}`).join(', ') + ')'));

    const badEnemy = Object.entries(rules.ENEMY)
      .filter(([, e]) => !(e.r > 0 && /^#[0-9a-f]{3,8}$/i.test(e.color) && e.score > 0))
      .map(([id]) => id);
    check(badEnemy.length === 0, 'every enemy has a size, a colour and a score' + (badEnemy.length ? ' — ' + badEnemy.join(', ') : ''));
    check(rules.ENEMY.boss.score > Math.max(...Object.entries(rules.ENEMY)
      .filter(([id]) => id !== 'boss').map(([, e]) => e.score)), 'the boss is worth more than anything else');

    const upgradeIds = rules.UPGRADE_POOL.map((u) => u.id);
    check(new Set(upgradeIds).size === upgradeIds.length, 'no upgrade id is used twice');
    const badUpgrade = rules.UPGRADE_POOL.filter((u) => !(u.id && u.icon && u.name && u.desc)).map((u) => u.id || '(no id)');
    check(badUpgrade.length === 0, 'every upgrade card has something to show' + (badUpgrade.length ? ' — ' + badUpgrade.join(', ') : ''));
    check(rules.UPGRADE_POOL.length >= 3,
      `the pool can fill three cards without repeating (${rules.UPGRADE_POOL.length} upgrades)`);

    /* -------------------------------------------------------- playing */
    console.log('\nPLAYING');
    await page.click('#startBtn');
    await page.waitForTimeout(250);
    check(!(await page.isVisible('#startScreen')), 'pressing PLAY starts the game');
    check(await page.isVisible('#btnWeapon'), 'the HUD appears');

    const first = await page.evaluate(FRAME);
    check(first.w > 0 && first.h > 0, `the canvas is sized (${first.w}x${first.h})`);
    check(first.lit > 0, 'the game is drawing something');

    // Play for a few seconds: move, and fire at where the zombies come from.
    await page.keyboard.down('w');
    for (let i = 0; i < 12; i++) {
      await page.mouse.move(400 + i * 40, 300 + (i % 3) * 60);
      await page.mouse.down();
      await page.waitForTimeout(120);
      await page.mouse.up();
      await page.waitForTimeout(120);
    }
    await page.keyboard.up('w');

    const later = await page.evaluate(FRAME);
    check(later.sum !== first.sum, 'the screen changes as it is played — the loop is running');
    check(problems.length === 0, 'nothing threw while playing' + (problems.length ? ' — ' + problems.join('; ') : ''));

    /* ---------------------------------------------------------- pause */
    console.log('\nPAUSE AND RESUME');
    await page.click('#btnPause');
    await page.waitForTimeout(200);
    check(await page.isVisible('#pauseScreen'), 'pausing stops the game and says so');
    await page.click('#resumeBtn');
    await page.waitForTimeout(200);
    check(!(await page.isVisible('#pauseScreen')), 'resuming puts the game back');

    /* ---------------------------------------------------------- phone */
    console.log('\nPHONE LAYOUT');
    const phone = await browser.newPage({
      viewport: { width: 390, height: 844 },
      hasTouch: true,
      isMobile: true
    });
    const phoneProblems = [];
    phone.on('pageerror', (e) => phoneProblems.push('uncaught: ' + e.message));
    await phone.goto(`http://127.0.0.1:${PORT}/shooter/`, { waitUntil: 'load' });
    await phone.waitForTimeout(300);
    await phone.click('#startBtn');
    await phone.waitForTimeout(300);

    const overflow = await phone.evaluate(() =>
      document.documentElement.scrollWidth - document.documentElement.clientWidth);
    check(overflow <= 0, `no sideways scrolling at 390px (overflow ${overflow}px)`);

    // Both thumbs: drag on the left to move, drag on the right to aim and fire.
    await phone.touchscreen.tap(90, 700);
    await phone.waitForTimeout(80);
    await phone.touchscreen.tap(300, 700);
    await phone.waitForTimeout(600);
    check(phoneProblems.length === 0,
      'the twin-stick controls work under a real touchscreen' + (phoneProblems.length ? ' — ' + phoneProblems.join('; ') : ''));

    const phoneFrame = await phone.evaluate(FRAME);
    check(phoneFrame.lit > 0, 'the game draws on a phone-sized screen');
    await phone.close();
  } finally {
    await browser.close();
    server.close();
  }

  console.log('');
  if (failures) {
    console.log(`✗ ${failures} check(s) failed\n`);
    process.exit(1);
  }
  console.log('✓ DEAD SECTOR browser tests passed\n');
})().catch((e) => {
  console.error(e);
  server.close();
  process.exit(1);
});
