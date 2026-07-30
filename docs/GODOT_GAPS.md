# What Maz Lacks Compared to Godot — an Honest Gap List

This is a deliberately honest, self-critical companion to `GODOT_PARITY.md`. That document
records where Maz meets or beats Godot feature-by-feature. This one does the opposite: it lists,
as completely and truthfully as possible, everything Godot has that Maz does **not** — or has only
as untested code. The engine's guiding rule is *never falsely claim total superiority*, and this
file is where that rule is kept honest.

**Short version:** Maz has enormous *breadth of game-logic code* — 495+ milestones of RPG systems,
physics math, audio DSP, AI, animation, and data structures, much of it CPU-unit-tested to a level
Godot's own source does not match. But Godot is a **complete, shipping product** used to release
thousands of real games across every platform, and Maz is not. The gaps below are mostly not
"missing algorithms" — they are the things that make an engine a usable product: a renderer proven
across *many* GPUs, broad platform reach, a more polished editor, working networking, and a decade of
ecosystem. (Note: the renderer, audio output, the editor application, and desktop export all already
work — see §1–§3. This list is about breadth and mileage, not missing basics.)

---

## 1. The foundational reality (the most important caveat)

This section separates two very different questions that were once conflated here: **"does it work on
real hardware?"** and **"can this headless cloud dev/CI box prove it automatically?"** The machine Maz
is developed on has **no GPU and no speaker**, so it can neither watch a frame draw nor hear a sound —
but that is a limitation of the *build box*, not of the engine.

- **The renderer runs on real hardware.** Maz has a real Vulkan renderer (sprites, meshes, PBR, shadows,
  bloom, SSAO) and it has been confirmed running on a real machine by the project owner. What it does
  **not** yet have is *automated visual-regression testing on a GPU* and the breadth of being
  battle-tested across many graphics cards, drivers, and OSes the way Godot's renderers are. So: proven
  to run, not yet proven *at scale*. (Because this build box has no GPU, frame-by-frame checks have to
  run on a real machine, not here — which is why they aren't automated yet. That is the honest residual,
  not "it doesn't draw pixels.")
- **Audio plays out of a real device.** `engine/src/audio/Audio.cpp` opens the system's default playback
  device (`SDL_OpenAudioDeviceStream` + `SDL_ResumeAudioStreamDevice`) and streams the DSP output to it,
  so on any machine with a sound device it plays. The honest residual is confirming every platform's
  audio backend and edge cases across time — coverage Godot has and Maz has not yet accumulated.
- **Networking now has real, verified, reliable UDP.** On top of the existing RPC/replication/ack
  *logic* (`maz/net/`), `maz::net::UdpSocket` is a real OS-socket datagram transport (loopback round-trip
  proven — `tests/net/loopback.cpp`), and `maz::net::ReliableChannel` wires the ack layer
  (`AckSender`/`AckReceiver`) onto it to deliver whole messages reliably and **in order even across a
  40%-packet-loss link** (`tests/net/reliable.cpp`, ctest `net_reliable`: 50/50 messages, exactly once,
  in order). This is the same code path two physical machines use, only the destination IP differs.
  What remains vs Godot: secure (DTLS-style) transport, WebSocket/WebRTC for browsers, and a true
  cross-machine soak test on the owner's two machines.
- **Maturity and real-world testing.** Godot is ~10+ years old, hardened by thousands of shipped
  titles, millions of user-hours, and a large contributor base finding and fixing edge cases. Maz
  has shipped zero real games and has no external users. Untested code, however elegant, is not the
  same as reliable code.

---

## 2. Platforms & export — Maz ships to nothing yet

Godot's defining strength is **one-click** export to many platforms. Maz has the export *machinery* —
desktop bundling works and the mobile/web foundations are in place — but not Godot's breadth of
turnkey, in-editor, cross-platform output.

- **Desktop export already works** — `tools/package.sh` bundles a built game (executable + its libraries
  + compiled shaders + assets + a launcher + README) into a single self-contained archive a player can
  download and run with nothing else installed, and it *self-verifies* the bundle launches from a scratch
  directory. Verified producing `dist/zomboid-1.0.0-linux-x86_64.tar.gz`. The remaining gap vs Godot is
  cross-OS output (making Windows/macOS bundles from one machine) and a one-click **Export** button inside
  the editor rather than a command line.
- **Mobile: engine-side foundation done, device build remains** — the shared work is in place and unit-
  tested here: touch + gestures, on-screen virtual controls, the callback-driven main loop, and the
  `PlatformBackend` device seams a phone game reads (safe area, orientation, power/thermal, haptics, soft
  keyboard, network, and the accelerometer/gyroscope motion sensors for tilt + shake) with a battery-aware
  frame pacer + dynamic resolution and autosave-on-suspend; plus a MoltenVK-safe renderer, a mobile render
  tier, and `tools/package_mobile.sh` staging a complete multi-ABI/`.pck` Android/iOS bundle (with a
  generated Android `build.gradle`) that `io::preflightMobileBundle` validates. What's left vs Godot is the
  concrete `AndroidBackend`/`IOSBackend`
  wiring those seams (including the sensors) to OS calls, the APK/IPA build (running Gradle with the SDL3
  module / an Xcode project + signing), and
  store packaging — all needing the owner's device toolchain ([MOBILE_BUILD.md](MOBILE_BUILD.md)).
- **No web/HTML5 export** — Godot compiles games to run in a browser (WebAssembly). Maz cannot.
- **No console support** — Godot has (third-party) paths to Switch/PlayStation/Xbox. Maz has none.
- **No XR/VR** — Godot has OpenXR (VR/AR headsets, controllers, passthrough). Maz has none.

---

## 3. The editor — Maz has editor *logic*, not an editor *application*

Godot **is** primarily its editor: a polished, dockable, mouse-driven desktop app that most users
never leave. Maz **has an editor application** — it builds via `tools/build_editor.sh` /
`tools/build_editor.bat` into `build/bin/editor`, and a Windows editor build has been released for
download. What it lacks is the *breadth and polish* of Godot's editor. Specifically still missing:

- A real, GPU-rendered, dockable editor window with panels, drag-and-drop, and a live 3D/2D viewport.
- Visual TileMap painting, terrain/GridMap painting, and polygon/collision drawing tools.
- A **visual shader editor** (node graph) and a **visual scripting** option.
- An **animation editor** timeline UI (Godot's AnimationPlayer/AnimationTree dock).
- A **theme editor**, font/import-settings UI, and a project manager.
- An integrated **debugger and profiler GUI** with remote debugging, breakpoints, and live scene inspection.
- An **asset importer UI** for the many formats below.

---

## 4. Import pipeline & formats

Godot imports a wide range of source files through its editor. Maz reads a handful (glTF, OBJ, WAV,
QOI, JSON, CSV, XML) but lacks importers for:

- **FBX**, **Collada (.dae)**, and direct **Blender** import.
- Many image formats and GPU-compressed textures (**KTX**, **Basis Universal**, **DDS**, **WebP**, **SVG**).
- **Ogg Vorbis / MP3** audio decoding.
- A general **font import** pipeline with fallback chains (Maz has basic TrueType only).

---

## 5. High-end 3D rendering (needs a working GPU first)

Even setting aside that Maz's renderer is unproven, Godot has advanced GPU features Maz has **no code**
for at all:

- **Global illumination** — baked **lightmaps**, **VoxelGI**, and **SDFGI** (real-time GI). Maz has none.
- **Reflection probes** and **screen-space reflections (SSR)**.
- **Decals** (projected textures) and **volumetric fog**.
- **GPU-driven particles** with collision (Maz's particles are CPU-side).
- **Occlusion culling** (Godot's occluder system) — Maz has frustum culling only.
- **GPU compute** pipelines / a general compute-dispatch API.
- **Heightmap terrain** authoring and rendering.
- A full **3D navigation server** with runtime navmesh baking and dynamic obstacles (Maz has 2D nav
  and navmesh *math*, not a baked 3D nav server).
- Mesh **LOD** generation and **visibility ranges**.

---

## 6. Scripting & language ecosystem

- **GDScript** — Godot's purpose-built, deeply editor-integrated game language with autocomplete,
  live debugging, and documentation tooltips. Maz has its own scripting VM, but not GDScript's
  maturity or tooling depth.
- **C# support** (via .NET) — a first-class option in Godot. Maz has none.
- **GDExtension** — Godot's stable C/C++ plugin ABI letting anyone add native modules without
  recompiling the engine. Maz has no plugin/extension system.
- A large **community-extension ecosystem** built on the above.

---

## 7. Text, UI, and localization depth

- **Complex text shaping** — Godot's TextServer does bi-directional text (Arabic/Hebrew), complex
  scripts (Indic, Thai), ligatures, and font fallback via HarfBuzz. Maz has a `TextServer` module but
  not full production-grade complex-script shaping.
- **Video playback** (Godot's VideoStreamPlayer, Theora/WebM). Maz has none.
- The sheer breadth and polish of Godot's **Control node** UI library and its **theming/editor**.
- Mature **localization tooling** (translation editor, POT generation) beyond Maz's CSV tables.

---

## 8. Ecosystem, documentation, and support

None of these are code — and all of them matter enormously to a real user:

- A comprehensive **documentation site**, class reference, and thousands of community **tutorials**.
- The **Asset Library** — in-editor browsing and one-click install of community plugins and assets.
- A large, active **community** (forums, Discord, contributors) for help and bug reports.
- A stable **release cadence**, long-term support versions, and a track record of fixing regressions.
- Third-party **tooling, courses, and books**.

---

## How to read this honestly

- Where Maz **matches or exceeds** Godot, it is almost always in *CPU-side game logic and math* that
  can be unit-tested without a GPU — and there Maz is genuinely, verifiably strong (see `GODOT_PARITY.md`).
- Where Maz **falls short**, it is almost always in *breadth and mileage*: mobile/web/console/VR reach,
  top-tier GPU visuals, automated GPU-farm testing, and years of ecosystem — and there Godot is far ahead.
  (The basics people assume are missing — the renderer, audio output, the editor app, desktop export — do
  work; see §1–§3.)
- The correct one-sentence summary is: **Maz is an exceptionally deep, well-tested game-logic library
  with a working-but-young renderer, editor, and export; Godot is a complete, shipping, cross-platform game engine and
  editor. They are not the same category of thing yet, and this document exists so that is never
  misrepresented.**
