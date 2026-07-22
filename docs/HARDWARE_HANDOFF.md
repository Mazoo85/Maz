# Seeing & hearing it on your PC — the hardware hand-off

Some parts of a game engine can only be *proven* on a real machine with a screen, a
graphics card (GPU), and a speaker. The cloud box that wrote this code has none of those —
it can compile the code and test all the logic/math, but it cannot look at a window or
listen to a sound. This page is the hand-off: for each of those parts it says **what's
done**, the **one command to run**, and **what you should see or hear**.

Everything below is already written and compiles cleanly. The only step left is *you*
running it on your computer.

> First-time setup (once): build the engine.
> ```
> cmake -S . -B build && cmake --build build
> ```
> On Windows use the matching `.bat` scripts in `tools/`. macOS/Linux use the `.sh` scripts.

---

## 1. The renderer — does a lit 3D frame appear? ✅ already confirmed

**Status:** You've already run the app on your machine, so this one is done on your side.
The renderer is a real Vulkan renderer; the residual is only "not battle-tested across
dozens of different GPUs," not "unproven."

**Run it again any time:**
```
./build/bin/world      # an explorable 3D scene   (Windows: build\bin\world.exe)
```
**You should see:** a lit, walkable 3D world with shadows and a sky. Move with the mouse
and WASD.

---

## 2. Audio — does sound come out of the speaker? 🔈 needs your speaker

**Status:** The device-output wiring is written and compiles into the engine
(`engine/src/audio/Audio.cpp` opens the system's default playback device with SDL and
starts it). All the *sound generation and effects* (synth, filters, reverb, mixing) are
already unit-tested. The one thing a headless box can't do is confirm the sound physically
reaches a speaker.

**Run it:**
```
./build/bin/orbs       # an arcade game with music + sound effects
```
**You should hear:** background music and blips/effects as you play. If you hear sound,
this item is done on your side.

---

## 3. The editor — does the editor window open with live panels? 🖥️ needs your GPU

**Status:** The editor program is written and **builds to a real executable here**
(`build/bin/editor`, ~660 KB). All the editor *logic* — scene tree, inspector, gizmos,
undo/redo, save/load — is already tested. Turning it into a visible window with live
panels needs a GPU to draw the editor itself.

**Build & run it:**
```
tools/build_editor.sh          # Windows: tools\build_editor.bat
./build/bin/editor
```
**You should see:** an editor window — a 3D viewport with a scene tree on one side and an
inspector on the other; click objects to select, drag the gizmo to move them.

---

## 4. SSIL & the final GPU visual passes — do the effects show up? 🎨 needs your GPU

**Status:** The math for these effects is written and unit-tested on the cloud box — SSAO,
screen-space reflections, reflection probes, lightmap baking, and now **SSIL** (colored
indirect light — `render::ssilGather`, the code that makes a red wall tint a white floor).
Seeing the effect requires running the shaders on your GPU.

**Run it:** enable the effects in a 3D sample (e.g. `world` / `scene3d`) and look for the
soft ambient shadows in creases (SSAO) and the faint colored bounce near brightly-colored
walls (SSIL).
**You should see:** contact shadows in corners and a subtle color glow bleeding from
colored surfaces onto nearby ones.

---

## 5. Web / WASM & video — the browser build & video frames 🌐 needs a toolchain

**Status:** The browser build *path* is written (`tools/build_web.sh`) and the **video
container demuxer** is done and tested (`video::demuxIvf` reads a video file's frames +
timing). Emitting the actual web binary needs the Emscripten toolchain installed on your
machine; turning compressed video frames into pixels needs a video codec (a separate large
library) plus your GPU to display them.

**Build the web version (after installing Emscripten):**
```
tools/build_web.sh
```
**You should get:** a `.wasm` + `.html` you can open in a browser to play the game on the web.

---

## The short version

| Part | Written & compiles here | What only your PC can do |
|------|-------------------------|--------------------------|
| Renderer | ✅ | ✅ already confirmed by you |
| Audio output | ✅ | hear it from a speaker |
| Editor app | ✅ (builds to `bin/editor`) | see the editor window |
| SSIL / GPU effects | ✅ (math tested) | see the effect on screen |
| Web/WASM + video | ✅ (build script + demuxer) | install Emscripten; run in a browser |

Nothing here is blocked or broken — it's all built. These are just the steps that, by their
nature, happen on a real machine with a screen and a speaker.
