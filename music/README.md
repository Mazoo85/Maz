# SONG FORGE

A generative music maker that **writes and plays complete songs** in your browser.

Pick a genre and a mood, press one button, and it composes a track — chord
progression, bassline, drums, arpeggio and melody — arranged into an intro,
verses, a chorus, a bridge and an outro, then synthesises the whole thing live.

No install, no account, no API key, no internet connection required.

**The easiest way to use it: [`music/songforge.html`](songforge.html)** — the whole
app as one file. Download it, double-click it, and it runs. No server, no
install, and it keeps working with the wifi off. Save it to a phone and it
opens there too.

```
open music/songforge.html      # the single-file app — double-click it
open music/index.html          # the same app, unbundled, for development
python3 -m http.server         # or serve the folder: http://localhost:8000/music/
```

---

## What it does

- **18 genres** — Lo-Fi Chill, Synthwave, Deep House, Ambient, Cinematic,
  Chiptune, Drum & Bass, Trap, Rock, Funk, Jazz, Disco, Techno, Drill,
  Afrobeat, Bossa Nova, Gospel Soul and Country.
- **5 time signatures** — 4/4, plus **3/4** waltz time, **6/8**, **5/4** and
  **7/8**. Pick one or let the style choose: a waltz country song, a 5/4
  cinematic piece, a 7/8 prog-rock riff. The bar length runs through the whole
  program — the composer, the piano-roll grid, the arrangement and the MIDI
  export all measure in bars of the song's own meter.
- **5 moods** — Chill, Uplifting, Dark, Dreamy, Driving. A mood bends the genre:
  brighter or darker scales, busier or sparser parts, more or less air.
- **Real arrangement** — sections have different energy, so the chorus is
  fuller than the verse, the intro holds back, and fills land at section ends.
- **Edit every note by hand.** A piano roll for the melodic parts and a step
  grid for the drums. Draw, drag, lengthen, erase. Rows in the song's key are
  lit, and with *In key* on, anything you draw fits.
- **Develop my idea** — draw a few notes, press it, and the generator reads
  your motif and builds the whole part around it: repeating it, answering it,
  turning it upside down. This is the loop that makes the two halves one
  program rather than two.
- **Re-roll any single part.** Don't like the melody? Rewrite just the melody
  over the same chords. Everything else stays exactly as it was.
- **Lock a part** you like, and a re-roll of everything writes around it.
- **36 instruments** — guitar, harp, nylon, marimba, vibraphone, organ, brass,
  reed, choir, voice, FM keys, supersaw, 808 and more — swappable on any part,
  plus a 16-piece kit with ride, tambourine, cowbell and conga, in **13 kits** —
  from a jazz kit played with brushes to a 909, an acoustic rock kit and a set
  of Latin hand percussion.
- **Undo** everything, including a re-roll or a cleared part (Ctrl+Z).
- **Hear notes as you draw them**, and **swap the instrument** on any part.
- **Change the song you have** — slide the tempo, move the key up or down a
  semitone, set the volume. The score is beats and pitches, so retiming and
  transposing keep the song and change nothing else. No need to throw away a
  track you liked and roll the dice again.
- **Swap any chord.** Tap a chord in the strip and pick a different one. Every
  part re-pitches onto the new chord — bass to the new root, arp and pad to the
  new tones, and only the melody notes that were sitting on a chord tone move —
  but nothing changes rhythm. The groove you had is the groove you keep.
- **Arrange the song by hand.** Every section is a card: duplicate it, delete
  it, or shove it left and right. Want two choruses back to back, or the bridge
  moved earlier? Drag the form around and the whole track re-times behind you.
  Ctrl+Z puts it back.
- **Mixer** — mute, solo or rebalance drums, bass, chords, arp, lead and pad.
- **An effects rack on every part** — reverb, delay, **chorus**, **bit crush**
  and a three-band **EQ** (bass, mids, treble), set separately for each of the
  six parts. Push the pad far back and keep the lead dry and up front; thicken
  the chords with chorus; wreck the drums with crush; carve the bass out of the
  way of the kick. One button puts a part back to plain.
- **Automation — the one thing that moves.** Every other setting holds for the
  whole song; here you draw a line across the track and the mix follows it. Two
  lanes: a **filter sweep** over the whole mix and a **volume fade**. Click to
  add a point, drag it, right-click or double-tap to remove it. Four one-tap
  shapes write the usual moves for you — *build into the chorus*, *open up on
  choruses*, *fade in*, *fade out* — and they read the song's own arrangement,
  so the build ends exactly where your chorus actually starts.
- **Ping-pong echo** — one toggle throws the delays out to the left and right
  instead of leaving them in the middle.
- **Export** — the finished track as a **.wav**, the notes as a **.mid**, or
  **stems**: every part as its own audio file, to mix by hand in GarageBand,
  Ableton, FL Studio, Logic or MuseScore.
- **Seeds** — every song has a short code like `VELVET-7318`. The same seed and
  settings always produce the same song, so *Copy link* hands someone the exact
  track you are hearing.
- **Saved songs keep the song, not the recipe.** Save and you get back exactly
  what you had — every note you drew, the chord you swapped, the sections you
  moved, the fade you added, the effects on each part, what you had muted. Kept
  on your own device in browser storage; nothing is uploaded.

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
3. **Meter.** The genre proposes a time signature and you can override it. A
   beat is a quarter note and a step a sixteenth in every meter; what changes is
   how many make a bar and — the part that matters musically — where the weight
   falls inside it. The drum patterns are written on a sixteen-step 4/4 bar and
   rewritten for other meters by role rather than truncated: hats keep their
   pulse, snares move onto the new bar's backbeats, and anything anchored to a
   downbeat is restated at each accent, so 7/8 comes out as 2+2+3 rather than
   seven even eighths.
4. **Harmony.** A progression is chosen as *scale degrees*, so the same numbers
   read as I–vi–IV–V in major and i–VI–iv–v in minor. Chords are checked for
   self-contradiction (a flat and natural fifth at once, a flat ninth grinding
   against the root) and rebuilt from a simpler shape if they fail. Bare
   diminished triads gain a seventh; augmented triads lose their sharp fifth.
   Voicings use **voice leading** — each chord moves as little as possible from
   the one before it, which is why the chord track doesn't leap around.
5. **Parts.** The bass follows the chord roots in a genre-specific pattern
   (walking, offbeat, driving eighths, sliding 808s). Drums come from
   sixteenth-step patterns per energy level, with fills at section boundaries.
   The arpeggio runs the chord tones. 
6. **Melody.** This is the part that makes it sound *written* rather than
   random: the composer invents a two-bar **motif** — a rhythm plus a contour —
   and then varies it. Phrases repeat it, transpose it, invert it, or keep the
   opening and change the ending (the classic question-and-answer shape). Notes
   on strong beats snap to chord tones; notes in between are free to pass
   through the scale.
7. **Phrasing.** Every second phrase closes: the last beat is cleared and the
   final note leans onto a chord tone and holds. A melody that never stops for
   breath reads as a stream of notes rather than a line.
8. **Arrangement.** Where a style calls for it, the bar before a chorus empties
   out — the kit stops, a snare roll climbs, a riser sweeps — and the chorus
   lands on an impact. Taking things away is what makes the next bar hit.
9. **Performance.** Swing pushes offbeats late, and every note is nudged a few
   milliseconds and a little louder or quieter, so nothing sits perfectly on the
   grid.

Then `synth.js` builds every sound from scratch — **36 instruments and 16 drum
pieces, no audio files anywhere in this app**. A filter can only take away what
the oscillator already has, so rather than one sawtooth wearing different
filters, there are five ways of making a sound:

| Model | What it gives |
|---|---|
| Subtractive | Saw/square/triangle stacks through a filter — synth bass, pads, saw leads, supersaw |
| Custom waveforms | Harmonic recipes turned into oscillator shapes — organ drawbars, brass, reeds, guitar, glass |
| FM | Two operators with a falling modulation index — bells, FM keys, FM bass, metallic leads |
| Formant | Fixed vowel resonances over a saw pair — choir and voice |
| Mallet | Inharmonic partials near 1, 4 and 10 — marimba and vibraphone (which is *why* they sound wooden and not like a sine) |

Each style also keeps a list of alternate instruments that suit it, and draws
from them per song — so two lo-fi tracks are not the same four sounds twice.

`engine.js` mixes it: each part runs through its own bit crusher and three-band
EQ, has a place in the stereo field, feeds the reverb, delay and chorus buses by
its own adjustable amount (a muted part feeds them nothing, so muting really is
silence), velocity opens filters as well as raising level, pads drift under slow
LFOs, and — where the style calls for it — **the kick ducks the sustained
parts**. Web Audio
compressors have no sidechain input, but the kick times are already in the
score, so the duck is scheduled as gain automation exactly where the kick lands.
That pump is most of what makes house, synthwave and trap sound like themselves;
ambient and chiptune have none.

A few of those effects are worth saying how they are actually built, because in
each case the trick *is* the sound:

| Effect | How |
|---|---|
| **Chorus** | Two short delay lines whose delay times wobble under slow LFOs, panned apart. A copy arriving a few milliseconds late and drifting in pitch is what "thick" means — it is a second player who cannot be perfectly in time or in tune. |
| **Bit crush** | A waveshaper that rounds every level to one of `2^bits` steps, from 16 bits down to 2. It is deliberately **not** oversampled: oversampling exists to suppress the aliasing a hard curve creates, and here that aliasing is the entire point. |
| **Ping-pong** | The input hits a left delay line, left feeds right, right feeds left again, each hard-panned — so one note walks across the room and back. Both this and the centred delay are built every time and only one is fed, so the switch is a gain change and the echoes already in the air ring out instead of being cut off. |
| **Automation** | Points in beats, ramped onto a master lowpass (before the limiter, so a sweep is still caught by the ceiling) and a master fader (after it, because a fade to silence is not something to limit back up). The same function writes one pass for live playback and one for the offline render, so an export sounds like what you heard. |

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
music/js/editor.js    # the piano roll and drum grid you draw on
music/js/automation.js # the lane you draw effect moves on
music/js/app.js       # interface, transport, mixer, note timeline

music/build-standalone.js  # folds all of the above into one file
music/songforge.html       # the built single-file app (generated — do not edit)
```

Edit the files in `js/` and `css/`, then run `node music/build-standalone.js`
to regenerate `songforge.html`. CI fails if the built file has drifted from its
sources, so the one you hand someone is always the one in the repo.

`composer.js` never makes a sound and `synth.js` never makes a decision — the
score is plain data (`{ t, d, p, v }` in beats), which is why the same code path
can play live, render an export, and write a MIDI file.

A save is that same score, written out. `Composer.packSong` drops everything a
fresh compose can rebuild — the genre and mood are whole objects of settings,
restored from their names on the way back in — and writes each note as a bare
array of numbers at the precision a note actually needs: a ten-thousandth of a
beat, which is half a millisecond at 120 BPM. That turns roughly 85 KB of JSON
per song into roughly 35, so thirty songs fit comfortably in browser storage.
Saves made before this existed still load; they are labelled *seed only*, because
all they can give you back is the original generated version.

The editor writes into those same arrays. A note you draw is indistinguishable
from a note the composer wrote, so it plays, renders and exports with no special
handling — and `Composer.motifFromEvents` can read your notes back out as a
motif and develop them. That is the whole merge: one score, written from both
ends.

## Tests

Two suites live in `music/tests/`:

```
node music/tests/music-logic.test.js       # 120 songs: structure, harmony, determinism, MIDI

npm --prefix music/tests install           # browser suite needs Playwright
npx --prefix music/tests playwright install chromium
node music/tests/music-browser.test.js     # real browser: UI, playback, audio, exports
node music/tests/standalone.test.js       # the built single file, opened from disk
```

The standalone suite saves a song with a hand-drawn note, a fade, a ping-pong
echo and per-part effects, throws it away by composing a new one, loads it back
and checks every part returns note for note — then plays it, to prove what came
back is a working song and not just data. The logic suite does the same round
trip in the abstract, over a song deliberately edited away from anything its
seed would produce.

The standalone suite also covers the editor: it clicks the grid to draw a note,
checks the note lands in key, erases it again, taps the drum grid, feeds a
hand-drawn motif through *Develop my idea* and checks the shape survives, and
confirms a locked part outlives a re-roll of everything. It drives the whole
effects rack and checks each control reaches its own part and no other, draws
and undoes automation points, and flips the ping-pong toggle. It also swaps a chord
and checks the bass rhythm under it is untouched, duplicates, moves and deletes
sections and checks the form and note positions follow, and moves a part's
reverb send and checks the slider still shows the right value after switching
parts and back.

The logic suite checks every genre × mood × length combination for gapless
harmony, in-range pitches, chords that don't contradict themselves, and that a
seed always reproduces the same song.

The standalone suite opens `songforge.html` over `file://` — no server, exactly
how a person would — because inlining is the kind of step that breaks a page
quietly: a lost script, a stray closing tag, a stylesheet that never applied.

The browser suite renders every genre offline and *measures the waveform* —
peak, RMS and crest factor — so problems you can only hear cannot pass
silently. It measures the effects the same way rather than trusting them: a filter sweep
has to start measurably duller than the same music unswept, a fade has to
actually reach silence, chorus has to widen a deliberately mono part, ping-pong
has to push the echoes off centre, and crushing has to add high-frequency grit
that was not there before.

It has already caught five real defects: a modal that covered the
page invisibly, a master bus that clipped on every genre, an instrument
saturation curve with 20 dB of hidden gain that flattened the mix, arrange
and send controls too small to hit with a thumb on a phone, and a one-tap build
that gave up on any song whose chorus came first.
