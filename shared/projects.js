/*
 * The one list of everything in this repo.
 *
 * Both the MAZ ARCADE hub (/index.html) and the in-app nav bar
 * (shared/maz-nav.js) read this file, so a project is added or renamed in
 * exactly one place and every page that links to it updates at once.
 *
 * `path` is always relative to the repo root. Pages deeper than the root
 * prefix it with `../` (see MazNav.rootPrefix below).
 */
(function (global) {
  'use strict';

  var PROJECTS = [
    {
      id: 'zomboid',
      name: 'ZOMBOID: ANCHORAGE',
      kind: 'play',
      path: 'zomboid/',
      tag: '16-bit neon survival',
      accent: '#ff2d95',
      blurb:
        'Open-world zombie survival across a tile-built replica of downtown Anchorage, ' +
        'rendered as a 1990s SEGA arcade title. Five decaying needs, day/night hordes, ' +
        'looting, melee durability and firearms.',
      badges: ['Desktop', 'Keyboard + mouse', 'Canvas 2D'],
      docs: 'zomboid/README.md'
    },
    {
      id: 'cells',
      name: 'NEON CELLS',
      kind: 'play',
      path: 'cells/',
      tag: 'roguelite action platformer',
      accent: '#2fe6c8',
      blurb:
        'A Dead Cells-style run: procedurally built biomes, permadeath, cells and gold, ' +
        'three scroll colours that decide what your run becomes, weapons with rolled ' +
        'affixes, skills, mutations, elites, two bosses, and blueprints that stay unlocked.',
      badges: ['Phone + desktop', 'Controller · keyboard · touch', 'Canvas 2D'],
      docs: 'cells/README.md'
    },
    {
      id: 'shooter',
      name: 'DEAD SECTOR',
      kind: 'play',
      path: 'shooter/',
      tag: 'phone-first twin-stick',
      accent: '#59d95a',
      blurb:
        'Top-down twin-stick zombie shooter in a single self-contained HTML file. ' +
        'Dual touch joysticks, escalating waves, three zombie types.',
      badges: ['Phone', 'Touch', 'Single file'],
      docs: 'shooter/README.md'
    },
    {
      id: 'music',
      name: 'SONG FORGE',
      kind: 'app',
      path: 'music/',
      tag: 'generative AI music maker',
      accent: '#00e5ff',
      blurb:
        'Writes and plays complete songs in the browser — chords, bass, drums, arpeggio ' +
        'and melody arranged into verses and choruses across 8 genres. WAV and MIDI export, ' +
        'offline, no API key.',
      badges: ['Phone + desktop', 'WebAudio', 'WAV / MIDI export'],
      docs: 'music/README.md'
    },
    {
      id: 'madlibs',
      name: 'MADLIBS STORY FORGE',
      kind: 'app',
      path: 'madlibs/',
      tag: 'story-idea generator',
      accent: '#39ff14',
      blurb:
        'Randomly forges story ideas broken into scene beats, ready to seed a storyboard ' +
        'or script. Zero dependencies.',
      badges: ['Phone + desktop', 'Zero deps'],
      docs: 'madlibs/README.md'
    },
    {
      id: 'names',
      name: 'NAME FORGE',
      kind: 'app',
      path: 'names/',
      tag: 'random name maker',
      accent: '#ff8a3d',
      blurb:
        'Rolls names out of 1000 adjectives and 1000 nouns — a random adjective ' +
        'and then a random noun, a million of them, in six styles with batches ' +
        'and export.',
      badges: ['Phone + desktop', '1000 × 1000 words', 'Zero deps'],
      docs: 'names/README.md'
    },
    {
      id: 'film',
      name: 'SCRIPT FORGE',
      kind: 'app',
      path: 'film/',
      tag: 'idea → finished short film',
      accent: '#ffb300',
      blurb:
        'Type what your film is about and get the whole thing back: a formatted ' +
        'screenplay, a shot list, and an animated short film — performed by jointed ' +
        'characters and shot with a moving camera across sets built in depth, scored ' +
        'by SONG FORGE, and downloadable as a video file. Every story gets three to ' +
        'five locations and its own beat order, always building to a real crisis ' +
        'somewhere no earlier scene has been — and no idea in mind borrows one of ' +
        'MADLIBS\'s 45 stories instead, right in the idea box so you can see what your ' +
        'film is actually about.',
      badges: ['Phone + desktop', 'Plays + records video', 'Scored by SONG FORGE'],
      docs: 'film/README.md'
    },
    {
      id: 'coda-pics',
      name: 'CODA PICS',
      kind: 'app',
      path: 'coda-pics/',
      tag: 'words \u2192 picture',
      accent: '#00d68f',
      blurb:
        'Type what you want to see and it paints it: 60 subjects, 20 settings, every hour ' +
        'and weather, finished in one of 14 art styles from pixel art to watercolour. ' +
        'Drawn from scratch in the browser \u2014 offline, no API key, free.',
      badges: ['Phone + desktop', 'Canvas 2D', 'PNG download'],
      docs: 'coda-pics/README.md'
    },
    {
      id: 'engine',
      name: 'MAZ ENGINE',
      kind: 'code',
      path: 'docs/ROADMAP.md',
      tag: 'C++20 · Vulkan · SDL3',
      accent: '#b46bff',
      blurb:
        'A native, 2D-first game engine architected so 3D drops in later. Fixed-timestep ' +
        'loop, Vulkan renderer, sprite batching, scene + asset layers, headless CI.',
      badges: ['C++20', 'Vulkan', 'CMake'],
      docs: 'docs/ARCHITECTURE.md'
    },
    {
      id: 'scraper',
      name: 'MAZ-SCRAPE',
      kind: 'code',
      path: 'scraper/README.md',
      tag: 'recipe-driven scraper',
      accent: '#ffb300',
      blurb:
        'General-purpose scraper for static HTML. Point it at a YAML recipe ' +
        '(field → CSS selector); it crawls, extracts and writes JSONL/CSV/SQLite, politely.',
      badges: ['Python', 'YAML recipes', 'Offline tests'],
      docs: 'scraper/README.md'
    },
    {
      id: 'crew',
      name: 'MAZ CREW',
      kind: 'code',
      path: 'crew/README.md',
      tag: 'planner → coder → reviewer → tester',
      accent: '#ff6b6b',
      blurb:
        'Runs a coding task through a small team of specialised agents with human ' +
        'checkpoints, a review pass and a bounded test-repair loop.',
      badges: ['Python', 'Agents', 'Offline tests'],
      docs: 'crew/README.md'
    }
  ];

  if (typeof module === 'object' && module.exports) {
    module.exports = { PROJECTS: PROJECTS };
  }
  global.MAZ_PROJECTS = PROJECTS;
})(typeof globalThis !== 'undefined' ? globalThis : this);
