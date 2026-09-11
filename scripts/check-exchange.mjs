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
// Keep this in step with check-links.mjs's SKIP_DIRS. Both walk the repo
// looking for HTML to scan, and the Forge now runs this script as a green
// gate on every zone (see forge/forge/checks.py's EXCHANGE_CHECK_CMD) — a
// directory only one of the two skips (like the gitignored `.superpowers/`
// scratch space) can turn a night red over a stray file CI never sees,
// because CI never checks that directory out at all.
const SKIP_DIRS = new Set([
  '.git', 'node_modules', 'build', 'dist', '__pycache__', '.venv', 'venv',
  '.pytest_cache', '.mypy_cache', '.claude',
  // Subagent working notes: gitignored scratch, absent on CI, and full of
  // example paths and fixture-shaped HTML that is not a real page.
  '.superpowers',
  'fixtures'
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
  let mod;
  try {
    mod = await import(pathToFileURL(projectsPath).href);
  } catch (err) {
    fail(`shared/projects.js failed to load: ${err.message}`);
  }
  if (mod) {
    const list = mod.PROJECTS ?? globalThis.MAZ_PROJECTS ?? [];
    if (!Array.isArray(list)) {
      fail('shared/projects.js: PROJECTS must be an array');
    } else {
      projectIds = new Set(list.map((p) => p?.id).filter(Boolean));
    }
  }
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
      continue;
    }
    /* A publisher's files must live under its own project, or the Forge
     * (which keys the dependency graph on `project`) is blind to a fully
     * declared, CI-green cross-project edge: e.g. music/composer publishing
     * a file that actually lives under madlibs/. */
    if (typeof entry.project === 'string' && !f.startsWith(`${entry.project}/`)) {
      fail(`${EXCHANGE_REL}: published id "${id}" names ${f}, which is outside ` +
           `its own project "${entry.project}" — every file it publishes must ` +
           `start with "${entry.project}/"`);
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

  /* A numeric id (e.g. 7 against a publishes key of "7") would resolve here
   * via JS's own-property coercion, but the Python reader on the Forge side
   * requires a string and rejects it — CI green, the nightly loop blind. */
  if (typeof c.id !== 'string') {
    fail(`${EXCHANGE_REL}: ${where} has an "id" that is not a string`);
    continue;
  }

  /* `publishes[c.id]` alone would resolve up the prototype chain, so an id
   * of "constructor", "toString", "valueOf" or "hasOwnProperty" would pass
   * the "nothing publishes this" check below for the wrong reason. */
  const published = Object.hasOwn(publishes, c.id) ? publishes[c.id] : undefined;
  if (!published) {
    fail(`${EXCHANGE_REL}: ${c.project} consumes "${c.id}", which nothing publishes`);
    continue;
  }
  if (published.project === c.project) {
    fail(`${EXCHANGE_REL}: project "${c.project}" consumes "${c.id}", its own published surface`);
  }
  if (typeof c.contract !== 'string' || !existsSync(join(ROOT, c.contract))) {
    fail(`${EXCHANGE_REL}: ${where} names contract ${c.contract}, which does not exist`);
  }
  if (c.page !== undefined && typeof c.page !== 'string') {
    fail(`${EXCHANGE_REL}: ${where} has a "page" that is not a string`);
  }
}

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
 * scanned: a README mentioning another project is prose, not coupling. The
 * whole relative reference is captured (not just its first `../` segment) so
 * that references nesting more than one level up (`../../music/...`) resolve
 * to the real project instead of being pattern-matched as project "..".
 *
 * Only <script src=...> and <link href=...> are code/asset coupling — a
 * <script> executes in the loading page and a <link> (stylesheet, etc.) is
 * fetched and applied to it. <a href> and <img src> are ordinary navigation
 * and content, not coupling, and are deliberately NOT matched here, however
 * deep their "../" nesting: linking to another project's page, or embedding
 * its image, does not run its code or make this page depend on its files. A
 * bare "src|href" on any tag over-flags those, and the old anchor on a
 * literal "../" under-flagged the opposite case: a page at repo depth 0 (the
 * hub index.html) can reach another project with no "../" at all
 * ("music/js/theory.js"), which is exactly the real coupling this rule
 * exists to catch. */
const SCRIPT_SRC = /<script\b[^>]*\bsrc=["']([^"']+)["'][^>]*>/gi;
const LINK_HREF = /<link\b[^>]*\bhref=["']([^"']+)["'][^>]*>/gi;

/* A reference with a scheme (data:, http:, https:, mailto:, ...) or a
 * protocol-relative "//host/..." isn't a repo-relative file at all — most
 * commonly the inline `data:image/svg+xml,...` favicons every page uses.
 * Resolving one of those against the page's directory would produce a
 * meaningless "project" and false-fail every page. */
const EXTERNAL_REF = /^(?:[a-z][a-z0-9+.-]*:|\/\/)/i;

/* `full` is the absolute path of the page doing the referencing: each
 * reference is resolved relative to the page's own directory into a
 * repo-relative path, and the project is read off that resolved path —
 * never off the raw `../` text — so depth and query strings can't fool it. */
function crossRefs(text, full) {
  const out = [];
  const pageDir = dirname(full);
  const raws = [];
  for (const m of text.matchAll(SCRIPT_SRC)) raws.push(m[1]);
  for (const m of text.matchAll(LINK_HREF)) raws.push(m[1]);
  for (const raw of raws) {
    if (EXTERNAL_REF.test(raw)) continue;
    const clean = raw.split('?')[0].split('#')[0];
    const path = relative(ROOT, resolve(pageDir, clean)).split('\\').join('/');
    out.push({ project: path.split('/')[0], path });
  }
  return out;
}

const pages = walkHtml(ROOT);
const declaredByPage = new Map();
for (const c of consumes) {
  if (c?.via === 'script' && typeof c.page === 'string') {
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
  const loaded = crossRefs(readFileSync(full, 'utf8'), full).map((r) => r.path);
  for (const entry of entries) {
    /* entry.id may name a published id whose "files" failed validation back
     * in section 3 (not an array at all) — fall back to [] rather than let a
     * non-array reach .filter()/.map() below. */
    const rawFiles = publishes[entry.id]?.files;
    const want = Array.isArray(rawFiles) ? rawFiles : [];
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
  if (typeof c?.page !== 'string') continue;
  const rawFiles = publishes[c.id]?.files;
  const want = Array.isArray(rawFiles) ? rawFiles : [];
  if (!declaredFiles.has(c.page)) declaredFiles.set(c.page, new Set());
  for (const f of want) declaredFiles.get(c.page).add(f);
}

for (const full of pages) {
  const page = relative(ROOT, full).split('\\').join('/');
  const owner = page.includes('/') ? page.split('/')[0] : null;
  const allowed = declaredFiles.get(page) ?? new Set();
  for (const ref of crossRefs(readFileSync(full, 'utf8'), full)) {
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
