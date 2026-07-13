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

**Stage 2 — the editor window (next):** a Dear ImGui interface with a Scene Hierarchy panel (the
list of objects), an Inspector panel (edit the selected object's transform), a live 3D viewport,
and transform gizmos. This is written and compile-verified in CI, then run on a GPU machine.

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

## Rendering a scene from the sandbox

| Flag | Effect |
|------|--------|
| `--scene PATH` | Load a `.mazscene` and render every entity at its transform; camera orbits. |
| `--load-model PATH` | Shortcut: wrap a single glTF/GLB in a one-entity scene and spin it. |

Both run headless as a no-op (no GPU/display needed for CI), so `sandbox --headless --scene …`
exits cleanly after loading — which is what `ctest scene_load_headless` checks.
