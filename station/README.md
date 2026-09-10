# CJC Music Station — Web Edition

A zero-dependency browser groovebox: the web counterpart of the native C++/SDL3
**CJC Music Station** DAW that lives in [`../apps/daw`](../apps/daw). Everything is
synthesized live with the Web Audio API — no samples, no files, no build step.

Open **[`index.html`](index.html)** in any modern browser (or serve the folder) and
press **Play** (or **Space**) — a demo groove and bassline are already loaded.

## What's in it

- **Drum rack** — six voices synthesized from scratch (kick with a pitch-drop, a
  noise+tone snare, filtered-noise closed/open hats, a multi-burst clap, and a tom),
  each with mute and volume, on a 16- or 32-step grid with beat grouping and a moving
  playhead.
- **Bass / lead** — a subtractive synth (saw / square / triangle / sine → resonant
  low-pass with a filter envelope → amp envelope) played from a minor-pentatonic
  mini piano-roll, one note per column, with cutoff / resonance / length / key controls.
- **Transport** — tempo (60–190 BPM), swing (delays the off-beat 16ths), master volume
  with a glue compressor, step count, demo-groove reload, and clear.
- **Light / dark theme** toggle, phone-responsive layout, and the pattern is remembered
  in the browser (`localStorage`).

## Relationship to the native DAW

This is a scoped-down, from-scratch Web Audio reimplementation — a playable slice of the
native station, not the C++ engine recompiled. The native app (`apps/daw`) remains the
full-featured DAW (dozens of drum voices and effects, plugin hosting, the 2-D playlist,
WAV/stem/MIDI export, `.cjc` projects). WAV export is intentionally omitted here: a page
cannot start a file download inside a sandboxed artifact host.

## Ideas for growth

More drum voices to match the native kit, per-step velocity, a second melodic channel,
pattern/song chaining, and send effects (delay / reverb).
