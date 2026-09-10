<!-- Part of MAZ ARCADE — see the repo root README for every project. -->

# MadLibs Story Forge

A zero-dependency browser app that **randomly forges story ideas** — each one
broken into labeled story **beats** (Logline → Setup → Inciting Incident →
Conflict → Climax → Resolution) so it drops straight into a storyboard or
script-writing workflow.

It fills MadLibs-style templates with words drawn randomly from a categorized
dictionary. **45 templates across 34 genres** × large word lists yields
**~10²⁸ distinct stories** — far more than the 5,000 target — and the app can
batch-export a whole library at once.

No build step, no dependencies. Pure HTML + CSS + vanilla JavaScript.

```
cd madlibs
python3 -m http.server      # then visit http://localhost:8000
# ...or just open index.html directly in a browser
```

## What it does

- **Forge a Story** — random template + randomly filled blanks, rendered as beat cards.
- **Genre filter** — fantasy, sci-fi, horror, mystery, romance, comedy, heist, noir,
  crime, cyberpunk, western, war, sports, historical, pirate, time-travel, and more.
- **Reroll Words** — keep the current template, roll a fresh set of words.
- **Copy / Export .md** — grab the current idea as Markdown.
- **Save to Library** — keep ideas you like (persisted in `localStorage`).
- **Batch export** — download 50 / 500 / 5000 unique ideas as a single `.md` file.

## Project layout

```
madlibs/
  index.html          entry point
  css/style.css       neon/retro styling
  js/dictionary.js    categorized word lists  -> window.MADLIBS_DICT
  js/templates.js     story templates         -> window.MADLIBS_TEMPLATES
  js/generator.js     the engine              -> window.MadlibsGenerator
  js/app.js           UI wiring
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

---

← Back to the [**MAZ ARCADE hub**](../index.html) · [repository README](../README.md) · [play/open this one](../madlibs/)
