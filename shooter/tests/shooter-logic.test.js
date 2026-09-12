/*
 * DEAD SECTOR — static checks on the single file.
 *
 *   node shooter/tests/shooter-logic.test.js
 *
 * No dependencies and no browser. DEAD SECTOR is one self-contained HTML file
 * by design — that is what it is for, a game you can email to someone — so
 * there are no modules to require. What can still be checked without running
 * it is whether the file agrees with itself: every element the script reaches
 * for exists in the markup above it, every id it defines is unique, and the
 * page carries what a phone-first game needs to work on a phone.
 *
 * The rules themselves (weapons, enemies, upgrades) and the game actually
 * playing are checked in shooter/tests/shooter-browser.test.js, which runs it
 * in a real Chromium.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const PAGE = path.join(__dirname, '..', 'index.html');
const html = fs.readFileSync(PAGE, 'utf8');
const script = (html.match(/<script>([\s\S]*?)<\/script>/) || [])[1] || '';

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

/** Every `id="..."` in the markup. */
function declaredIds() {
  const ids = [];
  const re = /\bid="([^"]+)"/g;
  let m;
  while ((m = re.exec(html)) !== null) ids.push(m[1]);
  return ids;
}

console.log('\nTHE FILE AGREES WITH ITSELF');

test('the page carries its game script inline', () => {
  assert(script.length > 5000, 'the inline script is missing or tiny — the game would not run');
});

test('every element the script reaches for exists in the markup', () => {
  // The check that earns this file. `document.getElementById('upCards')` with a
  // typo returns null, and the game throws the first time it opens the upgrade
  // screen — a crash a person hits mid-run, three waves in.
  const declared = new Set(declaredIds());
  const wanted = new Set();
  const re = /getElementById\(\s*['"]([^'"]+)['"]\s*\)/g;
  let m;
  while ((m = re.exec(script)) !== null) wanted.add(m[1]);
  assert(wanted.size > 0, 'the script looks up no elements at all — is it still there?');
  const missing = [...wanted].filter((id) => !declared.has(id));
  eq(missing.join(', '), '', 'ids the script asks for but the page never defines');
});

test('no id is defined twice', () => {
  const seen = new Set();
  const dupes = declaredIds().filter((id) => (seen.has(id) ? true : (seen.add(id), false)));
  eq([...new Set(dupes)].join(', '), '', 'duplicate ids — getElementById would pick one and ignore the rest');
});

test('every querySelector the script uses names a class the page defines', () => {
  const re = /querySelector(?:All)?\(\s*['"]\.([A-Za-z][\w-]*)['"]\s*\)/g;
  const missing = [];
  let m;
  while ((m = re.exec(script)) !== null) {
    if (!new RegExp(`class="[^"]*\\b${m[1]}\\b`).test(html) && !html.includes(`.${m[1]}`)) {
      missing.push(m[1]);
    }
  }
  eq(missing.join(', '), '', 'classes selected but never defined');
});

test('it really is self-contained — no external scripts but the shared nav', () => {
  // "A single self-contained HTML file" is the project's whole identity, and
  // the one thing it is allowed to load is the arcade nav every page carries.
  const srcs = [...html.matchAll(/<script[^>]*\bsrc="([^"]+)"/g)].map((x) => x[1]);
  const foreign = srcs.filter((s) => s !== '../shared/maz-nav.js');
  eq(foreign.join(', '), '', 'external scripts that break the single-file promise');
  assert(!/<link[^>]*rel="stylesheet"/.test(html), 'an external stylesheet was added');
});

console.log('\nPHONE FIRST');

test('the viewport is set up for a full-screen phone game', () => {
  const meta = (html.match(/<meta\s+name="viewport"[^>]*content="([^"]*)"/) || [])[1] || '';
  assert(meta, 'there is no viewport meta tag at all');
  assert(/width=device-width/.test(meta), 'the page will not fit a phone: ' + meta);
  assert(/viewport-fit=cover/.test(meta), 'the notch area is not covered: ' + meta);
});

test('touch input is wired up, not just mouse and keyboard', () => {
  // The badge on this project says "Phone / Touch / Single file". The first two
  // are only true if the page listens for fingers.
  for (const event of ['touchstart', 'touchmove', 'touchend']) {
    assert(script.includes(event), `nothing listens for ${event}`);
  }
});

test('the page blocks the browser gestures that ruin a twin-stick game', () => {
  assert(/touch-action\s*:\s*none/.test(html), 'without touch-action:none a drag scrolls the page instead of aiming');
  assert(/user-select\s*:\s*none/.test(html), 'without user-select:none a fast tap selects the HUD text');
});

console.log('\nTHE TEST HANDLE');

test('the rules are published for the browser test to check', () => {
  assert(/window\.DEAD_SECTOR\s*=/.test(script), 'the test handle is gone — shooter-browser.test.js cannot check the balance tables');
  for (const key of ['WEAPONS', 'WEAPON_ORDER', 'ENEMY', 'UPGRADE_POOL']) {
    assert(new RegExp(`${key}\\s*:`).test(script.slice(script.indexOf('window.DEAD_SECTOR'))),
      `${key} is not published`);
  }
});

console.log('');
if (failures.length) {
  console.log('✗ ' + failures.length + ' failed, ' + passed + ' passed\n');
  failures.forEach((f) => console.log('  - ' + f));
  process.exit(1);
}
console.log('✓ ' + passed + ' tests passed\n');
