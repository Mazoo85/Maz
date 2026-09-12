# 🎲 NAME FORGE

**Random name maker.** One adjective, one noun, and a coin flip for which of
the two goes first.

### ▶ [Open it](https://mazoo85.github.io/Maz/names/)

```
1000 adjectives  ×  1000 nouns  ×  2 possible orders  =  2,000,000 names
```

Every roll is random three times over: the adjective, the noun, and — the part
most name generators skip — **the order**. `Crimson Falcon` and
`Falcon Crimson` are both live on every single roll. If you do want the order
pinned, you can, but random is the default.

Open `index.html`. No build step, no server, no dependencies, nothing leaves
your browser.

---

## What it does

| | |
|---|---|
| 🎲 **Roll a name** | One name, big, front and centre. Press space to roll again. |
| 🔄 **Flip the order** | Keeps the two words, swaps which one leads. |
| 🎛 **Word order** | Random (default), adjective first, or noun first. |
| ✍️ **Style** | `Two Words`, `OneWord`, `hyphen-case`, `snake_case`, `lower case`, `SHOUT CASE`, or surprise me. |
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
├── js/generator.js       # the engine — picks the words AND rolls the order
├── js/app.js             # the UI: buttons, batches, saved names, export
└── tests/names-logic.test.js
```

`words.js` and `generator.js` are plain modules with no DOM in them, so they
run in the browser and in Node alike. That is what lets the whole thing be
tested without a browser:

```bash
node names/tests/names-logic.test.js
```

Those tests are the app's promises, written down: exactly 1000 words in each
list, no duplicates, no word ever paired with itself, and — checked over a
thousand rolls — an order that really is a coin flip rather than
adjective-first with a coat of paint.

## Adding words

Open `js/words.js` and add to either list. The words are grouped under
comments (colour, beasts, weather, myth…) purely so they're easy to read;
the engine just sees one long list of each. Two rules, both enforced by the
tests: plain lowercase letters only, and the lists must stay at exactly 1000
entries each — so if you add a word, retire one you like less.
