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
const QUALIFIED_RE = /\b([A-Za-z_]\w*)::([A-Za-z_]\w*)/g;

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
    // Qualified uses too — `audio::applyWindow` is unambiguous where a bare
    // `Window` is not, and apps write the qualified form because they say
    // `using namespace maz;` and nothing narrower.
    for (const m of text.matchAll(QUALIFIED_RE)) seen.add(`${m[1]}::${m[2]}`);
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

/*
 * A header's public surface: the types and free functions it declares.
 *
 * Matching a module by its FILE name misses more than it finds. Plenty of
 * headers declare no type of their own name at all — anim/AdditiveBlend.hpp
 * declares makeAdditiveDelta, applyAdditiveDelta and additiveBlendJoint;
 * math/VectorOps.hpp declares isFinite and isEqualApprox; render/MtlLoader.hpp
 * declares MtlMaterial — so no app could ever be seen to use them, whether it
 * does or not. That alone inflated "no app demonstrates this" to 552 modules.
 *
 * The two patterns are the ones tools/gen_api_docs.py already uses to build
 * docs/API.md from the same headers, so the inventory and the published API
 * reference agree on what a module's surface is.
 */
// The trailing (?![;]) drops forward declarations: `class Window;` in render/
// Renderer.hpp names platform's Window, it does not declare one, and counting
// it made every app that opens a window look like a demo of the audio module
// that happens to share the name.
const TYPE_RE = /^(?:template\s*<[^>]*>\s*)?(?:class|struct|enum class)\s+([A-Za-z_]\w*)\s*(?![;\s]*;)/gm;
const FUNC_RE = /^inline\s+.*?\b([A-Za-z_]\w*)\s*\(/gm;
// A free function DECLARED in the header and implemented in a .cpp — the shape
// every SDL-backed shim takes: `AppConfig parseArgs(int, char**);`,
// `std::string clipboardText();`, `std::string prefPath(...)`. Anchored at
// column zero, which is what separates a free function inside a namespace from
// a member declaration inside an indented class body.
const DECL_RE = /^([A-Za-z_][\w:<>,&*\s]*?)\s+([A-Za-z_]\w*)\s*\([^)]*\)\s*(?:const\s*)?;/gm;

// Names too common to be evidence of anything: an app writing `size` or `data`
// is not thereby demonstrating a module that happens to declare one.
const AMBIGUOUS = new Set([
  'begin', 'end', 'size', 'data', 'clear', 'reset', 'get', 'set', 'at', 'count',
  'empty', 'value', 'add', 'remove', 'find', 'next', 'prev', 'init', 'update',
  'draw', 'run', 'step', 'apply', 'build', 'make', 'load', 'save', 'read', 'write',
  'push', 'pop', 'front', 'back', 'first', 'last', 'min', 'max', 'abs', 'length'
]);

export function publicSymbols(source, headerName) {
  const out = new Set([headerName]);
  for (const { re, group } of [{ re: TYPE_RE, group: 1 }, { re: FUNC_RE, group: 1 },
                               { re: DECL_RE, group: 2 }]) {
    re.lastIndex = 0;
    let m;
    while ((m = re.exec(source)) !== null) {
      const name = m[group];
      // A three-letter free function is as likely to be a coincidence as a use.
      if (name.length >= 4 && !AMBIGUOUS.has(name)) out.add(name);
    }
  }
  return [...out];
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
      symbols: publicSymbols(text, name),
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
      // An app that never opens a platform::Window is a command-line tool
      // (mobilepack, the mobile exporter, is the one here). It runs in CI
      // already, has no frame to capture, and asking it for a headless mode or
      // a golden screenshot is asking the wrong question.
      windowed: /platform::Window\b/.test(source),
      // Whether it can run with no display.
      //
      // This used to test for the literal string "--headless" in the source,
      // and reported 38 apps as unable to run in CI. Every one of them could:
      // they call core::parseArgs, which owns the flag, and pass cfg.headless
      // into the window and renderer configs. All the check actually measured
      // was whether an app's comment happened to mention the flag by name.
      // What makes an app headless is the wiring, so that is what is read.
      headless: !/platform::Window\b/.test(source) ||
        (/\bparseArgs\b/.test(source) && /headless/i.test(source)),
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

  /*
   * Which modules declare each symbol. A name only one module declares is
   * evidence on its own; a name several share (`Window` is both platform's
   * window and audio's windowing function; `Config`, `Time` and `Input` all
   * repeat) is only evidence when written with its namespace. Without this,
   * every app that opens a window counted as a demo of the audio module.
   */
  const owners = new Map();
  for (const m of modules) {
    for (const sym of m.symbols) {
      if (!owners.has(sym)) owners.set(sym, new Set());
      owners.get(sym).add(m.id);
    }
  }

  const lookup = (index, mod) => {
    const hits = new Set();
    for (const sym of mod.symbols) {
      // Always accept the qualified form.
      for (const rel of index.get(`${mod.subsystem}::${sym}`) || []) hits.add(rel);
      // Accept the bare form only when no other module claims the same name.
      if ((owners.get(sym) || new Set()).size === 1) {
        for (const rel of index.get(sym) || []) hits.add(rel);
      }
    }
    return [...hits].sort();
  };

  for (const m of modules) {
    // Any of the module's public names appearing in a file counts as using it.
    m.demoedBy = lookup(appIndex, m);
    m.testedBy = lookup(testIndex, m);
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
