/*
 * Scanner for the browser half of the repo — the MAZ ARCADE projects — plus
 * the cross-project wiring that holds them together.
 *
 * shared/projects.js is already the repo's single list of projects (the hub
 * and the in-app nav both read it), so this scanner treats that file as the
 * source of truth for *what exists* and then measures each project against
 * the filesystem: does it have tests, a README, a nav bar, does it publish a
 * capability other projects can consume, does it consume one.
 */

import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { readText, filesIn, walkFiles, exists, isDir, countLines } from './fs.mjs';

/** Load shared/projects.js (a CommonJS UMD file) without a bundler. */
export async function loadProjects(root) {
  try {
    const mod = await import(pathToFileURL(join(root, 'shared', 'projects.js')).href);
    return (mod.default && mod.default.PROJECTS) || mod.PROJECTS || [];
  } catch {
    return [];
  }
}

export function loadExchange(root) {
  try {
    return JSON.parse(readText(join(root, 'shared', 'exchange.json')));
  } catch {
    return { publishes: {}, consumes: [] };
  }
}

// Top-level `function name(...)` and `const name = (…) =>` declarations. Good
// enough to spot the same helper written out in five projects, which is the
// only thing it is used for.
const FN_RE = /^\s*(?:function\s+([A-Za-z_$][\w$]*)|(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:function\b|\([^)]*\)\s*=>|[A-Za-z_$][\w$]*\s*=>))/gm;

export function declaredFunctions(source) {
  const out = new Set();
  let m;
  FN_RE.lastIndex = 0;
  while ((m = FN_RE.exec(source)) !== null) out.add(m[1] || m[2]);
  return out;
}

/**
 * Scan the browser projects listed in shared/projects.js that actually live in
 * their own directory (the engine and the Python tools are listed there too,
 * pointing at a doc, and are catalogued by their own scanners).
 */
export async function scanWeb(root) {
  const projects = await loadProjects(root);
  const exchange = loadExchange(root);
  const hub = readText(join(root, 'index.html'));

  const publishedBy = new Map();
  for (const [id, spec] of Object.entries(exchange.publishes || {})) {
    if (!publishedBy.has(spec.project)) publishedBy.set(spec.project, []);
    publishedBy.get(spec.project).push(id);
  }
  const consumedBy = new Map();
  for (const c of exchange.consumes || []) {
    if (!consumedBy.has(c.project)) consumedBy.set(c.project, []);
    consumedBy.get(c.project).push(c.id);
  }

  const out = [];
  for (const p of projects) {
    const dir = p.path.replace(/\/$/, '');
    if (!isDir(join(root, dir))) continue;   // engine / scraper / crew entries point at a doc
    const jsFiles = walkFiles(join(root, dir, 'js'), /\.js$/).map((f) => `${dir}/js/${f}`);
    const indexHtml = readText(join(root, dir, 'index.html'));
    const testFiles = walkFiles(join(root, dir, 'tests'), /\.(test|spec)\.(m?js|cjs)$/)
      .map((f) => `${dir}/tests/${f}`);
    const sources = [...jsFiles, ...(indexHtml ? [`${dir}/index.html`] : [])];
    const loc = sources.reduce((n, rel) => n + countLines(readText(join(root, rel))), 0);

    const fns = new Set();
    for (const rel of jsFiles) for (const f of declaredFunctions(readText(join(root, rel)))) fns.add(f);
    // A single-file app keeps its logic inline in index.html.
    if (jsFiles.length === 0) for (const f of declaredFunctions(indexHtml)) fns.add(f);

    out.push({
      kind: 'web-app',
      id: `web:${p.id}`,
      name: p.name,
      slug: p.id,
      path: dir,
      blurb: (p.blurb || '').trim(),
      tag: p.tag || '',
      badges: p.badges || [],
      loc,
      jsFiles,
      testFiles,
      hasTests: testFiles.length > 0,
      hasReadme: exists(join(root, dir, 'README.md')),
      inHub: hub.includes(`${dir}/`) || hub.includes('MAZ_PROJECTS'),
      hasNav: [...sources].some((rel) => readText(join(root, rel)).includes('maz-nav.js')),
      publishes: publishedBy.get(p.id) || [],
      consumes: consumedBy.get(p.id) || [],
      functions: [...fns].sort()
    });
  }
  return { webApps: out, exchange, projects };
}

/**
 * Helper names declared by more than one browser project. Each one is a
 * function written out N times that could be written once in shared/ — the
 * cheapest real cross-project win the repo has.
 */
export function duplicateHelpers(webApps, minProjects = 2) {
  const byName = new Map();
  for (const app of webApps) {
    for (const fn of app.functions) {
      if (!byName.has(fn)) byName.set(fn, []);
      byName.get(fn).push(app.slug);
    }
  }
  return [...byName.entries()]
    .filter(([, projects]) => projects.length >= minProjects)
    .map(([name, projects]) => ({ name, projects: projects.slice().sort() }))
    .sort((a, b) => b.projects.length - a.projects.length || (a.name < b.name ? -1 : 1));
}
