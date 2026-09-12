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

Files are written under the per-user preference directory (`SDL_GetPrefPath("MazEngine", "editor")`).

## Asset format (`.mazprefab`)

Saving an asset serializes the node list as a `scene::Prefab` (the engine's `.tscn`-style text
resource, via `maz::io::PrefabText`): the root is the asset and each part is a child node whose
property bag carries its mesh index, colour, transform, local AABB, and material. It round-trips back
into the editor with `Ctrl+Shift+O`, and any game can load it with `io::loadPrefabText` +
`scene::instantiate`. Asset metadata (character vs item, stats) is stored on the **root** node's
property bag — see `maz::game::AssetDef`.

## Baking

`Ctrl+Shift+B` flattens every visible part into a single mesh with `maz::editor::bakeComposite`, which
bakes each part's transform into its geometry (`render::applyTransform`) and concatenates them
(`render::mergeMeshes`), tinting each part's vertices by its swatch colour so the merged object keeps
its colours without per-part textures. The result is one draw call and one exportable object.
