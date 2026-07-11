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

### Iteration 3 — planned
- [ ] **M26 — Normal mapping**: tangent-space normal maps (derivative-based TBN, no vertex-format
  change) + a normal-map atlas through the glTF pipeline, for per-pixel surface bump under lights.
- [ ] **Bloom / tonemap post-processing**: its own focused effort — needs an offscreen HDR
  render-target refactor (scene → offscreen → bright/blur → composite → swapchain) touching every
  app's render path, so it gets dedicated care (night lamps + coins would bloom).
- [ ] World-space (billboarded) 3D particles; frustum culling; native scene serialization + a
  debug stats overlay.
