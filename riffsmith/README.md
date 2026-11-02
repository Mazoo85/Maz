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
| 3 | **Arpeggio** | Up, down, up & down, down & up, outside in, random, or whole-chord stabs. **Rhythm** chops: even, off-beats, dotted, syncopated 3-3-2, bounce, gallop, trap rolls, or random chops. Speed (¼ to 1/32 or *Follow Snap*), triplets, 1–4 octaves, gate (note length), rests, flip and alternate flip. Chord stabs + a rhythm = chopped chords. |
| 4 | **Mirror** | Turn parts upside down and/or back to front: every 2nd bar, every 2nd pair of bars, the second half, or random bars. |
| 5 | **Levels** | Velocity, velocity variation, accents on the beat, pan spread — and per-note sound shaping: brightness (a filter), brightness variation, resonance, and pitch wobble. |
| 6 | **Articulation** | Length multiplier, length variation, legato (notes join up), and ratchets (quick repeats). |
| 7 | **Groove** | Straight, swing (8ths or 16ths), shuffle, MPC swing, wonky (late 16ths), triplet feel, laid back, pushed, or drunk — by amount, plus human feel. |
| 8 | **Fit** | Snap to any scale (or follow the mood), lowest and highest note, what to do with notes outside the range (move by octaves, squash, remove), octave shift. |

Tick **Show before this step** to see the riff as it was before the step
you're on, as faded outlines behind the notes.

## Above the steps

**Play** (loops, and keeps playing while you change things), **Roll**,
**Surprise me** (new settings too), **Back / Next** through your last 100
riffs, presets, key, tempo, **Snap**, sound, and **Save MIDI**.

**Sounds:** pluck, keys, soft pad, strings, organ, marimba, bell, lead,
chiptune, bass, 808 bass — or **your own sample**: load any audio file and the
riff plays it, repitched (it's treated as sounding C4). A sample lasts until
you close the page.

**My presets:** save the current settings as your own preset, delete one,
and export them all to a file (or import a file) to keep them safe or move them
to another computer.

## Editing notes

The piano roll is editable, on the Snap grid:

- **Draw** (the default): click an empty spot to add a note, drag a note to
  move it, drag its right end to change its length.
- **Erase**, or **right-click** with either tool: remove a note.
- **The strip along the bottom** is velocity (how hard each note is hit):
  drag across it to set it.
- Tap the keyboard on the left to hear a note.

Once you edit, the riff is yours: rolling or changing a setting makes a fresh
one, and **Back** brings your edits back. Edits are saved with the MIDI file
and remembered by the browser.

Keys: **Space** play/stop · **D** roll · **Z / X** back and next ·
**P / E** draw and erase.

Brightness, resonance and pitch wobble shape Riffsmith's own sounds; a MIDI
file carries the notes, their timing, length and velocity.

## How it's built

| File | |
|---|---|
| `js/riff.js` | The engine: the eight steps, presets and the MIDI writer. Plain logic, no browser needed. Every step has its own seed, so the same settings always give the same riff and rolling one step changes only that step. |
| `js/synth.js` | Eleven small Web Audio instruments, the sample player, and a looping player that schedules ahead of the clock. |
| `js/app.js` | The page: controls, the editable piano roll, history, your presets, saving. |
| `sw.js`, `manifest.webmanifest`, `icons/` | What makes it installable and work offline. |

## Tests

```sh
node riffsmith/tests/riff-logic.test.js
```

Plain Node, no dependencies. They check that every note stays in key and in
range across hundreds of random riffs, that each step does what its controls
say (rhythm chops, snap, grooves, per-note shaping), that hand-edited notes
are tidied safely, that preset files round-trip, and that the saved MIDI file reads back to exactly the same notes. They
run on every push that touches this folder (`.github/workflows/riffsmith.yml`).
