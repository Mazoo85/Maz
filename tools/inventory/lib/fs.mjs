/*
 * Filesystem helpers shared by the scanners.
 *
 * Deliberately tiny and dependency-free: the inventory must run on a bare
 * `node` with nothing installed, exactly like scripts/check-links.mjs and
 * scripts/check-exchange.mjs, because it runs in CI alongside them.
 */

import { readFileSync, readdirSync, statSync, existsSync } from 'node:fs';
import { join } from 'node:path';

// Directories that can never hold an artifact worth cataloguing: build output,
// vendored code, caches, and agent scratch. Kept in one place so every scanner
// skips the same set.
export const SKIP_DIRS = new Set([
  '.git', 'node_modules', 'build', 'dist', '__pycache__', '.venv', 'venv',
  '.pytest_cache', '.mypy_cache', '.superpowers', 'third_party'
]);

export function readText(path) {
  try {
    return readFileSync(path, 'utf8');
  } catch {
    return '';
  }
}

export function isDir(path) {
  try {
    return statSync(path).isDirectory();
  } catch {
    return false;
  }
}

export function exists(path) {
  return existsSync(path);
}

/** Immediate sub-directory names of `dir`, sorted, skipping SKIP_DIRS. Never throws. */
export function subdirs(dir) {
  try {
    return readdirSync(dir, { withFileTypes: true })
      .filter((e) => e.isDirectory() && !SKIP_DIRS.has(e.name))
      .map((e) => e.name)
      .sort();
  } catch {
    return [];
  }
}

/** Immediate file names of `dir` matching `re`, sorted. Never throws. */
export function filesIn(dir, re = /./) {
  try {
    return readdirSync(dir, { withFileTypes: true })
      .filter((e) => e.isFile() && re.test(e.name))
      .map((e) => e.name)
      .sort();
  } catch {
    return [];
  }
}

/** Every file under `dir`, recursively, as paths relative to `dir`. Never throws. */
export function walkFiles(dir, re = /./, rel = '') {
  const out = [];
  let entries;
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const e of entries.sort((a, b) => (a.name < b.name ? -1 : 1))) {
    const childRel = rel ? `${rel}/${e.name}` : e.name;
    if (e.isDirectory()) {
      if (SKIP_DIRS.has(e.name)) continue;
      out.push(...walkFiles(join(dir, e.name), re, childRel));
    } else if (e.isFile() && re.test(e.name)) {
      out.push(childRel);
    }
  }
  return out;
}

/** Non-blank line count. The repo's one honest, cheap size signal. */
export function countLines(text) {
  if (!text) return 0;
  return text.split('\n').filter((l) => l.trim() !== '').length;
}
