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
| **⬇ Make the video file** | the finished film as a video | see *Which format you get*, below |

The **seed** on each script is the number that draft came from. The same idea
always gives the same film; a new seed gives a new take on it. Saving stores the
idea, settings and seed — not the pages — so opening a saved script rebuilds it
exactly.

---

## The film

The **🎬 The film** tab is the film itself, drawn in the page: 2.35:1 widescreen,
title card, scene by scene, and out on THE END.

- **The picture** is drawn in code — fifteen sets (a lamp room, a kitchen, woods,
  a corridor, a ship, a ward…), each built in three planes (back, mid, fore) at
  different parallax rates so a pan or a push reads as a real space, with a
  palette that comes from the genre and the hour. Nothing is downloaded, so it
  looks the same offline as online.
- **The characters** are built from ten-joint skeletons, not fixed poses: a
  pose is a set of joint angles kept inside a declared human range, and the
  beat and the tension pick each character's bearing — still at the open,
  moving on the push, a body that breaks at the crisis and straightens at the
  choice. A speaking character never turns away from the room, and their head
  dips and near hand lifts on every syllable, on the same clock as their voice.
  They're drawn between the mid and fore planes, so a foreground element can
  pass in front of them.
- **The air** carries weather chosen by genre and hour — rain, dust, fog, heat
  shimmer, embers or haze — and the light itself moves within a scene: a beam
  sweeping, headlights crossing, a bulb flickering harder as the tension climbs.
- **The camera** pushes, pulls, pans and tracks; scenes open wide, dialogue
  plays in close-up, over-the-shoulder or low angle — alternating framings
  rather than repeating one, holding the choice beat longer than the push —
  and the object the story turns on gets its own insert. It goes handheld at
  the crisis, whips between shots inside a scene, and rolls a couple of
  degrees only where the story has come apart. In a close-up or two-shot
  where the fore plane has decor in view, that decor is cheaply thrown out
  of focus — lifted toward the palette's deep tone rather than darkened,
  since it is already near-black — instead of getting a real blur, which was
  prototyped and measured meaningfully more expensive per pixel.
- **The sound** is a real score. SONG FORGE composes a song for this film
  specifically — to its exact length, with its sections turning over on your
  scene cuts and its instruments following the story: pad and chords under the
  opening, bass as it builds, drums through the middle, the full band only at
  the crisis, and back down to pad to end. It plays live, ducking under every
  line of dialogue, and the character voices and cut hits sit over the top.
- **Captions** carry the action lines and the dialogue, held long enough to read.

### Which format you get

The browser decides, not the app, and it matters:

- **`.mp4` (H.264)** — plays on everything: iPhone, iPad, QuickTime, Android,
  every browser, straight up to YouTube or Instagram. You get this when the
  browser can genuinely record H.264.
- **`.webm` (VP9)** — plays on computers (Chrome, Edge, Firefox, VLC) and
  Android, but **not on an iPhone, an iPad or in QuickTime.**

The app names the format on the button before you record, and says plainly when
it is the second one. If you are stuck with a `.webm` and need it on an Apple
device: upload it somewhere that re-encodes (YouTube, Google Photos), or run it
through a free converter like HandBrake.

One trap worth knowing about, because the app deliberately avoids it: a browser
can answer "yes, I support `video/mp4`" and then write **VP9 video inside an MP4
wrapper** — a file called `.mp4` that an iPhone still cannot play, which is
worse than an honest `.webm` because the name promises otherwise. SCRIPT FORGE
only asks for MP4 by an explicit H.264 codec string, and treats a bare claim as
a no. Headless Chromium is one of the browsers that lies here; there is a test
for it.

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
- **File size** is roughly 1.4 MB per 10 seconds at 540p, 2.7 MB at 720p and
  7.5 MB at 1080p. Drawn art is flat colour and hard edges, so 540p holds up far
  better than camera footage would at the same size — pick it when you need to
  send the film somewhere with a size limit.
- **The score plays live while you record**, the same way the character voices
  and the cut hits always have — none of it is rendered in advance. On a slow
  machine that could in principle glitch the audio in a recording. It has not
  happened in testing.

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
- **where**: **three to five** locations — the ones your text (or the hero's
  job) names lead, then generic connectors (a car, a street, a hallway…) fill
  out the rest, deduplicated. A five-scene film that only ever offered two
  rooms is what this widened.
- **what it turns on**: the thing after "finds / receives / steals / opens…",
  or a known object in your text. A location never gets used as the object.
- **what kind of film**: ten genres, scored on keyword hits; you can override it.
- **when** and **what the hero wants**: read from the words, with a fallback.

Anything it can't find, it chooses — seeded from your text, so the choice is
stable. And if there's barely anything to read — the idea box is empty or
under three words — the reader doesn't guess in a vacuum: it borrows one of
**MADLIBS STORY FORGE**'s 45 stories instead (`js/story-seed.js`, reading
`../madlibs/js/generator.js` as a library, MADLIBS itself untouched), mapping
its 34 genres and its Setup/Inciting Incident/Conflict/Climax/Resolution beats
onto SCRIPT FORGE's own ten genres and seven beats. The **🎁 Surprise me**
button does the same borrowing on purpose, any time, thin idea or not.

**The writer** (`js/screenplay.js`) lays the premise on a seven-beat spine —
Ordinary → Disruption → The Push → Complication → Crisis → The Choice → After
— but not always the same one: every length offers **several different beat
orders** (a 5-scene film alone has three), seeded so the same idea keeps its
shape while a new take can pick a different one, and every **short** and
**festival** shape guarantees a **Crisis** beat — the default 5-scene length
always reaches one. Each beat plays in its own place drawn from the premise's
three to five locations, spread across the running time rather than picked
once for the whole film, decided for the whole spine at once
(`placesForSpine`) rather than beat by beat, so three rules can be weighed
against each other instead of quietly fighting over the same indices:

1. the film opens and closes in the same location, always — that is what
   makes an ending feel like one, for every length and every shape.
2. the **crisis lands somewhere no earlier scene in the film has used** — the
   worst moment of the night lands somewhere unfamiliar, the way a real
   crisis does. Every premise offers 3-5 places, and at 3-5 places this rule
   **always holds** (checked over thousands of generated films at every
   length; it can only give way below 3 places, which nothing this app
   generates ever offers).
3. whatever's left over is what the *other* middle beats spread across, as
   widely as that leaves room for. **Rule 3 is the one that narrows when the
   arithmetic is tight, never rule 1 or 2** — and it narrows hardest at the
   low end: with only 3 places, one is the opening (which is also the close)
   and one is reserved for the crisis, so *every* other middle beat shares the
   single remaining room, at every length. About a third of premises offer 3
   places, so that is a real share of films, not a corner case. At 4 places a
   festival film spreads its 4 non-crisis middle beats over 2 rooms; at 5 it
   spreads properly.

   This is still a clear gain on what came before — the average film now uses
   3.85 distinct locations against 3.39 under the previous rule, and every
   film used exactly 2 before this work — but it is a trade, not a free win,
   and the sharpest case is 3 places rather than festival length.

Each beat then gets a scene: a slug line, action lines built from the beat's
own bank, and a dialogue *exchange* (whole exchanges, not stray lines, so what
the characters say follows on).

**Exterior scenes get their own wording.** The images in the lexicon were
written when a film had two places and they were nearly always interiors —
so once films started using three to five, "Rain finds the same crack in the
sill it always finds" began landing in a parking lot. About one exterior
scene in two carried a line that needed a room around it. Those lines now
have an outdoor twin (`LEX.OUTDOORS`) and the writer swaps them whenever the
scene is `EXT.`, for action lines and sound cues alike: the sill becomes a
gutter, the floor becomes the ground, the fridge cutting out becomes a
streetlight buzzing and stopping. Lines that use a room only as a *simile* —
"The woods go quiet the way a room goes quiet" — are deliberately left alone,
because they read correctly under an open sky.

**The director** (`js/film-reel.js`) turns the finished script into a *reel*:
every shot, how long it holds, what set it plays on, how the camera moves, what
is heard over it and how tense the moment is. It is plain data, so the entire
edit of a film can be checked in a test without a browser.

**The conductor** (`js/film-score.js`) turns a reel into the score request the
music is composed from — which of SONG FORGE's genres and moods this kind of
film is scored with, a tempo whose four-bar blocks land near the actual cuts, a
section per scene carrying that scene's energy and instruments, and the list of
level changes that ducks the music under every line. It is pure data in, pure
data out, so the musical shape of a film is checkable in Node the way its edit
already is.

**The film page loads SONG FORGE as a library.** `film/index.html` pulls in five
files from `../music/js/` — `theory.js`, `genres.js`, `synth.js`, `composer.js`
and `engine.js` — and SCRIPT FORGE calls `Composer.compose()` for a song built
to this film's length and sections, then plays it through
`Engine.Player({ context, destination })` into its own music bus. The two apps
share a repository and nothing else: there is no build step, no copy of the
music code inside `film/`, and if those files are missing or fail, the film
still plays and says so on the film tab.

**The artist** is three files now, split out of what used to be one: `js/film-art.js`
keeps the genre-and-hour palette and the film-stock grain and gate flicker;
`js/film-sets.js` draws the fifteen sets, each in its own back/mid/fore planes;
`js/film-figures.js` draws the characters as jointed figures and the insert
objects; `js/film-weather.js` picks and draws the air over everything else.
**The score** (`js/film-audio.js`) plays them — the composed music on one bus,
the cut hits, the pulse and the character voices on another. **The camera**
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
  js/story-seed.js    borrows a story from MADLIBS when your idea is thin (or you ask)
  js/screenplay.js    a premise      → scenes, elements, shots, runtime
  js/format.js        a script       → .fountain / .txt / .fdx / shot list
  js/film-reel.js     a script       → a reel: timed shots, framing, voices
  js/film-art.js      genre/hour palette, film-stock grain and gate flicker
  js/film-sets.js     15 sets, each drawn in back/mid/fore planes
  js/film-figures.js  jointed figures, poses, gesture, insert-shot objects
  js/film-weather.js  rain, dust, fog, shimmer, embers, haze
  js/film-score.js    a reel         → a score request + a ducking envelope
  js/film-audio.js    plays the score, the cuts and the character voices
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
