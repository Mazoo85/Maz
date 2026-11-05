# Build brief: our own music program, based on the FL Studio research

**For:** the Claude session running on Cody's own computer.
**From:** the cloud session that did the FL Studio research (5 November 2026).

Cody has no coding experience. Explain things plainly, show him the program running as early as
possible, and keep him in the loop at each step.

## 1. Read the research first

Everything is in this folder (`research/fl-studio/` in the Maz repo, on the
`claude/sharp-rubin-naty8q` branch).

| File | What it covers |
|---|---|
| `README.md` | The summary, FL Studio's weak spots, and a suggested first version |
| `01-whats-new-2026.md` | What's new in FL Studio 2026 |
| `02-how-it-works.md` | Window-by-window spec and a suggested data model; **the main build reference** |
| `03-editions-and-plugins.md` | Pricing, editions, and the list of built-in instruments and effects |
| `04-technical.md` | Audio engine, file formats, scripting, competitors, and code we could build on |

Treat the research as a description of *what* FL Studio does. We are building **our own original
program**:

- **No** copying FL Studio's name, logo, artwork, look, sounds or presets.
- Use the same *kinds* of features: pattern workflow, step sequencer, piano roll, playlist, mixer.
- Give it its own name and design. Ask Cody what he'd like to call it.

## 2. Look through what Cody already has

Cody asked for this explicitly. Search his computer for anything that could help:

- **Places to look:** his Desktop, his Documents and Downloads, project folders, and his Claude
  projects and sessions.
- **What to look for:**
  - music or audio projects;
  - samples and sound files;
  - earlier attempts at a music app or beat maker;
  - designs, notes and ideas;
  - code that plays sound or handles MIDI;
  - anything from his past Claude work.

Two places in the Maz repo deserve a look first: **`riffsmith/`** (a riff generator with note
editing, rhythm chops and sounds) and **`music/`** (a browser music app). Both may hold reusable
code.

Then:

1. **Make a short list** of what you found and how each item could help.
2. **Show Cody the list before using any of it.**
3. **Don't move, delete or change his files.** Copy what's needed into the new project instead.
4. **Skip anything private.** If something looks personal (finances, passwords, private
   documents), leave it out and don't read it further.

## 3. Choose how to build it, with Cody

**Rule from the repo's `CLAUDE.md`:**
- Do **not** build this on the Maz game engine.
- New projects do **not** join MAZ ARCADE unless Cody asks. Build it standalone: its own folder,
  look and docs, and its own CI workflow.

**Ask Cody which way to go before writing code**, and explain the trade-off in plain words:

| Option | Good for | Limits |
|---|---|---|
| **Browser app** (recommended to start) | Opens in any web browser and is easy to share. Works on any computer, including his. Uses Web Audio, AudioWorklet and WebMIDI | Can't load VST plugins |
| **Desktop app** (for example JUCE in C++) | Can host VST/CLAP plugins; the most audio performance | Much bigger job; some libraries need paid licences for a closed-source product |

## 4. What to build first

From the research README:

1. **Data model.**
   - Positions in integer ticks at 96 per beat.
   - **Patterns** that hold notes for many instruments at once.
   - **A playlist** that arranges copies of patterns on free-form tracks.
2. **Channel Rack with a step sequencer.** A grid of 16th-note buttons per instrument, plus a few
   built-in sounds: a simple synth and a sample player with some drum sounds.
3. **Piano Roll.**
   - Draw, select and delete notes; snap to grid.
   - A velocity lane.
   - Faint "ghost notes" from other instruments.
4. **Playlist.** Place pattern clips and audio clips, loop a section, play the song.
5. **Mixer.** Tracks with volume and pan, a few effects (EQ, compressor, delay, reverb), and sends.
6. **Saving and loading projects**, and exporting to a WAV file.
7. **Undo and redo.**

**Later:**
- automation clips;
- recording;
- live performance mode;
- melody and chord helpers;
- an audio editor;
- an AI assistant that can control the project.

**Build in small steps.** After each step:
- show Cody it working;
- add tests for the logic (timing, patterns, the mixer maths);
- commit.

## 5. Where it lives

- **Location:** keep it in the Maz repo, in its own new folder (Cody's rule: "this repo is the
  home").
- **Branch:** a new branch off `main`.
- **Pull requests:** don't open one unless Cody asks.
- **Before pushing:** run the repo's two quick checks:

  ```bash
  node scripts/check-links.mjs && node scripts/check-exchange.mjs
  ```
