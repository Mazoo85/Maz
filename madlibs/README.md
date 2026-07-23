# MadLibs Story Forge

A zero-dependency browser app that **randomly forges story ideas** — each one
broken into labeled story **beats** (Logline → Setup → Inciting Incident →
Conflict → Climax → Resolution) so it drops straight into a storyboard or
script-writing workflow.

It fills MadLibs-style templates with words drawn randomly from a categorized
dictionary. **45 templates across 34 genres** × large word lists yields
**~10²⁸ distinct stories** — far more than the 5,000 target — and the app can
batch-export a whole library at once.

No build step, no dependencies. Pure HTML + CSS + vanilla JavaScript — and it's
an **installable PWA** (Progressive Web App) that runs fully **offline**.

```
cd madlibs
python3 -m http.server      # then visit http://localhost:8000
```

> **Installing / offline support:** the service worker and "Install app" prompt
> need the app to be served over `http(s)` (or `localhost`) — not opened as a
> `file://` path. Use the `http.server` command above (or any static host).
> Once loaded, the whole app is cached and works with no network.

## Install it as an app

- **Desktop (Chrome/Edge):** click the **⬇ Install app** button in the header,
  or the install icon in the address bar.
- **Android (Chrome):** menu → **Install app** / **Add to Home screen**.
- **iOS (Safari):** Share → **Add to Home Screen**.

Installed, it launches in its own window with its own icon and works offline.

## What it does

- **Forge a Story** — random template + randomly filled blanks, rendered as beat cards.
- **Genre filter** — fantasy, sci-fi, horror, mystery, romance, comedy, heist, noir,
  crime, cyberpunk, western, war, sports, historical, pirate, time-travel, and more.
- **Reroll Words** — keep the current template, roll a fresh set of words.
- **Copy / Export .md** — grab the current idea as Markdown.
- **Save to Library** — keep ideas you like (persisted in `localStorage`).
- **Batch export** — download 50 / 500 / 5000 unique ideas as a single `.md` file.
- **Installable & offline** — add to your home screen / desktop; runs with no network.
- **🎬 Script this idea → Productions** — send an idea into production, copy a
  brief for Claude to write the 30-minute episode script, and track each project
  from Idea → Scripting → Script chosen → Storyboarded (see below).

## From idea to script to storyboard

The app has three screens: **Forge** (generate ideas), **Productions** (every
idea you've sent to production), and a **Project** page for each one.

Click **🎬 Script this idea** on any story and it opens as a project with three
tabs:

- **Idea** — the beat outline, plus a one-click *brief* to hand to Claude.
- **Script** — the 30-minute teleplay, rendered as a real screenplay **inside the
  app** (paste what Claude writes; scene headings, character cues, and
  parentheticals format automatically). Export as `.fountain`.
- **Storyboard** — a visual shot-list of panels (framing, camera, action,
  dialogue, duration), rendered **inside the app**.

Everything is stored locally in your browser. A complete example project —
**The Museum Ghost** (idea + full script + 14-panel storyboard) — is built in so
you can see it all working immediately.

The full pipeline, script format, and storyboard format are documented in
**[`PRODUCTION.md`](PRODUCTION.md)**.

## Project layout

```
madlibs/
  index.html             entry point
  manifest.webmanifest   PWA manifest (name, icons, colors, standalone)
  sw.js                  service worker (offline app-shell cache)
  icons/                 app icons (192/512 + maskable + apple-touch)
  css/style.css          neon/retro styling
  js/dictionary.js       categorized word lists  -> window.MADLIBS_DICT
  js/templates.js        story templates         -> window.MADLIBS_TEMPLATES
  js/generator.js        the engine              -> window.MadlibsGenerator
  js/screenplay.js       in-app screenplay viewer-> window.Screenplay
  js/storyboard.js       in-app storyboard viewer-> window.Storyboard
  js/sample.js           built-in example project-> window.MADLIBS_SAMPLE
  js/app.js              UI wiring (3 screens + routing)
  PRODUCTION.md          idea → script → storyboard pipeline
  productions/<slug>/    generated briefs, scripts, and storyboards
  tools/                 screenplay + storyboard page templates
```

## How the templates work

Each template is a set of beats whose text contains `{placeholders}`:

```js
{
  id: 'lost-heir',
  title: 'The Lost Heir',
  genre: 'fantasy',
  beats: [
    { label: 'Logline', text: 'A {adjective} {profession} named {name#hero} discovers they are the last heir to {place#realm}.' },
    { label: 'Climax',  text: 'At {place}, {name#hero} confronts {name#villain}, wielding the {magicItem#relic}.' }
  ]
}
```

Placeholder grammar (resolved in `generator.js`):

| Syntax            | Meaning                                                                 |
| ----------------- | ----------------------------------------------------------------------- |
| `{category}`      | a fresh random word from that dictionary category                       |
| `{category#tag}`  | a random word **reused** everywhere the same `#tag` appears in a story  |
| `{a}` / `{a-cap}` | the article `a`/`an` (or `A`/`An`), auto-chosen from the following word  |

The `#tag` form is what keeps a story coherent — the protagonist's name, their
realm, and the one magic relic all stay consistent across every beat.

## Extending it

- **More words:** append strings to any array in `js/dictionary.js`. Every new
  word multiplies the number of possible stories.
- **More stories:** append a template object to the array in `js/templates.js`.
  Category names you reference must exist in the dictionary.

## Engine API (also runnable headless in Node)

```js
const G = require('./js/generator.js');
G.generate({ genre: 'horror' });     // -> one story {title, genre, beats, seed, signature}
G.generateMany(5000);                // -> 5000 de-duplicated stories
G.fillTemplate(template, { seed });  // -> reproducible fill for a given seed
G.estimateCombinations();            // -> { log10, pretty } scale estimate
G.toMarkdown(story);                 // -> Markdown string
```

Generation is driven by a small seeded RNG, so a `seed` reproduces a story
exactly — handy for sharing or regenerating a specific idea.
