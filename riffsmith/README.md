# Riffsmith

**Build a riff in eight steps.** Pick a mood and Riffsmith writes a chord
progression, turns it into chords, breaks them into an arpeggio, then shapes
the loudness, note lengths, timing and range. Press **Roll** for a new riff
with the same settings, as often as you like, and **Save MIDI** to drag the one
you like into FL Studio or any other music program.

A riff generator in the spirit of a piano roll's "riff machine" tool, built
from scratch. Standalone: it is not part of MAZ ARCADE.

Open `index.html` in any browser. No build step, no server, no dependencies,
and nothing leaves your browser. Works on a phone or a computer, in light or
dark mode.

## Install it as an app

From the published site, Chrome and Edge show an **Install** button (in the
address bar, and on the page itself). Installed, Riffsmith opens in its own
window with its own icon and works with no internet. Installing needs the page
to come from a web address; opened straight from a file it works the same but
can't be installed.

## The eight steps

Each step is a tab. Every one can be switched off, re-rolled on its own, or
**kept** so Roll and Surprise me leave it alone. **Only up to this step** shows
and plays the riff as it stood part-way through.

| # | Step | What it does |
|---|---|---|
| 1 | **Progression** | Long notes, one per chord. Six moods (Happy, Chill, Dreamy, Sad, Dark, Epic), each with its own scale and progressions to pick from or let it choose. 1–16 bars, ½–2 bars per chord, and variation that swaps in other chords. |
| 2 | **Chords** | Triads, sevenths, ninths, suspended, power chords or a mix. Close, open or spread voicing. Smooth inversions (each chord moves as little as it can), random, or root position. Optional bass note. |
| 3 | **Arpeggio** | Up, down, up & down, down & up, outside in, random, or whole-chord stabs. ¼ to 1/32 notes, triplets, 1–4 octaves, gate (note length), rests, flip and alternate flip. |
| 4 | **Mirror** | Turn parts upside down and/or back to front: every 2nd bar, every 2nd pair of bars, the second half, or random bars. |
| 5 | **Levels** | Velocity, velocity variation, accents on the beat, and pan spread. |
| 6 | **Articulation** | Length multiplier, length variation, legato (notes join up), and ratchets (quick repeats). |
| 7 | **Groove** | Straight, swing (8ths or 16ths), shuffle, laid back or pushed, by amount, plus human feel. |
| 8 | **Fit** | Snap to any scale (or follow the mood), lowest and highest note, what to do with notes outside the range (move by octaves, squash, remove), octave shift. |

Above the steps: **Play** (loops, and keeps playing while you change things),
**Roll**, **Surprise me** (new settings too), **Back / Next** through your last
100 riffs, presets, key, tempo, six built-in sounds, and **Save MIDI**. Tap the
keyboard strip on the piano roll to hear a note.

Keys: **Space** play/stop · **D** roll · **Z / X** back and next.

## How it's built

| File | |
|---|---|
| `js/riff.js` | The engine: the eight steps, presets and the MIDI writer. Plain logic, no browser needed. Every step has its own seed, so the same settings always give the same riff and rolling one step changes only that step. |
| `js/synth.js` | Six small Web Audio instruments and a looping player that schedules ahead of the clock. |
| `js/app.js` | The page: controls, piano roll, history, saving. |
| `sw.js`, `manifest.webmanifest`, `icons/` | What makes it installable and work offline. |

## Tests

```sh
node riffsmith/tests/riff-logic.test.js
```

Plain Node, no dependencies. They check that every note stays in key and in
range across hundreds of random riffs, that each step does what its controls
say, and that the saved MIDI file reads back to exactly the same notes. They
run on every push that touches this folder (`.github/workflows/riffsmith.yml`).
