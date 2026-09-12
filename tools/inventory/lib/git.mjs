/*
 * What git knows about where this checkout is standing.
 *
 * The inventory describes a tree, and in this repo several Claude sessions work
 * on several branches at once. A catalogue that does not say WHICH tree it
 * describes is the kind of confidently wrong document this whole tool exists to
 * prevent — so the volatile facts live here, are read live, and are deliberately
 * NOT written into docs/INVENTORY.md, whose content has to stay deterministic
 * or `--check` would fail on every commit.
 */

import { execFileSync } from 'node:child_process';

function git(root, args) {
  try {
    return execFileSync('git', args, {
      cwd: root,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore']
    }).trim();
  } catch {
    return '';
  }
}

/**
 * Where this checkout stands relative to the trunk.
 *
 * `behind` is the number worth reading. CLAUDE.md records what happened the
 * last time nobody read it: the repo carried two mainlines for months, and work
 * aimed at the engine landed on a branch 1,339 commits behind it, where it
 * could never reach the engine. A branch that looks like a trunk may not be one.
 */
export function branchState(root, trunk = 'main') {
  const branch = git(root, ['rev-parse', '--abbrev-ref', 'HEAD']) || '(detached)';
  const head = git(root, ['rev-parse', '--short', 'HEAD']);

  // Prefer the remote trunk: a local `main` can itself be stale.
  const base = git(root, ['rev-parse', '--verify', '--quiet', `origin/${trunk}`])
    ? `origin/${trunk}`
    : trunk;

  let behind = null;
  let ahead = null;
  const counts = git(root, ['rev-list', '--left-right', '--count', `${base}...HEAD`]);
  const parts = counts.split(/\s+/).filter(Boolean);
  if (parts.length === 2) {
    behind = Number(parts[0]);
    ahead = Number(parts[1]);
  }

  return { branch, head, base, behind, ahead, isTrunk: branch === trunk };
}

/** Tracked paths with uncommitted changes, limited to what the inventory scans. */
export function dirtyPaths(root, prefixes) {
  const out = git(root, ['status', '--porcelain', '--untracked-files=normal']);
  if (!out) return [];
  return out
    .split('\n')
    .map((line) => line.slice(3).trim())
    .filter(Boolean)
    // A rename reads "old -> new"; the new name is what exists now.
    .map((p) => (p.includes(' -> ') ? p.split(' -> ')[1] : p))
    .filter((p) => prefixes.some((prefix) => p.startsWith(prefix)));
}

/** True when this looks like a git checkout at all. */
export function isRepo(root) {
  return git(root, ['rev-parse', '--is-inside-work-tree']) === 'true';
}
