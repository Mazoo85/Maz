# SCRIPT FORGE

**Type what your film is about. Watch the film. Download the video file.**

▶ **Open it: [mazoo85.github.io/Maz/film/](https://mazoo85.github.io/Maz/film/)**
(or, in this repo, open `film/index.html` in a browser)

You write one line — *"A lonely lighthouse keeper finds a radio that plays
tomorrow's news"* — and SCRIPT FORGE reads it for who is in it, where it
happens, what it turns on and what kind of film it wants to be. Then it builds
three things from that:

1. a properly formatted **screenplay** — slug lines, action, dialogue;
2. a **shot list** for every scene;
3. the **film itself** — an animated short you can watch in the page and
   download as a real video file, with a score, character voices and titles.

Everything runs in your browser. Nothing is uploaded, there is no account and
no API key, and it works with the wifi off.

---

## Using it

1. Type your idea in the box. A sentence is plenty; a paragraph is fine too.
2. Pick a **length** — 3, 5 or 7 scenes.
3. Leave **genre** on *Auto* to let it read your idea, or force one.
4. Press **Write the script**.
5. Open the **🎬 The film** tab and press play.
6. Don't like this take? **🎲 Another take** rewrites and re-shoots it.

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
| **⬇ Make the video file** | the finished film as a `.webm` video | any browser, VLC, YouTube, Instagram, a phone |

The **seed** on each script is the number that draft came from. The same idea
always gives the same film; a new seed gives a new take on it. Saving stores the
idea, settings and seed — not the pages — so opening a saved script rebuilds it
exactly.

---

## The film

The **🎬 The film** tab is the film itself, drawn in the page: 2.35:1 widescreen,
title card, scene by scene, and out on THE END.

- **The picture** is drawn in code — fifteen sets (a lamp room, a kitchen, woods,
  a corridor, a ship, a ward…), silhouetted characters with a rim light, and a
  palette that comes from the genre and the hour. Nothing is downloaded, so it
  looks the same offline as online.
- **The camera** pushes, pulls and pans; scenes open wide, dialogue plays in
  close-up, and the object the story turns on gets its own insert.
- **The sound** is generated too: a chord bed that opens up as the film gets
  tenser, a pulse under the tense stretches, a hit on every cut, and a voice for
  each character — pitched blips, one per syllable, low for the lead and higher
  for the foil.
- **Captions** carry the action lines and the dialogue, held long enough to read.

Press **⬇ Make the video file** and you get a real `.webm` — VP9 picture, Opus
sound — that plays in any browser, VLC, or straight up to YouTube, Instagram or
a phone.

**Worth knowing before you press it:**

- **Recording happens in real time.** A two-minute film takes two minutes, and
  the tab has to stay open and visible while it records. There is no faster way
  to do this in a browser.
- **Stopping early keeps what you shot.** ⏹ Stop during a recording finishes the
  file at that point rather than throwing it away.
- **Chrome, Edge and Firefox on a computer can save video. Safari and most
  phones cannot** — they will still play the film in the page, and the app tells
  you plainly if saving is unavailable.
- **Scrubbing inside the downloaded file is limited** until it has been through
  an editor or an upload: browsers record video as one continuous chunk with no
  index. It plays start to finish everywhere, and shows the right length —
  SCRIPT FORGE writes the duration into the file afterwards, which the browser
  itself does not do. To jump around, use the scrub bar in the app, which draws
  any frame on demand.
- **File size** is roughly 2 MB per 10 seconds at 720p, four times that at 1080p.

There is also a **read the lines aloud** option, which uses your browser's own
speech voice while the film plays. It is a live extra only: browsers do not let
a page capture that audio, so the downloaded file keeps the character voices
instead. The checkbox says so.

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

**The director** (`js/film-reel.js`) turns the finished script into a *reel*:
every shot, how long it holds, what set it plays on, how the camera moves, what
is heard over it and how tense the moment is. It is plain data, so the entire
edit of a film can be checked in a test without a browser.

**The artist** (`js/film-art.js`) draws the sets, the figures and the objects.
**The score** (`js/film-audio.js`) plays them. **The camera**
(`js/film-player.js`) puts the two together, frame by frame, and records them.
**The file fixer** (`js/film-webm.js`) writes the duration into the finished
video, which is the one thing the browser's recorder leaves out.

`js/lexicon.js` and `js/dialogue.js` are the words. `js/format.js` is every
script export. `js/app.js` is the page.

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
  js/film-reel.js     a script       → a reel: timed shots, framing, voices
  js/film-art.js      15 sets, figures, objects, genre palettes
  js/film-audio.js    the score, the cuts and the character voices
  js/film-player.js   draws any frame; plays and records the film
  js/film-webm.js     writes the duration into the recorded file
  js/app.js           buttons, rendering, the projector, the saved library
  tests/film-logic.test.js
  tests/film-browser.test.js
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
opens the shot list, rerolls, downloads all four script files and checks their
contents, saves and reloads, plays the film and confirms the picture actually
moves, scrubs, records a video file and then *plays that file back* to prove it
carries picture, sound and a real running time — and confirms nothing scrolls
sideways on a phone.

Both run in **[Site CI](../.github/workflows/site-ci.yml)** on every push.

## Honest limits

- It writes *structure*, not genius. Treat the output as a strong first draft to
  cut, rewrite and make yours — which is the normal life of a first draft.
- The film is an **animated short in a graphic-novel style**, not live action
  and not photoreal AI footage. Silhouettes, sets drawn in code, captions and a
  synthesised score. It is a real film you can watch and share; it is not a
  camera.
- Dialogue comes from a hand-written bank steered by your genre and beat, so
  two very different ideas in the same genre can share a line. Reroll, or
  rewrite the line — it is your film.
- It reads English, and reads it plainly. Sarcasm and metaphor go over its head.
