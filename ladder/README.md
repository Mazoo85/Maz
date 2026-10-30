# Ladder 24

**A polyphonic analog-style synthesizer in the browser**, modelled on the classic
1970s three-oscillator monosynths and their 24 dB/octave transistor-ladder
filter, then taken further: eight voices, unison, an arpeggiator, echo and reverb.

Open `index.html` in any current browser and press **Power** (or just play a
key). No build step, no server, no dependencies. Works with a mouse, a finger
on a phone, the computer keyboard, or a USB MIDI keyboard.

A standalone app: it is not part of MAZ ARCADE.

## Playing it

| | |
|---|---|
| **On-screen keys** | Tap or click. Slide across for a glissando. Lower on the key plays louder. |
| **Computer keys** | `A W S E D F T G Y H U J K O L P ; '` play from C3. `Z` / `X` shift the octave. `Esc` stops every note. |
| **MIDI keyboard** | Plug it in before pressing Power. Notes, velocity, pitch bend, mod wheel (CC 1) and sustain pedal (CC 64) all work. |
| **Pitch / Mod wheels** | Drag up and down. Pitch springs back to centre. |
| **Tempo** | At the top: drag the number, use − / +, or press **Tap** in time a few times. The light flashes on each beat (red on the first of four). **Hold** keeps your tempo when you change patch; otherwise each patch brings its own. |
| **Knobs** | Drag up/down (or left/right). Hold Shift for fine moves. Double-click to reset. Arrow keys work once a knob has focus. |

## What is on the panel

| Section | Controls |
|---|---|
| **Controllers** | Glide (portamento), bend range, voice mode (Poly 8 voices / Mono / Legato / Unison 5 stacked voices), unison detune, stereo width. |
| **Oscillator bank** | Three oscillators, each with six band-limited waves (triangle, saw, ramp, square, pulse, sine) and a range switch from 32' to 2'. Oscillators 2 and 3 tune ±7 semitones. Pulse width. Osc 3 can leave the keyboard and run as a slow LFO. **Pitch env** bends every oscillator with the filter contour, ±24 semitones: the drop in a kick, a tom or a zap. |
| **Mixer** | Levels for the three oscillators and white noise, plus Drive, which pushes the filter input into saturation. |
| **Ladder filter** | A zero-delay-feedback model of the four-pole ladder, with a tanh at its input. Cutoff, Emphasis (resonance, which self-oscillates into a pure sine at the top), Contour amount (±6 octaves) and key tracking, with its own ADSR. |
| **Loudness contour** | ADSR for the volume, and touch (velocity) amounts for volume and brightness. |
| **Modulation** | Source (LFO, Osc 3 at audio rate, or smoothed noise), LFO shape (triangle, saw, square, sample & hold), rate or **Sync** to the tempo (4 bars down to 1/16), a base amount the mod wheel adds to, and depths to pitch, filter and pulse width. |
| **Arpeggiator** | Up, down, up-down, as-played, random; 1–4 octaves; 1/4 to 1/32 steps including triplets, at the panel tempo; gate length; latch. It runs inside the audio engine, so the timing is sample-accurate. |
| **Echo & space** | A ping-pong echo that darkens with each repeat, **synced to the tempo** by default (1/16 to 1/2, dotted and triplet) or free-running in milliseconds, a generated reverb with an adjustable room size, and the master volume (with a limiter after it). |
| **Output** | An oscilloscope, a light for each of the eight voices, and an all-notes-off button. |

## Patches

Ninety-two factory sounds in ten groups, plus a plain **Init** patch to start from scratch:

| Group | Patches |
|---|---|
| **Bass** | Fat Bass, Sub Bass, Pedal Drop, Rubber Pluck, Acid Squelch, Funk Bass, Growl Bass, Reese Bass, Synth-Pop Bass |
| **Lead** | Glide Lead, Prog Solo, Screaming Emphasis, Square Lead, Fifths Lead, Theremin, Chiptune Lead |
| **Keys** | Electric Piano, Clav, Drawbar Organ, Poly Stab, Bell Piano, Reed Piano, Lo-Fi Piano, Toy Piano, Harpsichord, Celesta, Mallet Keys, Muted Keys, Synth Keys, House Piano, Combo Organ, Church Organ, Gospel Organ |
| **Pads** | Warm Pad, Glass Pad, Slow Sweep, Supersaw Unison, Dark Drone, Choir Pad, Shimmer Pad, Space Pad, PWM Pad, Breathing Pad, Rhythm Pad, Ambient Wash, Cinematic Swell, Soft Sine Pad, 80s Poly Pad, Vapor Pad, Glacier, Hollow Pad |
| **Brass & strings** | Brass Section, Brass Stab, Velvet Strings, Solo Cello |
| **Plucks** | Bright Pluck, Nylon Pluck, Kalimba, Marimba |
| **Drums** | **Drum Kit** (a different drum on every key, below), then each drum on its own: Analog Kick, 808 Boom, Hard Kick, Analog Tom, Syn Tom, Conga, Snare, Electro Snare, Rimshot, Hand Clap, Closed Hat, Open Hat, Shaker, Crash, Ride, Cowbell, Woodblock (one hit per key; the pitch follows the key) |
| **Effects** | Laser Zap, Siren, UFO, Wind, Ocean Waves |
| **Experimental** | Filter Whistle, Wobble, Sample & Hold Computer, Metal Bells |
| **Sequenced** | Pulse Arp, Latched Sequence, Berlin School, Chip Arp, Trance Gate, Random Bleeps (hold a chord) |

### The drum kit

Pick **Drum Kit** (or flip the **Drum kit** switch under Controllers) and every
key plays its own drum, with its name on the key. The layout covers two octaves
from C3 and repeats up and down the keyboard. It follows the drum-machine habit:
kick on C, snare on D, hi-hats on the black keys.

| | C | C♯ | D | D♯ | E | F | F♯ | G | G♯ | A | A♯ | B |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **C3 octave** | Kick | Rimshot | Snare | Clap | Electro snare | Low tom | Closed hat | Mid tom | Pedal hat | High tom | Open hat | Crash |
| **C4 octave** | 808 boom | Woodblock | Hard kick | Cowbell | Low syn tom | Ride | Shaker | High syn tom | Tambourine | Low conga | High conga | Laser zap |

Drums ring out after you let go, like real ones, and a closed or pedal hat cuts
off a ringing open hat. On the computer keyboard, `A` is the kick, `S` the snare,
`T` the closed hat and `U` the open hat. The sound knobs don't change the kit;
Tempo, the arpeggiator, Echo & space and Volume do.

Each patch sets its own Volume so they all play at about the same loudness;
a test holds every patch to that. **Save** keeps your own in this browser.
**Share** gives you a short patch code (`L24:…`) you can paste anywhere, and
a box to load one back.
The last sound you used is still there next time you open the page.

## Files

| File | What it is |
|---|---|
| `js/dsp.js` | The sound engine: oscillators, ladder filter, envelopes, voices, glide, arpeggiator. Runs in an AudioWorklet; the same file loads in Node for the tests. |
| `js/logic.js` | Every control's range, curve and readout, the factory patches, patch codes, the keyboard map. No sound, no page. |
| `js/app.js` | The page: panel, knobs, keyboard, wheels, MIDI, effects, scope, saving. |
| `css/ladder.css` | The look: black panel, walnut cheeks, amber lights. |
| `tests/ladder-logic.test.js` | Plays notes through the real engine and measures what comes out: pitch, filter slope, self-oscillation, release, voice stealing, glide, arpeggiator timing, patch codes, and that every factory patch sounds at a matching loudness. |

## Tests

```sh
node ladder/tests/ladder-logic.test.js
```

CI runs them on every change to `ladder/` (`.github/workflows/ladder.yml`).
