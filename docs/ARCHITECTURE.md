# Maz Engine — Architecture

## Design principles
1. **2D-first, 3D-ready.** The renderer is an interface (`maz::Renderer`) with a Vulkan
   implementation behind it. Cameras, scene data, and draw submission are abstracted so a 3D
   path slots in without rewriting gameplay code.
2. **Layered, one-directional dependencies.** `core` ← `platform` ← `render` ← `app`. Lower
   layers never include higher ones.
3. **Data-oriented where it counts.** Hot paths (entities, rendering) favor contiguous storage
   and handles over deep pointer graphs.
4. **Deterministic simulation.** Fixed-timestep update decoupled from render (a `1/60`
   accumulator), so gameplay is reproducible and frame-rate independent.
5. **Degrade gracefully.** No GPU / no display (CI, headless) must not crash — the renderer
   logs and no-ops so tooling and tests still run.

## Module map

```
core/       Log, Assert, Time (fixed-timestep clock), Config/args, KeyValueStore (save/load)
              — zero dependencies beyond the standard library
platform/   Window, Input (keyboard/mouse/gamepad), event pump, prefPath   (depends on: core, SDL3)
math/       maz::math = GLM re-export + helpers     (header-only)
render/     Renderer (interface) + Vulkan backend   (depends on: core, platform, math, Vulkan)
              VulkanContext  — instance, device, queues, debug messenger
              VulkanSwapchain— swapchain + offscreen HDR scene pass (16-bit float, MSAA ≤4×
                               resolving to a sampled sceneColor) + composite pass to the swapchain
              BloomChain     — bright-pass + ½-res separable Gaussian blur of the HDR scene
              PostProcess    — fullscreen composite: HDR sceneColor + bloom -> swapchain, with
                               optional ACES tonemap/exposure
              TextureStore   — shared texture registry (one descriptor layout, used by 2D + 3D)
              VulkanBuffer/Texture, SpriteRenderer — batched textured 2D sprites
              MeshRenderer   — textured 3D meshes; ambient + shadow-mapped sun + 8 point/spot lights
                               + dynamic sky + distance fog + normal mapping + emissive + specular
                               (Material) + wireframe debug draw + instancing + transparency.
                               Per-frame camera/light matrices live in a set-2 scene UBO; the
                               per-draw push is just model + material. drawMeshInstanced renders N
                               copies in one indexed draw via a per-instance vertex binding;
                               drawMeshTransparent alpha-blends depth-sorted translucent meshes
                               after the opaque pass.
              Particles3D    — world-space camera-facing additive billboard particles
              DebugDraw      — world-space debug lines / AABBs (collider + gizmo visualization)
              shapes         — procedural box / sphere / plane geometry
              loadGltf       — glTF 2.0 model import (cgltf) -> ModelData (mesh + base-color + normal map)
              loadGltfScene  — glTF 2.0 scene import -> SceneData (per-node mesh + transform + textures)
              Renderer       — beginFrame / drawSprite / drawMesh / endFrame
ui/         Font (TTF atlas: drawText/drawTextCentered/textWidth), DebugOverlay (FPS/draw stats)
ecs/        World — entity-component system (sparse-set pools, each/view)   (header-only)
game/       Tilemap, FlyCamera (first-person camera), Collision (AABB slide + ray/AABB queries),
            Shake (camera juice), SpatialGrid (uniform X/Z broadphase hash),
            NavGrid (8-directional A* grid pathfinding for moving AI)
fx/         ParticleSystem — pooled 2D particles   (on top of Renderer)
audio/      Audio — SDL3 device + real-time synth mixer (SFX + music)  (depends on: core, SDL3)
apps/
  sandbox/  Top-down tile-world demo
  orbs/     "ORB RUN" — a complete arcade game (states, HUD, audio, particles, save)
  swarm/    ECS demo — 800 entities through movement + render systems
  cube/     3D demo — lit, depth-tested spinning cube + 2D HUD
  scene3d/  ECS + 3D — ground plane + ring of shapes, orbiting camera
  world/    Explorable 3D — fly camera through a textured-floor block field
  model/    glTF demo — loads house.gltf at runtime, orbits with shadows + sky
  village/  VILLAGE QUEST — a game on the loaded village.gltf: house collision, coins, timer,
              win state, best-time save (scene loading + collision + audio + save composed)
  water/    Dynamic-mesh demo — a grid re-streamed each frame with summed sine waves, lit + fogged
```

## The frame loop (fixed timestep)

```
accumulator += frameDelta (clamped)
while (accumulator >= STEP) { update(STEP); accumulator -= STEP; }   // deterministic sim
render(interpolationAlpha = accumulator / STEP)                       // as fast as GPU allows
```

`core::Clock` owns the accumulator; the app calls `clock.tick()` and drains fixed steps. This
keeps physics/gameplay stable regardless of render FPS and enables replay/netcode later.

## Renderer abstraction (why 3D is "free" later)

`Renderer` exposes intent, not Vulkan detail:

```cpp
struct Renderer {
    virtual bool  init(Window&, const RendererConfig&) = 0;
    virtual void  onResize(uint32_t w, uint32_t h)     = 0;
    virtual bool  beginFrame()                          = 0;   // false => skip (minimized/no dev)
    virtual void  setClearColor(float r,g,b,a)          = 0;
    virtual void  endFrame()                            = 0;   // submit + present
    virtual void  shutdown()                            = 0;
};
```

`VulkanRenderer` implements this over Vulkan and owns a `SpriteRenderer` (batched textured
quads: `loadTexture`/`createTexture`/`drawSprite`/`setCamera2D`). A 3D mesh path will be added
as further submission methods (`drawMesh`) on the same interface — gameplay code never touches
Vulkan. Internals: `VulkanContext` (instance/device/queues + single-time command helper),
`VulkanSwapchain` (swapchain/render pass/framebuffers), `VulkanBuffer`/`VulkanTexture` (memory +
staging), `SpriteRenderer` (pipeline/descriptors/vertex streaming).

## Dependencies
- **SDL3** (`FetchContent`, tag `release-3.4.12`) — window, input, later audio/gamepad.
- **GLM** (`FetchContent`, tag `1.0.1`) — math, header-only.
- **Vulkan** (`find_package(Vulkan)`) — loader + headers; `glslangValidator` compiles shaders.
- **stb_image** (`FetchContent`, master) — image decoding, header-only.
- **cgltf** (`FetchContent`, tag `v1.14`) — glTF 2.0 model parsing, header-only.
- No other system installs required; SDL3 and GLM build from source at configure time.

## Headless / CI behavior
Run `sandbox --headless [--frames N]`. It uses SDL's dummy video driver when no display is
present, ticks the loop N times, attempts Vulkan init, and exits 0. If no Vulkan physical device
exists (typical in CI containers), the renderer logs a warning and the loop still runs — so the
smoke test validates wiring, lifetime, and shutdown without a GPU.

## Testing
Three layers, all under `ctest`:
- **Unit tests** (`tests/unit/main.cpp` → `maz_unit_tests`): a dependency-free `CHECK` runner over
  the pure-logic modules (math, collision, spatial grid, ECS, shake, particles). Fast, deterministic,
  no GPU.
- **Smoke tests**: each app run `--headless --frames 30` must exit 0 (wiring / lifetime / shutdown).
- **Golden-image tests** (`tools/golden.sh`): render each app on lavapipe under Xvfb and diff against
  committed references in `tests/golden/` using a per-app RMSE tolerance (tight for deterministic
  scenes, looser for time-animated ones). Catches structural render regressions; self-skips (exit 0)
  when software Vulkan / Xvfb / ImageMagick are absent, so it's harmless in a GPU-less CI. Re-record
  references after an intended visual change with `tools/golden.sh capture`.

## Coding conventions
- `PascalCase` types, `camelCase` functions/vars, `m_` member prefix, `MAZ_` macro prefix.
- Namespace everything in `maz::` (sub-namespaces `maz::core`, `maz::render`, `maz::math`).
- Headers `.hpp`, sources `.cpp`. Public headers under `engine/include/maz/`.
- `.clang-format` (LLVM-based, 4-space indent, 100 col) is the source of truth.
