#!/usr/bin/env node
/*
 * check-links — proves the repo is actually wired together.
 *
 * It walks every HTML and Markdown file and checks that:
 *   1. every local link and asset reference resolves to a real file;
 *   2. every project in shared/projects.js resolves from the repo root;
 *   3. every browser app is reachable from the hub, and links back to it via
 *      the shared nav.
 *
 * No dependencies — run it with `node scripts/check-links.mjs` from anywhere.
 * Exits non-zero (and prints what broke) if any of the above fails.
 */

import { readFileSync, readdirSync, statSync, existsSync } from 'node:fs';
import { join, dirname, resolve, relative, extname, posix } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

// LINKS_ROOT points the checker at another tree, which is how its own tests run
// it against a throwaway repo. Same override check-exchange.mjs takes.
const ROOT = process.env.LINKS_ROOT
  ? resolve(process.env.LINKS_ROOT)
  : resolve(dirname(fileURLToPath(import.meta.url)), '..');
// This list is intentionally NOT identical to check-exchange.mjs's SKIP_DIRS,
// even though both walk the repo for HTML — the two checkers skip `fixtures` for
// different reasons, and only one of those reasons applies here. This checker
// cares about dangling links, and the scraper's test fixtures are fake pages full
// of deliberately dangling URLs — real fixture noise for *this* rule, so
// `fixtures` stays skipped. check-exchange.mjs cares about undeclared
// cross-project <script>/<link> coupling, which a fixture page can produce
// exactly as easily as a real one, so it does NOT skip `fixtures` — see the
// comment on its own SKIP_DIRS. Do not "fix" this divergence back into alignment.
const SKIP_DIRS = new Set([
  '.git', 'node_modules', 'build', 'dist', '__pycache__', '.venv', 'venv',
  '.pytest_cache', '.mypy_cache', '.claude',
  // Subagent working notes: gitignored scratch, absent on CI, and full of quoted
  // regexes and example paths that are not links at all. Skipping it keeps a
  // stray local file from turning a locally-green night red for a failure CI
  // could never reproduce.
  '.superpowers',
  // Scraper test fixtures are fake pages full of deliberately dangling URLs.
  'fixtures'
]);

const errors = [];
const notes = [];

function fail(file, message) {
  errors.push(`${relative(ROOT, file) || '.'}: ${message}`);
}

/* ------------------------------------------------------------------ walk */
function walk(dir, out = []) {
  for (const entry of readdirSync(dir)) {
    if (SKIP_DIRS.has(entry)) continue;
    const full = join(dir, entry);
    const st = statSync(full);
    if (st.isDirectory()) walk(full, out);
    else out.push(full);
  }
  return out;
}

/* --------------------------------------------------------------- extract */
// href="..." / src="..." — single, double or unquoted.
const ATTR_RE = /(?:href|src)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s">]+))/gi;
// [label](target "optional title") — the target stops at whitespace or ')'.
const MD_RE = /\[[^\]]*\]\(\s*<?([^)\s>]+)>?(?:\s+["'][^)]*["'])?\s*\)/g;

function linksIn(file) {
  const text = readFileSync(file, 'utf8');
  const ext = extname(file).toLowerCase();
  const found = [];

  if (ext === '.html' || ext === '.htm') {
    // Blank out script/style bodies first: `card.href = p.path` is JavaScript,
    // not a link, and would otherwise be reported as broken.
    const markup = text
      .replace(/<script\b([^>]*)>[\s\S]*?<\/script>/gi, '<script$1></script>')
      .replace(/<style\b([^>]*)>[\s\S]*?<\/style>/gi, '<style$1></style>');
    for (const m of markup.matchAll(ATTR_RE)) found.push(m[1] ?? m[2] ?? m[3]);
  } else if (ext === '.md') {
    // Ignore fenced code blocks: they hold shell snippets, not real links.
    const body = text.replace(/```[\s\S]*?```/g, '');
    for (const m of body.matchAll(MD_RE)) found.push(m[1]);
  }
  return found;
}

function isExternal(target) {
  return (
    /^[a-z][a-z0-9+.-]*:/i.test(target) || // http:, https:, mailto:, data:, javascript:
    target.startsWith('//') ||
    target.startsWith('#') ||
    target.startsWith('{{') || // template placeholder
    target.trim() === ''
  );
}

/* Resolve a link and say whether something real is there. */
function resolves(fromFile, target) {
  const clean = decodeURI(target.split('#')[0].split('?')[0]);
  if (clean === '') return true; // pure anchor
  const base = target.startsWith('/') ? ROOT : dirname(fromFile);
  const abs = resolve(base, target.startsWith('/') ? '.' + clean : clean);

  if (!existsSync(abs)) return false;
  if (statSync(abs).isDirectory()) {
    // A directory link is fine if a browser or GitHub would render something.
    return ['index.html', 'README.md'].some((f) => existsSync(join(abs, f)));
  }
  return true;
}

/* -------------------------------------------------------- the manifest */
/* Loaded before anything else because it is the list of what exists: which
 * directories are scanned below comes from it, so adding a project to the
 * manifest is the only thing anyone has to remember. */
const manifestPath = join(ROOT, 'shared', 'projects.js');
let projects = [];
let manifestMissing = !existsSync(manifestPath);
if (!manifestMissing) {
  const mod = await import(pathToFileURL(manifestPath).href + `?t=${Date.now()}`);
  projects = mod.PROJECTS ?? mod.default?.PROJECTS ?? globalThis.MAZ_PROJECTS ?? [];
}

/* ------------------------------------------------------- 1. every link */
/* This checks the MAZ ARCADE web projects — the hub, the browser apps and
 * their docs. The engine's own C++ docs tree is not ours to police: it is far
 * larger, it has its own CI, and its API reference legitimately contains
 * things like [links](url) as prose describing Markdown syntax, which is not a
 * link at all. So scan the arcade roots rather than the whole repository.
 *
 * The roots come from the manifest rather than a list written out here. They
 * used to be written out here, and CODA PICS was added to the arcade without
 * being added to the list, so for as long as it existed none of its links were
 * checked by the gate whose whole job is checking links. Derived this way, a
 * project is covered the moment it is real. */
// Not every project's manifest path is a directory. A browser app points at
// its own folder, which is walked; a code project points at the single document
// that introduces it (`scraper/README.md`, `docs/ROADMAP.md`), and that document
// alone is what gets checked. Walking its folder instead would drag in all of
// docs/ — including the generated API reference, which is full of `[links](url)`
// written as prose about Markdown syntax and is not ours to police.
//
// `forge` is not in the manifest at all — it is machinery rather than something
// to open — but its docs link out like any other, so it stays named here.
const FIXED_ROOTS = ['shared', 'docs/superpowers', 'forge'];
const rootDirs = new Set(FIXED_ROOTS);
const rootFiles = new Set(['index.html', 'README.md']);
for (const p of projects) {
  const rel = String(p.path || '').replace(/\/$/, '');
  if (!rel) continue;
  const abs = join(ROOT, rel);
  if (!existsSync(abs)) continue;
  (statSync(abs).isDirectory() ? rootDirs : rootFiles).add(rel);
  // A project's `docs` entry is its front door even when `path` is the app.
  if (p.docs) rootFiles.add(String(p.docs));
}
const scanned = [...rootDirs]
  .map((r) => join(ROOT, r))
  .filter((d) => existsSync(d) && statSync(d).isDirectory())
  .flatMap((d) => walk(d));
for (const f of rootFiles) {
  const abs = join(ROOT, f);
  if (existsSync(abs) && !statSync(abs).isDirectory()) scanned.push(abs);
}

// A file can be reached twice — README.md is both a named root and inside a
// walked directory — and counting its links twice would misreport the total.
const files = [...new Set(scanned)]
  .filter((f) => ['.html', '.htm', '.md'].includes(extname(f).toLowerCase()));

let checked = 0;
for (const file of files) {
  for (const target of linksIn(file)) {
    if (isExternal(target)) continue;
    checked++;
    if (!resolves(file, target)) fail(file, `broken link → ${target}`);
  }
}
notes.push(`${checked} local links across ${files.length} HTML/Markdown files`);

/* --------------------------------------------- 2. the project manifest */
if (manifestMissing) {
  errors.push('shared/projects.js is missing — the hub and the nav both read it');
} else {
  if (!projects.length) errors.push('shared/projects.js exported no projects');

  const required = ['id', 'name', 'kind', 'path', 'tag', 'accent', 'blurb'];
  const seen = new Set();
  for (const p of projects) {
    for (const key of required) {
      if (!p?.[key]) errors.push(`shared/projects.js: project ${p?.id ?? '?'} is missing "${key}"`);
    }
    if (seen.has(p.id)) errors.push(`shared/projects.js: duplicate project id "${p.id}"`);
    seen.add(p.id);
    if (!resolves(join(ROOT, 'index.html'), p.path)) {
      errors.push(`shared/projects.js: project "${p.id}" points at ${p.path}, which does not exist`);
    }
    if (p.docs && !resolves(join(ROOT, 'index.html'), p.docs)) {
      errors.push(`shared/projects.js: project "${p.id}" docs point at ${p.docs}, which does not exist`);
    }
  }
  notes.push(`${projects.length} projects in the manifest, all resolving`);

  /* --------------------------------- 3. the hub <-> app round trip */
  const hub = join(ROOT, 'index.html');
  if (!existsSync(hub)) {
    errors.push('index.html (the hub) is missing');
  } else {
    const hubText = readFileSync(hub, 'utf8');
    if (!hubText.includes('shared/projects.js')) {
      errors.push('index.html does not load shared/projects.js, so the hub cannot list anything');
    }
    // The noscript fallback must stay in step with the manifest.
    for (const p of projects) {
      if (!hubText.includes(`"${p.path}"`)) {
        errors.push(`index.html noscript fallback is missing a link to ${p.path}`);
      }
    }
  }

  // Every browser app must carry the shared nav so you can get back out.
  for (const p of projects) {
    const entry = join(ROOT, p.path, 'index.html');
    if (!existsSync(entry)) continue; // docs-only projects have no page to wire
    const text = readFileSync(entry, 'utf8');
    if (!text.includes('shared/maz-nav.js')) {
      fail(entry, 'browser app does not include shared/maz-nav.js — it is a dead end');
    }
    if (!new RegExp(`data-current=["']${p.id}["']`).test(text)) {
      fail(entry, `nav is not told which project it is (expected data-current="${p.id}")`);
    }
  }
}

/* ----------------------------------------------------------- report */
if (errors.length) {
  console.error(`\n✖ ${errors.length} problem${errors.length === 1 ? '' : 's'}:\n`);
  for (const e of errors) console.error('  - ' + e);
  console.error('');
  process.exit(1);
}

console.log('✓ everything is connected');
for (const n of notes) console.log('  · ' + n);
