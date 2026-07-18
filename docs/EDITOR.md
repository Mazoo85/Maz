# Maz Editor

The visual editor for the Maz Engine — a tool for arranging a scene (placing, rotating, and
scaling objects) instead of writing code. This is roadmap **Phase 12**, built in stages.

## Status

**Stage 1 — the scene backbone (done, tested):**

- A **scene** is a list of **entities**; each entity has a name, a transform (position / rotation /
  scale), and an optional model to draw. See `maz::scene::Scene` / `Entity` / `Transform` in
  `engine/include/maz/scene/Scene.hpp`.
- Scenes **save and load** to a small human-readable `.mazscene` file
  (`maz::scene::saveScene` / `loadScene`). Round-trip verified by `ctest unit_scene`.
- The engine can **render a whole scene** of objects (not just one). Try it on a machine with a
  GPU + display:

  ```
  cmake -S . -B build -G Ninja && cmake --build build
  ./build/bin/sandbox --scene assets/scenes/demo.mazscene
  ```

  The camera slowly orbits an arrangement of cubes defined in `assets/scenes/demo.mazscene`.

**Stage 2 — the editor window (done, compile-verified):** the `editor` app — a Dear ImGui window
with a menu bar (New / Reload / Save / Add Cube), a **Hierarchy** panel (select / add / delete
objects), and an **Inspector** panel (edit the selected object's name and drag its
position / rotation / scale). The live 3D scene renders behind the panels. Run it on a machine with
a GPU + display:

```
cmake -S . -B build -G Ninja && cmake --build build
./build/bin/editor --scene assets/scenes/demo.mazscene
```

> CI verifies the editor **compiles** and boots headless; the interactive UI needs a real GPU and
> is not exercised in CI. Transform gizmos, an asset browser, and a docked render-to-texture
> viewport are still to come.

**Stage 3 — the character / item Creator (done, tested):** a **Creator** panel in the `editor`
app for authoring characters and items out of parametric primitives, with no external modelling
tool. You compose **parts** — box / sphere / cylinder / plane, each with its own size and transform
— tag the asset as a **Character** or an **Item**, and save it as a `.mazasset`. "Bake & Preview"
adds it to the live scene so you can see the composed mesh immediately. See "The `.mazasset` file
format" below and `maz::assets::CompositeAsset` / `maz::assets::Primitives`.

- Primitive generation is unit-tested headless (`ctest unit_primitives`): exact vertex/index counts,
  unit-length normals, and bounds.
- Asset baking + save/load round-trip is unit-tested (`ctest unit_asset`).
- A baked `.mazasset` is rasterized off-screen and its pixels checked (`ctest render_probe_asset`),
  proving the whole author → bake → draw path, not just the geometry math.
- Example assets: `assets/characters/hero.mazasset` (a blocky humanoid) and
  `assets/items/probe_asset.mazasset` (the render-probe fixture).

> Deferred to later phases: per-part colour/material, non-uniform part scale, character/item stats,
> and the cutscene timeline + video export. Phase 1 keeps part scale uniform so baked normals stay
> correct without an inverse-transpose.

## The `.mazscene` file format

A plain-text, line-oriented format — easy to read, diff, and hand-edit.

```
maz-scene 1
name "Demo Scene"
entity
  name "Center Cube"
  model "assets/models/cube.gltf"
  pos 0 0.1 0            # x y z, world units
  rot 0 25 0            # Euler degrees, applied Y then X then Z
  scale 1 1 1
```

- The first non-comment line must be `maz-scene <version>`.
- `name` before any `entity` sets the scene name; after an `entity` it names that entity.
- Blank lines and lines starting with `#` are ignored.
- Strings are `"double quoted"`; `\"` and `\\` are escapes.

An entity's `model` may point at a `.mazasset` (see below) just as it points at a glTF file; the
asset is baked to a mesh when loaded.

## The `.mazasset` file format

A composite asset authored in the Creator: a kinded list of parametric **parts** that bake into a
single mesh. Same line-oriented conventions as `.mazscene` (shared tokenizer; quotes, `#` comments,
and unknown lines tolerated for forward-compatibility).

```
maz-asset 1
name "Hero"
kind character            # character | item
part
  name "Torso"
  prim box                # box | sphere | cylinder | plane
  size 0.6 0.9 0.35       # box/plane full extents (plane uses x and z)
  radius 0.5              # sphere/cylinder radius
  height 1                # cylinder height
  segments 16             # sphere/cylinder subdivision around the axis
  rings 8                 # sphere latitude subdivision
  pos 0 1.15 0            # part transform, in the asset's local space
  rot 0 0 0              # Euler degrees, applied Y then X then Z
  scale 1 1 1            # Phase 1: keep uniform
```

- The first non-comment line must be `maz-asset <version>`.
- `name` / `kind` before any `part` describe the asset; `name` after a `part` names that part.
- Each shape reads only the fields it needs (a box ignores `radius`, etc.).
- `maz::assets::buildModel` bakes all parts into one `maz::assets::Model`, ready to upload and draw.
- Round-trip verified by `ctest unit_asset`.

## Rendering a scene from the sandbox

| Flag | Effect |
|------|--------|
| `--scene PATH` | Load a `.mazscene` and render every entity at its transform; camera orbits. |
| `--load-model PATH` | Shortcut: wrap a single glTF/GLB in a one-entity scene and spin it. |

Both run headless as a no-op (no GPU/display needed for CI), so `sandbox --headless --scene …`
exits cleanly after loading — which is what `ctest scene_load_headless` checks.
