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
    id: 'codapics-paints-for-the-others',
    from: ['web:coda-pics'],
    to: ['web:film', 'web:madlibs'],
    title: 'CODA PICS paints backdrops for SCRIPT FORGE and illustrates MADLIBS',
    detail:
      'CODA PICS turns a sentence into a finished picture in canvas 2D, offline. SCRIPT FORGE builds ' +
      'its sets from primitives and MADLIBS returns pure text. Publishing coda-pics/js as ' +
      'coda-pics/painter would let SCRIPT FORGE paint a title card and a establishing backdrop per ' +
      'location straight from its own scene description, and let MADLIBS show each story idea rather ' +
      'than only describing it.',
    effort: 'medium',
    value: 3
  },
  {
    id: 'madlibs-seeds-the-games',
    from: ['web:madlibs'],
    to: ['web:zomboid', 'web:coda-pics'],
    title: 'MADLIBS seeds prompts and in-game flavour text',
    detail:
      'madlibs/storyideas is already published and already consumed by SCRIPT FORGE. The same ' +
      'generator can hand CODA PICS a "surprise me" prompt that is a real scene rather than a random ' +
      'noun, and give ZOMBOID the radio broadcasts and note scraps its world is missing. Consuming an ' +
      'already-published capability is the cheapest integration in the repo.',
    effort: 'small',
    value: 2
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
        'These are finished, working features that nobody can see. Each one is a small app away from ' +
        'being discoverable, and apps/ is how this engine documents itself. Grouping several related ' +
        'modules into one demo is usually better than one app each.',
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
