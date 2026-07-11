# Maz Engine — Evaluation & Enhancement Backlog

A living document driving the autonomous improvement loop: evaluate the engine, list what to add,
implement it, then re-evaluate and repeat. Updated each iteration.

## Where the engine stands (after M21)

**Strong:**
- Clean layered architecture (`core ← platform ← render ← game/app`), pluggable `Renderer`.
- 2D: batched sprites, multi-camera, TrueType HUD text, tilemaps, 2D particles.
- 3D: textured meshes, depth, shadow mapping (PCF), gradient sky, MSAA, **point lights**.
- Assets: glTF **model** and **scene** loading (cgltf) with embedded base-color textures.
- Systems: fixed-timestep loop, sparse-set ECS, synth audio, save (KeyValueStore), AABB collision,
  fly camera.
- Three complete sample games (`orbs`, `world`, `village`/VILLAGE QUEST) + 5 demos.
- Verifies headless (lavapipe + Xvfb), 8 ctest smoke tests, warnings-as-error clean.

**Gaps (candidate work):**
1. Atmosphere: distance **fog**; **day/night** with a dynamic sky.
2. Input breadth: **gamepad / controller** support + action mapping.
3. Rendering quality: **bloom / tonemap** post-processing (needs render-to-texture); **normal
   mapping**; **spot lights**; fog.
4. Effects: **3D (world-space) particles** — current particles are 2D only.
5. Performance: **frustum culling**; instanced rendering.
6. Content/tools: native scene serialization; a simple in-game debug overlay.
7. Animation: skeletal animation (glTF skins) — large.

## Iteration log

### Iteration 1 — "Atmosphere & Input" (in progress)
- [x] **M22 — Distance fog** (engine): exponential fog to a fog color by camera distance, in the
  mesh path; fog params in the lights UBO, camera position threaded via `setCameraPosition` +
  push constant. Off by default (density 0) so existing apps are unchanged; `world` and `village`
  opt in so distance fades into the sky.
- [x] **M23 — Gamepad support** (platform): SDL3 gamepad in `platform::Input` (sticks, buttons,
  triggers, deadzone) with hotplug + graceful absence; `world`/`village` movement + look use the
  sticks (world uses triggers for up/down).
- [x] **M24 — Day/night dynamic sky** (engine+app): the sky's zenith/horizon/ground colors + sun
  are now driven by `SceneLighting` (defaults reproduce the old sky); `village` animates a ~48s
  sun arc so the sky, ambient, fog, and warm house lamps shift from bright noon to lamp-lit night.

**Iteration 1 complete.**

### Iteration 2 — "Lighting depth" (complete)
- [x] **M25 — Spot lights**: point lights gained an optional cone (spot axis + inner/outer angle,
  packed into the lights UBO; omni by default). A cool-white searchlight sweeps the village square
  at night — verified as a crisp cone pool distinct from the warm lamps.

### Iteration 3 — in progress
- [x] **M26 — Normal mapping**: derivative-based TBN in mesh.frag (no vertex-format change) + a
  set=3 normal sampler with a flat default (un-mapped meshes unchanged); the loader decodes glTF
  `normalTexture`; the house/village generators bake a normal-map atlas from a height field; model
  + village bind it. Verified: the brick walls and roof shingles now show per-pixel relief.
- [x] **M27 — Bloom / post-processing**: the scene now renders into an offscreen sceneColor
  (MSAA resolves there, SHADER_READ), and a composite pass runs a `PostProcess` fullscreen shader
  that samples it and writes the swapchain — threshold gaussian bloom, passthrough at strength 0.
  Landed incrementally (offscreen+passthrough verified pixel-identical for orbs/cube first, then
  bloom). Village scales bloom with the day/night cycle; night lamps + coins glow. Deferred within
  post: HDR float target + separable multi-pass blur + tonemap operator.
- [x] **M28 — World-space 3D particles**: a `Particles3D` renderer draws camera-facing additive
  billboards (expanded along the camera basis in the vertex shader, radial-falloff soft dots),
  depth-tested against the scene but not depth-writing, inside the scene pass. `Renderer` gains
  `setCameraBasis` + `drawParticle3D`. Village adds a bonfire ember plume that glows and blooms at
  night. Verified on lavapipe.
- [ ] Frustum culling; native scene serialization + a debug stats overlay; GPU-simulated particles.

### Iteration 4 — in progress
- [x] **M28 — World-space 3D particles** (bonfire embers).
- [x] **M29 — Debug stats overlay**: `Renderer::renderStats()` reports per-frame mesh/particle/
  sprite counts; `ui::DebugOverlay` draws smoothed FPS + frame-ms + those counts via the font
  (F3 toggles it in `world`). Verified: the overlay reads live stats on lavapipe.

### Iteration 5 — "Performance & juice" (in progress)
Evaluation found the mesh path draws all geometry unconditionally (no culling) and coin pickups
have no feedback effect. This iteration:
- [x] **M30 — Frustum culling**: each mesh gets a local AABB at upload; `flush` extracts the 6
  frustum planes from viewProj and skips meshes whose world AABB is fully outside. `renderStats`
  gains a `culled` count shown in the overlay. Verified in `world`: "MESH 36 (culled 51)" — >50%
  of draws skipped with the visible scene unchanged.
- [x] **M31 — Pickup-burst particles**: collecting a village coin spawns a 24-spark golden burst
  (short-lived pooled sparks integrated with gravity), drawn via Particles3D — they glow and bloom.
  Verified: the collect moment now sprays fading sparkles.

**Iteration 5 complete** (frustum culling + pickup juice).

### Iteration 6 — "Effects & polish" (in progress)
Evaluation: Particles3D only does additive blending (no smoke/dust); text centering is hand-rolled
in apps. This iteration:
- [x] **M32 — Alpha-blended smoke particles**: Particles3D gained a second pipeline + vertex list
  for alpha over-blending; `drawParticle3D(..., additive)` selects the mode (embers/sparks additive,
  smoke/dust alpha). The village bonfire now billows a gray smoke column above its glowing embers —
  both blend modes in one frame, verified on lavapipe.
- [x] **M33 — Font text alignment**: `Font::drawTextCentered` (uses `textWidth`); village's win
  text is now properly centered (was hand-offset by ~220px) and orbs' `centerText` routes through
  it (removing the duplicated math). Verified: orbs' title screen and village win text center.

**Iteration 6 complete** (smoke particles + text alignment).

### Iteration 7 — "Tooling & dynamic geometry" (in progress)
Evaluation: no visual debug-draw (wireframe) despite having a stats overlay; meshes are static
(no vertex animation). This iteration:
- [x] **M34 — Wireframe debug mode**: a second `VK_POLYGON_MODE_LINE` mesh pipeline toggled via
  `Renderer::setWireframe` (built only when the GPU exposes `fillModeNonSolid`, enabled on the
  logical device; silently stays filled otherwise). `world` toggles it with **F4** and accepts
  `--wireframe` at launch. Verified on lavapipe: boxes and the sphere render as their edge/triangle
  lattice while the 2D HUD stays solid.
- [x] **M35 — Dynamic meshes + water**: `Renderer::createDynamicMesh` + `updateMesh` re-stream a
  mesh's vertices every frame. Each dynamic mesh holds one host-visible vertex buffer *per frame in
  flight*; `updateMesh` writes the current frame's buffer (selected after the fence wait in
  `beginFrame`) so a write never races a frame still reading. The new `water` demo animates a 64×64
  grid with four summed sine trains, deriving per-vertex normals analytically so the sunlit crests
  and shadowed troughs respond to the ripples; distance fog fades it into the sky. Verified on
  lavapipe: two frames 2s apart differ by ~600k pixels (genuinely animating), no validation errors.

**Iteration 7 complete** (wireframe debug draw + dynamic meshes/water).

### Iteration 8 — "Debug visualization & materials" (in progress)
Evaluation: the only debug-draw was all-or-nothing mesh wireframe — no way to draw arbitrary
world-space lines (colliders, light ranges, paths, gizmos); and meshes could only be lit, nothing
could self-glow to feed bloom. This iteration:
- [x] **M36 — Debug line/shape renderer**: a `DebugDraw` renderer draws world-space colored lines
  (`Renderer::drawLine`) and AABBs (`drawAabb`) via a `LINE_LIST` pipeline with per-frame-in-flight
  vertex buffers, alpha-blended and depth-tested (geometry occludes them) inside the scene pass.
  `world` overlays every collision box in green with **F5** (or `--colliders`). Verified on lavapipe:
  the green boxes wrap each block exactly, confirming colliders match geometry; no validation errors.
- [x] **M37 — Emissive material term**: `Renderer::drawMeshEmissive` adds a per-draw emissive RGB
  (four more push-constant floats) to the fragment result *after* lighting and *before* fog, so an
  object self-illuminates and feeds bloom. `world` enables soft bloom and draws its pickups as
  pulsing emissive gold. Verified on lavapipe: the pickups glow vivid gold with a bloom halo while
  the (below-threshold) scene is unaffected; no validation errors.

**Iteration 8 complete** (debug line/collider draw + emissive materials).

### Iteration 9 — "HDR & game feel" (in progress)
Evaluation: the scene target was 8-bit LDR, so emissive/bloom clipped at 1.0 and there was no
tonemap; and pickups/hits had no camera feedback. This iteration:
- [x] **M38 — HDR scene target + ACES tonemap**: the offscreen scene color (and MSAA color) is now
  `R16G16B16A16_SFLOAT`, so lighting/emissive/bloom can exceed 1.0. The composite pass applies an
  exposure multiply + Narkowicz ACES tonemap (`Renderer::setTonemap`), off by default so 2D/3D apps
  are a faithful passthrough. Verified: orbs (2D) and cube (3D) unchanged with tonemap off; `water`
  with an over-bright sun rolls its sunlit crests off smoothly instead of clipping; no validation
  errors on the format change.
- [x] **M39 — Screen-shake / camera juice**: `maz::game::Shake` is a header-only trauma-based shake
  (applied amount = trauma², decaying linearly) whose offset/rotation are pure functions of
  (trauma, time) from layered sines — no RNG, deterministic. `world` adds trauma on each pickup and
  offsets the camera eye (restored afterward so collision sees the true position); the 2D HUD is
  unaffected. Verified on lavapipe: with shake pinned the frame-to-frame delta is ~468k pixels vs
  ~6.6k for a static camera (~70×), and the shaken frame renders cleanly; no validation errors.

**Iteration 9 complete** (HDR scene target + ACES tonemap; trauma-based camera shake).

### Iteration 10 — "Broadphase & bloom quality" (in progress)
Evaluation: collision tested every solid per move (O(n)), and bloom was a single-pass in-composite
blur (narrow, aliased at higher strengths). This iteration:
- [x] **M40 — Spatial grid broadphase**: `maz::game::SpatialGrid` buckets static colliders into an
  X/Z hash of square cells (Y ignored in bucketing; narrow-phase overlap is still full 3D). A
  `slideMove(pos, delta, half, grid)` overload gathers only the solids sharing the swept box's cells
  (deduped by a per-query stamp). `world` collides against the grid and draws occupied cells in cyan
  with **F6** (`--grid`). Verified on lavapipe: the grid overlay tiles the block field, movement and
  pickups still work (autopilot collects), no validation errors.
- [ ] **M41 — Separable downsampled bloom**: bright-pass + downsample the HDR scene to a half-res
  target, separable horizontal/vertical Gaussian blur, then add it back in composite.

Later: transparency/particle depth-sorting, hot-reload shaders, full PBR, save graphics settings via
KeyValueStore, FXAA, second/ortho viewport.
