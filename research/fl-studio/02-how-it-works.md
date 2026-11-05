# 2. How FL Studio works: a functional specification

> Part of the [FL Studio research pack](README.md). Researched 5 November 2026.
>
> **What this is.** A description of each part of FL Studio 2026, detailed enough for a team to
> rebuild something equivalent. It is built from excerpts of Image-Line's online manual and
> press coverage.
>
> **Confidence marks.**
> - **[uncertain]**: could not be checked against a current source.
> - **[background]**: general product knowledge, not confirmed in this research.
>
> Some manual pages are shared across versions, so a few details may describe 2024/2025
> behaviour.

---

## 2.1 The big idea: pattern-first

Most music programs are **track-based**: a "MIDI clip" belongs to one instrument track. FL Studio
is **pattern-based**, and this is the single most important thing to understand about it.

**The parts and how they relate:**

- **Channel.** A sound source: an instrument plugin, a sampler, an audio-clip channel, an
  automation channel, or a "layer". All channels live in the **Channel Rack** and are global to
  the project.
- **Pattern.** A named, coloured container.
  - It holds notes for **many channels at once** (a whole drum beat plus bass, for example).
  - It also holds per-channel parameter automation.
  - Step-sequencer steps and piano-roll notes are **the same data**; a step is just a short note.
- **Playlist.** The song timeline. Its tracks are **not tied to any instrument**: any clip can go
  on any track. There are three clip types:
  1. **Pattern clips** point at a Pattern. Every copy shares the same notes. "Make unique" clones
     the pattern.
  2. **Audio clips** point at an audio-clip channel, which points at a file on disk.
  3. **Automation clips** point at an automation channel, which holds an envelope.
- **Mixer.** Each channel is routed to a mixer track **by number** (Master by default).

**Suggested data model for our own program:**

```
Project     { tempo, timeSig, ppq, swing, channels[], patterns[], mixer{tracks[]}, arrangements[] }
Channel     { type, name, color, mixerTrack, plugin|sample, volume, pan, pitch, cutGroup, cutBy,
              swingMix, gate, shift, layerChildren[] }
Pattern     { id, name, color, length|auto, timeSig?, notesByChannel{ch: Note[]},
              eventsByChannel{ch: {param: Event[]}} }
Note        { pos(ticks), len, key, velocity, pan, release, modX, modY, finePitch,
              color(0..15 = MIDI channel), slide, porta, muted, group }
Arrangement { name, tracks[], clips[], markers[], timeSelection }
Clip        { kind(pattern|audio|automation), refId, track, start, length, offset, muted, color, group }
```

### Time
- **Positions are integer "ticks".**
- **Timebase (PPQ)** is a project setting. The default is **96 ticks per quarter note**; values
  up to 960 are reported **[uncertain on the full list]**.
- **A step is a 16th note**, so a step is 24 ticks at 96 PPQ.
- **Tempo:**
  - Set from the toolbar.
  - Can be automated: right-click → *Create automation clip*. Default range 60–180 BPM, which can
    be widened.
- **Time signature:**
  - Set per project.
  - Optionally set per pattern.
  - Changed along the timeline with time-signature markers.
- **Swing:**
  - A global swing knob.
  - Each channel has a "swing mix" amount.

### Pattern mode vs Song mode
- A **PAT / SONG** switch on the transport (shortcut **L**).
- **Pattern mode** loops the selected pattern.
- **Song mode** plays the Playlist.
- Recording and export follow whichever mode is active.

### Arrangements
- The Playlist can hold several **arrangements** (alternate versions of the song). They share the
  same patterns, channels and mixer.
- Arrangements can be cloned, renamed, deleted or merged (insert / merge / replace).

---

## 2.2 The main window

- **Toolbar and transport:**
  - Play, stop, record.
  - PAT/SONG switch, tempo, pattern selector, time display, master volume and pitch.
  - Metronome, wait-for-input, countdown, blend (overdub), loop-record.
  - CPU and memory meters.
  - "Multilink to controllers".
  - Window buttons.
- **Hint bar:**
  - Hover over anything to get a one-line explanation.
  - While you drag a knob, it shows the live value.
  - An extended hint panel is available.
- **Windows float, dock and resize.** Shortcuts:

  | Key | Window |
  |---|---|
  | **F5** | Playlist |
  | **F6** | Channel Rack |
  | **F7** | Piano Roll |
  | **F8** | Plugin Picker |
  | **F9** | Mixer |
  | Alt+F8 | Browser [background] |
  | F10 | Settings [background] |
  | F12 | Close all [background] |

- **Typing keyboard to piano** (Ctrl+T) turns the computer keyboard into a music keyboard.
- **Theme editor:**
  - Global hue, saturation, brightness, contrast and text.
  - Per-element colours.
  - Saved theme files.
- **Vector user interface:** scales smoothly to any screen size (since FL 12), with multi-touch
  support.

---

## 2.3 Channel Rack and Step Sequencer

**Purpose:** list every sound source, and show and edit the **selected pattern's** steps for each.

### One row per channel, left to right
- Mute/solo light: click mutes; right-click or Ctrl solos.
- Pan and volume knobs.
- **Mixer track number box:** drag or type to route the channel.
- Channel name button:
  - Click opens the instrument or its settings.
  - Right-click opens a menu: rename, colour, icon, graph editor, clone, delete, replace…
- Selection light.
- **Either** a **grid of 16th-note step buttons**, **or** a mini piano-roll preview if the
  channel has real melodic notes.

**Above the rows:** the pattern selector, the swing knob, and a "filter group" selector (show only
some channels). **Below the rows:** a "+" button to add a channel.

### Steps
- Left-click turns a step on. Right-click turns it off. Drag to paint several.
- Pattern length is automatic (the end of the last bar used) or set by hand, **1–512 steps**.
- **Fill tool:** fill every N steps, rotate, Euclidean-style fills.
- Ctrl+L sends the selected channels to free mixer tracks [background].

### Graph editor (under the steps)
- Bar editors for each step's:
  - velocity
  - release
  - fine pitch (±100 cents)
  - pan
  - Mod X / Mod Y
  - shift (sub-step timing)
- Ctrl+drag scales all bars together. Right-drag draws a ramp.

### Channel types
1. **Instrument plugin:** native or third-party, inside the plugin wrapper.
2. **Sampler:** the built-in sample player.
   - **Sample tab:** loop points, time-stretch mode, reverse, normalise, fades.
   - **Envelope/instrument tab:** envelopes for volume, cutoff, resonance, pitch and pan; LFOs;
     filter; arpeggiator.
3. **Audio-clip channel:** created automatically when audio is dropped on the Playlist.
4. **Automation channel:** backs automation clips.
5. **Layer:** plays its child channels together. Options: random or round-robin choice,
   crossfade, and key-range splits.

### Per-channel settings
- **Gate:** maximum note length.
- **Shift:** delay.
- **Swing mix.**
- **Cut / Cut by groups:** one channel silences another. The classic use is an open hi-hat choked
  by a closed one.
- Root note, pitch-bend range, polyphony and mono, portamento time, MIDI port, mixer track.

---

## 2.4 Piano Roll

**Purpose:** detailed note editing for **one channel** in the current pattern.

### Layout
- Piano keyboard on the left (click a key to hear it).
- Ruler and markers on top.
- Note grid in the middle.
- **Event lane** at the bottom (velocity, pan and so on).
- Toolbar: menu (F3), snap, tools, chord "stamp" selector, channel selector, scale highlighting,
  ghost-channel switch.

### Tools

| Tool | Key | What it does |
|---|---|---|
| Draw | P | Click to add a note (copying the last note's length and properties), drag to move, drag the right edge to resize |
| Paint | B | Drag to paint a run of notes |
| Delete | D | Delete (right-click also deletes in any tool) |
| Mute | T | Grey notes out |
| Slice | C | Cut notes at the snap position |
| Select | E | Box-select (Ctrl+drag works in any tool) |
| Zoom | Z | Drag to zoom |
| Playback | Y | Scrub to listen |

### Common actions

| Action | Shortcut |
|---|---|
| Bypass snap while dragging | Alt |
| Select all | Ctrl+A |
| Invert selection | Shift+I |
| Duplicate selection to the right | Ctrl+B |
| Quick quantise | Ctrl+Q |
| Quick legato | Ctrl+L |
| Transpose up/down an octave | Ctrl+↑/↓ [background] |

### Note properties
- Velocity.
- Release velocity.
- Pan.
- **Mod X / Mod Y:** FL's own per-note controllers, which can drive things like filter cutoff.
- Fine pitch (±100 cents).
- Time shift.
- **Colour:** 16 colours, which double as **MIDI channels 1–16**.
- **Slide:** glides the pitch of notes that are already playing toward this note.
- **Porta:** glides into this note from the previous one.

**How to edit them:**
- Double-click a note to open a properties dialog.
- Alt + mouse wheel edits the property under the mouse.
- Draw in the event lane.

### Helpers
- **Ghost notes:** other channels' notes are shown faintly. Since 2024 this includes overlapping
  Playlist patterns.
- **Scale highlighting:** choose a root and a scale (or **Auto**-detect) and the matching rows are
  shaded. Some tools snap to the scale.
- **Chord stamps:** pick a chord type, then click once to place the whole chord.
- **Chord Progression tool** (2024+): insert a progression in a key.
  - **2026 additions:** chord nudge (drag to swap chords), chord detection, voice-leading stamps.
- **Custom key labels** (2026): rename any piano key, which is useful for drum maps.

### Tools menu (each opens a dialog with live preview)

| Tool | Shortcut | What it does |
|---|---|---|
| Quantize | Alt+Q | Quantise starts and ends, with a strength setting and groove templates |
| Arpeggiate | Alt+A | Chords → arpeggios |
| Strum | Alt+S | Guitar-style chord offsets |
| Flam | Alt+F | Drum flams |
| Chop | Alt+U | Chop notes into a rhythm |
| Glue | | Join notes |
| Limit | Alt+K | Force notes into a range or scale |
| Scale levels | Alt+X | Scale or offset property values |
| LFO | Alt+O | Draw an LFO shape into note properties |
| Randomize | Alt+R | Generate or randomise notes within a scale |
| **Riff Machine** | Alt+E | Step-by-step melody generator (stages below) |
| Others | | Legato, Articulate, Flip, Fill each N steps, Claw machine |
| **Scripts** | | Python scripts that create or change notes (see [04-technical.md](04-technical.md)) |

Riff Machine stages: chords → arpeggiator → groove → lengths → levels and pan → mirror → fit →
finalise.

---

## 2.5 Playlist (the arranger)

### Layout
- **Picker panel** on the left lists every pattern, audio clip and automation clip; drag them in.
- **Track headers:** name, colour, mute, solo, height, lock, group fold.
- **Ruler:** markers and time selection.
- **Clip area.**
- **Toolbar:** snap, tools, arrangement selector, pattern selector.

### Tracks
- Up to **500** generic tracks.
- Tracks can be grouped and folded.
- Auto-colour options.
- **Consolidate track** renders a track to audio and mutes the originals.

### Tools
- Same as the Piano Roll, plus **Slip**, which moves the content inside a clip.
- Shift+G groups clips so they move together.

### Clip menu (on each clip)
- **Make unique:** gives this copy its own pattern or automation. For audio, it gets its own
  channel but the same file.
- **Make unique as sample:** also copies the audio file.
- **Chop:** split a pattern clip by its notes, or audio by its transients.
- **Consolidate selection:** render the selected clips to one audio file.
- Merge, split by channel, rename, colour, mute.
- **Audio clip properties (2025):** per-clip stretch mode, pitch and reverse.
  - Fades in and out with curve shape.
  - "Pitch selected clips ±1 semitone" and "Reverse" for a multi-clip selection.

### Markers and time selection
- **Time marker:** Alt+T. **Time-signature marker:** Shift+Alt+T.
- Right-click a marker to make it a loop, start, stop, pause, skip, jump or punch in/out point.
- **Time selection:** drag on the ruler. It limits playback and export, and drives
  insert-space and delete-space editing.

### Performance Mode (live clip launching)
- Playlist tracks become **clip launchers**. The area before the Start marker is the launch zone,
  and only one clip per track plays at a time.
- Each track has a launch mode, such as Retrigger, Hold & stop, Hold & motion, Latch, Random
  **[uncertain on the full list]**.
- Each track has a sync setting (step, beat, bar…).
- Time markers define pages on pad controllers such as the Akai Fire and Launchpad.

---

## 2.6 Mixer

### Structure
- **Master** + up to **500 insert tracks** + a **Current** track for analysing the selected
  channel.
- **Dynamic Mixer Tracks (2025):** tracks appear and disappear as needed, instead of a fixed bank.

### Each strip
- Name, colour, icon.
- Peak meters.
- Fader and pan.
- **Stereo separation** knob: left widens, right narrows toward mono.
- Mute and solo.
- **Arm-for-recording** button.
- Swap left/right and polarity invert [background].
- A **send switch to every other track**.

### Track properties panel (right side)
- **10 effect slots**, each with wet/dry mix and on/off.
- Audio input and output selectors.
- A built-in 3-band EQ after the slots.
- A latency setting.
- For more than 10 effects, send the track to another track, or use **Patcher** in one slot.

### Routing
- Any track can send to any number of other tracks (Master by default). Each send has a level
  knob on its cable.
- **"Sidechain to this track"** creates a send at **zero level**. It feeds a compressor's
  sidechain input without being heard.
- Routing view highlights a track's sources and destinations.
- Menu commands:
  - "Route selected to this track only".
  - "Unroute tracks routed to selected".

### Sound behaviour
- **Pan law:** circular; the centre sits about 3 dB below hard-panned.
- FL's pan **blends the two sides** rather than only turning one side down.

### Plugin delay compensation (PDC)
- **Automatic** from each plugin's reported latency.
- A manual per-track offset can be added on top.

### Mixer states
Saved presets of track layouts, routing, EQ and effects.

### Recording to disk
1. Pick an input on a track; this arms it.
2. Record. A WAV file is written and an audio clip appears in the Playlist.
3. The capture point is selectable, from the raw input through post-effects or post-fader.
4. Don't record on Master.
5. 2026 added a choice of bit depth: 16, 24 or 32-bit.

---

## 2.7 Browser, Plugin Manager and Plugin Picker

- **Browser:** a tree of:
  - the current project (patterns, channels, mixer, automation, **undo History**);
  - recent files;
  - the plugin database and presets;
  - sample packs and your own folders;
  - starred items;
  - FL Cloud sounds and backups.
- **Browser features:** tags, search, audition with auto-play, and drag-and-drop onto the Channel
  Rack, Playlist, Piano Roll or a mixer slot.
- **Plugin Manager:** scans VST/VST3/CLAP/AU folders and builds the **plugin database**, the list
  of installed plugins plus favourites sorted by category.
- **Plugin Picker** (F8): a full-screen grid of plugin icons; drag one where you want it.
- **Plugin wrapper:** a common header on every plugin window.
  - Presets with previous/next.
  - Randomise.
  - Detach.
  - Processing options: smart disable, threading, latency, MIDI ports, multiple outputs to the
    mixer.

---

## 2.8 Automation

FL Studio has **several** ways to move a knob over time:

1. **Automation clips:** right-click any control → *Create automation clip*.
   - The clip is an envelope of points with curve shapes: single curve, double curve, hold,
     stairs, smooth, pulse, wave.
   - It has a built-in LFO generator.
   - It sits on any Playlist track.
   - One clip can drive many targets.
2. **Pattern events:**
   - Knob moves are recorded *inside a pattern*.
   - Edit them with right-click → *Edit events* (the Event Editor).
   - They can be converted to an automation clip.
3. **Link to controller:**
   - Bind a control to a hardware knob (MIDI CC, with "move a knob to detect").
   - Or bind it to an internal source: an automation clip, Peak Controller, Formula Controller,
     LFO or envelope.
   - A mapping formula can reshape the response.
4. **Multilink:**
   1. Switch it on.
   2. Wiggle several on-screen controls.
   3. Wiggle several hardware knobs.
   4. They are paired in order.

   It can also create many automation clips at once.
5. **Controller plugins:**
   - **Peak Controller:** volume → control signal.
   - **Formula Controller:** maths.
   - **Envelope Controller:** note-triggered envelopes and LFOs.
   - **Patcher** macros.

---

## 2.9 Audio editing

### Stretch and pitch modes (Sampler and audio clips)

| Mode | Behaviour |
|---|---|
| Resample | Pitch and speed are linked, like a tape machine |
| Auto | Picks a mode using the sample's tempo info |
| élastique e3 Generic / Mono (vocals) / Slice | Offline, highest quality (FL licenses the zplane élastique engine) |
| Stretch / Stretch Pro | Real-time, follows tempo automation |

Controls: pitch (semitones and cents), time multiplier, and length in beats or seconds. Clips can
auto-fit to tempo changes.

### Edison (the built-in audio editor)
- A destructive editor that lives in a mixer slot or opens from a clip.
- Record on input, on play, or on a trigger.
- **Tools:**
  - Noise removal (learn a noise profile, then clean).
  - Blur, normalise, fade, reverse, EQ, convolution.
  - Loop tuning.
  - Beat detection and slicing (regions become slices in Slicex).
  - Spectral view.
  - Time and pitch tools.
  - AI de-clipper.
  - Stem separation.
- Drag the result back to the Playlist or Channel Rack.

### Audio Logger (2026)
Always records the last ~60 seconds of the master output so ideas are never lost.

---

## 2.10 Recording

- **Record filter** (right-click Record): choose audio, notes and/or automation. Audio goes into
  Edison or straight onto the Playlist.
- **Options:**
  - Countdown.
  - **Wait for input:** recording starts on the first note.
  - Metronome.
  - **Loop record.**
  - **Blend:** overdub. When it is off, each pass replaces the last.
  - Step-edit mode.
- **Takes:** loop record with blend off; earlier takes are muted.
- **Notes** are recorded into the selected pattern.
- **Knob moves** are recorded as events or as automation clips.

---

## 2.11 Export

- **Formats:**
  - WAV (16, 24 or 32-bit float)
  - MP3 (up to 320 kbps)
  - OGG
  - FLAC
  - MIDI
- **What to export:** pattern or song; the whole song or the loop/selection.
- **Tail handling:** leave, cut or wrap the remainder.
- **Quality:** sample rate, resampling quality (up to 512-point sinc), dithering, and "HQ for all
  plugins".
- **Stems:**
  - "Split mixer tracks" writes one WAV per mixer track.
  - "Split playlist tracks" also exists.
- **Command-line rendering:** for example `/Emp3,ogg,wav`.
- **Online extras through FL Cloud:** AI mastering, distribution to streaming services, stem
  separation, Remix a Song.

---

## 2.12 Undo, templates, Loop Starter, Gopher

- **Undo:**

  | Action | Shortcut |
  |---|---|
  | Undo (also toggles redo of the last step) | Ctrl+Z |
  | Step back through history | Ctrl+Alt+Z |

  - The full history list is in the Browser; click an entry to jump to it.
  - The number of levels is configurable.
- **Templates:** File → New from template.
- **Welcome window** (2026): includes "Remix a song".
- **Loop Starter** (2025+): pick a genre and get a ready-made set of loops in the Channel Rack.
- **Gopher:** an AI side panel. In 2026 it can act on the project ("lower track 3 by 3 dB").
  - Building this for our own program needs a clean internal **project API** that the assistant
    can call.
  - FL's Python scripting modules (mixer, channels, patterns, playlist…) are a good model.

---

## 2.13 Keyboard shortcut highlights

| Keys | Action |
|---|---|
| Space | Play / stop |
| L | Pattern / Song |
| F5–F9 | Playlist, Channel Rack, Piano Roll, Plugin Picker, Mixer |
| Ctrl+Z / Ctrl+Alt+Z | Undo / history |
| Ctrl+T | Computer keyboard as piano |
| Ctrl+B | Duplicate selection |
| Ctrl+Q / Alt+Q | Quick quantise / quantise tool |
| Alt+A S F K X O R E | Arpeggiate, Strum, Flam, Limit, Scale levels, LFO, Randomize, Riff Machine |
| P B D T C E Z Y | Draw, Paint, Delete, Mute, Slice, Select, Zoom, Playback |
| Shift+G | Group clips |
| Alt+T / Shift+Alt+T | Time marker / time-signature marker |
| Right-click | Delete in grids |
| Alt+drag | Ignore snap |

The same key can do different things depending on which window has focus.

---

## Sources

**Image-Line online manual:**
- Workflow and Channel Rack:
  - [Workflow basics](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_workflow.htm)
  - [Channel Rack](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/channelrack.htm)
  - [Channel misc settings](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/chansettings_misc.htm)
  - [Layer](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/chansettings_layer.htm)
  - [Sampler](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/chansettings_sampler.htm)
- Piano Roll:
  - [Piano Roll](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/pianoroll.htm)
  - [Piano Roll menu](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/pianoroll_menu.htm)
  - [Riff Machine](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/pianoroll_riff.htm)
  - [Piano Roll scripting](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/pianoroll_scripting_api.htm)
- Playlist:
  - [Playlist](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/playlist.htm)
  - [Audio clips](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/playlist_audioclip.htm)
  - [Automation clips](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/playlist_automationclip.htm)
  - [Performance mode](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/playlist_performance.htm)
- Mixer:
  - [Mixer](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer.htm)
  - [Track properties](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer_trackprops.htm)
  - [Routing](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/mixer_iorouting.htm)
  - [PDC made simple](https://www.image-line.com/fl-studio-news/plugin-delay-compensation-pdc-made-simple)
- Automation:
  - [Event editor](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/automation_eventeditor.htm)
  - [Linking](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/automation_linking.htm)
- Audio, recording and export:
  - [Recording audio](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/recording_audio.htm)
  - [Stretch modes](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/editortool_stretch.htm)
  - [Edison](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/plugins/Edison_3.htm)
  - [Save/export formats](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/fformats_save_export.htm)
- Browser, settings and shortcuts:
  - [Browser](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/browser.htm)
  - [Project settings](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/songsettings_settings.htm)
  - [Themes](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/envsettings_themes.htm)
  - [Shortcuts](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_shortcuts.htm)

**Other:**
- [FL Studio 2025 what's new](https://www.image-line.com/fl-studio-news/fl-studio-2025-whats-new-2)
- [MusicTech: FL Studio 2025 first look](https://musictech.com/news/gear/fl-studio-2025-first-look-new-features-i-love-what-needs-refining)
- [MusicTech: FL Studio 2024 review](https://musictech.com/reviews/fl-studio-2024-review/)
- [PyFLP project model](https://pyflp.readthedocs.io/en/latest/reference/project.html)
