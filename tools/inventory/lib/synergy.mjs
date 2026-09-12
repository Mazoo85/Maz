/*
 * Cross-pollination — what each program can lend the others.
 *
 * Two kinds of entry live here, and the difference matters:
 *
 *  - COMPUTED opportunities are derived from the scan. "These five helpers are
 *    written out in four projects each" is a fact on disk, and the entry is
 *    regenerated every run, so it disappears the moment the work is done.
 *
 *  - DECLARED pairings are judgement — "SONG FORGE can score the silent
 *    browser games" is not something a scanner can conclude. They are written
 *    down here so they can be read and argued with rather than invented afresh
 *    each time, and every one names real artifact ids: a pairing pointing at
 *    something that no longer exists is reported as stale and fails --check.
 *    That is what stops this table from quietly becoming fiction.
 */

/**
 * Reviewed pairings. `from` lends a capability, `to` gains one.
 * `effort` is small (an afternoon) / medium (a day) / large (a project).
 */
export const DECLARED_PAIRINGS = [
  {
    id: 'codapics-painter-needs-a-caller-that-speaks-its-language',
    from: ['web:coda-pics'],
    to: ['web:film', 'web:madlibs'],
    title: 'CODA PICS can be lent now — but not to SCRIPT FORGE or MADLIBS as they speak today',
    detail:
      'The surface exists: coda-pics/painter is published, takes a sentence and a canvas, and — ' +
      'unlike the studio page — reports how much of the picture came from the words. It refuses ' +
      'rather than guessing when asked to, because CODA PICS invents a subject for anything it does ' +
      'not recognise and a caller cannot otherwise tell a picture of the thing it asked for from a ' +
      'picture of something else.\n\n' +
      'What is NOT true is the obvious next step, and it was measured rather than assumed. Fed ' +
      'SCRIPT FORGE\'s scene headings, about half painted something unrelated, and fed MADLIBS ' +
      'loglines, six of twelve were refused outright with the rest only loosely related. Those ' +
      'figures are from before the trunk taught the reader to understand more of what people type, ' +
      'so the ratio has moved — but the cause has not: CODA PICS knows a set of subjects and ' +
      'settings chosen for being PAINTABLE, and neither a screenplay nor a story generator draws ' +
      'from that list. Re-measure before acting on the numbers.\n\n' +
      'So the work is not wiring, it is words. Either CODA PICS\'s lexicon grows the interiors and ' +
      'institutions a screenplay is full of — stairwells, offices, police stations, hospital rooms — ' +
      'or a caller translates its own vocabulary into CODA PICS\'s before asking. Until one of those ' +
      'happens, a consumer would get a refusal half the time and a wrong picture some of the rest, ' +
      'and no amount of integration code improves that.',
    effort: 'large',
    value: 2
  },
  {
    id: 'the-template-filler-is-lendable-now',
    from: ['web:madlibs'],
    to: ['web:coda-pics'],
    title: 'MADLIBS\' template filler is lendable now — and CODA PICS no longer needs it',
    detail:
      'This entry has been wrong twice, and both corrections are the useful part of it. It first ' +
      'said MADLIBS could hand CODA PICS a "surprise me" prompt. Tried: MADLIBS writes story prose ' +
      'and CODA PICS parses scene descriptions, so a story beat painted "a sword in stone in an ' +
      'island at sunset". It then said the machinery underneath was the real shared thing, and that ' +
      'CODA PICS\'s own copy of it got articles wrong — "a orange fish", about one prompt in forty.\n\n' +
      'Both halves are now done, and they did not need to meet. CODA PICS\'s article bug is fixed ' +
      'where it lived, with a regression test. And madlibs/js/generator.js\'s fillTemplate and pick ' +
      'now take a dictionary instead of reading MADLIBS_DICT at load time, so the filler is ' +
      'published as madlibs/templates: seeded, reproducible, tagged words consistent across beats, ' +
      'and the articles right — for anyone\'s vocabulary.\n\n' +
      'CODA PICS is deliberately NOT wired to it. What it would gain now is tag consistency and ' +
      'sentence capitalisation, and its four surprise templates need neither; what it would pay is a ' +
      'cross-project dependency for a button. The capability is there for the next caller that has a ' +
      'real template to fill, which is the right place for it to wait.',
    effort: 'small',
    value: 1
  },
  {
    id: 'scraper-fills-the-worlds',
    from: ['py:scraper'],
    to: ['web:zomboid', 'app:village', 'app:world'],
    title: 'MAZ-SCRAPE fills the game worlds with real data',
    detail:
      'The scraper turns a YAML recipe into JSONL/CSV/SQLite from static HTML. ZOMBOID\'s Anchorage, ' +
      'apps/village and apps/world are all populated by hand-written name and place tables today. A ' +
      'recipe that harvests real street, business and place names into a JSON table the games load ' +
      'would make all three worlds larger without a line of new game code.',
    effort: 'medium',
    value: 2
  },
  {
    id: 'engine-gains-a-composer',
    from: ['web:music'],
    to: ['engine:audio/MusicSequencer', 'engine:audio/MusicTheory', 'app:music'],
    title: 'SONG FORGE supplies the one music layer the engine does not have',
    detail:
      'The engine already has the layers underneath and above a composer: audio::MusicTheory does ' +
      'note/pitch conversion, audio::MusicScales the scale tables, audio::Oscillator and ' +
      'audio::BusGraph the synthesis and mixing, and audio::MusicSequencer switches between music ' +
      'segments on the beat as the action changes. What nothing under engine/include/maz/audio/ does ' +
      'is WRITE the segments — pick a progression, lay a bassline and a drum pattern under it, ' +
      'arrange verses and choruses. music/js/genres.js and music/js/composer.js do exactly that, as ' +
      'plain data and pure functions, for eight genres. Porting them to an ' +
      'engine/include/maz/audio/MusicComposer.hpp that emits segments for MusicSequencer to switch ' +
      'between would give every native Maz game an endless, non-repeating, state-aware score, and ' +
      'apps/music is already standing there as the demo to extend.',
    effort: 'large',
    value: 3
  },
  {
    id: 'goldens-become-the-arcade-shelf',
    from: ['app:pong'],
    to: ['web:film'],
    title: 'The golden screenshots become the arcade\'s cover art',
    detail:
      'tests/golden/ holds a deterministic captured frame for most apps, produced purely to catch ' +
      'rendering regressions. That is also a ready-made, always-current screenshot library: the hub ' +
      'at index.html lists every project as text today, and could show each native demo\'s golden ' +
      'frame as its tile art at zero maintenance cost, because CI regenerates them.',
    effort: 'small',
    value: 2
  }
];

/**
 * Both wordings of a counted headline, picked by the count. Worth the six lines:
 * these titles are the first thing read in the report and in the Forge's queue,
 * and "1 docs are not linked from anywhere" reads like a bug in the tool.
 */
function counted(n, one, many) {
  return n === 1 ? one : many.replace('{n}', String(n));
}

/**
 * Pairings that are already built. These generate no work — they are here so
 * the report says how the programs currently feed each other, not only how they
 * could. A wiring that stops being true should be moved back up to
 * DECLARED_PAIRINGS rather than quietly deleted; both lists are validated the
 * same way, so neither can name an artifact that no longer exists.
 */
export const REALIZED_PAIRINGS = [
  {
    id: 'film-consumes-music-and-madlibs',
    from: ['web:music', 'web:madlibs'],
    to: ['web:film'],
    title: 'SCRIPT FORGE is scored by SONG FORGE and seeded by MADLIBS',
    detail:
      'film/index.html loads music/js and madlibs/js directly: every short film is scored by the ' +
      'same composer SONG FORGE uses, and an empty idea box borrows one of MADLIBS\'s stories rather ' +
      'than failing. Declared in shared/exchange.json as music/composer and madlibs/storyideas, and ' +
      'held to it by film/tests/film-logic.test.js and scripts/check-exchange.mjs.'
  },
  {
    id: 'songforge-scores-zomboid',
    from: ['web:music'],
    to: ['web:zomboid'],
    title: 'SONG FORGE writes the soundtrack for ZOMBOID: ANCHORAGE',
    detail:
      'ZOMBOID used to loop an eight-step bassline against an eight-step lead for as long as you ' +
      'played, knowing nothing about the game. It now consumes music/soundtrack — a small ' +
      'playback-only surface over SONG FORGE\'s composer, published in shared/exchange.json — and ' +
      'plays composed, arranged chiptune that does not repeat and follows the city: calm by day, ' +
      'dark at night, driving when something is close. The music plays through ZOMBOID\'s own audio ' +
      'context and music bus, so its mute key and levels still apply, and its sound effects, which ' +
      'are its character, are untouched. Composing a new track on a mood change costs 1-14ms ' +
      'measured, so it happens mid-play without dropping a frame.\n\n' +
      'DEAD SECTOR deliberately does NOT do this. Its whole identity is one self-contained HTML file ' +
      'you can email to someone — it says so on its card, and shooter/tests/shooter-logic.test.js ' +
      'fails if an external script appears in it. Loading six of SONG FORGE\'s modules would buy ' +
      'better music by spending the thing the project is for, and embedding a copy of the composer ' +
      'would be exactly the drifting duplicate this inventory exists to find. Its six-note scale walk ' +
      'stays.'
  },
  {
    id: 'inventory-feeds-the-forge',
    from: ['tool:inventory'],
    to: ['py:forge', 'py:crew'],
    title: 'The inventory queue is a Forge signal, and Crew does the work',
    detail:
      'forge/forge/signals/inventory.py reads the ranked queue out of docs/inventory.json and turns ' +
      'it into candidates for the nightly run, fanning a grouped task ("38 apps have no headless ' +
      'mode") out into one candidate per app so each night gets a job it can finish. The Forge hands ' +
      'the pick to Maz Crew — planner, coder, reviewer, tester — which is the same single-scoped-' +
      'instruction-with-paths shape the inventory writes. So the catalogue, the scheduler and the ' +
      'coding team close a loop: what this report finds becomes work that gets done without anyone ' +
      'asking, inside the zones the Forge is allowed to verify.'
  }
];

/** "145 in render, 117 in math, …" — so the biggest number in the report is plannable. */
function bySubsystem(modules) {
  const counts = new Map();
  for (const m of modules) counts.set(m.subsystem, (counts.get(m.subsystem) || 0) + 1);
  const parts = [...counts.entries()]
    .sort((a, b) => b[1] - a[1])
    .map(([sub, n]) => `${n} in \`${sub}\``);
  return `They are not spread evenly: ${parts.slice(0, 6).join(', ')}` +
    `${parts.length > 6 ? `, and ${parts.length - 6} other subsystems` : ''}.`;
}

/** Opportunities the scan can prove, regenerated every run. */
export function computeOpportunities(model) {
  const out = [];
  const { apps, modules, webApps, duplicateHelpers: dupes, docs } = model;

  const testedNotDemoed = modules.filter((m) => m.testedBy.length > 0 && m.demoedBy.length === 0);
  if (testedNotDemoed.length) {
    out.push({
      id: 'engine-features-with-no-demo',
      covers: 'engine-module:demoed',
      kind: 'computed',
      title: counted(testedNotDemoed.length,
        'One engine module is tested but no app shows it',
        '{n} engine modules are tested but no app shows them'),
      detail:
        'These are finished, working features that nobody can see, and `apps/` is how this engine ' +
        'documents itself. ' + bySubsystem(testedNotDemoed) + ' Writing 499 apps is not the answer ' +
        'and never was: one demo can show a dozen related modules at once — a single "mesh repair" ' +
        'app for the degenerate, self-intersection and closest-point analysers, one "curve fitting" ' +
        'app for the least-squares family — so the work is closer to a few dozen apps than 499. ' +
        'Start where the count is highest and the modules cluster most naturally.',
      subjects: testedNotDemoed.map((m) => m.id),
      effort: 'medium',
      value: 3
    });
  }

  const dark = modules.filter((m) => m.testedBy.length === 0 && m.demoedBy.length === 0);
  if (dark.length) {
    out.push({
      id: 'engine-features-with-neither',
      kind: 'computed',
      title: counted(dark.length,
        'One engine module has neither a test nor a demo',
        '{n} engine modules have neither a test nor a demo'),
      detail:
        'Nothing in the repo names these types. They are either genuinely unused — in which case they ' +
        'are unproven code that will rot — or they are used through another module\'s API and only ' +
        'look unused. Both readings are worth resolving: a test settles it either way.',
      subjects: dark.map((m) => m.id),
      effort: 'large',
      value: 2
    });
  }

  const noHeadless = apps.filter((a) => !a.headless);
  if (noHeadless.length) {
    out.push({
      id: 'apps-that-cannot-run-in-ci',
      covers: 'app:headless',
      kind: 'computed',
      title: counted(noHeadless.length,
        'One app cannot run without a display',
        '{n} apps cannot run without a display'),
      detail:
        'Apps with --headless / --frames N are run by CI on every push and can be captured as goldens; ' +
        'apps without them are only ever proven by someone opening a window. The flag is a dozen lines ' +
        'copied from apps/_template/main.cpp and it converts each app into a test.',
      subjects: noHeadless.map((a) => a.id),
      effort: 'medium',
      value: 3
    });
  }

  const untestedWeb = webApps.filter((w) => !w.hasTests);
  if (untestedWeb.length) {
    out.push({
      id: 'browser-projects-with-no-tests',
      covers: 'web-app:tests',
      kind: 'computed',
      title: counted(untestedWeb.length,
        'One browser project has no tests at all',
        '{n} browser projects have no tests at all'),
      detail:
        'film/tests/film-logic.test.js is the pattern: plain `node` over the project\'s pure logic, no ' +
        'dependencies, seconds to run, already wired into CI. Every project that lacks it is one ' +
        'refactor away from silent breakage.',
      subjects: untestedWeb.map((w) => w.id),
      effort: 'medium',
      value: 3
    });
  }

  // Every entry here is the same normalised code in two or more projects, not
  // merely the same name — six projects declare a `noise` and no two of them
  // mean the same thing by it, so name-matching alone would send someone to
  // merge functions that have nothing in common.
  if (dupes.length) {
    out.push({
      id: 'helpers-copied-between-projects',
      kind: 'computed',
      title: counted(dupes.length,
        'One helper is written out identically in two browser projects',
        '{n} helpers are written out identically in more than one browser project'),
      detail:
        `${dupes.map((d) => '`' + d.name + '(' + d.params + ')`' +
          ' in ' + d.projects.join(' and ')).join(', ')}. ` +
        'These are the same code in more than one place, so one shared/maz-util.js — declared in ' +
        'shared/exchange.json, covered by one test, loaded by each page — replaces every copy and ' +
        'means a fix lands everywhere at once. Start with these: they are proven identical, so ' +
        'moving them cannot change behaviour.',
      subjects: dupes.map((d) => `helper:${d.name}`),
      effort: 'small',
      value: 3
    });
  }

  const orphanDocs = docs.filter((d) => !d.linkedFrom);
  if (orphanDocs.length) {
    out.push({
      id: 'docs-nothing-links-to',
      covers: 'doc:linked',
      kind: 'computed',
      title: counted(orphanDocs.length,
        'One doc is not linked from anywhere',
        '{n} docs are not linked from anywhere'),
      detail:
        'A doc that nothing references is invisible to a newcomer and to a future session, however ' +
        'good it is. Link each from README.md or a sibling doc, or retire it.',
      subjects: orphanDocs.map((d) => d.id),
      effort: 'small',
      value: 1
    });
  }

  return out;
}

/**
 * Validate the declared table against the scanned model.
 * Returns { pairings, stale } — stale entries name an artifact that is gone.
 */
export function validatePairings(model, pairings = DECLARED_PAIRINGS) {
  const known = new Set(model.all.map((a) => a.id));
  // The inventory catalogues itself: it is an artifact of this repo like any
  // other, and pairings are allowed to name it.
  known.add('tool:inventory');

  const ok = [];
  const stale = [];
  for (const p of pairings) {
    const missing = [...p.from, ...p.to].filter((id) => !known.has(id));
    if (missing.length) stale.push({ ...p, missing });
    else ok.push(p);
  }
  return { pairings: ok, stale };
}
