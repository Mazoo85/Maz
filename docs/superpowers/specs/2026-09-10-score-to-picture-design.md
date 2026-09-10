# Score to picture — SONG FORGE scores a SCRIPT FORGE film

**Date:** 2026-09-10
**Status:** approved, ready to plan
**Sub-project:** 1 of 3 in the combined movie maker (see *Where this sits*, below)

## The goal

A film made in SCRIPT FORGE currently plays over a four-note chord bed written
inside `film/js/film-audio.js`. It is a placeholder. SONG FORGE, in the same
repository, composes complete songs and renders them offline faster than real
time.

This joins them: every film gets a real composed score, fitted to its exact
length, whose sections turn over with the scenes and whose instruments follow
the tension the reel already tracks — sparse under the opening, full band at
the crisis, resolving at the end — ducked under the dialogue and baked into the
downloaded video.

## Where this sits

The user wants one movie maker built from four existing projects. That is four
subsystems, so it is decomposed and sequenced:

1. **Score to picture** — this spec.
2. **Better picture** — a renderer upgrade: characters that act, layered
   parallax sets, moving light and weather, richer camera language.
3. **Better stories** — MADLIBS becomes the story brain so films stop sharing
   one seven-beat spine.
4. **Maz Engine renders the reel** — a later engine milestone that reads the
   same reel data and renders it natively in 3D. Maz Engine is at M0 (window,
   clear screen, clean shutdown) and is a native app, so the browser stays the
   fast renderer and the engine becomes the high-quality one. This is the
   reason the reel stays plain data with no browser in it.

Explicitly **out of scope for this spec**: editing controls of any kind (the
user chose not to prioritise them), sound effects beyond the existing cut hits
and character voices, the renderer upgrade, the story brain, and any Maz Engine
work.

## Architecture

One new module plus small, optional additions to two existing ones. Every
existing app keeps working standalone and keeps its current tests.

```
your idea → premise → script → reel ──┐
                                       ├→ score request → SONG FORGE composes
                     scene tension ────┘                        ↓
                                               Engine.renderOffline (AudioBuffer)
                                                                ↓
                                             music bus (ducked under dialogue)
                                                                ↓
                              + effects bus (cut hits, character voices)
                                                                ↓
                                            speakers + the recorded video file
```

### New: `film/js/film-score.js` — the conductor

Pure logic. A reel goes in; a plain description comes out. No audio, no DOM, so
the whole musical shape of a film is testable in Node exactly as the edit
already is.

```js
FilmScore.request(reel) → {
  genre: 'cinematic',            // a SONG FORGE genre id
  mood: 'dark',                  // a SONG FORGE mood id
  seed: 1234,                    // derived from the film's seed
  seconds: 156.4,                // the film's exact duration
  bpm: 84,                       // chosen to land bars near the cuts
  sections: [                    // one or more per scene, in order
    { type: 'intro', bars: 8, energy: 0.15, scene: 1,
      parts: { pad: true, chords: true, bass: false,
               drums: false, arp: false, lead: false } },
    // ...one entry per section, covering the whole film
  ]
}

FilmScore.duckEnvelope(reel) → [ { t: 12.4, gain: 1 },
                                 { t: 12.65, gain: 0.35 }, ... ]
```

### Changed: `music/js/composer.js` — optional scoring inputs

`compose(opts)` gains three optional inputs. **Passing none of them leaves
behaviour identical**, so SONG FORGE the app and its test suite are untouched:

- `opts.seconds` — compose to a target duration instead of a `short/medium/long`
  preset. Bars are derived from the tempo and rounded **up** to a whole four-bar
  block, so the song is never shorter than the picture.
- `opts.sections` — use this section plan (`type`, `bars`, `energy`) instead of
  `planStructure()`'s pop-song pattern.
- `opts.parts` — use these instrument choices per section instead of
  `assignParts()`'s rolls.

### Changed: `film/js/film-audio.js` — two buses instead of a bed

The synthesised chord bed is removed. What remains splits in two:

- **music bus** — an `AudioBufferSourceNode` playing the rendered score, with
  the duck envelope applied as gain automation.
- **effects bus** — the existing cut hits and character voices, which become
  sound design over a real score rather than the whole soundtrack. Hit level
  drops to about 0.6 now that they are not carrying the film alone.

Both feed the existing master gain, which already goes to the speakers *and*
the `MediaStreamDestination` the recorder uses — so the score reaches the
downloaded file with no extra work.

## Fitting rules

### Genre and mood

| Film genre | SONG FORGE genre | Mood |
|---|---|---|
| Drama | `cinematic` | `chill` |
| Thriller | `cinematic` | `driving` |
| Horror | `ambient` | `dark` |
| Comedy | `lofi` | `uplifting` |
| Romance | `lofi` | `dreamy` |
| Sci-Fi | `synthwave` | `dreamy` |
| Mystery | `cinematic` | `dark` |
| Fantasy | `cinematic` | `dreamy` |
| Heist | `dnb` | `driving` |
| Western | `cinematic` | `chill` |

Every value must exist in SONG FORGE's own `GENRES` and `MOODS` tables; a test
asserts this against those tables rather than against a copy, so a typo cannot
ship and a renamed genre fails loudly.

### Length and tempo

The song is composed to the film's exact duration, rounded up to a whole
four-bar block. The tempo is chosen from within the genre's own range as the one
whose four-bar block boundaries land closest to the actual scene cuts, measured
as total absolute error across every cut. This costs nothing and tightens the
fit; where the genre range is narrow it simply picks the default.

### Sections

Each scene becomes one or more sections, minimum four bars, placed on the
nearest four-bar boundary to its cut, so the music turns over with the story.
Section types map from the beat the scene is built on:

| Beat | Section type |
|---|---|
| open | `intro` |
| spark, push | `verse` |
| turn | `bridge` |
| crisis | `chorus` |
| choice | `bridge` |
| after | `outro` |

The title card and the end card are shots rather than scenes. The title card is
covered by the first section and the end card by the last, which is what makes
the music start on the title and finish on THE END rather than around them.

### Instruments follow the tension

The reel already rates every beat from 0.15 (ordinary) to 0.88 (crisis). That
number becomes the section's `energy`, and picks the band:

| Tension | Instruments |
|---|---|
| under 0.3 | pad, chords |
| 0.3 – 0.5 | + bass |
| 0.5 – 0.7 | + drums |
| over 0.7 | + lead, arp — the full band, only at the crisis |

The **final scene always resolves to pad and chords**, overriding its own
tension. This matters most in a three-scene micro film, where the last beat is
the choice at 0.5 and would otherwise end on drums; a film should land, not stop.

This rule is the difference between a song playing under a film and a score.

### Determinism

The score's seed derives from the film's seed, so the same idea gives the same
film *and* the same music, every time — consistent with everything else in the
app.

## Mixing

The duck envelope comes straight from the reel, which knows exactly when each
line is spoken: the music bus ramps to about a third over a quarter-second
before a line, and back to full after it. It is a sorted list of time and level
points, computed with no audio involved and tested as data.

## Lifecycle and failure

The score is composed and rendered **before** the film plays, with a brief
**"Scoring…"** state on the film tab, and cached against the film's seed so
play, replay and record all reuse one render. Recording is unaffected in
duration: the render happens before it starts.

Music must never stop a film being watched. If the browser has no
`OfflineAudioContext`, or composing or rendering throws, or the render runs past
**15 seconds**, the film falls back to the existing simple bed and the note on
the film tab says so plainly — the same posture as the video-format warning. A
slow phone gets a film with modest music, not a hang.

The cache is invalidated when the script changes (new seed, new length, new
idea).

## Testing

**Logic (`film/tests/film-logic.test.js`, Node, no browser):**

- every film genre maps to a genre and mood that exist in SONG FORGE's tables
- the section plan covers the whole film with no gaps or overlaps, nothing
  shorter than four bars, and totals at least the film's duration
- the crisis scene is the highest-energy section; the closing scene is lower
  than the crisis; no drums below 0.5; the full band only above 0.7
- the duck envelope has one duck per spoken line, levels within 0 and 1, returns
  to full between lines, and is sorted by time
- tempo choice: quantisation error is no worse than the genre's default tempo
  would produce
- the same film produces an identical score request every time

**Browser (`film/tests/film-browser.test.js`):**

- a film plays with a rendered score whose buffer is at least as long as the
  picture
- the recorded file carries audio (already checked; the assertion stays)
- with `OfflineAudioContext` stubbed out in the page, the film still plays and
  the note explains the fallback

**SONG FORGE's own suites must still pass unchanged**, proving the new options
are genuinely optional.

## Risks

- **Render time is unmeasured.** Two and a half minutes of offline audio may
  take longer than is comfortable at press-play. Mitigations in the design are
  the cache and the 15-second timeout; if it proves slow, the fix is to start
  rendering when the script is written rather than when play is pressed, since
  the user is reading the script by then. **Measure this first, before building
  anything else.**
- **Section boundaries are quantised**, so a cut can miss its musical change by
  up to a bar. The tempo search reduces this; it does not eliminate it. Accepted.
- **Micro films are short.** A three-scene film must still produce a valid song;
  the minimum bar count guards this and a test covers the shortest case.

## Build order

1. Measure `Engine.renderOffline` time for a 2–3 minute song. Decide render
   timing on the result.
2. `film-score.js` with its logic tests: request, sections, energy, duck
   envelope, tempo choice.
3. The optional `seconds` / `sections` / `parts` inputs in `composer.js`, with
   SONG FORGE's suites proving nothing changed by default.
4. Rewire `film-audio.js` to the two buses; delete the chord bed.
5. The "Scoring…" state, the cache, the timeout and the fallback note.
6. Browser tests, including the stubbed-out fallback path.
7. Update `film/README.md` and the hub blurb.
