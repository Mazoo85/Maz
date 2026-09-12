# The Exchange Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make it a build failure for one project in this repo to depend on another without declaring it, and make the Forge's `checks: green` cover the projects its change can actually break.

**Architecture:** A plain JSON file (`shared/exchange.json`) declares who publishes what and who consumes it. A Node checker (`scripts/check-exchange.mjs`) run by site CI proves the declarations match the actual pages in both directions. The Forge reads the same file so a zone's checks include every downstream consumer's checks.

**Tech Stack:** Node 22 (no dependencies, ESM, `node:` builtins only) for the checker; Python 3.10+ stdlib for the Forge changes; plain JSON for the declaration.

**Spec:** [`docs/superpowers/specs/2026-09-11-the-exchange-design.md`](../specs/2026-09-11-the-exchange-design.md)

## Global Constraints

- **Node scripts:** ESM (`import`, not `require`), no npm dependencies, `node:`-prefixed builtins, shebang `#!/usr/bin/env node`. Match `scripts/check-links.mjs`: collect into an `errors` array, print `✖ N problems` and `process.exit(1)`, or print `✓ …` plus `notes`.
- **Forge Python:** 3.10+ only — no `tomllib`, no `match`. Stdlib plus `typer` and `rich`. Never import from `crew/`.
- **Forge tests:** must pass under `-W error`. Run with `cd forge && python -m pytest tests/ -q -W error`.
- **Every new test is mutation-checked:** delete or invert the guard it names, confirm that test fails, restore. A test that passes with its guard removed is a plan failure, not a passing test.
- **`forge/` and `.github/workflows/` are `HARD_NO_TOUCH` for the Forge itself.** A human (you) may edit them; the Forge may not. Nothing in this plan changes that.
- **The declaration is data, not trust.** `check-exchange.mjs` and the Forge both treat `shared/exchange.json` as untrusted input: malformed shapes produce a named error, never a crash and never a silent pass.

## File Structure

| File | Responsibility |
|---|---|
| `shared/exchange.json` | **Create.** The declaration: what each project publishes, what each consumes. |
| `scripts/check-exchange.mjs` | **Create.** Proves the declaration matches reality. Exits non-zero with named reasons. |
| `scripts/tests/check-exchange.test.mjs` | **Create.** Fixture-driven tests, one per failure rule. |
| `.github/workflows/site-ci.yml` | **Modify.** Add two steps to the `links` job. |
| `forge/forge/exchange.py` | **Create.** Loads and validates the declaration for the Forge. Raises `ExchangeError`. |
| `forge/forge/checks.py` | **Modify.** `PROJECT_CHECKS` + `ZONE_PROJECT` replace the hand-written zone map; add `all_commands`. |
| `forge/forge/verify.py` | **Modify.** `run_checks` uses `all_commands`; an unreadable declaration fails the checks. |
| `forge/forge/decide.py` | **Modify.** Optional early gate: skip every candidate with `config_error` when told the declaration is bad. |
| `forge/forge/cli.py`, `forge/forge/orchestrate.py` | **Modify.** Compute that flag and pass it. |
| `forge/tests/test_exchange.py` | **Create.** Loader and validation tests. |
| `forge/tests/test_checks.py` | **Create.** `commands_for` / `all_commands` tests. `checks.py` currently has no test file of its own. |
| `docs/FORGE.md` | **Modify.** Correct what "green" now covers. |

**Layering note the implementer must not get wrong.** The Forge has two gates and they are not equal:

- **VERIFY is the safety net.** `all_commands(zone, root)` raises `ExchangeError` when the declaration is unreadable, and `run_checks` turns that into a failed check. This is unconditional and has no default that weakens it.
- **DECIDE is an optimisation.** Its `exchange_ok` flag lets a run skip candidates early instead of burning a Crew run first. It defaults to `True` — that default is safe *only because* VERIFY fails closed regardless. Do not move the safety guarantee into DECIDE's default.

---

### Task 1: The declaration, and the rules that check it against itself

**Files:**
- Create: `shared/exchange.json`
- Create: `scripts/check-exchange.mjs`
- Create: `scripts/tests/check-exchange.test.mjs`
- Modify: `.github/workflows/site-ci.yml`

**Interfaces:**
- Consumes: nothing — this is the first task.
- Produces: `shared/exchange.json` with top-level keys `publishes` (object keyed by published id) and `consumes` (array). `scripts/check-exchange.mjs` exits 0 or 1. `scripts/tests/check-exchange.test.mjs` exports nothing; it is run directly by `node`.

This task covers spec rules 1, 2, 3, 6 and 7 — everything checkable without reading an HTML page. Task 2 adds the two rules that compare the declaration against real pages.

- [ ] **Step 1: Write the declaration**

Create `shared/exchange.json` exactly as follows. The five `files` are copied from `film/index.html` lines 164-168 and the order is the load order SCRIPT FORGE uses.

```json
{
  "_comment": "Which projects publish something other projects use, and who uses it. Read by scripts/check-exchange.mjs and by forge/forge/exchange.py. Adding a cross-project <script> tag without declaring it here fails CI.",
  "publishes": {
    "music/composer": {
      "project": "music",
      "summary": "Compose and play a song from a genre, mood and length.",
      "files": [
        "music/js/theory.js",
        "music/js/genres.js",
        "music/js/synth.js",
        "music/js/composer.js",
        "music/js/engine.js"
      ]
    }
  },
  "consumes": [
    {
      "project": "film",
      "id": "music/composer",
      "via": "script",
      "page": "film/index.html",
      "contract": "film/tests/film-logic.test.js"
    }
  ]
}
```

- [ ] **Step 2: Write the failing tests**

Create `scripts/tests/check-exchange.test.mjs`. It writes fixture repos into a temp directory and runs the checker against each with `EXCHANGE_ROOT` pointing at the fixture, asserting both the exit code and that the message names the right reason.

```javascript
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
  assertFailsWith(repo(goodFiles(), { consumes: [] }), 'publishes');
});

console.log('\n' + (failed ? `✗ ${failed} failed, ${passed} passed` : `✓ ${passed} tests passed`));
process.exit(failed ? 1 : 0);
```

- [ ] **Step 3: Run the tests and confirm they fail for the right reason**

```bash
node scripts/tests/check-exchange.test.mjs
```

Expected: the process fails because `scripts/check-exchange.mjs` does not exist — `execFileSync` reports `Cannot find module`. That is the correct pre-implementation failure.

- [ ] **Step 4: Write the checker**

Create `scripts/check-exchange.mjs`. `EXCHANGE_ROOT` exists purely so the tests can point it at a fixture; unset, it resolves the real repo root exactly as `check-links.mjs` does.

```javascript
#!/usr/bin/env node
/*
 * check-exchange — proves the declared dependencies between projects are true.
 *
 * shared/exchange.json says which projects publish something other projects
 * use, and who uses it. This checks the declaration against reality in both
 * directions: nothing is declared that isn't real, and nothing real is left
 * undeclared.
 *
 * No dependencies — run it with `node scripts/check-exchange.mjs` from
 * anywhere. Exits non-zero (and prints what broke) if anything is out of step.
 *
 * EXCHANGE_ROOT overrides the repo root, for the tests only.
 */

import { readFileSync, existsSync, readdirSync, statSync } from 'node:fs';
import { join, dirname, resolve, relative } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const ROOT = process.env.EXCHANGE_ROOT
  ? resolve(process.env.EXCHANGE_ROOT)
  : resolve(dirname(fileURLToPath(import.meta.url)), '..');

const EXCHANGE_REL = 'shared/exchange.json';
const SKIP_DIRS = new Set([
  '.git', 'node_modules', 'build', 'dist', '__pycache__', '.venv', 'venv',
  '.pytest_cache', '.mypy_cache', '.claude', 'fixtures'
]);

/* `shared/` is infrastructure every page uses (the nav, the project list),
 * not a project dependency. Excluding it is what keeps the undeclared-coupling
 * rule pointed at real coupling instead of at the nav bar. */
const INFRASTRUCTURE = new Set(['shared', 'assets']);

const errors = [];
const notes = [];
const fail = (message) => errors.push(message);

function report() {
  if (errors.length) {
    console.error(`\n✖ ${errors.length} problem${errors.length === 1 ? '' : 's'}:\n`);
    for (const e of errors) console.error('  - ' + e);
    console.error('');
    process.exit(1);
  }
  console.log('✓ every cross-project dependency is declared');
  for (const n of notes) console.log('  · ' + n);
}

/* ------------------------------------------------ 1. read the declaration */
const exchangePath = join(ROOT, EXCHANGE_REL);
if (!existsSync(exchangePath)) {
  fail(`${EXCHANGE_REL} is missing — it is what declares which projects use each other`);
  report();
}

let exchange;
try {
  exchange = JSON.parse(readFileSync(exchangePath, 'utf8'));
} catch (err) {
  fail(`${EXCHANGE_REL} is not valid JSON: ${err.message}`);
  report();
}

const publishes = exchange?.publishes;
const consumes = exchange?.consumes;
if (!publishes || typeof publishes !== 'object' || Array.isArray(publishes)) {
  fail(`${EXCHANGE_REL}: "publishes" must be an object keyed by published id`);
}
if (!Array.isArray(consumes)) {
  fail(`${EXCHANGE_REL}: "consumes" must be an array`);
}
if (errors.length) report();

/* ------------------------------------------- 2. the project list agrees */
const projectsPath = join(ROOT, 'shared', 'projects.js');
let projectIds = new Set();
if (!existsSync(projectsPath)) {
  fail('shared/projects.js is missing — it is the list of what projects exist');
} else {
  const mod = await import(pathToFileURL(projectsPath).href);
  const list = mod.PROJECTS ?? globalThis.MAZ_PROJECTS ?? [];
  projectIds = new Set(list.map((p) => p?.id).filter(Boolean));
}

const knownProject = (id, where) => {
  if (!projectIds.has(id)) {
    fail(`${EXCHANGE_REL}: ${where} names project "${id}", which is not in shared/projects.js`);
    return false;
  }
  return true;
};

/* ------------------------------------------------ 3. publishers are real */
for (const [id, entry] of Object.entries(publishes)) {
  if (!entry || typeof entry !== 'object') {
    fail(`${EXCHANGE_REL}: published id "${id}" is not an object`);
    continue;
  }
  knownProject(entry.project, `publishes["${id}"]`);
  if (!Array.isArray(entry.files) || entry.files.length === 0) {
    fail(`${EXCHANGE_REL}: published id "${id}" declares no files`);
    continue;
  }
  for (const f of entry.files) {
    if (typeof f !== 'string' || !existsSync(join(ROOT, f))) {
      fail(`${EXCHANGE_REL}: published id "${id}" names ${f}, which does not exist`);
    }
  }
}

/* ------------------------------------------------- 4. consumers are real */
for (const c of consumes) {
  if (!c || typeof c !== 'object') {
    fail(`${EXCHANGE_REL}: a "consumes" entry is not an object`);
    continue;
  }
  const where = `consumes entry for "${c.id}"`;
  knownProject(c.project, where);

  const published = publishes[c.id];
  if (!published) {
    fail(`${EXCHANGE_REL}: ${c.project} consumes "${c.id}", which nothing publishes`);
    continue;
  }
  if (published.project === c.project) {
    fail(`${EXCHANGE_REL}: project "${c.project}" consumes "${c.id}", its own published surface`);
  }
  if (!c.contract || !existsSync(join(ROOT, c.contract))) {
    fail(`${EXCHANGE_REL}: ${where} names contract ${c.contract}, which does not exist`);
  }
}

notes.push(`${Object.keys(publishes).length} published surface(s), ${consumes.length} declared use(s)`);
report();
```

- [ ] **Step 5: Run the tests and confirm they pass**

```bash
node scripts/tests/check-exchange.test.mjs
```

Expected: `✓ 8 tests passed`.

- [ ] **Step 6: Run the checker against the real repo**

```bash
node scripts/check-exchange.mjs
```

Expected:
```
✓ every cross-project dependency is declared
  · 1 published surface(s), 1 declared use(s)
```

- [ ] **Step 7: Mutation-check every new test**

For each, make the edit, run `node scripts/tests/check-exchange.test.mjs`, confirm the named test fails, then revert.

| Mutation in `check-exchange.mjs` | Test that must fail |
|---|---|
| Delete the `if (!published)` block | `consuming an id nobody publishes fails` |
| Make `knownProject` always `return true` | `a project not in shared/projects.js fails` |
| Drop the `existsSync(join(ROOT, f))` check on published files | `a published file that does not exist fails` |
| Drop the `c.contract` existence check | `a contract file that does not exist fails` |
| Delete the `published.project === c.project` block | `a project consuming its own published id fails` |
| Replace the `JSON.parse` try/catch with a bare `JSON.parse` | `a malformed exchange.json fails with a named error` (it would throw instead) |
| Delete the `publishes` shape check | `publishes missing entirely fails rather than throwing` |

Any mutation that leaves all tests green is a vacuous test — rewrite it before continuing.

- [ ] **Step 8: Wire it into CI**

In `.github/workflows/site-ci.yml`, inside the `links` job, add two steps after the `Check every local link resolves` step:

```yaml
      - name: Check every cross-project dependency is declared
        run: node scripts/check-exchange.mjs
      - name: check-exchange's own tests
        run: node scripts/tests/check-exchange.test.mjs
```

The `links` job has no path filter, so this runs on every pull request.

- [ ] **Step 9: Commit**

```bash
git add shared/exchange.json scripts/check-exchange.mjs scripts/tests/check-exchange.test.mjs .github/workflows/site-ci.yml
git commit -m "Declare the one real dependency between projects, and check it

shared/exchange.json records that SCRIPT FORGE consumes SONG FORGE by
loading five of its files in order. check-exchange.mjs proves the
declaration is internally coherent: nothing consumes an id nobody
publishes, every named file and contract test exists, both manifests
agree on what projects exist, and no project consumes itself.

Comparing the declaration against the real pages comes next."
```

---

### Task 2: The rules with teeth — declaration versus reality

**Files:**
- Modify: `scripts/check-exchange.mjs`
- Modify: `scripts/tests/check-exchange.test.mjs`

**Interfaces:**
- Consumes: `scripts/check-exchange.mjs` from Task 1 — its `ROOT`, `errors`, `fail()`, `report()`, `INFRASTRUCTURE`, `SKIP_DIRS`, `publishes`, `consumes`. The fixture helpers `repo()`, `check()`, `assertFailsWith()`, `goodExchange()`, `goodFiles()`, `GOOD_PAGE`, `MUSIC_FILES` from the Task 1 test file.
- Produces: the finished checker. Nothing later depends on its internals.

This is spec rules 4 and 5. Rule 5 — a cross-project `<script>` with no declaration — is the one that does the work.

- [ ] **Step 1: Write the failing tests**

Append to `scripts/tests/check-exchange.test.mjs`, immediately **before** the final `console.log(...)`/`process.exit(...)` lines:

```javascript
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
```

- [ ] **Step 2: Run the tests and confirm the new ones fail**

```bash
node scripts/tests/check-exchange.test.mjs
```

Expected: the three `assertFailsWith` tests fail with `expected the checker to fail, it exited 0`. The two that assert success already pass — they will become meaningful once the rule exists, and Step 5 proves it.

- [ ] **Step 3: Implement both rules**

In `scripts/check-exchange.mjs`, replace the final two lines:

```javascript
notes.push(`${Object.keys(publishes).length} published surface(s), ${consumes.length} declared use(s)`);
report();
```

with:

```javascript
/* ------------------------------------- 5. the declaration matches the page */
function walkHtml(dir, out = []) {
  for (const entry of readdirSync(dir)) {
    if (SKIP_DIRS.has(entry)) continue;
    const full = join(dir, entry);
    if (statSync(full).isDirectory()) walkHtml(full, out);
    else if (entry.endsWith('.html')) out.push(full);
  }
  return out;
}

/* Every `../<something>/...` a page loads through src= or href=. Only HTML is
 * scanned: a README mentioning another project is prose, not coupling. */
const CROSS_REF = /(?:src|href)=["']\.\.\/([^/"']+)\/([^"']+)["']/g;

function crossRefs(text) {
  const out = [];
  for (const m of text.matchAll(CROSS_REF)) out.push({ project: m[1], path: m[1] + '/' + m[2] });
  return out;
}

const pages = walkHtml(ROOT);
const declaredByPage = new Map();
for (const c of consumes) {
  if (c?.via === 'script' && c.page) {
    if (!declaredByPage.has(c.page)) declaredByPage.set(c.page, []);
    declaredByPage.get(c.page).push(c);
  }
}

/* 5a. every declared script link is loaded, in the declared order. */
for (const [page, entries] of declaredByPage) {
  const full = join(ROOT, page);
  if (!existsSync(full)) {
    fail(`${EXCHANGE_REL}: declares a use on ${page}, which does not exist`);
    continue;
  }
  const loaded = crossRefs(readFileSync(full, 'utf8')).map((r) => r.path);
  for (const entry of entries) {
    const want = publishes[entry.id]?.files ?? [];
    const missing = want.filter((f) => !loaded.includes(f));
    if (missing.length) {
      fail(`${page} declares it uses "${entry.id}" but does not load ${missing.join(', ')}`);
      continue;
    }
    const positions = want.map((f) => loaded.indexOf(f));
    const ordered = positions.every((p, i) => i === 0 || p > positions[i - 1]);
    if (!ordered) {
      fail(`${page} loads "${entry.id}" files in the wrong order — ` +
           `they must appear as ${want.join(', ')}`);
    }
  }
}

/* 5b. nothing reaches into another project without declaring it. */
const declaredFiles = new Map();
for (const c of consumes) {
  if (!c?.page) continue;
  const want = publishes[c.id]?.files ?? [];
  if (!declaredFiles.has(c.page)) declaredFiles.set(c.page, new Set());
  for (const f of want) declaredFiles.get(c.page).add(f);
}

for (const full of pages) {
  const page = relative(ROOT, full).split('\\').join('/');
  const owner = page.includes('/') ? page.split('/')[0] : null;
  const allowed = declaredFiles.get(page) ?? new Set();
  for (const ref of crossRefs(readFileSync(full, 'utf8'))) {
    if (INFRASTRUCTURE.has(ref.project)) continue;
    if (ref.project === owner) continue;
    if (allowed.has(ref.path)) continue;
    fail(`${page} loads ${ref.path} from another project, but ${EXCHANGE_REL} ` +
         `does not declare that it consumes it`);
  }
}

notes.push(`${Object.keys(publishes).length} published surface(s), ${consumes.length} declared use(s)`);
notes.push(`${pages.length} HTML page(s) scanned for undeclared coupling`);
report();
```

- [ ] **Step 4: Run the tests and confirm they pass**

```bash
node scripts/tests/check-exchange.test.mjs
```

Expected: `✓ 13 tests passed`.

- [ ] **Step 5: Mutation-check the new tests**

| Mutation in `check-exchange.mjs` | Test that must fail |
|---|---|
| Replace the `ordered` computation with `const ordered = true;` | `a page loading the files out of the declared order fails` |
| Delete the `if (missing.length)` block | `a page missing one of the declared files fails` |
| Delete the whole `5b` loop | `an undeclared cross-project script tag fails` |
| Remove `shared` from `INFRASTRUCTURE` | `loading shared/ is not coupling and does not fail` |
| Delete the `ref.project === owner` line | `a page may load its own files without declaring anything` |

- [ ] **Step 6: Run against the real repo**

```bash
node scripts/check-exchange.mjs && node scripts/check-links.mjs
```

Expected: both pass. `check-exchange` reports 1 published surface, 1 declared use, and the real page count.

- [ ] **Step 7: Prove the rule bites on the real repo**

Temporarily add an undeclared coupling and confirm CI would catch it:

```bash
sed -i 's|<script src="js/app.js"></script>|<script src="../music/js/synth.js"></script>\n  <script src="js/app.js"></script>|' madlibs/index.html
node scripts/check-exchange.mjs; echo "exit: $?"
git checkout madlibs/index.html
```

Expected: exit 1, with a message naming `madlibs/index.html` and `music/js/synth.js`. Then `git status` must show a clean tree again.

- [ ] **Step 8: Commit**

```bash
git add scripts/check-exchange.mjs scripts/tests/check-exchange.test.mjs
git commit -m "Fail the build on undeclared coupling between projects

Two rules that compare shared/exchange.json against the real pages: a
declared use must actually load the published files in the declared
order, and any cross-project script tag with no declaration behind it
fails the build naming the page and the file.

shared/ and assets/ are excluded — every page uses the nav, and that is
infrastructure rather than a dependency. A page loading its own files is
not coupling either.

Verified by adding a real undeclared tag to madlibs/index.html: the
checker exits 1 and names both the page and the file."
```

---

### Task 3: `forge/forge/exchange.py` — the Forge reads the declaration

**Files:**
- Create: `forge/forge/exchange.py`
- Create: `forge/tests/test_exchange.py`

**Interfaces:**
- Consumes: `shared/exchange.json` as written in Task 1.
- Produces:
  - `class ExchangeError(Exception)`
  - `EXCHANGE_PATH: str` = `"shared/exchange.json"`
  - `load(root: Path) -> Exchange` — raises `ExchangeError` on missing, unreadable, non-JSON or structurally invalid input.
  - `class Exchange` (frozen dataclass) with `consumers_of(project: str) -> tuple[str, ...]` — the ids of projects that consume anything published by `project`, sorted, deduplicated.
  - `is_loadable(root: Path) -> bool` — `True` if `load` would succeed.

- [ ] **Step 1: Write the failing tests**

Create `forge/tests/test_exchange.py`:

```python
"""Reading shared/exchange.json — who depends on whom."""

import json

import pytest

from forge.exchange import Exchange, ExchangeError, is_loadable, load

GOOD = {
    "publishes": {
        "music/composer": {
            "project": "music",
            "summary": "Compose a song.",
            "files": ["music/js/composer.js"],
        }
    },
    "consumes": [
        {
            "project": "film",
            "id": "music/composer",
            "via": "script",
            "page": "film/index.html",
            "contract": "film/tests/film-logic.test.js",
        }
    ],
}


def _write(root, data):
    shared = root / "shared"
    shared.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        (shared / "exchange.json").write_text(data, encoding="utf-8")
    else:
        (shared / "exchange.json").write_text(json.dumps(data), encoding="utf-8")
    return root


def test_consumers_of_names_the_downstream_project(tmp_path):
    ex = load(_write(tmp_path, GOOD))
    assert ex.consumers_of("music") == ("film",)


def test_a_project_nobody_depends_on_has_no_consumers(tmp_path):
    ex = load(_write(tmp_path, GOOD))
    assert ex.consumers_of("madlibs") == ()


def test_the_consumer_is_not_its_own_consumer(tmp_path):
    ex = load(_write(tmp_path, GOOD))
    assert ex.consumers_of("film") == ()


def test_two_consumers_of_one_project_are_both_named_once(tmp_path):
    data = json.loads(json.dumps(GOOD))
    data["publishes"]["music/player"] = {
        "project": "music", "summary": "Play.", "files": ["music/js/engine.js"],
    }
    data["consumes"].append({
        "project": "shooter", "id": "music/player", "via": "script",
        "page": "shooter/index.html", "contract": "shooter/tests/t.js",
    })
    data["consumes"].append({
        "project": "film", "id": "music/player", "via": "script",
        "page": "film/index.html", "contract": "film/tests/film-logic.test.js",
    })
    ex = load(_write(tmp_path, data))
    assert ex.consumers_of("music") == ("film", "shooter")


def test_a_missing_file_raises_rather_than_returning_empty(tmp_path):
    # Returning an empty Exchange would silently narrow the Forge's checks,
    # which is the exact defect this whole feature exists to remove.
    with pytest.raises(ExchangeError):
        load(tmp_path)


def test_invalid_json_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, "{ not json"))


def test_invalid_utf8_raises(tmp_path):
    shared = tmp_path / "shared"
    shared.mkdir(parents=True)
    (shared / "exchange.json").write_bytes(b'{"publishes": {}, "consumes": [\xff]}')
    with pytest.raises(ExchangeError):
        load(tmp_path)


def test_publishes_of_the_wrong_type_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": [], "consumes": []}))


def test_consumes_of_the_wrong_type_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": {}, "consumes": {}}))


def test_a_consumes_entry_that_is_not_a_dict_raises(tmp_path):
    # The container is a list, but one element is junk. A guard that checks
    # the container and not its elements is a defect this codebase has
    # shipped five times.
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": {}, "consumes": ["film"]}))


def test_a_publishes_entry_that_is_not_a_dict_raises(tmp_path):
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, {"publishes": {"music/x": "music"}, "consumes": []}))


def test_a_consumes_entry_naming_an_unpublished_id_raises(tmp_path):
    data = json.loads(json.dumps(GOOD))
    data["consumes"][0]["id"] = "music/ghost"
    with pytest.raises(ExchangeError):
        load(_write(tmp_path, data))


def test_is_loadable_is_true_for_a_good_file(tmp_path):
    assert is_loadable(_write(tmp_path, GOOD)) is True


def test_is_loadable_is_false_for_a_bad_file_and_does_not_raise(tmp_path):
    assert is_loadable(_write(tmp_path, "{ not json")) is False


def test_is_loadable_is_false_when_the_file_is_missing(tmp_path):
    assert is_loadable(tmp_path) is False
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd forge && python -m pytest tests/test_exchange.py -q
```

Expected: collection error, `ModuleNotFoundError: No module named 'forge.exchange'`.

- [ ] **Step 3: Write the module**

Create `forge/forge/exchange.py`:

```python
"""Who depends on whom — read from shared/exchange.json.

The Forge's checks are only as honest as its knowledge of what a change can
break. `music/`'s own tests say nothing about SCRIPT FORGE, which loads five
of SONG FORGE's files; without this file the loop could break SCRIPT FORGE,
record `checks: green`, and open a pull request describing verified work.

Unlike every collector in `signals/`, this module **raises**. That is
deliberate and is the whole point. A signal that cannot read its source
should degrade to fewer candidates; a module that cannot tell you what a
change might break must not answer "nothing", because "nothing" is
indistinguishable from a correct answer and silently narrows what gets
verified. Callers decide what to do with the failure — see
`checks.all_commands` and `decide`.
"""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path

EXCHANGE_PATH = "shared/exchange.json"


class ExchangeError(Exception):
    """The declaration is missing, unreadable, or not the shape promised."""


@dataclass(frozen=True)
class Exchange:
    """The dependency graph, already validated."""

    # published id -> the project that publishes it
    publisher: dict
    # (consuming project, published id) pairs
    uses: tuple

    def consumers_of(self, project: str) -> tuple[str, ...]:
        """Projects that consume something `project` publishes.

        Sorted and deduplicated so a caller's command list is stable between
        runs: the ledger records what was run, and a set's iteration order
        would make two identical nights look different.
        """
        out = set()
        for consumer, published_id in self.uses:
            if self.publisher.get(published_id) == project and consumer != project:
                out.add(consumer)
        return tuple(sorted(out))


def load(root: Path) -> Exchange:
    """Read and validate the declaration. Raises `ExchangeError`, never returns empty on error."""
    path = root / EXCHANGE_PATH
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, ValueError) as exc:
        raise ExchangeError(f"{EXCHANGE_PATH}: {exc}") from exc

    if not isinstance(raw, dict):
        raise ExchangeError(f"{EXCHANGE_PATH}: top level is not an object")

    publishes = raw.get("publishes")
    consumes = raw.get("consumes")
    if not isinstance(publishes, dict):
        raise ExchangeError(f'{EXCHANGE_PATH}: "publishes" is not an object')
    if not isinstance(consumes, list):
        raise ExchangeError(f'{EXCHANGE_PATH}: "consumes" is not an array')

    publisher: dict = {}
    for published_id, entry in publishes.items():
        # The container being a dict says nothing about its values.
        if not isinstance(entry, dict):
            raise ExchangeError(f'{EXCHANGE_PATH}: published id "{published_id}" is not an object')
        project = entry.get("project")
        if not isinstance(project, str) or not project:
            raise ExchangeError(f'{EXCHANGE_PATH}: published id "{published_id}" names no project')
        publisher[published_id] = project

    uses: list = []
    for entry in consumes:
        if not isinstance(entry, dict):
            raise ExchangeError(f'{EXCHANGE_PATH}: a "consumes" entry is not an object')
        consumer = entry.get("project")
        published_id = entry.get("id")
        if not isinstance(consumer, str) or not consumer:
            raise ExchangeError(f'{EXCHANGE_PATH}: a "consumes" entry names no project')
        if not isinstance(published_id, str) or published_id not in publisher:
            raise ExchangeError(
                f'{EXCHANGE_PATH}: {consumer} consumes "{published_id}", which nothing publishes'
            )
        uses.append((consumer, published_id))

    return Exchange(publisher=publisher, uses=tuple(uses))


def is_loadable(root: Path) -> bool:
    """True if `load` would succeed. Never raises."""
    try:
        load(root)
    except ExchangeError:
        return False
    return True
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cd forge && python -m pytest tests/test_exchange.py -q -W error
```

Expected: `15 passed`.

- [ ] **Step 5: Mutation-check**

| Mutation in `exchange.py` | Test that must fail |
|---|---|
| `if not isinstance(entry, dict)` in the `consumes` loop → `if False:` | `a consumes entry that is not a dict raises` |
| `if not isinstance(entry, dict)` in the `publishes` loop → `if False:` | `a publishes entry that is not a dict raises` |
| `published_id not in publisher` removed from the condition | `a consumes entry naming an unpublished id raises` |
| `consumer != project` removed from `consumers_of` | `the consumer is not its own consumer` |
| `tuple(sorted(out))` → `tuple(out)` | `two consumers of one project are both named once` (order-dependent; if it passes by luck, the test is too weak — make it assert the sorted tuple) |
| `except (OSError, UnicodeDecodeError, ValueError)` → `except OSError` | `invalid json raises` and `invalid utf8 raises` |

- [ ] **Step 6: Commit**

```bash
git add forge/forge/exchange.py forge/tests/test_exchange.py
git commit -m "Teach the Forge to read the dependency declaration

forge/forge/exchange.py loads shared/exchange.json and answers one
question: which projects consume something this project publishes.

Unlike every collector in signals/, this module raises rather than
degrading to empty. That inversion is the point: a signal that cannot
read its source should yield fewer candidates, but a module that cannot
say what a change might break must not answer 'nothing' — 'nothing' is
indistinguishable from a correct answer and silently narrows what gets
verified, which is the defect this feature exists to remove.

Validation checks elements, not just containers: a consumes list that is
a list of the wrong things is rejected, not trusted."
```

---

### Task 4: A zone's checks include everything downstream

**Files:**
- Modify: `forge/forge/checks.py`
- Modify: `forge/forge/verify.py:49-60` (the `run_checks` function)
- Create: `forge/tests/test_checks.py`

**Interfaces:**
- Consumes: `forge.exchange.load`, `ExchangeError` (Task 3).
- Produces:
  - `PROJECT_CHECKS: dict[str, tuple[tuple[str, ...], ...]]` — project id → commands.
  - `ZONE_PROJECT: dict[str, str | None]` — safe zone → the project it belongs to, or `None`.
  - `commands_for(zone: str) -> tuple[tuple[str, ...], ...]` — **unchanged signature and behaviour**; a zone's own commands only.
  - `all_commands(zone: str, root: Path) -> tuple[tuple[str, ...], ...]` — own plus every downstream consumer's. Raises `ExchangeError`.
  - `run_checks(zone, root, runner=None)` now returns `CheckResult(ok=False, ...)` when the declaration is unreadable.

- [ ] **Step 1: Write the failing tests**

Create `forge/tests/test_checks.py`:

```python
"""What "green" covers: a zone's own checks, plus everything downstream."""

import json

import pytest

from forge.checks import PROJECT_CHECKS, all_commands, commands_for
from forge.exchange import ExchangeError

GOOD = {
    "publishes": {
        "music/composer": {
            "project": "music",
            "summary": "Compose a song.",
            "files": ["music/js/composer.js"],
        }
    },
    "consumes": [
        {
            "project": "film",
            "id": "music/composer",
            "via": "script",
            "page": "film/index.html",
            "contract": "film/tests/film-logic.test.js",
        }
    ],
}


def _repo(root, data=GOOD):
    shared = root / "shared"
    shared.mkdir(parents=True, exist_ok=True)
    text = data if isinstance(data, str) else json.dumps(data)
    (shared / "exchange.json").write_text(text, encoding="utf-8")
    return root


def test_commands_for_is_unchanged_and_takes_no_root():
    # The pure, own-zone map. Task 4 must not alter what existing callers see.
    assert commands_for("music/") == (("node", "music/tests/music-logic.test.js"),)
    assert commands_for("docs/") == ()
    assert commands_for("nowhere/") == ()
    assert commands_for("tests/") == ()


def test_music_gains_films_tests_because_film_consumes_music(tmp_path):
    cmds = all_commands("music/", _repo(tmp_path))
    assert ("node", "music/tests/music-logic.test.js") in cmds
    assert ("node", "film/tests/film-logic.test.js") in cmds


def test_the_zones_own_checks_come_first(tmp_path):
    cmds = all_commands("music/", _repo(tmp_path))
    own = cmds.index(("node", "music/tests/music-logic.test.js"))
    downstream = cmds.index(("node", "film/tests/film-logic.test.js"))
    assert own < downstream


def test_a_zone_with_no_consumers_is_unchanged(tmp_path):
    assert all_commands("crew/tests/", _repo(tmp_path)) == commands_for("crew/tests/")


def test_a_zone_with_no_checks_of_its_own_stays_empty(tmp_path):
    assert all_commands("docs/", _repo(tmp_path)) == ()


def test_a_consumer_with_no_known_checks_adds_nothing(tmp_path):
    # zomboid consumes music but has no entry in PROJECT_CHECKS. Inventing a
    # command for it would be the `tests/` mistake all over again.
    data = json.loads(json.dumps(GOOD))
    data["consumes"].append({
        "project": "zomboid", "id": "music/composer", "via": "script",
        "page": "zomboid/index.html", "contract": "zomboid/tests/t.js",
    })
    assert "zomboid" not in PROJECT_CHECKS
    cmds = all_commands("music/", _repo(tmp_path, data))
    assert not any("zomboid" in " ".join(c) for c in cmds)


def test_a_command_needed_twice_is_run_once(tmp_path):
    data = json.loads(json.dumps(GOOD))
    data["publishes"]["music/player"] = {
        "project": "music", "summary": "Play.", "files": ["music/js/engine.js"],
    }
    data["consumes"].append({
        "project": "film", "id": "music/player", "via": "script",
        "page": "film/index.html", "contract": "film/tests/film-logic.test.js",
    })
    cmds = all_commands("music/", _repo(tmp_path, data))
    assert cmds.count(("node", "film/tests/film-logic.test.js")) == 1


def test_an_unreadable_declaration_raises_rather_than_narrowing(tmp_path):
    # The whole point: falling back to own-zone checks here would report
    # `checks: green` having verified less than it claims.
    with pytest.raises(ExchangeError):
        all_commands("music/", _repo(tmp_path, "{ not json"))


def test_a_missing_declaration_raises_even_for_a_zone_with_no_consumers(tmp_path):
    # Fail closed everywhere. Without the file we cannot prove a zone has no
    # consumers, so "no consumers" is not an answer we are entitled to give.
    with pytest.raises(ExchangeError):
        all_commands("crew/tests/", tmp_path)


def test_run_checks_fails_when_the_declaration_is_unreadable(tmp_path):
    from forge.verify import run_checks

    _repo(tmp_path, "{ not json")
    result = run_checks("music/", tmp_path, runner=lambda cmd, root: (0, "ok"))
    assert result.ok is False
    assert "exchange" in result.output.lower()


def test_run_checks_runs_the_downstream_command(tmp_path):
    from forge.verify import run_checks

    calls = []
    result = run_checks("music/", _repo(tmp_path),
                        runner=lambda cmd, root: (calls.append(cmd), (0, "ok"))[1])
    assert result.ok is True
    assert ("node", "film/tests/film-logic.test.js") in calls
```

- [ ] **Step 2: Run the tests to verify they fail**

```bash
cd forge && python -m pytest tests/test_checks.py -q
```

Expected: `ImportError: cannot import name 'all_commands' from 'forge.checks'`.

- [ ] **Step 3: Rewrite `checks.py`**

Replace the whole of `forge/forge/checks.py` with:

```python
"""What "green" means, per zone.

Structural, not tunable: which command tests a directory is a fact about the
repo, so it lives in code beside the other path maps (see zones.RISK_PATHS and
signals.ci.WORKFLOW_SUBJECTS) rather than in forge.json, which is reserved for
the numbers a human tunes.

A zone with no commands passes trivially. That is correct for docs and content:
there is nothing to run, and CI on the pull request remains the real gate.

`tests/` has no entry, and that omission is deliberate, not an oversight: in
this repo that directory is C++ (CMakeLists.txt, unit_*.cpp), built and run
only through `cmake`/`ctest` against the Vulkan SDK (see
`.github/workflows/ci.yml`) — a build this sandbox cannot run and no command
this module may honestly invent. An earlier version of this map pointed
`tests/` at `python -m pytest -q forge/tests`, which runs the Forge's own
Python suite and verifies nothing whatsoever about a C++ change — a change
under `tests/` would have been "verified" by a command that never looks at
it. `config.ForgeConfig.safe_zones` matches this by leaving `tests/` out of
its default, so the two files cannot drift back into that mismatch: an
unknown zone here (`commands_for` returning `()`) is indistinguishable from
"nothing to check", same as `docs/`, but a zone that isn't in `safe_zones` to
begin with is never reachable to ask.

**A zone's own tests are not the whole story.** SCRIPT FORGE loads five of
SONG FORGE's files, so a change in `music/` can break `film/` while music's
own tests stay green. `all_commands` reads `shared/exchange.json` and adds
the checks of every project declared to consume this one. `commands_for`
remains the pure own-zone answer, for callers that genuinely want only that.
"""

from __future__ import annotations

from pathlib import Path

from .exchange import load

# What verifies each project, whether or not it is a safe zone. `film` is not
# a safe zone and is not expected to become one — it is here because a change
# in `music/` must run film's tests, not because the Forge may edit film.
PROJECT_CHECKS: dict[str, tuple[tuple[str, ...], ...]] = {
    "music": (("node", "music/tests/music-logic.test.js"),),
    "film": (("node", "film/tests/film-logic.test.js"),),
    "scraper": (
        ("python", "-m", "pytest", "-q", "scraper/tests"),
        ("python", "-m", "compileall", "-q", "scraper/scraper"),
    ),
    "crew": (("python", "-m", "pytest", "-q", "crew/tests"),),
}

# Which project a safe zone belongs to. `None` means "no project owns this",
# which is how docs and content get their correct empty command list.
ZONE_PROJECT: dict[str, str | None] = {
    "music/": "music",
    "scraper/": "scraper",
    "crew/tests/": "crew",
    "docs/": None,
    "madlibs/": None,
    "shooter/": None,
}


def commands_for(zone: str) -> tuple[tuple[str, ...], ...]:
    """The commands that verify a zone itself. Unknown zones have none.

    Deliberately pure and root-free: it answers only "what tests this
    directory", which is the question `ZONE_PROJECT` and `PROJECT_CHECKS`
    can answer without reading anything from disk.
    """
    project = ZONE_PROJECT.get(zone)
    if project is None:
        return ()
    return PROJECT_CHECKS.get(project, ())


def all_commands(zone: str, root: Path) -> tuple[tuple[str, ...], ...]:
    """A zone's own checks plus those of every project that consumes it.

    Raises `ExchangeError` when `shared/exchange.json` cannot be read — for
    every zone, including one with nothing downstream. Without that file we
    cannot prove a zone has no consumers, so "no consumers" is not an answer
    we are entitled to give, and returning the own-zone list anyway would
    report `checks: green` having verified less than it claims. That is the
    precise defect this function exists to remove; reintroducing it as the
    error path would be worse than never having written it.
    """
    exchange = load(root)
    out = list(commands_for(zone))
    project = ZONE_PROJECT.get(zone)
    if project is None:
        return tuple(out)
    for consumer in exchange.consumers_of(project):
        for cmd in PROJECT_CHECKS.get(consumer, ()):
            if cmd not in out:
                out.append(cmd)
    return tuple(out)
```

- [ ] **Step 4: Point `run_checks` at it**

In `forge/forge/verify.py`, change the import on line 18:

```python
from .checks import commands_for
```

to:

```python
from .checks import all_commands
from .exchange import ExchangeError
```

and replace the body of `run_checks`:

```python
def run_checks(zone: str, root: Path | None, runner=None) -> CheckResult:
    """Run every command for the zone, stopping at the first failure.

    "Every command" includes the checks of projects downstream of this zone,
    so an unreadable `shared/exchange.json` is a failed check rather than a
    shorter list of commands.
    """
    run = runner or _default_runner
    try:
        commands = all_commands(zone, root or Path.cwd())
    except ExchangeError as exc:
        return CheckResult(False, (), f"cannot tell what this change could break: {exc}")
    ran: list[str] = []
    for cmd in commands:
        ran.append(" ".join(cmd))
        try:
            code, output = run(cmd, root)
        except Exception as exc:  # noqa: BLE001
            return CheckResult(False, tuple(ran), f"{ran[-1]} could not run: {exc}")
        if code != 0:
            return CheckResult(False, tuple(ran), output[-2000:])
    return CheckResult(True, tuple(ran), "")
```

- [ ] **Step 5: Fix the existing verify tests**

`forge/tests/test_verify.py` calls `run_checks(zone, tmp_path, ...)` with no `shared/exchange.json` in `tmp_path`, so those tests will now fail closed. Add this helper near the top of the file, after the imports, and call it on `tmp_path` in every test that calls `run_checks`:

```python
import json


def _exchange(root):
    """Give a tmp_path repo the declaration run_checks now requires."""
    shared = root / "shared"
    shared.mkdir(parents=True, exist_ok=True)
    (shared / "exchange.json").write_text(
        json.dumps({"publishes": {}, "consumes": []}), encoding="utf-8"
    )
    return root
```

An empty-but-valid declaration keeps those tests testing what they were written to test — command running and failure handling — without entangling them in the dependency graph. The test named `test_all_commands_must_pass` asserts `len(calls) == len(commands_for("scraper/")) == 2`; with an empty declaration `scraper/` still has no consumers, so that assertion stands unchanged.

- [ ] **Step 6: Run the whole suite**

```bash
cd forge && python -m pytest tests/ -q -W error
```

Expected: all tests pass, 301 + 15 (Task 3) + 11 (this task) = 327 or more.

- [ ] **Step 7: Mutation-check**

| Mutation | Test that must fail |
|---|---|
| `all_commands`: move `exchange = load(root)` below the `if project is None: return` | `a missing declaration raises even for a zone with no consumers` |
| `all_commands`: wrap `load(root)` in `try/except ExchangeError: return tuple(out)` | `an unreadable declaration raises rather than narrowing` |
| `all_commands`: delete the `if cmd not in out` guard | `a command needed twice is run once` |
| `all_commands`: append downstream before own | `the zone's own checks come first` |
| `PROJECT_CHECKS`: add a `"zomboid"` entry | `a consumer with no known checks adds nothing` |
| `verify.run_checks`: remove the `except ExchangeError` block | `run checks fails when the declaration is unreadable` (it would raise) |

- [ ] **Step 8: Verify against the real repo**

```bash
cd /home/user/Maz && PYTHONPATH=forge python -c "
from pathlib import Path
from forge.checks import all_commands
for cmd in all_commands('music/', Path('.')):
    print(' '.join(cmd))
"
```

Expected, in this order:
```
node music/tests/music-logic.test.js
node film/tests/film-logic.test.js
```

- [ ] **Step 9: Commit**

```bash
git add forge/forge/checks.py forge/forge/verify.py forge/tests/test_checks.py forge/tests/test_verify.py
git commit -m "A zone's checks now include everything downstream of it

Changing music/ runs film's tests too, because film declares it consumes
music/composer. Before this, the Forge could break SCRIPT FORGE, record
checks: green, and open a pull request describing verified work.

commands_for keeps its signature and its pure own-zone answer. The new
all_commands(zone, root) adds the downstream checks and raises when
shared/exchange.json cannot be read — for every zone, including ones
with nothing downstream, because without that file 'no consumers' is not
an answer we are entitled to give.

checks.py also had no test file of its own until now."
```

---

### Task 5: Fail early instead of after the Crew run

**Files:**
- Modify: `forge/forge/decide.py:108-170`
- Modify: `forge/forge/cli.py:141-152`
- Modify: `forge/forge/orchestrate.py:91`
- Modify: `forge/tests/test_decide.py`

**Interfaces:**
- Consumes: `forge.exchange.is_loadable` (Task 3).
- Produces: `decide(pulse, config, strikes=None, recent_zones=None, exchange_ok=True)`. When `exchange_ok` is `False`, every candidate is skipped with `config_error` and `chosen` is `None`.

**Read this before implementing.** The `exchange_ok=True` default is *not* the safety guarantee — Task 4's `run_checks` is, and it has no such default. This gate exists only so a bad declaration costs a quiet night instead of a wasted Crew run. Do not remove Task 4's check on the strength of this one.

- [ ] **Step 1: Write the failing tests**

Append to `forge/tests/test_decide.py`:

```python
def test_a_bad_exchange_skips_everything_as_config_error():
    # The Forge cannot tell what a change would break, so it declines to
    # choose. A quiet night is a correct night; a night that verifies less
    # than it claims is not.
    record = decide(_pulse(DOC, MUSIC), ForgeConfig(), exchange_ok=False)
    assert record["chosen"] is None
    assert record["skipped"]["config_error"] == record["considered"]


def test_a_bad_exchange_still_counts_what_it_considered():
    record = decide(_pulse(DOC, MUSIC), ForgeConfig(), exchange_ok=False)
    assert record["considered"] == 2


def test_a_good_exchange_changes_nothing():
    with_flag = decide(_pulse(DOC, MUSIC), ForgeConfig(), exchange_ok=True)
    without = decide(_pulse(DOC, MUSIC), ForgeConfig())
    assert with_flag == without
```

Read the existing `_pulse`, `DOC` and `MUSIC` definitions at the top of that file and reuse them; do not redefine them.

- [ ] **Step 2: Run to verify they fail**

```bash
cd forge && python -m pytest tests/test_decide.py -q
```

Expected: `TypeError: decide() got an unexpected keyword argument 'exchange_ok'`.

- [ ] **Step 3: Add the gate**

In `forge/forge/decide.py`, change the signature:

```python
def decide(
    pulse: dict,
    config: ForgeConfig,
    strikes: dict | None = None,
    recent_zones: list | None = None,
    exchange_ok: bool = True,
) -> dict:
```

Add to the docstring, after the existing paragraph:

```
    `exchange_ok` is the caller's answer to "can shared/exchange.json be
    read?". False skips every candidate as `config_error`: without that file
    the Forge cannot tell which projects a change could break, and choosing
    anyway would mean verifying less than the ledger claims. This is an
    early exit, not the safety guarantee — `verify.run_checks` fails closed
    on the same condition with no default to weaken it, which is why the
    default here can safely be True.
```

Then, immediately after `considered += 1` in the candidate loop, add:

```python
        if not exchange_ok:
            skipped["config_error"] += 1
            continue
```

- [ ] **Step 4: Run to verify they pass**

```bash
cd forge && python -m pytest tests/test_decide.py -q -W error
```

Expected: all pass.

- [ ] **Step 5: Pass the flag from both real callers**

In `forge/forge/cli.py`, add to the imports at the top:

```python
from .exchange import is_loadable
```

and change the `decide_step` call in the `decide` command (line 149):

```python
    record = decide_step(pulse, cfg, strikes=ledger_mod.strikes(r, cfg),
                         recent_zones=ledger_mod.recent_zones(r, cfg),
                         exchange_ok=is_loadable(r))
```

In `forge/forge/orchestrate.py`, add to the imports:

```python
from .exchange import is_loadable
```

and change the call on line 91:

```python
    record = decide_step(pulse, config, strikes=strikes,
                         recent_zones=recent_zones,
                         exchange_ok=is_loadable(root))
```

Read the surrounding lines first — the existing call's other keyword arguments and the name of the root variable in scope must be preserved exactly.

- [ ] **Step 6: Run the whole suite**

```bash
cd forge && python -m pytest tests/ -q -W error
```

Expected: all pass.

- [ ] **Step 7: Mutation-check**

| Mutation | Test that must fail |
|---|---|
| `if not exchange_ok:` → `if False:` | `a bad exchange skips everything as config error` |
| Put the `exchange_ok` check *before* `considered += 1` | `a bad exchange still counts what it considered` |
| `exchange_ok: bool = True` → `= False` | `a good exchange changes nothing` |

- [ ] **Step 8: Prove it end to end on the real repo**

```bash
cd /home/user/Maz
PYTHONPATH=forge python -m forge.cli sense >/dev/null
PYTHONPATH=forge python -m forge.cli decide | tail -3
mv shared/exchange.json /tmp/exchange.json.bak
PYTHONPATH=forge python -m forge.cli decide | tail -3
mv /tmp/exchange.json.bak shared/exchange.json
git checkout forge/ledger/ 2>/dev/null; rm -rf forge/state
git status --short
```

Expected: the first `decide` picks the robots.py job in zone `scraper/`. The second reports `config_error` equal to the number considered and picks nothing. `git status` is clean at the end.

- [ ] **Step 9: Commit**

```bash
git add forge/forge/decide.py forge/forge/cli.py forge/forge/orchestrate.py forge/tests/test_decide.py
git commit -m "Skip the night early when the dependency declaration is broken

An unreadable shared/exchange.json already fails the checks in VERIFY.
Discovering it there means burning a Crew run first. DECIDE now takes an
exchange_ok flag from its two real callers and skips every candidate as
config_error, which the ledger already records.

The flag defaults to True and that is deliberate: the safety guarantee
lives in run_checks, which fails closed with no default to weaken it.
This is an early exit, not the gate."
```

---

### Task 6: Say what changed

**Files:**
- Modify: `docs/FORGE.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: everything above. Produces: no code.

- [ ] **Step 1: Read the section that is now wrong**

```bash
grep -n "A safe zone only means something if a check backs it up" -A 12 docs/FORGE.md
```

That paragraph explains that a zone's checks are its own and that widening `safe_zones` without a check command leaves work unverified. It is still true, but it no longer tells the whole story.

- [ ] **Step 2: Add the downstream paragraph**

Immediately after that paragraph in `docs/FORGE.md`, add:

```markdown
**A zone's checks now include everything downstream of it.**
`shared/exchange.json` records which projects use each other — today only
that SCRIPT FORGE loads five of SONG FORGE's files. So a change in `music/`
runs `film/`'s tests as well as music's own, and `checks: green` means both
passed. Before this, the Forge could break SCRIPT FORGE, record green, and
open a pull request describing verified work; CI on that pull request caught
the break, but the ledger — the permanent record of the loop's judgement —
carried a false claim.

If `shared/exchange.json` is missing or malformed the Forge stops for the
night rather than falling back to a zone's own checks: it records
`config_error` and picks nothing. Falling back would mean verifying less
while still reporting green, which is the defect this exists to remove. A
quiet night is a correct night.

Adding a cross-project `<script>` tag without declaring it in
`shared/exchange.json` fails CI (`scripts/check-exchange.mjs`), so the
declaration cannot quietly rot.
```

- [ ] **Step 3: Update the README's Forge row**

`README.md` line 32 is the hub table's Forge row:

```markdown
| ⚒️ | **The Forge** | The repo's own nightly loop: reads the roadmap, CI and `TODO` markers, picks one task, hands it to Maz Crew, and opens a draft PR. Never pushes to the default branch, never merges. | [docs](docs/FORGE.md) |
```

Replace the description cell so it reads:

```markdown
| ⚒️ | **The Forge** | The repo's own nightly loop: reads the roadmap, CI and `TODO` markers, picks one task, hands it to Maz Crew, and opens a draft PR. Verifies against every project declared to depend on what it changed. Never pushes to the default branch, never merges. | [docs](docs/FORGE.md) |
```

Leave the rest of the table alone. Do not add a row for the Exchange — it is not an app on the hub, and `check-links.mjs` requires every manifest project to have a matching hub link.

- [ ] **Step 4: Verify the docs still pass their own checks**

```bash
node scripts/check-links.mjs && node scripts/check-exchange.mjs && node scripts/tests/check-exchange.test.mjs
cd forge && python -m pytest tests/ -q -W error
```

Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add docs/FORGE.md README.md
git commit -m "Document what green now covers

The operator's guide said a zone's checks are its own. They now include
the checks of every project declared to consume it, and a missing or
malformed declaration stops the night rather than quietly verifying
less."
```

---

## Self-Review

**Spec coverage.**

| Spec section | Task |
|---|---|
| `shared/exchange.json` — the declaration | 1 |
| Rule: consumes an id nobody publishes | 1 |
| Rule: project not in `shared/projects.js` | 1 |
| Rule: published file does not exist | 1 |
| Rule: declared link does not match the page, in order | 2 |
| Rule: undeclared cross-project `<script>` | 2 |
| Rule: contract file does not exist | 1 |
| Rule: project consumes its own id | 1 |
| `shared/` excluded from the coupling rule | 2 |
| `forge/forge/checks.py` widened; `ZONE_CHECKS` derived | 4 |
| `config_error` at DECIDE time | 5 |
| Fail closed rather than falling back | 4 (guarantee) and 5 (early exit) |
| Testing: fixture per failure rule | 1, 2 |
| Testing: mutation checks | every task |
| Docs | 6 |

**Type consistency.** `commands_for(zone)` keeps its one-argument signature throughout; `all_commands(zone, root)` is the two-argument form and is the only one that touches disk. `Exchange.consumers_of(project)` returns `tuple[str, ...]` and is called only from `all_commands`. `is_loadable(root) -> bool` is called only from `cli.py` and `orchestrate.py`. `ExchangeError` is raised in `exchange.py` and caught in `verify.run_checks` only.

**Known deliberate omissions.** Versioned published surfaces (`music/composer@2`), artifact storage, runtime message passing, and dependency declarations for Python or C++ — all listed out of scope in the spec. `music-ci.yml`'s path filter is left alone, also per the spec.
