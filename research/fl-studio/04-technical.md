# 4. Under the hood: technology, file formats, extensibility, competitors

> Part of the [FL Studio research pack](README.md). Researched 5 November 2026.
>
> **Confidence marks.**
> - **[confirmed]**: an official or primary source.
> - **[secondary]**: reputable press or community sources.
> - **[uncertain]**: background knowledge, or the sources conflict.

---

## 4.1 Platforms and requirements

| | Windows | macOS |
|---|---|---|
| **OS** | Windows 10 or 11 (x64 only; **no Windows on ARM**) | 10.15 Catalina or later |
| **Chips** | Intel/AMD, 2 GHz+ | Intel and **native Apple Silicon** (since 20.8.4, 2021) |
| **Minimum** | 4 GB RAM, 4 GB disk | same |
| **Recommended** | Quad-core, 8–16 GB RAM, SSD | macOS 13+ on Apple Silicon |
| **Linux** | **None** | n/a |

- **Plugin bridging on Apple Silicon:** in native mode FL **bridges** old Intel VST plugins in a
  separate process. AU plugins are handled by macOS itself [confirmed].
- **Platform parity:** the two platforms are now nearly equal. A few legacy plugins are
  Windows-only [secondary].

## 4.2 How it is built

- **Language:** mostly **Delphi (Object Pascal)** with some assembly. Embarcadero (the maker of
  Delphi) published a case study on Image-Line [secondary, strong].
  - The macOS build was historically compiled with **Free Pascal**. Its current toolchain is
    [uncertain].
  - Newer parts, such as the plugin-format wrappers, embedded Python and web panels, are probably
    other languages [uncertain].
- **User interface:**
  - Its **own widget toolkit**, not native OS controls, so it looks the same on both systems.
  - Fully **vector-drawn** since FL 12 (2015), so it scales to 4K/8K.
  - Multi-touch.
- **Web panels:** Gopher and the FL Cloud panels look like **embedded web views** [secondary].

## 4.3 Audio engine

- **Drivers:**
  - **Windows:**
    - ASIO (recommended).
    - **FL Studio ASIO:** Image-Line's own ASIO driver on top of WASAPI. It lets other apps share
      the sound card.
    - WASAPI/DirectSound as a fallback.
  - **macOS:** CoreAudio.
    - Since 2026, separate input and output devices can be chosen.
    - Since 2026, **Audio Workgroups** for steadier real-time threads on Apple Silicon.
- **Latency:**
  - Image-Line advises at least a 10 ms buffer.
  - A "triple buffer" option doubles latency to gain stability.
  - Recording latency compensation is separate.
- **Processing:**
  - 32-bit float internally [uncertain whether any mixing is 64-bit].
  - The sample rate follows the driver (44.1–192 kHz).
- **Multi-core:** options for "multithreaded generator processing" and "multithreaded mixer
  processing" [confirmed option names].
- **Smart Disable** automatically bypasses plugins that are outputting silence, to save CPU.
- **PDC:**
  - Automatic, from plugin-reported latency, plus manual offsets.
  - Plugin developers report FL's PDC has quirks. **Getting PDC right is a chance for a competitor
    to do better.**
- **Timing:**
  - PPQ default 96, maximum reportedly 960 [uncertain].
  - 2026 reduced MIDI-recording jitter.
- **Render quality:**
  - Interpolation from linear up to **512-point sinc**.
  - "HQ for all plugins".
  - Dither.
  - Live playback quality and render quality are set separately.

## 4.4 Plugin hosting

| Format / feature | Status |
|---|---|
| VST2 | Yes; 32-bit plugins bridged on Windows [confirmed] |
| VST3 | Yes [confirmed] |
| AU | Yes, macOS [confirmed] |
| **CLAP** | **Yes, since FL Studio 2024** (July 2024) [confirmed] |
| AAX | No (Pro Tools only) |
| **ARA2** (Melodyne-style integration) | **No.** Staff said "on the list" (Nov 2024); still missing as of Sept 2026 [secondary, strong] |
| Crash sandboxing per plugin | **No** (unlike Bitwig). Only bridged plugins run in their own process [secondary] |
| MPE | Reported for VST/CLAP plugins in recent versions [uncertain] |

- **Fruity Wrapper** hosts every third-party plugin. It provides:
  - presets;
  - processing options (smart disable, threading, fixed buffers, latency override);
  - MIDI port mapping;
  - a bridging switch;
  - "link parameter to controller".
- **Plugin Manager** scans plugins into a categorised database of `.fst` shortcut presets used by
  the Browser and Picker.

## 4.5 File formats

### `.flp`: the project file (binary, reverse-engineered by the open-source PyFLP project)

```
"FLhd" | u32 size (=6) | i16 format | u16 channel count | u16 PPQ
"FLdt" | u32 size      | event, event, event, ...
```

Each **event** is a 1-byte ID followed by a value. The ID decides the value's size:

| Event ID | Value size |
|---|---|
| 0–63 | 1 byte |
| 64–127 | 2 bytes |
| 128–191 | 4 bytes (integer or float) |
| 192–255 | a variable-length size, then that many bytes (text in UTF-16, structs, nested data) |

- Plugin state is stored as opaque blobs inside wrapper structs.
- The format is undocumented by Image-Line.
- **Open parsers:** PyFLP (Python, GPLv3, slow maintenance since about 2023) and Monad.FLParser
  (C#).
- A competitor could realistically **import** tempo, patterns, notes, playlist clips and mixer
  routing. Plugin sounds would be "best effort".

### Other formats

| Extension | What it is |
|---|---|
| `.fst` | Channel or plugin preset (same event encoding) |
| `.fsc` | Score (notes) or automation data |
| `.fnv` | Mixer track state |
| `.zip` | "Zipped loop package": the project plus all its samples |

- **Import:**
  - WAV, AIFF, MP3, OGG, FLAC.
  - MIDI.
  - REX (with the REX library).
  - SF2 SoundFonts.
- **Export:**
  - WAV (16/24/32f), MP3, OGG, FLAC, MIDI.
  - Stems ("split mixer tracks").
  - "Project data files": a folder with every audio file used.

## 4.6 Extensibility (scripting and hardware)

### Python MIDI-controller scripting
- Added in FL 20.7. It uses an embedded, cut-down Python; recent builds report Python 3.12
  [secondary].
- **Event-driven:** FL calls the script's functions (`OnInit`, `OnMidiMsg`, `OnNoteOn`,
  `OnControlChange`, `OnRefresh`, `OnIdle`, `OnProjectLoad`…), and the script calls back into
  FL's API.
- **API modules:**
  - `transport`, `mixer`, `channels`, `patterns`, `playlist`, `arrangement`;
  - `plugins` (read and set any plugin parameter);
  - `ui`, `device`, `general`, `midi`, `screen` (the Akai Fire display), `utils`,
    `launchMapPages`.

  That is about **427 functions across 14 modules**. Each function is tagged with the API version
  that introduced it.
- **Script location:** `Documents/Image-Line/FL Studio/Settings/Hardware/<device>/device_<name>.py`.
- **Official type stubs:** `IL-Group/FL-Studio-API-Stubs` (LGPL-3.0). These are now unmaintained
  in favour of `MaddyGuthridge/FL-Studio-API-Stubs`.
- **Community tools:**
  - **Flapi** drives FL from outside over a virtual MIDI cable.
  - Several **MCP servers** already let AI models control FL Studio. This is the same idea as
    Gopher.

### Other scripting
- **Piano Roll scripts:** Python, `flpianoroll` module. They read and write `score.notes`, and
  can build a small dialog UI with `ScriptDialog`. Gopher can now write these.
- **Edison scripts:** `enveditor` module, for audio editing.
- **VFX Script:** Gopher can generate this code too.

### Modular tools
- Patcher (a plugin graph).
- Formula Controller, Peak Controller, LFO.
- Control Surface (custom panels).
- Multilink.

### Hardware
- **Akai Fire:** made for FL; scripted in Python.
- **Novation FLkey.**
- **Native Instruments Kontrol S MK3:** via scripts.
- **Mackie Control / HUI.**
- Dozens of vendor scripts, updated with "Update MIDI scripts".
- **FL Studio Remote:** a phone/tablet control app.

## 4.7 Cloud and AI

| Feature | Since | How it works |
|---|---|---|
| **Stem separation** | 21.2 (2023) | Runs **on your computer** (the model downloads on first use). Splits into drums, bass, instruments, vocals. Image-Line will not say which model it uses; it may be Demucs-based [uncertain] |
| **AI mastering** | 21.2; more controls 2024 | Cloud. Genre presets plus manual bass/high EQ, warmth, exciter, multiband, width |
| **Distribution** | 21.2 | Release to streaming services (reported DistroKid partnership) |
| **FL Cloud Plugins** | 2024 | Rent third-party plugins (NI, UVI, Melda, Sonible, Baby Audio…) by tier |
| **Loop Starter** | 2025 | Builds a genre loop set from FL Cloud sounds, matched to tempo and key; "find similar samples" |
| **Gopher** | 2025 (chat), 2026 (actions) | Cloud LLM (vendor undisclosed) in a web panel, trained on the manual. In 2026 it can operate the project. Image-Line says it doesn't train on user projects |
| **Chord tools** | 2024 / 2026 | Described as AI-assisted; method undisclosed |
| **Project backup** | 2026 | Encrypted cloud copies on save; 500 MB / 5 GB / 1 TB by tier |
| **Real-time collaboration** | n/a | **None.** Projects are shared as files or zips |

## 4.8 Competitors

| Program | Platforms | What it is known for |
|---|---|---|
| **FL Studio 2026** | Win, Mac | Pattern workflow, step sequencer, best-in-class Piano Roll, lifetime free updates, hip-hop/EDM user base, huge tutorial ecosystem |
| Ableton Live 12 | Win, Mac | Session-view clip launching, Max for Live, live performance |
| Logic Pro 11 | Mac, iPad | Apple-only, free updates, Stem Splitter, Session Players, ARA2 |
| Bitwig Studio | Win, Mac, **Linux** | Per-plugin crash sandboxing, "The Grid" modular, CLAP pioneer |
| Cubase | Win, Mac | Deep MIDI and scoring, ARA2, owner of VST |
| Reaper 7 | Win, Mac, Linux | Cheap, tiny, extremely scriptable, ARA2, CLAP |
| **LMMS** | Win, Mac, Linux | **Free, open-source FL-style program** (C++/Qt, GPL): Song Editor, Beat+Bassline editor, Piano Roll. Limited audio recording |

Exact current version numbers for these competitors are [uncertain].

**FL's known weak spots (openings for us):**
- No ARA.
- Weaker audio comping and editing.
- No plugin crash sandboxing.
- No Linux version.
- PDC quirks.
- No real-time collaboration.

## 4.9 Building blocks we could use

| Component | What for | License (verify before use) |
|---|---|---|
| **JUCE 8** | Cross-platform audio, UI and plugin hosting | AGPLv3 or commercial (free tier for small revenue) [secondary] |
| **Tracktion Engine** | A whole DAW engine (timeline, clips, plugins, rendering) on JUCE | GPLv3 or commercial [confirmed] |
| **VST3 SDK** | Host VST3 plugins | **MIT since VST 3.8 (Oct 2025)** [confirmed] |
| **ASIO SDK** | Low-latency Windows audio | GPLv3 or a Steinberg agreement (since Oct 2025) [confirmed] |
| **CLAP SDK** | Modern open plugin format | MIT [uncertain] |
| PortAudio / RtAudio / miniaudio | Audio device input and output | MIT-style [uncertain] |
| libsndfile | Read and write WAV/AIFF/FLAC/OGG/MP3 | LGPL-2.1 (link dynamically) [uncertain] |
| libsamplerate / r8brain | High-quality resampling | BSD / MIT [uncertain] |
| Rubber Band | Time-stretch and pitch-shift | GPL or commercial [uncertain] |
| Signalsmith Stretch | Time-stretch alternative | MIT [uncertain] |
| **Demucs v4** / Spleeter | Stem separation | MIT (check the model-weight terms) [uncertain] |
| ONNX Runtime | Run AI models locally | MIT [uncertain] |
| LAME, minimp3, dr_libs, libFLAC, libvorbis | Codecs | Various permissive / LGPL [uncertain] |
| Embedded CPython / pybind11 | A scripting layer like FL's | PSF / BSD [uncertain] |
| Ableton Link | Tempo sync with other apps and devices | GPLv2 or commercial [uncertain] |
| LMMS, Ardour, Zrythm source | Reference designs for pattern and linear DAWs | GPL / AGPL: reference only if we go closed-source |

**Licensing in one paragraph.**
- **No licensing burden:** VST3 and CLAP are both MIT now.
- **Needs a deal or a GPL product:** ASIO (a Steinberg agreement, unless the product is GPL),
  JUCE and Tracktion (paid licences for closed products), Rubber Band (commercial licence, or
  use Signalsmith instead).
- **Usable with care:** libsndfile must be dynamically linked. Demucs is usable, but check the
  model-weight terms.

**Web option.** If we build for the **web browser** instead, the Web Audio API plus
AudioWorklet, WebMIDI and WebAssembly DSP cover the core needs. VST plugin hosting would not be
possible in a browser.

## Sources

**Image-Line**
- Manual pages:
  - [Audio settings](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/envsettings_audio.htm)
  - [External plugins](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm)
  - [Save/export formats](https://www.image-line.com/fl-studio-learning-content/fl-studio-online-manual/html/fformats_save_export.htm)
  - [MIDI scripting](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/midi_scripting.htm)
  - [What's new](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_new.htm)
- Support and comparison pages:
  - [System requirements](https://support.image-line.com/action/knowledgebase?ans=82)
  - [Windows vs Mac](https://www.image-line.com/windows-vs-mac-fl-studio-versions/)
- News and forum:
  - [PDC made simple](https://www.image-line.com/fl-studio-news/plugin-delay-compensation-pdc-made-simple)
  - [FL Cloud 2026](https://www.image-line.com/news/whats-new-in-fl-cloud26)
  - [24.1.1 / CLAP](https://forum.image-line.com/viewtopic.php?p=1955986)
  - [ARA "on the list"](https://forum.image-line.com/viewtopic.php?p=1959571)
  - [Stem model question](https://forum.image-line.com/viewtopic.php?p=1883379)
- [IL-Group API stubs](https://github.com/IL-Group/FL-Studio-API-Stubs)

**Format and tools**
- [PyFLP](https://github.com/demberto/PyFLP)
- [PyFLP FLP format doc](https://github.com/demberto/PyFLP/blob/master/docs/architecture/flp-format.rst)
- [MaddyGuthridge stubs](https://github.com/MaddyGuthridge/FL-Studio-API-Stubs)
- [Flapi](https://github.com/MaddyGuthridge/Flapi)

**Press**
- [KVR: FL Studio 2024 and CLAP](https://www.kvraudio.com/news/image-line-releases-fl-studio-2024---plugins-to-fl-cloud-new-ai-features-effects-workflow-improvements-clap-support-61173)
- [CDM: 20.9 and Apple M1](https://cdm.link/2021/12/fl-studio-20-9-drops-with-new-effects-undo-everything-expanded-apple-m1-plug-in-support/)
- [Embarcadero case study](https://embarcadero.com/case-study/image-line-software-case-study/download)
- [Wikipedia](https://en.wikipedia.org/wiki/FL_Studio)
- [Gearnews: ARA across DAWs](https://www.gearnews.com/?p=174770)

**Licensing**
- [Steinberg VST 3.8 MIT press release](https://ocl-steinberg-live.steinberg.net/_storage/asset/819253/storage/master/Press%20Release%20-%202025-10-29%20-%20VST%203.8%20-%20EN.pdf)
- [LibreArts: VST3 and ASIO relicensing](https://librearts.org/2025/11/steinberg-relicenses-vst3-and-asio/)
- [Tracktion Engine](https://github.com/Tracktion/tracktion_engine/)
- [JUCE](https://en.wikipedia.org/wiki/JUCE)
