# Ladder 24

**A polyphonic analog-style synthesizer in the browser**, modelled on the classic
1970s three-oscillator monosynths and their 24 dB/octave transistor-ladder
filter, then taken further: eight voices, unison, an arpeggiator, echo and reverb.

Open `index.html` in any current browser and press **Power** (or just play a
key). No build step, no server, no dependencies. Works with a mouse, a finger
on a phone, the computer keyboard, or a USB MIDI keyboard.

A standalone app: it is not part of MAZ ARCADE.

**Desktop program:** the same synthesizer as an installable program for
Windows, Mac and Linux lives in [`desktop/`](desktop/README.md), with the
install steps.

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
| **Ladder filter** | A zero-delay-feedback model of the four-pole ladder, with a tanh at its input. **Mode** mixes its four stages for low-pass 24 or 12 dB, band-pass, or high-pass 12 or 24 dB. Cutoff, Emphasis (resonance, which self-oscillates into a pure sine at the top), Contour amount (±6 octaves) and key tracking, with its own ADSR. |
| **Loudness contour** | ADSR for the volume, and touch (velocity) amounts for volume and brightness. |
| **Modulation** | Source (LFO, Osc 3 at audio rate, or smoothed noise), LFO shape (triangle, saw, square, sample & hold), rate or **Sync** to the tempo (4 bars down to 1/16), a base amount the mod wheel adds to, and depths to pitch, filter and pulse width. |
| **Arpeggiator** | Up, down, up-down, as-played, random; 1–4 octaves; 1/4 to 1/32 steps including triplets, at the panel tempo; gate length; latch. It runs inside the audio engine, so the timing is sample-accurate. |
| **Echo & space** | A ping-pong echo that darkens with each repeat, **synced to the tempo** by default (1/16 to 1/2, dotted and triplet) or free-running in milliseconds, a generated reverb with an adjustable room size, and the master volume (with a limiter after it). |
| **Output** | An oscilloscope, a light for each of the eight voices, and an all-notes-off button. |

## Patches

Three hundred and thirty factory sounds: **30 in each of eleven sections**, plus a plain
**Init** patch to start from scratch. Pick a section in the first menu and a
sound in the second; **‹ ›** step through every sound in order.

| Section | Sounds |
|---|---|
| **Bass** | Fat Bass, Sub Bass, Pedal Drop, Rubber Pluck, Acid Squelch, Funk Bass, Growl Bass, Reese Bass, Synth-Pop Bass, Round Bass, Punch Bass, Pluck Bass, Soul Bass, Wide Unison Bass, Detroit Bass, Electro Bass, 808 Glide Bass, Hoover Bass, Dub Bass, Squelchy Saw, Fingered Bass, Synced Wobble, Triplet Wobble, Gritty Bass, Velvet Sub, Hollow Bass, Snappy Bass, Rumble Bass, Fifth Stab Bass, Talking Bass |
| **Lead** | Glide Lead, Prog Solo, Screaming Emphasis, Square Lead, Fifths Lead, Theremin, Chiptune Lead, Bright Saw Lead, Soft Flute Lead, Whistle Lead, Pan Pipe, Brass Lead, G-Funk Lead, Acid Lead, Nasal Lead, Detuned Lead, Fat Mono Lead, Portamento Lead, Handheld Game Lead, Laser Lead, Smooth Sine Lead, Overdrive Lead, Hollow Square Lead, Octave Lead, Weeping Lead, Oboe Lead, Clarinet Lead, Trumpet Lead, Wah Lead, Siren Lead |
| **Keys** | Electric Piano, Clav, Drawbar Organ, Poly Stab, Bell Piano, Reed Piano, Lo-Fi Piano, Toy Piano, Harpsichord, Celesta, Mallet Keys, Muted Keys, Synth Keys, House Piano, Combo Organ, Church Organ, Gospel Organ, Bright Piano, Upright Piano, Glass Keys, Tine Piano, Bright Tines, Accordion, Harmonium, Music Box, Clavinet Wah, Mellow Keys, Stab Organ, Rock Organ, Bark Piano |
| **Pads** | Warm Pad, Glass Pad, Slow Sweep, Supersaw Unison, Dark Drone, Choir Pad, Shimmer Pad, Space Pad, PWM Pad, Breathing Pad, Rhythm Pad, Ambient Wash, Cinematic Swell, Soft Sine Pad, 80s Poly Pad, Vapor Pad, Glacier, Hollow Pad, String Pad, Dream Pad, Night Pad, Sunrise Pad, Organ Pad, Two-Bar Sweep, Bell Pad, Frozen Pad, Sixteenth Gate Pad, Low Drone Pad, Airy Pad, Analog Brass Pad |
| **Brass & strings** | Brass Section, Brass Stab, Velvet Strings, Solo Cello, Synth Brass, Soft Brass, Horn Section, Trombone, French Horn, Tuba, Brass Swell, Fanfare, Funk Horns, Saxophone, String Ensemble, Violin, Viola, Double Bass, Pizzicato, Tremolo Strings, Slow Strings, Chamber Strings, Cello Section, Orchestra Hit, Bright Strings, Dark Strings, Fiddle, Analog String Machine, Staccato Strings, Crescendo Strings |
| **Plucks** | Bright Pluck, Nylon Pluck, Kalimba, Marimba, Harp, Koto, Banjo, Guitar Pluck, Muted Guitar, Twangy Pluck, Bell Pluck, Glass Pluck, Synth Pluck, Trance Pluck, Xylophone, Vibraphone, Steel Drum, Water Drop, Rubber Band, Short Pluck, Mandolin, Mbira, Lute, Clean Guitar, Electric Guitar, Funk Guitar, Chime, Blip, Dulcimer, Echo Pluck |
| **Drums** | Drum Kit, Analog Kick, 808 Boom, Hard Kick, Analog Tom, Syn Tom, Conga, Snare, Electro Snare, Rimshot, Hand Clap, Closed Hat, Open Hat, Shaker, Crash, Ride, Cowbell, Woodblock, Rock Kick, Soft Kick, Brush Snare, Piccolo Snare, Big Room Clap, Finger Snap, Floor Tom, Timbale, Bongo, Splash Cymbal, China Cymbal, Triangle |
| **Drum Kits** | Classic 808, Trap 808, Techno 909, House, Minimal Techno, Rock, Jazz Brushes, Funk, Boom Bap, Lo-Fi, Electro, Synthwave 80s, Disco, Drum & Bass, Dubstep, Industrial, Latin Percussion, Afrobeat, Reggae Dub, Big Room EDM, Chiptune, Vintage Rhythm Box, UK Garage, Ambient, Metal, Glitch Beats, Cinematic, Tribal, Electronic Toms 80s, Hand Percussion |
| **Effects** | Laser Zap, Siren, UFO, Wind, Ocean Waves, Thunder, Rain, Heartbeat, Bubbles, Alarm, Radio Static, Robot Voice, Engine Rumble, Spaceship Hum, Teleport, Power Down, Power Up, Game Coin, Game Jump, Explosion, Wind Chimes, Birds, Crickets, Underwater, Ghost, Laser Gun, Phone Tone, Static Swell, Glitch, Train Horn |
| **Experimental** | Filter Whistle, Wobble, Sample & Hold Computer, Metal Bells, Resonance Sweep, Audio-Rate Filter, Breath Whistle, Ringing Bass, Chaos Engine, Drive Feedback, Reverse Contour, Backwards Pad, Microtonal Drift, Detune Cloud, Tritone Machine, Spectral Sines, Gated Noise, Audio PWM, Slow Morph, Sub Harmonic, Stepped Filter, Random Melody, Tape Wobble, Talking Filter, Bent Bell, Feedback Scream, Noise Bell, Octave Jumper, Saw Swarm, Granular Fizz |
| **Sequenced** | Pulse Arp, Latched Sequence, Berlin School, Chip Arp, Trance Gate, Random Bleeps, Octave Bass Arp, Disco Octaves, Synthwave Arp, Ambient Arp, Techno Stab Seq, Acid Sequence, Bell Arp, Chip Melody, Pluck Cascade, Triplet Run, Half-Time Arp, Driving Bass, Thirty-Second Ripple, Sequencer Lead, Flowing Arp, Random Sparkle, Dub Techno Chord, Electro Bounce, Hypnotic Loop, Harp Glissando, Marimba Pattern, Trance Lead Arp, Slow Motion, Drum Kit Arp |

Drums play one hit per key, with the pitch following the key. Each sound in
**Drum Kits** puts a different drum on every key (below). Sequenced sounds run the arpeggiator:
hold a chord. **Drum Kit Arp** latches a drum pattern from the keys you hold.

### Drum kits

Pick a kit from the **Drum Kits** section (or flip the **Drum kit** switch under
Controllers and choose from its **Kit** menu) and every key plays its own drum,
with its name on the key. The layout covers two octaves from C3 and repeats up
and down the keyboard. Every kit uses the same drum-machine layout: kick on C,
snare on D, hi-hats on the black keys.

| | C | C♯ | D | D♯ | E | F | F♯ | G | G♯ | A | A♯ | B |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **C3 octave** | Kick | Rimshot | Snare | Clap | Snare 2 | Low tom | Closed hat | Mid tom | Pedal hat | High tom | Open hat | Crash |
| **C4 octave** | Long kick | Clave | Hard kick | Cowbell | Low conga | Ride | Shaker | Mid conga | Tambourine | High conga | Effect | Splash |

Some kits rename a key for what it really is (bongos, timbales, taiko, djembe,
maracas). Drums ring out after you let go, and a closed or pedal hat cuts off a
ringing open hat. On the computer keyboard, `A` is the kick, `S` the snare, `T`
the closed hat and `U` the open hat. The sound knobs don't change a kit; Tempo,
the arpeggiator, Echo & space and Volume do.

Each kit is a short description of its character (`KITS` in `js/logic.js`),
turned into 24 drums by recipes that build every sound from oscillators, noise
and filters, the way an analog drum machine does. **Classic 808** follows the
original analog design: a deep sine kick with a long tail, a two-tone snare,
hi-hats and cymbals from three detuned square waves (the original uses six) and noise through a high-pass
filter, the 540/800 Hz cowbell, clave, maracas and three congas. It is a
synthesized recreation, not samples of the hardware.

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
