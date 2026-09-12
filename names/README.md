# 🎲 NAME FORGE

**Random name maker.** A random adjective, then a random noun — `Crimson
Falcon`, `Velvet Mantis`, `Sunken Harbour`.

### ▶ [Open it](https://mazoo85.github.io/Maz/names/)

```
1000 adjectives  ×  1000 nouns  =  1,000,000 names
```

The words are random; the shape of the name is not. Every name comes out as
an adjective and then a noun, because that is what reads like a name. (If you
ever want it the other way round, or want the app to flip a coin each time,
the **Word order** dropdown will do it — but adjective-first is the default
and what it does when left alone.)

Open `index.html`. No build step, no server, no dependencies, nothing leaves
your browser.

---

## What it does

| | |
|---|---|
| 🎲 **Roll a name** | One name, big, front and centre. Press space to roll again. |
| ✍️ **Style** | `Two Words`, `OneWord`, `hyphen-case`, `snake_case`, `lower case`, `SHOUT CASE`, or surprise me. |
| 🎛 **Word order** | Adjective first (default), noun first, or let the dice decide. |
| 📚 **Batches** | 10, 25 or 100 at a time, all different from each other. |
| ★ **Saved names** | Kept in your browser's local storage; copy or export any time. |
| ⬇ **Export** | `.txt` (just the names) or `.csv` (name, both words, order, style, seed). |

Every name carries a **seed**. Same seed, same name — so a name you liked can
be rolled again later even if you didn't save it.

---

## How it's built

```
names/
├── index.html            # the page
├── css/style.css         # neon theme, no dependencies
├── js/words.js           # the word bank: 1000 adjectives + 1000 nouns
├── js/generator.js       # the engine — draws the pair, builds the name
├── js/app.js             # the UI: buttons, batches, saved names, export
├── manifest.webmanifest  # lets it install to a home screen
├── sw.js                 # service worker: the offline copy
├── icons/                # 192, 512 and apple-touch, drawn to match
└── tests/
    ├── names-logic.test.js     # the engine, headless
    └── names-browser.test.js   # the real page, in Chromium
```

`words.js` and `generator.js` are plain modules with no DOM in them, so they
run in the browser and in Node alike. That is what lets the engine be tested
without a browser:

```bash
node names/tests/names-logic.test.js     # word bank + engine
node names/tests/names-browser.test.js   # drives the page in Chromium
```

Those tests are the app's promises, written down: exactly 1000 words in each
list, no duplicates, no word ever paired with itself, and — checked over a
thousand rolls — a name that is always an adjective followed by a noun.

## Offline, and installable

The app never needed the network to roll a name — the word banks are plain
scripts in the page — so the only thing standing between it and working with
no signal was the browser having to fetch those files. `sw.js` caches them on
the first visit, which makes the whole app work offline afterwards, and
`manifest.webmanifest` lets it be installed to a home screen and opened like
an app rather than a tab.

This is not taken on trust: the browser suite opens the app, cuts the
connection at the browser, reloads, and asserts it still comes up styled and
still rolls a name. If the service worker ever stops registering, that section
fails rather than quietly leaving people with a dead page in a tunnel.

Change any file the app is made of and bump `CACHE` in `sw.js`, or browsers
will keep serving the copy they already have.

## Adding words

Open `js/words.js` and add to either list. The words are grouped under
comments (colour, beasts, weather, myth…) purely so they're easy to read;
the engine just sees one long list of each. Two rules, both enforced by the
tests: plain lowercase letters only, and the lists must stay at exactly 1000
entries each — so if you add a word, retire one you like less.
