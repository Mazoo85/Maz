#!/usr/bin/env node
/*
 * maz-inventory — list every program and artifact in this repository, measure
 * what each one is still missing, and rank the work that closes the gaps.
 *
 *   node tools/inventory/main.mjs            # print the report to stdout
 *   node tools/inventory/main.mjs --write    # write docs/INVENTORY.md + docs/inventory.json
 *   node tools/inventory/main.mjs --check    # fail if those files are out of date
 *   node tools/inventory/main.mjs --watch    # keep them current while you work
 *   node tools/inventory/main.mjs --status   # which tree this describes, and is it current
 *   node tools/inventory/main.mjs --summary  # just the numbers
 *   node tools/inventory/main.mjs --json     # the machine-readable model to stdout
 *
 * No dependencies — it runs on a bare `node`, like the other repo gates, so it
 * works in CI and on a machine with no Vulkan SDK and no Python.
 *
 * INVENTORY_ROOT overrides the repository root, which is how the tests point it
 * at a throwaway repo in a temp directory.
 */

import { writeFileSync, readFileSync, mkdirSync } from 'node:fs';
import { join, dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildModel } from './lib/model.mjs';
import { render } from './lib/render.mjs';
import { watchTree, watchRoots } from './lib/watch.mjs';
import { branchState, dirtyPaths, isRepo } from './lib/git.mjs';

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(process.env.INVENTORY_ROOT || join(HERE, '..', '..'));

const MD_PATH = join(ROOT, 'docs', 'INVENTORY.md');
const JSON_PATH = join(ROOT, 'docs', 'inventory.json');

/**
 * The machine-readable view. Deliberately a *projection*, not the whole model:
 * the Forge and any other consumer need the queue and the headline counts, and
 * dumping 690 module records as well would make a 2 MB file that churns on
 * every edit to a header comment.
 */
function toJson(model) {
  return {
    schema: 1,
    stats: model.stats,
    queue: model.queue.map((t) => ({
      id: t.id,
      priority: t.priority,
      kind: t.kind,
      check: t.check,
      title: t.title,
      detail: t.detail,
      paths: t.paths,
      source: t.source,
      // Grouped tasks carry their members so a consumer can fan out over them —
      // "38 apps need a headless mode" is not one night's work, but any one of
      // those 38 apps is. memberCount is the true total; members is capped.
      ...(t.members ? { memberCount: t.memberCount, members: t.members } : {})
    })),
    pairings: model.pairings.map((p) => ({
      id: p.id, title: p.title, from: p.from, to: p.to, effort: p.effort, value: p.value
    })),
    stalePairings: model.stalePairings.map((p) => ({ id: p.id, missing: p.missing }))
  };
}

function summary(model) {
  const s = model.stats;
  return [
    `artifacts        ${s.totalArtifacts}`,
    `  apps           ${s.apps}`,
    `  engine modules ${s.engineModules} across ${s.engineSubsystems} subsystems`,
    `  browser apps   ${s.webApps}`,
    `  python tools   ${s.pyTools}`,
    `  ci gates       ${s.gates}`,
    `  build tools    ${s.buildTools}`,
    `  docs           ${s.docs}`,
    `checks passing   ${s.checksPassed}/${s.checksTotal}`,
    `open tasks       ${s.openTasks}`,
    `stale pairings   ${model.stalePairings.length}`
  ].join('\n');
}

function warnStale(model) {
  for (const st of model.stalePairings) {
    process.stderr.write(
      `warning: pairing "${st.id}" names artifacts that no longer exist: ${st.missing.join(', ')}\n` +
      `         fix or remove it in tools/inventory/lib/synergy.mjs\n`);
  }
}

/**
 * Write both outputs, but only the ones whose content actually changed.
 *
 * Returns what it wrote. Skipping an identical write is not an optimisation:
 * --watch watches a tree the outputs live inside, so an unconditional write is
 * a loop, and another session regenerating the same bytes must not look like a
 * change.
 */
function writeOutputs(md, js) {
  const wrote = [];
  mkdirSync(dirname(MD_PATH), { recursive: true });
  if (readIfPresent(MD_PATH) !== md) {
    writeFileSync(MD_PATH, md);
    wrote.push('docs/INVENTORY.md');
  }
  if (readIfPresent(JSON_PATH) !== js) {
    writeFileSync(JSON_PATH, js);
    wrote.push('docs/inventory.json');
  }
  return wrote;
}


/**
 * Where this checkout stands, and whether the committed catalogue still
 * describes it.
 *
 * Deliberately not written into docs/INVENTORY.md: that file's content has to
 * stay byte-identical for an unchanged tree or `--check` fails on every commit.
 * Branch and commit are facts about the moment, so they are printed on demand
 * instead.
 */
async function status(model, md, js) {
  const lines = [];
  const fresh = readIfPresent(MD_PATH) === md && readIfPresent(JSON_PATH) === js;

  if (isRepo(ROOT)) {
    const g = branchState(ROOT);
    lines.push(`branch      ${g.branch}${g.isTrunk ? '  (the trunk)' : ''} at ${g.head}`);
    if (g.behind === null) {
      lines.push(`trunk       ${g.base} is not in this checkout — run: git fetch origin main`);
    } else {
      lines.push(`vs ${g.base}  ${g.behind} behind, ${g.ahead} ahead`);
      // CLAUDE.md records what happened the last time nobody read this number:
      // work aimed at the engine landed on a branch that could not reach it.
      if (g.behind > 0) {
        lines.push(`            ${g.behind} commit${g.behind === 1 ? '' : 's'} landed on the trunk ` +
                   'while this branch was being written — merge before trusting a comparison');
      }
    }
    const dirty = dirtyPaths(ROOT, ['apps/', 'engine/', 'tests/', 'tools/', 'scripts/', 'shared/', 'docs/']);
    lines.push(`uncommitted ${dirty.length} scanned path${dirty.length === 1 ? '' : 's'}` +
               (dirty.length ? `: ${dirty.slice(0, 4).join(', ')}${dirty.length > 4 ? ', …' : ''}` : ''));
  } else {
    lines.push('branch      (not a git checkout)');
  }

  lines.push(`catalogue   ${fresh ? 'matches this tree' : 'STALE — run: node tools/inventory/main.mjs --write'}`);
  lines.push('');
  lines.push(summary(model));
  return lines.join('\n');
}

/**
 * Keep the two outputs current while the repository is being worked on.
 *
 * Runs until interrupted. Prints a line only when something actually changed,
 * so a long quiet session says nothing rather than scrolling.
 */
async function watch(initialModel, initialMd, initialJs) {
  let model = initialModel;
  writeOutputs(initialMd, initialJs);

  const roots = watchRoots(ROOT, model);
  process.stdout.write(
    `watching ${roots.length} director${roots.length === 1 ? 'y' : 'ies'} — ` +
    `${model.stats.totalArtifacts} artifacts, ${model.stats.openTasks} open tasks\n` +
    'ctrl-c to stop\n');

  let running = false;
  let again = false;

  const rescan = async () => {
    // A scan takes about a second. If the tree changes while one is running,
    // remember to go round again rather than starting a second scan on top of
    // it — otherwise a busy merge queues dozens of overlapping scans.
    if (running) { again = true; return; }
    running = true;
    try {
      const next = await buildModel(ROOT);
      const md = render(next);
      const js = JSON.stringify(toJson(next), null, 2) + '\n';
      const wrote = writeOutputs(md, js);
      if (wrote.length) {
        const before = model.stats;
        const after = next.stats;
        const moved = [];
        if (after.totalArtifacts !== before.totalArtifacts) {
          moved.push(`artifacts ${before.totalArtifacts} -> ${after.totalArtifacts}`);
        }
        if (after.openTasks !== before.openTasks) {
          moved.push(`open tasks ${before.openTasks} -> ${after.openTasks}`);
        }
        if (after.checksPassed !== before.checksPassed) {
          moved.push(`checks ${before.checksPassed} -> ${after.checksPassed}`);
        }
        process.stdout.write(
          `${new Date().toTimeString().slice(0, 8)}  updated` +
          `${moved.length ? ' — ' + moved.join(', ') : ''}\n`);
        for (const st of next.stalePairings) {
          process.stdout.write(`          warning: pairing "${st.id}" names ${st.missing.join(', ')}\n`);
        }
      }
      model = next;
    } catch (err) {
      // A scan during a half-finished write can legitimately fail. Say so and
      // keep watching; the next change will try again.
      process.stdout.write(`${new Date().toTimeString().slice(0, 8)}  scan failed: ${err.message}\n`);
    } finally {
      running = false;
      if (again) { again = false; await rescan(); }
    }
  };

  const stop = watchTree(ROOT, roots, () => { void rescan(); });

  return new Promise((resolve) => {
    const finish = () => {
      stop();
      process.stdout.write('\nstopped watching\n');
      resolve(0);
    };
    process.on('SIGINT', finish);
    process.on('SIGTERM', finish);
  });
}

function readIfPresent(path) {
  try {
    return readFileSync(path, 'utf8');
  } catch {
    return null;
  }
}

async function main(argv) {
  const flags = new Set(argv.slice(2));
  const model = await buildModel(ROOT);

  if (flags.has('--summary')) {
    process.stdout.write(summary(model) + '\n');
    warnStale(model);
    return 0;
  }

  if (flags.has('--json')) {
    process.stdout.write(JSON.stringify(toJson(model), null, 2) + '\n');
    return 0;
  }

  const md = render(model);
  const js = JSON.stringify(toJson(model), null, 2) + '\n';

  if (flags.has('--status')) {
    process.stdout.write((await status(model, md, js)) + '\n');
    warnStale(model);
    return 0;
  }

  if (flags.has('--watch')) {
    return watch(model, md, js);
  }

  if (flags.has('--check')) {
    const problems = [];
    if (readIfPresent(MD_PATH) !== md) {
      problems.push(`docs/INVENTORY.md is out of date — regenerate with:\n    node tools/inventory/main.mjs --write`);
    }
    if (readIfPresent(JSON_PATH) !== js) {
      problems.push(`docs/inventory.json is out of date — regenerate with:\n    node tools/inventory/main.mjs --write`);
    }
    for (const st of model.stalePairings) {
      problems.push(`pairing "${st.id}" names artifacts that no longer exist: ${st.missing.join(', ')}\n    Fix or remove it in tools/inventory/lib/synergy.mjs`);
    }
    if (problems.length) {
      process.stderr.write('inventory check failed:\n\n' + problems.map((p) => '  - ' + p).join('\n\n') + '\n');
      return 1;
    }
    process.stdout.write(`inventory ok — ${model.stats.totalArtifacts} artifacts, ${model.stats.openTasks} open tasks\n`);
    return 0;
  }

  if (flags.has('--write')) {
    const wrote = writeOutputs(md, js);
    process.stdout.write(
      `${wrote.length ? 'wrote ' + wrote.join(' and ') : 'already up to date'}\n${summary(model)}\n`);
    warnStale(model);
    // Writing the report is not the gate. A stale pairing is a real problem, but
    // refusing to exit 0 here would mean the command that FIXES a stale report
    // reports failure — so it warns, and `--check` is what fails the build.
    return 0;
  }

  process.stdout.write(md);
  return 0;
}

main(process.argv).then(
  (code) => { process.exitCode = code; },
  (err) => {
    process.stderr.write(`inventory failed: ${err && err.stack ? err.stack : err}\n`);
    process.exitCode = 2;
  }
);
