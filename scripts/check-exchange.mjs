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
