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
| **Knobs** | Drag up/down (or left/right). Hold Shift for fine moves. Double-click to reset. Arrow keys work once a knob has focus. |

## What is on the panel

| Section | Controls |
|---|---|
| **Controllers** | Glide (portamento), bend range, voice mode (Poly 8 voices / Mono / Legato / Unison 5 stacked voices), unison detune, stereo width. |
| **Oscillator bank** | Three oscillators, each with six band-limited waves (triangle, saw, ramp, square, pulse, sine) and a range switch from 32' to 2'. Oscillators 2 and 3 tune ±7 semitones. Pulse width. Osc 3 can leave the keyboard and run as a slow LFO. |
| **Mixer** | Levels for the three oscillators and white noise, plus Drive, which pushes the filter input into saturation. |
| **Ladder filter** | A zero-delay-feedback model of the four-pole ladder, with a tanh at its input. Cutoff, Emphasis (resonance, which self-oscillates into a pure sine at the top), Contour amount (±6 octaves) and key tracking, with its own ADSR. |
| **Loudness contour** | ADSR for the volume, and touch (velocity) amounts for volume and brightness. |
| **Modulation** | Source (LFO, Osc 3 at audio rate, or smoothed noise), LFO shape (triangle, saw, square, sample & hold) and rate, a base amount the mod wheel adds to, and depths to pitch, filter and pulse width. |
| **Arpeggiator** | Up, down, up-down, as-played, random; 1–4 octaves; 1/4 to 1/32 steps including triplets; tempo; gate length; latch. It runs inside the audio engine, so the timing is sample-accurate. |
| **Echo & space** | A ping-pong echo that darkens with each repeat, a generated reverb with an adjustable room size, and the master volume (with a limiter after it). |
| **Output** | An oscilloscope, a light for each of the eight voices, and an all-notes-off button. |

## Patches

Fifteen factory patches across bass, lead, poly, experimental and sequenced
sounds. **Save** keeps your own in this browser. **Share** gives you a short
patch code (`L24:…`) you can paste anywhere, and a box to load one back.
The last sound you used is still there next time you open the page.

## Files

| File | What it is |
|---|---|
| `js/dsp.js` | The sound engine: oscillators, ladder filter, envelopes, voices, glide, arpeggiator. Runs in an AudioWorklet; the same file loads in Node for the tests. |
| `js/logic.js` | Every control's range, curve and readout, the factory patches, patch codes, the keyboard map. No sound, no page. |
| `js/app.js` | The page: panel, knobs, keyboard, wheels, MIDI, effects, scope, saving. |
| `css/ladder.css` | The look: black panel, walnut cheeks, amber lights. |
| `tests/ladder-logic.test.js` | Plays notes through the real engine and measures what comes out: pitch, filter slope, self-oscillation, release, voice stealing, glide, arpeggiator timing, patch codes. |

## Tests

```sh
node ladder/tests/ladder-logic.test.js
```

CI runs them on every change to `ladder/` (`.github/workflows/ladder.yml`).
