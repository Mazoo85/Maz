# Maz Editor — character / item creator

`apps/editor` is the in-engine scene editor: a 3D viewport, a scene-tree panel, an inspector, move /
rotate / scale gizmos, undo/redo, and a bottom dock (Assets / Profiler / Output). On top of that it
doubles as a **character / item creator** — you compose an object out of the primitive palette (box,
sphere, cylinder, cone, torus, capsule), give each part a transform and material, then save it as a
reusable asset or bake it into a single mesh.

## Composing an asset

1. Add parts from the **Assets** dock and place them with the gizmos (keys `1`/`2`/`3` = move / rotate /
   scale; `Shift`+click multi-selects; hold to drag on the ground plane).
2. Tune each part in the **Inspector** — transform, colour swatch, and PBR material (roughness /
   metallic / specular / emissive).
3. Save, bake, or package (see below).

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| `1` / `2` / `3` | Move / Rotate / Scale gizmo |
| `Shift`+click | Toggle a node in the multi-selection |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Ctrl+D` / `Delete` | Duplicate / delete the selection |
| `Ctrl+S` / `Ctrl+O` | Save / open the working **scene** (`scene.json`) |
| `Ctrl+Shift+S` / `Ctrl+Shift+O` | Save / open the composition as a reusable **asset** (`asset.mazprefab`) |
| `Ctrl+B` | Package the scene into a distributable resource pack (`scene.mazpack`) |
| `Ctrl+Shift+B` | Bake the visible parts into one merged, per-part-tinted mesh (reported in the Output dock) |
| `Ctrl+Shift+K` | Toggle the asset's kind between **item** and **character** (saved with the asset) |

Files are written under the per-user preference directory (`SDL_GetPrefPath("MazEngine", "editor")`).

## Asset format (`.mazprefab`)

Saving an asset serializes the node list as a `scene::Prefab` (the engine's `.tscn`-style text
resource, via `maz::io::PrefabText`): the root is the asset and each part is a child node whose
property bag carries its mesh index, colour, transform, local AABB, and material. It round-trips back
into the editor with `Ctrl+Shift+O`, and any game can load it with `io::loadPrefabText` +
`scene::instantiate`.

Asset metadata — whether the asset is a **character** or an **item**, plus its typed **stats** (hp,
damage, heal, weight, …) — is stored on the **root** node's property bag by `maz::game::AssetDef`
(`assetDefToProps` / `assetDefFromProps`). In the editor the kind is toggled with `Ctrl+Shift+K`;
stats are authored directly in the `.mazprefab` text (as `key = TYPE value` lines on the root node)
and read back with the asset. A game turns the loaded definition into behaviour via
`AssetDef::statF/statI/statB`.

## Baking

`Ctrl+Shift+B` flattens every visible part into a single mesh with `maz::editor::bakeComposite`, which
bakes each part's transform into its geometry (`render::applyTransform`) and concatenates them
(`render::mergeMeshes`), tinting each part's vertices by its swatch colour so the merged object keeps
its colours without per-part textures. The result is one draw call and one exportable object.

## Exporting a turntable (full-motion video)

The headless `cutscene_export` tool renders a saved asset to a looping animated **GIF** — an orbiting
turntable — by baking it and rendering each frame. By default it renders **on the CPU** (no
GPU/window) with `render::renderMeshPreview`; with `--gpu` it renders the **real Vulkan PBR frame
graph** (sky, shadows, bloom, tonemap, colour grade) into an offscreen target and reads each frame
back with `Renderer::captureImage` — no window needed:

```sh
cutscene_export assets/characters/hero.mazprefab --out hero.gif --fps 30 --seconds 4 --size 256
cutscene_export assets/characters/hero.mazprefab --out hero.gif --gpu        # real PBR frames
```

| Flag | Meaning (default) |
|---|---|
| `--out FILE` | Output animated GIF (`cutscene.gif`) |
| `--fps N` | Frames per second (`30`) |
| `--seconds N` | Clip length; one full 360° orbit (`3`) |
| `--size N` | Square frame size in pixels (`256`) |
| `--frames N` | Cap the frame count (0 = no cap) |
| `--frames-dir DIR` | Also write each frame as a numbered image (`frame_0000.<ext>`, …) into `DIR` |
| `--frame-format ppm\|qoi` | Frame image format when `--frames-dir` is set (`ppm`) |
| `--aa 1-4` | Supersample anti-aliasing factor (`2`), CPU path only — higher is smoother but slower |
| `--gpu` | Render the real PBR frame graph offscreen (surfaceless Vulkan + `captureImage`) instead of the CPU preview. Falls back to the CPU rasterizer when no Vulkan device is available. |
| `--obj FILE` | Also export the baked composite as a Wavefront **OBJ** mesh (positions/uvs/normals + per-vertex colour) for Blender / other engines. |
| `--glb FILE` | Also export as a binary **glTF** (`.glb`) — the modern standard; **one `pbrMetallicRoughness` material per part** (base colour + roughness + metallic + emissive) plus `COLOR_0` vertex colours, so per-part materials carry into Blender / other engines. Re-imports with `render::loadGltf`. |

The GIF is a self-contained preview; the `--frames-dir` PPM/QOI sequence is for pulling into `ffmpeg`
or a video editor (e.g. `ffmpeg -i frame_%04d.ppm out.mp4`). `--gpu` produces frames that match the
in-editor viewport; the CPU default runs anywhere the engine compiles.
