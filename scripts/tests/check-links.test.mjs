#!/usr/bin/env node
/*
 * Tests for check-links — one per rule it enforces.
 *
 * Each builds a throwaway repo in os.tmpdir(), runs the checker against it with
 * LINKS_ROOT, and asserts it fails FOR THAT REASON rather than merely failing.
 * A test that only asserts a non-zero exit would pass for any bug at all,
 * including the checker crashing on startup.
 *
 *   node scripts/tests/check-links.test.mjs
 */
'use strict';

import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { tmpdir } from 'node:os';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import assert from 'node:assert';

const HERE = dirname(fileURLToPath(import.meta.url));
const CHECKER = join(HERE, '..', 'check-links.mjs');

let passed = 0;
let failed = 0;
const tmpRoots = [];

function test(name, fn) {
  try {
    fn();
    passed += 1;
    console.log(`  ok  ${name}`);
  } catch (err) {
    failed += 1;
    console.error(`  FAIL ${name}\n       ${err.message}`);
  }
}

function write(root, rel, text) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, text);
}

function manifest(projects) {
  return `(function (g) { var P = ${JSON.stringify(projects, null, 2)};\n` +
    "if (typeof module === 'object' && module.exports) module.exports = { PROJECTS: P };\n" +
    'g.MAZ_PROJECTS = P; })(globalThis);\n';
}

const APP = {
  id: 'alpha', name: 'ALPHA', kind: 'play', path: 'alpha/',
  tag: 'a tag', accent: '#ff00ff', blurb: 'A blurb.'
};

/** A repo where everything is correct; each test then breaks one thing. */
function makeRepo(overrides = {}) {
  const root = mkdtempSync(join(tmpdir(), 'maz-links-'));
  tmpRoots.push(root);
  const projects = overrides.projects || [APP];

  write(root, 'shared/projects.js', manifest(projects));
  write(root, 'shared/maz-nav.js', '/* nav */');
  write(root, 'index.html',
    '<script src="shared/projects.js"></script>\n' +
    '<noscript>' + projects.map((p) => `<a href="${p.path}">${p.name}</a>`).join('') + '</noscript>');
  write(root, 'README.md', '# Repo\n');
  for (const p of projects) {
    if (!p.path.endsWith('/')) continue;
    write(root, `${p.path}index.html`,
      `<script src="../shared/maz-nav.js" data-current="${p.id}"></script>`);
    write(root, `${p.path}README.md`, `# ${p.name}\n`);
  }
  return root;
}

function run(root) {
  try {
    return { ok: true, text: execFileSync(process.execPath, [CHECKER], {
      env: { ...process.env, LINKS_ROOT: root },
      encoding: 'utf8',
      // Capture stderr rather than letting it through: the checker printing the
      // failure each test provoked is expected, and inheriting it buries the
      // test results it is meant to be proving.
      stdio: ['ignore', 'pipe', 'pipe']
    }) };
  } catch (err) {
    return { ok: false, text: String(err.stdout || '') + String(err.stderr || '') };
  }
}

console.log('check-links');

test('a correct repo passes', () => {
  const r = run(makeRepo());
  assert.ok(r.ok, `a clean repo was rejected:\n${r.text}`);
  assert.ok(r.text.includes('everything is connected'), r.text);
});

/* ------------------------------------------------------------ rule 1 */

test('a dangling link in a page is reported, with the target named', () => {
  const root = makeRepo();
  write(root, 'alpha/index.html',
    '<script src="../shared/maz-nav.js" data-current="alpha"></script>' +
    '<a href="nowhere.html">gone</a>');
  const r = run(root);
  assert.strictEqual(r.ok, false, 'a broken link was accepted');
  assert.ok(r.text.includes('nowhere.html'), r.text);
});

test('a dangling link in a Markdown file is reported', () => {
  const root = makeRepo();
  write(root, 'alpha/README.md', '# Alpha\n\nSee [the guide](guide.md).\n');
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('guide.md'), r.text);
});

test('external links, anchors and template placeholders are left alone', () => {
  const root = makeRepo();
  write(root, 'alpha/README.md',
    '# Alpha\n[web](https://example.com) [mail](mailto:a@b.c) [top](#heading) [tpl]({{url}})\n');
  assert.ok(run(root).ok, 'an external or anchor-only link was treated as broken');
});

test('a link inside a fenced code block is not a link', () => {
  const root = makeRepo();
  write(root, 'alpha/README.md', '# Alpha\n\n```\nsee [this](not-a-real-file.md)\n```\n');
  assert.ok(run(root).ok, 'a code sample was mistaken for a link');
});

test('JavaScript inside a <script> tag is not a link', () => {
  // `card.href = p.path` is an assignment, not an href attribute.
  const root = makeRepo();
  write(root, 'alpha/index.html',
    '<script src="../shared/maz-nav.js" data-current="alpha"></script>' +
    '<script>const a = document.createElement("a"); a.href = p.path;</script>');
  assert.ok(run(root).ok, 'script contents were scanned as markup');
});

test('a link to a directory holding an index.html or README resolves', () => {
  const root = makeRepo();
  write(root, 'alpha/README.md', '# Alpha\n\n[the app](.) and [the hub](../)\n');
  assert.ok(run(root).ok, 'a directory link was called broken');
});

test('every project directory is scanned, including one added later', () => {
  // The bug this guards: the list of directories to scan used to be written out
  // by hand, and CODA PICS was added to the arcade without being added to it.
  // For as long as that lasted, none of its links were checked by the gate
  // whose whole job is checking links.
  const beta = { ...APP, id: 'beta', name: 'BETA', path: 'beta/' };
  const root = makeRepo({ projects: [APP, beta] });
  write(root, 'beta/README.md', '# Beta\n\n[missing](nope.md)\n');
  const r = run(root);
  assert.strictEqual(r.ok, false, 'a project added to the manifest went unscanned');
  assert.ok(r.text.includes('nope.md'), r.text);
});

/* ------------------------------------------------------------ rule 2 */

test('a missing manifest is reported as such', () => {
  const root = makeRepo();
  rmSync(join(root, 'shared', 'projects.js'));
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('shared/projects.js is missing'), r.text);
});

test('a project missing a required field is named, with the field', () => {
  const root = makeRepo({ projects: [{ ...APP, blurb: '' }] });
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('blurb'), r.text);
  assert.ok(r.text.includes('alpha'), 'the failure should say which project');
});

test('two projects sharing an id is reported', () => {
  const root = makeRepo({ projects: [APP, { ...APP, name: 'ALPHA TWO' }] });
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('duplicate project id'), r.text);
});

test('a project pointing at a path that does not exist is reported', () => {
  const root = makeRepo();
  write(root, 'shared/projects.js', manifest([{ ...APP, path: 'ghost/' }]));
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('ghost/'), r.text);
});

/* ------------------------------------------------------------ rule 3 */

test('a hub that does not load the manifest is reported', () => {
  const root = makeRepo();
  write(root, 'index.html', '<noscript><a href="alpha/">ALPHA</a></noscript>');
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('does not load shared/projects.js'), r.text);
});

test('a hub whose noscript fallback has fallen behind the manifest is reported', () => {
  const root = makeRepo();
  write(root, 'index.html', '<script src="shared/projects.js"></script><noscript></noscript>');
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('noscript fallback'), r.text);
});

test('an app with no nav is reported as a dead end', () => {
  const root = makeRepo();
  write(root, 'alpha/index.html', '<h1>ALPHA</h1>');
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('dead end'), r.text);
});

test('an app whose nav does not know which project it is, is reported', () => {
  const root = makeRepo();
  write(root, 'alpha/index.html', '<script src="../shared/maz-nav.js"></script>');
  const r = run(root);
  assert.strictEqual(r.ok, false);
  assert.ok(r.text.includes('data-current'), r.text);
});

test('a docs-only project with no page of its own is not asked for a nav', () => {
  const root = makeRepo();
  write(root, 'shared/projects.js', manifest([APP, {
    ...APP, id: 'lib', name: 'LIB', kind: 'code', path: 'lib/README.md'
  }]));
  write(root, 'lib/README.md', '# Lib\n');
  write(root, 'index.html',
    '<script src="shared/projects.js"></script>' +
    '<noscript><a href="alpha/">A</a><a href="lib/README.md">L</a></noscript>');
  assert.ok(run(root).ok, 'a project with no index.html was asked to carry a nav');
});

for (const r of tmpRoots) rmSync(r, { recursive: true, force: true });

console.log(`\n${passed} passed, ${failed} failed`);
process.exitCode = failed ? 1 : 0;
