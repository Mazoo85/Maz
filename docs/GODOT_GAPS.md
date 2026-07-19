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
"missing algorithms" — they are the things that make an engine a usable product: a proven renderer,
platform reach, a real editor application, working networking, and a decade of ecosystem.

---

## 1. The foundational reality (the most important caveat)

These are the gaps that matter most, and they are not about missing code — they are about code that
has **never been proven to actually run**.

- **The renderer is unverified.** Maz has a Vulkan renderer (sprites, meshes, PBR, shadows, bloom,
  SSAO, etc.) written in code, but the build environment used to develop it **has no GPU**. Nothing
  visual has been confirmed to actually draw a single pixel on real hardware here. Godot's renderers
  (Vulkan, Direct3D 12, OpenGL, and a web renderer) are shipping and battle-tested in released games.
  *Until Maz runs on a real GPU and is validated frame-by-frame, every rendering feature is "written"
  but not "proven."* This is the single biggest gap.
- **Audio has no verified device output.** Maz has a deep audio *DSP* library (filters, reverb,
  synthesis, mixing, spectrum analysis) but no confirmed, cross-platform path that actually plays
  sound out of a real speaker. Godot has working audio backends on every platform.
- **Networking has no verified transport.** Maz has RPC-dispatch and multiplayer-spawner *logic*
  (`maz/net/`), but not a proven, secure, real-world network transport moving packets between two
  machines. Godot ships high-level multiplayer over ENet/WebSocket/WebRTC, used in live games.
- **Maturity and real-world testing.** Godot is ~10+ years old, hardened by thousands of shipped
  titles, millions of user-hours, and a large contributor base finding and fixing edge cases. Maz
  has shipped zero real games and has no external users. Untested code, however elegant, is not the
  same as reliable code.

---

## 2. Platforms & export — Maz ships to nothing yet

Godot's defining strength is one-click export to many platforms. Maz has **none** of this.

- **No desktop export templates** — Godot packages a finished game into a standalone Windows/macOS/
  Linux executable with bundled assets. Maz has a build system, not a game-export pipeline.
- **No mobile** — Godot exports to Android and iOS (touch input, sensors, store packaging). Maz has none.
- **No web/HTML5 export** — Godot compiles games to run in a browser (WebAssembly). Maz cannot.
- **No console support** — Godot has (third-party) paths to Switch/PlayStation/Xbox. Maz has none.
- **No XR/VR** — Godot has OpenXR (VR/AR headsets, controllers, passthrough). Maz has none.

---

## 3. The editor — Maz has editor *logic*, not an editor *application*

Godot **is** primarily its editor: a polished, dockable, mouse-driven desktop app that most users
never leave. Maz has building blocks (`maz/editor/`: a scene model, inspector logic, gizmo math,
undo/redo, pick-ray) but **not a shipping editor program** a designer can open and use. Specifically missing:

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
- Where Maz **falls short**, it is almost always in *things that require a running GPU, a shipping
  editor application, platform-export toolchains, or years of ecosystem* — and there Godot is far ahead.
- The correct one-sentence summary is: **Maz is an exceptionally deep, well-tested game-logic library
  with an as-yet-unproven renderer; Godot is a complete, shipping, cross-platform game engine and
  editor. They are not the same category of thing yet, and this document exists so that is never
  misrepresented.**
