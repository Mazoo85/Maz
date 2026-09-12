/*
 * Completeness checks — what "finished" means for each kind of artifact.
 *
 * A check is deliberately narrow and mechanical: something the scanner can see
 * on disk, phrased so that failing it names its own fix. "Is PONG a good game"
 * is not a check. "Does PONG run headless so CI can capture it" is.
 *
 * Each check returns { id, label, ok, fix }. `fix` is the sentence that ends
 * up in the work queue, so it is written as an instruction, not a complaint.
 */

const CHECKS = {
  app: [
    {
      id: 'registered',
      missing: 'not built by CMake',
      label: 'built by CMake',
      test: (a) => a.registered,
      fix: (a) => `Register apps/${a.name} in the root CMakeLists.txt (add_subdirectory(apps/${a.name})) — it is not built today, so nothing compiles or tests it.`
    },
    {
      id: 'cmake',
      missing: 'no CMakeLists.txt',
      label: 'has CMakeLists.txt',
      test: (a) => a.hasCMake,
      fix: (a) => `Add apps/${a.name}/CMakeLists.txt (add_executable + target_link_libraries maz::maz maz_warnings).`
    },
    {
      id: 'documented',
      missing: 'no header comment',
      label: 'header comment says what it shows',
      test: (a) => Boolean(a.title) && a.blurb.length >= 40,
      fix: (a) => `Open apps/${a.name}/main.cpp with the standard header comment: a quoted title and a sentence or two on what the demo shows.`
    },
    {
      id: 'headless',
      missing: 'no headless mode',
      label: 'runs headless for CI',
      test: (a) => a.headless,
      fix: (a) => `Teach apps/${a.name} the --headless / --frames N flags so CI can run it without a display.`
    },
    {
      id: 'golden',
      missing: 'no golden screenshot',
      label: 'has a golden screenshot',
      test: (a) => a.golden,
      fix: (a) => `Capture a golden frame for ${a.name} into tests/golden/${a.name}.png so a rendering regression is caught automatically.`
    },
    {
      id: 'uses-engine',
      missing: 'names no engine module',
      label: 'exercises a named engine module',
      test: (a) => (a.usesModules || []).length > 0,
      fix: (a) => `apps/${a.name} names no engine module type — either it is reimplementing something engine/include/maz/ already has, or it needs a comment saying which module it demonstrates.`
    }
  ],

  'engine-module': [
    {
      id: 'documented',
      missing: 'no header comment',
      label: 'header has a doc comment',
      test: (m) => m.blurb.length >= 30,
      fix: (m) => `Give ${m.path} a leading doc comment — docs/API.md is generated from it, so an undocumented header is a blank entry in the public API reference.`
    },
    {
      id: 'tested',
      missing: 'no test, no golden',
      label: 'covered by a test or a golden image',
      // Either counts. A unit test is stronger and is what most modules should
      // have; a golden-image comparison through an app that demonstrates the
      // module is the only coverage available to the Vulkan renderer, the font
      // atlas and the debug overlay, and it is what catches a broken render
      // pass. See `goldenBy` in scan-cpp.mjs.
      test: (m) => m.testedBy.length > 0 || m.goldenBy.length > 0,
      fix: (m) => `Nothing exercises maz::${m.subsystem}::${m.name}: no test names it, and no app that demonstrates it has a golden frame. Add a unit test under tests/, or a golden for an app that shows it.`
    },
    {
      id: 'demoed',
      missing: 'no demo app',
      label: 'shown by a sample app',
      test: (m) => m.demoedBy.length > 0,
      fix: (m) => `No app under apps/ demonstrates maz::${m.subsystem}::${m.name}. Either fold it into an existing demo or give it one, so the feature is discoverable and visually verified.`
    }
  ],

  'web-app': [
    {
      id: 'readme',
      missing: 'no README',
      label: 'has a README',
      test: (w) => w.hasReadme,
      fix: (w) => `Write ${w.path}/README.md — what it does, how to run it, what it publishes for other projects.`
    },
    {
      id: 'tests',
      missing: 'no tests',
      label: 'has a logic test',
      test: (w) => w.hasTests,
      fix: (w) => `Add ${w.path}/tests/ with a dependency-free Node test over its pure logic, in the style of film/tests/film-logic.test.js, and run it in CI.`
    },
    {
      id: 'nav',
      missing: 'no hub nav',
      label: 'links back to the hub',
      test: (w) => w.hasNav,
      fix: (w) => `Load shared/maz-nav.js in ${w.path}/index.html so the project links back to MAZ ARCADE and across to its siblings.`
    },
    {
      id: 'exchange',
      missing: 'not in exchange.json',
      label: 'declared in shared/exchange.json',
      // Waived for a deliberately self-contained project — see `selfContained`
      // in scan-web.mjs. That is a design decision declared on the project's
      // own card, not an omission.
      test: (w) => w.publishes.length > 0 || w.consumes.length > 0 || w.selfContained,
      fix: (w) => `${w.name} neither publishes a capability nor consumes one. Declare in shared/exchange.json what it can lend the other projects — that manifest is how they find each other.`
    }
  ],

  'py-tool': [
    {
      id: 'readme',
      missing: 'no README',
      label: 'has a README',
      test: (t) => t.hasReadme,
      fix: (t) => `Write ${t.path}/README.md — install, use, develop.`
    },
    {
      id: 'tests',
      missing: 'no tests',
      label: 'has tests',
      test: (t) => t.hasTests,
      fix: (t) => `Add ${t.path}/tests/ with pytest cases for its pure logic.`
    },
    {
      id: 'ci',
      missing: 'not run by CI',
      label: 'runs in CI',
      test: (t) => t.ciWorkflows.length > 0,
      fix: (t) => `Add a .github/workflows job that runs ${t.path}'s test suite, so a break is caught on push.`
    }
  ],

  gate: [
    {
      id: 'tests',
      missing: 'no tests',
      label: 'rule-encoding checkers are tested',
      // Only asked of a check-*.mjs. See `rulesBased` in scan-repo.mjs for why
      // a browser driver is exempt rather than merely excused.
      test: (g) => !g.rulesBased || g.hasTests,
      fix: (g) => `Add scripts/tests/${g.name.replace(/\.(mjs|cjs|js)$/, '')}.test.mjs — a checker with no tests of its own can pass for the wrong reason.`
    },
    {
      id: 'ci',
      missing: 'not run by CI',
      label: 'wired into a workflow',
      test: (g) => g.ciWorkflows.length > 0,
      fix: (g) => `Run ${g.path} from a .github/workflows job — an unrun gate protects nothing.`
    }
  ],

  doc: [
    {
      id: 'linked',
      missing: 'nothing links to it',
      label: 'reachable from somewhere',
      test: (d) => d.linkedFrom,
      fix: (d) => `Nothing links to ${d.path}. Link it from README.md, CLAUDE.md or a sibling doc, or delete it.`
    }
  ],

  'build-tool': [
    {
      id: 'documented',
      missing: 'no header comment',
      label: 'says what it does',
      test: (t) => t.blurb.length >= 20,
      fix: (t) => `Open ${t.path} with a one-line comment saying what it builds or generates and how it is invoked.`
    }
  ]
};

/** Run every check for an artifact's kind. Returns the artifact, annotated. */
export function checkArtifact(artifact) {
  const defs = CHECKS[artifact.kind] || [];
  const checks = defs.map((def) => {
    let ok = false;
    try {
      ok = Boolean(def.test(artifact));
    } catch {
      ok = false;   // a scanner field the artifact never got: treat as failing, not as a crash
    }
    return {
      id: def.id,
      label: def.label,
      missing: def.missing,
      ok,
      fix: ok ? '' : def.fix(artifact)
    };
  });
  const passed = checks.filter((c) => c.ok).length;
  return {
    ...artifact,
    checks,
    passed,
    total: checks.length,
    score: checks.length ? passed / checks.length : 1
  };
}

export function checkAll(artifacts) {
  return artifacts.map(checkArtifact);
}

export const CHECK_DEFS = CHECKS;
