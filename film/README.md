# SCRIPT FORGE

**Type what your film is about. Get a shootable short-film script back.**

▶ **Open it: [mazoo85.github.io/Maz/film/](https://mazoo85.github.io/Maz/film/)**
(or, in this repo, open `film/index.html` in a browser)

You write one line — *"A lonely lighthouse keeper finds a radio that plays
tomorrow's news"* — and SCRIPT FORGE reads it for who is in it, where it
happens, what it turns on and what kind of film it wants to be, then builds a
properly formatted screenplay on a short-film beat spine: slug lines, action,
dialogue, a shot list for every scene, and an honest runtime estimate.

Everything runs in your browser. Nothing is uploaded, there is no account and
no API key, and it works with the wifi off.

---

## Using it

1. Type your idea in the box. A sentence is plenty; a paragraph is fine too.
2. Pick a **length** — 3, 5 or 7 scenes.
3. Leave **genre** on *Auto* to let it read your idea, or force one.
4. Press **Write the script**.
5. Don't like this take? **🎲 Another take** rewrites the same idea differently.

Then take it out of the app:

| Button | What you get | Open it with |
|---|---|---|
| **📋 Copy** | the whole script as text | anywhere |
| **⬇ .fountain** | the screenwriter's plain-text format | Final Draft, Highland, Slugline, Fade In, WriterDuet, Celtx |
| **⬇ .fdx** | Final Draft's own file | Final Draft |
| **⬇ .txt** | laid out in the standard Courier columns | Notepad, Word, print |
| **⬇ shot list** | every scene, its job, and the shots to cover it | Notes, GitHub, anything Markdown |
| **🖨 Print / PDF** | a clean white screenplay page | your printer, or "Save as PDF" |
| **★ Save** | keeps the idea in this browser so you can reopen the exact same draft | this device only |

The **seed** on each script is the number that draft came from. The same idea
always gives the same film; a new seed gives a new take on it. Saving stores the
idea, settings and seed — not the pages — so opening a saved script rebuilds it
exactly.

---

## What it actually does

It is not a chatbot and does not call one. It is a *reader* and a *writer*:

**The reader** (`js/parse.js`) pulls a premise out of your sentence —

- **who**: names it finds (`Ada`, or "a man named Tobias") and jobs or
  relationships it knows (`lighthouse keeper`, `detective`, `sister`). A role
  behind a possessive — "*her* father" — belongs to the second character, not
  the lead. Capitals after "in" or "at" are read as places, not people.
- **where**: locations in your text, or the one that comes with the job, plus a
  second location the film can cut to.
- **what it turns on**: the thing after "finds / receives / steals / opens…",
  or a known object in your text. A location never gets used as the object.
- **what kind of film**: ten genres, scored on keyword hits; you can override it.
- **when** and **what the hero wants**: read from the words, with a fallback.

Anything it can't find, it chooses — seeded from your text, so the choice is
stable.

**The writer** (`js/screenplay.js`) lays the premise on a seven-beat spine —
Ordinary → Disruption → The Push → Complication → Crisis → The Choice → After —
and gives each beat a scene: a slug line, action lines built from the beat's
own bank, and a dialogue *exchange* (whole exchanges, not stray lines, so what
the characters say follows on). The film opens and closes in the same location,
because that is what makes an ending feel like one.

`js/lexicon.js` and `js/dialogue.js` are the words. `js/format.js` is every
export. `js/app.js` is the page.

---

## Files

```
film/
  index.html          the page
  css/style.css       neon shell, paper-white screenplay page, print styles
  js/lexicon.js       genres, places, roles, objects, names, beats
  js/dialogue.js      dialogue exchanges by beat and genre
  js/parse.js         your sentence  → a premise
  js/screenplay.js    a premise      → scenes, elements, shots, runtime
  js/format.js        a script       → .fountain / .txt / .fdx / shot list
  js/app.js           buttons, rendering, the saved library
  tests/film-logic.test.js
```

## Checking it

```
node film/tests/film-logic.test.js                  # no dependencies, instant

npm --prefix music/tests install                    # once, for Playwright
node film/tests/film-browser.test.js                # the real app, in Chromium
```

The first suite has no dependencies. It proves the reader gets the obvious cases right, that no
script ever reaches the page with an unfilled placeholder or an orphan line of
dialogue, that the same idea rebuilds the same film, and that all four exports
come out well-formed. The second one drives the actual page: it types an idea, presses the button,
opens the shot list, rerolls, downloads all four files and checks their
contents, saves and reloads, and confirms nothing scrolls sideways on a phone.

Both run in **[Site CI](../.github/workflows/site-ci.yml)** on every push.

## Honest limits

- It writes *structure*, not genius. Treat the output as a strong first draft to
  cut, rewrite and make yours — which is the normal life of a first draft.
- Dialogue comes from a hand-written bank steered by your genre and beat, so
  two very different ideas in the same genre can share a line. Reroll, or
  rewrite the line — it is your film.
- It reads English, and reads it plainly. Sarcasm and metaphor go over its head.
