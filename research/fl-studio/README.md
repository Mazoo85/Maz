# FL Studio research pack

Research on **FL Studio 2026**, the newest version of Image-Line's music-making program. It was
gathered on **5 November 2026** to guide building our own program later.

> **Not part of the Maz engine.** This is research only. Per `CLAUDE.md`, whatever we build from
> it will use something other than the Maz engine, chosen with Cody.

## Read in this order

| File | What's in it |
|---|---|
| [01-whats-new-2026.md](01-whats-new-2026.md) | The newest version, every 2026.1.x update, and how FL got here from 21.x to 2026 |
| [02-how-it-works.md](02-how-it-works.md) | Window-by-window spec: Channel Rack, Piano Roll, Playlist, Mixer, automation, recording, export, shortcuts, and a suggested data model |
| [03-editions-and-plugins.md](03-editions-and-plugins.md) | What it costs, the four editions, FL Cloud, and the full built-in plugin list |
| [04-technical.md](04-technical.md) | Platforms, audio engine, plugin formats, the `.flp` file format, scripting, AI and cloud, competitors, and open-source building blocks |

## FL Studio 2026 at a glance

- **Newest build:** FL Studio **2026.1.7b**, released 12 October 2026. FL Studio 2026 itself came
  out on 7 July 2026.
- **Platforms:** Windows 10/11 and macOS 10.15+, with native Apple Silicon. There is no Linux
  version.
- **Editions:**

  | Edition | Price (2026) |
  |---|---|
  | Fruity | $99 |
  | Producer | about $179–199 |
  | Signature | about $269–299 |
  | All Plugins | about $449–499 |

  You buy once and get **every future version free**. There is also an optional **FL Cloud**
  subscription (sounds, AI mastering, distribution, rented plugins, backup), and Rent-to-Own from
  $20 a month.
- **New in 2026:**
  - Rebuilt FLEX instrument (lighter on CPU).
  - **Gopher**, an AI assistant that can now *operate* the project.
  - **Remix a Song**: load a song, auto-detect its tempo, and split it into stems.
  - **Audio Logger**: always records the last ~60 seconds.
  - Cloud project backup.
  - Transmitter effect.
  - Note Arpeggiator and chord tools.
  - Tighter MIDI timing.
  - macOS audio improvements.

## What makes FL Studio *FL Studio*

These are the things our program would need in order to feel familiar to FL users:

1. **Patterns, not tracks.** A pattern holds notes for many instruments at once. The Playlist
   arranges copies of patterns on free-form tracks, and any clip can go on any track.
2. **Step sequencer.** The Channel Rack's grid of 16th-note buttons is the fastest way to make a
   beat. It is FL's signature.
3. **The best Piano Roll.** Ghost notes, slide and porta notes, per-note Mod X/Y, chord stamps,
   scale highlighting, and generators such as Riff Machine, Strum, Arpeggiate and Randomize. It
   is also scriptable in Python.
4. **A flexible mixer.** Up to 500 tracks, 10 effect slots each, any-to-any routing, and
   zero-level sidechain sends.
5. **Many ways to automate.** Automation clips, recorded events, link-to-controller, Multilink,
   and controller plugins.
6. **Instant feedback.** A hint bar that explains everything you hover over, drag-and-drop from
   the Browser, and a vector UI that scales to any screen.
7. **Generous business model.** Lifetime free updates, an unlimited trial (you just can't reopen
   saved projects), and a simple upgrade path where audio recording is the main paid unlock.

## Where FL Studio is weak (our openings)

- No **Linux** or **web** version.
- No **real-time collaboration**: projects are shared as files.
- No **ARA2** (deep Melodyne-style integration) and no **per-plugin crash protection**.
- Plugin latency compensation has quirks, and audio comping and editing is weaker than in rivals.
- The project format is closed and undocumented. Only community reverse-engineering (PyFLP)
  exists.
- The AI features (stems, mastering, Gopher) are closed, and the models behind them are
  undisclosed.

## Suggested first version of our program

Drawn from [02-how-it-works.md](02-how-it-works.md) and [04-technical.md](04-technical.md):

1. The data model: integer ticks at 96 PPQ, and patterns holding notes for many channels.
2. Channel Rack with a step grid and a graph editor.
3. Piano Roll: draw, select and slice tools, velocity and pan lanes, snap, ghost notes.
4. Playlist with pattern, audio and automation clips, "make unique", markers and loop.
5. Mixer: N tracks × 10 effect slots, sends, sidechain-at-zero, pan law, latency compensation.
6. Automation clips and link-to-controller.
7. WAV export, with stems.
8. Undo history.

**Later layers:** performance mode, Riff Machine, an audio editor (like Edison), cloud features
and an AI assistant. The assistant should come last, on top of a clean internal project API like
FL's Python modules.

**Choosing the technology** (to decide together before building):

| Option | Strengths | Limits |
|---|---|---|
| **Desktop app** (JUCE or Tracktion Engine) | Plugin hosting, top audio performance | Paid licences for closed source |
| **Browser app** (Web Audio + AudioWorklet + WebMIDI) | Instant to share and try; collaboration is easier | No VST plugins |

## How reliable this is

- The research tools could not open Image-Line's website directly; its pages were blocked by
  the research environment's network. Facts were taken from search-engine excerpts of Image-Line's
  official pages (manual, release posts, shop) and from press coverage, then cross-checked.
- Each file marks claims as **confirmed**, **secondary** or **uncertain**.
- The least certain areas are:
  - exactly which edition includes which plugin;
  - current FL Cloud prices;
  - the timebase options;
  - internal engine details.

  Check these on image-line.com before relying on them.
