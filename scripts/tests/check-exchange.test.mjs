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
  const files = goodFiles();
  files['film/index.html'] = GOOD_PAGE
    .replace('<script src="../music/js/synth.js"></script>\n', '');
  assertFailsWith(repo(files, goodExchange()), 'music/js/synth.js');
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
  const files = goodFiles();
  files['film/index.html'] = GOOD_PAGE.replace(
    '</body>', '<script src="js/app.js"></script>\n</body>');
  files['film/js/app.js'] = '// app\n';
  const { code, output } = check(repo(files, goodExchange()));
  assert.strictEqual(code, 0, 'same-project scripts are not coupling:\n' + output);
});

for (const root of tmpRoots) rmSync(root, { recursive: true, force: true });

console.log('\n' + (failed ? `✗ ${failed} failed, ${passed} passed` : `✓ ${passed} tests passed`));
process.exit(failed ? 1 : 0);
