#!/usr/bin/env node
/*
 * Tests for the inventory.
 *
 * The end-to-end ones build a miniature repository in os.tmpdir() — one app,
 * two engine headers, two browser projects — and point the tool at it with
 * INVENTORY_ROOT. A fixture is used rather than the real repo on purpose: a
 * test that asserts "161 apps" starts failing the day Cody writes the 162nd,
 * which teaches everyone to ignore it. These assert on *behaviour* instead, so
 * they stay true however much the repo grows.
 *
 *   node tools/inventory/tests/inventory.test.mjs
 */

import { mkdtempSync, mkdirSync, writeFileSync, readFileSync, rmSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { tmpdir } from 'node:os';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert';

import { parseHeaderComment, extractDocComment } from '../lib/scan-cpp.mjs';
import { declaredFunctions, extractFunctions, duplicateHelpers } from '../lib/scan-web.mjs';
import { validatePairings } from '../lib/synergy.mjs';
import { buildModel } from '../lib/model.mjs';
import { watchRoots } from '../lib/watch.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const MAIN = join(HERE, '..', 'main.mjs');

let passed = 0;
let failed = 0;
const tmpRoots = [];

function test(name, fn) {
  try {
    const r = fn();
    if (r && typeof r.then === 'function') {
      return r.then(
        () => { passed += 1; console.log(`  ok  ${name}`); },
        (err) => { failed += 1; console.error(`  FAIL ${name}\n       ${err.message}`); }
      );
    }
    passed += 1;
    console.log(`  ok  ${name}`);
  } catch (err) {
    failed += 1;
    console.error(`  FAIL ${name}\n       ${err.message}`);
  }
  return Promise.resolve();
}

function write(root, rel, text) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, text);
}

/**
 * A miniature repo with one of each thing the scanners look for, wired so that
 * some checks pass and some fail — a fixture where everything passes proves
 * nothing about the failure paths.
 */
function makeFixture() {
  const root = mkdtempSync(join(tmpdir(), 'maz-inventory-'));
  tmpRoots.push(root);

  // Two apps: `shown` is complete, `hidden` is registered but has no headless
  // mode, no golden, and demonstrates nothing.
  write(root, 'CMakeLists.txt', 'add_subdirectory(apps/shown)\nadd_subdirectory(apps/hidden)\n');
  write(root, 'apps/shown/CMakeLists.txt', 'add_executable(shown main.cpp)\n');
  write(root, 'apps/shown/main.cpp',
    '// Maz Engine — "SHOWN" — demonstrates the widget module end to end, with a moving camera\n' +
    '// and a HUD. Run --headless / --frames N for CI.\n' +
    '#include "maz/Engine.hpp"\n' +
    'int main(int argc, char** argv) {\n' +
    '  core::AppConfig cfg = core::parseArgs(argc, argv);\n' +
    '  platform::Window w; wc.headless = cfg.headless; w.init(wc);\n' +
    '  game::Widget widget; return 0;\n}\n');
  write(root, 'apps/hidden/CMakeLists.txt', 'add_executable(hidden main.cpp)\n');
  write(root, 'apps/hidden/main.cpp',
    '#include "maz/Engine.hpp"\nint main() { platform::Window w; w.init({}); return 0; }\n');
  write(root, 'tests/golden/shown.png', 'not really a png');

  // A command-line tool that lives in apps/ but never opens a window — the
  // mobile exporter is the real one. It cannot be asked for a headless mode it
  // already has by definition, nor for a screenshot of a frame it never draws.
  write(root, 'CMakeLists.txt',
    'add_subdirectory(apps/shown)\nadd_subdirectory(apps/hidden)\nadd_subdirectory(apps/tool)\n');
  write(root, 'apps/tool/CMakeLists.txt', 'add_executable(tool main.cpp)\n');
  write(root, 'apps/tool/main.cpp',
    '// Maz Engine — "TOOL" — a command-line exporter that stages files on disk and never\n' +
    '// opens a window at all; --selftest runs it end to end in CI.\n' +
    '#include "maz/Engine.hpp"\nint main() { game::Widget w; return 0; }\n');

  // Two engine modules: Widget is demoed and tested, Ghost is neither.
  write(root, 'engine/include/maz/game/Widget.hpp',
    '#pragma once\n#include <vector>\n\nnamespace maz::game {\n\n' +
    '// maz::game WIDGET — the thing this fixture exists to catalogue, described at enough\n' +
    '// length to clear the doc-comment threshold.\nstruct Widget {};\n}\n');
  write(root, 'engine/include/maz/game/Ghost.hpp',
    '#pragma once\nnamespace maz::game { struct Ghost {}; }\n');
  // Tested but demonstrated by nothing — the repo's single most common state,
  // and the one the "no app shows them" opportunity is about.
  write(root, 'engine/include/maz/game/Hidden.hpp',
    '#pragma once\nnamespace maz::game {\n\n' +
    '// maz::game HIDDEN — a finished, tested feature that no sample app puts on screen,\n' +
    '// which is exactly the gap this fixture exists to reproduce.\nstruct Hidden {};\n}\n');
  write(root, 'tests/unit/main.cpp',
    'int main() { maz::game::Widget w; maz::game::Hidden h; return 0; }\n');

  // Two browser projects sharing a helper; only one has tests.
  write(root, 'shared/projects.js',
    "(function (g) { var P = [\n" +
    "  { id: 'alpha', name: 'ALPHA', path: 'alpha/', blurb: 'First.', badges: [] },\n" +
    "  { id: 'beta', name: 'BETA', path: 'beta/', blurb: 'Second.', badges: [] }\n" +
    "];\nif (typeof module === 'object' && module.exports) module.exports = { PROJECTS: P };\n" +
    "g.MAZ_PROJECTS = P; })(globalThis);\n");
  write(root, 'shared/exchange.json', JSON.stringify({
    publishes: { 'alpha/thing': { project: 'alpha', summary: 'A thing.', files: ['alpha/js/app.js'] } },
    consumes: []
  }, null, 2));
  write(root, 'alpha/index.html', '<script src="../shared/maz-nav.js"></script><script src="js/app.js"></script>');
  write(root, 'alpha/js/app.js', 'function clamp(v, a, b) { return v; }\nfunction onlyAlpha() {}\n');
  write(root, 'alpha/README.md', '# Alpha\n');
  write(root, 'alpha/tests/alpha.test.mjs', 'console.log("ok");\n');
  write(root, 'beta/index.html', '<script src="../shared/maz-nav.js"></script><script src="js/app.js"></script>');
  write(root, 'beta/js/app.js', 'function clamp(v, a, b) { return v; }\nfunction onlyBeta() {}\n');
  write(root, 'beta/README.md', '# Beta\n');

  write(root, 'index.html', '<script src="shared/projects.js"></script>');
  write(root, 'docs/LINKED.md', '# Linked\n');
  write(root, 'docs/ORPHAN.md', '# Orphan\n');
  write(root, 'README.md', 'See [linked](docs/LINKED.md).\n');
  write(root, 'scripts/check-thing.mjs', '/* check-thing — proves a thing. */\n');
  write(root, 'tools/build.sh', '#!/bin/sh\n# Builds the thing for release.\n');
  return root;
}

function inventory(root, args) {
  return execFileSync(process.execPath, [MAIN, ...args], {
    env: { ...process.env, INVENTORY_ROOT: root },
    encoding: 'utf8'
  });
}

console.log('inventory');

await test('parseHeaderComment pulls the quoted title and the blurb off an app', () => {
  const { title, blurb } = parseHeaderComment(
    '// Maz Engine — "PONG" — a classic two-paddle game built from the 2D primitives.\n' +
    '// Left paddle is the player.\n\n#include "maz/Engine.hpp"\n');
  assert.strictEqual(title, 'PONG');
  assert.ok(blurb.startsWith('a classic two-paddle game'), `blurb was: ${blurb}`);
  assert.ok(!blurb.includes('Maz Engine'), 'the boilerplate prefix should be stripped');
});

await test('extractDocComment finds a header comment that sits below the includes', () => {
  // The bug this guards: engine headers open with #pragma once and a block of
  // #includes, and many put the doc comment inside the namespace. Reading only
  // the top of the file reported every one of them as undocumented.
  const doc = extractDocComment(
    '#pragma once\n\n#include <cmath>\n\nnamespace maz::audio {\n\n' +
    '// maz::audio MUSIC THEORY — note/pitch conversion.\n' +
    '// Header-only and deterministic.\nstruct T {};\n');
  assert.ok(doc.includes('MUSIC THEORY'), `got: ${doc}`);
  assert.ok(doc.includes('deterministic'), 'the whole comment run should be joined');
});

await test('extractDocComment stops at the end of the first comment run', () => {
  const doc = extractDocComment('#pragma once\n// First block.\nstruct A {};\n// Second block.\n');
  assert.strictEqual(doc, 'First block.');
});

await test('declaredFunctions sees both function and arrow declarations', () => {
  const fns = declaredFunctions('function a() {}\nconst b = () => 1;\nvar c = function () {};\n');
  assert.deepStrictEqual([...fns].sort(), ['a', 'b', 'c']);
});

await test('extractFunctions returns each function with its body', () => {
  const fns = extractFunctions('function add(a, b) { return a + b; }\nfunction wrap() { if (1) { return 2; } }\n');
  assert.deepStrictEqual(fns.map((f) => f.name), ['add', 'wrap']);
  assert.strictEqual(fns[0].body, 'return $0 + $1;'.replace('$0', 'a').replace('$1', 'b'));
  // Nested braces must not end the body early.
  assert.ok(fns[1].body.includes('return 2;'), fns[1].body);
});

await test('duplicateHelpers reports the same code in two projects', () => {
  const clamp = 'function clamp(v, lo, hi) { return v < lo ? lo : v > hi ? hi : v; }';
  const clampRenamed = 'function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }';
  const dupes = duplicateHelpers([
    { slug: 'alpha', definitions: extractFunctions(clamp).map((f) => ({ ...f, file: 'a.js' })) },
    { slug: 'beta', definitions: extractFunctions(clampRenamed).map((f) => ({ ...f, file: 'b.js' })) }
  ]);
  assert.deepStrictEqual(dupes.map((d) => d.name), ['clamp'],
    'the same function with different parameter names is still the same function');
  assert.deepStrictEqual(dupes[0].projects, ['alpha', 'beta']);
});

await test('duplicateHelpers does NOT report two different functions that share a name', () => {
  // The bug this guards: `noise` generates text variation in SCRIPT FORGE and a
  // burst of audio static in ZOMBOID. Matching on the name alone reported them
  // as duplicated code and would have sent someone to merge them.
  const textNoise = 'function noise(key, count) { return key.repeat(count); }';
  const audioNoise = 'function noise(dur, gain) { return new AudioBuffer(dur * gain); }';
  const dupes = duplicateHelpers([
    { slug: 'film', definitions: extractFunctions(textNoise).map((f) => ({ ...f, file: 'a.js' })) },
    { slug: 'zomboid', definitions: extractFunctions(audioNoise).map((f) => ({ ...f, file: 'b.js' })) }
  ]);
  assert.deepStrictEqual(dupes, []);
});

await test('validatePairings flags a pairing that names an artifact which does not exist', () => {
  const model = { all: [{ id: 'web:alpha' }] };
  const { pairings, stale } = validatePairings(model, [
    { id: 'real', from: ['web:alpha'], to: ['tool:inventory'] },
    { id: 'ghost', from: ['web:alpha'], to: ['web:deleted'] }
  ]);
  assert.deepStrictEqual(pairings.map((p) => p.id), ['real']);
  assert.deepStrictEqual(stale.map((p) => p.id), ['ghost']);
  assert.deepStrictEqual(stale[0].missing, ['web:deleted']);
});

const ROOT = makeFixture();

await test('a module named by an app is demonstrated; one named by nothing is not', async () => {
  const m = await buildModel(ROOT);
  const widget = m.modules.find((x) => x.name === 'Widget');
  const ghost = m.modules.find((x) => x.name === 'Ghost');
  assert.deepStrictEqual(widget.demoedBy, ['apps/shown/main.cpp', 'apps/tool/main.cpp']);
  assert.deepStrictEqual(widget.testedBy, ['tests/unit/main.cpp']);
  assert.deepStrictEqual(ghost.demoedBy, []);
  assert.deepStrictEqual(ghost.testedBy, []);
});

await test('a module with no test still counts as covered if a golden app demonstrates it', async () => {
  // The Vulkan renderer, the font atlas and the debug overlay cannot be
  // exercised by a headless unit test. What covers them is the golden_images
  // ctest rendering an app that uses them and comparing the frame, and the
  // checker has to know that or it reports the engine's most-exercised code as
  // untested. `shown` has a golden and names Widget; nothing else does.
  const m = await buildModel(ROOT);
  const widget = m.modules.find((x) => x.name === 'Widget');
  const hidden = m.modules.find((x) => x.name === 'Hidden');
  assert.deepStrictEqual(widget.goldenBy, ['app:shown']);
  assert.deepStrictEqual(hidden.goldenBy, [], 'a module no golden app demonstrates has no golden coverage');
  assert.strictEqual(widget.checks.find((c) => c.id === 'tested').ok, true);
});

await test('the app that demonstrates nothing fails exactly the checks it should', async () => {
  const m = await buildModel(ROOT);
  const hidden = m.apps.find((a) => a.name === 'hidden');
  const shown = m.apps.find((a) => a.name === 'shown');
  const failing = (a) => a.checks.filter((c) => !c.ok).map((c) => c.id).sort();
  assert.deepStrictEqual(failing(hidden), ['documented', 'golden', 'headless', 'uses-engine']);
  assert.deepStrictEqual(failing(shown), []);
});

await test('a command-line tool is not asked for a headless mode or a screenshot', async () => {
  // The bug this guards, and it was the report's largest finding: "runs
  // headless" tested for the literal string "--headless" in the source, and
  // named 38 apps that could all run headless already — they call parseArgs,
  // which owns the flag, and pass cfg.headless through. All it measured was
  // whether a comment happened to mention the flag. And apps/ holds one thing
  // that is not a windowed app at all: a command-line exporter with no frame
  // to capture and nothing to run headlessly, because it never opens a window.
  const m = await buildModel(ROOT);
  const tool = m.apps.find((a) => a.name === 'tool');
  const shown = m.apps.find((a) => a.name === 'shown');
  assert.strictEqual(tool.windowed, false, 'an app with no platform::Window is a command-line tool');
  assert.strictEqual(shown.windowed, true);
  assert.deepStrictEqual(tool.checks.filter((c) => !c.ok).map((c) => c.id), [],
    'the command-line tool was asked for something it cannot have');
  assert.strictEqual(shown.headless, true, 'wiring cfg.headless through is what makes an app headless');
});

await test('an unregistered app is reported as not built', async () => {
  write(ROOT, 'apps/stray/CMakeLists.txt', 'add_executable(stray main.cpp)\n');
  write(ROOT, 'apps/stray/main.cpp', '// Maz Engine — "STRAY" — never wired into the build at all.\nint main(){}\n');
  const m = await buildModel(ROOT);
  const stray = m.apps.find((a) => a.name === 'stray');
  const registered = stray.checks.find((c) => c.id === 'registered');
  assert.strictEqual(registered.ok, false);
  assert.ok(registered.fix.includes('add_subdirectory(apps/stray)'), 'the fix should name the exact line to add');
  rmSync(join(ROOT, 'apps/stray'), { recursive: true, force: true });
});

await test('a browser project with no tests and no exchange entry fails both checks', async () => {
  const m = await buildModel(ROOT);
  const beta = m.webApps.find((w) => w.slug === 'beta');
  const alpha = m.webApps.find((w) => w.slug === 'alpha');
  assert.deepStrictEqual(beta.checks.filter((c) => !c.ok).map((c) => c.id).sort(), ['exchange', 'tests']);
  assert.deepStrictEqual(alpha.checks.filter((c) => !c.ok).map((c) => c.id), []);
});

await test('a doc nothing links to is found, and a linked one is not', async () => {
  const m = await buildModel(ROOT);
  const orphan = m.docs.find((d) => d.name === 'ORPHAN.md');
  const linked = m.docs.find((d) => d.name === 'LINKED.md');
  assert.strictEqual(orphan.linkedFrom, false);
  assert.strictEqual(linked.linkedFrom, true);
});

await test('engine-module failures are grouped into one task, not one task each', async () => {
  const m = await buildModel(ROOT);
  const perModule = m.queue.filter((t) => t.id.startsWith('engine:'));
  const grouped = m.queue.filter((t) => t.id.startsWith('group:engine-module:'));
  assert.strictEqual(perModule.length, 0, 'no task should name a single engine module');
  assert.ok(grouped.length > 0, 'the shared failures should collapse into grouped tasks');
  const demoed = grouped.find((t) => t.check === 'demoed');
  assert.ok(
    demoed.members.some((m) => m.id === 'engine:game/Hidden' && m.path && m.fix),
    'each member should carry the path and the fix a consumer needs to act on it'
  );
});

await test('an opportunity that explains a grouped check is merged into it, not queued twice', async () => {
  const m = await buildModel(ROOT);
  const titles = m.queue.map((t) => t.title);
  const dupes = titles.filter((t, i) => titles.indexOf(t) !== i);
  assert.deepStrictEqual(dupes, [], `the queue repeats itself: ${dupes.join(' | ')}`);

  // The "no demo app" opportunity carries the reasoning; the grouped task
  // carries the member list. After merging, one task has both.
  const demoed = m.queue.find((t) => t.id === 'group:engine-module:demoed');
  assert.ok(demoed.title.includes('no app shows'), `title lost the reasoning: ${demoed.title}`);
  assert.ok(demoed.detail.includes('Hidden'), 'detail lost the member list');
});

await test('an opportunity with no group to merge into is still queued', async () => {
  const m = await buildModel(ROOT);
  // Browser projects are itemised — one task each — so nothing absorbs the
  // "projects have no tests" opportunity and it must survive on its own.
  assert.ok(
    m.queue.some((t) => t.id === 'opportunity:browser-projects-with-no-tests'),
    'the unabsorbed opportunity was silently dropped'
  );
});

await test('every queued task carries the paths it touches, so the Forge can zone-check it', async () => {
  const m = await buildModel(ROOT);
  const scoped = m.queue.filter((t) => t.kind !== 'opportunity');
  assert.ok(scoped.length > 0);
  for (const t of scoped) {
    assert.ok(Array.isArray(t.paths) && t.paths.length > 0, `task ${t.id} has no paths`);
  }
});

// The fixture's artifacts are not the real repo's, so every declared pairing in
// synergy.mjs is stale here. That is the honest answer for this repo, and it
// means --check always fails in the fixture — so these tests read WHY it failed
// rather than only that it did.
function checkOutput(root) {
  try {
    return { ok: true, text: inventory(root, ['--check']) };
  } catch (err) {
    return { ok: false, text: String(err.stdout || '') + String(err.stderr || '') };
  }
}

await test('--check does not complain the report is out of date right after --write', () => {
  inventory(ROOT, ['--write']);
  const r = checkOutput(ROOT);
  assert.ok(!r.text.includes('out of date'), `freshly written report reported stale:\n${r.text}`);
});

await test('--check fails and says so when the report has drifted from the repo', () => {
  inventory(ROOT, ['--write']);
  const md = join(ROOT, 'docs', 'INVENTORY.md');
  writeFileSync(md, readFileSync(md, 'utf8') + '\nhand-edited\n');
  const r = checkOutput(ROOT);
  assert.strictEqual(r.ok, false, '--check should exit non-zero when the report is stale');
  assert.ok(r.text.includes('docs/INVENTORY.md is out of date'), r.text);
  assert.ok(r.text.includes('--write'), 'the failure should name the command that fixes it');
  inventory(ROOT, ['--write']);
});

await test('--check fails on a pairing that names an artifact which no longer exists', () => {
  const r = checkOutput(ROOT);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('no longer exist'), r.text);
  assert.ok(r.text.includes('synergy.mjs'), 'the failure should name the file to edit');
});

await test('the report is deterministic — no clock, no run id', () => {
  const first = inventory(ROOT, []);
  const second = inventory(ROOT, []);
  assert.strictEqual(first, second, 'two runs over an unchanged repo must render identically');
});

await test('--json emits the queue with a schema version', () => {
  const data = JSON.parse(inventory(ROOT, ['--json']));
  assert.strictEqual(data.schema, 1);
  assert.ok(Array.isArray(data.queue) && data.queue.length > 0);
  assert.ok(data.stats.totalArtifacts > 0);
  for (const t of data.queue) {
    assert.ok(t.id && t.title && t.detail, `malformed task: ${JSON.stringify(t)}`);
    assert.ok([1, 2, 3].includes(t.priority), `bad priority on ${t.id}`);
  }
});

await test('the scanners survive a repo that is missing everything', async () => {
  const empty = mkdtempSync(join(tmpdir(), 'maz-inventory-empty-'));
  tmpRoots.push(empty);
  const m = await buildModel(empty);
  assert.strictEqual(m.stats.totalArtifacts, 0);
  assert.strictEqual(m.apps.length, 0);
  // Every declared pairing names artifacts, and none of them exist here, so all
  // of them are stale — which is the honest answer, not a crash.
  assert.ok(m.stalePairings.length > 0);
});

/* ------------------------------------------ working beside other sessions */

await test('writing twice in a row writes nothing the second time', () => {
  // Not an optimisation. --watch watches a tree the outputs live inside, so an
  // unconditional write is a loop; and another session regenerating the same
  // bytes must not look like a change to this one.
  inventory(ROOT, ['--write']);
  const second = inventory(ROOT, ['--write']);
  assert.ok(second.includes('already up to date'), second);
});

await test('--status says which tree it describes and whether the catalogue matches', () => {
  inventory(ROOT, ['--write']);
  const out = inventory(ROOT, ['--status']);
  assert.ok(out.includes('catalogue'), out);
  assert.ok(out.includes('matches this tree'), `a freshly written catalogue reported stale:\n${out}`);
  assert.ok(out.includes('artifacts'), 'the summary should be there too');
});

await test('--status reports a stale catalogue rather than a fresh one', () => {
  inventory(ROOT, ['--write']);
  write(ROOT, 'apps/late/CMakeLists.txt', 'add_executable(late main.cpp)\n');
  write(ROOT, 'apps/late/main.cpp', '// Maz Engine — "LATE" — added after the catalogue was written.\nint main(){}\n');
  const out = inventory(ROOT, ['--status']);
  assert.ok(out.includes('STALE'), `a changed tree was reported as current:\n${out}`);
  assert.ok(out.includes('--write'), 'it should name the command that fixes it');
  rmSync(join(ROOT, 'apps/late'), { recursive: true, force: true });
  inventory(ROOT, ['--write']);
});

await test('the watched roots cover every project, including ones added later', async () => {
  // The point of deriving them: a browser project or Python tool another
  // session adds is watched without anyone editing watch.mjs.
  const m = await buildModel(ROOT);
  const roots = watchRoots(ROOT, m);
  assert.ok(roots.includes('apps'), 'apps/ must be watched');
  assert.ok(roots.includes('engine'), 'engine/ must be watched');
  assert.ok(roots.includes('docs'), 'docs/ must be watched — a doc link is catalogued');
  for (const w of m.webApps) {
    assert.ok(roots.includes(w.path), `${w.path} is a project but is not watched`);
  }
  assert.strictEqual(new Set(roots).size, roots.length, 'a root is listed twice');
  assert.ok(!roots.includes('nope'), 'a root that does not exist should be dropped');
});

for (const r of tmpRoots) rmSync(r, { recursive: true, force: true });

console.log(`\n${passed} passed, ${failed} failed`);
process.exitCode = failed ? 1 : 0;
