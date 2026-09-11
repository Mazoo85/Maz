<!-- Part of MAZ ARCADE — see the repo root README for every project. -->

# SONG FORGE

A generative music maker that **writes and plays complete songs** in your browser.

Pick a genre and a mood, press one button, and it composes a track — chord
progression, bassline, drums, arpeggio and melody — arranged into an intro,
verses, a chorus, a bridge and an outro, then synthesises the whole thing live.

No install, no account, no API key, no internet connection required.

```
open music/index.html          # double-click it, or:
python3 -m http.server         # then visit http://localhost:8000/music/
```

---

## What it does

- **8 genres** — Lo-Fi Chill, Synthwave, Deep House, Ambient, Cinematic,
  Chiptune, Drum & Bass, Trap.
- **5 moods** — Chill, Uplifting, Dark, Dreamy, Driving. A mood bends the genre:
  brighter or darker scales, busier or sparser parts, more or less air.
- **Real arrangement** — sections have different energy, so the chorus is
  fuller than the verse, the intro holds back, and fills land at section ends.
- **Re-roll any single part.** Don't like the melody? Rewrite just the melody
  over the same chords. Everything else stays exactly as it was.
- **Mixer** — mute or rebalance drums, bass, chords, arp, lead and pad.
- **Export** — download the finished track as a **.wav**, or the notes as a
  **.mid** you can open in GarageBand, Ableton, FL Studio, Logic or MuseScore.
- **Seeds** — every song has a short code like `VELVET-7318`. The same seed and
  settings always produce the same song, so *Copy link* hands someone the exact
  track you are hearing.
- **Saved songs** are kept on your own device (browser storage). Nothing is uploaded.

Works on a phone or a desktop. **Space** plays and pauses, **G** composes a new one.

---

## How the "AI" actually works

It is a **music-theory-driven generative composer** — the rules a songwriter
uses, encoded and then explored by a seeded random number generator. It runs
entirely on your machine, and it costs nothing to run.

Each song is built in this order:

1. **Key and scale.** The genre proposes candidate scales, the mood weights
   them — Dark pulls toward Phrygian and minor, Uplifting toward major and
   Lydian.
2. **Structure.** Sections are laid out to hit the requested length, each with
   an energy level: intro `0.35`, verse `0.65`, bridge `0.5`, chorus `1.0`.
   Energy decides which drum pattern plays, whether the lead sings, how open the
   filters are, and how hard the notes hit.
3. **Harmony.** A progression is chosen as *scale degrees*, so the same numbers
   read as I–vi–IV–V in major and i–VI–iv–v in minor. Chords are checked for
   self-contradiction (a flat and natural fifth at once, a flat ninth grinding
   against the root) and rebuilt from a simpler shape if they fail. Bare
   diminished triads gain a seventh; augmented triads lose their sharp fifth.
   Voicings use **voice leading** — each chord moves as little as possible from
   the one before it, which is why the chord track doesn't leap around.
4. **Parts.** The bass follows the chord roots in a genre-specific pattern
   (walking, offbeat, driving eighths, sliding 808s). Drums come from
   sixteenth-step patterns per energy level, with fills at section boundaries.
   The arpeggio runs the chord tones. 
5. **Melody.** This is the part that makes it sound *written* rather than
   random: the composer invents a two-bar **motif** — a rhythm plus a contour —
   and then varies it. Phrases repeat it, transpose it, invert it, or keep the
   opening and change the ending (the classic question-and-answer shape). Notes
   on strong beats snap to chord tones; notes in between are free to pass
   through the scale.
6. **Performance.** Swing pushes offbeats late, and every note is nudged a few
   milliseconds and a little louder or quieter, so nothing sits perfectly on the
   grid.

Then `synth.js` builds every sound from oscillators and filtered noise —
subtractive synth voices, FM bells, Rhodes-style electric piano, 808s, and drum
kits made from pitch-swept sines and shaped noise. There are no audio files
anywhere in this app.

---

## Project layout

```
music/index.html      # the page
music/css/style.css   # neon dark, phone-first
music/js/theory.js    # seeded RNG, scales, chord building, voice leading
music/js/genres.js    # the taste: tempo, scales, progressions, drum patterns, synth presets
music/js/composer.js  # writes the song — structure, harmony, and every part
music/js/synth.js     # Web Audio instruments and drum kits, all synthesised
music/js/engine.js    # mixer graph, look-ahead scheduler, offline render
music/js/export.js    # .wav encoder and standard MIDI file writer
music/js/app.js       # interface, transport, mixer, note timeline
```

`composer.js` never makes a sound and `synth.js` never makes a decision — the
score is plain data (`{ t, d, p, v }` in beats), which is why the same code path
can play live, render an export, and write a MIDI file.

### SONG FORGE is also a library

[**SCRIPT FORGE**](../film/) scores its films with it. `film/index.html` loads
`theory.js`, `genres.js`, `synth.js`, `composer.js` and `engine.js` straight
from this folder and drives them from `film/js/film-score.js`, so two bits of
this API have a second consumer and should not change shape without a look next
door:

- **`Composer.compose()`'s optional `seconds`, `sections` and `parts`.** A film
  asks for a song of an exact length, with one section per scene, and with the
  instruments named per section — pad and chords under the opening, the full
  band only at the crisis — rather than letting the genre choose an arrangement.
- **`Engine.Player({ context, destination })`.** The film supplies its own
  `AudioContext` and plays the song into its own gain node, so the music can be
  ducked under the dialogue and recorded with the picture.

`music/tests/` does not cover that use; `node film/tests/film-logic.test.js` and
`node film/tests/film-browser.test.js` do.

## Tests

Two suites live in `music/tests/`:

```
node music/tests/music-logic.test.js       # 120 songs: structure, harmony, determinism, MIDI

npm --prefix music/tests install           # browser suite needs Playwright
npx --prefix music/tests playwright install chromium
node music/tests/music-browser.test.js     # real browser: UI, playback, audio, exports
```

The logic suite checks every genre × mood × length combination for gapless
harmony, in-range pitches, chords that don't contradict themselves, and that a
seed always reproduces the same song.

The browser suite renders every genre offline and *measures the waveform* —
peak, RMS and crest factor — so problems you can only hear cannot pass
silently. It has already caught three real defects: a modal that covered the
page invisibly, a master bus that clipped on every genre, and an instrument
saturation curve with 20 dB of hidden gain that flattened the mix.

---

← Back to the [**MAZ ARCADE hub**](../index.html) · [repository README](../README.md) · [play/open this one](../music/)
