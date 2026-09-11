#!/usr/bin/env node
/*
 * Tests for check-exchange — one per failure rule.
 *
 * Each builds a throwaway repo in os.tmpdir(), runs the checker against it
 * with EXCHANGE_ROOT, and asserts the checker fails FOR THAT REASON rather
 * than merely failing. A test that only asserts a non-zero exit would pass
 * for any bug at all.
 *
 *   node scripts/tests/check-exchange.test.mjs
 */
'use strict';

import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { tmpdir } from 'node:os';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert';

const HERE = dirname(fileURLToPath(import.meta.url));
const CHECKER = join(HERE, '..', 'check-exchange.mjs');

let passed = 0;
let failed = 0;
const tmpRoots = [];

function test(name, fn) {
  try {
    fn();
    console.log('  ✓ ' + name);
    passed++;
  } catch (err) {
    console.error('  ✗ ' + name + '\n    ' + err.message);
    failed++;
  }
}

/* Build a repo with the given files. `files` maps repo-relative path to text.
 * `exchange` is the object written to shared/exchange.json. */
function repo(files, exchange, projectIds = ['music', 'film']) {
  const root = mkdtempSync(join(tmpdir(), 'exchange-'));
  tmpRoots.push(root);
  const write = (rel, text) => {
    mkdirSync(join(root, dirname(rel)), { recursive: true });
    writeFileSync(join(root, rel), text);
  };
  write('shared/exchange.json', JSON.stringify(exchange, null, 2));
  const entries = projectIds
    .map((id) => `{ id: '${id}', name: '${id}', kind: 'play', path: '${id}/', ` +
                 `tag: 't', accent: '#fff', blurb: 'b' }`)
    .join(',\n    ');
  write('shared/projects.js', `export const PROJECTS = [\n    ${entries}\n];\n`);
  for (const [rel, text] of Object.entries(files)) write(rel, text);
  return root;
}

/* Run the checker. Returns { code, output }. */
function check(root) {
  try {
    const out = execFileSync('node', [CHECKER], {
      env: { ...process.env, EXCHANGE_ROOT: root },
      encoding: 'utf8'
    });
    return { code: 0, output: out };
  } catch (err) {
    return { code: err.status ?? 1, output: (err.stdout ?? '') + (err.stderr ?? '') };
  }
}

function assertFailsWith(root, needle) {
  const { code, output } = check(root);
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(output.includes(needle),
    'failed for the wrong reason.\n  wanted: ' + needle + '\n  got:\n' + output);
}

const GOOD_PAGE =
  '<!doctype html><html><head></head><body>\n' +
  '<script src="../music/js/theory.js"></script>\n' +
  '<script src="../music/js/genres.js"></script>\n' +
  '<script src="../music/js/synth.js"></script>\n' +
  '<script src="../music/js/composer.js"></script>\n' +
  '<script src="../music/js/engine.js"></script>\n' +
  '</body></html>\n';

const MUSIC_FILES = [
  'music/js/theory.js', 'music/js/genres.js', 'music/js/synth.js',
  'music/js/composer.js', 'music/js/engine.js'
];

function goodExchange() {
  return {
    publishes: {
      'music/composer': { project: 'music', summary: 's', files: [...MUSIC_FILES] }
    },
    consumes: [
      { project: 'film', id: 'music/composer', via: 'script',
        page: 'film/index.html', contract: 'film/tests/film-logic.test.js' }
    ]
  };
}

/* Every file a valid fixture needs to exist on disk. */
function goodFiles() {
  const files = { 'film/index.html': GOOD_PAGE, 'film/tests/film-logic.test.js': '// contract\n' };
  for (const f of MUSIC_FILES) files[f] = '// ' + f + '\n';
  return files;
}

console.log('\nCHECK-EXCHANGE');

test('a correct declaration passes', () => {
  const { code, output } = check(repo(goodFiles(), goodExchange()));
  assert.strictEqual(code, 0, 'a valid repo should pass:\n' + output);
});

test('consuming an id nobody publishes fails, naming the id', () => {
  const ex = goodExchange();
  ex.consumes[0].id = 'music/nonexistent';
  assertFailsWith(repo(goodFiles(), ex), 'music/nonexistent');
});

test('a project not in shared/projects.js fails, naming the project', () => {
  const ex = goodExchange();
  ex.consumes[0].project = 'ghost';
  assertFailsWith(repo(goodFiles(), ex, ['music', 'film']), 'ghost');
});

test('a published file that does not exist fails, naming the file', () => {
  const ex = goodExchange();
  ex.publishes['music/composer'].files.push('music/js/missing.js');
  assertFailsWith(repo(goodFiles(), ex), 'music/js/missing.js');
});

test('a contract file that does not exist fails, naming the file', () => {
  const ex = goodExchange();
  ex.consumes[0].contract = 'film/tests/nope.test.js';
  assertFailsWith(repo(goodFiles(), ex), 'film/tests/nope.test.js');
});

test('a project consuming its own published id fails', () => {
  const ex = goodExchange();
  ex.consumes[0].project = 'music';
  assertFailsWith(repo(goodFiles(), ex), 'its own');
});

test('a malformed exchange.json fails with a named error, not a crash', () => {
  const root = repo(goodFiles(), goodExchange());
  writeFileSync(join(root, 'shared/exchange.json'), '{ not json');
  assertFailsWith(root, 'shared/exchange.json');
});

test('publishes missing entirely fails rather than throwing', () => {
  const { code, output } = check(repo(goodFiles(), { consumes: [] }));
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(!output.includes('TypeError'),
    'checker should not crash with TypeError. got:\n' + output);
  assert.ok(output.includes('publishes'),
    'failed for the wrong reason.\n  wanted: publishes\n  got:\n' + output);
});

test('a non-string contract fails rather than crashing path.join', () => {
  const ex = goodExchange();
  ex.consumes[0].contract = 123;
  const { code, output } = check(repo(goodFiles(), ex));
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(!output.includes('TypeError'),
    'checker should not crash with TypeError. got:\n' + output);
  assert.ok(output.includes('names contract 123'),
    'failed for the wrong reason.\n  wanted: names contract 123\n  got:\n' + output);
});

test('a broken shared/projects.js fails rather than throwing', () => {
  const root = repo(goodFiles(), goodExchange());
  writeFileSync(join(root, 'shared/projects.js'), 'export const PROJECTS = (;\n');
  const { code, output } = check(root);
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(!output.includes('TypeError') && !/^SyntaxError/m.test(output),
    'checker should not crash uncaught. got:\n' + output);
  assert.ok(output.includes('shared/projects.js failed to load'),
    'failed for the wrong reason.\n  wanted: shared/projects.js failed to load\n  got:\n' + output);
});

test('a non-array PROJECTS in shared/projects.js fails, naming the problem', () => {
  const root = repo(goodFiles(), goodExchange());
  writeFileSync(join(root, 'shared/projects.js'), "export const PROJECTS = 'nope';\n");
  const { code, output } = check(root);
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(!output.includes('TypeError'),
    'checker should not crash with TypeError. got:\n' + output);
  assert.ok(output.includes('PROJECTS must be an array'),
    'failed for the wrong reason.\n  wanted: PROJECTS must be an array\n  got:\n' + output);
});

test('a page loading the files out of the declared order fails', () => {
  const files = goodFiles();
  files['film/index.html'] = GOOD_PAGE
    .replace('<script src="../music/js/synth.js"></script>\n', '')
    .replace('<script src="../music/js/engine.js"></script>\n',
             '<script src="../music/js/engine.js"></script>\n' +
             '<script src="../music/js/synth.js"></script>\n');
  assertFailsWith(repo(files, goodExchange()), 'order');
});

test('a page missing one of the declared files fails, naming it', () => {
  // The missing file is deliberately the FIRST declared one, not the third.
  // If the "missing" check is deleted, the order check that runs in its
  // place sees loaded.indexOf(theory.js) === -1 for the first entry, and
  // "-1 < every later real index" reads as already-ordered — so the checker
  // would exit 0 (a silent pass on genuinely missing coupling) rather than
  // merely failing for a different reason. Asserting the specific "does not
  // load" phrasing plus exit 1 catches that silent pass; a substring that
  // also appears in the "wrong order" message would not.
  const files = goodFiles();
  files['film/index.html'] = GOOD_PAGE
    .replace('<script src="../music/js/theory.js"></script>\n', '');
  assertFailsWith(repo(files, goodExchange()), 'does not load music/js/theory.js');
});

test('an undeclared cross-project script tag fails, naming the page', () => {
  // The rule this whole checker exists for: a new coupling that nobody wrote
  // down must turn CI red rather than pass quietly.
  const files = goodFiles();
  files['madlibs/index.html'] =
    '<!doctype html><html><body>\n' +
    '<script src="../music/js/synth.js"></script>\n' +
    '</body></html>\n';
  const root = repo(files, goodExchange(), ['music', 'film', 'madlibs']);
  assertFailsWith(root, 'madlibs/index.html');
});

test('loading shared/ is not coupling and does not fail', () => {
  const files = goodFiles();
  files['madlibs/index.html'] =
    '<!doctype html><html><body>\n' +
    '<script src="../shared/maz-nav.js"></script>\n' +
    '</body></html>\n';
  files['shared/maz-nav.js'] = '// nav\n';
  const root = repo(files, goodExchange(), ['music', 'film', 'madlibs']);
  const { code, output } = check(root);
  assert.strictEqual(code, 0, 'shared/ must not count as a project dependency:\n' + output);
});

test('a page may load its own files without declaring anything', () => {
  // Uses "../film/js/app.js" rather than "js/app.js": a same-project ref
  // with no "../" never matches CROSS_REF at all, so it never reaches the
  // `ref.project === owner` guard this test is meant to exercise. This form
  // resolves to the page's own project and does reach that guard.
  const files = goodFiles();
  files['film/index.html'] = GOOD_PAGE.replace(
    '</body>', '<script src="../film/js/app.js"></script>\n</body>');
  files['film/js/app.js'] = '// app\n';
  const { code, output } = check(repo(files, goodExchange()));
  assert.strictEqual(code, 0, 'same-project scripts are not coupling:\n' + output);
});

test('a reference more than one level up resolves to the real project, not ".."', () => {
  // A page nested two directories deep referencing "../../music/..." used to
  // be pattern-matched as project "..", which matches neither INFRASTRUCTURE
  // nor the page's own owner — so it was always reported as undeclared
  // coupling, even when correctly declared. Resolving the reference relative
  // to the page's directory into a repo-relative path fixes that.
  const files = {
    'film/sub/page.html':
      '<!doctype html><html><body>\n' +
      '<script src="../../music/js/synth.js"></script>\n' +
      '</body></html>\n',
    'music/js/synth.js': '// synth\n',
    'film/tests/film-logic.test.js': '// contract\n'
  };
  const ex = {
    publishes: {
      'music/synth': { project: 'music', summary: 's', files: ['music/js/synth.js'] }
    },
    consumes: [
      { project: 'film', id: 'music/synth', via: 'script',
        page: 'film/sub/page.html', contract: 'film/tests/film-logic.test.js' }
    ]
  };
  const { code, output } = check(repo(files, ex));
  assert.strictEqual(code, 0,
    'a correctly-resolved ../../ reference to a declared project should pass:\n' + output);
});

test('a query string on a cross-project reference is stripped before comparing', () => {
  const files = goodFiles();
  files['film/index.html'] = GOOD_PAGE.replace(
    '<script src="../music/js/synth.js"></script>',
    '<script src="../music/js/synth.js?v=2"></script>');
  const { code, output } = check(repo(files, goodExchange()));
  assert.strictEqual(code, 0,
    'a cache-busting query string should not break the declared-file match:\n' + output);
});

test('a non-array "files" on a published id fails rather than crashing .filter', () => {
  const ex = goodExchange();
  ex.publishes['music/composer'].files = 'oops-not-an-array';
  const { code, output } = check(repo(goodFiles(), ex));
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(!output.includes('TypeError'),
    'checker should not crash with TypeError. got:\n' + output);
  assert.ok(output.includes('declares no files'),
    'failed for the wrong reason.\n  wanted: declares no files\n  got:\n' + output);
});

test('a non-string "page" on a consumes entry fails rather than crashing path.join', () => {
  const ex = goodExchange();
  ex.consumes[0].page = 42;
  const { code, output } = check(repo(goodFiles(), ex));
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(!output.includes('TypeError'),
    'checker should not crash with TypeError. got:\n' + output);
  assert.ok(output.includes('has a "page" that is not a string'),
    'failed for the wrong reason.\n  wanted: has a "page" that is not a string\n  got:\n' + output);
});

test('a plain <a href> and <img src> to another project are not coupling, even nested', () => {
  // A bare directory href like "../music/" can never be declared away — it
  // can't appear in a publisher's `files` list in any spelling that also
  // satisfies the load-order rule. If the tag filter regressed to matching
  // bare src|href on ANY tag (not just <script src> / <link href>), this
  // would false-fail: it deliberately puts the reference several directories
  // deep, past "../", so a filter that only special-cased same-project refs
  // (rather than tag name) could not accidentally pass this by luck.
  const files = goodFiles();
  files['film/index.html'] = GOOD_PAGE.replace(
    '</body>',
    '<a href="../music/">Make a song in SONG FORGE</a>\n' +
    '<img src="../music/poster.png" alt="poster">\n</body>');
  const { code, output } = check(repo(files, goodExchange()));
  assert.strictEqual(code, 0,
    'plain navigation/content to another project must not be flagged as coupling:\n' + output);
});

test('a depth-0 page reaching another project with no "../" is still caught', () => {
  // The old CROSS_REF was anchored on a literal "../", so a page at repo
  // depth 0 (the hub index.html) reaching another project needs no "../" at
  // all — "music/js/synth.js" resolves correctly from the root without it —
  // and that reference never matched. Real, undeclared coupling sailed
  // through the rule that exists to catch it. Nothing declares this page's
  // use of music/js/synth.js, so it must fail and name both.
  const files = goodFiles();
  files['index.html'] =
    '<!doctype html><html><body>\n' +
    '<script src="music/js/synth.js"></script>\n' +
    '</body></html>\n';
  const { code, output } = check(repo(files, goodExchange()));
  assert.strictEqual(code, 1, 'expected the checker to fail, it exited 0:\n' + output);
  assert.ok(output.includes('index.html') && output.includes('music/js/synth.js'),
    'failed for the wrong reason, or did not name both the page and the file.\n  got:\n' + output);
});

test('a published file outside its own project fails, naming the id, file and project', () => {
  // Nothing required a publisher's `files` to live inside its own `project`.
  // Here music/composer (project "music") reaches into madlibs/ — a fully
  // declared, CI-green cross-project dependency the Forge cannot see at all,
  // since it keys the dependency graph on `project`.
  const ex = goodExchange();
  ex.publishes['music/composer'].files.push('madlibs/js/generator.js');
  const files = goodFiles();
  files['madlibs/js/generator.js'] = '// generator\n';
  assertFailsWith(repo(files, ex, ['music', 'film', 'madlibs']),
    'names madlibs/js/generator.js, which is outside its own project "music"');
});

test('a numeric consumes id is rejected rather than silently coerced', () => {
  // JS's own-property lookup would coerce publishes[7] to publishes["7"],
  // but the Python reader on the Forge side requires a string id and rejects
  // a numeric one outright — CI green here, the Forge dead every night.
  const ex = {
    publishes: {
      '7': { project: 'music', summary: 's', files: [...MUSIC_FILES] }
    },
    consumes: [
      { project: 'film', id: 7, via: 'script',
        page: 'film/index.html', contract: 'film/tests/film-logic.test.js' }
    ]
  };
  assertFailsWith(repo(goodFiles(), ex), 'is not a string');
});

test('a consumes id of "constructor" does not resolve via the prototype chain', () => {
  // publishes[c.id] alone resolves up the prototype chain, so an id of
  // "constructor" (or "toString", "valueOf", "hasOwnProperty") would pass
  // the "nothing publishes this" check for the wrong reason — it finds
  // Object.prototype.constructor, not a real publisher.
  const ex = goodExchange();
  ex.consumes[0].id = 'constructor';
  assertFailsWith(repo(goodFiles(), ex), 'consumes "constructor", which nothing publishes');
});

for (const root of tmpRoots) rmSync(root, { recursive: true, force: true });

console.log('\n' + (failed ? `✗ ${failed} failed, ${passed} passed` : `✓ ${passed} tests passed`));
process.exit(failed ? 1 : 0);
