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
open music/cjc-station.html    # the original CJC Music Station, standalone
python3 -m http.server         # or serve the folder: http://localhost:8000/music/
```

---

## What it does

- **19 genres** — Lo-Fi Chill, Synthwave, Deep House, Ambient, Cinematic,
  Chiptune, Drum & Bass, **Hip-Hop**, Trap, Rock, Funk, Jazz, Disco, Techno,
  Drill, Afrobeat, Bossa Nova, Gospel Soul and Country.
- **46 scales** — every mode of the major scale, every mode of melodic and
  harmonic minor worth playing, the bebop scales, the symmetric ones (whole
  tone, both diminished scales, augmented, chromatic), and the five- and
  six-note scales from the blues to hirajoshi. Grouped in the picker, because
  forty-six names in one list is a wall.
- **14 chord shapes** — power, triad, sus2, sus4, 6, 7, add9, 9, 6/9, 11, 13,
  shell voicings and quartal stacks, named the way a musician would write them
  (`Cmaj7`, `Am7♭5`, `C7sus4`, `G13`).
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
- **Riffs.** Where a style is built on repetition, the arpeggio part plays a
  *riff* instead: one figure, invented once, restated on every chord. An
  arpeggio changes shape whenever the harmony does, which is why it decorates;
  a riff keeps its rhythm and its intervals and moves bodily to each new root,
  and that repetition is the hook.
- **Simplify or fill in** any part with one button — thin it to its skeleton, or
  add detail between the notes that are already there.
- **Re-roll any single part.** Don't like the melody? Rewrite just the melody
  over the same chords. Everything else stays exactly as it was.
- **Lock a part** you like, and a re-roll of everything writes around it.
- **52 instruments** — guitar, harp, nylon, marimba, vibraphone, organ, brass,
  reed, choir, voice, FM keys, supersaw, 808, violin, cello, flute, clarinet,
  oboe, a string section, trombone, saxophone, muted trumpet, sitar, koto,
  kalimba, steel drum, accordion, banjo and steel-string guitar — swappable on
  any part,
  plus a 22-piece kit with ride, tambourine, cowbell, conga, **clave**,
  **woodblock**, **bongo**, **timbale**, **triangle** and **cabasa**, in **14 kits** —
  from a jazz kit played with brushes to a 909, an acoustic rock kit, a set of
  Latin hand percussion, and a **boom-bap** kit built to sound like a kit heard
  through a sampler.
- **The Station** — a looping groovebox on the same page: one bar or two on a
  grid of pads, going round. It arrives **already filled in with the song's own
  groove** rather than empty, because a step sequencer that starts blank has
  handed the job straight back to you. Click a pad to cycle it *off → on →
  accent*, place bass notes on the lower grid, then put the loop back into the
  song across every bar. It makes no sound of its own — a loop here is a very
  short song with looping switched on, handed to the same player everything
  else uses, so it comes out through the same kits, effects and mixer.
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
- **A seventh part: the answer.** A second melodic voice that plays *call and
  response* with the lead — it speaks in the gaps the lead leaves, moves the
  opposite way to the phrase it is answering, sits on the other side of the
  stereo field, and shuts up when the lead comes back in.
- **Mixer** — mute, solo or rebalance drums, bass, chords, arp, lead, answer
  and pad.
- **An effects rack on every part** — reverb, delay, **chorus**, **bit crush**,
  a **compressor**, a **transient shaper**, a **swirl** (flanger, phaser or
  rotary speaker), a **colour** (ring modulator, wave folder or wah), an
  **auto-pan sweep** and a three-band **EQ** (bass, mids,
  treble), set separately for each part — plus **glue** across the whole mix,
  which is what makes several parts sound like one performance. Push the pad far back and keep the lead dry and up front; thicken
  the chords with chorus; wreck the drums with crush; carve the bass out of the
  way of the kick. One button puts a part back to plain.
- **Automation — the one thing that moves.** Every other setting holds for the
  whole song; here you draw a line across the track and the mix follows it. Two
  lanes: a **filter sweep** over the whole mix and a **volume fade**. Click to
  add a point, drag it, right-click or double-tap to remove it. Four one-tap
  shapes write the usual moves for you — *build into the chorus*, *open up on
  choruses*, *fade in*, *fade out* — and they read the song's own arrangement,
  so the build ends exactly where your chorus actually starts.
- **Harmony lines and octave doubling** — give any melodic part a second voice
  a third above or a sixth below, or double it an octave lower. The interval is
  counted in *scale steps*, not semitones, so it widens and narrows to stay in
  the key — which is the whole difference between a harmony line and a detuned
  copy. These are real notes, so they come out in the MIDI export and undo like
  any other edit.
- **Loudness on export** — every WAV reports how loud it actually is in LUFS,
  the measure streaming services use, and you can ask for a target (−14 for
  streaming, −16 for podcasts, −9 for loud) and have the file normalised to it.
  Files are dithered, so quiet fades stay clean.
- **Split glue** — glue the mix with one compressor across everything, or with
  three across bass, middle and treble separately, which gets noticeably more
  level for the same amount of squeezing.
- **Pump and chop** — **Pump** sets how hard the kick ducks everything else and
  **Pump speed** how quickly it breathes back, the two controls that make house
  and synthwave sound like themselves. **Chop** cuts any part into a rhythmic
  pulse on a grid of quarters, eighths or sixteenths.
- **Ping-pong echo** — one toggle throws the delays out to the left and right
  instead of leaving them in the middle.
- **Three kinds of echo** — **digital** (clean repeats), **tape** (each repeat
  darker, softer and slightly out of tune, the way a real tape loop wobbles)
  and **multi-tap** (three taps bouncing across the stereo field with no
  feedback at all, a rhythm rather than a fade). All four lines are built every
  time and only one is fed, so switching flavour never cuts off the repeats
  already ringing out.
- **Four reverbs and an adjustable room** — room, **gated** (the eighties
  snare: a long tail with the end chopped off, which sounds nothing like a
  short one), **reverse** (it swells into the note instead of trailing away)
  and **shimmer** (an octave above the tail). Plus a size control, echo timing
  in musical divisions down to a dotted sixteenth, and how many times it
  repeats.
- **Master tone and imaging** — a three-band EQ across the whole mix, a
  **stereo width** control, and **mono bass**, which takes the stereo out of
  the low end so it stays solid on a big system without narrowing anything
  above it.
- **Export** — the finished track as a **.wav**, the notes as a **.mid**, or
  **stems**: every part as its own audio file, to mix by hand in GarageBand,
  Ableton, FL Studio, Logic or MuseScore.
- **Feel** — eight grooves (straight, light, swung, hard, MPC, drunk, pushed,
  laid back) that change how the song is *played* rather than what it plays, so
  you can move the groove while you listen. Plus a **looseness** control for how
  far notes drift off the grid when they are written.
- **A click track** with a bar counted in, for playing along.
- **Half time** — where a style calls for it, a chorus or bridge drops to half
  speed under music that has not slowed down.
- **Choose the scale, the chord rate and the time signature** yourself, or
  leave any of them to the style.
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
4. **Key changes.** Where a style calls for it, the last chorus lifts a
   semitone or a tone and stays there. It is a `keyShift` on the section rather
   than a rewrite, so transposing the whole song still works and moving that
   section moves the modulation with it — and the melody follows, because the
   lead snaps to the key in force where it is rather than the one the song
   opened in.
5. **Harmony.** A progression is chosen as *scale degrees*, so the same numbers
   read as I–vi–IV–V in major and i–VI–iv–v in minor. Chords are checked for
   self-contradiction (a flat and natural fifth at once, a flat ninth grinding
   against the root) and rebuilt from a simpler shape if they fail. Bare
   diminished triads gain a seventh; augmented triads lose their sharp fifth.
   **Turnarounds** hinge the phrase: the last bar of a four-bar phrase gives way
   to the dominant and hands you back to the top, so a section is not the same
   few chords repeated. Only a chord shorter than the phrase is a candidate —
   splitting the chord that *is* the phrase would mean nobody who asks for one
   change every four bars ever gets one.
   Voicings use **voice leading** — each chord moves as little as possible from
   the one before it, which is why the chord track doesn't leap around.
   Three things stop a progression sounding like a loop. **Secondary dominants**
   are borrowed from outside the key — the major-with-a-flat-seventh chord a
   fifth above wherever the music is going, which is the strongest pull in tonal
   music and cannot be built by stacking scale degrees. **Inversions** put the
   third or fifth in the bass when that moves less than the root would, so the
   bass walks under held harmony instead of jumping (and the chord is named
   `C/E` accordingly). And **cadences** choose the last chord of each section
   for where the music is going: a chorus or an ending lands on the tonic, a
   verse or a bridge stops on the dominant and leans forward.
6. **Parts.** The bass follows the chord roots in a genre-specific pattern
   (walking, offbeat, driving eighths, sliding 808s). Drums come from
   sixteenth-step patterns per energy level, with fills at section boundaries.
   The arpeggio runs the chord tones. 
7. **The answer.** Where a style calls for it, a second voice answers the lead.
   It is composed last and reads the lead directly, because there is no way to
   write call and response without knowing what the call was. It fills gaps
   rather than doubling — two voices moving in parallel read as one thicker
   voice, so it moves in contrary motion to the phrase before it and lands its
   last note on a chord tone, like a small cadence of its own. A long held lead
   note counts as a gap: the lead sitting still is exactly when a second voice
   moving underneath is audible as a second voice.
8. **Melody.** The chorus quotes the verse rather than starting again: the
   rhythm is what the ear recognises, so it survives, and the contour is what
   makes it a different phrase, so that is what moves. An unrelated chorus reads
   as a different track spliced in. This is the part that makes it sound
   *written* rather than
   random: the composer invents a two-bar **motif** — a rhythm plus a contour —
   and then varies it. Phrases repeat it, transpose it, invert it, or keep the
   opening and change the ending (the classic question-and-answer shape). Notes
   on strong beats snap to chord tones; notes in between are free to pass
   through the scale.
9. **Phrasing.** Every phrase gets a breath and every second one a full close:
   the tail is cleared and the final note leans onto a chord tone and holds. A
   melody that never stops for breath reads as a stream of notes rather than a
   line — and the silence is also what leaves the second voice somewhere to
   answer.
10. **Arrangement.** Where a style calls for it, the bar before a chorus empties
   out — the kit stops, a snare roll climbs, a riser sweeps — and the chorus
   lands on an impact. Taking things away is what makes the next bar hit.
11. **Performance.** Two different things, deliberately kept apart. *Humanising*
   is part of the written performance — a few thousandths of a beat and a little
   louder or quieter, decided by the part's own seed — so it lives in the score
   and changes when a part is re-rolled. *Swing* is not: it is how the score is
   played, the same notes pushed late on the offbeats, so it is applied at
   playback by the scheduler, the offline render and the MIDI writer alike.
   That is what makes the groove something you can move while you listen instead
   of something that needs a part rewritten — and it means what you hear, what
   you export and what you save all swing identically.

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
| Mallet | Inharmonic partials near 1, 4 and 10 — marimba, vibraphone, kalimba and steel drum (which is *why* they sound wooden and not like a sine) |
| Physical model | A plucked string simulated rather than imitated — sitar, koto, banjo, steel-string guitar |

The physical model is worth a word, because it is the only one where the timbre
is not specified anywhere. Karplus-Strong fills a buffer with a burst of noise
one wavelength long, then makes every later sample the average of the two a
wavelength earlier, scaled just under 1. The noise is the pluck, the wavelength
is the string's round trip, and the averaging is the energy the high harmonics
lose on each trip — which is why the sound turns from bright to mellow as it
rings, and why low notes ring longer than high ones, without either being
written down. It is computed directly rather than built as a delay line feeding
back through a filter, which is the textbook Web Audio arrangement and does not
work: a BiquadFilterNode inside a feedback cycle is unstable in this engine, and
was measured growing to 10³⁴ at a feedback of 0.9 while the identical loop
without the filter behaved perfectly.

Two things stop any of it sounding sequenced. **Velocity layers**: a note hit
hard picks up saturation and one played softly is rolled off, because level
alone is not how an instrument gets louder. **Round robin**: every note is
detuned by a few cents derived from its own start time, so no two consecutive
notes are identical — deterministically, so the same song always sounds the
same.

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
| **Colour** | Three ways to make a part sound wrong on purpose, sharing one slider. **Ring** multiplies the part by a tone — Web Audio has no multiplier, but a gain node *is* one, since its gain is an audio-rate parameter you can drive with an oscillator, and the result is the clangorous sum-and-difference tones of a ring modulator. **Fold** drives the signal past full scale and folds it back rather than clipping it, which adds harmonics that were never in the original instead of merely squaring off the ones that were. **Wah** is a narrow resonant band swept across the part. All three are built every time and blended dry-to-wet, so the slider is genuinely silent at zero and moves while the song is playing; only switching kind rebuilds. |
| **The original CJC Station** | [`cjc-station.html`](cjc-station.html) is the groovebox as it was before it moved in here: its own page, its own look, its own audio engine, one file with nothing to install. It is kept because the built-in Station is a *different thing built on the same idea* — Song Forge's Station borrows the layout and the accents but plays through this app's kits and effects and is filled in by the composer, where the original starts from a demo groove and waits for you. One bug was fixed on the way in: the drum names never rendered, for the specificity reason recorded in `style.css`. |
| **One audio engine** | The Station could have been a second sequencer with its own oscillators — that is how it began life as a separate app. Instead a loop *is* a song: the pads are turned into a four-beat song object with looping on and handed to the existing player. Every drum kit, every effect, the mixer and the whole master chain come along for free, and there is no second engine to keep in step with the first. Leaving the Station reloads the real song, and the suite checks the song is untouched by the visit. |
| **A note ladder in the song's key** | The Station's note grid is eight rungs of *the song's own scale from its own root*, not a fixed pentatonic. A fixed ladder is simpler and sounds wrong the moment the song is in anything but a minor key; reading the scale means a note placed on the grid belongs with everything else in the app, and changing the song's key moves the ladder with it. |
| **Append-only drum names** | A saved song stores each drum as its index in one list, so the order of that list *is* a file format. `rim` was missing from it for the program's whole life: `indexOf` returned −1, and every sidestick in every saved song came back as a kick drum. Seven genres write sidestick patterns and all of them were quietly rewritten on save, with nothing to complain about it. The list is now append-only by rule, the first fifteen slots are pinned by a test, and a second test round-trips a song carrying one note of every drum any genre asks for. |
| **The filter follows the keyboard** | One cutoff for a whole instrument is the clearest way a synthesised sound gives itself away. A low note's harmonics sit far below the filter and pass untouched; a high note has most of its own above it and loses them, so the top octave comes out dull and thin while the bottom comes out muddy. Measured across four octaves, the bass preset was giving up **60% of its harmonic content** by the top of its range and the pad 48%. The cutoff now rises with the note at 0.6 of the pitch change — not 1.0, because holding the tone perfectly even is not what real instruments do either; a piano and a voice both darken a little as they climb. The falloff drops to 37% and 29%. |
| **Oscillators that do not start together** | Web Audio oscillators always begin at phase zero, so a stack of them started on the same instant is perfectly aligned — they rise as one coherent surge and the attack spikes. Each oscillator now starts a fraction of a millisecond early, by an amount derived from the note's own start time, which leaves each at a different point in its cycle by the time the note is audible. The envelope is silent through the whole offset, so nothing moves in time, and the same song still renders identically twice. On the seven-oscillator supersaw the attack peak drops **40%** for the same sustained loudness, and on its worst note 43%. |
| **Drums that start from silence** | Every drum's envelope now takes a millisecond and a half to reach full level instead of arriving there between one sample and the next. A step like that contains every frequency there is, including all the ones above half the sample rate, which fold back down as a thin metallic edge on top of the hit. Measured as the loudness of the very first sample: the kick used to start a third of the way up full scale (0.318) and now starts at 0.008 — thirty-nine times quieter. |
| **A snare with two layers** | A snare is the stick on the head — bright, gone in a few hundredths of a second — *plus* the wire snares rattling underneath, which are lower and last far longer. One band of noise cannot be both at once, and the old single-band snare measurably got **brighter as it decayed**, which is backwards. With a fast bright crack and a slower low rattle the brightness ratio between the attack and the tail flips from 0.84 to 1.37. Two tuned heads rather than one, too, a fourth apart, so the tone under the noise is a drum and not a beep. |
| **Cymbals made of metal** | Filtered white noise is the usual shortcut for a hi-hat, and it is why drum machines that use it sound like escaping steam. Struck metal does not make noise — it rings at a handful of frequencies that are *inharmonic*, not whole-number multiples of anything, and that clash is the whole sound. Six square partials at the ratios the classic drum machines used, mixed under the noise, rendered once per context into a buffer rather than six live oscillators per hit: a busy hat pattern is several hundred hits a minute. Each hit reads that buffer at a slightly different speed, which moves all six partials together — the shift a real cymbal gives you for being struck in a slightly different place. |
| **Chords from a parent scale** | Chords here are made by stacking every other note of a scale, which is where the idea of a triad comes from and works perfectly for any seven-note scale. It falls apart everywhere else: stack every other note of a five-note scale and you get fourths, of the whole-tone scale and every chord is augmented, of the chromatic scale and every chord is a cluster. So a scale that is not seven notes long names a parent to take its harmony from, which is not a workaround but what the music does — a blues melody is played over dominant chords, a minor-pentatonic riff over ordinary minor harmony. |
| **The two intervals no chord survives** | A semitone between two notes of a voicing, and a minor ninth — a semitone with an octave added, which stays just as harsh. Both are checked between the notes *as stacked*, not as pitch classes, and that distinction is the whole thing: a major seventh chord holds a B against a C and is the most consonant chord there is, because they are eleven semitones apart and not one. This is what caught the sixth chords built on a degree whose sixth is flat, where the ♭6 landed directly against the fifth — they were being accepted *and* mis-named, because the naming rules have no symbol for a ♭6 and silently left it out, so the chord strip claimed a plain minor triad while the pad played a clash. |
| **Harmony lines** | Written as notes rather than done with a pitch shifter, because the notes are already known and a shifter would only guess at them. The interval is counted in scale steps: a third is *two steps*, which comes out as four semitones from one degree and three from the next, and that bending is what makes the second voice belong to the key. Where the tune goes chromatic — the third of a secondary dominant, a passing note — the harmony is measured from the nearest scale degree so it stays in key, and then steps on if that would land it within a whole tone of the note, because a second is not a harmony, it is a mistake. Measured across nine style-and-seed combinations: every added note in key, every interval a major or minor third, and both sizes present in all nine. |
| **Loudness (LUFS)** | How loud a track *sounds*, which is neither its peak nor its average — a track can peak at full scale and still sound quiet. Measured to ITU-R BS.1770, the standard every streaming service uses: K-weight the audio (a shelf standing in for the head, a highpass for the fact that deep bass contributes less than its energy suggests), average the energy in 400 ms blocks, then throw away the blocks that are mostly silence before averaging what is left. That last step is why a song with a long quiet intro does not read as quiet. The standard publishes filter coefficients for 48 kHz only, so these are built from the design parameters and come out right at any rate. Checked against the calibration point the standard is defined by: a full-scale 1 kHz stereo sine reads **0.01 LUFS**. |
| **Dither** | Rounding to 16 bits leaves an error that follows the signal, which makes it *distortion* rather than noise — audible as a gritty tail on a fade, where the music is using only the last few steps. A triangle of noise one step tall, added before rounding, breaks that correlation: the error becomes an even hiss about 90 dB down. It also buys resolution below the last bit — a sine three tenths of one step tall survives the file with a correlation of 0.39, where plain rounding erases it to exactly nothing. |
| **Split glue** | Three compressors across bass, middle and treble instead of one across everything. The obvious claim for this — that splitting stops the kick dragging the hats down — was asserted here for a while and does **not** survive being measured properly: across four combinations of hat level and glue amount the split is worse on that count in three of them, because the high band hears only the hats and compresses them against their own level, where a wideband compressor had them as a small part of a much bigger signal. What is reliably true is the other half of why engineers reach for a multiband: **about 7% more level for the same glue setting**, in every configuration tried, because each band gives up only what that band needs to. The crossovers are Linkwitz-Riley: two filters in series at each split point, because a single pair splits the signal but does not put it back together — the halves arrive out of phase around the crossover and partly cancel. Cascading two is what makes them sum flat again, and the bass band gets an allpass at the upper split so it picks up the same phase shift the other two do on their way through it. |
| **Butterworth Q in Web Audio** | Worth writing down because it is a trap. For `lowpass` and `highpass` — and *only* those two — the `Q` parameter is read in **decibels**: it is the gain at the cutoff frequency. The default of 1 is therefore a filter that is 1 dB *up* exactly where it should be coming down. A Butterworth is 0.707 in the usual quality-factor sense, which here is 20·log₁₀(0.707) = **−3.01 dB**. Built with `0.707` in the belief that it meant the usual thing, the crossover above summed to **+7.5 dB** at both split points and every band came back 47% too loud; at −3.01 it sums to 0.000 dB at every frequency tested. |
| **Pump (sidechain)** | Web Audio compressors have no sidechain input, but the kick times are already in the score — so rather than following a signal, the duck is written as gain automation exactly where the kick lands. Depth and recovery are read fresh on every kick instead of baked into the graph, so both move while the song plays, and the gain node is built for every duckable part whether or not the style pumps at all — which is what lets a style with no pump be given one without rebuilding anything. |
| **Chop** | A rhythmic gate, and deliberately *not* a noise gate. A noise gate exists to remove hiss and microphone bleed between the notes of a recording, and there is neither in a mix synthesised from a score — every part is already silent when it is not playing. What people actually reach for a gate to do is cut a held pad into a pulse, so that is what this does: open for the first half of each step, shut for the second, on a grid of quarters, eighths or sixteenths. It runs off its own counter rather than off the notes, the way the metronome does — a gate that only fired where a note started would be following the part instead of cutting it — and the counter resyncs from the clock, so switching it on mid-song lands it on the grid rather than grinding through the steps it missed. |
| **Tape echo** | The same delay line, but the feedback path is filtered *and* saturated, so each repeat comes back darker and softer rather than merely quieter — and two LFOs bend the delay time, a slow drift and a faster quiver. That last part is the whole character: a tape echo's loop never runs at exactly constant speed, so every repeat is a little out of tune, and changing how long a delay is *is* changing the speed the sound comes off it. A lowpass inside a feedback loop is safe at this scale; the flanger's instability came from a loop only milliseconds long, where the loop gain compounds hundreds of times a second. |
| **Multi-tap** | Three fixed taps at rising delays and falling levels, spread hard left, hard right and centre, with no feedback whatsoever. A feedback delay repeats one rhythm getting quieter; this plays a pattern — which is why it is its own flavour rather than a setting on the others. |
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
