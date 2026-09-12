/*
 * Scanner for everything that is neither the engine nor a browser app: the
 * Python tools, the dependency-free Node gates under scripts/, the build and
 * packaging helpers under tools/, the CI workflows, and the docs.
 *
 * These are artifacts too — the Forge is a program Cody built just as much as
 * PONG is — and they are the ones most likely to rot unnoticed, because
 * nothing renders them on a screen.
 */

import { join } from 'node:path';
import { readText, subdirs, filesIn, walkFiles, exists, countLines } from './fs.mjs';

/** Python tools: any top-level directory carrying a pyproject.toml. */
export function scanPythonTools(root) {
  const workflows = filesIn(join(root, '.github', 'workflows'), /\.ya?ml$/)
    .map((f) => ({ file: f, text: readText(join(root, '.github', 'workflows', f)) }));

  const out = [];
  for (const name of subdirs(root)) {
    const dir = join(root, name);
    const pyproject = readText(join(dir, 'pyproject.toml'));
    if (!pyproject) continue;

    const modules = walkFiles(join(dir, name), /\.py$/);
    const tests = walkFiles(join(dir, 'tests'), /^test_.*\.py$/);
    const loc = modules.reduce((n, rel) => n + countLines(readText(join(dir, name, rel))), 0);
    const summary = (pyproject.match(/^description\s*=\s*"([^"]*)"/m) || [])[1] || '';
    const scripts = [...pyproject.matchAll(/^([A-Za-z0-9_-]+)\s*=\s*"[\w.]+:[\w.]+"/gm)].map((m) => m[1]);

    out.push({
      kind: 'py-tool',
      id: `py:${name}`,
      name,
      path: name,
      blurb: summary,
      loc,
      modules: modules.map((m) => `${name}/${name}/${m}`),
      tests: tests.map((t) => `${name}/tests/${t}`),
      hasTests: tests.length > 0,
      hasReadme: exists(join(dir, 'README.md')),
      entryPoints: scripts,
      ciWorkflows: workflows.filter((w) => w.text.includes(`${name}/`)).map((w) => w.file)
    });
  }
  return out;
}

/** The dependency-free Node gates under scripts/ — the repo's own CI checks. */
export function scanGates(root) {
  const workflows = filesIn(join(root, '.github', 'workflows'), /\.ya?ml$/)
    .map((f) => ({ file: f, text: readText(join(root, '.github', 'workflows', f)) }));
  const testFiles = filesIn(join(root, 'scripts', 'tests'), /\.(m?js|cjs)$/);

  return filesIn(join(root, 'scripts'), /\.(mjs|cjs|js)$/).map((f) => {
    const name = f.replace(/\.(mjs|cjs|js)$/, '');
    const text = readText(join(root, 'scripts', f));
    const { blurb } = firstBlockComment(text);
    return {
      kind: 'gate',
      id: `gate:${name}`,
      name: f,
      path: `scripts/${f}`,
      blurb,
      loc: countLines(text),
      // A rule-encoding checker (check-*.mjs) decides whether the repo is
      // correct by applying logic that can quietly be wrong — a regex that
      // stops matching, a path resolver that says yes to everything. It needs
      // tests. A driver like smoke-site.cjs asserts on what a real browser
      // does, so its failures are loud and its "tests" would mean mocking a
      // browser to check a browser. The distinction is what the check reads.
      rulesBased: /^check-/.test(name),
      hasTests: testFiles.some((t) => t.startsWith(name + '.')),
      testFiles: testFiles.filter((t) => t.startsWith(name + '.')).map((t) => `scripts/tests/${t}`),
      ciWorkflows: workflows.filter((w) => w.text.includes(`scripts/${f}`)).map((w) => w.file)
    };
  });
}

/** Build, packaging and codegen helpers under tools/. */
export function scanBuildTools(root) {
  return filesIn(join(root, 'tools'), /\.(sh|bat|py|cpp)$/).map((f) => {
    const text = readText(join(root, 'tools', f));
    const { blurb } = firstBlockComment(text);
    return {
      kind: 'build-tool',
      id: `tool:${f}`,
      name: f,
      path: `tools/${f}`,
      blurb,
      loc: countLines(text)
    };
  });
}

/** Docs, and whether anything in the repo actually links to each one. */
export function scanDocs(root) {
  // INVENTORY.md is this tool's own output. Cataloguing it would make the
  // document's own numbers change the moment it first exists, so the very first
  // run and the second would disagree and `--check` would fail for no reason.
  const docs = filesIn(join(root, 'docs'), /\.md$/).filter((f) => f !== 'INVENTORY.md');
  // Every place a link could live: the root markdown files, the hub, and the
  // other docs. A doc nothing links to is a doc nobody will ever find.
  const haystack = [
    ...filesIn(root, /\.md$/).map((f) => readText(join(root, f))),
    readText(join(root, 'index.html')),
    readText(join(root, 'CLAUDE.md')),
    ...docs.map((f) => readText(join(root, 'docs', f))),
    ...subdirs(root).map((d) => readText(join(root, d, 'README.md')))
  ].join('\n');

  return docs.map((f) => {
    const text = readText(join(root, 'docs', f));
    const title = (text.match(/^#\s+(.+)$/m) || [])[1] || f;
    const others = haystack.replace(text, '');   // a doc linking to itself does not count
    return {
      kind: 'doc',
      id: `doc:${f}`,
      name: f,
      path: `docs/${f}`,
      blurb: title.trim(),
      loc: countLines(text),
      generated: /auto-generated/i.test(text.slice(0, 600)),
      linkedFrom: others.includes(`docs/${f}`) || others.includes(`(${f})`)
    };
  });
}

/**
 * A script's own first line of description, whatever it writes comments in.
 *
 * Every comment style in `tools/` and `scripts/` has to be here or the checker
 * lies: it reported tools/build_editor.bat as undocumented because a Windows
 * batch file says REM, and the file opens with a better description than most
 * of the ones that passed.
 */
export function firstBlockComment(text) {
  const block = text.match(/\/\*([\s\S]*?)\*\//);
  if (block) {
    const body = block[1].split('\n').map((l) => l.replace(/^\s*\*?\s?/, '').trim()).filter(Boolean);
    return { blurb: (body[0] || '').replace(/\s+/g, ' ') };
  }
  // Python says it with a module docstring, not a comment.
  const docstring = text.match(/^\s*(?:#![^\n]*\n)?\s*("""|''')([\s\S]*?)\1/);
  if (docstring) {
    const first = docstring[2].trim().split('\n')[0].trim();
    if (first) return { blurb: first.replace(/\s+/g, ' ') };
  }

  const lines = text.split('\n');
  for (const raw of lines) {
    const line = raw.trim();
    if (line.startsWith('#!') || line === '' || /^@echo\b/i.test(line)) continue;   // shebang, blank, batch header
    if (line.startsWith('#')) return { blurb: line.replace(/^#\s?/, '').trim() };
    if (line.startsWith('//')) return { blurb: line.replace(/^\/\/\s?/, '').trim() };
    if (/^REM\b/i.test(line)) return { blurb: line.replace(/^REM\s?/i, '').trim() };
    break;   // real code before any comment: this script documents nothing
  }
  return { blurb: '' };
}
