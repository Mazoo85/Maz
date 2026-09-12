#!/usr/bin/env node
/*
 * maz-inventory — list every program and artifact in this repository, measure
 * what each one is still missing, and rank the work that closes the gaps.
 *
 *   node tools/inventory/main.mjs            # print the report to stdout
 *   node tools/inventory/main.mjs --write    # write docs/INVENTORY.md + docs/inventory.json
 *   node tools/inventory/main.mjs --check    # fail if those files are out of date
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
      source: t.source
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

  const md = render(model) ;
  const js = JSON.stringify(toJson(model), null, 2) + '\n';

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
    mkdirSync(dirname(MD_PATH), { recursive: true });
    writeFileSync(MD_PATH, md);
    writeFileSync(JSON_PATH, js);
    process.stdout.write(`wrote docs/INVENTORY.md and docs/inventory.json\n${summary(model)}\n`);
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
