<!-- Part of MAZ ARCADE — see the repo root README for every project. -->

# Blender → Maz Engine pipeline

This is the pathway for getting 3D models made in **Blender** into the **Maz Engine**.

Engines and Blender don't talk directly — they meet in the middle using a standard 3D
file format. Maz uses **glTF 2.0** (the modern, open standard; think "PDF for 3D
models"), which Blender exports natively.

```
   Blender  ──export──▶  model.glb  ──maz::assets::loadModel──▶  Maz Engine
  (you model)          (glTF 2.0 file)         (cgltf)          (mesh data in memory)
```

## Current status

Working today:

- **Export from Blender** — a one-click helper (`tools/blender/maz_export.py`) writes a
  `.glb` with the settings the engine expects.
- **Load into the engine** — `maz::assets::loadModel()` reads the `.glb`/`.gltf` into
  meshes (positions, normals, UVs) plus an axis-aligned bounding box.
- **Verified end to end** — a headless test (`ctest -R model_load_gltf`) loads
  `assets/models/cube.gltf` and checks the geometry.

- **Drawing the model on screen** — `./build/bin/sandbox --load-model <file>` draws it as a
  spinning, lit mesh (needs a real GPU + display; runs headless as a no-op for CI). See the mesh
  renderer in `engine/src/render/MeshRenderer.cpp`.

Not done yet (later milestones):

- Materials/textures, skeletal animation, and async streaming via the asset manager.
- Back-face culling (currently draws all faces, sorted by the depth buffer) and instancing.

## Exporting from Blender

### The easy way (add-on)

1. In Blender: **Edit → Preferences → Add-ons → Install…**
2. Pick `tools/blender/maz_export.py`, then tick the checkbox to enable it.
3. **File → Export → "Maz Engine (.glb)"**, choose a location under `assets/models/`,
   and export. Tick *Selected Objects Only* to export just what's selected.

### The scripted way (headless / automation)

```
blender myfile.blend --background --python tools/blender/maz_export.py -- \
    --out assets/models/myfile.glb [--selected]
```

Both paths route through the same export settings, so results are identical:

| Setting | Value | Why |
|---------|-------|-----|
| Format | `GLB` (binary) | One self-contained file — no loose `.bin`/textures to lose |
| Up axis | Y-up, right-handed | The glTF standard; the engine's loader assumes it |
| Apply modifiers | on | What you see in Blender is what the engine gets |
| Normals / UVs | exported | Needed for lighting and texturing later |

## Loading in the engine

```cpp
#include "maz/assets/Model.hpp"

maz::assets::Model model;
std::string err;
if (maz::assets::loadModel("assets/models/cube.gltf", model, &err)) {
    // model.meshes[i].vertices / .indices, model.bounds, model.triangleCount()
} else {
    // err holds a human-readable reason
}
```

Each `Vertex` is POD and tightly packed (`position[3]`, `normal[3]`, `uv[2]`), so a
mesh's vertex vector can be uploaded to a Vulkan vertex buffer verbatim once the mesh
renderer lands.

### Try it from the sandbox

```
./build/bin/sandbox --headless --frames 1 --load-model assets/models/cube.gltf
```

Prints the mesh count, vertex/triangle totals, and bounds — no GPU or display required.

## Under the hood

- **Parser:** [cgltf](https://github.com/jkuhlmann/cgltf) (single-header, MIT), vendored
  at `engine/third_party/cgltf/`. Its implementation is compiled as its own target
  (`maz_cgltf`) with relaxed warnings, since the engine builds first-party code with
  `-Werror`.
- **Loader:** `engine/src/assets/Model.cpp` — triangulated primitives only; missing
  normals/UVs default to zero; indices are generated when a primitive omits them.
- **Test asset:** `assets/models/cube.gltf` — a unit cube (24 verts, 12 tris) with an
  embedded buffer, so the repo needs no binary blobs to run the test.

---

← Back to the [**MAZ ARCADE hub**](../index.html) · [repository README](../README.md)
