/*
 * Scanner for the native half of the repo: the engine headers, the sample
 * apps under apps/, and the C++ test suite under tests/.
 *
 * The one non-obvious thing here is how an app is linked to the engine
 * features it demonstrates. Apps include `maz/Engine.hpp` and nothing else —
 * that umbrella header pulls in 150+ modules — so `#include` says nothing
 * about what an app actually uses. What does say it is the *type names*: a
 * header at engine/include/maz/game/AStar2D.hpp declares `game::AStar2D`, and
 * an app that demonstrates it has to write that identifier somewhere. So we
 * tokenise every app and every test once into an identifier index, then ask
 * that index which files mention each module's name. It is a heuristic, but a
 * well-behaved one: a false positive needs a same-named identifier, and a
 * false negative needs a module whose own type is never named.
 */

import { join } from 'node:path';
import { readText, subdirs, filesIn, walkFiles, exists, countLines } from './fs.mjs';

const IDENT_RE = /[A-Za-z_][A-Za-z0-9_]*/g;

/**
 * Build `identifier -> [files that mention it]` over a set of source files.
 * One pass over the corpus instead of one grep per module name.
 */
export function indexIdentifiers(root, relPaths) {
  const index = new Map();
  for (const rel of relPaths) {
    const text = readText(join(root, rel));
    if (!text) continue;
    const seen = new Set(text.match(IDENT_RE) || []);
    for (const id of seen) {
      let bucket = index.get(id);
      if (!bucket) index.set(id, (bucket = []));
      bucket.push(rel);
    }
  }
  return index;
}

/**
 * Pull the title and blurb out of an app's leading comment block.
 *
 * Every app opens with the same shape, e.g.
 *   // Maz Engine — "PONG" — a complete, classic two-paddle game ...
 *   // ... continued across a few more lines.
 * The quoted word is the app's display name; everything after it is the blurb.
 */
export function parseHeaderComment(source) {
  const lines = [];
  for (const line of source.split('\n')) {
    const t = line.trim();
    if (t.startsWith('//')) {
      lines.push(t.replace(/^\/\/\s?/, ''));
      continue;
    }
    if (t === '' && lines.length === 0) continue;  // tolerate a leading blank line
    break;
  }
  const text = lines.join(' ').replace(/\s+/g, ' ').trim();
  const quoted = text.match(/"([^"]+)"/);
  const title = quoted ? quoted[1] : '';
  let blurb = text;
  if (quoted) blurb = text.slice(quoted.index + quoted[0].length);
  blurb = blurb.replace(/^\s*[—\-–:(]\s*/, '').trim();
  return { title, blurb, raw: text };
}

/**
 * Pull an engine header's doc comment out of the file.
 *
 * Unlike an app, a header does not open with its comment: `#pragma once` and a
 * block of `#include`s come first, and on many headers the doc comment sits
 * below `namespace maz::audio {`. So rather than reading the top of the file,
 * find the first run of consecutive `//` lines anywhere in it. tools/gen_api_docs.py
 * reads the same comment to build docs/API.md, so a header this returns nothing
 * for is a blank entry in the published API reference.
 */
export function extractDocComment(source, maxLines = 200) {
  const lines = source.split('\n', maxLines);
  const run = [];
  for (const line of lines) {
    const t = line.trim();
    if (t.startsWith('//')) {
      run.push(t.replace(/^\/\/+\s?/, ''));
      continue;
    }
    if (run.length) break;   // the run ended: that first block is the doc comment
  }
  return run.join(' ').replace(/\s+/g, ' ').trim();
}

/** Every engine module: one header under engine/include/maz/. */
export function scanEngine(root) {
  const base = join(root, 'engine', 'include', 'maz');
  const rels = walkFiles(base, /\.hpp$/);
  return rels.map((rel) => {
    const parts = rel.split('/');
    const file = parts[parts.length - 1];
    const name = file.replace(/\.hpp$/, '');
    const subsystem = parts.length > 1 ? parts[0] : '(root)';
    const text = readText(join(base, rel));
    const blurb = extractDocComment(text);
    return {
      kind: 'engine-module',
      id: `engine:${subsystem}/${name}`,
      name,
      subsystem,
      path: `engine/include/maz/${rel}`,
      blurb,
      loc: countLines(text)
    };
  });
}

/** Every sample app / game: one directory under apps/. */
export function scanApps(root) {
  const cmake = readText(join(root, 'CMakeLists.txt'));
  return subdirs(join(root, 'apps')).map((name) => {
    const dir = join(root, 'apps', name);
    const mainRel = `apps/${name}/main.cpp`;
    const source = readText(join(dir, 'main.cpp'));
    const { title, blurb } = parseHeaderComment(source);
    return {
      kind: 'app',
      id: `app:${name}`,
      name,
      path: `apps/${name}`,
      title: title || name.toUpperCase(),
      blurb,
      loc: countLines(source),
      mainRel,
      hasMain: source !== '',
      hasCMake: exists(join(dir, 'CMakeLists.txt')),
      registered: cmake.includes(`add_subdirectory(apps/${name})`),
      headless: source.includes('--headless'),
      demoFlag: source.includes('--demo'),
      // The template is the mobile-ready starting point; an app that never
      // mentions touch or a safe area cannot be played on a phone.
      touch: /Touch|touch|FingerDown|safeArea|SafeArea/.test(source),
      golden: exists(join(root, 'tests', 'golden', `${name}.png`))
    };
  });
}

/** Every C++ test translation unit under tests/. */
export function scanCppTests(root) {
  const rels = walkFiles(join(root, 'tests'), /\.(cpp|hpp)$/);
  return rels.map((rel) => ({
    kind: 'cpp-test',
    id: `test:${rel}`,
    name: rel,
    path: `tests/${rel}`,
    loc: countLines(readText(join(root, 'tests', rel)))
  }));
}

/**
 * Cross-link modules to the apps and tests that name them, and apps to the
 * modules they exercise. Mutates and returns the inputs.
 */
export function linkUsage(root, modules, apps, cppTests) {
  const appIndex = indexIdentifiers(root, apps.filter((a) => a.hasMain).map((a) => a.mainRel));
  const testIndex = indexIdentifiers(root, cppTests.map((t) => t.path));

  const byApp = new Map(apps.map((a) => [a.mainRel, a]));
  for (const a of apps) a.usesModules = [];

  for (const m of modules) {
    m.demoedBy = (appIndex.get(m.name) || []).slice().sort();
    m.testedBy = (testIndex.get(m.name) || []).slice().sort();
    // A module demonstrated by an app that has a committed golden frame is
    // covered by the `golden_images` ctest, which renders that app on software
    // Vulkan and compares the result. That is real coverage — it is what
    // catches a broken render pass — and it is the only kind available to the
    // Vulkan renderer, the font atlas and the debug overlay, none of which can
    // be exercised by a headless unit test. Weaker than a unit test at edge
    // cases, so it is tracked separately rather than folded into testedBy.
    m.goldenBy = m.demoedBy
      .map((rel) => byApp.get(rel))
      .filter((a) => a && a.golden)
      .map((a) => a.id)
      .sort();
    for (const rel of m.demoedBy) {
      const app = byApp.get(rel);
      if (app) app.usesModules.push(m.id);
    }
  }
  for (const a of apps) a.usesModules.sort();
  return { modules, apps, cppTests };
}
