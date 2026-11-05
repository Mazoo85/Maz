# 1. What's new in FL Studio 2026 (and how it got here)

> Part of the [FL Studio research pack](README.md). Researched 5 November 2026.
>
> **Source quality.** Image-Line's own pages could not be opened directly from the research
> environment, so this is built from search-engine excerpts of them (forum release posts, the
> online manual's "What's new" page, the 2026 release page) plus press coverage.
> **[secondary]** = only third-party sources say so. **[uncertain]** = sources disagree or it
> could not be confirmed.
>
> Image-Line's forum "Latest Release" posts repeat the whole cumulative 2026 feature list above
> each build's own notes, so headline features are counted under the 2026 major release and only
> build-specific items are listed per point release.

---

## 1.1 Overview

| | |
|---|---|
| **Version** | FL Studio 2026 (internal 26.1) |
| **Released** | 7 July 2026 (public Beta 1 on 30 March 2026, Beta 11 on 18 June, RC 1 on 26 June) |
| **Latest build** | **2026.1.7b**, 12 October 2026 (Windows 26.1.7.5654, macOS 26.1.7.5421) |
| **Cost to existing owners** | Free (Lifetime Free Updates); you must update your license after installing |
| **Editions** | Fruity, Producer, Signature, All Plugins (see [03-editions-and-plugins.md](03-editions-and-plugins.md)) |

**Headline features:**
- Rebuilt FLEX
- FL Cloud project backup
- Gopher assistant that can act inside a project
- Transmitter plugin
- Eight free Core Series sound packs
- Remix a Song
- Audio Logger
- Note Arpeggiator
- Chord Progression tool and chord detection improvements
- More accurate MIDI recording
- macOS audio changes

---

## 1.2 The 2026 features in detail

### Rebuilt FLEX
- **What it is:** Image-Line calls it a "completely rebuilt version of FLEX with a new engine,
  redesigned interface and expanded sound library". It is the first full rework since FLEX
  launched in FL Studio 20.5.
- **For the user:**
  - A new browser with a **Library** page (packs you have) and a **Store** page (new packs).
  - Search across all packs, with genre filters.
  - A redesigned Play window **[secondary]**.
  - Full-size preset artwork **[secondary]**.
- **Performance:** "up to 50% lower CPU" and faster loading of large multi-sample patches. The
  figure applies to the new second-generation packs, not to every preset **[secondary]**.
- **Compatibility:** old packs still work. Core Series packs, and later packs, need FL Studio
  2026 or newer.
- **Edition:** all editions.
- **Unchanged:** the 8-macro design.

### Core Series packs
- Eight free packs with 200+ presets in total, built on the new FLEX engine, for every edition.
- Reported names: Core Synths, Bass, Brass, Winds, Strings, Guitars, Pianos, 808s **[secondary]**.

### Gopher, the AI assistant (now able to act)
- **2025:** introduced as a multilingual chat assistant, "a Large Language Model based on the
  Manual and Knowledge Base", that answers questions.
- **2025.2:** became much better at writing Piano Roll scripts.
- **2026:** it "can now control some features in FL Studio and plugins" (labelled
  **experimental**, with the list of controls still growing).
- **Actions reported** (mostly **[secondary]**):
  - Open the main windows.
  - Change the project tempo.
  - Manage channels in the Channel Rack.
  - Organise Playlist and Mixer tracks.
  - Route a channel to a mixer track, or one mixer track to another.
  - Set a track's level and pan.
  - Add a named effect to a named mixer slot.
  - Change plugin parameters, but only ones the plugin publishes to FL Studio.
  - Write and run Piano Roll (Python) scripts.
  - Generate VFX Script code.
  - Quantize notes and fill sequencer steps.
- **How you use it:** type a request in a chat panel. Actions run only after the user allows them
  **[secondary]**. It does not compose finished songs.
- **Privacy and AI provider:**
  - Image-Line says Gopher doesn't train on user data.
  - The text of requests is logged to improve it.
  - The AI provider is not disclosed. It needs internet, so it is almost certainly cloud-hosted
    **[uncertain]**.
- **Edition:** no restriction found **[uncertain]**.

### Remix a Song
- **Where:** the Tools menu, and a one-click option in the Welcome window.
- **What it does:**
  1. You pick an audio file.
  2. FL Studio detects its tempo and sets the project BPM to match. This option was renamed
     **"Auto tempo"** in 2026.1.7.
  3. It runs AI stem separation (drums, bass, instruments, vocals).
  4. It places each stem on its own Playlist track.
- **Under the hood:** the on-device stem separator added in 21.2. The model downloads the first
  time you use it **[secondary]**.
- **Edition:** likely Producer or higher, like stem separation in general **[secondary]**.

### Audio Logger
- An always-on recorder of the **Master** output: the audio version of the old MIDI Logger.
- It keeps a rolling buffer, reported as about 60 seconds **[secondary]**.
- When something good happens that you weren't recording, you "dump" the buffer into
  **Edison** and keep it. Dumps go to the *Recorded* folder.
- The Mixer's old "Open audio logger" entry was renamed "Open audio editor and record" to avoid
  confusion.

### FL Cloud project backup (and FL Cloud in general)
- **New:** back up projects to FL Cloud Storage, optionally on every save (a Project settings
  option). Backups show in the Browser.
- Storage is private, encrypted, and never used to train AI.
- **Quotas:** Free 500 MB, Plus 5 GB, Pro 1 TB.
- **The FL Cloud subscription** (pre-2026, for context):
  - 2M+ royalty-free samples.
  - AI mastering (21.2; extra controls in 2024).
  - Distribution to streaming services (21.2).
  - FL Cloud Plugins, third-party plugins by tier (2024).
  - Loop Starter gets a bigger library for subscribers.
- **Tiers:**
  - Free: about 10 plugins.
  - Plus: 18 plugins, 5 GB, mastering.
  - Pro: 90+ plugins, mastering and distribution, about **$149 a year** **[secondary]**.
  - FL Cloud does **not** include FL Studio itself.
- **Rent-to-Own Plus (about September 2026):** pay monthly toward owning FL Studio, with FL Cloud
  Plus included. From about $20 a month for Producer Edition.

### Transmitter (new effect)
- Splits any signal into **Transient** and **Sustain** streams.
- Each stream goes to **its own mixer track**, so the attack and the body of a sound can be
  processed separately.
- **Edition:** All Plugins Edition only.

### Chords and the Piano Roll
- **Chord Progression tool** (from 2024):
  - Now has a Tools-menu entry.
  - **Chord nudge** lets you drag to swap or slide chords.
  - You can preview chords from the computer keyboard.
  - Right-click the Piano Roll's Tools icon to open it. The Riff Machine moved to middle-click.
- **Chord detection**, with a toolbar option in 2026.1.2.
- **Chord Stamp** has top-down and bottom-up voice-leading modes **[secondary]**.
- **Custom key labels:** rename any piano key; label presets can be sorted into folders.
- **Note Arpeggiator:** "Turn incoming notes into rhythmic, repeating, and re-pitched patterns".

### Timing and audio I/O
- **MIDI recording:** "Incoming MIDI events are now recorded more accurately with reduced jitter,
  including when tempo changes during playback."
- **macOS:** separate input and output devices can be chosen; before, an aggregate device was
  needed.
- **macOS, Apple Silicon:** uses Apple's **Audio Workgroups** API, so audio threads get real-time
  scheduling alongside the system audio thread and there are fewer dropouts **[partly
  secondary]**.

### Smaller changes
- **Gross Beat:** a new option stops MIDI notes triggering slots while Gross Beat is focused, and
  small tempo changes no longer reset its delay line.
- The Missing Samples dialog names the missing files and plugins.
- Mixer disk recording lets you choose the bit depth (16, 24 or 32-bit).
- New MIDI scripting functions **[uncertain]**.

---

## 1.3 Point releases

| Version | Date | Notable build-specific changes |
|---|---|---|
| 2026 (26.1) | 7 Jul 2026 | Major release (above). |
| 2026.1.1 | 13 Jul 2026 | Missing-samples names; backup count can go down to 1; Gross Beat tempo handling; crash fixes. |
| 2026.1.2 | 20 Jul 2026 | Cloud backup on save is optional; renamable piano keys; 16- and 32-bit disk recording; chord-detection toolbar option; better Loop Starter cloud filters; Transmitter "Steady" renamed "Sustain". |
| 2026.1.3 | 27 Jul 2026 | Fixed a freeze when MIDI scripts call FL functions; fixed a Wave Traveller crash; one-click license renewal in FLEX; startup, shutdown, memory-leak and Browser fixes **[partly secondary]**. |
| 2026.1.4 | 10 Aug 2026 | Possibly new FLEX analysis modes and MIDI-script functions **[uncertain]**. |
| 2026.1.5 | 24 Aug 2026 | Grouped-track collapse menu; Mixer reset gesture; 24-bit disk recording; Delete in Channel Settings; fixed zero-length notes from **FL Studio Remote**. |
| 2026.1.6 | 7 Sep 2026 | A notification when a new MIDI controller is connected; fixes. |
| 2026.1.7 | 1 Oct 2026 | "Change project tempo" renamed "Auto tempo"; fixes. |
| **2026.1.7b** | **12 Oct 2026** | Same installers as RC2; fixed the Fire edition not unlocking. **Newest build.** |

There are hints of a 2026.2 beta with better chord-generation data **[uncertain]**.

---

## 1.4 How FL Studio got here (recent history)

| Version | When | Headline additions |
|---|---|---|
| **21.0** | 2022 | Multi-language interface, among other changes. |
| **21.1** | Aug 2023 | Snap to Scale; **Python Piano Roll scripting**; coloured waveforms; Hyper Chorus; AI Declipper in Edison. |
| **21.2** | late 2023 | **AI stem separation** (Producer and up); **FL Cloud** launched (sounds, AI mastering, distribution); Kepler synth. |
| **2024** | Jul 2024 | First year-named version. FL Cloud Plugins; mastering controls; Kepler Exo synth; Chord Progression tool; Note Repeat; Low Lifter, Spreader, vintage chorus; Edison declipper and stems. |
| **2025** | Jul 2025 | **Gopher** chat assistant; **Loop Starter**; **Dynamic Mixer Tracks** (up to 500); Emphasis limiter; Mobile Rack (FL Studio Mobile instruments and effects on desktop); stems to separate mixer tracks. |
| **2025.2** | Nov 2025 | Gopher writes better Piano Roll scripts; Chord Stamp update; Kepler Exo upgrades; Fruity Slicer 2 and Emphasizer in all editions. |
| **2026** | Jul 2026 | Everything in section 1.2. |

**The pattern:** each year Image-Line ships one large, free yearly release, then a stream of
point releases every 1–3 weeks. Recent priorities:
- AI assistance (stems, mastering, Gopher).
- Cloud services as the recurring revenue stream.
- Quality of life for beat-making.

---

## Sources

**Image-Line (official)**
- Release posts:
  - [2026 release](https://forum.image-line.com/viewtopic.php?t=341294)
  - [2026.1.1](https://forum.image-line.com/viewtopic.php?t=341401)
  - [2026.1.2](https://forum.image-line.com/viewtopic.php?t=341478)
  - [2026.1.3](https://forum.image-line.com/viewtopic.php?t=341668)
  - [2026.1.4](https://forum.image-line.com/viewtopic.php?t=341850)
  - [2026.1.5](https://forum.image-line.com/viewtopic.php?t=342068)
  - [2026.1.6](https://forum.image-line.com/viewtopic.php?t=342331)
  - [2026.1.7](https://forum.image-line.com/viewtopic.php?t=342646)
  - [2026.1.7b](https://forum.image-line.com/viewtopic.php?t=342829)
  - [Beta 1](https://forum.image-line.com/viewtopic.php?t=339865)
  - [Beta 4](https://forum.image-line.com/viewtopic.php?t=340196)
  - [Beta 11](https://forum.image-line.com/viewtopic.php?t=341017)
  - [RC 1](https://forum.image-line.com/viewtopic.php?t=341110)
- [Manual: What's new](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_new.htm)
- [2026 release page](https://www.image-line.com/fl-studio/release/2026)
- [All-new FLEX](https://www.image-line.com/news/all-new-flex)
- [FLEX](https://www.image-line.com/flex)
- [Rent-to-Own Plus](https://www.image-line.com/news/announcing-rent-to-own-plus)
- What's new in earlier versions:
  - [2025](https://www.image-line.com/fl-studio-news/fl-studio-2025-whats-new-2)
  - [2025.2](https://www.image-line.com/news/fl-studio-2025-2)
  - [2024](https://www.image-line.com/fl-studio-news/fl-studio-2024-whats-new)
  - [21.2](https://www.image-line.com/fl-studio-news/fl-studio-21-2-whats-new)
  - [21.1](https://www.image-line.com/fl-studio-news/fl-studio-211-whats-new/)

**Press and secondary**
- [AudioTechnology](https://www.audiotechnology.com/by-brand/fl-studio/fl-studio-2026-released)
- [MusicTech](https://musictech.com/news/gear/fl-studio-2026-everything-you-need-to-know/)
- [MusicRadar: Gopher control](https://www.musicradar.com/music-tech/you-can-now-control-fl-studio-with-an-ai-chatbot-from-inside-the-daw)
- [Rekkerd](https://rekkerd.org/image-line-launches-fl-studio-2026-redesigned-flex-secure-cloud-project-backup-smart-assistance-more/)
- [AlternativeTo](https://alternativeto.net/news/2026/7/fl-studio-2026-brings-rebuilt-flex-fl-cloud-project-storage-and-new-transmitter-plugin/)
- [Attack Magazine](https://www.attackmagazine.com/news/fl-studio-2026-adds-redesigned-flex-cloud-backup-and-built-in-smart-assistance/)
- [KnowsAudio: Audio Logger](https://www.knowsaudio.com/fl-studio-2026-beta-2-introduces-always-on-audio-logger/)
- [flstudiopro: 2026](https://flstudiopro.com/fl-studio-2026/)
- [flstudiopro: Gopher](https://flstudiopro.com/fl-studio-gopher-ai/)
- [RouteNote: Gopher](https://routenote.com/blog/fl-studio-2026-gopher-ai-production-assistant/)
- [Neowin: 2026.1.3](https://www.neowin.net/software/fl-studio-26135570/)
- [Wikipedia](https://en.wikipedia.org/wiki/FL_Studio)
