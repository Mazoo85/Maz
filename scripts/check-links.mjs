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

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
// Keep this in step with check-exchange.mjs's SKIP_DIRS — both walk the
// repo for HTML, and a directory only one of them skips lets a file that
// is invisible to CI (this one is gitignored) turn a night red or green
// for the wrong reason, depending only on which checker happened to look.
const SKIP_DIRS = new Set([
  '.git', 'node_modules', 'build', 'dist', '__pycache__', '.venv', 'venv',
  '.pytest_cache', '.mypy_cache', '.claude',
  // Subagent working notes: gitignored scratch, absent on CI, and full of
  // quoted regexes and example paths that are not links at all.
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

/* ------------------------------------------------------- 1. every link */
const files = walk(ROOT).filter((f) => ['.html', '.htm', '.md'].includes(extname(f).toLowerCase()));

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
const manifestPath = join(ROOT, 'shared', 'projects.js');
if (!existsSync(manifestPath)) {
  errors.push('shared/projects.js is missing — the hub and the nav both read it');
} else {
  const { PROJECTS } = await import(pathToFileURL(manifestPath).href);
  const projects = PROJECTS ?? globalThis.MAZ_PROJECTS ?? [];
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
