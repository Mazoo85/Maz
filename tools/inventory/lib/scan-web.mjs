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

// Top-level `function name(...)` declarations, and the `const name = ...`
// forms. Good enough to find the same helper written out in five projects,
// which is the only thing it is used for.
const FN_RE = /^\s*(?:function\s+([A-Za-z_$][\w$]*)|(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:function\b|\([^)]*\)\s*=>|[A-Za-z_$][\w$]*\s*=>))/gm;

export function declaredFunctions(source) {
  const out = new Set();
  let m;
  FN_RE.lastIndex = 0;
  while ((m = FN_RE.exec(source)) !== null) out.add(m[1] || m[2]);
  return out;
}

/** Strip comments and collapse whitespace, so formatting differences do not hide a copy. */
function normalise(code) {
  return code
    .replace(/\/\*[\s\S]*?\*\//g, ' ')
    .replace(/(^|[^:])\/\/[^\n]*/g, '$1 ')
    .replace(/\s+/g, ' ')
    .trim();
}

const FN_DECL_RE = /\bfunction\s+([A-Za-z_$][\w$]*)\s*\(([^)]*)\)\s*\{/g;

/**
 * A function's identity with its parameters renamed to their positions.
 *
 * Only plain named parameters are renamed; a default value or a destructured
 * parameter is left alone, which at worst means two such functions compare as
 * different — a missed suggestion, never a wrong one.
 */
function alphaRename(name, params, body) {
  const names = params
    .split(',')
    .map((x) => x.trim())
    .filter((x) => /^[A-Za-z_$][\w$]*$/.test(x));
  let out = body;
  names.forEach((param, i) => {
    out = out.replace(new RegExp(`\\b${param}\\b`, 'g'), `$$${i}`);
  });
  return `${name}(${names.map((_, i) => `$${i}`).join(',')}){${out}}`;
}

/**
 * Every `function name(args) { ... }` in a source file, with its body.
 *
 * Bodies matter because a shared *name* is not a shared *function*. `noise` is
 * a text-variation helper in SCRIPT FORGE and a burst of audio static in
 * ZOMBOID; `pick` takes a list in one project and a dictionary category in
 * another. Reporting those as duplicated code would send someone to merge two
 * functions that have nothing in common, which is worse than reporting nothing.
 *
 * Bodies are found by counting braces from the opening one. String and regex
 * literals containing an unbalanced brace would throw that count off; in this
 * repo's code they do not, and the cost of being wrong is one helper wrongly
 * omitted from a refactoring suggestion, so a full JS parser is not worth it.
 */
export function extractFunctions(source) {
  const out = [];
  let m;
  FN_DECL_RE.lastIndex = 0;
  while ((m = FN_DECL_RE.exec(source)) !== null) {
    const open = FN_DECL_RE.lastIndex - 1;
    let depth = 0;
    let end = -1;
    for (let i = open; i < source.length; i += 1) {
      const ch = source[i];
      if (ch === '{') depth += 1;
      else if (ch === '}') {
        depth -= 1;
        if (depth === 0) { end = i; break; }
      }
    }
    if (end < 0) continue;
    const params = normalise(m[2]);
    out.push({
      name: m[1],
      params,
      body: normalise(source.slice(open + 1, end)),
      // Two projects writing the same helper rarely agree on parameter names —
      // `clamp(v, lo, hi)` and `clamp(v, a, b)` are the same function. The
      // signature renames each parameter to its position, so identity survives
      // that while still separating genuinely different code.
      signature: alphaRename(m[1], params, normalise(source.slice(open + 1, end)))
    });
    FN_DECL_RE.lastIndex = end;
  }
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
    const bodies = [];
    // A single-file app keeps its logic inline in index.html, so that page is
    // read for functions when the project has no js/ directory of its own.
    const sourceFiles = jsFiles.length ? jsFiles : (indexHtml ? [`${dir}/index.html`] : []);
    for (const rel of sourceFiles) {
      const text = readText(join(root, rel));
      for (const f of declaredFunctions(text)) fns.add(f);
      for (const f of extractFunctions(text)) bodies.push({ ...f, file: rel });
    }

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
      functions: [...fns].sort(),
      definitions: bodies
    });
  }
  return { webApps: out, exchange, projects };
}

/**
 * Helpers that two or more browser projects have written out identically.
 *
 * Identity is the function's normalised parameter list and body, not its name.
 * That is the whole difference between a finding worth acting on and a
 * misleading one: six projects declare a `noise`, and no two of them mean the
 * same thing by it. Only a real copy — the same code, in more than one place —
 * can be replaced by one shared module without changing behaviour.
 */
export function duplicateHelpers(webApps, minProjects = 2) {
  const byImplementation = new Map();
  for (const app of webApps) {
    for (const fn of app.definitions || []) {
      if (!fn.body) continue;   // an empty stub is not a helper worth sharing
      const key = fn.signature;
      if (!byImplementation.has(key)) {
        byImplementation.set(key, { name: fn.name, params: fn.params, projects: new Set(), files: [] });
      }
      const hit = byImplementation.get(key);
      hit.projects.add(app.slug);
      hit.files.push(fn.file);
    }
  }
  return [...byImplementation.values()]
    .filter((h) => h.projects.size >= minProjects)
    .map((h) => ({
      name: h.name,
      params: h.params,
      projects: [...h.projects].sort(),
      files: h.files.slice().sort()
    }))
    .sort((a, b) => b.projects.length - a.projects.length || (a.name < b.name ? -1 : 1));
}
