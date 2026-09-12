/*
 * Keep the catalogue current while the repository is being worked on.
 *
 * The inventory was built to run once and be checked in CI, which makes it true
 * at every commit and stale at every moment in between. That is the wrong shape
 * for how this repo is actually used: several sessions edit it at once, and a
 * catalogue that is only right at commit time is a catalogue nobody can trust
 * mid-change — including the Forge, which reads docs/inventory.json to decide
 * what to work on.
 *
 * So: watch the directories the scan reads, and rewrite the outputs whenever
 * they change. Three things this has to get right, because it runs unattended
 * beside other writers:
 *
 *   - It must never write a file whose content has not changed. The outputs are
 *     inside a watched tree, so an unconditional write is a loop.
 *   - It must debounce. A git checkout or a merge touches hundreds of files in
 *     a burst, and scanning once per file would be both wasteful and wrong —
 *     the tree is only consistent once the burst ends.
 *   - It must survive another process writing the same outputs. Another session
 *     regenerating them looks exactly like a source change; a re-scan produces
 *     the same bytes, the write is skipped, and nothing oscillates.
 */

import { watch as fsWatch } from 'node:fs';
import { join } from 'node:path';
import { exists } from './fs.mjs';

// Directories worth watching: everything the scanners read. `docs` is included
// because a doc's existence and its links are catalogued — but the two
// generated files inside it are ignored below, or writing them would wake us.
export const WATCH_ROOTS = [
  'apps', 'engine', 'tests', 'tools', 'scripts', 'shared', '.github'
];

// Plus the browser projects and Python tools, discovered rather than listed so
// a project added by another session is watched without editing this file.
export function watchRoots(root, model) {
  const fromModel = [
    ...model.webApps.map((w) => w.path),
    ...model.pyTools.map((t) => t.path)
  ];
  return [...new Set([...WATCH_ROOTS, ...fromModel, 'docs'])]
    .filter((r) => exists(join(root, r)));
}

// Changes to these never mean the repository changed — they ARE the answer.
const GENERATED = new Set(['INVENTORY.md', 'inventory.json']);

// Editors write a save through a swarm of temporary files; none is a real edit.
const IGNORED = /(^\.|~$|\.swp$|\.tmp$|^4913$|\.lock$)/;

function interesting(filename) {
  if (!filename) return true;         // some platforms report no name; assume real
  const base = filename.split('/').pop();
  if (GENERATED.has(base)) return false;
  return !IGNORED.test(base);
}

/**
 * Watch `roots` and call `onChange` once per quiet burst.
 * Returns a stop() that closes every watcher.
 */
export function watchTree(root, roots, onChange, debounceMs = 400) {
  const watchers = [];
  let timer = null;

  const fire = () => {
    timer = null;
    onChange();
  };

  for (const rel of roots) {
    try {
      const w = fsWatch(join(root, rel), { recursive: true }, (_event, filename) => {
        if (!interesting(filename)) return;
        if (timer) clearTimeout(timer);
        timer = setTimeout(fire, debounceMs);
      });
      w.on('error', () => { /* a directory removed mid-watch is not fatal */ });
      watchers.push(w);
    } catch {
      // Recursive watching is unavailable on some platforms and some
      // filesystems. Losing one root costs live updates for that subtree, not
      // the whole mode, and --check in CI is still the backstop.
    }
  }

  return () => {
    if (timer) clearTimeout(timer);
    for (const w of watchers) {
      try { w.close(); } catch { /* already closed */ }
    }
  };
}
