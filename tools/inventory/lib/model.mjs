/*
 * Assemble the whole picture: scan everything, check everything, cross-link
 * everything, and rank the work that falls out of it.
 *
 * Everything downstream (the Markdown report, the JSON the Forge reads, the
 * tests) is a view over the single object this returns, so there is one place
 * where "what is in this repo" is decided.
 */

import { scanEngine, scanApps, scanCppTests, linkUsage } from './scan-cpp.mjs';
import { scanWeb, duplicateHelpers } from './scan-web.mjs';
import { scanPythonTools, scanGates, scanBuildTools, scanDocs } from './scan-repo.mjs';
import { checkAll } from './checks.mjs';
import { computeOpportunities, validatePairings } from './synergy.mjs';
import { buildQueue } from './queue.mjs';

export async function buildModel(root) {
  const rawModules = scanEngine(root);
  const rawApps = scanApps(root);
  const cppTests = scanCppTests(root);
  linkUsage(root, rawModules, rawApps, cppTests);

  const { webApps: rawWeb, exchange } = await scanWeb(root);
  const rawPy = scanPythonTools(root);
  const rawGates = scanGates(root);
  const rawTools = scanBuildTools(root);
  const rawDocs = scanDocs(root);

  const modules = checkAll(rawModules);
  const apps = checkAll(rawApps);
  const webApps = checkAll(rawWeb);
  const pyTools = checkAll(rawPy);
  const gates = checkAll(rawGates);
  const buildTools = checkAll(rawTools);
  const docs = checkAll(rawDocs);

  const all = [...apps, ...modules, ...webApps, ...pyTools, ...gates, ...buildTools, ...docs];

  const model = {
    root,
    apps,
    modules,
    cppTests,
    webApps,
    pyTools,
    gates,
    buildTools,
    docs,
    exchange,
    all,
    duplicateHelpers: duplicateHelpers(rawWeb)
  };

  model.opportunities = computeOpportunities(model);
  const { pairings, stale } = validatePairings(model);
  model.pairings = pairings;
  model.stalePairings = stale;
  model.queue = buildQueue(model);
  model.stats = summarise(model);
  return model;
}

function summarise(m) {
  const pct = (n, d) => (d ? Math.round((n / d) * 100) : 100);
  return {
    apps: m.apps.length,
    engineModules: m.modules.length,
    engineSubsystems: new Set(m.modules.map((x) => x.subsystem)).size,
    cppTests: m.cppTests.length,
    webApps: m.webApps.length,
    pyTools: m.pyTools.length,
    gates: m.gates.length,
    buildTools: m.buildTools.length,
    docs: m.docs.length,
    totalArtifacts: m.all.length,
    linesOfApps: m.apps.reduce((n, a) => n + a.loc, 0),
    linesOfEngine: m.modules.reduce((n, a) => n + a.loc, 0),
    linesOfWeb: m.webApps.reduce((n, a) => n + a.loc, 0),
    appsHeadless: pct(m.apps.filter((a) => a.headless).length, m.apps.length),
    appsGolden: pct(m.apps.filter((a) => a.golden).length, m.apps.length),
    modulesTested: pct(m.modules.filter((x) => x.testedBy.length).length, m.modules.length),
    modulesDemoed: pct(m.modules.filter((x) => x.demoedBy.length).length, m.modules.length),
    checksPassed: m.all.reduce((n, a) => n + a.passed, 0),
    checksTotal: m.all.reduce((n, a) => n + a.total, 0),
    openTasks: m.queue.length
  };
}
