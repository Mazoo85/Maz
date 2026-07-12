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
- [x] **M41 — Separable downsampled bloom**: a `BloomChain` runs three half-res fullscreen passes —
  bright-pass + 2×2 downsample of the HDR sceneColor, then a separable horizontal and vertical
  9-tap Gaussian (ping-pong between two float targets). The composite samples the blurred result
  (a second sampler) and adds it scaled by strength; the in-shader single-pass bloom is gone. Wider,
  smoother glow at lower cost (O(2n) taps, ¼ the pixels). Verified on lavapipe: orbs (2D) and cube
  (3D) pixel-identical with bloom off, water/world show a broader soft glow with bloom on, no
  validation errors across the new render passes.

**Iteration 10 complete** (spatial-grid broadphase; separable downsampled bloom).

### Iteration 11 — "Materials & settings" (in progress)
Evaluation: meshes were purely diffuse (no glossy highlights), and graphics/quality choices weren't
persisted. This iteration:
- [x] **M42 — Specular/roughness material**: `Renderer::Material` (albedo, normal, emissive,
  roughness, specular) + `drawMeshMaterial`. mesh.frag adds a Blinn-Phong sun highlight gated by the
  specular strength (packed into the spare `camPos.w`) and roughness (spare `emissive.w`); matte
  defaults (specular 0) leave every existing draw byte-identical. The `cube` demo opts in and shows a
  glossy highlight sweeping across its faces. Verified on lavapipe: highlight visible in the spin
  montage, orbs/matte unchanged, no validation errors.
- [x] **M43 — Persist graphics settings**: `world` loads bloom/tonemap/wireframe from a
  `settings.ini` (via `KeyValueStore` at `platform::prefPath`) and re-applies them on launch; F7
  (bloom), F8 (tonemap), and F4 (wireframe) toggle and immediately save, and a HUD line shows the
  live state. Verified on lavapipe: an externally-written `bloom=0/tonemap=1/wireframe=1` is loaded
  and applied (wireframe scene, tonemap look, no bloom) with the HUD matching.

**Iteration 11 complete** (specular/roughness material; persistent graphics settings).

### Iteration 12 — "Shadow & post polish" (in progress)
Evaluation: shadows used a hard 2×2 PCF (blocky edges), and post-processing had no color grading.
This iteration:
- [x] **M44 — Soft shadows**: `shadowFactor()` widened to a 5×5 PCF kernel (25 taps at 1.5-texel
  spread) for a soft penumbra. Shader-only; every shadowed 3D app benefits. Verified on lavapipe:
  the `model` house casts a soft self-shadow with no blocky edges, no validation errors.
- [x] **M45 — Vignette + color grade**: the composite gained an opt-in grade (soft vignette +
  saturation + contrast) via a second push-constant vec4 and `Renderer::setColorGrade`; off by
  default, so bloom-off/tonemap-off apps stay byte-identical. VILLAGE QUEST wears a cinematic grade
  that deepens at night. Verified on lavapipe: the village shows darkened corners + richer color,
  orbs (grade off) unchanged, no validation errors.

**Iteration 12 complete** (soft shadows; composite color grade).

### Iteration 13 — "Lens polish & navigation" (in progress)
- [x] **M46 — Chromatic aberration**: an opt-in radial RGB split in the composite (third push vec4,
  `Renderer::setChromaticAberration`); off by default so other apps are unchanged. VILLAGE QUEST
  enables a gentle amount. Verified on lavapipe: clear red/cyan fringing on the corner HUD text and
  edge silhouettes, no validation errors.
- [x] **M47 — World minimap**: `world` draws a top-down corner radar — a translucent panel with the
  blocks (gray), uncollected pickups (gold), and the player (cyan) plotted from world X/Z into the
  panel via 2D sprites. Verified on lavapipe: the radar mirrors the block field with the player and
  pickups placed correctly, 3D scene unaffected, no validation errors.

**Iteration 13 complete** (chromatic aberration; world minimap).

### Iteration 14 — "Grain & particle forces" (in progress)
- [x] **M48 — Film grain**: opt-in animated hashed noise added at the end of the composite
  (`Renderer::setFilmGrain(strength, time)`, packed into the third push vec4); off by default.
  VILLAGE QUEST adds a gentle moving grain. Verified on lavapipe: clear speckle across a cropped sky
  region, no validation errors.
- [x] **M49 — Particle attractor**: `fx::ParticleSystem::setAttractor(x, y, strength, swirl)` adds a
  radial pull toward a point plus a perpendicular swirl each update (a vortex); `clearAttractor`
  restores free flight. ORB RUN's title screen emits an ambient cloud that the attractor swirls into
  an orbiting vortex behind the logo (cleared when a round starts). Verified on lavapipe: the title
  shows a clear particle vortex, gameplay bursts unaffected, no validation errors.

**Iteration 14 complete** (film grain; particle attractor/vortex).

### Iteration 15 — "Foundations: automated testing" (in progress)
Self-directed evaluation: the engine's biggest *process* gap is that everything was verified by
hand (screenshots) or by "exits 0" smoke tests — no automated protection against logic or visual
regressions. Highest-leverage fix before more features: real testing.
- [x] **M50 — Unit test suite**: a dependency-free `CHECK`/`CHECK_NEAR` runner (`tests/unit/main.cpp`
  → `maz_unit_tests`, wired into ctest) covering `math` (Vulkan perspective Y-flip, ortho2D mapping,
  vector identities), `game::Collision` (AABB overlap, slideMove stop + slide), `game::SpatialGrid`
  (gather locality, grid vs vector slideMove parity, occupied-cell count), `ecs::World`
  (create/destroy/id-reuse, add/get/has/remove, each/view iteration), `game::Shake` (trauma clamp +
  decay), and the `fx` particle attractor. 44 checks, all passing; ctest is now 10/10.
- [x] **M51 — Golden-image regression harness**: `tools/golden.sh` renders each app on lavapipe
  under Xvfb and diffs against committed references (`tests/golden/`) using a **per-app RMSE
  tolerance** — tight (0.03) for deterministic scenes like `cube`/`water`, looser (0.20–0.22) for the
  time-animated `world`/`village` — so it fails on structural breaks (broken pass, wrong colours,
  missing geometry) without false-positiving on animation jitter. Integrated into ctest (self-skips
  without a GPU). Verified both directions: clean runs pass (11/11 ctest), and a deliberately altered
  cube texture is caught (RMSE 0.039 > 0.03). Finding along the way: the flat first attempt missed
  the change while false-positiving on `world`, which is what drove the per-app thresholds.

**Iteration 15 complete** (unit tests + golden-image regression harness — the engine now has
automated logic and visual regression protection).

### Iteration 16 — "Rendering correctness" (in progress)
Self-directed: the biggest backlog items (material-UBO refactor, instancing) are large shared-render
refactors best given their own iterations. This iteration takes two foundational *correctness* wins
that the new test infra can verify rigorously.
- [x] **M52 — Texture mipmaps**: `VulkanTexture::create` now allocates a full mip chain and blit-
  generates it (`vkCmdBlitImage`, linear downsample, per-level layout transitions). The sampler uses
  `minFilter LINEAR` + `mipmapMode LINEAR` (trilinear) so minified/distant surfaces anti-alias, while
  `magFilter` stays NEAREST so near surfaces and the 2D UI keep their crisp look. Verified on
  lavapipe: scene3d's distant checker floor is smooth instead of shimmering, no validation errors;
  golden references re-recorded (intended change), ctest 11/11.
- [x] **M53 — Ray vs AABB queries**: `game::raycastAabb` (slab method, handles ray-inside and
  parallel-slab cases, respects maxDist) and `game::raycast` (nearest hit over a list, returns the
  struck index). Pure logic → 12 new unit checks (56 total). `world` casts a forward ray from the
  camera each frame and outlines the looked-at block in orange via the debug-line renderer — a worked
  targeting/interaction example. Verified: unit tests pass, the static-spawn frame shows the aimed
  block highlighted, no validation errors, ctest 11/11.

**Iteration 16 complete** (texture mipmaps; ray/AABB queries + look-at targeting).

### Iteration 17 — "Uniform system refactor" (in progress)
Self-directed: with golden-image regression protection in place, take on the #1 foundational item —
retire the mesh push-constant packing.
- [x] **M54 — Per-frame scene UBO + slim push constant**: the mesh push constant was 56 floats /
  224 bytes (mvp+model+lightVP+camPos+material), *over* Vulkan's 128-byte guaranteed limit (a
  portability risk) and out of spare slots (roughness/specular were crammed into `emissive.w` /
  `camPos.w`). Moved the per-frame data (viewProj, lightVP, camPos) into the existing set-2 scene
  UBO (now `VERTEX|FRAGMENT`; `flush` writes the matrices once per frame, `setLighting` writes only
  its own region so they don't clobber each other). The push now carries just `model` + two material
  vec4s = **96 bytes**, cleanly under the limit, with named material fields instead of packed
  leftovers. `mesh.vert`/`mesh.frag` read the matrices/camPos from the UBO. Verified: build clean,
  ctest 11/11, and the golden check is **pixel-equivalent** against the pre-refactor references
  (model RMSE 0, cube within spin jitter) — proving the refactor is behaviour-preserving. Village
  (all lighting/fog/spot/emissive/specular features) renders identically. Bonus: fixed a latent bug
  in the golden harness where the RMSE parser didn't handle scientific-notation values near zero.

**Iteration 17 complete** (per-frame scene UBO; push constant back under 128 bytes).

### Iteration 18 — "Instanced rendering" (in progress)
Self-directed: with the per-draw push constant now slim (M54), take on the #1 remaining performance
item — draw many copies of a mesh without one draw call each.
- [x] **M55 — Instanced mesh rendering**: added a second graphics pipeline
  (`mesh_instanced.vert`) that reads the per-instance model matrix from a **second vertex binding**
  (`VK_VERTEX_INPUT_RATE_INSTANCE`, four vec4 attributes = one mat4), leaving the original `drawMesh`
  path byte-for-byte unchanged. New `Renderer::drawMeshInstanced(mesh, models16, count, material)`
  packs the caller's matrices into a per-frame host-visible instance buffer (budget 8192) and issues
  a single `vkCmdDrawIndexed(indexCount, count, …)`. The new `instances` demo animates **484 cubes
  (bob + spin) in one instanced draw call** with an orbiting camera and fog. Found and fixed a bug
  where `flush()` early-returned on frames with only instanced draws (no regular `drawMesh` calls),
  which left the field invisible. Verified on lavapipe: the 484-cube field renders and animates
  correctly, no validation errors; new `instances_headless_smoke` + golden reference wired into
  ctest → **12/12**, existing apps' goldens unchanged (isolated pipeline, no regression).

**Iteration 18 complete** (instanced mesh rendering; 484 cubes in one draw call).

### Iteration 19 — "Transparency" (in progress)
Self-directed: the mesh path was opaque-only — a hard limitation for any real scene (glass,
water panes, holograms, fade-outs). Fill that gap.
- [x] **M56 — Transparent (alpha-blended) mesh rendering**: added a third mesh pipeline with alpha
  blending (`SRC_ALPHA`/`ONE_MINUS_SRC_ALPHA`) and **depth-write disabled** (depth-test still on),
  so translucent surfaces test against the opaque scene but don't occlude each other. New
  `Renderer::drawMeshTransparent(mesh, model, material, opacity)` collects translucent draws into a
  separate list; at flush they are **sorted back-to-front by camera distance** and drawn after all
  opaque + instanced geometry. The shared `mesh.frag` now outputs `material1.y` as opacity (opaque
  draws set it to 1.0, so they're unchanged). Transparent meshes receive full lighting/fog but do
  not cast shadows. New `glass` demo: three colored panes blended over five opaque colored pillars,
  camera orbiting so the tints sweep across the geometry. Verified on lavapipe: the pillars read
  clearly through the glass and overlapping panes stack their tints correctly, no validation errors;
  new `glass_headless_smoke` + golden reference wired into ctest → **13/13**, and every existing
  app's golden is unchanged (opaque path byte-identical), proving no regression.

**Iteration 19 complete** (transparent mesh rendering; depth-sorted alpha blending).

### Iteration 20 — "Navigation" (in progress)
Self-directed: the engine's rendering is deep but its *gameplay* systems are comparatively thin.
The single most reusable missing AI primitive is pathfinding — any game with a moving agent (and
the eventual Zomboid port's zombies) needs it. Build it as pure, unit-testable logic.
- [x] **M57 — A\* grid pathfinding**: header-only `game::NavGrid` — a uniform X/Z navigation grid
  of walkable/blocked cells with 8-directional A\* (orthogonal cost 1, diagonal √2, octile-distance
  heuristic, **no diagonal corner-cutting**), deterministic tie-breaking, and world↔cell mapping
  (`worldToCell`/`cellToWorld`/`blockWorldBox`, `findPath`/`findWorldPath`). No GPU or windowing
  deps, so it unit-tests headlessly (84 checks total now pass) covering the open-grid shortest
  diagonal, routing around a wall, an unreachable sealed goal, blocked endpoints, world round-trips,
  and the corner-cutting rule. New `maze` demo: a walker crosses a 20×20 obstacle field, re-planning
  each leg with A\* and following the waypoints; the route is drawn as a debug polyline. Verified on
  lavapipe (agent tracks the path to the goal, no validation errors); new `maze_headless_smoke` +
  golden reference → ctest **14/14**, every existing golden unchanged.

**Iteration 20 complete** (A* grid pathfinding + maze demo).

### Iteration 21 — "Steering" (in progress)
Self-directed: pathfinding produces a route, but agents need to *move* along it convincingly and
not pile into each other. Build the steering layer directly on top of M57.
- [x] **M58 — Steering behaviors**: header-only `game::Steering` — Reynolds-style `seek`, `flee`,
  `arrive` (eases to a stop inside a slow radius), `separation` (inverse-square push off crowding
  neighbors), and `followPath` (arrive at each waypoint, auto-advancing the index), plus `integrate`
  (semi-implicit Euler with maxForce/maxSpeed clamps). Forces are accelerations that compose by
  addition, so a caller sums the behaviors it wants and integrates once. Pure vector math →
  unit-tested headlessly (96 checks total): limit clamping, seek/flee direction, arrive speed
  ramp, separation push-off, integrate speed cap, a full seek-to-target convergence sim, and
  followPath index advancement. New `crowd` demo composes M57+M58: 14 agents each A*-route through
  the maze to a roving goal, follow the waypoints, and separate so the flock streams through the
  corridors without stacking or clipping. Verified on lavapipe (agents spread and flow toward the
  goal, no validation errors); new `crowd_headless_smoke` + golden → ctest **15/15**, every existing
  golden unchanged.

**Iteration 21 complete** (steering behaviors + crowd demo).

### Iteration 22 — "Tweening" (in progress)
Self-directed: the demos hand-roll `sin`-based motion everywhere and there is no reusable way to
animate a value over time — yet UI, cameras, moving platforms, doors, and gameplay juice all need
exactly that. Build a general easing/tween toolkit (cross-cutting, not AI-specific this time).
- [x] **M59 — Tweening + easing**: header-only `maz::anim` (new module) — 15 Penner-style easing
  curves via `ease(Ease, t)` (t auto-clamped; Back/Elastic deliberately overshoot), a generic
  `mix`, and a `Tween` time-cursor with `Once`/`Repeat`/`PingPong` looping that exposes raw
  `progress()`, curved `eased()`, and `sample(from, to)` to interpolate any float/glm-vector/color.
  One Tween can drive many values. Pure math → unit-tested headlessly (148 checks total): endpoint
  pinning for all 15 curves, known curve values, input clamping, Back overshoot, the three loop
  modes, ping-pong direction reversal, and zero-duration handling. New `tween` demo: 8 markers on a
  single shared ping-pong tween, each drawn with a different curve so the motion differences
  (linear vs. accelerating vs. spring vs. bounce) are directly comparable; a second tween color-
  fades the floor. Fixed an ADL ambiguity where `Tween::sample` on a vector type saw both `anim::mix`
  and `glm::mix` (qualified the call). Verified on lavapipe (markers stagger by curve, no validation
  errors); new `tween_headless_smoke` + golden → ctest **16/16**, every existing golden unchanged.

**Iteration 22 complete** (tweening + easing curves).

### Iteration 23 — "Interactive UI" (in progress)
Self-directed: the engine can draw text and sprites but has no *interactive* UI — no buttons, no
menus. Every shippable game needs a main menu, a pause screen, settings. Build the widget layer.
- [x] **M60 — Immediate-mode UI**: header-only `ui::Context` — an IMGUI-style widget set
  (`panel`, `label`, `button`, `toggle`, `slider`) that both draws (via the 2D sprite + font path)
  and returns interaction. Widgets are keyed by a caller id so the context tracks the hovered
  ("hot") and pressed ("active") widget across frames: a button only fires when press *and* release
  land on the same widget, and a slider keeps dragging even when the pointer slips off the track.
  Draw calls are guarded so a renderer-less/headless context still runs the full interaction logic —
  which is how the unit tests exercise it (167 checks total): rect hit-testing, slider value
  mapping + clamping, the button press/release-elsewhere state machine, toggle flip, and slider
  drag. New `menu` demo: a settings screen (buttons + two toggles + a volume slider) driven by a
  deterministic self-playing cursor for stable golden capture, handing over to a real mouse the
  moment one moves. Verified on lavapipe (widgets highlight/toggle/slide, no validation errors); new
  `menu_headless_smoke` + golden → ctest **17/17**, every existing golden unchanged.

**Iteration 23 complete** (immediate-mode UI + menu demo).

### Iteration 24 — "Serialization" (in progress)
Self-directed: the engine has runtime systems but no way to persist structured state beyond simple
key/value — no save games, no level files, no foundation for an editor or content pipeline. Build
the binary serialization backbone.
- [x] **M61 — Binary serialization + scene save/load**: header-only `maz::io` — `ByteWriter`
  (append POD / string / POD-vector, plus a magic+version header) and `ByteReader` (read them back
  with bounds checks so truncated or corrupt input sets `ok() == false` instead of over-reading),
  plus `writeFile`/`readFile`. Host byte order (fine for the LE desktop targets; a swap layer can
  slot in later). Pure logic → unit-tested headlessly (183 checks total): mixed-scalar/POD/string/
  vector round-trip, header validation, wrong magic and wrong version rejection, and two truncation
  cases (a cut POD and a cut length-prefixed string) that must fail cleanly. New `persist` demo:
  authors a 14-prop scene, serializes it to a real file under the pref dir, **clears the in-memory
  copy**, reads the file back, and renders the reconstructed scene — proving an end-to-end disk
  round-trip (log confirms `wrote=1 loaded=1 (14 props, 604 bytes)`). Verified on lavapipe (the
  reloaded props render with their saved transforms/colors, no validation errors); new
  `persist_headless_smoke` + golden → ctest **18/18**, every existing golden unchanged.

**Iteration 24 complete** (binary serialization + scene save/load).

### Iteration 25 — "State machines" (in progress)
Self-directed: the AI stack has locomotion (pathfinding + steering) but no decision layer, and the
same primitive underpins game flow (menu/playing/paused) and animation states. Build a generic FSM
and use it to cap the AI stack.
- [x] **M62 — Finite state machine**: header-only `game::StateMachine<StateId>` — states carry
  optional onEnter/onUpdate/onExit callbacks; transitions are guarded predicates evaluated every
  `update()`, with `addAnyTransition` for from-any-state rules. Evaluation is deterministic: any-
  transitions first (registration order), then the current state's, first true guard wins, one
  switch per update, then the (new) state's onUpdate runs. Pure logic → unit-tested headlessly (198
  checks total): enter/exit/update side effects fire correctly, a Patrol→Chase→Return→Patrol cycle
  with the switch count, that a left state's onUpdate stops running, and that an any-transition
  takes priority over a would-fire per-state one. New `guard` demo composes M62 with M58 steering:
  4 guards run a Patrol/Chase/Return FSM — patrol the posts, flip to Chase (seek) when the roving
  intruder comes within range, and Return to the nearest post when it escapes — color-coded
  green/red/amber by state (the HUD's "N chasing" tracks live state). Verified on lavapipe (guards
  near the intruder turn red and pursue while distant ones stay green, no validation errors); new
  `guard_headless_smoke` + golden → ctest **19/19**, every existing golden unchanged.

**Iteration 25 complete** (finite state machine + guard AI).

### Iteration 26 — "Sprite animation" (in progress)
Self-directed: the 2D toolkit (sprites, tilemaps, text, particles) has no way to animate a sprite
through frames — the one missing primitive for 2D characters (walk cycles, explosions, idle bobs).
- [x] **M63 — Sprite-sheet (flipbook) animation**: header-only `anim::SpriteAnim` — plays a list of
  `SpriteFrame` UV rects (built from a regular grid sheet via `gridFrames(cols, rows, first,
  count)`) at a fixed fps, with looping (wrap) or one-shot (clamp + `finished()`) modes; the current
  `frame()` copies straight into `SpriteDesc`'s uvMin/uvMax. Pure frame timing → unit-tested
  headlessly (220 checks total): grid UV geometry (strip + 2x2 row-major), fps-paced frame advance,
  loop wrap, one-shot clamp/finished/reset, and a single-frame no-op clip. New `sprites` demo:
  generates an 8-frame sheet at runtime (a dot orbiting a ring), then plays it back on a large hero
  sprite plus a 10x6 grid started at staggered phases, so a diagonal wave of motion sweeps the grid
  (the HUD tracks the hero's live frame). Verified on lavapipe (the phase wave is clearly visible,
  no validation errors); new `sprites_headless_smoke` + golden → ctest **20/20**, every existing
  golden unchanged.

**Iteration 26 complete** (sprite-sheet flipbook animation).

### Iteration 27 — "Event bus" (in progress)
Self-directed: the engine has many systems (audio, particles, UI, scoring) but they'd have to call
each other directly to react to gameplay. A publish/subscribe hub is the standard decoupling glue —
one of the last missing architectural primitives.
- [x] **M64 — Type-safe event bus**: header-only `core::EventBus` — `subscribe<T>(fn)` returns a
  token, `emit<T>(event)` delivers to every subscriber of exactly that type (isolated by
  `std::type_index`), `unsubscribe(token)` removes one. Dispatch iterates a **snapshot** of the
  handler list, so a handler may subscribe/unsubscribe or emit further events mid-dispatch without
  invalidating the loop. Pure logic → unit-tested headlessly (231 checks total): multi-subscriber
  fan-out in order with payload, type isolation (emitting one type doesn't call another's handlers),
  unsubscribe stops delivery to just that handler, no-subscriber no-op, and self-unsubscribe during
  dispatch (re-entrancy). New `events` demo: one emitter fires an ImpactEvent every 0.35s and three
  independent subscribers react — a particle burst, a score/energy tally, and an expanding ring —
  none referencing the others; the HUD shows each subscriber's own counter. Verified on lavapipe
  (rings + bursts bloom at each impact, deterministic RMSE 0, no validation errors); new
  `events_headless_smoke` + golden → ctest **21/21**, every existing golden unchanged.

**Iteration 27 complete** (type-safe event bus).

### Iteration 28 — "Parallelism" (in progress)
Self-directed: the whole engine runs single-threaded, yet Phase 1's "thread pool + job system" was
never built — the foundation that lets any heavy data-parallel work (image gen, particle/transform
updates, culling, batched pathfinding) use every core.
- [x] **M65 — Job system / thread pool**: header-only `core::JobSystem` — a fixed worker pool with a
  task queue; `submit<F>(f)` runs a callable on a worker and returns a `std::future` for its result,
  `parallelFor(begin, end, fn, grain)` and `parallelRanges(begin, end, fn)` split an index range
  into chunks across the workers and block until the whole range is done. Unit-tested headlessly
  (239 checks total): a 10k-element parallelFor writing distinct indices (race-free, correct
  values), every index visited exactly once (atomic tally), parallelRanges tiling a range exactly
  once, futures carrying results (sum), empty/inverted ranges as no-ops, and a single-worker pool.
  New `jobs` demo: computes a 1024x576 Julia fractal single-threaded, then via `parallelFor`,
  displays the (identical) image and reports the timings — a measured **~3.85x speedup on 4 threads
  with byte-identical output** (`identical=1` in the log), so parallelism changes only speed. New
  `jobs_headless_smoke` + golden (RMSE ~0.004, HUD timing variance absorbed) → ctest **22/22**,
  every existing golden unchanged.

**Iteration 28 complete** (job system / thread pool).

### Iteration 29 — "Asset cache" (in progress)
Self-directed: every app creates textures/meshes ad hoc with no sharing or lifetime tracking — the
same asset loads many times. A reference-counted, dedup-by-key resource cache is the core of an
asset manager and a prerequisite for any content pipeline.
- [x] **M66 — Resource cache / asset manager**: header-only generic `core::ResourceCache<Key,T>` —
  `acquire(key, loader)` builds the value via the loader on first request and returns the same
  cached instance (bumping a refcount) on later requests; `release(key, onEvict)` drops a reference
  and, at zero, runs an unload callback and erases the entry; plus `find`, `refCount`, `size`, and
  `loads()`/`hits()` stats. Storage is `std::unordered_map`, so a `T&` from acquire stays valid
  until that key is evicted. Pure logic → unit-tested headlessly (271 checks total): load-once +
  dedup (loader called once, same instance, hit counted), distinct keys as separate loads, refcount
  down to eviction with the callback firing the stored value, no-op release of unknown keys, `find`
  not touching refcounts, a 100-acquire/5-key stress (5 loads + 95 hits), and clear() evicting all.
  New `assetcache` demo: a 240-tile mosaic whose colors resolve through the cache to **only 8 GPU
  textures (232 cache hits, 97% saved)**, shown in the HUD. Renamed the app binary to `assetcache`
  to avoid clashing with the staged `bin/assets/` font directory. Verified on lavapipe (mosaic
  renders, RMSE 0, no validation errors); new `assetcache_headless_smoke` + golden → ctest
  **23/23**, every existing golden unchanged.

**Iteration 29 complete** (resource cache / asset manager core).

### Iteration 30 — "Skeletal animation" (in progress)
Self-directed: the biggest remaining capability gap for 3D games — deforming a mesh with a bone
hierarchy (animated characters). Tackled risk-first: build the math as pure logic and drive it
through the EXISTING dynamic-mesh path (CPU skinning), so no vertex-format or shader change can
regress the mesh pipeline.
- [x] **M67 — Skeletal animation core**: header-only `anim::Skeleton` — joints stored parents-
  before-children with a parent index and a rest-pose local transform; `setJoints` precomputes each
  joint's global bind matrix (parent-chained) and its inverse. `computeGlobals` chains an animated
  pose's local transforms into model space; `computeSkinning` returns `skin[i] = globalPose[i] *
  inverseBind[i]`, which is the identity at rest (a strong correctness invariant). Pure matrix math
  → unit-tested headlessly (287 checks total): rest-pose skin == identity, global-bind child
  placement, transform chaining (translate root -> child follows), and rigid rotation moving a
  bound vertex to the expected spot for both a root- and a child-bound vertex. New `skeleton` demo:
  a tapered tube bound to an 8-bone chain, each vertex weighted to its two nearest bones; a
  travelling bend wave animates the pose, the mesh is CPU-skinned (position + normal blend) and
  streamed via `updateMesh`, and the bone chain is drawn as a debug line over the deforming skin —
  the tube smoothly S-curves with no faceting at the joints. Verified on lavapipe (448 vertices
  skinned, no validation errors); new `skeleton_headless_smoke` + golden → ctest **24/24**, every
  existing golden unchanged.

**Iteration 30 complete** (skeletal animation core + CPU skinning).

### Iteration 31 — "Animation clips" (in progress)
Self-directed: the skeleton can be posed, but nothing yet turns authored keyframes into a pose or
blends between animations — the playback layer that makes skeletal animation usable (idle/walk/run
clips, cross-fades).
- [x] **M68 — Keyframe animation clips + blending**: header-only `anim::AnimClip` — per-joint
  translation/rotation/scale keyframe tracks; `sample(time, rest, out)` interpolates each track
  (vec3 lerp, quaternion **slerp**) into per-joint local poses, wrapping time when looping and
  falling back to the rest pose for unkeyed tracks. `blendPoses(a, b, w)` cross-fades two poses
  (lerp T/S, slerp R) — the basis of animation state blending. `posesToLocals` feeds the result to
  `Skeleton::computeSkinning`. Pure math → unit-tested headlessly (306 checks total): vec3 lerp +
  endpoint clamp, quaternion slerp midpoint (45 deg) and clamp, `JointPose::matrix` identity, clip
  sampling with loop-wrap, unkeyed-track rest fallback, and pose blend (translation lerp + rotation
  slerp at 22.5 deg + weight clamp). New `animclip` demo: two authored looping clips (a travelling
  "wave" of keyframed joint rotations and a "coil") sampled every frame and cross-faded by an
  oscillating weight, driving the M67 skinned tube; the HUD shows the live blend weight. Verified on
  lavapipe (the tube morphs smoothly between waving and coiling, no validation errors); new
  `animclip_headless_smoke` + golden → ctest **25/25**, every existing golden unchanged.

**Iteration 31 complete** (keyframe animation clips + blending).

### Iteration 32 — "2D physics" (in progress)
Self-directed: the engine can *detect* collisions (AABB overlap, raycast, slide) but has no
*dynamics* — nothing has velocity, mass, or bounce, and objects can't resolve contacts or stack. A
real rigid-body layer is a genuine capability gap for physics-driven games.
- [x] **M69 — 2D impulse physics**: header-only `game::PhysicsWorld2D` — circle bodies with
  position, velocity, inverse mass (0 = static), and restitution; `step` integrates gravity, then
  for a few iterations resolves circle-circle overlaps with a **normal impulse** (only when the
  bodies are closing, scaled by `(1+e)` and inverse masses) plus **Baumgarte positional correction**
  with slop (so resting stacks don't sink or jitter), and bounces bodies off a static box.
  `collideCircles`/`collideBounds` are exposed as free functions. Pure logic → unit-tested
  headlessly (618 checks total): head-on elastic collision conserves momentum and separates, a
  static body is unmoved by impact while the dynamic one bounces, positional correction increases a
  heavy overlap's separation toward the contact distance without over-shooting, a wall reflects
  velocity by restitution (5 -> -2.5) and clamps position, and a ball under gravity settles ON an
  inelastic floor across 300 steps and never sinks below it. New `physics` demo: 45 varied balls
  drop, bounce off the walls and each other, and stack into a stable non-overlapping pile.
  Verified on lavapipe (the pile is stable, no interpenetration, RMSE 0, no validation errors); new
  `physics_headless_smoke` + golden → ctest **26/26**, every existing golden unchanged.

**Iteration 32 complete** (2D impulse physics).

### Iteration 33 — "Animation controller" (in progress)
Self-directed: skeleton (M67) and clips (M68) exist as separate primitives, but nothing ties them
into the stateful controller a game actually drives — named clips with timed cross-fades between
them. That's the standard animation-graph top layer.
- [x] **M70 — Animation controller**: header-only `anim::Animator` — a clip library (`addClip`/
  `findClip`) plus a two-track cross-fader. `play(id_or_name, fadeDuration)` moves the current clip
  to an outgoing track and fades to the new one; `update(dt)` advances both tracks and the fade
  timer; `pose()` returns the blended per-joint result (both clips keep animating during the fade,
  so motion doesn't freeze). The first clip snaps in; replaying the active clip is a no-op.
  Pure logic → unit-tested headlessly (636 checks total): rest pose when idle, first-clip snap,
  A->B cross-fade sampled at start (all A), midpoint (50/50 blend) and end (all B), fadeProgress,
  and no-restart on repeat. New `animator` demo: a skinned tube driven by the controller through
  three named clips (idle / wave / coil), auto-cycling every 3s with a 0.7s cross-fade; the HUD
  shows the active clip and live fade %. Verified on lavapipe (the tube eases between animations,
  the HUD caught a "cross-fading 19%" transition, no validation errors); new
  `animator_headless_smoke` + golden → ctest **27/27**, every existing golden unchanged.

**Iteration 33 complete** (animation controller with cross-fades).

### Iteration 34 — "Behavior trees" (in progress)
Self-directed: the AI stack has a finite state machine (M62), but FSMs get unwieldy as behavior
grows — behavior trees are the scalable, reactive alternative every modern engine ships.
- [x] **M71 — Behavior trees**: header-only `game::bt` — `Status` (Success/Failure/Running),
  `Action`/`Condition` leaves wrapping gameplay via `std::function`, and `Sequence` (AND, stop at
  first non-Success), `Selector` (priority OR, stop at first non-Failure), and `Inverter`
  composites, with variadic `sequence()`/`selector()` builders and a `BehaviorTree` root holder. The
  composites are **reactive (memoryless)**: every tick re-evaluates from the first child, so a
  higher-priority branch pre-empts a running lower-priority one the instant its condition flips —
  the behaviour reactive agents want. Pure logic → unit-tested headlessly (654 checks total):
  sequence/selector/inverter/condition truth tables, short-circuit (later children not ticked after
  a decisive result), and a reactive-priority scenario where flipping an alarm flag pre-empts the
  fallback branch and then falls back again. New `behavior` demo: 5 agents each driven by a
  `selector(flee, chase, patrol)` tree — they reactively pre-empt patrol to chase and chase to flee
  as an intruder crosses range rings, coloured by the active leaf (green/red/yellow), composing the
  tree with M58 steering. Verified on lavapipe (agents nearest the intruder flip to flee while
  distant ones patrol, HUD tallies live counts, no validation errors); new
  `behavior_headless_smoke` + golden → ctest **28/28**, every existing golden unchanged.

**Iteration 34 complete** (reactive behavior trees).

### Iteration 35 — "Box physics" (in progress)
Self-directed: the 2D physics (M69) was circle-only. Boxes + friction are the missing pieces for
platformers, crates, and walls — extended risk-first so the proven circle path stays byte-identical.
- [x] **M72 — Box colliders + friction**: `Body2D` gains a `shape` (Circle | Box), box half-extents,
  and a `friction` coefficient. New contact generators — box-box (separate on the axis of least
  penetration), circle-box (closest-point, with an inside-box fallback), plus the existing circle-
  circle — feed a **unified** `resolveContact` that applies the normal impulse, a **Coulomb friction**
  tangential impulse (clamped to `mu * jn`), and positional correction. `PhysicsWorld2D::step` now
  dispatches through the shape-aware `collide`, and `collideBounds` uses per-axis extents. Friction
  defaults to 0 and `resolveContact` matches the old circle math exactly, so the circle path is
  **byte-identical** (the `physics` golden is unchanged — verified). Pure logic → unit-tested
  headlessly (663 checks total): box-box separates on the least-penetration axis, a dynamic box and
  a ball each settle ON a static box platform (not sinking/tunnelling), the static platform never
  moves, and friction removes a sliding box's horizontal speed while a frictionless one keeps it.
  New `boxes` demo: a mix of boxes and balls drops onto three static ledges and stacks squarely with
  friction. Verified on lavapipe (mixed ball/box stacks rest stably, RMSE 0, no validation errors);
  new `boxes_headless_smoke` + golden → ctest **29/29**, every existing golden unchanged.

**Iteration 35 complete** (2D box colliders + friction).

### Iteration 36 — "App framework" (done)
Self-directed: after 18 isolated feature modules the biggest gap is the layer *above* them — the
engine had no notion of game *scenes/states* (menu, playing, paused) with a stack and overlays. That
application-framework layer is what turns single-screen demos into an actual game shell. M74 then
*proves* it by shipping a real game built from nothing but existing engine systems.
- [x] **M73 — Scene stack / game state manager**: header-only `core::SceneStack` over a `Scene`
  base with an enter/exit/pause/resume lifecycle. `push` pauses the current top and enters the new
  scene; `pop` exits it and resumes the revealed one; `replace` swaps the top; `clear` unwinds all.
  Scenes expose `blocksUpdate()` (a modal scene freezes those beneath) and `blocksRender()` (opaque
  vs. transparent overlay); `update` walks top-down stopping at the first modal scene, and `render`
  draws from the topmost opaque scene up so overlays composite over the game. Stack mutations
  requested **during** update are DEFERRED and applied afterwards, so a scene can safely pop or
  replace itself without invalidating the iteration. `render()` is a no-op hook so the core stays
  renderer-agnostic and unit-tests headless (688 checks total): push/pause/resume/pop order,
  replace, update propagation through a non-modal overlay vs. a modal one, deferred self-push, and
  clear(). New `scenes` demo: a Menu scene replaces itself with a Game scene (bouncing physics),
  which pushes a **transparent** Pause overlay — the game freezes but shows through — then pops it to
  resume; the HUD prints the live stack depth + top scene. Verified on lavapipe (the PAUSED panel
  draws over the frozen balls, "stack depth 2 top: Pause", no validation errors); new
  `scenes_headless_smoke` + golden → ctest **30/30**, every existing golden unchanged.
- [x] **M74 — CATCHER (integration game)**: the point of an engine is that the parts compose, so this
  milestone builds a *whole* small game (`apps/catcher`) out of pieces already in the tree — no new
  engine code, only assembly. The **scene stack** runs the shell (title → play → game-over, each
  scene transitioning itself via the deferred-mutation path); the **event bus** is the gameplay spine
  — a caught coin publishes a `CatchEvent` and a missed one a `HitEvent`, and independent subscribers
  turn those into score, a gold or red **particle** burst, and a **screen-shake** kick, so scoring /
  juice / feedback stay decoupled; **2D contact tests** decide paddle-vs-coin and paddle-vs-hazard;
  and the **KeyValueStore** persists the high score to the pref path across runs. Because a golden has
  to be reproducible, the game ships a deterministic **attract-mode AI** (seeded xorshift RNG, fixed
  timestep) that plays itself — tracking the nearest coin and dodging hazards — so no input or wall
  clock enters the frame. Verified on lavapipe (HUD "SCORE / LIVES / BEST", a falling gold coin, the
  paddle tracking below, "ATTRACT MODE" prompt; a catch fires its burst — no validation errors); new
  `catcher_headless_smoke` + golden (RMSE 0.0044, threshold tightened to 0.08) → ctest **31/31**, and
  every existing app's golden unchanged. This is the capstone that shows the engine is not a bag of
  demos but a set of systems that build a game together.

### Iteration 37 — "Human-editable data" (done)
Self-directed: the engine could persist data (binary `Serialize`, ini-style `KeyValueStore`) but had
no **human-readable structured format** — the thing you actually author by hand for configs, levels,
and tuning, and the thing a future editor/tool would read and write. That is the highest-leverage
foundational gap: many systems (scene description, difficulty tables, per-app settings) want to be
*data*, and data wants to be editable.
- [x] **M75 — JSON value + parser + serializer (`io::Json`)**: a header-only, zero-dependency
  `JsonValue` — a tagged union over the six JSON types with **insertion-ordered** objects (a parallel
  key list, not a re-sorting `std::map`, so parse → dump → parse is byte-stable and diffs stay small).
  Reads are total functions: `j["a"]["b"].asInt(default)` walks through missing keys and past array
  bounds to a shared null sentinel and returns the default rather than crashing, so loading partial or
  untrusted data is safe. `parseJson` is a hand-written recursive-descent scanner that is fully
  bounds-checked and **never throws** — malformed input yields a null value plus a human-readable error
  with line/column (it keeps the first/deepest error), covering unterminated containers/strings,
  missing colons/commas, bad literals, control chars, and `\uXXXX` escapes (decoded to UTF-8).
  `dump(indent)` re-serializes compact or pretty, emitting integral numbers without a spurious `.0`.
  Unit-tested to **757 checks** total: a full round-trip of a nested document, typed reads with
  defaults, chained missing-key safety, order preservation, escape/`\u` round-trips, in-code document
  building, ten distinct malformed inputs all failing cleanly, and pretty-print. New `data` demo:
  an embedded JSON document describes the clear color and eight sprites (shape / fractional position /
  size / tint / bob amplitude+speed+phase / spin), and the app parses it at runtime and renders it —
  nothing on screen is hard-coded. Verified on lavapipe (five discs bobbing at data-driven heights, a
  floor bar, two spinning boxes, the JSON-supplied title in the HUD; no validation errors); new
  `data_headless_smoke` + golden (RMSE 0.0079, threshold 0.12) → ctest **32/32**, every existing
  golden unchanged.
- [x] **M76 — On-disk JSON levels (close the data pipeline)**: M75 gave the engine a JSON format but
  every use so far read an *embedded* string. This makes it real — content on disk that a person
  edits. `io::Json` gains file helpers (`readTextFile`/`writeTextFile`, `parseJsonFile`/
  `writeJsonFile`); a missing/unreadable file returns not-ok with a clear error so "file absent" can't
  masquerade as "parsed null". A hand-authored `assets/levels/arena.json` describes a level as a tile
  grid (an array of equal-length strings, one char per tile code), a palette (code → RGB), a solidity
  list, and pickup coordinates — the compact, diff-friendly shape a level editor would emit. It is
  staged into `bin/assets/levels/` by the engine build alongside fonts and models. The new `level`
  demo reads that file **from disk** at startup, builds a `game::Tilemap` + pickups + palette from it,
  renders it top-down, and then round-trips the parsed document straight back out to the save
  directory with `writeJsonFile` — proving the format saves as well as loads. Loader fields degrade to
  defaults on malformed input, so a partial file still draws. Unit tests (**771 checks** total) cover
  a level round-trip (tile rows + pickups survive parse→dump→parse), file write→read equality on a
  temp path, and a missing-file failing cleanly. Verified on lavapipe (a 16×11 arena with a wall
  border, interior wall pattern, red hazard tiles at the `2` codes, six pulsing gold pickups, HUD
  "loaded 'ARENA' from arena.json — 16x11 tiles, 6 pickups"; the round-trip save logged to the pref
  path; no validation errors); new `level_headless_smoke` + golden (RMSE 0.0009, threshold 0.05) →
  ctest **33/33**, every existing golden unchanged. Note: two golden-image runs must not execute
  concurrently — they share the Xvfb display and temp PNGs, which corrupts both; run them serially.

### Iteration 38 — "Config as a first-class system" (done)
Self-directed: the engine had settings scattered across code and a few app-local `KeyValueStore`
reads, but no *central* notion of a tunable — the named, typed, documented, clamped variable that any
subsystem registers and a config file drives. That is the standard engine backbone (Quake-style
cvars), high-leverage because every subsystem gets uniform, discoverable, file-driven tuning for free.
- [x] **M77 — CVar / config system (`core::CVarRegistry` + `io::Config`)**: a dependency-free registry
  of named tunables. Each entry carries a type (bool/int/float/string), a default, a human-readable
  description, and — for numbers — an optional `[lo, hi]` clamp. Values are set through typed setters
  (which clamp) or `setFromString` (which coerces `"true"`/`"3"`/`"1.5"` into the declared type and
  clamps), so the same registry serves programmatic use, command-line `name=value` flags
  (`applyAssignments`), and text configs. Registration is idempotent — re-registering a name keeps its
  current value, so start-up order doesn't clobber overrides. The whole set iterates (`entries()`) for
  a settings UI or a dump. To keep `core` at zero dependencies, the JSON glue lives one layer up in
  `io/Config.hpp`: `loadConfig` applies a parsed JSON object onto matching cvars (coercing JSON types
  into each cvar's type, ignoring unknown keys, respecting clamps), `configToJson` serializes the
  registry back out, and `loadConfigFile`/`saveConfigFile` do the disk round-trip — so `config.json`
  drives the engine. Unit-tested to **811 checks** total: registration/defaults, idempotent
  re-register, range clamping on setters, per-type `setFromString` coercion + failure on garbage,
  CLI assignment parsing, `loadConfig` with type coercion/unknown-key skipping/clamping, a
  `configToJson` round-trip into a fresh registry, and a file save→load round-trip. The new `config`
  demo registers six cvars (title, orb count/speed/hue, brightness, grid toggle), applies a JSON
  config over them, and renders a scene driven **entirely** by the resulting values — a ring of N
  hue-swept orbiting dots over an optional grid — beside a live `name = value` table of the whole
  registry. Verified on lavapipe (14 dots from `scene.orbCount=14`, orange→cyan sweep from
  `scene.hue=0.08`, grid on, table matching the config; no validation errors); new
  `config_headless_smoke` + golden (RMSE 0, threshold 0.05) → ctest **34/34**, every existing golden
  unchanged.

### Iteration 39 — "Measure the frame" (done)
Self-directed: the engine had an FPS/draw-count overlay (M29) but no real *profiler* — the tool that
attributes frame time to named, nested spans of work, which is the prerequisite for any serious
optimization ("you can't fix what you can't measure"). That is foundational infrastructure, not polish.
- [x] **M78 — Hierarchical CPU profiler (`core::Profiler`)**: nestable timing zones opened with
  `begin(name)` and closed with `end()`. Zones form a tree, so each records **inclusive** time (the
  whole span) and **self** time (inclusive minus the direct children) — together they locate a hotspot
  that either number alone would hide. Per frame each distinct zone name aggregates call count,
  inclusive, and self microseconds; across frames an exponential moving average smooths the millisecond
  readout so a live display is legible instead of flickering. The core is deliberately
  **time-source-agnostic**: `begin`/`end` take a monotonic microsecond timestamp, so unit tests and the
  golden demo feed synthetic timestamps (no wall clock) and get byte-reproducible output, while real
  code uses `nowMicros()` (steady_clock) or the `ScopedZone` RAII guard. Unit-tested to **834 checks**
  total: a hand-built nested frame verifying inclusive time per zone, self = inclusive − children at
  every level (parents small, leaves self == inclusive), depth, call counts, multi-invocation
  aggregation within a frame, per-frame reset, EMA convergence to a steady value, and an unbalanced
  `end()` being ignored rather than crashing. The new `profiler` demo replays a fixed ~16 ms frame
  (update → physics/ai/particles, render → shadow/opaque/transparent/ui) into the profiler each frame
  and draws the zone tree as an indented bar chart — bar width ∝ inclusive share, indented by depth,
  labeled with inclusive/self ms and call count. Verified on lavapipe (frame 16.00 ms; update self
  0.20 and render self 0.00 with their children summing correctly; leaves self == inclusive; no
  validation errors); new `profiler_headless_smoke` + golden (RMSE 0, threshold 0.05) → ctest
  **35/35**, every existing golden unchanged.

### Iteration 40 — "Persist the world" (done)
Self-directed: the engine could persist *bytes* (Serialize), *settings* (KeyValueStore, CVars), and
*hand-authored* data (JSON levels), but not the one thing a running game most needs to save — its live
**entity world**. That is the content backbone under save games, prefabs, and any future editor, and
it was the largest remaining foundational gap. The ECS stores components in type-erased pools, so the
question was how to serialize arbitrary component types without a full reflection system.
- [x] **M79 — ECS scene serialization (`io::SceneSerializer`)**: the standard reflection-lite answer —
  the app teaches the serializer each component type *once* by handing it a name plus to/from-JSON
  converters; the serializer type-erases those into `(World&, Entity)` operations. `saveWorld` gathers
  every entity carrying a registered component (via `each<T>`), emits them in **ascending-id order**
  (so the document is deterministic and diff-friendly) as `{ "entities": [ { "id", "components": {
  "Transform": {…}, … } } ] }`, and `loadWorld` rebuilds the world — creating an entity per record and
  adding each recognized component, silently skipping component names it doesn't know (forward/back
  compatible). Convenience `saveWorldFile`/`loadWorldFile` do the disk round-trip. It lives in the io
  layer so `ecs` stays dependency-free. Unit-tested to **852 checks** total: a three-entity world with
  mixed components saved and reloaded into a fresh world with every value verified (transform sum,
  hp sum, tag string), a dump→parse→load text round-trip, a file save→load round-trip, and an unknown
  component in a document being skipped while known ones still load. The new `ecsave` demo makes the
  round-trip literal: it builds a *source* world (seven entities with Transform / Look / Spin
  components), serializes it to JSON, loads that JSON into a **second, fresh** world, and renders the
  second world — so every disc and spinning box on screen was reconstructed from serialized data (the
  Spin component surviving proves behavior-driving data round-trips too). Verified on lavapipe (four
  discs + three rotated boxes matching the source scene, HUD "built 7 entities → serialized to JSON →
  reloaded 7"; the scene also written to the save dir; no validation errors); new
  `ecsave_headless_smoke` + golden (RMSE 0.0079, threshold 0.05) → ctest **36/36**, every existing
  golden unchanged.

### Iteration 41 — "Actions, not scancodes" (done)
Self-directed: the platform layer exposed raw device state (keyboard, mouse, gamepad) but every app
that used input reached straight for scancodes — there was no *action-mapping* layer, the standard
abstraction that lets gameplay ask about intents ("Jump", "MoveX") instead of physical keys, binds one
action to several devices at once, and makes rebinding possible. That's a Phase-1 foundational gap.
- [x] **M80 — Input action map (`input::ActionMap`)**: named button and axis actions over an
  SDL-independent core. A **button action** binds any number of sources (`Key` / `MouseButton` /
  `PadButton`) and is down if *any* is down, yielding `held` / `pressed` (edge down this frame) /
  `released` (edge up) — so Space and pad-A both fire the same Jump. An **axis action** combines
  negative/positive button pairs (each ±1) with analog pad axes (scaled, so an inverted-Y is just
  scale −1), clamped to [−1, 1] — so WASD, the arrow keys, and a thumbstick all drive one MoveX.
  `update()` takes sampler callbacks (`down(device, code)`, `analog(axis)`) rather than touching SDL,
  which keeps it unit-testable and backend-agnostic; an app wires the samplers to `platform::Input`.
  Unit-tested to **875 checks** total: edge detection across frames (press → held+pressed, hold →
  held only, release → released edge), an alternate bound source (pad) driving the same action, axis
  from a key pair (+1 / −1 / 0 when both), analog-plus-key combination clamping to 1.0, an inverted
  analog axis, and unknown actions reading neutral. The new `actions` demo drives an avatar entirely
  through mapped actions — MoveX/MoveY set velocity, Dash boosts speed, Fire spawns a flash — and feeds
  a deterministic scripted input (a pure function of the fixed-step clock) OR'd with the real device,
  so it self-plays for the golden yet stays fully playable (WASD/arrows/Space/Shift or a gamepad). A
  HUD shows each action's live state (axis bars, button held-indicators, fire count). Verified on
  lavapipe (MoveX/MoveY bars at +1.00, Fire/Dash indicators, "Fire count: 2", the avatar moved by the
  mapped axes; no validation errors); new `actions_headless_smoke` + golden (RMSE 0, threshold 0.05) →
  ctest **37/37**, every existing golden unchanged.

### Iteration 42 — "Parents and children" (done)
Self-directed: the ECS was flat and the only hierarchy in the engine was the mesh-skinning joint tree
— there was no *general* transform hierarchy, the scene-graph primitive every composite object needs
(a weapon on a hand, a turret on a tank, a moon around a planet, a UI badge pinned to a unit). Without
it, any "attached" object has to re-derive its parent's motion by hand. That's a Phase-4 foundational
gap.
- [x] **M81 — 2D transform hierarchy (`scene::TransformGraph`)**: nodes each hold a LOCAL transform
  (position, rotation, scale) plus a parent; `update()` walks the tree and composes WORLD transforms
  parent-first. Composition is the standard decomposed TRS — world rotation = parent + local, world
  scale = parent · local (component-wise), world position = parent position + parent-rotated,
  parent-scaled local position — which is exact for uniform scale and the pragmatic norm for 2D scene
  graphs. Propagation is a **memoized recursion**, so children may be created (or reparented) in any
  order relative to their parents and still resolve correctly, with a computed-flag guard so a stray
  cycle terminates instead of hanging. `localToWorld` maps a point through a node's world transform
  (for spawn points, muzzle positions, attach anchors). Unit-tested to **890 checks** total: a child
  offset following a parent's 90° rotation to the right place, scale propagation, a nested grandchild
  under a rotated middle node, `localToWorld` at the origin and an offset, reparenting changing the
  derived world position, and a lower-index child resolving correctly under a higher-index parent
  (creation-order independence). The new `solar` demo is the canonical proof: a sun → 4 planets →
  moons tree (17 nodes) where the app sets *only* each orbit pivot's local rotation to speed·t; the
  graph then sweeps every planet around the sun and every moon around its planet — the moon riding its
  planet's motion for free, which is the entire point. Verified on lavapipe (sun centered, four faint
  orbit rings, four planets each on its ring, a small moon beside each planet; HUD "sun → 4 planets →
  moons (17 nodes; parents spin, children follow)"; no validation errors); new `solar_headless_smoke`
  + golden (RMSE 0, threshold 0.05) → ctest **38/38**, every existing golden unchanged.

### Iteration 43 — "A camera that follows" (done)
Self-directed: the engine had the *pieces* for a 2D game camera — a `Camera2D` data struct the sprite
renderer honors, and a `Shake` — but no *controller* tying them into the behavior every scrolling 2D
game actually ships: track a target smoothly, don't jitter on tiny motion, and never show past the
level edge. Every app that wanted a following view had to hand-roll it. That's a foundational gap.
- [x] **M82 — 2D follow camera (`game::CameraController2D`)**: three standard behaviors layered into
  one controller. A **deadzone** box lets the target drift near the focus without scrolling (the camera
  only moves to put the target back on the box edge), killing jitter. **Smoothing** eases the focus
  toward its desired point with a frame-rate-independent exponential (`1 − e^(−k·dt)`), so scrolling
  feels weighty rather than locked. **World-bounds** clamping keeps the visible rectangle inside the
  level — the camera stops at the edge instead of revealing the void — and centers an axis whose world
  span is smaller than the view. A shake offset rides on top of the final center without feeding back
  into the follow position, and `worldToScreen` maps world points to pixels for HUD markers/culling.
  It's math-only (no renderer dependency): the app reads `center()`/`zoom()` to fill a `Camera2D`.
  Unit-tested to **918 checks** total: snap-to-target with no deadzone, a target held inside the
  deadzone vs. pushed to the edge, smoothing that approaches monotonically without overshoot and
  effectively arrives on a long step, bounds clamping at both corners and interior pass-through,
  small-world axis centering, shake shifting `center()` but not `position()`, and `worldToScreen`
  mapping the focus to the screen center with zoom-scaled offsets. The new `camera` demo tracks an
  avatar on a deterministic path around a 2600×1800 world (far larger than the screen), drawing the
  marker grid, world border, and deadzone box through the follow camera in a world-space pass and the
  HUD in a pixel-space pass. Verified on lavapipe (grid scrolled to the camera's world position, green
  deadzone box at the focus, avatar led out ahead as the view eases after it, the camera's view edge
  clamped exactly at the world boundary; HUD "cam (1960,1130) avatar (2336,932)"; no validation
  errors); new `camera_headless_smoke` + golden (RMSE 0.023, threshold 0.10) → ctest **39/39**, every
  existing golden unchanged.

### Iteration 44 — "Time, scheduled" (done)
Self-directed: the engine had a fixed-step clock and tween cursors, but no way to say "do this in 3
seconds", "spawn one every 2 seconds", or "wait, then act, then wait, then act". That delayed/repeating/
sequenced-time primitive underpins spawn waves, cooldowns, delayed effects, and scripted moments — it
was a genuine gap that every gameplay system would otherwise re-hand-roll.
- [x] **M83 — Time scheduler + sequences (`core::Scheduler`, `core::Sequence`)**: two header-only
  pieces on the fixed-step clock, so both are deterministic. `Scheduler` is fire-and-forget timers:
  `after(delay)` runs a callback once, `every(interval, repeats)` runs it a finite count or forever,
  `cancel(handle)` stops a pending one. `update(dt)` steps every timer and fires what came due —
  **catching up** across multiple intervals if a big dt lands past several (a `while` drain), and
  invoking callbacks *after* stepping so a callback may safely schedule or cancel timers without
  corrupting the in-progress update (new timers wait for the next tick). `Sequence` plays an ordered
  script — `wait(seconds)`, `call(fn)`, `span(duration, fn(progress 0..1))` — carrying leftover time
  from one step into the next within a single update and optionally looping; the tricky exact-boundary
  case (a `wait(1)` landing on a 1.0s step) still fires the following instantaneous `call` the same
  frame, because Call steps run whenever reached regardless of remaining budget (a bug the tests caught
  and the fix addressed). Unit-tested to **947 checks** total: one-shot timing (not before / exactly
  at / never again), finite repeat counts, infinite repeat with big-dt catch-up, cancel, a callback
  scheduling another timer mid-update deferring to the next tick, and for sequences the wait→call
  ordering, span progress reaching exactly 1.0, leftover-time carry across steps, looping, and reset.
  The new `fireworks` demo is entirely timer-driven: `every(0.4)` launches a rocket, each rocket's
  `after(riseTime)` fires its explosion into a particle burst at the apex, `every(2.0)` fires a finale
  volley of five, and a looping `Sequence` pulses the title glow via `span`. Verified on lavapipe
  (rising colored rockets with trails, mid-air bursts, drifting particles under gravity, HUD "active:9
  launched:10 bursts:3 particles:244"; no validation errors); new `fireworks_headless_smoke` + golden
  (RMSE 0.014, threshold 0.06) → ctest **40/40**, every existing golden unchanged.

### Iteration 45 — "One source of randomness" (done)
Self-directed: several demos hand-rolled their own little xorshift RNG — a duplication smell and, worse,
a foundational gap: the engine had no single, seeded, reproducible random source, which is what replays,
procedural generation, and deterministic tests all require. "Deterministic RNG" was explicitly on the
roadmap.
- [x] **M84 — Deterministic RNG (`core::Random`)**: one seeded PRNG for the whole engine. The core is
  **xoshiro256\*\*** (fast, statistically strong) seeded through **SplitMix64**, so even a small or zero
  seed fills the 256-bit state well and every derived value traces back to one reproducible stream. On
  top of `next()` it exposes floats/doubles in `[0,1)`, inclusive integer ranges (degenerate/reversed
  ranges handled), float ranges, `chance(p)`, a weighted index pick (zero-weight entries excluded), a
  uniform container `pick`, an in-place Fisher-Yates `shuffle`, a Box-Muller `gaussian` (with a cached
  spare), and an angle. It is stdlib-only (no glm), so `core` keeps zero dependencies. Unit-tested to
  **3262 checks** total: same-seed reproducibility vs. different-seed divergence, re-seed rewind, floats
  bounded to `[0,1)`, inclusive int ranges that never escape and reach both endpoints, float-range
  bounds, `chance` extremes, weighted picks excluding zero weights while a 9:1 weight dominates, a
  shuffle that is both deterministic for a fixed seed and a true permutation (sum + presence), and
  `pick` returning in-set elements. The new `scatter` demo generates a 520-token field from a fixed
  seed — positions sampled uniformly in a disc (radius scaled by √u for even density), each token's
  rarity from `weighted({70,20,8,2})` driving color and size — with a legend tallying the resulting
  distribution. Verified on lavapipe (an even token disc, rarer tokens larger and on top, legend
  reading Common 68% / Uncommon 23% / Rare 8% / Epic 2% — matching the weights; "same seed, same
  field"; no validation errors); new `scatter_headless_smoke` + golden (RMSE 0, threshold 0.05) →
  ctest **41/41**, every existing golden unchanged.

### Iteration 46 — "Noise shapes worlds" (done)
Self-directed: M84 gave the engine a seeded RNG, but random *points* aren't enough for procedural
content — terrain, textures, clouds, and cave systems need *smooth, spatially-coherent* randomness. The
engine had none. Perlin/fbm noise is the canonical primitive, and it builds directly on the new RNG.
- [x] **M85 — Procedural noise (`core::Noise`)**: classic Perlin gradient noise. A per-seed
  permutation table (Fisher-Yates shuffled with `core::Random`, so the same seed always yields the same
  field) feeds fade/lerp interpolation of lattice gradients, giving `noise2(x,y)` in ~[-1,1] that is
  exactly 0 at integer lattice points and continuous everywhere. `fbm2` layers octaves (fractal
  Brownian motion — rising frequency, falling amplitude) normalized back into [-1,1] for natural,
  multi-scale detail. Header-only, on top of `core::Random`. Unit-tested to **3338 checks** total:
  same-seed reproducibility vs. different-seed divergence, exact-zero at every integer lattice point in
  a 7×7 block, output bounded to ≤1.0001 across a dense 400×200 scan while still varying (>0.4 peak),
  continuity (a 0.01 input step never moves the output more than 0.1 — smooth, not white noise), fbm
  bounded + reproducible, and fbm-at-1-octave equaling plain `noise2`. The new `noise` demo generates a
  480×270 terrain heightmap once from an `fbm2` field (6 octaves), mapping height through a
  water→shallow→sand→grass→forest→rock→snow color ramp with a cheap slope hillshade for relief, then
  draws it full-screen. Verified on lavapipe (a natural continent — blue lakes, sandy shorelines, green
  forested land with shaded relief; HUD "fbm noise, 6 octaves, seed 0xA11CE5EED — same seed, same
  continent"; no validation errors); new `noise_headless_smoke` + golden (RMSE 0, threshold 0.05) →
  ctest **42/42**, every existing golden unchanged.

### Iteration 47 — "Benchmarking against Godot: UI" (done)
Directed to use Godot as the north star: evaluate its subsystems, find the highest-leverage gaps Maz can
realistically close, and build them. Honest framing first — several Godot pillars (a shipping GUI
editor, GDScript/C# VMs, console/mobile/web export, global illumination, a Jolt-grade 3D physics
engine) are **structurally out of reach in this headless sandbox**, and no loop count changes that; the
loop's real value is closing the *closable* gaps. The comparison surfaced this ranked gap list: (1) UI —
Godot's Control system (anchors + containers) vs. Maz's immediate-mode-only widgets; (2) 2D lights/
shadows; (3) 3D rigid-body physics + joints; (4) navmesh; (5) audio buses/effects/spatial; (6) blend
trees/IK. The clearest, cleanly-buildable win is #1.
- [x] **M86 — Retained UI layout (`ui::LayoutNode`), toward Godot's Control system**: Maz had only
  immediate-mode widgets with hand-typed pixel coordinates that broke at any other resolution. This adds
  Godot's exact placement model: **anchors** (`anchorMin`/`anchorMax` as per-edge fractions of the
  parent) + **margin offsets** in pixels, so `(0,0,1,1)` fills, `(0,0,1,0)+offsets` is a stretch-to-width
  top bar, `(0.5,0.5,0.5,0.5)` pins a fixed-size box to the center. Plus **container** modes —
  `HBox`/`VBox` distribute children by min-size with an `expand` flag sharing leftover space (and
  `spacing`/`pad`), `Center` centers. `layout()` walks the tree from a root rect and fills every node's
  computed `rect`, so the whole UI is resolution-responsive with no hard-coded pixel. The existing
  `ui::Rect` was extracted to a shared `ui/Rect.hpp` (with `right/bottom/centerX/centerY`) so the
  immediate-mode UI and the layout system agree on one rectangle type. Unit-tested to **3373 checks**
  total: anchor fill-with-margin, center-pin via anchors, responsiveness (a top bar staying full-width at
  two window sizes), HBox with fixed + expand + spacing, VBox equal-expander split, Center placement,
  container padding, and a VBox nested inside an HBox cell. The new `uilayout` demo assembles a real app
  shell — a top bar, a fixed-width sidebar VBox of five buttons, an expanding content panel, and a
  center-pinned modal whose own VBox stacks a title, body, and an HBox of OK/Cancel buttons — entirely
  from the layout tree. Verified on lavapipe (the full app UI renders correctly, buttons evenly stacked,
  modal centered; no validation errors); new `uilayout_headless_smoke` + golden (RMSE 0, threshold 0.05)
  → ctest **43/43**. Also bumped the `cube` golden threshold 0.03→0.05 (an unusually tight bound for a
  rotating 3D scene; the render is unchanged — verified visually — but sat marginally over at the settle
  frame). Every other existing golden unchanged.

Standing note (Godot benchmark): reaching literal parity "in every way" is not achievable here — a full
editor, scripting VMs, console/mobile/web export, GI, and a mature 3D physics engine can't be built in a
headless sandbox. The loop's honest goal is to keep closing the highest-leverage *closable* gaps; it
should not, and will not, declare total superiority over Godot.

### Iteration 48 — "Benchmarking against Godot: navigation" (done)
Continuing the Godot benchmark. With UI closed (M86), the ranked closable gaps were: navmesh
pathfinding; 2D lights/shadows; 3D rigid-body physics; audio buses/spatial; animation blend spaces.
2D lights need arbitrary-polygon fill the sprite renderer doesn't have yet (deferred behind a polygon-
draw milestone). Navmesh was the clear, cleanly-buildable win — it needs no new renderer features and
directly parallels Godot's NavigationServer, upgrading Maz's grid A* to polygon navigation.
- [x] **M87 — Navigation mesh (`game::NavMesh`), toward Godot's NavigationServer**: the walkable area is
  a set of **convex polygon cells**; `build()` matches shared edges to form the cell adjacency graph
  (each shared edge is a portal). `findPath()` locates the start/goal cells, **A*-searches the cell
  graph** for the corridor, then runs **Mikko Mononen's "simple stupid funnel"** over the corridor's
  portals to string-pull a short, smooth path that hugs reflex corners — instead of the staircase a
  uniform grid produces. The subtle part is portal orientation: each portal's endpoints must be labeled
  left/right relative to the direction of travel, which I derived from the cross product of the
  cell-to-cell travel vector with each endpoint (getting the handedness right took a hand-traced
  L-corridor to pin down — the initial centroid-based sign was inverted). Header-only, 2D-math-only, so
  it unit-tests without a GPU. Unit-tested to **3389 checks** total: point location + same-cell direct
  path, a straight two-cell corridor producing no spurious bend, an **L-corridor whose 3-point path
  hugs the reflex corner at (10,10)** (the funnel's whole purpose), disconnected cells yielding no path,
  and an out-of-mesh point yielding no path. The new `navmesh` demo lays out a room around a central
  pillar as eight convex cells and routes an agent from one corner to another; the funnel bends the
  path tightly around the pillar's corner. Verified on lavapipe (the mesh cells, the pillar hole, and a
  3-waypoint corner-hugging path render correctly; no validation errors); new `navmesh_headless_smoke`
  + golden (RMSE 0, threshold 0.05) — golden capture/check and ctest run SERIALLY per the harness note
  (concurrent Xvfb runs corrupt each other) → ctest **44/44**, every existing golden unchanged.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 49 — "Benchmarking against Godot: vector shapes" (done)
Continuing the Godot benchmark. The top-ranked closable gap was 2D lights/shadows, but that needs a
prerequisite Maz lacked: the renderer could only draw textured quads, with no way to fill an arbitrary
shape — so a light cone, a shadow volume, or Godot's Polygon2D were impossible. This iteration builds
that prerequisite (also a Godot feature in its own right: `draw_colored_polygon` / `Polygon2D`).
- [x] **M88 — Filled convex polygons (`Renderer::drawConvexPolygon`)**: the sprite renderer already
  streams textured quads (6 vertices each, pos/uv/color) into a per-frame dynamic buffer batched by
  texture. Polygon fill slots straight into that: `drawConvexPolygon(points, count, color)` triangulates
  the polygon as a fan from `points[0]`, emits `3·(count−2)` vertices with uv=(0,0) and the flat color
  into the same stream, batched against a renderer-owned **1×1 white texture** (sampled white × vertex
  color = the fill color) — so it reuses the exact sprite pipeline and alpha-blends like everything
  else, at zero new pipeline cost. `VulkanRenderer` creates the white texture at init; the base
  `Renderer` provides a no-op default so headless stays clean. The new `vectors` demo draws a row of
  regular N-gons (triangle→octagon), a 64-sided polygon that reads as a smooth filled circle, and three
  overlapping translucent triangles whose crossings composite correctly — none of which the quad-only
  path could do. Verified on lavapipe (all shapes fill and alpha-blend correctly; no validation errors);
  new `vectors_headless_smoke` + golden (RMSE 0, threshold 0.05). The renderer change is additive, so
  every existing 2D/3D golden is unchanged — confirmed by a serial golden run (strays killed first,
  golden check then ctest run one at a time). ctest **45/45**. Unit count steady at **3389** (this is a
  render-path feature, exercised by the golden rather than the CPU unit suite).

### Iteration 50 — "Benchmarking against Godot: 2D lights + shadows" (done)
Continuing the Godot benchmark. With arbitrary polygon fill in hand (M88), the top-ranked closable gap —
2D dynamic lights that cast shadows (Godot's `Light2D` + `LightOccluder2D`) — was finally buildable.
- [x] **M89 — 2D lights + shadows (`game::Visibility2D` + `Renderer::drawPolygonFan`)**: a point light's
  lit region is the polygon of everything it can see past a set of blocking segments. `game::Visibility2D`
  computes it with the classic **angle-sweep**: add the four bounding-box edges as occluders, cast a ray
  toward every occluder endpoint (nudged ±0.00015 rad to slip past corners), keep the nearest hit, and
  return the hits in angular order — a star-shaped visibility polygon. The notches it carves out behind
  occluders **are** the shadows: hard-edged and geometrically exact, not a blur. Rendering it needed one
  new renderer primitive: `drawPolygonFan(PolyVertex*, count)` — a triangle fan where **each vertex
  carries its own color**, so the light can be bright at the center and fade to zero alpha at the rim
  (a finite-radius glow). It reuses the same batched sprite pipeline + 1×1 white texture as
  `drawConvexPolygon`, so it costs no new pipeline and alpha-composites over the dark room. The geometry
  is pure 2D math (no GPU), so it unit-tests directly: a `testVisibility2D` covers ray/segment
  intersection, an empty room (every interior point lit), and a wall casting a shadow (point behind the
  wall is *not* contained, point in front *is*). The new `lights2d` demo lights a near-black room with
  three colored lights (warm / cool / magenta) and four solid boxes; on lavapipe the light pools and the
  boxes' shadow wedges render exactly as intended (no validation errors). New `lights2d_headless_smoke`
  + golden (RMSE 0, threshold 0.05). The renderer change is additive (a non-pure virtual with a no-op
  default), so every existing 2D/3D golden is unchanged — confirmed by a serial golden run (strays
  killed first; golden check then ctest one at a time). ctest **46/46**. Unit count **3389 → 3399**.

### Iteration 51 — "Benchmarking against Godot: additive 2D light blending" (done)
Continuing the Godot benchmark. M89 shipped 2D lights, but with one honest flaw called out in its own
summary: the light pools were **alpha-blended**, so where two lights overlapped they *averaged* (a
bright light over a dim one just muddied) instead of *brightening*. Godot's Light2D composites lights
**additively** — light is energy, and energy adds. Closing that was the top-ranked closable gap.
- [x] **M90 — Additive blending in the 2D renderer (`BlendMode` + a 2nd sprite pipeline)**: the sprite
  renderer had exactly one pipeline (standard alpha "over" blend), shared by sprites, fonts, polygons,
  and the M89 lights. This adds a second pipeline that is byte-identical except for the blend
  attachment — src·alpha is **added** to the destination (dst factor `ONE`) — built right after the
  alpha one from the same shaders/layout. A `BlendMode { Alpha, Additive }` enum threads through
  `drawConvexPolygon` / `drawPolygonFan` (defaulting to Alpha, so every existing call is unchanged);
  each `Batch` records its blend mode, `fillPolygon*` starts a new batch when the mode changes, and
  `flush` binds the matching pipeline per batch (tracking the currently-bound one to avoid redundant
  binds). The `lights2d` demo now draws its light pools additively: where the warm, cool, and magenta
  lights overlap the colors sum toward white — visibly, correctly brighter than the M89 render — while
  the boxes still cast their hard shadows. Verified on lavapipe (the additive hotspots appear exactly
  where pools overlap; no validation errors); `lights2d` golden recaptured (RMSE 0, threshold 0.05).
  The change is additive and opt-in, so all 40 other goldens are unchanged — confirmed by a serial
  golden run (strays killed first; golden check then ctest one at a time). ctest **46/46**. This is a
  render-path feature exercised by the golden, so the CPU unit count is steady at **3399**. Additive
  blending is also the standard mode for glows / fire / energy, so it's reusable well beyond lights.

### Iteration 52 — "Benchmarking against Godot: 2D rigid-body rotation" (done)
Continuing the Godot benchmark. After three rendering iterations (M88–M90), the ranked list's clearest
remaining *foundational* gap was in physics: Maz's 2D bodies could translate but never **rotate** —
boxes slid around permanently axis-aligned, while Godot's flagship `RigidBody2D` has full angular
dynamics (torque, angular velocity, moment of inertia). That's a capability gap, not a polish gap, so
it outranked the lighting refinements.
- [x] **M91 — 2D rigid-body rotation (`game::Body2D::enableRotation` + an oriented solver)**: a body
  now carries an orientation (`angle`), spin (`angularVel`), and an inverse moment of inertia
  (`invInertia`, derived from shape + mass: `m(w²+h²)/12` for a box, `½mr²` for a disc). Collision
  detection gained oriented-box support — SAT over the four face normals with a single deepest-vertex
  contact point, plus circle-vs-oriented-box — and the contact solver became rotational: impulses are
  applied at the contact point with the full `1/m + (r×n)²/I` effective mass, so an off-centre corner
  hit produces torque and the box tumbles. Added `linear/angularDamping` (like Godot's `linear_damp` /
  `angular_damp`) so a pile settles. Crucially, **rotation is opt-in**: `invInertia` defaults to 0
  (infinite inertia = locked), and `PhysicsWorld2D::step` dispatches to the oriented solver only when
  some body has enabled it — so every existing scene runs the byte-identical old path. That reduction
  is exact (the rotational impulse with `invInertia = 0` collapses to the old centre-of-mass impulse,
  and `x + 0.0f == x` in IEEE), and it's confirmed by the pre-existing physics tests still passing
  unchanged. New `testPhysics2DRotation` covers the inertia formulas, free-spin angle integration,
  angular damping, and a tilted box dropped on a floor that topples flat and rests at the right height
  (its instantaneous `angularVel` limit-cycles between the two bottom corners — a known single-contact
  artifact — so the test asserts the settled *pose*, angle + height, not the momentary spin). The new
  `tumble` demo drops 11 tilted rectangles into a bin; on lavapipe they fall, tumble on their corners,
  and settle into a believable leaning heap. New `tumble_headless_smoke` + golden (settle 4.5 s,
  threshold 0.06). Non-rotating goldens all unchanged — confirmed by a serial golden run (strays
  killed first; golden check then ctest one at a time). ctest **47/47**. Unit count **3399 → 3410**.
  Honest scope: this is a single-contact-point solver with damping — great for tumbling and settling,
  but not as rock-solid for tall precise stacks as Godot's multi-point + warm-started solver.

### Iteration 53 — "Benchmarking against Godot: animation blend spaces" (done)
Continuing the Godot benchmark, and diversifying after physics (M91): the animation system had clip
sampling (M68), 2-way pose crossfade, and a cross-fade controller (M70), but no way to blend
animations by a *parameter* — Godot's `AnimationTree` BlendSpace1D/2D, the standard way to drive
locomotion (blend idle/walk/run by speed, or 8-way movement by a 2-D direction). That was the clearest
remaining animation gap.
- [x] **M92 — Animation blend spaces (`anim::BlendSpace1D` / `BlendSpace2D` + `blendPosesWeighted`)**:
  a blend space places animations (referenced by an integer id) at positions in a 1-D or 2-D parameter
  space and, for a query point, returns the small set of animations to mix and each one's weight
  (summing to 1). 1-D blends the two straddling samples linearly (clamping past the ends); 2-D returns
  barycentric weights over a caller-supplied triangulation — the same triangle model Godot uses, made
  explicit and therefore exactly testable — with an outside query clamped onto the nearest triangle.
  A new `anim::blendPosesWeighted` folds N weighted poses into one (translation/scale lerp, rotation via
  incremental normalized slerp), the N-way generalization of `blendPoses`. All pure geometry, so it
  unit-tests headlessly: `testBlendSpace` covers 1-D neighbour/clamp/exact-sample cases, 2-D barycentric
  weights + a vertex query + an outside clamp (weights stay >= 0 and sum to 1), and the weighted pose
  mix (a 3-way average and a single-pose identity). Unit count **3410 -> 3443**. The new `blendspace`
  demo builds an 11-joint stick-figure skeleton and four corner poses, then renders a 5x3 grid of
  figures — one per sampled (x,y) cell — so a single skeleton's pose is seen morphing smoothly from
  corner to corner across the space; verified on lavapipe (the interpolation reads clearly, no
  validation errors). New `blendspace_headless_smoke` + golden (static scene, RMSE ~0, threshold 0.05).
  Purely additive (a new header + a new app), so all existing goldens are unchanged — confirmed by a
  serial golden run (strays killed first; golden check then ctest one at a time). ctest **48/48**.

### Iteration 54 — "Benchmarking against Godot: 2D physics joints" (done)
Continuing the Godot benchmark, building on the M91 rotational solver: the physics could collide bodies
but had no way to *connect* them — no ropes, chains, hinges, or springs — while Godot ships PinJoint2D
and DampedSpringJoint2D. That constraint layer was the clearest way to extend the physics momentum from
M91, and it's deterministic + unit-testable + has a strong settled-state visual golden.
- [x] **M93 — 2D physics joints (`game::Joint2D` Pin + Spring)**: a joint ties two bodies together (or
  one body to a fixed world point) and is solved by sequential impulses inside the oriented step, so
  joints and contacts compose. A **Pin** is a point-to-point constraint driving the two world anchors
  together — the standard 2x2 effective-mass solve (`K = [[m+I·ry², -I·rx·ry],[…, m+I·rx²]]`) with a
  Baumgarte position bias so drift is pulled out; a chain of pinned links behaves like a rope/hinge. A
  **Spring** applies a soft restoring impulse `-(k·stretch + c·relVel)` along the joint axis (Godot's
  DampedSpringJoint2D). Anchors are given in each body's local (rotated) frame; `b < 0` anchors to a
  world point. `PhysicsWorld2D::step` now routes to the oriented solver when joints exist (as well as
  when rotation is enabled), so a joints-only scene works; non-jointed non-rotating scenes still hit the
  byte-identical old path. `testPhysics2DJoints` proves a pin pendulum holds its arm length (~100 units)
  while it visibly swings, and a stretched damped spring returns to and rests at its rest length. Unit
  count **3443 -> 3448**. The new `joints` demo pins 11 boxes end-to-end between two posts into a rope
  bridge that sags into a clean catenary, and hangs four masses from springs of increasing stiffness
  (each settling at a different stretch); verified on lavapipe (the catenary + spring rest states read
  clearly, no validation errors). New `joints_headless_smoke` + golden (settle 4.5 s, threshold 0.06).
  Additive change (joints default to none), so all existing goldens are unchanged — confirmed by a
  serial golden run (strays killed first; golden check then ctest one at a time). ctest **49/49**.
  Honest scope: Pin uses Baumgarte (not full position projection) and there's no warm-starting, so very
  long/heavy chains are softer than Godot's solver; it's excellent for ropes, bridges, pendulums, and
  springs.

### Iteration 55 — "Benchmarking against Godot: 2D positional audio" (done)
Continuing the Godot benchmark, and finally diversifying into AUDIO — the subsystem where Maz was
thinnest. The mixer synthesized voices with a master volume but was mono and non-spatial, while Godot's
AudioStreamPlayer2D places sounds in the world: they fade with distance and pan across the stereo field
by direction. Closing that is high-leverage and its core is pure, deterministic math.
- [x] **M94 — 2D positional audio (`audio::spatialize` + a stereo pan mixer)**: `audio::spatialize`
  takes a `Listener2D` (position + a "right" axis) and a source position and returns the source's
  per-channel gain = a distance **attenuation** (Linear ramp, or InverseDistance = refDist/d, silent
  past maxDistance) times a **constant-power stereo pan** (theta sweeps 0..pi/2 as the source moves
  left..right along the listener's right axis, so left² + right² == gain²). Pure header math — no device
  — so `testSpatial2D` checks it exactly: the attenuation curve (full inside ref, half at 2x ref for
  inverse, 0 past max, linear midpoint), a centred source (equal L/R at 0.707), hard-left/hard-right
  sources (all energy in one ear), silence past max range, and the constant-power invariant. Unit count
  **3448 -> 3462**. The SDL mixer became **stereo**: each `Voice` carries a left/right gain
  (`SoundDesc::leftGain`/`rightGain`, default 1,1 so existing SFX are unchanged), `feed()` now writes
  interleaved L/R, and `renderVoice` is still advanced exactly once per frame then split across channels
  — so on a real device a blip fed spatializer gains actually moves across the stereo image. The new
  `spatial2d` demo visualizes the field on lavapipe: a listener with two range rings and five sources,
  each drawn with a halo whose brightness scales with its gain and an L|R bar showing its pan, plus a
  master stereo meter — the left sources skew blue (left), the right skew red (right), the close source
  is loud, and the source past max range is faint. New `spatial2d_headless_smoke` + golden (static,
  RMSE 0, threshold 0.05). The mixer change is default-centred, and the visual goldens don't sample
  audio, so all existing goldens are unchanged — confirmed by a serial golden run (strays killed first;
  golden check then ctest one at a time). ctest **50/50**. Honest scope: this is 2D pan + attenuation
  (Godot AudioStreamPlayer2D); it is not a full bus graph with DSP effects, nor 3D spatialization with
  doppler — those remain.

### Iteration 56 — "Benchmarking against Godot: UI text input + focus" (done)
Continuing the Godot benchmark, diversifying into UI. Maz had immediate-mode widgets (M60) and a
retained layout system (M86), but no way for a player to *type* — no editable text field and no focus
system, while Godot's Control tree has LineEdit plus Tab-based focus traversal. That's a clear
interactive-UI gap, and its core (a text-edit model + a focus ring) is pure, deterministic logic.
- [x] **M95 — UI text input (`ui::TextField`) + focus navigation (`ui::FocusChain`)**: `TextField` is a
  single-line edit model — a string + a byte caret with `insert` (filters control chars, honours a max
  length), `backspace`/`del`, `moveLeft`/`moveRight`/`home`/`end` — the model behind Godot's LineEdit.
  `FocusChain` is an ordered set of focusable widget ids with `next()`/`prev()` (Tab / Shift+Tab, with
  wraparound), `focus(id)`, and `focused()` — Godot's focus_next / focus_previous. Both are pure logic,
  so `testTextInput` checks them exactly: caret editing (insert/backspace/delete/home/end/no-op at
  bounds, control chars ignored, max length), focus wraparound + jump + unknown-id no-op, and the
  immediate-mode `Context::textField` widget itself (a renderer-less Context still runs the interaction:
  typed input lands only in the focused field, and a click grabs focus). Unit count **3462 -> 3492**.
  The new `Context::textField` widget draws the box (accent border when focused), the text, and a solid
  caret at the measured caret x (via `Font::textWidth`), and applies a per-frame `TextEditInput` (typed
  chars + editing keys, supplied by the caller) to whichever field owns focus. The new `form` demo is an
  account-settings form of four fields: click or Tab to move focus (the focused field shows the accent
  border + caret), type to edit, with one field length-capped — the demo maps SDL scancodes to
  characters and feeds the widget. Deterministic initial state (Server focused, caret at end), so the
  render is golden-stable; verified on lavapipe (the focused field's border + caret render correctly, no
  validation errors). New `form_headless_smoke` + golden (static, RMSE 0, threshold 0.05). Purely
  additive (new header + a new widget method + a new app), so all existing goldens are unchanged —
  confirmed by a serial golden run (strays killed first; golden check then ctest one at a time). ctest
  **51/51**. Honest scope: this is single-line editing + focus + click/Tab (Godot LineEdit); it has no
  text selection/clipboard, no multi-line TextEdit, and no theme resources yet — those remain.

### Iteration 57 — "Benchmarking against Godot: procedural caves + tilemap autotiling" (done)
Continuing the Godot benchmark, rotating to procedural generation / tilemaps for breadth. Maz had a
tilemap (M76), Perlin/fBm noise (M85), and a seeded RNG (M84), but nothing that *generated* a level or
picked tiles by their neighbourhood — while Godot ships TileMap terrain sets (autotiling) and procedural
gen is one of its biggest use cases. Both halves are pure, deterministic logic, so they close a real gap
with rigorous verification.
- [x] **M96 — Procedural caves (`game::CellularCave`) + tilemap autotiling (`game::autotileMask4`)**:
  `CellularCave::generate` grows an organic cavern by cellular automata — a seeded random fill, then N
  smoothing passes where a cell becomes solid iff a majority of its 8 neighbours are solid (the classic
  "4-5 rule"), with the border forced solid so the cave is enclosed. `autotileMask4` returns a wall
  cell's 4-bit edge mask (bit per N/E/S/W neighbour that is also solid; out-of-bounds counts as solid so
  map-edge walls don't grow spurious borders) — exactly the value a terrain tileset keys on to choose a
  border tile — plus `autotileIndex4` for a 16-tile blob atlas. Pure logic on `core::Random`, so
  `testAutoTile` checks it exactly: cave generation is reproducible for a seed (and differs across
  seeds), the whole border is solid, the result is mixed (not uniform), and the mask is 0x0F when
  fully-surrounded, 0 when isolated, clears the right bit when a side opens, and reads out-of-bounds as
  solid at a corner. Unit count **3492 -> 3505**. The new `cave` demo generates a seeded cavern and
  renders each wall cell **inset on the sides that face open floor** (driven by its mask), so the walls
  round off into smooth terrain-tile borders, shaded slightly by how enclosed each cell is; on lavapipe
  it reads as a proper cave with pillars, nooks, and winding passages. New `cave_headless_smoke` +
  golden (static, RMSE 0, threshold 0.05). Purely additive (new header + a new app), so all existing
  goldens are unchanged — confirmed by a serial golden run (strays killed first; golden check then ctest
  one at a time). ctest **52/52**. Honest scope: this is bitmask (4-connected) autotiling + cellular
  cave gen; it is not the full 47-tile Wang/blob terrain matcher, BSP/room-graph dungeon generation, or
  a runtime TileMap terrain-painting API — those remain.

### Iteration 58 — "Benchmarking against Godot: 2-bone inverse kinematics" (done)
Continuing the Godot benchmark. The animation system had skeletons (M67), clips + blending (M68),
a controller (M70), and blend spaces (M92), but everything was FORWARD kinematics — you posed joints and
the ends followed. Godot's SkeletonModification2D adds inverse kinematics: you place a *target* and the
joints solve to reach it (foot planting, hand-to-object, look/aim). 2-bone IK is the canonical building
block, it's pure closed-form math, and it has a crisp visual golden — a strong, high-confidence pick.
- [x] **M97 — 2-bone inverse kinematics (`anim::solveTwoBoneIK`)**: given a fixed root (shoulder), two
  bone lengths, a target, and a `bendSign`, it returns the middle joint (elbow) and end effector. The
  elbow is placed by the law of cosines: the angle at the root between the root→target line and the
  upper bone is `acos((len1²+d²−len2²)/(2·len1·d))`, and `bendSign` (+1/−1) rotates the upper bone to
  either side so the elbow bends up or down. Within the working range `[|len1−len2|, len1+len2]` the hand
  lands on the target exactly; beyond it the chain points straight at the target, fully extended
  (`reachable=false`). Pure 2D math (no skeleton, no GPU), so `testTwoBoneIK` checks it exactly: a
  reachable target puts the hand on the target with both bone lengths preserved; flipping `bendSign` puts
  the elbow on the opposite side of the root→target line (the cross-product sign flips) while still
  reaching; an out-of-reach target gives a straight, collinear arm at full stretch; and a target exactly
  at full stretch is reachable with a straight arm. Unit count **3505 → 3523**. The new `reach` demo is a
  6×3 grid of arms, each solving toward its own fanned-out target with alternating bend direction —
  reachable targets ringed green, the bottom-right out-of-reach ones red with the arm extended straight
  at them; on lavapipe the elbow solve + bend + overreach all read clearly. New `reach_headless_smoke` +
  golden (static, RMSE 0, threshold 0.05). Purely additive (new header + a new app), so all existing
  goldens are unchanged — confirmed by a serial golden run (strays killed first; golden check then ctest
  one at a time). ctest **53/53**. Honest scope: this is closed-form 2-bone IK; it is not a multi-bone
  CCD/FABRIK chain solver, a full-body IK rig, or a Skeleton-integrated modification stack — those remain.

### Iteration 59 — "Benchmarking against Godot: RVO local avoidance" (done)
Continuing the Godot benchmark; the AI subsystem was due. Maz had *global* navigation — grid A* (M57),
navmesh + funnel (M87), steering forces (M58) — but every agent planned in isolation, so a crowd
sharing a space would walk straight through each other. Godot's `NavigationAgent2D` adds *local*
avoidance (RVO/RVO2): each agent continuously adjusts its velocity to dodge nearby moving agents. It's
the canonical missing piece on top of the existing navigation stack, it's pure deterministic 2D math
(unit-testable, golden-stable), and it has a crisp visual golden — the classic antipodal circle test.
- [x] **M98 — RVO local collision avoidance (`game::rvoVelocity`)**: given an agent's position,
  current velocity, *preferred* velocity (toward its goal), radius, max speed, and a list of moving
  neighbours, it returns a nearby velocity that avoids imminent collisions. It samples candidate
  velocities — the preference, a full stop, and a fan of 16 directions × 4 speeds — and scores each by
  the soonest collision it would produce against any neighbour. The collision test (`detail::timeToCollision`)
  solves the quadratic for when two closing discs first touch, and the relative velocity fed in is the
  **reciprocal** `2·c − vA − vB`, which is what makes the avoidance *shared*: both agents run the same
  rule and each takes half the dodge, so a head-on pair peels apart smoothly rather than mirroring each
  other into a deadlock. Cost = distance-from-preference + `maxSpeed`-weighted collision penalty, and
  because both the preference and zero are always candidates, an agent with no threats returns its
  preferred velocity *exactly*. Pure 2D math (no navmesh, no GPU), so `testAvoidance` checks it exactly:
  no neighbours returns the preference bit-for-bit; a blocker sitting dead ahead forces a nonzero
  sideways component; and a full 240-step head-on crossing of two agents keeps their gap above
  `2·radius − 0.15` the whole way *and* both still reach the far side. Unit count **3523 → 3529**. The
  new `avoid` demo is the textbook stress test — 14 agents evenly spaced on a circle, each heading for
  the point directly opposite, so every path crosses the crowded centre; the whole crossing is simulated
  once at startup (fixed step) and each agent's trail recorded, then drawn statically, so the render is
  fully deterministic regardless of capture timing. On lavapipe the trails bulge outward into the
  classic lens/almond shape around the middle — agents clearly routing around each other, never
  overlapping, re-forming on the far side. New `avoid_headless_smoke` + golden (static, RMSE 0,
  threshold 0.05). Purely additive (new header + new app), so all existing goldens are unchanged —
  confirmed by a serial golden run (strays killed first; golden check then ctest one at a time). ctest
  **54/54**. Honest scope: this is agent-vs-agent RVO with a velocity-sampling solver; it is not full
  ORCA half-plane linear programming, it doesn't yet avoid *static* navmesh obstacles, and it isn't
  wired into `NavMesh`/`Steering` as an integrated crowd simulation — those remain.

### Iteration 60 — "Benchmarking against Godot: audio DSP + mix buses" (done)
Continuing the Godot benchmark; audio was the least-developed subsystem. Maz had a real-time SDL mixer
with synthesized voices and 2D pan/attenuation (M5/M94), but the mix was a *flat sum* — no bus routing
and, crucially, no **effects**. Godot's AudioServer is built around buses each carrying an ordered
effect chain (AudioEffectFilter, AudioEffectDelay, reverb, ...). The reusable, high-leverage,
exactly-testable core of that is pure per-sample DSP, and it has a crisp deterministic golden — an
offline waveform scope showing what each effect does.
- [x] **M99 — Audio DSP effects + mix buses (`audio::Biquad` / `audio::Delay` / `audio::Bus`)**: a
  header-only DSP core matching the `Spatial2D` pattern (pure math, no device, so any backend can
  consume it). `Biquad` is the standard second-order IIR filter behind Godot's AudioEffectFilter, with
  RBJ-audio-EQ-cookbook low/high/band-pass factories (cutoff + resonance Q) and a transposed-direct-
  form-II `process` (coefficients pre-normalized by a0, so no per-sample division). `Delay` is a
  feedback echo — a ring buffer with wet/feedback controls — matching AudioEffectDelay. `Bus` holds an
  ordered `std::vector<unique_ptr<Effect>>` chain plus an output gain and runs a sample (or a whole
  buffer) through each effect in series, exactly like a Godot audio bus. `testAudioDsp` checks the math
  exactly: the low-pass passes DC at unity gain and a 200 Hz tone at ~full amplitude while crushing an
  8 kHz tone to <5% (>8× ratio); the high-pass decays a DC input to ~0 and blocks a 100 Hz tone while
  passing 10 kHz; a unit impulse through a 10-sample delay (wet 0.8, feedback 0.5) reappears at exactly
  sample 10 at 0.8 and at sample 20 at 0.4, with silence between the taps; and an empty bus is a
  gain-only pass-through while a low-pass→delay chain preserves both the immediate and the delayed
  energy. Unit count **3529 → 3544**. The new `bus` demo is an offline scope: it synthesizes one
  plucked-sawtooth note (rich harmonics, then silence) at 44.1 kHz, runs it through a 600 Hz low-pass, a
  1.5 kHz high-pass, and a low-pass→delay bus, and draws all four as stacked waveform bands (decimated
  for display) — the whole thing computed once at startup so the render is deterministic. On lavapipe
  the difference is obvious: the source's buzzy sawtooth teeth become smooth rounded waves under the
  low-pass, collapse to just the sharp transient spikes under the high-pass, and the bus band shows the
  filtered note followed by evenly-spaced decaying echoes. New `bus_headless_smoke` + golden (static,
  RMSE 0, threshold 0.05). Purely additive (new header + new app), so all existing goldens are
  unchanged — confirmed by a serial golden run (strays killed first; golden check then ctest one at a
  time). ctest **55/55**. Honest scope: this is the pure DSP + bus-chain core plus reverb-less filter/
  delay effects; it does not yet include reverb/distortion/compressor effects, and the *real-time*
  mixer still sums voices flatly — per-voice bus routing through the live SDL callback is the remaining
  integration step (and can't be verified headlessly here anyway, so it's deliberately deferred).

### Iteration 61 — "Benchmarking against Godot: keyframe timeline" (done)
Continuing the Godot benchmark; the animation subsystem had skeletons (M67), clips (M68), a controller
(M70), blend spaces (M92), IK (M97), and scalar tweens (M59) — but every one of those either blends
*poses* or animates a *single* value. Godot's `AnimationPlayer` — one of the most-used nodes in the
whole engine — is different: it drives *many named property tracks* from keyframes over a shared
timeline (cutscenes, UI transitions, property animation). Maz had no such thing. It's pure,
exactly-testable math and has a crisp editor-style golden, so it was the clear highest-leverage pick.
- [x] **M100 — Keyframe timeline / sequencer (`anim::Timeline`)**: a `Track` is a time-sorted list of
  `Keyframe`s (time → value + the easing used to reach the *next* key); `sample(t)` holds the endpoints
  (no extrapolation) and eases between the neighbouring keys inside the range. A `Timeline` is a set of
  named tracks plus a playhead that advances over the clip length (auto = longest track, or explicit)
  under the existing Once/Repeat/PingPong `Loop` policy, reusing `anim::Ease`/`ease`/`mix` so it shares
  one easing vocabulary with `Tween`. `value(name)` samples at the playhead; `valueAt(name,t)` at an
  explicit time. `testTimeline` checks it exactly: endpoint holding before/after the keys, exact key
  values, linear midpoints, per-segment easing (QuadIn gives 2.5 at the half-point of a 0→10 segment),
  out-of-order `add` staying sorted, independent multi-track sampling, unknown-track→0, Repeat wrapping
  the playhead, and PingPong reflecting the query time on the way back. Unit count **3544 → 3571**. The
  new `timeline` demo authors one animation with five tracks (x, y, rotation, scale, and r/g/b colour)
  and renders it two ways at once: an *onion-skin trail* of a little arrow sampled at 13 even times
  across the clip (so you see the whole motion — it arcs over the top, spins two full turns, pulses
  bigger in the middle, and shifts red→green→magenta), and an *editor-style track panel* below with a
  lane per track showing the value curve, its keyframe dots, and a playhead line at a fixed time with
  the sampled point marked. Everything samples the timeline at fixed times, so the render is fully
  deterministic. On lavapipe the eases are visibly correct — the scale lane shows its BackOut overshoot
  bumps, the y lane its sine arc. New `timeline_headless_smoke` + golden (static, RMSE 0, threshold
  0.05). Purely additive (new header + new app), so all existing goldens are unchanged — confirmed by a
  serial golden run (strays killed first; golden check then ctest one at a time). ctest **56/56**.
  Honest scope: this is a value-track timeline with per-segment easing; it does not yet include
  call-method/trigger tracks, a bezier-handle curve editor, or a visual track-editing UI — those remain.

### Iteration 62 — "Benchmarking against Godot: soft 2D shadows" (done)
Continuing the Godot benchmark; the rendering subsystem was the most overdue (last touched at M90's
additive lights). Maz's 2D lights (M89/M90) cast razor-sharp shadows: the visibility polygon splits the
world into "lit" or "not," with no in-between. Godot's Light2D softens shadow edges. The physically
right way is an AREA light — a light with size casts a soft edge with an inner umbra (sees none of the
light), an outer lit region, and a penumbra between (sees only part). That's pure, exactly-testable 2D
geometry with a striking side-by-side golden, so it was the clear rendering pick.
- [x] **M101 — Soft (penumbra) 2D shadows (`game::SoftShadow2D`)**: model the light as a disc and
  sample it. `diskSamples` places `count` points across the disc on a Vogel/sunflower spiral (even areal
  coverage, no RNG — deterministic, so goldens are stable), degenerating to a single centre point for
  count≤1 or radius 0 (a plain point light). `segmentsIntersect` is a strict interior segment-crossing
  test (shared endpoints/grazes don't count) and `lineBlocked` is the line-of-sight test over the
  occluders. `softVisibility(p, lightCenter, lightRadius, occluders, samples)` returns the fraction of
  the disc visible from p — 1 fully lit, 0 umbra, in between penumbra. `testSoftShadow2D` checks it
  exactly: proper vs non-crossing vs shared-endpoint segments; sample count, all-within-radius,
  determinism, and the count≤1/radius-0 degeneracies; no occluders → 1.0; a wall spanning the whole
  light → 0.0; a wall covering only one side → strictly between 0 and 1. Unit count **3571 → 3608**. The
  new `softshadow` demo draws the *same* box+light twice: left a hard point light (1 sample) with a
  crisp shadow edge, right an area light (24 disc samples) rendered by compositing one faint
  Visibility2D fan per sample additively — so regions reached by every sample are fully lit, regions
  reached by none are umbra, and the boundary feathers into a penumbra that widens with distance from
  the caster. On lavapipe the contrast is unmistakable: a hard-edged wedge on the left, a soft graded
  penumbra on the right with the area-light disc visibly glowing. New `softshadow_headless_smoke` +
  golden (static, RMSE 0, threshold 0.05). Purely additive (new header + new app), so all existing
  goldens are unchanged — confirmed by a serial golden run (strays killed first; golden check then ctest
  one at a time). ctest **57/57**. Honest scope: this is area-light-sampled soft shadows composited as N
  additive fans; it is not a GPU shadow-map blur, and the lights are still flat-coloured (no
  normal-mapped / textured 2D lights) — those remain.

### Iteration 63 — "Benchmarking against Godot: groove / slider joints" (done)
Continuing the Godot benchmark; physics was the most overdue subsystem. Maz's Joint2D (M93) had a Pin
and a damped Spring — but Godot ships a THIRD 2D joint, GrooveJoint2D: a body pinned to a *line* (a
rail), free to slide along it but held on it. It's the exact missing piece to complete Godot's 2D joint
set, and (unlike a box-stacking solver rewrite) it slots into the existing sequential-impulse step as a
new constraint without touching the Pin/Spring paths — so it's a clean, low-risk, additive physics win
with a crisp settling golden.
- [x] **M102 — Groove / slider joint (`Joint2D::Groove`)**: a new joint type plus a `solveGroove`
  constraint. Given a groove body `g`, a slider `s`, a groove anchor + `axis` (local to `g`), and the
  slider's anchor, it projects the slider anchor onto the groove line (so the torque arms are correct),
  then applies a single impulse along the groove *normal* (with a Baumgarte position bias) to cancel
  off-line motion while leaving motion along the axis free — make `g` static for a world-fixed rail. The
  only change to the existing solver is one added `else if` in the joint dispatch and two new
  fields/enum value on `Joint2D`; the Pin and Spring paths are untouched, so **every existing physics
  golden (physics, boxes, tumble, joints) is byte-identical** — confirmed RMSE 0 across the board.
  `testPhysics2DGroove` checks it exactly: a slider started off a horizontal groove is pulled onto the
  line (|y|<0.2) while its along-groove x is left free (unchanged); and on a 45° groove under gravity the
  body's perpendicular drift stays <1 the whole time while it slides >40 units down the incline. Unit
  count **3608 → 3612**. The new `groove` demo drops three boxes onto tilted rails at different angles —
  each slides down its own incline (not straight down) and settles against a stop block, a static
  equilibrium so the render is deterministic. On lavapipe all three boxes rest correctly at the low ends
  of their rails. New `groove_headless_smoke` + golden (settled, RMSE 0, threshold 0.06). ctest
  **58/58**. Honest scope: this completes the 2D joint TRIO (pin/spring/groove); it is not a full
  constraint zoo (no motorized/limited slider, gear, or weld joints), and the broader box-stacking
  stability work (2-point warm-started manifolds) is still open.

### Iteration 64 — "Benchmarking against Godot: nine-patch StyleBox" (done)
Continuing the Godot benchmark; UI was the most overdue non-recently-touched subsystem. Maz had a
retained layout system (M86) and text input (M95), but no THEMING — Godot draws every Panel and Button
through a StyleBox, most powerfully a nine-patch (StyleBoxTexture) that scales a bordered/rounded skin to
any size without distorting its corner art. That nine-slice mapping is pure geometry — exactly testable
and with a striking multi-size golden — so it was the clear UI pick.
- [x] **M103 — Nine-patch / StyleBox (`ui::ninePatch`)**: given a destination `Rect`, per-edge `Border`
  insets, and a source `Rect`, it slices both into a 3×3 grid and returns nine `Patch`es (src region →
  dst region + a cell tag). The four corners are the border size in *both* src and dst (never scaled),
  the four edges stretch along one axis, and the center absorbs the rest — with all middle dimensions
  clamped non-negative so a sub-border destination collapses gracefully instead of going inside-out.
  Header-only geometry (reuses `ui::Rect`, no GPU). `testStyleBox` checks it exactly: corners pinned to
  the four dst corners at the fixed border size; edges keeping their border thickness while stretching
  the other axis; the center = (w−L−R)×(h−T−B); the nine dst cells tiling the destination area exactly
  and the nine src cells tiling the source area exactly; corner sizes identical between a 60×40 and a
  500×380 destination while the center grows; and a 20×20 destination clamping every middle to zero.
  Unit count **3612 → 3654**. The new `stylebox` demo themes four differently-sized panels (small
  square, wide bar, tall column, large box) plus a three-button row from one style — gold corners stay
  fixed, blue edges stretch, the dark center fills — so the whole gallery reads as one consistent skin
  at every size. On lavapipe the corner cells are visibly identical across all panel sizes while edges
  and centers scale. New `stylebox_headless_smoke` + golden (static, RMSE 0, threshold 0.05). Purely
  additive (new header + new app), so all existing goldens are unchanged — confirmed by a serial golden
  run. ctest **59/59**. Honest scope: this is the nine-slice mapping + a flat-colour StyleBox draw; it
  is not a full theme *server* (named styles per control class), a StyleBoxFlat with rounded corners /
  drop shadows, or texture-sampled patches — those remain.

### Iteration 65 — "Benchmarking against Godot: behavior-tree blackboard + parallel/decorators" (done)
Continuing the Godot benchmark; AI was the most overdue subsystem. Maz's behavior trees (M71) had the
core reactive composites (Sequence/Selector/Inverter + Action/Condition) but were missing the two
things that make a BT a real authoring tool: a BLACKBOARD (shared working memory that decouples leaves
and lets data drive decisions) and the PARALLEL composite + common DECORATORS (repeat, force-result).
Those slot straight into the existing tree as new node types without touching the composites already
there — a clean, low-risk, exactly-testable additive win with a legible golden.
- [x] **M104 — Behavior-tree blackboard + parallel/decorator nodes (`game::bt`)**: a `Blackboard`
  (std::any-backed typed key-value store with `set`/`get`/`getOr`/`has`/`erase`/`clear`; `getOr` is the
  safe read — missing key or wrong type returns the fallback, never throws); a `Parallel` composite that
  ticks *every* child each tick and resolves by policy (RequireOne = OR, RequireAll = AND); and
  `Repeater` (repeat a child N times, abort on failure), `AlwaysSucceed`/`AlwaysFail` (force the result,
  pass Running through), and `Tap` (a transparent probe that records a node's status for visualization)
  decorators, all with builder helpers. Purely additive — the existing Sequence/Selector/Inverter/
  Action/Condition and the `behavior` demo are untouched (its golden stayed RMSE 0). `testBehaviorTreeExtras`
  checks each exactly: blackboard typing + overwrite/erase/clear + wrong-type fallback; both parallel
  policies + proof that all children are ticked (no short-circuit); the repeater returning Running until
  its count then Success, and aborting on a child Failure; the force-result decorators; Tap recording;
  and an integration where a blackboard flag drives a reactive selector (engage fails → patrol parallel
  runs; flag flips → engage runs and the patrol branch is never ticked). Unit count **3654 → 3686**. The
  new `blackboard` demo draws a sentry's tree as a node graph *twice* — `visible=false` (PATROL) and
  `visible=true` (ENGAGE) — every box coloured by the REAL per-node tick status (green Success / red
  Failure / amber Running / gray not-evaluated) via Tap. It makes the whole point visible: one
  blackboard flag flips the selector between the SEQUENCE engage branch and the PARALLEL patrol+scan
  branch (with a REPEAT decorator looping the route), and the unused branch is left gray/un-ticked. New
  `blackboard_headless_smoke` + golden (static, RMSE 0, threshold 0.05). ctest **60/60**. Honest scope:
  this rounds out the classic BT node set; it is not utility AI / GOAP, a HTN planner, or an event-
  driven BT scheduler — those remain.

### Iteration 66 — "Benchmarking against Godot: animation state machine" (done)
Continuing the Godot benchmark; animation was the most overdue subsystem. Maz had blend spaces (M92), a
cross-fade animator (M70), an FSM (M62), and a keyframe timeline (M100) — but no *animation* state
machine, which is the heart of Godot's AnimationTree (`AnimationNodeStateMachine`): a graph of states
that cross-fade between each other, where a state can itself be a blend space. All the pieces existed;
what was missing was the composition. It's pure logic with a legible, deterministic golden, so it was
the clear animation pick.
- [x] **M105 — Animation state machine (`anim::AnimStateMachine`)**: a graph of named states (each with
  an int payload — a clip or blend-space id) and cross-fading transitions (a fade time + an optional
  condition callback, plus explicit `travel(name)`). `update(dt)` advances an in-progress cross-fade and,
  when idle, fires the first outgoing transition from the current state whose condition returns true.
  The key design choice: `active()` returns the current state(s) with weights that **sum to 1** — during
  a fade it's the from-state at `1−t` and the to-state at `t` — which is the *exact same shape* as a
  blend space's weights, so the output feeds straight into `blendPosesWeighted` and a state can itself be
  a blend space (state weight × leaf weight). `testAnimStateMachine` checks it exactly: it starts in the
  start state at full weight; a condition fires a cross-fade whose two weights always partition unity
  with a genuine mid-blend, then resolve to the pure target; `travel()` with a fade-0 transition switches
  instantly with no blend; only *outgoing* transitions from the current state fire; and a state weight
  multiplied through a nested blend space still sums to 1. Unit count **3686 → 3706**. The new
  `statemachine` demo steps a locomotion machine (idle/move/jump) over a scripted speed+jump timeline and
  draws the active-state weights as stacked colour bands across time — every cross-fade shows as one
  colour smoothly slanting into the next (idle→move→jump→idle→move→idle), and the `move` band is itself a
  walk→run blend space shaded dark→bright by speed, so you can *see* a state machine layered over a blend
  space. Precomputed once, so the render is deterministic. New `statemachine_headless_smoke` + golden
  (static, RMSE 0, threshold 0.05). Purely additive (new header + new app), so all existing goldens are
  unchanged — confirmed by a serial golden run. ctest **61/61**. Honest scope: this is a flat state
  machine with cross-fades + condition/travel transitions; it is not nested sub-state-machines,
  root-motion extraction, or an animation-tree blend-graph editor — those remain.

### Iteration 67 — "Benchmarking against Godot: reverb / distortion / compressor" (done)
Continuing the Godot benchmark; audio was the most overdue subsystem. M99 gave the DSP core (biquad
filter, feedback delay, a `Bus` effect chain), but Godot's `AudioEffect*` library also includes the
three effects that make a mix sound finished — reverb (space), distortion (grit), and a compressor
(dynamics). They're pure per-sample DSP that slots straight into the existing `Bus`, exactly testable,
and with the same deterministic waveform-scope golden as M99, so it was the clear audio pick.
- [x] **M106 — Reverb + distortion + compressor (`audio::Reverb` / `Distortion` / `Compressor`)**: added
  additively to `Dsp.hpp`. `Reverb` is a Schroeder/Freeverb — four parallel `Comb` filters (each a delay
  with a one-pole low-pass in its feedback path, the `damp` control, at mutually-detuned Freeverb delay
  lengths scaled to the sample rate) summed, then two series `Allpass` filters for diffusion, mixed
  wet/dry. `Distortion` is a `tanh` soft-clip waveshaper normalized so a full-scale input stays
  full-scale. `Compressor` is a peak-envelope dynamics processor (fast-attack/slow-release follower;
  above `threshold` it pulls the gain toward `ratio`:1). All three get `Effect` wrappers so they drop
  into the `Bus`. `testAudioEffects` checks each exactly: the distortion is odd-symmetric, monotonic,
  unity at full scale, and compresses dynamics (half-input keeps >half output); the compressor settles a
  full-scale DC input to the predicted 0.625 gain while passing a sub-threshold input essentially
  unchanged; the reverb is a bit-exact pass-through at wet 0 and rings out a decaying tail (energy well
  past the input) otherwise; and a bare comb re-emits an impulse at exactly its delay length, decayed by
  feedback. Unit count **3706 → 3719**. The new `reverb` demo scopes one source (a loud plucked note
  then a quiet one) through each effect as stacked waveforms: the reverb fills the band with a dense
  decaying tail past both notes, the distortion soft-clips them fatter/flatter, and the compressor
  visibly pulls the loud note down toward the quiet one so the two even out. Precomputed at 44.1 kHz →
  deterministic. New `reverb_headless_smoke` + golden (static, RMSE 0, threshold 0.05). Purely additive
  (extended `Dsp.hpp` + new app), so all existing goldens — including the M99 `bus` scope — are
  unchanged, confirmed by a serial golden run. ctest **62/62**. Honest scope: this completes the core
  `AudioEffect` set (filter/delay/reverb/distortion/compressor); it is not the full Godot list (no
  chorus/phaser/limiter/pitch-shift), and the *real-time* mixer still sums voices flatly — per-voice bus
  routing through the live SDL callback remains the deferred (headlessly-unverifiable) integration step.

### Iteration 68 — "Benchmarking against Godot: multi-bone FABRIK IK" (done)
Rotating off audio/anim-state and back to the skeleton: M97 gave a closed-form *two-bone* IK solver, but
Godot's `SkeletonModification2DFABRIK` solves a whole *chain* of arbitrary length so a tentacle, tail, or
multi-segment arm can reach a target while every bone keeps its length. FABRIK (Forward And Backward
Reaching Inverse Kinematics) is the standard iterative algorithm for exactly that, it's pure math (fully
unit-testable), and it slots additively into the existing `anim/IK.hpp`, so it was the clear
highest-leverage closable gap this round.
- [x] **M107 — Multi-bone FABRIK IK (`anim::solveFabrik`)**: added additively after `solveTwoBoneIK` in
  `IK.hpp`. Takes a joint chain (`std::vector<vec2>`) and a target; captures each bone's rest length and
  the fixed base position up front. If the target is farther than the chain's total reach, it straightens
  the whole chain along the base→target ray (the best it can do). Otherwise it iterates: a **backward**
  pass pins the tip to the target and drags each earlier joint inward to preserve bone length, then a
  **forward** pass pins the base back to its anchor and pushes each later joint outward — repeating until
  the tip is within tolerance of the target. A small `onLine` helper re-places a joint at the correct
  distance along a direction. `testFabrik` checks it exactly: a reachable target is reached with every
  bone length preserved and the base unmoved; a straight-up target is reached; an out-of-reach target
  leaves the chain straightened collinearly with the tip at exactly the total reach along the target ray
  (verified for two different unreachable targets); a single-bone chain and a degenerate <2-joint chain
  are handled. Unit count **3719 → 3735**. The new `tentacle` demo anchors seven eight-bone chains along
  a floor, each fanned toward its own target — the middle chains curl smoothly to reachable (green)
  targets while the outer ones straighten toward out-of-reach (red) targets, all with visibly uniform
  bone lengths — solved once at startup and drawn statically for a deterministic golden (RMSE 0, threshold
  0.05). Purely additive (extended `IK.hpp` + new app), so all existing goldens — including the M97 `reach`
  two-bone scene — are byte-unchanged, confirmed by a serial golden run (`reach` RMSE 0, `tentacle` RMSE
  0). ctest **63/63**. Honest scope: this is position-based FABRIK; it does not yet do pole targets, per-
  bone angle constraints, or a Skeleton2D-integrated modification stack — those remain Godot-side gaps.

### Iteration 69 — "Benchmarking against Godot: GOAP planner" (done)
Rotating to gameplay/AI (last AI was M104's behaviour tree). Godot's AI story is a scene-tree of nodes
plus community behaviour-tree add-ons — but it ships no *planner*: nothing that, given a goal and a set of
actions, works out the sequence of actions itself. Goal-Oriented Action Planning (the classic F.E.A.R.
technique) does exactly that, and it's the highest-leverage AI gap because it's a genuine capability Godot
lacks, it's purely additive (a brand-new header — zero regression risk), and it's pure graph search so it
unit-tests headlessly and to provable optimality.
- [x] **M108 — GOAP planner (`game::goap`)**: a new header. The world is a set of boolean facts packed
  into a 64-bit word; a `Condition` is a partial state (a `mask` of the facts it cares about + their
  wanted `want` values) used for goals and preconditions; an `Action` is a precondition `Condition`, a
  `set`/`clear` bitmask of effects, and a `cost`, with chainable builders (`.needs().sets().clears().
  withCost()`). `plan(start, goal, library)` runs **A\*** over world states — `g` = accumulated action
  cost, `h` = an admissible lower bound (goal unmet ⇒ at least one more action at the cheapest action's
  cost) — popping the goal first, so the returned action sequence is guaranteed minimum-cost; it
  reconstructs the plan by walking predecessor links, returns an empty found-plan when the goal already
  holds, and reports `found=false` for unreachable goals (bounded by a max-expansion backstop).
  `testGoap` checks it hard: a five-action "make fire" plan comes back at the exact optimal cost 8 and
  avoids a deliberately-overpriced shortcut; raising the axe-route cost flips the planner to the shortcut
  (cost 11); the already-satisfied goal yields an empty zero-cost plan; a fact no action can produce is
  unreachable; partial-`Condition` match semantics and an empty library are covered. Unit count **3735 →
  3761**. The new `goap` demo has a survival agent plan "make fire" from six actions and draws the result:
  the action library down the left (the skipped "scavenge wood" decoy dimmed), and the optimal plan as a
  left-to-right flow — START → get axe → forest → chop → camp → build fire (green) — with a five-dot
  world-state strip under each step so you watch the facts turn green one by one until FIRE lights.
  Planned once at startup → deterministic golden (RMSE 0, threshold 0.05). Purely additive (new header +
  new app), so every existing golden is byte-unchanged, confirmed by a serial golden run. ctest **64/64**.
  Honest scope: this is boolean-fact STRIPS-style GOAP; it is not yet numeric/fuzzy world state, a utility-
  AI scorer, or an HTN task-network planner, and it caps the world at 64 facts — those remain as further
  planner work.

### Iteration 70 — "Benchmarking against Godot: StyleBoxFlat + Theme server" (done)
Rotating to UI (last UI was M103's nine-patch). M103 gave the *texture* half of Godot's theming
(StyleBoxTexture); the far more-used half is **StyleBoxFlat** — the procedurally-drawn rounded-corner
panel that is the look of nearly every default Godot control — plus the **Theme** resource that names
styles per control class + state. It's the highest-leverage UI gap: a marquee Godot look Maz couldn't
produce, purely additive (a brand-new header — zero regression risk), and mostly geometry so it
unit-tests headlessly and the demo renders deterministically.
- [x] **M109 — StyleBoxFlat + Theme (`ui::StyleBoxFlat` / `ui::Theme`)**: a new `ui/Theme.hpp`.
  `roundedRectPolygon(rect, corners, seg)` traces the four corner arcs into a single convex outline
  (per-corner radius, each clamped to half the shorter side; a zero-radius corner collapses to the sharp
  point), so it drops straight into the existing `drawConvexPolygon`. `StyleBoxFlat` is the Godot value
  type — bg fill, border colour + width, per-corner `Corners` radius, a soft drop shadow (colour + spread
  + offset), and content margins with a `contentRect` helper — and `drawStyleBoxFlat` renders it by
  layering shadow (an expanded, offset rounded rect) → border (the outer rounded rect) → fill (an inset
  rounded rect, so a border ring shows). `Theme` is a small registry: named `StyleBoxFlat`s and `Color`s
  keyed by string, with a default fallback and a `styleBox(type, state)` overload that falls back
  "type/state" → "type/normal" → default, exactly Godot's resolution order. `testTheme` checks the
  geometry (sharp corners give exactly four vertices in TL/TR/BR/BL order; a rounded box gives
  `4·(seg+1)` vertices all inside the bounds with no vertex left in the sharp corner; a huge radius clamps
  to a stadium), the content-margin insets, and the theme registry (exact hit, type/state fallback chain,
  default fallback, colour fallback). Unit count **3761 → 3956**. The new `theme` demo builds one dark
  theme and draws a button in each state (normal/hover/pressed/disabled) resolved *through* the theme —
  visibly different fills, borders and shadows, the pressed one sunk in, the disabled one flat and dimmed
  — plus a gallery of the individual features (sharp, rounded, thick border, soft drop shadow, a
  max-radius pill, and a top-corners-only tab). All static → deterministic golden (RMSE 0, threshold
  0.05). Purely additive (new header + new app), so every existing golden is byte-unchanged, confirmed by
  a serial golden run. ctest **64/64 → 65/65**. Honest scope: this is the StyleBoxFlat feature set + a
  flat named-style registry; it is not yet a full per-control-class theme *cascade* (inherited base types),
  anti-aliased corner edges (the corners are polygon-faceted at `seg` segments), or theme overrides bound
  live to the immediate-mode widget set — those remain UI gaps.

### Iteration 71 — "Benchmarking against Godot: 2-point contact manifolds (stable stacks)" (done)
Rotating to physics (last physics was M102's groove joint). The biggest remaining 2D-physics credibility
gap: the oriented rigid-body solver generated a SINGLE contact point per box pair (the incident box's
deepest vertex). A single point stops overlap but carries no torque balance, so an oriented box resting on
another slowly rotates off its support and the stack topples — exactly the thing a game physics engine is
judged on. Box2D and Godot both build a TWO-point manifold along the shared face by reference/incident-face
clipping. That is the highest-leverage physics pick; unlike a purely-additive module it touches the contact
path, so I gated it behind an opt-in flag to keep every existing rotating golden bit-identical.
- [x] **M110 — 2-point contact manifolds (`PhysicsWorld2D::solveManifolds`)**: new `detail::Contact2`
  (up to two points, each with its own penetration) + `obbObbManifold`, added additively to `Physics2D.hpp`.
  It runs the same SAT to find the separating axis + normal (a→b), then identifies the reference box (the
  one owning the axis) and the incident box, picks the reference face (outward normal ≈ the axis) and the
  incident face (most anti-parallel), and **clips** the incident segment to the reference face's two side
  planes (`clipSegment`, the standard Sutherland-Hodgman half-plane clip), keeping the clipped points that
  lie behind the reference face with their depth as penetration. `resolveManifold` then applies the
  existing, tested single-point rotational solve (`resolveRot`) at *each* point, so two points sharing a
  face give the torque balance that holds a stack square. A new `solveManifolds` world flag (default
  **false**) selects this path in `stepRotational`; the else-branch is the exact previous single-point code,
  so every existing rotating scene (`tumble`, `joints`, `groove`) is byte-unchanged — confirmed by a serial
  golden run. `testManifold2` checks the geometry (two boxes overlapping in y give **two** points on the
  shared face, correct +y normal and 0.5 penetration; a horizontal overlap gives the (1,0) normal; a
  separated pair reports no contact) and the end-to-end payoff (a five-box tower settled with manifolds ON
  stays within 0.15 rad of upright, and is provably no worse-tilted than the single-point solver on the
  same scene). Unit count **3956 → 3975**. The new `stack` demo drops two identical five-box towers side by
  side, simulated once at startup and drawn statically: the left (`solveManifolds = true`) settles into a
  clean square tower, the right (false) shears and topples — the only difference between them is the flag.
  Deterministic golden (RMSE 0, threshold 0.06). ctest **65/65 → 66/66**. Honest scope: this is a two-point
  clip resolved with per-point sequential impulses; it is not yet *cross-frame warm starting* (persisting
  accumulated impulses by contact ID across steps) or a *block solver* (solving both points simultaneously),
  which Box2D adds for very tall/heavy stacks — and it is opt-in rather than the default until the existing
  demos migrate. Those remain physics gaps.

### Iteration 72 — "Benchmarking against Godot: normal-mapped 2D lighting" (done)
Rotating to 2D rendering (last was M101's soft shadows). Maz's 2D lights (M89 flat pools + occluder
shadows, M101 penumbra) all shade a light as a *flat* colour wash — they have no idea about the surface
underneath. Godot's Light2D reads a sprite's NORMAL MAP so each texel is lit by how squarely it faces the
light, which is what makes a flat-painted 2D wall or floor look embossed and three-dimensional. That
per-texel normal response was the clear rendering gap: it's pure shading math (a 3D Lambert term), fully
unit-testable, deterministic, and a brand-new header so it can't regress anything.
- [x] **M111 — Normal-mapped 2D lighting (`game::PointLight2D` / `shadeSurface`)**: a new math-only
  `NormalLight2D.hpp`. A `PointLight2D` floats at a `height` above the surface plane (setting the grazing
  angle) with a colour, `energy`, and `range`. `shadePointLight(p, n, albedo, L)` builds the 3D direction
  from the texel (on the z=0 plane) up to the light, takes the Lambert **N·L** against the texel's unit
  surface normal `n` (tangent space, +z out of the screen), and multiplies by a smooth `(1−(d/range)²)²`
  distance falloff — returning zero when the texel faces away or lies outside range. `shadeSurface` sums
  several lights over an `albedo·ambient` base and clamps to [0,1]; `decodeNormal` unpacks the usual
  blue-encoded normal-map texel. `testNormalLight` pins it down: a flat texel under an overhead light is
  fully lit; a normal tilted toward the light beats one tilted away; a back-facing or out-of-range texel
  gets nothing; nearer beats farther; ambient keeps unlit areas off pure black while a strong light clamps
  to white; the flat (0.5,0.5,1) texel decodes to +z and every decode is unit-length. Unit count **3975 →
  3992**. The new `normalmap` demo paints one flat stone-coloured surface with a procedural dome-bump
  normal map and lights it with three coloured point lights (warm, cool, magenta): each dome is bright on
  the side facing a light and shadowed on the far side, so the flat field reads as rows of raised studs,
  and the three colour pools overlap and mix. Purely additive (new header + new app), so every existing
  golden is byte-unchanged (confirmed by a serial golden run); the shaded field is static → deterministic
  golden (RMSE 0, threshold 0.05). ctest **66/66 → 67/67**. Honest scope: this is the normal-map lighting
  model computed per cell on the CPU; it is not yet a GPU fragment-shader sprite-material path, a
  texture-projected light *cookie*, or specular/rim terms — those remain rendering gaps (and a true
  per-pixel shader path needs the Vulkan sprite pipeline, not just this math).

### Iteration 73 — "Benchmarking against Godot: flow-field pathfinding" (done)
Rotating to gameplay/AI (last AI was M108's GOAP). Maz already has grid A* (M57) and navmesh A* (M87),
and Godot's NavigationServer is likewise per-agent A* — every unit runs its own search. For a large crowd
converging on ONE goal that is wasteful and doesn't scale; the standard answer, which neither Godot nor
Maz had, is a FLOW FIELD: search once from the goal and let every agent read a precomputed direction. It's
a distinct, recognisable capability, pure grid math (deterministic, headless-testable), and a brand-new
header so it can't regress anything — the clear highest-leverage crowd-AI pick.
- [x] **M112 — Flow-field pathfinding (`game::FlowField`)**: a new header. `build(w,h,blocked,goalX,goalY)`
  runs one 8-connected **Dijkstra outward from the goal** (diagonal cost √2, corner-cutting disallowed
  like the existing NavGrid, stable ties by insertion sequence) to fill an **integration field** — the
  least cost-to-goal for every cell — then **bakes the flow field**: each reachable walkable cell stores a
  unit vector toward its lowest-cost eligible neighbour, i.e. straight down the cost gradient to the goal.
  Any number of agents then path for free via `sampleFlow(worldPos, cellSize, origin)`, which maps a world
  position to its cell's direction. `testFlowField` pins it down: in an open corridor the cost rises one
  per cell and every cell flows toward the goal; on an open grid the far corner reaches the goal by a
  straight diagonal (cost 4√2) and mid cells flow at it (dot > 0.5); a wall makes cells behind it cost more
  than the straight-line distance and steer toward the gap rather than into the wall, and wall cells carry
  zero flow; a boxed-off goal leaves outside cells unreachable; `sampleFlow` picks the right cell and
  returns zero outside the grid. Unit count **3992 → 4020**. The new `flowfield` demo draws the whole
  pipeline: the integration field as a heat map (warm near the goal, cold far), the baked flow as a grid
  of arrows, two staggered wall barriers, and 90 agents released on the left that were streamed along the
  field (once, at startup, fixed timestep) around both barriers to the goal — their trails tracing the
  flow lines. Purely additive (new header + new app), so every existing golden is byte-unchanged (confirmed
  by a serial golden run); precomputed → deterministic golden (RMSE 0, threshold 0.05). ctest **67/67 →
  68/68**. Honest scope: this is a single static-goal flow field with per-cell (not sub-cell-interpolated)
  sampling; it does not yet blend flow with local RVO avoidance (M98) for agent-agent separation, re-bake
  incrementally when the goal moves, or do hierarchical/portal flow fields for huge maps — those remain
  crowd-AI gaps.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

Later (Godot-gap priorities + backlog): wiring the DSP buses into the real-time mixer (per-voice bus
routing) + chorus/phaser/limiter/pitch-shift effects + WAV/OGG file loading (M106 completes the core
AudioEffect set; M99 gives the bus core); nested sub-state-machines + root-motion on the animation state
machine (M105 gives a flat cross-fading state machine); utility-AI scorers + HTN task-network planners +
numeric/fuzzy world state on top of the planner (M108 gives boolean-fact STRIPS-style GOAP A* planning;
M104 rounds out the classic BT node set + blackboard); a full per-control-class theme *cascade* (inherited
base types) + anti-aliased StyleBoxFlat corners + theme overrides bound to the live widget set (M109 gives
StyleBoxFlat rounded corners/border/shadow + a flat named-style Theme registry; M103 gives the nine-slice
texture mapping);
cross-frame warm starting + a block solver for very tall stacks + making two-point manifolds the default +
motorized/limited slider + gear/weld joints (M110 gives opt-in two-point contact manifolds for stable
stacks; M102 completes the pin/spring/groove trio); a GPU fragment-shader sprite-material path + a
texture-projected light *cookie* + specular/rim terms (M111 gives CPU normal-mapped point lighting; M101
gives soft shadows); call-method/trigger tracks + a visual track editor on the
timeline (M100 gives value tracks + per-segment easing);
ORCA half-plane avoidance + blending flow fields with local RVO for agent separation + incremental
goal-move re-bake + hierarchical/portal flow fields (M112 gives static-goal flow-field crowd pathfinding;
M98 gives agent-vs-agent velocity-sampling RVO); FABRIK pole targets + per-bone angle
constraints + a Skeleton2D-integrated IK modification stack (M107 gives multi-bone FABRIK chains; M97 gives
closed-form 2-bone IK); 47-tile Wang autotiling + BSP/room
dungeon gen (M96 gives 4-bit autotiling + cellular caves); text selection/clipboard + multi-line
TextEdit + controller UI nav (M95 gives single-line LineEdit + focus); 3D spatial
audio + doppler + WAV/OGG loading (M94 gives 2D pan/attenuation); animation blend *trees* (state-machine
over blend spaces) + IK (M92 gives blend spaces); 3D rigid-body physics; GPU particles;
navmesh dynamic obstacles; parallel/decorator BT nodes + a blackboard, GPU skinning, glTF skin/animation
import, prefabs/blueprints on the scene serializer, an ECS Transform/Parent wired to TransformGraph,
noise-driven tilemap/cave generation, localization / string tables, order-independent transparency,
material/uniform system, cross-platform CI, a deterministic hold-frame screenshot mode.
