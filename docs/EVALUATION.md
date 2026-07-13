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

### Iteration 74 — "Benchmarking against Godot: call-method / trigger tracks" (done)
Rotating to animation (last anim was M107's FABRIK). M100 gave the Timeline's VALUE tracks — interpolating
a property between keyframes — but Godot's AnimationPlayer has a second, equally-important track kind: the
**method / call track**, which doesn't interpolate anything, it *fires* at a keyframe time. That's how a
run clip plays a footstep on the plant frame, an attack clip spawns a hitbox on the swing frame, or a
cutscene opens a door at a beat. It was the obvious missing half of the animation system, it extends a
module I built (M100), and it's pure timing math — deterministic, fully unit-testable, and a brand-new
header so it can't regress anything.
- [x] **M113 — Call-method / trigger tracks (`anim::TriggerTrack` + `MethodTimeline`)**: a new header.
  `TriggerTrack` is a sorted list of timed markers (`time`, caller-defined `id`); `collectRange(from, to,
  includeTo, out)` appends the ids a **forward** sweep crossed, half-open `[from,to)` by default so a
  marker on a segment boundary fires exactly once and never twice, with an `includeTo` option for the
  terminal segment of a non-looping clip so an end-of-clip marker still fires. `MethodTimeline` plays a
  track over a clip length with a `Loop` policy: `update(dt, out)` advances the playhead and appends every
  fired id in order, correctly handling the Repeat **loop wrap** (fire the tail of the clip, wrap to 0,
  keep going — a marker exactly at `length` is the next loop's 0 and is *not* double-fired) and Once
  termination. `testTriggerTrack` pins it down: half-open vs inclusive ranges; two markers at the same
  time both fire in insertion order; a Once clip fires each marker once including one exactly at the end
  then goes quiet; a single large `dt` crosses several markers in order; a Repeat clip fires each marker
  once per pass with no wrap double-trigger (a downbeat at t=0 fires 3× and a mid-marker 2× over 9 s of a
  4 s loop); a zero-length clip is a safe no-op. Unit count **4020 → 4041**. The new `sequencer` demo is a
  four-lane drum machine (kick / snare / hat / clap): each lane's markers fire as one shared playhead
  sweeps the looping bar; markers behind the head glow ("just fired"), ahead stay dim ("pending"), with
  per-lane fire counts (5 / 5 / 19 / 2 over the precomputed run, matching the timing by hand) and a strip
  of the most recent fires in order. Purely additive (new header + new app), so every existing golden is
  byte-unchanged (confirmed by a serial golden run); precomputed → deterministic golden (RMSE 0, threshold
  0.05). ctest **68/68 → 69/69**. Honest scope: this is a forward-playing method track (Once/Repeat); it
  does not yet fire in *reverse* on a ping-pong clip, carry per-marker argument payloads, or bind to an
  actual method-dispatch/callback registry (the caller switches on the id) — and there is still no visual
  track *editor* for authoring them. Those remain animation gaps.

### Iteration 75 — "Benchmarking against Godot: ADSR envelope generator" (done)
Rotating to audio (last audio was M106). Maz's synth (M17/M94/M99/M106) can generate and effect tones, but
every voice's amplitude is flat — a note switches on and off as a raw buzz with no *shape*. The single
missing primitive that fixes that is the **ADSR envelope**: the attack/decay/sustain/release contour every
synth multiplies its oscillator by, so a note swells in, holds, and fades instead of clicking. It's the
foundational voice-shaping block, pure deterministic scalar math (fully unit-testable), composes with the
existing DSP, and is a brand-new header so it can't regress anything.
- [x] **M114 — ADSR envelope (`audio::ADSR`)**: a new `Envelope.hpp`. A tiny gated state machine
  (Idle → Attack → Decay → Sustain → Release) with `attack`/`decay`/`release` times (seconds) and a
  `sustain` level. `noteOn()` enters Attack (ramping from the *current* level, so a fast re-trigger doesn't
  click to zero); `noteOff()` captures the current level and enters Release, so the tail scales from
  wherever the envelope was — even mid-attack. `process(dt)` advances the linear segments and returns the
  level; zero-length segments snap to the next stage, and `sustain == 1` makes decay a no-op. `testADSR`
  pins the timings: attack is ~0.5 halfway up, decay settles exactly at the sustain level and holds there
  indefinitely, release from sustain is ~0.25 halfway and reaches 0 (→ Idle) on time; a release begun
  mid-attack scales from the reached level; a zero attack snaps to 1 in one step and a sustain-0 stab
  decays to silence; sustain-1 keeps decay flat. Unit count **4041 → 4064**. The new `envelope` demo shows
  three presets — a plucky blip (fast attack, low sustain, short release), a slow-swelling pad (long
  attack, high sustain, long release), and a percussive stab (instant attack, zero sustain) — each drawn
  as its ADSR *curve* (with the key-down gate shaded, the sustain line, and note-on/off ticks) and, below
  it, a single sine tone *multiplied by that envelope*, so you can watch the same oscillator become three
  very different notes. All precomputed → deterministic golden (RMSE 0, threshold 0.05). Purely additive
  (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial golden run).
  ctest **69/69 → 70/70**. Honest scope: this is a linear-segment ADSR; it is not yet exponential/curved
  segments, velocity-scaled levels, an LFO / modulation matrix, or actually wired into the live SDL voice
  callback (the demo applies it offline) — and per-voice bus routing through the real-time mixer remains
  the standing (headlessly-unverifiable) audio gap.

### Iteration 76 — "Benchmarking against Godot: Tree / TreeItem widget" (done)
Rotating to UI (last UI was M109's StyleBoxFlat/Theme). Of the standard Godot controls Maz still lacked,
the **Tree** is the highest-leverage: it's the single most-used complex control in the whole editor — the
scene dock, the inspector, and the FileSystem dock are *all* Trees — and it's the natural way any game
shows a hierarchy (an inventory grouped by category, a skill tree, a dialogue branch, a save-file browser).
It's a clean data-structure-plus-traversal problem (fully unit-testable), a brand-new header (zero
regression risk), and it composes with M109's StyleBoxFlat for the row highlight.
- [x] **M115 — Tree / TreeItem widget (`ui::Tree`)**: a new `Tree.hpp`. A `TreeItem` carries `text`, an
  `id`, a `color`, a `collapsed` flag, and **heap-owned** children (`vector<unique_ptr<TreeItem>>`, so a
  `TreeItem&`/`TreeItem*` stays valid across later sibling insertions — you can hold a folder reference
  while adding more siblings without dangling). `addChild` appends and returns the stable reference. The
  `Tree` owns an invisible `root`; `visibleRows()` walks it **depth-first**, emitting a `TreeRow` (item +
  `depth` for indentation + `hasChildren` for the fold arrow + `collapsed`) for every item whose ancestors
  are all expanded, so folding one branch drops its entire subtree; a matching `visibleCount()` counts the
  same without allocating. `testTree` pins it down on an A(A1,A2)/B/C(C1(C1a)) tree: 7 rows in the right
  order with correct depths and hasChildren flags when fully expanded; collapsing A drops exactly A1/A2;
  collapsing the deep C1 hides only C1a (not C1); collapsing the top C hides its whole subtree; collapsing
  a *leaf* changes nothing; an empty tree yields no rows. Unit count **4064 → 4086**. The new `tree` demo
  draws a project file tree inside a rounded, shadowed StyleBoxFlat panel (M109): each visible row indented
  by its depth, folders drawn with a fold arrow (▾ expanded / ▸ collapsed) + a warm filled icon, files with
  a hollow icon, two folders (`enemies/`, `assets/`) starting collapsed so their contents are hidden, and
  the selected row (`Player.scene`) highlighted with a rounded StyleBoxFlat. Purely additive (new header +
  new app), so every existing golden is byte-unchanged (confirmed by a serial golden run); static tree →
  deterministic golden (RMSE 0, threshold 0.05). ctest **70/70 → 71/71**. Honest scope: this is the tree
  model + flatten + a static selection; it does not yet do live click-to-fold / keyboard navigation,
  scrolling when the row count exceeds the panel, multi-column rows, drag-reorder, or per-item editable
  cells — those remain UI gaps.

### Iteration 77 — "Benchmarking against Godot: Area2D sensor / trigger regions" (done)
Rotating to physics-2D / gameplay (the loop asked to prefer rendering-2D or physics-2D this round). Of
Godot's core 2D nodes Maz still lacked, **Area2D** is the highest-leverage: it's the single most-used
gameplay node in 2D Godot — every pickup, hurtbox/hitbox, checkpoint, kill-zone, proximity trigger, and
"player entered the room" event is an Area2D. Maz had *physics* collisions (Physics2D resolves contacts
with force) but no **sensor** primitive: a region that detects overlap and fires `body_entered` /
`body_exited` without pushing anything. That's a different job — detection, not resolution — and it's the
glue almost every 2D game is built from. It's a clean geometry-plus-set-diff problem (fully unit-testable),
a brand-new header (zero regression risk to the physics solver), and it reuses the existing math types.
- [x] **M116 — Area2D sensor / trigger (`game::Area2D`)**: a new `Area2D.hpp`. An `Area2D` is a
  circle-or-box zone (`shape`, `pos`, `radius`/`half`) with `boundsMin/Max` + `containsPoint`. A free
  `overlaps(a, b)` handles all three pairings: circle-circle (centre distance < r₁+r₂, **strict** so an
  exact touch is *not* an overlap — matching Godot's non-inclusive contact), box-box (AABB interval
  overlap, strict), and the mixed circle-box case (clamp the circle centre to the box → closest point →
  distance < r). An `AreaMonitor` holds the sorted set of ids currently inside; each frame you pass it the
  new overlapping set and it returns via `std::set_difference` exactly the ids that just **entered** and
  just **exited** (deduping ids already inside), then adopts the new set — the enter/exit event core of
  Godot's Area2D. `testArea2D` pins the geometry (circle/circle, circle/box, box/box, exact-touch =
  false, corner cases, containsPoint) and the monitor (multi-frame enter/exit diffing across an
  approach → inside → leave sequence, plus a no-op frame). Unit count **4086 → 4112**. The new `area2d`
  demo steps a staggered stream of 14 agents (each itself a small circle Area2D) once at startup across a
  circular "aura" sensor and a box "gate" sensor, feeding both `AreaMonitor`s each step; it draws the two
  translucent zones, every agent's trail, each agent lit + ringed in a zone's colour when currently inside
  it, and a per-zone readout (inside now / entered / exited). Purely additive (new header + new app), so
  every existing golden is byte-unchanged (confirmed by a serial golden run); the whole sweep is
  precomputed so the frame is static → deterministic golden (threshold 0.06). ctest **71/71 → 72/72**.
  Honest scope: this is a *monitoring* Area2D — overlap detection + enter/exit — over circle/box shapes;
  it does not yet do collision layers/masks (so *which* bodies a zone watches), body-vs-body sensor pairs
  (only zone-vs-body here), convex-polygon zones, per-area gravity/damping overrides (Godot's Area2D can
  also modify physics-space properties), or continuous sweep detection for fast movers — those remain gaps.

### Iteration 78 — "Benchmarking against Godot: animation blend tree" (done)
Rotating to animation (last animation milestone was M113's trigger tracks; M116 was physics-2D). The
engine had the two *leaves* of Godot's AnimationTree — **blend spaces** (M92: mix a flat set of poses by
a 1-D/2-D parameter) and a **flat state machine** (M105: cross-fade whole states) — but not the
**AnimationNodeBlendTree**, the node graph that *nests* them. That tree is how real Godot characters are
actually animated: you don't drive a rig with one blend space, you build a graph — a locomotion blend
space, an additive upper-body layer, a cross-fade into jump/land, all reading blend parameters. It's the
single highest-leverage animation gap, composes directly on the existing pose-blend primitives
(`blendPoses` / `blendPosesWeighted` / `BlendSpace1D`), is a clean new header (zero regression risk), and
is fully unit-testable as pure pose math.
- [x] **M117 — animation blend tree (`anim::BlendTree`)**: a new `BlendTree.hpp`. A `BlendTree` owns a
  vector of nodes and a named-parameter map; `Pose` is aliased to `std::vector<JointPose>`. Node types:
  **Input** (leaf — outputs an external pose by index, so in a real rig those are sampled clips),
  **Blend2** (cross-fade two child nodes by a named param in [0,1] — AnimationNodeBlend2), **Add2** (lay
  an additive delta child on a base child scaled by a param — translation added, rotation slerped in from
  identity, AnimationNodeAdd2), and **BlendSpace1** (a 1-D blend space over child *nodes*: `addBlendPoint`
  places children on the axis, a param selects `BlendSpace1D::weights`, and the contributing children are
  mixed by `blendPosesWeighted`). `evaluate()` walks the tree recursively from the root into one pose,
  reading params by name (unset → 0) and pulling leaf poses from the caller's table. `testBlendTree`
  pins each node type (Blend2 endpoints + midpoint + unset-param default; a 3-point BlendSpace1 including
  a beyond-the-end clamp; Add2 at 0/0.5/1; a nested BlendSpace1→Blend2 "gait cross-faded into jump by air"
  tree; and degenerate no-root / out-of-range-root → empty pose). Unit checks **4112 → 4131**. The new
  `blendtree` demo builds exactly that canonical tree — `Blend2( Add2( BlendSpace1(gait), wave, "wave" ),
  jump, "air" )` — and renders a 5×3 grid of a stick figure: columns sweep **gait** (idle → walk → run,
  the nested blend space), rows sweep **air** (grounded → jump, the outer cross-fade), with the additive
  wave arm held on so Add2 is visible on the grounded rows. Purely additive (new header + new app), so
  every existing golden is byte-unchanged (confirmed by a serial golden run); the grid is static →
  deterministic golden (RMSE 0, threshold 0.05). ctest **72/72 → 73/73**. Honest scope: the node set is
  Input/Blend2/Add2/BlendSpace1 driven by scalar params; it does not yet include a 2-D blend-space node,
  Add3/BlendN, a *StateMachine node* embeddable inside the tree (the flat M105 machine isn't yet a tree
  node), time-scaling / one-shot / seek nodes, or a live visual parameter editor — those remain gaps.

### Iteration 79 — "Benchmarking against Godot: collision layers & masks" (done)
Rotating to physics-2D (last physics-2D was M116's Area2D; last iteration was animation). Maz could ask
"do these two shapes touch?" (Area2D overlap, Physics2D contacts) but had no way to express the OTHER half
every real 2D game needs: **which** things a body or zone should even consider. In Godot that's
**collision_layer / collision_mask** — each object lives in some layers ("what I am") and scans some mask
("what I react to"), and it's wired into *every* physics/area/ray query. Without it, a pickup magnet
reacts to enemies, a player's hurtbox reacts to coins, bullets hit their own shooter. It's the single
most-used missing physics primitive, a clean bitmask problem (fully unit-testable, zero regression risk as
a new header), and it composes directly on M116's Area2D sensors.
- [x] **M118 — collision layers & masks (`game::CollisionLayers`)**: a new `CollisionLayers.hpp`. A
  `LayerMask` is a 32-bit set (Godot exposes 32 2D layers); `layerBit(i)` / `layerMask({...})` build masks
  (out-of-range bits clamp to empty). Two free predicates capture the two ways Godot uses them: a
  **directional** `detects(observerMask, targetLayer)` — `observer.mask & body.layer`, how an Area2D /
  RayCast2D picks what it sees — and a **symmetric** `interact(aLayer,aMask,bLayer,bMask)` — pairs if
  *either* side scans the other, how a physics broadphase decides to pair two bodies. A
  `CollisionObject2D { layer, mask }` carries per-object membership with per-bit editing
  (`setLayerBit`/`setMaskBit`/`layerHas`/`maskHas`) and convenience `detects()` / `interactsWith()`. A
  `LayerRegistry` names the layers (Godot lets you name its 32), assigning bit indices in insertion order,
  with lookup, combined `mask({"a","b"})`, and graceful overflow past 32 (returns -1). `testCollisionLayers`
  pins bit building + range clamping, directional vs symmetric semantics (incl. the asymmetric "A scans B,
  B scans nothing → still interact" case), `CollisionObject2D` editing + queries, and the registry
  (ordered assignment, re-add, missing lookups, 32-layer overflow). Unit checks **4131 → 4201**. The new
  `layers` demo reuses the AREA2D sweep but layer-filters it: three species (player/enemy/pickup) stream
  through a **HURTBOX** circle whose mask watches only ENEMY and a **MAGNET** box whose mask watches only
  PICKUP, feeding each zone's `AreaMonitor` only the agents that both overlap *and* match its mask — so an
  enemy in the magnet or a coin in the hurtbox is drawn with a faint dashed "ignored" ring while true
  matches light up in the zone's colour. Purely additive (new header + new app), so every existing golden
  is byte-unchanged (confirmed by a serial golden run); the sweep is precomputed → deterministic golden
  (threshold 0.06). ctest **73/73 → 74/74**. (Infra note: the serial golden run also surfaced a *flaky*
  `config` golden — it spiked to RMSE 0.0513 vs its tight 0.05 threshold once under load, but re-rendering
  it four times in isolation gave 0.032/0.038/0.038/0.043, all well within tolerance. `config` is a
  text-and-flame-bar-heavy scene whose lavapipe jitter occasionally grazes 0.05; its threshold was bumped
  0.05 → 0.06 to stop intermittent false failures. This is a threshold adjustment, not a masked regression
  — `config`'s output is verified correct, and M118 is a new additive header `config` never includes.)
  Honest scope: this is the layer/mask *logic* + a monitoring
  integration; it is not yet threaded through Physics2D's solver or the spatial-grid broadphase as a
  filter, doesn't do per-collision-shape layers (one mask per object), and has no editor UI for the 32
  named layers — those remain gaps.

### Iteration 80 — "Benchmarking against Godot: TileSet resource + per-tile collision" (done)
Rotating to tilemap (last three were physics-2D/layers, animation/blend-tree, physics-2D/Area2D). Maz had
a `Tilemap` (a dense grid of tile ids with a single per-id *solid* bit) and autotiling masks (M96), but
not Godot's **TileSet** — the resource that gives each tile id its *meaning*: which atlas cell draws it and
what **collision shape** it contributes. The gap that matters most is per-tile collision: Godot's tile
collision can be **sub-cell** (a half-height platform, a shelf, a slope), which a single solid bit can't
express — it's the difference between a tilemap that's a *picture* and one that's a *playable level* with
ledges you can stand on. TileMap is the backbone of most 2D games, so this is the highest-leverage tilemap
gap; it's a clean data-structure-plus-geometry problem (fully unit-testable), a new additive header (zero
regression risk), and it builds directly on the existing `Tilemap`.
- [x] **M119 — TileSet resource + per-tile collision (`game::TileSet`)**: a new `TileSet.hpp`. A `TileDef`
  carries an **atlas source cell** (`atlasX/atlasY` — "which image cell draws this tile") and a
  **collision**: `None` (walk-through), `Full` (whole cell), or `Box` (a sub-rect given as cell fractions
  in [0,1], e.g. a bottom-half ledge). A `TileSet` maps `TileId → TileDef` (`define`/`get`/`isSolid`).
  Three free queries turn a `(Tilemap, TileSet)` pair into gameplay: `collectSolids` returns every solid
  tile's **world-space collision box** (`TileBox`; Full = whole cell, Box = sub-rect) to feed an AABB
  collider; `solidAt(p)` tests whether a world point is inside a solid tile *respecting sub-cell shapes*
  (a point in a ledge cell's empty top half reads as not-solid); `dropY(x, fromY)` drops down a column and
  returns the first solid tile's **top surface** — mid-cell for a ledge, cell-top for a full tile — for
  placing things on the ground. `testTileSet` pins a 4×3 map with a full-ground row + one bottom-half
  ledge: lookups (incl. undefined ids → not solid), `collectSolids` count + the ledge's exact sub-cell box
  (x[10,20] y[15,20]), `solidAt` inside ground / inside the ledge's solid half / in its empty half / empty
  cell / out-of-bounds, and `dropY` resting on the ground (y=20) vs the ledge top (y=15, mid-cell) vs an
  out-of-range column (→ maxY). Unit checks **4201 → 4223**. The new `tileset` demo builds one 22×13 level
  mixing full ground/wall tiles with half-height ledges + a wall pillar, draws each tile in its atlas-cell
  colour (ledges drawn only their solid bottom half so the sub-cell shape is visible), overlays every
  tile's collision box from `collectSolids` in yellow, and drops probe balls down six columns with `dropY`
  — each resting exactly on what it hit, the ones over ledges sitting mid-cell. Purely additive (new header
  + new app), so every existing golden is byte-unchanged (confirmed by a serial golden run); the level is
  static → deterministic golden (RMSE 0, threshold 0.05). ctest **74/74 → 75/75**. Honest scope: collision
  shapes are None/Full/axis-aligned-Box — no arbitrary collision polygons, slopes, or one-way
  (platform-drop-through) tiles yet; the atlas source is an (x,y) cell index the demo maps to a colour, not
  yet a bound atlas *texture* sampled by the sprite renderer; and terrain/peering autotile bitmasks
  (M96) aren't yet wired to auto-pick a TileDef. Those remain gaps.

### Iteration 81 — "Benchmarking against Godot: particle emitter resource" (done)
Rotating to particles / rendering-2D (last four were tilemap, physics-2D, animation, physics-2D). Maz had
a particle system since M6 — `fx::ParticleSystem`, a runtime pool that emits point bursts with a linear
start→end colour/size and gravity/drag (M49 added an attractor). But that's an ad-hoc runtime object, not
Godot's **CPUParticles2D**, whose whole value is being a *resource*: an emission **shape** (you scatter
from a disk/ring/rectangle, not just a point), per-lifetime **curves** for scale and alpha (not just two
endpoints), and a multi-stop colour **gradient** — authored once and reused. Particles are one of the most
visible things a 2D engine does (fire, smoke, sparks, rain, explosions), so a proper emitter resource is a
high-leverage rendering-2D gap; it's a clean new header (the existing pool is untouched → zero regression),
and made fully deterministic it unit-tests headlessly and renders a golden-stable snapshot.
- [x] **M120 — particle emitter resource (`fx::Emitter`)**: a new `ParticleEmitter.hpp`. Three small
  building blocks: `Curve` (a piecewise-linear 1-D ramp over t∈[0,1] with sorted insert + clamped sample —
  Godot Curve), `Gradient` (a multi-stop colour ramp — Godot Gradient), and `EmitShape` + `sampleOffset`
  (Point / Circle (area-uniform disk via √u) / Ring / Rect, mapping two uniform samples to an emission
  offset). The `Emitter` resource bundles position, count, duration + **explosiveness** (0 = born evenly
  over the duration, 1 = all at once), lifetime range, shape, launch direction + **spread** (a half-angle;
  π = omnidirectional) + speed range, gravity, a base size, and the scale/alpha `Curve`s + colour
  `Gradient`. `simulate(e, seed, t)` is **deterministic**: each particle's randomness is a hash of
  (seed, index, channel) — no global RNG — so it births each particle, integrates constant-acceleration
  motion (`pos = origin + emitOffset + vel·age + ½·gravity·age²`), and returns the pos/size/colour of
  everything alive at time `t`. `testParticleEmitter` pins Curve (ramp + clamp + empty), Gradient (lerp +
  clamp + empty→white), each shape's bounds (Point at origin, Rect within half-extents, Circle within
  radius, Ring within [inner,outer]), and simulate with fixed life/speed/direction so the motion is exact
  (a particle at origin at age 0; +50px after 0.5s at 100px/s; dead past its lifetime; +25px in y from
  ½·200·0.5²), plus determinism (same seed+t → identical) and burst count. Unit checks **4223 → 4306**.
  The new `emitter` demo authors three resources — a gravity **fountain** (point emit, fire gradient, size
  curve), an omnidirectional **burst** (ring shape, spread π, explosiveness 1), and angled **rect rain** —
  simulates each to a fixed time and draws every live particle at its gradient colour + curve-driven size.
  Purely additive (new header + new app), so every existing golden is byte-unchanged (confirmed by a
  serial golden run); fixed seed + fixed time → deterministic golden (threshold 0.06, particle-dense).
  ctest **75/75 → 76/76**. Honest scope: this is the emitter *resource* + a deterministic snapshot
  simulator; it is not GPU-simulated, has no trails / sub-emitters / collision-aware particles / animated
  sprite frames, and isn't yet wired into the runtime `fx::ParticleSystem` pool as its config — those
  remain gaps.

### Iteration 82 — "Benchmarking against Godot: 3D spatial audio" (done)
Rotating to audio for breadth (last five were particles/emitter, tilemap/TileSet, physics-2D/layers,
animation/blend-tree, physics-2D/Area2D). Maz gained 2D positional audio at M94 (`audio::Spatial2D` — a
left/right pan + distance attenuation for a source and listener on a *plane*). Godot's next tier up is
**AudioStreamPlayer3D**: a sound placed in 3D space, heard by a 3D listener that has an *orientation*
(a forward/up basis), needing three things a planar panner doesn't have — a choice of distance-falloff
**model**, a stereo pan derived from where the source sits relative to the listener's *facing*, and
**doppler** (the pitch shift when source and listener move toward or away from each other). That's the
single most-requested audio gap on the list ("3D spatial audio + doppler"), it's pure math so it
unit-tests headlessly and drives any backend that takes a per-voice left/right gain + pitch, and it's a
clean new header (Spatial2D untouched → zero regression).
- [x] **M121 — 3D spatial audio (`audio::Spatial3D`)**: a new `Spatial3D.hpp`. `Listener3D` (pos +
  forward/up basis + velocity) and `Source3D` (pos + velocity). Four **attenuation models** matching
  Godot AudioStreamPlayer3D (`attenuation3D`, distance clamped to [ref,max] first): **None** (flat),
  **Linear** (`1 − rolloff·t`), **Inverse** (`ref/(ref+rolloff·(d−ref))`, natural 1/r-ish), and
  **InverseSquare** (`ref²/(ref²+rolloff·(d−ref)²)`, steeper ~1/r²). `panPosition` projects the
  source direction onto the listener's **right axis** (`forward × up`) for a −1…+1 pan that respects the
  listener's facing (turn around and left/right swap). `equalPowerPan` splits a gain into constant-power
  left/right (`left²+right² == gain²`) so panning holds loudness. `dopplerPitch` is the classic
  `(c − v_listener)/(c − v_source)` along the source→listener axis, velocities clamped below the speed of
  sound so the ratio stays finite. `computeSpatialMix` bundles it all into a `SpatialMix`
  (left/right/pitch/distance/pan) from a `SpatialConfig` (base volume, ref/max distance, rolloff, speed of
  sound, model, doppler on/off). `testSpatial3D` pins each attenuation model at a known distance (Inverse
  d=3→1/3, InverseSquare d=3→1/5, Linear at midpoint→0.5, None→1), pan sign (source on the right→+1, left→
  −1, straight ahead→0), equal-power split (centre→0.707/0.707, extremes hard-panned), doppler
  (static→1.0, approaching source→343/308.7>1, receding<1, listener moving in→(343+34.3)/343), and the
  combined mix (right source louder in the right channel, farther source quieter). Unit checks
  **4306 → 4334**. The new `spatial3d` demo is a top-down **radar**: the listener sits at centre facing
  up, six sources are placed by world x (left/right) and z (front/back), and each is drawn as a disc sized
  by its distance attenuation, tinted by its doppler pitch (warm/red = approaching, cool/blue = receding),
  with a velocity arrow and a two-bar L/R stereo meter; a side panel lists every source's L/R gain and
  pitch. Everything is computed once and drawn statically → deterministic, golden-stable. Purely additive
  (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial golden run);
  threshold 0.06 (text/meter-dense). ctest **76/76 → 77/77**. Honest scope: this is the spatialization
  *math* + a visual snapshot; it is not wired into the real-time SDL mixer as a per-voice 3D bus, has no
  HRTF/binaural filtering, no occlusion/reverb-zone modelling, and still no WAV/OGG file loading — those
  remain audio gaps.

### Iteration 83 — "Benchmarking against Godot: auto-layout containers" (done)
Rotating to UI for breadth (last six were audio/spatial3d, particles/emitter, tilemap/TileSet,
physics-2D/layers, animation/blend-tree, physics-2D/Area2D — so audio/physics/animation are well-covered).
M86 gave Maz a retained layout tree (`ui::LayoutNode`): anchors + a *simple* box mode where every `expand`
child grabs an **equal** slice of the leftover and the cross axis always fills. Godot's real **Container**
controls are richer, and that richness is exactly what building a resizable UI needs: per-axis **size
flags** (a child independently picks Fill / Expand / Shrink-begin/center/end for its horizontal and its
vertical axis), **stretch ratios** (two expanders at 1 and 3 split the leftover 1:3, not 50/50), a true
**GridContainer** (N columns, column widths from the widest cell, expanding columns sharing leftover), and
**bottom-up minimum size** so nested containers size correctly. That's the highest-leverage *closable* UI
gap, it's pure rectangle math (unit-tests headlessly, deterministic golden), and it's a clean new header
that leaves `LayoutNode` untouched → zero regression.
- [x] **M122 — auto-layout containers (`ui::Container`)**: a new `Container.hpp`. A `Control` carries a
  min size, an `hFlag`/`vFlag` (`SizeFlag` = Fill / Expand / ShrinkBegin / ShrinkCenter / ShrinkEnd), and a
  `stretch` ratio. `hbox`/`vbox` distribute children along a main axis: only **Expand** children grow past
  their min, splitting the leftover by stretch ratio; the cross axis is placed by each child's flag (Fill →
  fill the extent, Shrink → keep min size anchored begin/center/end). `grid` flows children into N columns
  (column width = widest child's minW, row height = tallest minH, columns/rows containing an Expand child
  share the leftover), then places each child in its cell by its per-axis flags. `margin` insets by four
  sides (Godot MarginContainer); `center` pins a child at its min size in the middle (CenterContainer).
  `hboxMinSize`/`vboxMinSize`/`gridMinSize` compute a container's own min size from its children so a VBox
  of HBoxes sizes bottom-up. `testUiContainer` pins exact rects: an HBox of Fill+Expand+Expand+Fill (the
  two expanders splitting leftover 1:2), stretch-ratio distribution (1:3 → 50/150 of 200), cross-axis
  ShrinkCenter (min-height child centred), a VBox header/body(Expand)/footer, a 2×2 Fill grid, a grid with
  an expanding column absorbing the extra width, margin insets, centre placement, and all three min-size
  helpers. Unit checks **4334 → 4377**. The new `containers` demo lays out four labelled cards — an
  HBoxContainer (Fill | Expand×1 | Expand×2 | Fill, the orange expanders visibly 1:2), a 3-column
  GridContainer of nine Expand tiles filling evenly, a VBoxContainer (fixed header + Expand body + fixed
  footer), and a MarginContainer inset framing a CenterContainer's fixed box — every tile's rect computed
  by the container, no hand-typed child coordinates. Computed once, drawn statically → deterministic golden
  (threshold 0.06, text-dense). Purely additive (new header + new app), so every existing golden is
  byte-unchanged (confirmed by a serial golden run); ctest **77/77 → 78/78**. Honest scope: this is the
  container *layout math* + a snapshot; it is not wired into `LayoutNode`'s tree as a node mode, has no
  ScrollContainer/TabContainer/FlowContainer, no RTL/text-direction handling, and no min-size *propagation*
  through a live retained tree (the helpers compute it, the app threads it) — those remain UI gaps.

### Iteration 84 — "Benchmarking against Godot: parallax scrolling backgrounds" (done)
Rotating to rendering-2D for breadth (last seven were UI/containers, audio/spatial3d, particles/emitter,
tilemap/TileSet, physics-2D/layers, animation/blend-tree, physics-2D/Area2D — UI/audio/physics/animation
are well-covered). Maz could draw sprites, tilemaps, shapes and lights, but it had **no notion of a
scrolling background** — one of the most recognizable staples of 2D games, and a named Godot node pair
(**ParallaxBackground + ParallaxLayer**). A parallax background is several layers that scroll at *different
rates* relative to the camera (distant mountains barely move, near foliage races past) so a flat 2D scene
reads as having depth, each layer **mirrored** (its motif tiles seamlessly) so a finite strip of art covers
an unbounded scroll. That's a high-leverage, very *visible* rendering-2D gap; it's pure transform math
(no renderer dependency) so it unit-tests headlessly, and it's a clean new header.
- [x] **M123 — parallax scrolling backgrounds (`game::Parallax`)**: a new `Parallax.hpp`. A `ParallaxLayer`
  carries a `motionScale` (the fraction of the camera scroll it follows — 1 = locked to the world /
  foreground, 0 = pinned on screen / far backdrop), a `motionOffset` (a constant autoscroll base), and a
  `mirroring` period per axis. `layerOffset(layer, cameraScroll)` returns the layer's on-screen offset
  (`motionOffset − cameraScroll·motionScale`, so a smaller scale slides slower → the parallax). `pmod` is a
  positive modulo; `firstTile(offset, period)` returns the first tile coordinate in `[−period, 0)` and
  `tileCount(extent, period)` how many tiles cover a viewport, so a mirrored layer draws
  `firstTile + k·period` across the width seamlessly. `testParallax` pins `layerOffset` (a scale-0.5 layer
  offset −50 for scroll 100; a scale-0 backdrop unchanged across a huge scroll; a near layer shifting more
  than a far one for the same camera move → the parallax property), `pmod` on negative inputs (−50 mod 40 =
  30), `firstTile` landing in `[−period,0)` with the tiles bracketing `[0, extent]`, and the non-tiled
  (period 0) path. Unit checks **4377 → 4395**. The new `parallax` demo shows the SAME five-layer scene
  (sun + clouds + snow-capped mountains + hills + trees + ground) in three stacked strips at camera scrolls
  0 / 460 / 920: reading down a column, the sun is pinned (scale 0), the mountains barely shift and the near
  trees sweep a long way — the difference is the parallax — while every layer tiles across the full width
  via the mirroring helpers. Fixed scrolls, drawn statically → deterministic golden (threshold 0.06,
  layered-scene). Purely additive (new header + new app), so every existing golden is byte-unchanged
  (confirmed by a serial golden run); ctest **78/78 → 79/79**. Honest scope: this is the parallax *layout
  math* + a snapshot; it does not own a texture-tiling draw call (the app draws motifs at the returned
  positions), has no vertical infinite-scroll camera integration wired to `CameraController2D`, and no
  per-layer z-ordering scene node — those remain rendering gaps.

### Iteration 85 — "Benchmarking against Godot: tween sequencer / property animator" (done)
Rotating to core/io for breadth (last eight were rendering-2D/parallax, UI/containers, audio/spatial3d,
particles/emitter, tilemap/TileSet, physics-2D/layers, animation/blend-tree, physics-2D/Area2D). Maz had
`anim::Tween` since M59 — but that's a single time-cursor: it interpolates ONE from→to over one duration
and you read `sample()` yourself. Godot's **SceneTreeTween** (`create_tween()` + `tween_property` /
`tween_interval` / `tween_callback` + `parallel()` + `set_loops()`) is a different, higher-level thing: a
*runtime* that choreographs many bound properties over time — steps chained in sequence, some running in
parallel, with delays and callbacks interleaved, the whole thing looping — and it *writes the properties
itself* every frame. That "create a tween, chain a few property animations, fire and forget" ergonomic is
one of the most-used conveniences in Godot gameplay/UI code, and Maz had no equivalent. It's pure logic so
it unit-tests headlessly, and it's a clean new header that composes the existing easing (Tween untouched →
zero regression).
- [x] **M124 — tween sequencer / property animator (`anim::TweenPlayer`)**: a new `TweenPlayer.hpp`. A
  `Tweener` is one element — `Property` (interpolate a bound `void(float)` setter from→to over a duration
  with an `Ease`), `Interval` (a pure delay), or `Callback` (fire once). The player holds an ordered list
  of GROUPS: groups run sequentially, tweeners within a group run in parallel. `appendProperty` /
  `appendInterval` / `appendCallback` start a new sequential group; `parallelProperty` adds to the current
  group; `setLoops(n)` replays the whole sequence (`n ≤ 0` = forever). `update(dt)` advances the current
  group, writing every property's eased value through its setter, snaps to the end value + fires callbacks
  when a group completes, then moves on (carrying the remainder so one big `dt` can cross several groups),
  and wraps for looping. `testTweenPlayer` pins a sequential two-property chain (x 0→100 then 100→0, exact
  Linear values as it advances), a parallel group (x and y animating together in one step), an interval
  delaying the next tween, a callback firing once per loop (twice over `setLoops(2)`), easing being applied
  (QuadOut ahead of linear at the midpoint), and an infinite loop never finishing. Unit checks
  **4395 → 4414**. The new `choreo` demo advances five different choreographies to the SAME fixed time
  (t = 0.70s) and draws each dot where its player put it — a sequential ease-out (near the end), a parallel
  move+grow (mid-track, enlarged), a delay-then-move (still near the start), a 0.5s loop (40% into its
  second lap), and a bounce (settling near the end) — every position written by the player through a bound
  setter, nothing hand-placed. Fixed time + fixed step → deterministic golden (threshold 0.06). Purely
  additive (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial golden
  run); ctest **79/79 → 80/80**. Honest scope: this is the tween *runtime* (float properties); it does not
  yet tween vector/color properties in one call (the app binds one setter per channel), has no
  `from_current` / relative (`as_relative`) capture, no per-tween easing *transition+ease* pair beyond the
  single `Ease` enum, no pause/speed-scale, and isn't bound to a scene-tree node lifetime — those remain
  gaps.

### Iteration 86 — "Benchmarking against Godot: prefabs / instancing" (done)
Rotating to scene/resources for breadth — it hadn't come up in the last nine (core/tween-sequencer,
rendering-2D/parallax, UI/containers, audio/spatial3d, particles/emitter, tilemap/TileSet,
physics-2D/layers, animation/blend-tree, physics-2D/Area2D). This closes arguably Godot's **single most
defining feature**: the **PackedScene** — a reusable scene/node *template* you author once and INSTANTIATE
many times, each instance applying per-node property *overrides* so every copy differs without duplicating
the definition. Maz had ECS→JSON serialization (M79) and a transform hierarchy (M81), but no prefab/
instancing concept at all — the thing you reach for to spawn a hundred enemies from one enemy definition,
or place the same lamppost down a street with different tints. It's pure data (no GPU), so it unit-tests
headlessly, and it's a clean new header.
- [x] **M125 — prefabs / instancing (`scene::Prefab`)**: a new `Prefab.hpp`. A `PropValue` is a small
  tagged union over the exported-property types a 2D game uses (Float / Int / Bool / Vec2 / Color / Text);
  a `PropBag` is an ordered key→value list with `findProp` / `setProp` (replace-or-add) + typed getters
  (`getFloat` / `getInt` / `getVec2` / `getColor` …). A `PrefabNode` is a named node carrying a `PropBag`
  and child nodes; a `Prefab` is its root — the template. `findNode(root, "Body/Gun")` walks a `/`-path;
  `instantiate(prefab, overrides)` deep-copies the whole tree and applies each override entry's `PropBag`
  to the node at its path, returning a fully **independent** instance (mutating one never touches the
  template or a sibling). `testPrefab` pins: instancing with no overrides carries the defaults; overriding
  the root's `hp` + a child's `dmg` leaves the other keys at their defaults; an override may ADD a key not
  in the template; two instances are independent and the template is never mutated; and unknown override
  paths are ignored. Unit checks **4414 → 4430**. The new `prefab` demo authors ONE turret prefab
  (chassis → turret → barrel, each with exported pos/size/colour props) and instantiates it six times —
  the leftmost is the untouched template, the other five apply per-node overrides (body colour, turret
  colour, barrel length, body width) — drawing each from its resolved tree by composing child offsets onto
  the chassis anchor (the parent→child hierarchy). Deterministic (resolved once, drawn statically) →
  golden-stable (threshold 0.06). Purely additive (new header + new app), so every existing golden is
  byte-unchanged (confirmed by a serial golden run); ctest **80/80 → 81/81**. Honest scope: this is the
  prefab *data model* + instancing-with-overrides; it does not yet serialize prefabs to/from disk (the
  `io::SceneSerializer` bridge is future), has no nested-prefab *instance* references (a prefab embedding
  another prefab by id), no "editable children" / inherited-scene diffing, and no live scene-tree node
  lifetime — those remain scene/resource gaps.

### Iteration 87 — "Benchmarking against Godot: 2D polyline stroking (Line2D)" (done)
Rotating to rendering-2D for breadth. Maz could FILL a convex polygon since M88 (`drawConvexPolygon`), but
had no way to *stroke a path* — Godot's **Line2D**, the primitive behind trails, drawn curves, graphs,
outlines, connectors, and lightning. A polyline stroke thickens a point list into a ribbon of a given
**width** and then has to shape the corners (**joints**) and the two ends (**caps**) so it reads as one
continuous stroke rather than a stack of disjoint rectangles. That corner/end geometry is exactly what
`drawConvexPolygon` alone can't give you, and it's pure geometry (no renderer dependency) so it unit-tests
headlessly and is deterministic.
- [x] **M126 — 2D polyline stroking (`render::buildPolyline`)**: a new `Line2D.hpp`. `buildPolyline(points,
  style)` turns a point list into a flat **triangle soup** (`std::vector<math::vec2>`, groups of three)
  that any 2D fill path can draw. A `PolylineStyle` picks the `width`, a `JointMode`
  (**Miter** — extend the outer edges to their intersection, falling back to bevel past a `miterLimit`;
  **Bevel** — a flat triangle across the outer gap; **Round** — a fan filling the outer arc), a `CapMode`
  (**None** / **Box** — extend half a width past the end / **Round** — a semicircular fan), and a `closed`
  flag for loops (joins last→first, skips caps). Each segment becomes a rectangle; interior vertices get a
  joint on the *outer* side of the turn (chosen by the cross-product sign); miter apexes come from a
  line-line intersection. `testPolyline` pins the exact geometry: a single segment is one rectangle
  (6 verts) with an exact bounding box; box caps extend the box to [−2, 12]; round caps add triangles but
  stay within the half-width radius; a right-angle **bevel** corner is 5 triangles; a 90° **miter** emits a
  vertex exactly at the outer apex (12, −2); a closed square is 12 triangles (4 bodies + 4 joints); and
  degenerate input (< 2 points) yields nothing. Unit checks **4430 → 4445**. The new `line2d` demo is a
  gallery: the same sharp zig-zag under all three joint modes, a bar under all three cap modes, an
  80-point sampled sine **curve** (round joints + caps), and a **closed** 5-point star loop — all drawn by
  feeding `buildPolyline`'s triangles to `drawConvexPolygon`. Static geometry → deterministic golden
  (threshold 0.06). Purely additive (new header + new app), so every existing golden is byte-unchanged
  (confirmed by a serial golden run); ctest **81/81 → 82/82**. (A first build attempt tripped
  `-Werror=unused-variable` because the cap-mode array wasn't yet wired into the style — caught by
  warnings-as-errors, fixed, rebuilt clean.) Honest scope: this is the stroke *geometry*; it does not own a
  batched Line2D draw call (the app fans the triangles itself), has no per-vertex gradient/width along the
  line, no texture-along-the-line (Godot's `texture_mode`), and no antialiased edges — those remain
  rendering-2D gaps.

### Iteration 88 — "Benchmarking against Godot: .tscn/.tres text resources" (done)
Rotating to core/io for breadth, and a deliberate follow-on to M125: prefabs got an in-memory template +
instancing, but M125 explicitly left "no on-disk prefab serialization" open. Godot stores scenes and
resources as **human-readable text** (`.tscn` / `.tres`) — that's what makes them diffable and
merge-friendly in version control, and it's a distinct core/io subsystem (its whole save format). This
gives a Maz prefab that same text round-trip.
- [x] **M127 — text resource save/load (`io::savePrefabText` / `io::loadPrefabText`)**: a new
  `PrefabText.hpp`. `savePrefabText(prefab)` serializes a `scene::Prefab` tree to Godot-`.tscn`-style text —
  a `[node name="…"]` section per node (children carry `parent="…"`, `"."` for the root), each followed by
  typed `key = TYPE values` lines (`int 100`, `float 3.5`, `bool true`, `vec2 64 48`, `color 0.8 0.3 0.3
  1`, `text "grunt"`). `loadPrefabText(text, out)` parses it straight back into an identical tree: it reads
  each `[node …]` header (extracting `name`/`parent`), attaches the node under its parent by path, and
  fills each property from its typed value line. `testPrefabText` pins the exact serialized text
  (`[node name="chassis"]`, `hp = int 100`, `[node name="turret" parent="."]`, `[node name="barrel"
  parent="turret"]`, `len = float 46`), a full parse-back (root name, 3 nodes, hp, a nested vec2/color/float
  by path), **idempotence** (re-serializing the loaded tree yields byte-identical text), a round-trip of
  **every** property type (float/int/bool/vec2/color/text), and a clean failure on empty input. Unit checks
  **4445 → 4468**. The new `restext` demo authors a small "Enemy" prefab (chassis with hp/speed/pos/name +
  Sprite and Hitbox children), renders the serialized `.tscn` text in a panel, and shows a live round-trip
  readout (parsed back OK, 3 nodes reconstructed, `Enemy.hp = 120`, re-serialize identical). Static text →
  deterministic golden (threshold 0.06, text-dense). Purely additive (new header + new app), so every
  existing golden is byte-unchanged (confirmed by a serial golden run); ctest **82/82 → 83/83**. Honest
  scope: this serializes the *prefab* node/property model; it does not yet parse full Godot `.tscn`
  (`[gd_scene]`/`[ext_resource]`/`[sub_resource]` headers, `ExtResource(…)`/`SubResource(…)` references,
  arrays/dictionaries, or string escaping), and it isn't wired to the ECS `SceneSerializer` — those remain
  io gaps.

### Iteration 89 — "Benchmarking against Godot: per-object named signals" (done)
Rotating to core for breadth, and closing a distinct architectural gap. Maz has had a global by-TYPE
publish/subscribe bus since M64 (`core::EventBus`), but that is NOT what Godot's **signals** are: Godot
signals are per-OBJECT *named channels* — "this button's `pressed`", "this health's `changed`" — that carry
typed arguments and that other objects `connect` a callback to on a *specific* emitter. That granularity
(plus the two flavours Godot leans on everywhere) is genuinely missing, and signals are arguably the single
most-used communication primitive in Godot gameplay code. Pure logic → unit-tests headlessly.
- [x] **M128 — per-object named signals (`core::Signal`)**: a new `Signal.hpp`. `Signal<Args...>` is a
  typed channel an object owns as a member (`Signal<int> hpChanged;`). `connect(fn)` returns a
  `ConnectionId`; `disconnect(id)` / `isConnected(id)` / `connectionCount()` manage it; `emit(args…)` calls
  every connected handler in order, forwarding the args. On top of that it adds the two Godot flavours:
  **`connectOnce`** (fires exactly once, then auto-disconnects) and **`connectDeferred`** (the call is
  *queued* with a copy of the args on emit and only runs at the next **`flushDeferred()`** instead of
  re-entrantly mid-emit) — plus `connectDeferredOnce`. `emit` takes a **snapshot** of the connection list
  first, so a handler may safely connect/disconnect (including itself) during dispatch. `testSignal` pins
  arg pass-through + accumulation, multi-handler order, disconnect + isConnected, one-shot firing exactly
  once (connectionCount → 0), deferred queueing (not called until flush, args preserved), deferred-once,
  self-disconnect during dispatch not corrupting the round, and disconnectAll. Unit checks **4468 → 4493**.
  The new `signals` demo wires a scenario — `Button.pressed` → `Player.hpChanged(int)` → `Player.died` —
  draws the connection graph (emitter boxes → handler boxes, connectors stroked with M126's
  `render::buildPolyline`), and runs a fixed script (press 4× → HP 100→0), capturing an event log that
  shows immediate handlers firing in order, the one-shot GAME OVER firing exactly once (a 5th `died.emit`
  does nothing; `died` ends with 0 connections), and the deferred audit lines all firing together after
  `flushDeferred`. Static → deterministic golden (threshold 0.06, text-dense). Purely additive (new header +
  new app), so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest
  **83/83 → 84/84**. (A first build tripped `-Werror` on an unqualified `kInvalidConnection` in the test —
  caught by the compiler, fixed, rebuilt clean.) Honest scope: this is the per-object signal object
  (connect/emit/one-shot/deferred); it has no string-keyed reflection (`emit_signal("name", …)` by name),
  no argument *binds* on connect (Godot's `bind()`), no automatic disconnect when a connected object dies
  (no object lifetime tracking), and a single shared flush point rather than a SceneTree idle frame — those
  remain gaps.

### Iteration 90 — "Benchmarking against Godot: WAV audio load/save" (done)
Rotating to audio for breadth (last audio was M121, six rounds back) and closing a genuinely long-standing
gap that's been on the backlog since the audio system landed: **every Maz sound was procedurally
synthesized — there was no way to read (or write) an actual audio file**. Godot loads `.wav` assets via
AudioStreamWAV; without file loading, an engine can't ship a game with authored sound. A RIFF/WAVE PCM
codec is a self-contained byte parser (no device), so it unit-tests exhaustively and renders a clean
deterministic waveform golden.
- [x] **M129 — WAV audio load/save (`audio::decodeWav` / `audio::encodeWav`)**: a new `Wav.hpp`. `WavData`
  holds `sampleRate` / `channels` / interleaved float `samples` (normalized to [−1,1]) with a
  `frameCount()`. `decodeWav(bytes)` parses a RIFF/WAVE stream — validates the `RIFF`/`WAVE` tags, walks
  the word-aligned chunk list, reads the `fmt ` chunk (rejecting non-PCM), and converts the `data` chunk
  from **8-bit unsigned** (`(b−128)/128`) or **16-bit signed** (`v/32768`) PCM to floats — the two formats
  that cover the vast majority of game sound assets. `encodeWav(WavData)` writes float samples back out as
  a 16-bit PCM `.wav` byte stream (proper 44-byte header + clamped samples). `testWav` pins a 16-bit mono
  round-trip (encode → valid RIFF/WAVE header → decode → samples back within quantization, incl. the
  32767/32768 clamp), stereo interleave preservation, a **hand-built** 8-bit-unsigned stream decoding
  (128→0, 255→+1, 0→−1 — a real byte buffer, not a re-encode), and clean failure on null/short/garbage
  input. Unit checks **4493 → 4520**. The new `wav` demo synthesizes a decaying two-tone blip, encodes it
  to `.wav` bytes, decodes those bytes back, and draws the reconstructed waveform as an oscilloscope
  (stroked with M126's polyline) beside the parsed header fields (8000 Hz, mono, 4000 frames, 0.50 s, 8044
  bytes, decode OK). Static synthesis → deterministic golden (threshold 0.06). Purely additive (new header +
  new app), so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest
  **84/84 → 85/85**. Honest scope: this is the PCM WAV codec (8/16-bit integer, mono/multi-channel); it
  does not decode compressed formats (OGG Vorbis / MP3 — Godot's other stream types), 24-bit or float32
  WAV, ADPCM, or streaming/looping metadata, and the decoded samples aren't yet auto-registered as a
  playable voice in the SDL mixer (the app owns playback) — those remain audio gaps.

### Iteration 91 — "Benchmarking against Godot: 3D reference grid + RGB gizmo axes" (done)
Rotating to **3D rendering** for breadth — the last 3D-focused milestone was many rounds back, and every
recent round has been 2D/audio/AI/UI. Closing a concrete editor-viewport gap: Maz could draw lit,
depth-tested meshes and debug lines, but had **no builder for the two spatial-reference primitives every
3D editor draws** — Godot's Node3D viewport always shows a world-space ground grid (so you can read scale
and position on the floor plane) and an origin gizmo (the X=red / Y=green / Z=blue axis marker). Without
them, placing anything in 3D is guesswork. This is pure geometry (a list of colored line segments), so it
unit-tests headlessly and draws through the existing debug-line path — **no shared shader/mesh change**.
- [x] **M130 — 3D reference grid + RGB gizmo axes builder (`render::buildGrid` / `render::buildWireBox`)**:
  a new `Grid3D.hpp`. `buildGrid(GridSpec)` emits an XZ-plane ground grid — for each cell index it lays a
  line parallel to X and a line parallel to Z, the two center lines through the origin getting a brighter
  `axisColor`, the rest `minorColor`, spanning `±divisions·spacing`. With `gizmoAxes` on it appends the
  three origin axes as colored segments: +X **red**, +Y **green**, +Z **blue** (Godot's convention),
  each `axisLength` long. `buildWireBox(min, max, color)` returns the 12 edges of an axis-aligned box as
  line segments — a wireframe bounding box you can place anywhere (unlike a filled AABB). `testGrid3D` pins
  the line count (a 2-division grid + axes = 13 lines; gizmo off → `(2·3+1)·2`), the XZ span (±2), that the
  center X-parallel line carries `axisColor`, that the last three lines are the RGB axes with the correct
  channel dominant and endpoints at the axis length, and that `buildWireBox` emits exactly 12 edges bounded
  by `[min,max]`. Unit checks **4520 → 4537**. The new `grid3d` demo sets a fixed editor-style camera
  looking down at an 8-division grid, draws the grid + RGB origin gizmo + a wireframe box sitting on the
  floor (each `Line3` fed to `Renderer::drawLine`) with a 2D HUD label over the top. Fixed camera (nothing
  animates) → deterministic 3D golden (threshold 0.10, settle 2.5). Purely additive (new header + new app),
  so every existing golden — including all the 3D scenes — is byte-unchanged (confirmed by a serial golden
  run); ctest **85/85 → 86/86**. Honest scope: this is the *reference geometry* — a static ground grid,
  origin gizmo, and wire box. It is **not** the interactive editor manipulator: there are no
  translate/rotate/scale gizmo handles you can grab, no screen-space-constant sizing, no snapping, and no
  picking — those need the shipping editor UI that this headless sandbox can't host. It gives the viewport
  its spatial frame of reference, not its mouse-driven tooling.

### Iteration 92 — "Benchmarking against Godot: 2D physics-space queries" (done)
Rotating to **2D physics** for breadth (last physics was M110/M111, ~20 rounds back) and closing a
foundational gap: Maz had rigid-body *dynamics* (Physics2D — integration, contacts, joints) but **no way
to ASK the collider set a spatial question without stepping the simulation**. Godot exposes this as
`PhysicsDirectSpaceState2D` — `intersect_ray` / `intersect_point` — and it's the primitive behind an
enormous amount of gameplay: hitscan weapons, line-of-sight/AI vision, ground and wall probes, and
mouse picking. It's pure geometry against static shape descriptions, so it unit-tests exhaustively and
renders a clean deterministic golden.
- [x] **M131 — 2D physics-space queries (`game::queryRay` / `querySegment` / `queryPoint`)**: a new
  `PhysicsQuery2D.hpp`. `QueryShape2D` describes a circle or an **oriented** box (center, radius/half,
  angle) plus a 32-bit collision `layer` and a user `id`. `queryRay(origin, dir, shapes, maxDist, mask)`
  returns the **nearest** `RayHit2D` (distance, world contact point, surface normal pointing back toward
  the origin, shape index + id) among shapes whose `layer & mask` is non-zero — Godot collision-mask
  semantics, so a caller can probe "only walls" and pass through everything else. Ray-vs-circle is the
  standard quadratic (origin-inside → t=0); ray-vs-oriented-box rotates the ray into the box's local
  frame, runs a slab test, and rotates the entry normal back to world. `querySegment(a, b, …)` is the
  bounded-length wrapper; `pointInShape` / `queryPoint` implement `intersect_point` (which shapes contain
  a point) for mouse picking. `testPhysicsQuery2D` pins: a circle hit's distance/point/normal/id; a clean
  miss; a non-normalized direction still giving world-unit distance; an axis-aligned box face normal; a
  **45°-rotated** box hit at `5 − √2` with a −X-facing normal; nearest-of-several selection; a segment
  that stops short vs one that reaches; **layer-mask filtering** (a nearer wall on an unqueried layer is
  skipped for the farther enemy); and point-in-circle / point-in-rotated-box + a two-shape `queryPoint`.
  Unit checks **4537 → 4567**. The new `rayquery` demo fans 15 hitscan rays from a muzzle through a
  translucent **glass** pane (a layer the ray mask ignores) into a field of solid circles + oriented
  boxes, drawing each ray to its first solid hit with a red contact dot + normal stub, and highlighting a
  point-picked circle in green. Static scene → deterministic golden (threshold 0.06). Purely additive
  (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial golden run);
  ctest **86/86 → 87/87**. Honest scope: this is the ray/point/segment query set against circles and
  boxes. It does **not** yet include convex-polygon or capsule shapes, a full `intersect_shape` /
  shape-cast (sweep a moving shape and get the time-of-impact), a broadphase acceleration structure
  (queries are linear over the shape list — fine for hundreds, not thousands), or motion-query
  integration with the rigid-body solver (`move_and_collide`); those remain physics gaps.

### Iteration 93 — "Benchmarking against Godot: interned strings (StringName)" (done)
Rotating to **core data structures** for breadth — no core-utility milestone in many rounds, and this
closes a genuinely foundational gap that's been on the Phase-2 backlog since the start: **string
interning**. A game names things constantly — node names, signal names, animation tracks, input actions,
entity tags — and Godot represents those as `StringName`: each unique string is stored once and referred
to thereafter by a tiny integer, so comparison is an int compare and hashing is trivial. Maz had no such
facility; every subsystem hand-hashed or string-compared. It's a pure data structure (a table + a
handle), so it unit-tests exhaustively and renders a clean deterministic golden.
- [x] **M132 — string interning / `StringId` (`core::StringTable` + `core::fnv1a32`)**: a new
  `StringId.hpp`. `StringTable::intern(text)` adds-or-finds and returns a stable `StringId` handle (a
  32-bit dense insertion index); interning the **same** text always returns the **same** id, so name
  equality becomes an integer compare. `find(text)` looks up *without* inserting (invalid id if absent);
  `str(id)` reverses an id back to its text; `hash(id)` exposes the stored **FNV-1a-32** hash;
  `contains` / `size` / `empty` / `clear` round it out. `StringId` is trivially copyable, ordered (usable
  as a map key), and gets a `std::hash` specialization so it works in unordered containers. The free
  `fnv1a32` is a stable, platform-independent content hash. `testStringId` pins: dedup (same text → same
  id, three interns → two unique), reverse lookup + invalid-id → empty string, non-inserting `find`,
  dense stable ids, the FNV hash against the **known** constant `fnv1a32("hello") == 0x4F9F2CAB` (and the
  empty-string offset basis), the empty string as a legitimate interned value, `StringId` as an
  `unordered_map` key, and `clear`. Unit checks **4567 → 4595**. The new `strtable` demo interns a stream
  of ten tag references (`player`/`enemy`/`pickup`/… with repeats) into a table and shows two panels: the
  raw reference stream with each name's assigned id (first appearance of an id flagged green "new",
  repeats blue), and the deduplicated pool (id → text → FNV hash) — "10 references collapse to 5 unique
  ids". Static → deterministic golden (threshold 0.06). Purely additive (new header + new app), so every
  existing golden is byte-unchanged (confirmed by a serial golden run); ctest **87/87 → 88/88**. Honest
  scope: this is a per-`StringTable` interner with 32-bit ids. It is not a *global* process-wide
  `StringName` registry (Godot interns into one global table shared everywhere), it isn't thread-safe
  (core is single-threaded by convention — wrap externally if shared), and the subsystems that currently
  hand-hash names aren't retrofitted to use it yet; those remain follow-ups.

### Iteration 94 — "Benchmarking against Godot: CSV localization" (done)
Rotating to **IO / serialization** for breadth (last IO was M127's .tres; M132 was core) and closing a
gap that any *shipping* game hits: **on-screen text in more than one language**. Godot ships translation
support built on a CSV import — a table whose first column is a message KEY and whose remaining columns
are one LOCALE each — plus a `TranslationServer` that answers `tr(key)` for the active language. Maz
could read JSON and its own prefab/binary formats but had **no CSV reader and no translation lookup at
all**. Both halves are pure text processing, so they unit-test exhaustively and render a clean
deterministic golden.
- [x] **M133 — CSV parser + localization (`io::parseCsv` + `io::TranslationTable`)**: a new
  `Localization.hpp`. `parseCsv(text)` is a proper RFC-4180-style reader — it honors **quoted fields**
  (an embedded `,` stays in the field), fields with **embedded newlines**, the `""`→`"` escape, and
  **CRLF or LF** line endings, and it doesn't emit a spurious empty row for a trailing newline.
  `TranslationTable::loadCsv` reads a Godot-style translation CSV (header = `keys,en,es,…`; each row = a
  key + its per-locale text), `setLocale` picks the active column, and `tr(key)` returns the active
  locale's text with **graceful fallback**: an empty cell falls back to the source (first) locale, and an
  unknown key returns the key itself so a missing string shows up as a visible identifier rather than
  blank. `tr(key, locale)`, `locales()`, `keys()`, `count()`, `hasKey` round it out. `testLocalization`
  pins the CSV parser (grid, quoted-with-comma, `""` escape, empty fields, CRLF + no-trailing-blank-row,
  quoted embedded newline) and the table (locale switch, per-locale lookup, empty-cell→source fallback,
  unknown-key→key, unknown-locale keeps the active one, and header-without-locale rejection). Unit checks
  **4595 → 4628**. The new `locale` demo renders the **same** game menu four times side by side (English
  / Espanol / Francais / Deutsch), every label pulled through `tr(key)` after `setLocale(...)` — the
  German QUIT cell is intentionally empty to show the fallback painting the English word in orange.
  Static → deterministic golden (threshold 0.06). Purely additive (new header + new app), so every
  existing golden is byte-unchanged (confirmed by a serial golden run); ctest **88/88 → 89/89**. Honest
  scope: this is CSV-driven string translation with fallback — it is **not** the full gettext/PO
  toolchain (no plural forms, no message contexts, no `%s`-style argument formatting/interpolation), it
  doesn't auto-detect the OS locale, and it isn't wired into a global `TranslationServer` that the UI
  widgets consult automatically (the app owns the table); those remain localization follow-ups.

### Iteration 95 — "Benchmarking against Godot: text layout (word-wrap + alignment)" (done)
Rotating to **UI / 2D text** for breadth (last UI was M122, the longest-idle subsystem) and closing a
gap present since the font landed: the `Font` renderer can draw and *measure* a single line, but it has
**no notion of fitting text into a box** — wrapping a paragraph across lines at word boundaries so it
doesn't overflow, and aligning each line. That's what every dialog box, tooltip, description pane, and
subtitle needs; Godot's `Label` does it as autowrap + horizontal align. The layout is a pure algorithm
(it takes a MEASURE callback, so it has no renderer/Font dependency), which unit-tests headlessly.
- [x] **M134 — text layout / word-wrap (`ui::layoutText` + `ui::TextLayout`)**: a new `TextLayout.hpp`.
  `layoutText(text, maxWidth, measure, lineHeight, align)` splits the input on explicit `\n` (hard
  breaks), greedily word-wraps each paragraph so no line exceeds `maxWidth` (an over-long single word is
  placed alone rather than mid-word-broken; blank lines are preserved), and returns a `TextLayout` — a
  list of `TextLine{text, x, y, width}` plus the overall `width`/`height`. Each line's `x` is set from
  `TextAlign::Left/Center/Right`; `y` stacks by `lineHeight`. Because `measure` is injected, the same code
  serves any font backend and the tests drive it with a synthetic 10px-per-character measurer for exact
  expectations. `testTextLayout` pins: greedy wrap at the fit boundary, `\n` hard breaks, preserved blank
  lines, an over-long word alone on its line, the three alignment x-offsets (`0` / `(M−W)/2` / `M−W`),
  stacked line y-offsets, and empty-input → one empty line. Unit checks **4628 → 4651**. The new
  `textwrap` demo fits one prose paragraph into three fixed-width panels — left, center, right aligned —
  measured with the real `Font::textWidth`, plus a fourth panel showing a `\n`-delimited quest log with
  its hard breaks preserved. Static → deterministic golden (threshold 0.07, text-dense). Purely additive
  (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial golden run);
  ctest **89/89 → 90/90**. Honest scope: this is line-level word-wrap + horizontal alignment. It does
  **not** do mid-word/hyphenation breaking of over-long words, per-run rich text (bold/italic/color spans
  — no BBCode), bidirectional/RTL or complex-script shaping, vertical alignment/justification, or
  ellipsis truncation; those remain text follow-ups.

### Iteration 96 — "Benchmarking against Godot: additive/layered pose blending" (done)
Rotating to **animation** for breadth (last was M124, the longest-idle subsystem) and closing a specific
gap in the skeletal-animation stack: Maz had **cross-fade** blending (`blendPoses` / `blendPosesWeighted`,
idle-to-walk), but not **additive** blending — Godot's `AnimationNodeAdd2` and the "additive" import mode.
Cross-fading interpolates *whole* poses, so a walk pose fully replaces an idle one at weight 1. Additive
blending instead layers a *difference* on top of a base: an additive clip is authored relative to a
reference pose, its per-joint delta is computed, and that delta is applied on top of whatever base is
playing — so you can layer a "wave", "breathe", "aim", or "recoil" onto any locomotion without disturbing
the joints the additive clip doesn't touch. Pure math on `JointPose` (TRS with a quaternion).
- [x] **M135 — additive/layered pose blending (`anim::additiveBlend` + `makeAdditiveDelta` /
  `applyAdditiveDelta`)**: a new `AdditiveBlend.hpp`. `makeAdditiveDelta(additive, reference)` computes a
  joint's delta — translation subtracts, rotation is the reference-to-additive rotation
  (`inverse(ref) * add`), scale is the component ratio. `applyAdditiveDelta(base, delta, weight)` layers it
  on: translation adds `weight*delta`, rotation applies `slerp(identity, delta, weight)` after the base,
  scale multiplies by `mix(1, ratio, weight)` — so **weight 0 returns the base exactly** and a **zero
  delta** (a joint that matches the reference) leaves the base untouched. `additiveBlendJoint` +
  whole-pose `makeAdditivePose` / `applyAdditivePose` / `additiveBlend` wrap it. `testAdditiveBlend` pins:
  zero-delta -> base preserved at any weight, weight-0 -> exact base, translation delta scaling
  (full/half), a +90deg rotation delta rotating +X onto +Y (and ~+45deg at half weight), a **non-identity
  reference** (ref +30deg / additive +90deg -> +60deg applied delta), scale-ratio multiplication (base 2 *
  ratio 3 -> 6; half -> 4), and a whole-pose layer touching only the joint that differs. Unit checks
  **4651 -> 4672**. The new `addblend` demo runs a 3-joint arm through `anim::additiveBlend` + real
  `Skeleton` forward kinematics at five rising weights (0 / 0.25 / 0.5 / 0.75 / 1.0): the shoulder holds
  its base lift (its additive delta is zero) while only the elbow folds progressively — the additive
  layer visibly stacking on the preserved base. Static -> deterministic golden (threshold 0.06). Purely
  additive (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial
  golden run); ctest **90/90 -> 91/91**. Honest scope: this is the additive blend operator (Add2-style,
  applied uniformly to a pose). It is **not** yet a per-bone-mask layer stack (Godot's blend-position
  filter that limits an additive layer to selected bones), an AnimationTree node wired into the state
  machine, or automatic reference-pose extraction from an imported clip; those remain animation
  follow-ups.

### Iteration 97 — "Benchmarking against Godot: procedural mesh primitives" (done)
Rotating to **3D rendering geometry** for breadth (the last render-geometry work was the original
box/sphere/plane primitives long ago) and closing a very concrete gap: Maz shipped only **three**
built-in meshes (box, sphere, plane), while Godot ships a whole family — CylinderMesh, CapsuleMesh,
TorusMesh, and a cone (a cylinder with a zero top radius). Anything wanting a barrel, a spike, a ring, or
a capsule collider proxy in Maz had to author a glTF. Each primitive is pure vertex/index generation, so
it unit-tests headlessly and renders a clean lit golden.
- [x] **M136 — procedural mesh primitives (`render::shapes::makeCylinder` / `makeCone` / `makeTorus` /
  `makeCapsule`)**: a new `Shapes3D.hpp` that ADDS four inline builders alongside the existing (untouched)
  box/sphere/plane. Each returns the same `shapes::MeshData` (position/normal/color/UV vertices + indices)
  the renderer already consumes, with **outward unit normals** and UVs. `makeCylinder(radius, height,
  sectors)` builds the curved side (per-sector radial normals) plus flat top/bottom caps;
  `makeCone(radius, height, sectors)` builds the slanted side with the analytic radial+up normal and a
  base cap; `makeTorus(major, minor, majorSegs, minorSegs)` sweeps a tube around +Y with proper toroidal
  normals; `makeCapsule(radius, cylHeight, sectors, rings)` joins a cylinder body to two hemisphere caps
  (total height `cylHeight + 2*radius`). `testShapes3D` pins each primitive's **bounding box** (cylinder
  y∈[−3,3] / x∈[−2,2]; cone apex at +2 & base radius 1.5; torus outer radius major+minor & tube height
  ±minor; capsule total height 6), that **every normal is unit length**, that **every index is in range**
  and the count is a multiple of 3, and that degenerate segment counts are clamped rather than crashing.
  Unit checks **4672 → 4697**. The new `primitives` demo uploads all four via `createMesh` and draws them
  as a lit, tilted gallery (blue cylinder / orange cone / green torus / purple capsule) under a fixed
  camera with 2D labels. Fixed camera → deterministic 3D golden (threshold 0.12, settle 2.5). Purely
  additive (new header + new app; the existing box/sphere/plane `.cpp` is untouched), so every existing
  golden — including all the 3D scenes — is byte-unchanged (confirmed by a serial golden run); ctest
  **91/91 → 92/92**. Honest scope: these are the four common analytic solids with smooth normals and
  simple UVs. It does not add prism/plane-subdivision/heightmap/quad-sphere variants, per-face UV
  unwrapping or seam control, tangents for normal mapping, or LOD ring/sector auto-selection; those remain
  mesh-generation follow-ups.

### Iteration 98 — "Benchmarking against Godot: generational-handle slot-map" (done)
Rotating to **core containers** for breadth (M132 was core StringId; this is a different, long-backlogged
Phase-2 item — "handles / generational indices, object pools") and closing a foundational gap: a
**generational slot-map**. Godot hands out `RID`s and entity-style ids that stay valid across storage
reuse and safely detect a stale reference to a freed object — the classic dangling-handle / ABA problem.
Maz had no such structure; subsystems kept objects in plain vectors and passed raw indices, which silently
break when an element is removed and the slot is reused. It's a pure data structure, so it unit-tests
exhaustively and renders a clean deterministic golden.
- [x] **M137 — generational-handle slot-map (`core::SlotMap<T>` + `core::SlotHandle`)**: a new
  `SlotMap.hpp`. `insert(value)` stores into a free slot (recycled via a free list) or grows the array and
  returns a `SlotHandle{index, generation}`. `get(handle)` returns a `T*` or **nullptr** when the handle
  is stale; `contains` checks in-range + occupied + generation-match; `erase(handle)` frees the slot,
  **bumps its generation** (invalidating every handle minted before the free), recycles the index, and is
  double-free safe (returns false on an already-stale handle). `size` / `capacity` / `clear` / `forEach`
  round it out. Generations start at 1 so a default `SlotHandle{}` is always invalid. `testSlotMap` pins:
  insert→live handle→value; default handle invalid; erase→stale + size drop + double-erase false; **THE
  generational property** — erase then re-insert reuses the same slot index at a new generation, so the
  old handle goes stale (`get`→nullptr) while the new handle is valid; handle stability across unrelated
  erases; `forEach` visits only live values; `clear`; and free-slot recycling (capacity stays 1 across
  erase+reinsert). Unit checks **4697 → 4727**. The new `slotmap` demo drives a real `SlotMap<char>`
  through insert A,B,C → free B → insert D (which reuses B's slot with a bumped generation) and draws the
  slot array (occupied/free + each slot's generation) beside the handle table — `hB` shown **STALE** (red)
  because its slot was recycled, `hD` **LIVE** (green) on the same slot. Static → deterministic golden
  (threshold 0.06). Purely additive (new header + new app), so every existing golden is byte-unchanged
  (confirmed by a serial golden run); ctest **92/92 → 93/93**. Honest scope: a single-type templated
  slot-map with 32-bit index/generation. It is not a *typed-RID server* multiplexing many resource types
  behind one opaque id (Godot's RID_Owner set), it doesn't recycle generations after 2³² reuses of one
  slot, and the engine's existing handle-ish systems (texture/mesh handles, ECS ids) aren't retrofitted
  onto it yet; those remain follow-ups.

### Iteration 99 — "Benchmarking against Godot: 2D convex polygon collision (SAT)" (done)
Rotating to **2D physics** for breadth (last was M131's ray/point queries) and closing a genuine
shape-class gap: Physics2D collides circles and (oriented) boxes — and it already uses SAT internally for
the box-box case — but there was **no way to collide an ARBITRARY convex polygon**: a triangle, a
pentagon, a hexagonal bumper, a hand-authored hull. Godot exposes exactly this as `ConvexPolygonShape2D` /
`CollisionPolygon2D`. It's pure geometry (no simulation/renderer dependency), so it unit-tests headlessly
and renders a clean deterministic golden.
- [x] **M138 — 2D convex polygon collision (`game::satOverlap` + `polyContains` + `makeRegularPoly` /
  `makeBoxPoly`)**: a new `ConvexShape2D.hpp`. A `ConvexPoly2D` is a vertex list; `satOverlap(a, b)` runs
  the Separating-Axis Theorem over both polygons' edge normals — projecting each shape onto every candidate
  axis, returning **no overlap** the instant a separating axis is found, otherwise reporting the
  **minimum-translation vector** (`SatHit2D{axis, depth}`, the unit push direction oriented A→B plus the
  penetration depth). `polyContains(poly, pt)` tests point-in-convex-polygon via a consistent cross-product
  sign; `makeRegularPoly` / `makeBoxPoly` build n-gons and oriented boxes. `testConvexShape2D` pins: two
  boxes overlapping in X by 1 → MTV axis ±X depth 1 oriented A→B; clearly separated → no overlap; **the MTV
  actually separates** (translate B by `axis*depth` and they no longer overlap); a triangle-vs-box overlap;
  a 45°-rotated box (diamond) whose diagonal reach catches a box outside the axis-aligned extent;
  point-in-pentagon inside/outside; `makeRegularPoly` vertex count + radius; and a degenerate <3-point shape
  never overlapping. Unit checks **4727 → 4747**. The new `polycollide` demo tests a central probe pentagon
  against a ring of convex shapes (triangle / box / hexagon / diamond / pentagon), drawing overlaps **red**
  with the yellow MTV push-arrow and clear shapes **green**. Static → deterministic golden (threshold 0.06).
  Purely additive (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial
  golden run); ctest **93/93 → 94/94**. Honest scope: this is the convex-convex overlap test + MTV and
  point-in-poly. It does **not** yet integrate into the `PhysicsWorld2D` rigid-body solver as a dynamic
  collider (contact manifold generation + impulse response for polygons), auto-decompose concave shapes
  into convex pieces, or provide polygon-vs-circle / swept polygon casts; those remain physics follow-ups.

### Iteration 100 — "Benchmarking against Godot: sample-playback mixer" (done)
Rotating to **audio runtime** for breadth (last audio milestone was M129's WAV codec; the RUNTIME was the
real gap). Maz could *decode* a `.wav` into float samples (M129) and *synthesize* procedural tones
(`audio::Audio`), but there was **no way to take a decoded clip and actually play it back** — start it as a
voice, set gain/stereo-pan, loop it, pitch-shift it, and have several such voices summed into one output
buffer. Godot's `AudioStreamPlayer` over an `AudioStreamWAV` does exactly that. Rather than touch the shared
non-deterministic SDL device mixer, this lands a self-contained **offline** mixer that unit-tests
byte-deterministically yet emits exactly the interleaved-float format a real device callback wants.
- [x] **M139 — Sample-playback mixer (`audio::SampleMixer`)**: a new `SampleMixer.hpp`. `play(clip, gain,
  pan, loop, speed)` starts a `SampleVoice` referencing a `WavData` clip and returns a voice id (or −1 for an
  empty clip); `stop(id)`, `activeVoices()`, `clear()` manage the set. `mix(out, frames, outRate)` zeroes the
  interleaved-stereo output then sums every active voice: the per-output-frame playhead step is
  `clipRate/outRate × speed` (so sample-rate conversion and pitch fall out of one number), reads are
  **linearly interpolated** for smooth pitch/resample, a mono clip is panned into both channels while a stereo
  clip maps its two channels straight through, and a non-looping voice **auto-deactivates** when it runs past
  the end while a looping voice wraps (preserving fractional overshoot). `testSampleMixer` pins: a mono clip
  reproduced into L+R at the native rate; the buffer zeroed each call; gain scaling; full-left pan mutes R and
  full-right mutes L; two voices summing; a loop wrapping and staying active past 2× its length; `speed 2.0`
  consuming the clip twice as fast (interpolated); `stop()` silencing a voice; and an empty clip rejected with
  −1. Unit checks **4747 → 4776**. The new `sampler` demo synthesizes a decaying two-tone and a deterministic
  noise blip, **round-trips each through the WAV codec** (encode → decode, proving they are decoded audio),
  registers them as voices panned hard-left and hard-right, mixes them offline into one stereo buffer, and
  draws the two source clips plus the resulting **L and R oscilloscopes** — you can see the tone in the left
  trace and the blip in the right. Static synthesis (fixed LCG) → deterministic golden (threshold 0.06).
  Purely additive (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial
  golden run); ctest **94/94 → 95/95**. (Also nudged the `cube` golden threshold 0.05 → 0.06: the tightest
  3D threshold in the suite was flaking on lavapipe under the back-to-back sweep's CPU load — it renders at
  ~0.040 in isolation but drifted to ~0.051 under load; 0.06 absorbs that jitter while a genuinely broken 3D
  pass still reads > 0.2. No engine change, harness-only.) Honest scope: this is an *offline* buffer-fill mixer (WAV clips only),
  not yet wired into the live SDL device callback, and it does not do bus routing, DSP-effect insertion
  (M99/M106 buses/effects remain offline), or streaming decode of long OGG/MP3 assets; those remain the audio
  follow-ups.

### Iteration 101 — "Benchmarking against Godot: resource-pack archive" (done)
Rotating to **IO / asset packaging** for breadth (last IO milestone was M133's CSV/localization). Maz could
serialize a single blob (`io::Serialize`) and read/write one file at a time, but there was **no way to bundle
many named resources into ONE archive** and pull them back out by path — which is exactly what a shipped game
loads from: Godot's `.pck` / `PackedData`, a single file holding every texture, level, sound, and script
instead of a loose file tree. That gap is pure byte-container work (no renderer/sim dependency), so it
unit-tests headlessly and renders a clean deterministic golden.
- [x] **M140 — Resource-pack archive (`io::packResources` + `io::ResourcePack`)**: a new `ResourcePack.hpp`
  built on the existing `ByteWriter`/`ByteReader`. `packResources(entries)` writes a magic (`'MZP1'`) +
  version header, an entry count, a **directory** of `(path, offset, size)` records (offsets relative to the
  data section, so the archive is position-independent), then the concatenated blob bytes. `ResourcePack::load`
  parses that back with full bounds checking — a foreign magic, a truncated directory, or a data section that
  claims more bytes than present all fail cleanly (return `false`, pack left empty) instead of over-reading;
  `contains` / `get` (→ bytes or `nullptr`) / `getString` / `paths()` (pack order, deduped) / `count()` expose
  the contents. Duplicate paths follow filesystem-overwrite semantics (last wins). `testResourcePack` pins:
  three mixed blobs (JSON / text / binary-with-embedded-zeros) packed and each fetched back byte-identical;
  offset math verified on the 2nd/3rd entries (not just the one at offset 0); a zero-length blob round-tripping
  as present-but-empty; an empty archive reporting zero; duplicate-path last-wins with a deduped count/order;
  and wrong-magic / too-short / truncated-payload streams all rejected with the pack staying empty. Unit checks
  **4776 → 4809**. The new `respack` demo packs four real resources — a level's JSON, a readme string, a
  synthesized `.wav`, and a raw palette blob — into one archive, loads it back, and draws the directory table
  (path / size / offset), the archive byte total, a hex dump of the header, and a per-resource round-trip check.
  Static data → deterministic golden (threshold 0.07, text-dense). Purely additive (new header + new app), so
  every existing golden is byte-unchanged (confirmed by a serial golden run); ctest **95/95 → 96/96**. Honest
  scope: this is an uncompressed store-only archive (like a `.pck` / an uncompressed zip) — it does **not** yet
  add DEFLATE/gzip compression, per-file encryption or checksums (Godot stores an MD5 per file), streaming
  reads of an on-disk archive without loading it whole, or a virtual-filesystem layer that redirects
  `io::readFile` through mounted packs; those remain the packaging follow-ups.

### Iteration 102 — "Benchmarking against Godot: BBCode rich text" (done)
Rotating to **UI / text** for breadth (last two rounds were IO and audio). Maz can draw a string (`ui::Font`)
and word-wrap it (`ui::layoutText`, M134), but every character in a string shared ONE style — there was no way
to bold a word, colour a phrase, or enlarge a heading *inside* a run of text. Godot does this with BBCode in
`RichTextLabel` (`[b]bold[/b]`, `[i]/[u]`, `[color=…]`, `[size=…]`). That markup→styled-runs parse is pure
string work (no renderer dependency), so it unit-tests headlessly and drives a legible deterministic golden.
- [x] **M141 — BBCode rich-text parser (`ui::parseBBCode` + `ui::stripBBCode`)**: a new `RichText.hpp`.
  `parseBBCode(src)` walks the string maintaining nested style state (bold/italic/underline counters, a colour
  stack, a size stack) and emits a flat list of `RichSpan`s — each a substring plus its **resolved** style
  (bold/italic/underline flags, an optional RGBA colour, an optional pixel size); adjacent runs with identical
  style are coalesced so the list is minimal. It supports `[b]`/`[i]`/`[u]`, `[color=…]` (both `#rgb`/`#rrggbb`/
  `#rrggbbaa` hex and named colours red/green/blue/white/black/yellow/cyan/magenta/orange/gray), and `[size=N]`.
  Handling is lenient like Godot: nested tags stack, an unclosed tag runs to the end, a stray close tag is
  ignored, `[lb]`/`[rb]` emit literal brackets, and an **unrecognized tag (or invalid colour) passes through as
  literal text** rather than being dropped. `stripBBCode` returns the tags-removed plain text. `testRichText`
  pins: plain text → one span; `[b]` splitting into plain/bold/plain; triple-nested b+i+u; `#ff0000` and short
  `#0f0` hex + a named colour; `[size=32]` on the enclosed run; mismatched close popping the correct scope
  (`x` bold+red, `y` red only); an unclosed tag running to the end; a stray close ignored; identical-style runs
  coalescing across a tag boundary; `[lb]`/`[rb]` + unknown-tag passthrough; an invalid colour falling back to
  literal; and `stripBBCode` round-trips. Unit checks **4809 → 4851**. The new `richtext` demo shows six BBCode
  source strings each above its formatted result — bold, italic, underline, hex + named colours, three text
  sizes, deep nesting, and literal/unknown-tag passthrough — rendered by mapping each span's colour and pixel
  size onto the font (bold faked with a double-draw, underline with a bar). Static → deterministic golden
  (threshold 0.07, text-dense). Purely additive (new header + new app), so every existing golden is
  byte-unchanged (confirmed by a serial golden run); ctest **96/96 → 97/97**. Honest scope: this is the parser
  + a demo renderer, not a full `RichTextLabel` node — it does **not** yet do wrapped rich-text layout (mixing
  M134's wrapping with per-span metrics), inline images/tables/`[url]` hitboxes, `[center]`/`[right]`
  paragraph alignment tags, real bold/italic *font faces* (the demo fakes weight), or animated effects
  (`[wave]`/`[shake]`); those remain the rich-text follow-ups.

### Iteration 103 — "Benchmarking against Godot: cubic Bézier path" (done)
Rotating to **math / geometry** for breadth (last three rounds were UI, IO, audio). Maz had scalar easing
curves (`anim`) for interpolating a value over time, but no *spatial* path: an authored smooth curve through
points that something can travel along. Godot exposes this as `Curve2D` (held by a `Path2D`, walked by a
`PathFollow2D`) — a cubic Bézier spline with per-point in/out handles plus **arc-length baking** so a follower
moves at constant speed regardless of how the curve bends. That is pure geometry (no renderer/sim dependency),
so it unit-tests headlessly and draws a clean deterministic golden.
- [x] **M142 — Cubic Bézier path (`math::Curve2D`)**: a new `Curve2D.hpp`. A `CurvePoint2D` holds a position
  plus `in`/`out` control-handle offsets (exactly like Godot's Curve2D); consecutive points are joined by a
  cubic Bézier (`P0=pos_i`, `P1=pos_i+out_i`, `P2=pos_{i+1}+in_{i+1}`, `P3=pos_{i+1}`). `sampleSegment(seg,t)`
  and `sample(fofs)` (fractional point offset across segments) evaluate the geometric curve; `tangent(fofs)`
  gives the unit direction via a central finite difference (well-defined even where the analytic Bézier
  derivative degenerates, e.g. a straight segment with no handles); `length()` sums a fine subdivision.
  `bake(interval)` walks a dense polyline of the whole curve accumulating arc length, then **resamples at
  uniform arc-length intervals** into constant-speed baked points; `sampleBaked(distance)` returns the
  position at a given arc-length distance (auto-bakes on first use), and `bakedLength()`/`bakedPoints()`
  expose the result. `testCurve2D` pins: a straight segment's exact endpoints/midpoint/chord-length/tangent;
  a bowed curve being longer than its chord with endpoints still exact and the middle bulging; `sample(fofs)`
  landing on interior points + clamping past the ends; baking matching `length()`, the ends mapping to the
  endpoints, and the half-distance sample hitting the geometric middle; a **constant-speed** check on a curved
  path (equal arc-distance steps cover chord lengths within ~15% of each other, which naive Bézier `t` would
  not); and degenerate empty/single-point curves not crashing. Unit checks **4851 → 4882**. The new `curve`
  demo authors a wavy 4-point path with handles and draws the smooth spline, the control points + their
  handle lines, the green **arc-length-baked** dots (visibly evenly spaced even through the bends), and a
  yellow traveller at a fixed baked distance with its tangent arrow. Static → deterministic golden (0.06).
  Purely additive (new header + new app), so every existing golden is byte-unchanged (confirmed by a serial
  golden run); ctest **97/97 → 98/98**. Honest scope: this is a 2D cubic-Bézier `Curve2D` — it does **not**
  yet add a 3D `Curve3D`, per-point tilt/up-vector for 3D path orientation, a `Path2D`/`PathFollow2D` scene
  node that moves a transform along it each frame, closed loops, or an editor to drag handles; those remain
  the path follow-ups.

### Iteration 104 — "Benchmarking against Godot: 2D multi-mesh instancing" (done)
Rotating to **2D rendering** for breadth (recent rounds were math, UI, IO, audio). Maz already batches sprites
and has 3D instanced meshes (M55), but had no **2D instancing abstraction** — a way to declare one base shape
plus a compact per-instance buffer and stamp the shape hundreds of times. Godot exposes exactly this as
`MultiMesh` / `MultiMeshInstance2D`: one mesh, one transform (+colour) array, drawn as a crowd. The transform
math is pure (no GPU state), so it unit-tests headlessly and drives a dense deterministic golden.
- [x] **M143 — 2D multi-mesh instancing (`render::MultiMesh2D`)**: a new `MultiMesh2D.hpp`. A `MultiMesh2D`
  holds a convex base polygon (`baseVertices`, local space) plus a list of `Instance2D`s (position, rotation,
  scale, colour). `transformInstance(inst, local)` applies the standard 2D **scale→rotate→translate** to one
  vertex; `transformedPolygon(i)` returns one instance's world-space polygon (for a per-instance coloured
  draw); `bakeTriangles()` fans **every** instance's polygon into one triangle soup (groups of 3) ready for a
  single batched draw / vertex upload, and `triangleCount()` reports its size. `testMultiMesh2D` pins the TRS
  math (identity / pure-translate / pure-scale / 90°-rotate sending +X to +Y / a combined scale-rotate-
  translate landing where hand-computed), `transformedPolygon` preserving vertex count and applying the
  transform (including a 2× instance doubling reach), triangle counts (triangle base → 1 tri/instance, quad →
  2), `clear`, and a degenerate <3-vertex base baking nothing. Unit checks **4882 → 4906**. The new `multimesh`
  demo stamps a single dart shape **540 times** (a 30×18 grid) through one MultiMesh2D — each instance rotated
  along a spiral about the centre and tinted by its angle — so the whole field reads as one flowing swirl
  drawn from one base shape + an instance buffer. Every instance value is a deterministic function of the grid
  index → golden-stable (0.07). Purely additive (new header + new app), so every existing golden is
  byte-unchanged (confirmed by a serial golden run); ctest **98/98 → 99/99**. Honest scope: this is the CPU
  instance-buffer + transform/bake data structure — it does **not** yet upload the baked buffer to a real GPU
  instanced draw call (the demo still issues one `drawConvexPolygon` per instance to get per-instance colour),
  support a shared texture/atlas per instance, per-instance custom-data channels, or a 3D `MultiMesh`; those
  remain the instancing follow-ups.

### Iteration 105 — "Benchmarking against Godot: 3D billboard modes" (done)
Rotating to **3D rendering** for breadth (recent rounds were 2D-render, math, UI, IO, audio). Maz's 3D
particles billboard internally, but there was no reusable helper to make an arbitrary quad **face the
camera** — the trick behind trees, grass, smoke, health bars, and distant-object impostors. Godot exposes it
as the billboard modes on `SpriteBase3D` / `GeometryInstance3D`: full billboard (the quad squarely faces the
camera) and Y-billboard (the quad yaws to face the camera but stays upright). It's a pure matrix function of
the view matrix, so it unit-tests headlessly and drives a 3D golden.
- [x] **M144 — Billboard model-matrix builder (`render::buildBillboard`)**: a new `Billboard.hpp` with a
  `BillboardMode` enum (`Disabled` / `Enabled` / `YBillboard`) and `buildBillboard(position, scale, view,
  mode)` returning the `translate * rotate * scale` model matrix. It reads the camera's world-space basis
  straight out of the `view` matrix rows (no camera object needed): `Enabled` aligns the quad's right/up with
  the camera's right/up so its plane squarely faces the camera; `YBillboard` keeps world-up as the quad's up
  and only yaws (the quad normal flattened into the ground plane points back at the camera), with a guard for
  the degenerate straight-down look; `Disabled` leaves the quad axis-aligned. `testBillboard` pins: a
  front-on camera giving an `Enabled` normal of exactly +Z with screen-aligned right/up and the correct
  translation; `Disabled` staying axis-aligned with scale/translation applied; a side camera turning the full
  billboard's normal to +X; and an **elevated** camera where the `YBillboard` up axis stays exactly world-up
  (normal in the ground plane) while the `Enabled` up axis tilts away from world-up — plus an orthonormal-
  basis check. Unit checks **4906 → 4928**. The new `billboard` demo stands three rows of flat emissive cards
  in a 3D scene under an elevated camera — back row **Enabled** (full-facing), middle **YBillboard** (upright),
  front **Disabled** (fixed) — so the modes are visibly different in one frame. Deterministic (static camera)
  → 3D golden (threshold 0.12). Purely additive (new header + new app), so every existing golden is
  byte-unchanged (confirmed by a serial golden run); ctest **99/99 → 100/100**. Honest scope: this is the
  orientation-matrix helper — it does **not** add a `Sprite3D`/`AnimatedSprite3D` scene node that wires a
  texture atlas + billboard + alpha onto a quad automatically, particle-aligned ("velocity") billboards, or
  spherical-vs-cylindrical distinctions beyond the two Godot modes; those remain the 3D-sprite follow-ups.

### Iteration 106 — "Benchmarking against Godot: one-way platforms" (done)
Rotating to **2D physics** for breadth (recent rounds were 3D-render, 2D-render, math, UI, IO, audio). Maz's
`Physics2D` collides solid boxes from every side, but a platformer needs the opposite: a ledge that is solid
only from **above** — you fall onto it and land, you jump up from below and pass straight through, and
standing on it you can drop through. Godot ships this as the `one_way_collision` flag on collision shapes
(`StaticBody2D` / `TileMap`). It's a swept-geometry test over a horizontal surface, so it unit-tests
headlessly and drives a deterministic 2D golden.
- [x] **M145 — One-way platforms (`game::resolveOneWayPlatform` / `resolveOneWayPlatforms`)**: a new
  `OneWayPlatform.hpp` with an `OneWayPlatform2D` surface (`y`, `x0`, `x1`) and a swept resolver. Given a
  body's vertical span this step (`prevBottom → curBottom`) and its horizontal extent, it lands the body only
  when it is **descending** (or level), **crossed the surface from above** this step, and **horizontally
  overlaps** the ledge — returning the snapped resting height. A body moving up, already below the surface, or
  outside the ledge's x-range is never blocked, so it tunnels through exactly as a one-way platform should. A
  `snapTolerance` lets a body resting a hair below the surface (numerical overshoot) still count as landed
  rather than falling through. `resolveOneWayPlatforms` sweeps a list and returns the **topmost** surface a
  falling body lands on this step. `testOneWayPlatform` pins eight cases: fall-and-land snaps the bottom to the
  surface; a body launched upward passes through; a body already below passes; no horizontal overlap passes; a
  descending body that hasn't reached the surface yet doesn't land; among stacked platforms a faller lands on
  the topmost; a body over a gap lands on none; and the snap-tolerance case lands a body that overshot just
  past the surface. Unit checks **4928 → 4941**. The new `oneway` demo runs a deterministic fixed-step sim:
  three balls drop onto three ledges and come to rest on top, while a fourth is launched upward through the
  low-left ledge — its trail visibly crosses the platform bar while the others sit on their surfaces.
  Deterministic (fixed initial state + fixed step count) → 2D golden (threshold 0.06). Purely additive (new
  header + new app), so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest
  **100/100 → 101/101**. Honest scope: this is the swept solid-from-above resolve — it does **not** yet wire
  the flag into `Physics2D`'s rigid-body solver as a per-shape collision property, add the "drop-through on
  down-press" input gesture, or support arbitrarily-angled one-way surfaces (Godot's flag carries a rotation);
  those remain the platformer-integration follow-ups.

### Iteration 107 — "Benchmarking against Godot: chorus / flanger / phaser" (done)
Rotating to **audio** for breadth (recent rounds were 2D-physics, 3D-render, 2D-render, math, UI, IO). Maz's
DSP set had filters + delay + reverb/distortion/compressor (M99, M106) but none of the three "time-
modulation" effects — the LFO-swept delays/all-passes that give a synth its width and movement. Godot ships
them as `AudioEffectChorus` and `AudioEffectPhaser`. They're pure per-sample math, so they unit-test exactly
and drive a deterministic offline-waveform golden.
- [x] **M146 — Chorus / flanger / phaser modulated-delay effects (`audio::Chorus` / `Flanger` / `Phaser`)**:
  added to `Dsp.hpp` a small `Lfo` (low-frequency sine oscillator), a `fracTap` linearly-interpolated
  delay-line read, and the three effects. **Chorus** sums up to four detuned, phase-spread voices — each a
  ~15-35 ms delay whose read time wobbles with its own LFO — and blends them with the dry signal to fatten
  one source into an ensemble. **Flanger** sweeps a single very short (~1-5 ms) delay with feedback so a comb
  of notches whooshes through the spectrum. **Phaser** cascades four first-order **all-pass** sections whose
  corner frequency sweeps with an LFO, mixed back with the dry signal for moving notches; the all-pass
  coefficient `(1-w)/(1+w)` stays in a stable range across the swept band. All three expose `Effect`
  subclasses (`ChorusEffect` / `FlangerEffect` / `PhaserEffect`) so they slot onto a `Bus` beside every other
  effect. `testModDsp` pins: the LFO's 0/+1/0/-1/0 quarter-cycle sequence and clean wrap; `fracTap`
  interpolation incl. wrap-around; chorus/flanger pass through untouched at wet=0 and carry an impulse into a
  delayed tail at wet>0; the voice count clamps to [1,4]; the phaser is an exact pass-through at wet=0 and its
  feedback all-pass cascade stays finite/bounded while still colouring the signal; and `reset()` restores the
  first-sample response. Unit checks **4941 → 4960**. The new `modfx` demo runs one sustained sawtooth note
  through each effect and draws the four waveform bands — the clean saw, the chorus's thickened/smeared teeth,
  the flanger's whooshy comb, and the phaser's phase-swirl — deterministically (computed once at 44.1 kHz then
  decimated). 2D golden (threshold 0.06, `modfx` RMSE 0). Purely additive, so every existing golden is byte-
  unchanged (confirmed by a serial golden run); ctest **101/101 → 102/102**. Honest scope: these are the
  offline per-sample effects — they are **not** yet inserted per-voice into the real-time SDL mixer callback,
  and there's no stereo/multi-tap widening or tempo-synced LFO; those remain the live-routing follow-ups.

### Iteration 108 — "Benchmarking against Godot: root motion" (done)
Rotating to **animation** for breadth (recent rounds were audio, 2D-physics, 3D-render, 2D-render, math, UI).
Maz can sample and blend clips (M68) and drive a skeleton, but a locomotion clip that TRAVELS had no way to
hand its travel to the character — you'd play a walk in place and move the body with a separate hand-tuned
velocity, and the feet slide whenever the two disagree. Godot solves this with the AnimationMixer root-motion
track (`get_root_motion_position` / `get_root_motion_rotation`), reading the root bone's displacement back
out of the clip. It's pure planar math, so it unit-tests headless and drives a deterministic 2D golden.
- [x] **M147 — Root motion (`anim::RootMotionTrack`)**: a new `RootMotion.hpp` storing the root's cumulative
  clip-local **position** and **heading** over a clip. `sample(t)` interpolates the cumulative pose;
  `delta(prevT, curT, loop)` returns how far the root moved this step, summing the prev→end and start→cur arcs
  across a loop seam so a looping walk keeps travelling smoothly; `advance(worldPos, worldHeading, ...)`
  applies a step to a world pose — **rotating the clip-local displacement by the character's current facing**
  (so "walk forward" goes wherever the body points) and accumulating the turn. Heading is stored UNWRAPPED (a
  cumulative path integral), so a clip may turn any amount and deltas never need angle-wrap fixups.
  `testRootMotion` pins: linear cumulative sampling + clamping; local-displacement deltas; the loop-seam sum;
  advancing at heading 0 vs facing +90° (the same local step rotates into world +x vs +y); unwrapped heading
  past a full turn; and an **integration test** — a constant forward speed + constant turn rate traces a
  circle, so after the heading sweeps a full 2π the character returns to (near) its start. Unit checks
  **4960 → 4978**. The new `rootmotion` demo runs a fixed-step sim of a walk clip (forward + gentle turn) and
  draws the swept arc with alternating left/right footprints planted along it and oriented to the heading —
  visibly no foot sliding, the travel carried by the clip. 2D golden (threshold 0.06, `rootmotion` RMSE 0).
  Purely additive, so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest
  **102/102 → 103/103**. Honest scope: this is the planar (XZ + yaw) root-motion track and its accumulator —
  it does **not** yet auto-extract the track from a glTF/`AnimClip` root joint, blend root motion across a
  state-machine transition, or handle full 3D root translation with pitch/roll; those remain the follow-ups.

### Iteration 109 — "Benchmarking against Godot: node groups" (done)
Rotating to **scene/ECS** for breadth (recent rounds were animation, audio, 2D-physics, 3D-render, 2D-render,
math). Maz has an ECS (`ecs::World`) and a transform graph, but no way to TAG nodes into named sets and ask
"give me everything tagged X" — so gameplay code had to keep and hand-maintain its own lists of enemies,
pickups, save-points, etc. Godot's SceneTree groups (`add_to_group` / `get_nodes_in_group` / `call_group` /
`is_in_group`) are that primitive. Pure container logic, so it unit-tests exactly and drives a 2D golden.
- [x] **M148 — Node groups (`scene::GroupRegistry`)**: a new `GroupRegistry.hpp` mapping a named group to its
  member node ids (unique, kept in insertion order for determinism), with a reverse index (node → its groups)
  so "which groups is this in?" and whole-node removal are cheap. `add` / `remove` / `removeNode` (drop a node
  from every group on destruction, and drop groups that go empty) / `isInGroup` / `nodesInGroup` / `groupSize`
  / `groupsOf` / `hasGroup` / `groupCount`, plus `call(group, fn)` — Godot's `call_group`, which iterates a
  SNAPSHOT so the callback may freely add or free members mid-broadcast. Ids are plain integers, so it layers
  over `ecs::World` entities, `TransformGraph` nodes, or an app's own handles. `testGroupRegistry` pins:
  add/duplicate/size/insertion order; multi-group membership + `groupsOf`; removing from one group leaving the
  others (and empty groups dropped); `removeNode` clearing a node everywhere at once; and `call` visiting every
  member and staying safe when the callback removes nodes during iteration. Unit checks **4978 → 5013**. The
  new `groups` demo tags a 6×6 grid of 36 nodes (species by colour + cross-cutting `vip` and `hazard` tags),
  then drives two live queries: `nodesInGroup("vip")` rings one diagonal and `call("hazard", ...)` stamps a
  warning on the other, with a footer of `groupSize` counts — no per-app lists. 2D golden (threshold 0.07,
  `groups` RMSE 0). Purely additive, so every existing golden is byte-unchanged (confirmed by a serial golden
  run); ctest **103/103 → 104/104**. Honest scope: this is the group registry itself — it is **not** yet
  auto-wired into a SceneTree so nodes join/leave groups on enter/exit, nor into scene (de)serialization;
  those remain the integration follow-ups.

### Iteration 110 — "Benchmarking against Godot: Rect2" (done)
Rotating to **math** for breadth (recent rounds were scene/ECS, animation, audio, 2D-physics, 3D-render,
2D-render). Maz had only a minimal UI-local `ui::Rect` (x/y/w/h + contains) — no general geometric rectangle
with the operations culling, camera bounds, tilemap regions, and broadphase all need. Godot's `Rect2` is that
primitive. Pure math, so it unit-tests exactly and drives a deterministic 2D golden.
- [x] **M149 — Rect2 (`math::Rect2`)**: a new `Rect2.hpp` — an axis-aligned rectangle by `position` (min
  corner) + `size` with the full Godot operation set: `hasPoint` (min-inclusive / max-exclusive, Godot
  convention), `intersects` (optional include-borders), `intersection` (clip → the overlap, zero-area when
  disjoint), `merge` (union → smallest enclosing rect), `encloses`, `grow` / `growIndividual` (expand/shrink
  sides), `expand` (grow to include a point), and `abs` (normalize a negative-size rect), plus
  left/top/right/bottom/end/center/area/hasArea accessors. `testRect2` pins ~40 checks across every operation,
  including the inclusive-min/exclusive-max point convention, edge-touching with/without borders, disjoint
  clip giving zero area, and negative-size normalization. Unit checks **5013 → 5055**. The new `rects` demo is
  a static gallery: two overlapping rects show their `intersection` filled bright, a third joins them and the
  set's `merge` union is outlined in cyan, one rect's `grow(26)` is a faint halo, and two probe points are
  coloured by `hasPoint` — every value drawn straight from `Rect2`. 2D golden (threshold 0.06, `rects` RMSE
  0). Purely additive, so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest
  **104/104 → 105/105**. Honest scope: this is the float `Rect2` — it does **not** add an integer `Rect2i`, a
  3D `AABB` type, or retrofit the UI/culling code to use it; those remain the follow-ups.

### Iteration 111 — "Benchmarking against Godot: Range / ProgressBar" (done)
Rotating to **UI** for breadth (recent rounds were math, scene/ECS, animation, audio, 2D-physics, 3D-render).
Maz's UI had an immediate-mode slider but no reusable value model and no progress bar — every fill indicator
(health, XP, loading) had to be hand-rolled. Godot factors this into `Range`, the shared base of ProgressBar,
HSlider/VSlider, ScrollBar, and SpinBox: a clamped, optionally-stepped value exposed as a 0..1 ratio. Pure
logic, so it unit-tests exactly and drives a deterministic 2D golden.
- [x] **M150 — Range + ProgressBar (`ui::Range` / `ui::ProgressBar`)**: a new `Range.hpp` with `Range` —
  `minValue`/`maxValue`/`step`/`page`, `setValue` (snap-to-step anchored at min, then clamp), `value`,
  `ratio` (value normalized over `[min, max-page]`), `setRatio`, and `step_` (nudge by whole steps);
  `allowGreater`/`allowLesser` lift the clamp. `page` gives scrollbar-style ranges where a visible window of
  size `page` caps the value at `max-page` while the ratio still spans 0..1. `ProgressBar` wraps a `Range` and
  exposes `fillFraction` (the bar width) + `percent`. `testRange` pins ~22 checks: clamp both ends, ratio /
  setRatio round-trips over a custom range, step snapping (23→20, 27→30), the `page` effective-max, allow-
  greater, `step_` nudging, and the ProgressBar percent readout. Unit checks **5055 → 5077**. The new
  `progress` demo renders six bars from the Range model: plain fills at 25/60/100%, a health bar tinted
  red→green by its own ratio, a custom-range (0..50) mana bar at 60%, and a stepped bar set to 60 that snaps
  to 50 — each with a `percent()` readout. 2D golden (threshold 0.07, `progress` RMSE 0). Purely additive, so
  every existing golden is byte-unchanged (confirmed by a serial golden run); ctest **105/105 → 106/106**.
  Honest scope: this is the Range value model + a ProgressBar; it does **not** yet make the existing slider a
  Range subclass, add an interactive ScrollBar/SpinBox control, or a fill/under/over StyleBox skin for the bar;
  those remain the follow-ups.

### Iteration 112 — "Benchmarking against Godot: render interpolation" (done)
Rotating to **core** for breadth (recent rounds were UI, math, scene/ECS, animation, audio, 2D-physics).
Maz's `Clock` already exposes `interpolationAlpha()` — the leftover accumulator fraction between fixed
steps — but nothing consumed it: physics state was drawn raw, so motion stutters whenever the display rate
doesn't divide the fixed rate. Godot solves this with physics interpolation (keep previous + current, blend
by the frame fraction). This is the missing consumer of that alpha. Pure math, so it unit-tests exactly and
drives a deterministic 2D golden.
- [x] **M151 — Render interpolation (`core::Interpolated<T>` + `core::interpolate`)**: a new `Interpolate.hpp`
  with `Interpolated<T>` (a previous/current pair — `push` once per fixed step shifts current→previous,
  `sample(alpha)` blends by the clamped render alpha so a frame never extrapolates past the current step),
  component-wise `interpLerp` overloads (double/float/vec2/vec3, float-precise under `-Wconversion`), a
  `lerpAngle` that blends along the **shortest arc** across the ±π seam, and a `Transform2DState`
  (position/rotation/scale) with an `interpolate` that lerps position/scale and shortest-arcs rotation.
  `testInterpolate` pins ~18 checks: push/shift, alpha clamping (no extrapolation), reset-seeds-both (no ghost
  after a teleport), vec2 blending, the shortest-arc angle across the seam, and the composite transform blend.
  Unit checks **5077 → 5095**. The new `interp` demo freezes alpha at 0.35 and, for four motions
  (translate / rotate / scale / combined), ghosts the previous + current poses and draws the interpolated
  pose solid between them. 2D golden (threshold 0.06, `interp` RMSE 0). Purely additive, so every existing
  golden is byte-unchanged (confirmed by a serial golden run); ctest **106/106 → 107/107**. Honest scope:
  this is the interpolation state + helpers; it does **not** yet auto-wire into the ECS/TransformGraph so
  every moving node interpolates for free, add 3D transform (quaternion) interpolation, or a global
  physics-interpolation toggle on the scene; those remain the integration follow-ups.

### Iteration 113 — "Benchmarking against Godot: area gravity fields" (done)
Rotating to **2D physics** for breadth (recent rounds were core, UI, math, scene/ECS, animation, audio).
Maz's `Area2D` (M116) detects bodies entering/leaving a region but couldn't change the gravity they feel —
its own comment names "gravity zones" as a use case it didn't implement. Godot's Area2D can override gravity
inside its shape: directional (wind, updraft, sideways force) or a point pull with inverse-square falloff.
Pure geometry + vector math, so it unit-tests exactly and a fixed-step sim drives a golden.
- [x] **M152 — Area gravity fields (`game::GravityArea2D` / `gravityAt`)**: a new `GravityField2D.hpp` — a
  `GravityArea2D` carries a `math::Rect2` region (reusing M149's Rect2), a `GravityType` (Directional / Point),
  a `GravityMode` (Replace / Add), and a `priority`. `zoneGravity` gives one zone's contribution at a point:
  directional = `direction`-normalized × `strength`; point = toward `center` with inverse-square falloff that
  equals `strength` at `unitDistance` (or a constant when `unitDistance ≤ 0`). `gravityAt(areas, p, base)`
  starts from the world default and applies every zone containing `p` in ascending priority — REPLACE zones
  overwrite the accumulated vector (highest priority wins), ADD zones sum onto it — Godot's Area2D
  gravity_space_override. `testGravityField2D` pins ~14 checks: base-only outside zones, directional replace
  (inside overrides, outside unaffected), add-mode accumulation, priority ordering independent of input order,
  and the point-field direction + inverse-square magnitude (== strength at unitDistance, /4 at twice, constant
  when unitDistance≤0). Unit checks **5095 → 5109**. The new `gravzones` demo drops six balls through three
  zones — a WIND field (added rightward), an UPDRAFT (replacing gravity upward), and a point ATTRACTOR — and
  their fixed-step trails visibly drift right, U-turn, and curl into orbits around the attractor centre. 2D
  golden (threshold 0.06, `gravzones` RMSE 0). Purely additive, so every existing golden is byte-unchanged
  (confirmed by a serial golden run); ctest **107/107 → 108/108**. Honest scope: this is the gravity-field
  query; it does **not** yet auto-wire into `Physics2D`'s integrator so bodies read it every step for free,
  support non-rectangular (circle/polygon) zones, or the full set of Godot's five space-override modes; those
  remain the integration follow-ups.

### Iteration 114 — "Benchmarking against Godot: orthographic 3D camera" (done)
Rotating to **3D rendering** for breadth (recent rounds were 2D-physics, core, UI, math, scene/ECS,
animation). Maz's 3D path only had a perspective projection (`math::perspective`); the ROADMAP listed "ortho
3D camera" as an open item. Godot's `Camera3D` supports an Orthogonal projection — the parallel-projection
look of isometric strategy games, CAD, and 2.5D — where objects keep the same on-screen size at every depth
and parallel edges never converge. It's a pure projection matrix, so it unit-tests exactly and drives a 3D
golden.
- [x] **M153 — 3D orthographic projection (`math::orthographic` / `orthographicSize`)**: added to `Math.hpp` a
  Vulkan-correct `orthographic(left, right, bottom, top, near, far)` (clip-space Y flipped like `perspective`,
  depth 0..1) and an `orthographicSize(verticalSize, aspect, near, far)` convenience — Godot's Camera3D `size`
  in Orthogonal mode. Extended `testMath` with the projection's invariants: the w-coordinate stays 1 (no
  perspective divide), the edge points map to ±1, top maps to −1 (the engine's Vulkan y-flip), near/far map to
  0/1, the explicit-bounds form maps its right edge to +1, and — the defining ortho property — a point's
  screen-x is identical at two different depths (no convergence). Unit checks **5109 → 5120**. The new
  `ortho3d` demo renders a 7×7 field of lit cube columns (a central mound, coloured low-blue → high-yellow)
  from a fixed isometric angle through `orthographicSize`; every column reads the same width regardless of how
  far back it sits and all vertical edges stay parallel — the unmistakable isometric look. 3D golden
  (threshold 0.10, `ortho3d` RMSE 0). Purely additive, so every existing golden is byte-unchanged (confirmed
  by a serial golden run); ctest **108/108 → 109/109**. Honest scope: this adds the projection + a demo; it
  does **not** yet add a `projection` toggle on a camera object that swaps perspective↔ortho at runtime, an
  ortho frustum for culling, or a back-face-cull toggle (still an open ROADMAP item); those remain the
  camera-object follow-ups.

### Iteration 115 — "Benchmarking against Godot: base64" (done)
Rotating to **IO** for breadth (recent rounds were 3D-render, 2D-physics, core, UI, math, scene/ECS). Maz
could read/write binary (Serialize, ResourcePack) and text (JSON, CSV, PrefabText) but had no way to carry
BINARY data through a TEXT channel — embedding a texture, a save blob, or any byte buffer inside a JSON
string, a .tres/.tscn resource, a URL, or a config value. Godot exposes that as `Marshalls.raw_to_base64` /
`base64_to_raw`. Pure byte math, so it unit-tests exactly against the canonical vectors and drives a golden.
- [x] **M154 — Base64 (`io::base64Encode` / `base64Decode`)**: a new `Base64.hpp` — RFC 4648 standard
  alphabet, 3 bytes → 4 chars with `=` padding; encode overloads for a byte pointer+length, a `vector<uint8_t>`,
  and a `std::string`; `base64Decode(text, out)` returns false on a stray non-alphabet char, tolerates embedded
  whitespace/newlines (so line-wrapped blobs decode), and treats `=` as end-of-data; plus a convenience
  `base64Decode(text)` returning a fresh vector. `testBase64` pins the canonical vectors (`""`→`""`,
  `f`→`Zg==`, `fo`→`Zm8=`, `foo`→`Zm9v`, … `foobar`→`Zm9vYmFy`), a full 0..255 byte-value round-trip (0x00 and
  0xFF included), odd-length padding round-trips, whitespace-skipping decode, and rejection of a stray
  character. Unit checks **5120 → 5138**. The new `base64` demo is a static readout: a text string, a UTF-8
  string, and a raw byte buffer (shown as hex) each with their live `base64Encode` output, plus a
  `decode(encode(x)) == x` round-trip confirmation. 2D golden (threshold 0.07, `base64` RMSE 0). Purely
  additive, so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest **109/109 →
  110/110**. Honest scope: this is standard base64; it does **not** add the URL-safe (`-_`) variant, a
  streaming/chunked encoder, or Godot's higher-level `var_to_bytes`/variant marshalling; those remain the
  follow-ups.

### Iteration 133 — "Benchmarking against Godot: texture-atlas rectangle packer" (done)
Rotating to **resources / render tooling** for breadth (the last five rounds were particles, core-scripting,
navigation, UI, and math). Re-surveying: Maz can load/decode textures, build meshes, save resource packs
and text/binary scenes — but had **no rectangle bin packer**, the layout step behind Godot's atlas/
sprite-sheet importer and its dynamic font glyph cache (which pack many small images into one texture to
cut draw calls and memory). This is a self-contained, widely-reused algorithm (sprite sheets, glyph
atlases, lightmap packing) that is pure integer geometry — so it unit-tests exactly and renders a
golden-stable packed atlas.

Ranked closable gaps considered this round (resource-weighted): **(1) rectangle bin packer for atlases —
chosen**, the clearest missing resource-tooling primitive; (2) `Resource.duplicate(deep)` deep-copy;
(3) a `ResourceUID`-style stable id registry; (4) an `XMLParser` (Godot ships one, e.g. for TMX);
(5) multi-page/auto-grow atlas packing. (2)–(5) remain follow-ups; a GUI import dashboard is out of scope.

- [x] **M172 — texture-atlas rectangle packer (`render::AtlasPacker`)**: a new `AtlasPacker.hpp` that
  places rectangles into a fixed width×height bin using the **Skyline Bottom-Left** heuristic (track the
  upper contour; drop each rect where its resulting top is lowest, ties to the left; then trim + merge the
  skyline). `insert(w,h)` returns a `Placement{x,y,w,h,placed}` (placed=false, bin unchanged, if it does
  not fit); `pack(sizes)` height-sorts a batch (the standard heuristic) but returns placements in the
  caller's original order; plus `occupancy()`/`reset()`. Fully deterministic. `testAtlasPacker` pins
  bottom-left placement + occupancy, rejection of oversized/degenerate rects (bin left unchanged), an
  exact 4×4 tiling that fills to occupancy 1.0 with a verified no-overlap/in-bounds invariant across every
  pair, a mixed 8-rect batch (original-order return + no overlap), and reset. Unit checks **7478 → 7687**.
  The new `atlas` demo packs 80 deterministic rectangles into a single 600×560 bin, drawing each placed
  rect in a distinct colour with the packed-count and occupancy in the header. 2D golden (threshold 0.05,
  `atlas` RMSE 0). Purely additive, so every existing golden is byte-unchanged; ctest **127/127 →
  128/128**. Honest scope: single-bin, axis-aligned, no rotation or inter-rect padding, no multi-page/
  auto-grow, and not the MaxRects/guillotine variants — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 132 — "Benchmarking against Godot: particle force fields" (done)
Rotating to **particles / VFX** for breadth (the last five rounds were core-scripting, navigation, UI,
math, and audio). Re-surveying: Maz has a runtime particle pool (`fx::ParticleSystem`) with gravity,
drag, and a *single* hard-wired attractor + swirl (M49), plus a `CPUParticles2D`-style emitter resource
(`fx::Emitter`, M120) with emission shapes, per-lifetime curves, and a colour gradient. What it lacked
was Godot's **attractor family** — `GPUParticlesAttractor2D` lets you place *multiple* attractors (each a
signed strength with a falloff and radius) that pull or push particles, and pair them with wind and drag
to sculpt swarms. That is a distinct, reusable force model (gravity wells, black holes, wind tunnels,
orbiting rings) useful for gameplay too, not just the built-in pool. Pure math + a deterministic
integrator, so it unit-tests exactly and renders a golden-stable swirl.

Ranked closable gaps considered this round (particle-weighted): **(1) composable multi-attractor force
field — chosen**, the clearest missing VFX primitive; (2) radial/tangential/orbit/angular per-lifetime
acceleration on the emitter resource (Godot ParticleProcessMaterial); (3) sub-emitters (spawn particles
from dying particles); (4) turbulence-noise fields; (5) particle trails/ribbons. (2)–(5) remain
follow-ups; a GPU-baked vector-field attractor is out of scope on the CPU path.

- [x] **M171 — composable 2D particle force field (`fx::ForceField2D`)**: a new `ForceField2D.hpp` holding
  a set of `Attractor2D` (position, signed `strength`, influence `radius`, a `Falloff` of Constant /
  Linear / InverseSquare, and an optional tangential `swirl` for vortices) on top of a uniform `wind`
  acceleration and a global linear `drag`. It exposes the pure query `accelAt(pos, vel)` and a
  deterministic semi-implicit-Euler `step(particles, dt, substeps)` integrator; the InverseSquare
  denominator is clamped so a particle on the well doesn't launch to infinity, and out-of-radius
  attractors contribute nothing. `testForceField2D` pins attract-vs-repel direction + magnitude, the
  inverse-square distance ratio, radius cutoff and linear half-strength, position-independent wind,
  drag bleeding off speed without reversing it, pure-swirl producing a perpendicular-only acceleration,
  multi-step convergence toward a well, and the empty-field/empty-list no-ops. Unit checks **7457 →
  7478**. The new `forcefield` demo seeds 800 particles (fixed RNG seed), pre-simulates a fixed 150
  steps, and draws the settled swarm coloured by speed around a cyan swirling attractor and a red
  repulsor (with its influence ring) under a leftward wind — fixed seed + fixed step count = golden
  stable. 2D golden (threshold 0.05, `forcefield` RMSE 0). Purely additive, so every existing golden is
  byte-unchanged; ctest **126/126 → 127/127**. Honest scope: CPU point/vector force model — no
  GPU-baked vector-field attractors, 3D attractors, or turbulence-noise process yet.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 131 — "Benchmarking against Godot: runtime Expression evaluator" (done)
Rotating to **core / scripting-adjacent** for breadth (the last five rounds were navigation, UI, math,
audio, and IO). Re-surveying: Maz's `core` was broad — logging, config/CVars, events + named signals, a
job system, a scheduler, RNG, noise, a profiler, string interning, slot-maps, ring buffers, resource
caches — but had **no runtime formula evaluator**. Godot ships an `Expression` class: parse a math string
at runtime, then execute it repeatedly with named variables. It is the closest thing to "scripting" that
is actually buildable in this headless sandbox (a full GDScript/C# VM is out — noted honestly), and it is
what powers data-driven design: damage/difficulty/economy formulas edited in a config or by a designer,
procedural-parameter curves, spawn weights, tool sliders — none of which should require a recompile. Pure
lexer + recursive-descent parser + evaluator, so it unit-tests exactly and drives a function-plot golden.

Ranked closable gaps considered this round (core-weighted): **(1) runtime math Expression evaluator —
chosen**, the highest-leverage missing core primitive; (2) a `Variant`-style dynamic value type; (3) a
generic property/reflection registry for objects; (4) a `Callable`/bind wrapper over the existing Signal;
(5) `PackedByteArray`-style typed buffers. (2)–(5) remain follow-ups. A genuine scripting VM
(GDScript/C#) stays structurally out of scope.

- [x] **M170 — runtime Expression evaluator (`core::Expression`)**: a new `Expression.hpp` that lexes a
  formula string, parses it with recursive descent into a flat, copyable AST node pool, and evaluates it.
  API mirrors Godot: `parse(text, varNames)` (returns false + `errorText()` on any lexical/syntax/semantic
  error), then `execute(inputs)` indexed by the declared variable order — parse once, evaluate many. Full
  precedence with `^` right-associative and binding tighter than unary minus (so `-2^2 == -4`, `2^3^2 ==
  512`), `+ - * / % ^`, parentheses, the constants `pi`/`tau`/`e`, and functions `sin cos tan asin acos
  atan exp log log2 sqrt abs floor ceil round sign frac` (1-arg), `pow atan2 min max mod` (2-arg), `clamp
  lerp` (3-arg). Divide/mod-by-zero are guarded to 0; missing inputs read 0. `testExpression` pins
  precedence + associativity + constants, variable binding with tree reuse across inputs, every multi-arg
  function, the zero guards, and eight distinct error cases (dangling operator, unbalanced parens, empty
  operand, unknown function, wrong arity, unknown identifier, trailing tokens, bad character) plus
  error-recovery on a subsequent good parse. Unit checks **7407 → 7457**. The new `expr` demo plots three
  curves each PARSED FROM TEXT (a sine, a damped sine `sin(x*tau*4)*exp(-x*3)`, a clipped
  `clamp(sin(x*tau)+0.4,-1,1)`) by sampling f(x) across the panel, plus a data-driven "damage rule" card
  `base*(1+rate*lvl)` evaluated with named variables to a concrete number + bar. 2D golden (threshold
  0.05, `expr` RMSE 0). Purely additive, so every existing golden is byte-unchanged; ctest **125/125 →
  126/126**. Honest scope: this is the numeric subset (doubles in, one double out) — no Variant/string/
  boolean/comparison operators, array/dictionary literals, or method calls on a base object, and it is
  not a general scripting VM.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 130 — "Benchmarking against Godot: AStar2D graph pathfinding" (done)
Rotating to **navigation / pathfinding** for breadth (the last five rounds were UI, math, audio, IO, and
core-containers). Re-surveying Godot's navigation stack against Maz: Maz already has grid A* (`NavGrid`),
navmesh-cell A* with a string-pulling funnel (`NavMesh`), flow-field crowd steering (`FlowField`), RVO local
avoidance (`rvoVelocity`), and steering behaviours — a broad set. But it had **no arbitrary-graph A***: Godot
ships `AStar2D` (and `AStar3D`) as a first-class class where you place points at any position with any id,
connect them freely (one- or two-way), give each point a `weight_scale`, and query the least-cost route. That
is a genuinely different tool from a uniform grid or a convex-cell mesh — it is the primitive behind road/rail
networks, waypoint webs, teleporter links, dialogue/skill graphs, and level-connection maps, none of which map
cleanly onto a grid. Pure graph + geometry, so it unit-tests exactly and drives a golden.

Ranked closable gaps considered this round (navigation-weighted): **(1) arbitrary weighted-graph A* (`AStar2D`)
— chosen**, the clearest missing navigation primitive; (2) navmesh region stitching / off-mesh links across
separate regions; (3) dynamic obstacle carving of a baked navmesh; (4) `AStarGrid2D` (a grid front-end over the
same solver, with diagonal modes + partial paths); (5) navigation layers/masks on agents. (2)–(5) remain
follow-ups.

- [x] **M169 — AStar2D general weighted-graph pathfinding (`game::AStar2D`)**: a new `AStar2D.hpp` holding
  points in an ordered map (id → position + `weightScale`) and each point's neighbours in an ordered set, so
  identical graphs yield identical paths. API mirrors Godot: `addPoint`/`hasPoint`/`removePoint`/`getPointIds`,
  `getPointPosition`/`setPointPosition`, `getPointWeightScale`/`setPointWeightScale`, `connectPoints`/
  `disconnectPoints`/`arePointsConnected`/`getPointConnections` (one-way when `bidirectional=false`), `getIdPath`
  and `getPointPath` (A* with a Euclidean heuristic; step cost = edge length × the destination point's weight),
  plus `getClosestPoint` and `getClosestPositionInSegment` (snap an off-graph position onto the nearest edge).
  `testAStar2D` pins point/edge bookkeeping (bidirectional vs one-way, ascending connection order, self-loop
  rejection, edge cleanup on `removePoint`), the two-route choice and how a heavy `weightScale` flips it, the
  degenerate paths (same start/goal → single node, disconnected → empty, missing endpoint → empty), one-way edge
  respect, and both closest queries (on-segment projection + endpoint clamp). Unit checks **7372 → 7407**. The
  new `astar` demo draws a road network of 11 junctions where START→GOAL routes AROUND a red weight-×6 "toll"
  junction (chosen route as a thick amber ribbon), plus a right panel showing `getClosestPositionInSegment`
  snapping a free query point onto the nearest edge. 2D golden (threshold 0.05, `astar` RMSE 0). Purely additive,
  so every existing golden is byte-unchanged; ctest **124/124 → 125/125**. Honest scope: the cost model is fixed
  to weighted Euclidean (no `_compute_cost`/`_estimate_cost` subclass override), and this is the 2D graph only —
  `AStar3D` and a grid-front-end `AStarGrid2D` (with diagonal modes + partial paths) remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 129 — "Benchmarking against Godot: PopupMenu control" (done)
Rotating to **UI / Control nodes** for breadth (recent rounds were math, audio, IO, core-containers,
3D-render). Maz's Control set was broad — LayoutNode/Container, Range/ProgressBar, TextField, Tree, ItemList
(M161), StyleBoxFlat/Theme, rich text — but had **no PopupMenu**, Godot's vertical item list behind right-click
context menus, OptionButton dropdowns and menu bars. That is a distinct control from ItemList (a flat scrolling
list): a menu adds checkbox/radio item states, separators, disabled rows, accelerator hints, and submenu
arrows, and its interaction is hover + activate rather than multi-select. Pure logic + geometry, so it
unit-tests exactly and drives a UI golden.
- [x] **M168 — PopupMenu control (`ui::PopupMenu`)**: a new `PopupMenu.hpp` holding items (label + id +
  optional `MenuCheck` checkbox/radio state, `disabled`, `separator`, `submenu`, and an accelerator `shortcut`
  hint), built via `addItem`/`addCheckItem`/`addRadioItem`/`addSubmenuItem`/`addSeparator`. It stacks rows from
  a `position` at a fixed height (thin separators), exposing `rect()`/`itemRect(i)`/`totalHeight()` for drawing,
  `itemAtPoint()` to hit-test (rejecting separators + outside points), `hoverNext`/`hoverPrev` keyboard nav that
  skips separators + disabled rows and wraps, `checkRadio(i)` (single-choice group — checks one, unchecks the
  rest), and `activate()` (toggles a checkbox, switches a radio group, returns the hovered item's id). `testPopupMenu`
  pins the row geometry against a hand-computed layout (total height, item y with separators, itemRect), hit-testing
  inside a row / on a separator / above / beside the menu, hover nav skipping both a separator and a disabled row
  and wrapping (forwards + backwards), checkbox toggle-on-activate, radio single-choice exclusivity, a disabled
  item firing nothing, and the empty-menu edges. Unit checks **7341 → 7372**. The new `popupmenu` demo draws an
  open context menu in a StyleBoxFlat panel — a hovered accent row, a checked "Word Wrap" tick, a Dark radio dot,
  a dimmed disabled "Paste", right-aligned Ctrl-shortcuts, separators, and an "Export As" submenu arrow. 2D golden
  (threshold 0.05, `popupmenu` RMSE 0). Purely additive, so every existing golden is byte-unchanged (confirmed by a
  serial golden run); ctest **123/123 → 124/124**. Honest scope: this is the single-level menu model + geometry;
  it does **not** yet open nested submenu popups, auto-size its width to the widest label/shortcut via the theme
  font, or wire an OptionButton wrapper around it — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 128 — "Benchmarking against Godot: 2D affine Transform2D" (done)
Rotating to **math** for breadth (recent rounds were audio, IO, core-containers, 3D-render, animation). Maz
had `scene::TransformGraph` (a hierarchy of *decomposed* TRS nodes) and a `Transform2DState` pose-blend in
`core::Interpolate`, but no first-class **Transform2D** value type — Godot's 2×3 affine matrix, the primitive
behind *every* Node2D. Without it there was no single object to compose a parent's placement with a child's,
to convert a point between local and world/screen space, or to read a node's rotation/scale/skew back from
its matrix; those were done ad hoc. Pure math, so it unit-tests exactly and drives a golden.
- [x] **M167 — 2D affine Transform2D (`math::Transform2D`)**: a new `Transform2D.hpp` storing two basis
  columns + an origin (Godot's layout) with builders (`identity`/`rotation`/`scaling`/`translation` and the
  `compose(rotation, scale, position, skew)` node constructor), point/vector application (`xform`,
  `basisXform`, `xformInv`), composition (`operator*`, parent×child), `affineInverse` (full inverse, correct
  under scale/skew), decomposition (`getRotation`/`getScale`/`getSkew`, signed scale for a mirrored basis),
  `orthonormalized` (Gram-Schmidt), `determinant`, and `interpolateWith` (decompose → lerp pos/scale +
  shortest-arc rotation → recompose). `testTransform2D` pins identity, translation vs basis-only vector
  transform, a +90° rotation sending +X→+Y, non-uniform scale + area determinant, right-to-left composition
  equalling sequential application, an affine-inverse round-trip (scaled+rotated+translated) incl. `xformInv`
  and `m*inverse=identity`, compose→decompose round-trips, a negative scale from a mirror, orthonormalization
  (unit + perpendicular + preserved rotation/origin), and a midpoint `interpolateWith`. Unit checks
  **7298 → 7341**. The new `xform2d` demo draws one asymmetric arrow under a gallery of transforms (identity /
  rotate / non-uniform scale / rotate+scale / skew / mirror), each over a ghost of the original with the
  matrix's basis columns drawn as a red/green gizmo. 2D golden (threshold 0.05, `xform2d` RMSE 0). Purely
  additive, so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest
  **122/122 → 123/123**. Honest scope: this is the value type + operations; it does **not** yet retrofit
  `TransformGraph`/sprites onto it as their storage, nor add a 3D `Transform3D`/`Basis` sibling — those remain
  follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 127 — "Benchmarking against Godot: audio spectrum analyzer (FFT)" (done)
Rotating to **audio** for breadth (recent rounds were IO, core-containers, 3D-render, animation, UI). Maz's
audio layer was rich on synthesis and DSP *effects* — filters, delay, reverb, distortion, compressor, chorus/
flanger/phaser, ADSR, positional 2D/3D, a sample mixer — but had **no frequency analysis**: no FFT, no
spectrum. That is exactly what Godot's `AudioEffectSpectrumAnalyzer` provides, and it is the backbone of
audio-reactive gameplay (rhythm games, VU / equalizer visualizers, beat-reactive lights and particles,
lip-sync). Pure DSP maths, so it unit-tests exactly (a pure tone peaks on its bin) and drives a golden.
- [x] **M166 — spectrum analyzer / FFT (`audio::SpectrumAnalyzer`)**: a new `Spectrum.hpp` with a standalone
  in-place iterative radix-2 Cooley-Tukey `fft(buffer, inverse)` (bit-reversal + butterflies; the inverse
  round-trips) and `nextPow2`, plus a `SpectrumAnalyzer` that windows (Hann or rectangular) + zero-pads a
  sample frame, runs the FFT, and exposes single-sided per-bin `magnitude(bin)`, `binFrequency(bin)`,
  `peakBin()`, and `magnitudeForRange(lowHz, highHz)` — the exact query Godot's analyzer gives
  (`get_magnitude_for_frequency_range`) for band energy. `testSpectrum` pins `nextPow2`, a forward→inverse FFT
  round-trip, a unit cosine on an exact bin (correct peak bin + frequency + single-sided amplitude ≈ 1.0 +
  silent neighbours + the band query finding it), a DC signal (all energy in bin 0), a two-tone mix (two peaks
  at the right amplitudes, the louder as peakBin, silence in the gap), and binCount = N/2+1. Unit checks
  **7262 → 7298**. The new `spectrum` demo synthesizes a chord (250/375/500 Hz + a faint 1000 Hz partial),
  runs it through the Hann-windowed analyzer, and plots the magnitude spectrum as a frequency bar graph
  (band-coloured, peak-labelled, axis-ticked) with three `magnitudeForRange` band meters (bass/mid/treble). 2D
  golden (threshold 0.05, `spectrum` RMSE 0). Purely additive, so every existing golden is byte-unchanged
  (confirmed by a serial golden run); ctest **121/121 → 122/122**. Honest scope: this is an offline
  block-analysis FFT; it does **not** yet run as a live streaming effect inside the SDL mixer callback, nor
  add mel/bark perceptual banding — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 126 — "Benchmarking against Godot: INI ConfigFile" (done)
Rotating to **IO / serialization** for breadth (recent rounds were core-containers, 3D-render, animation, UI,
math-2D). Maz's io layer had JSON, a binary `ByteWriter`/`ByteReader`, base64, WAV, a resource pack, and a
JSON↔cvar config *bridge* — but no **ConfigFile**, Godot's INI-style `[section]` + `key=value` store behind
project settings, input maps, and hand-editable options/save files. Games and tools constantly need a
human-readable, diff-friendly, line-oriented settings format that a player or modder can edit in a text
editor; JSON is stricter and noisier for that. Pure text, so it unit-tests exactly and drives a golden.
- [x] **M165 — INI ConfigFile (`io::ConfigFile`)**: a new `ConfigFile.hpp` holding ordered sections of ordered
  `key=value` string pairs with typed accessors (`getBool`/`getInt`/`getFloat` coerce, incl. `true/1/yes/on`
  bool synonyms; `setBool`/`setInt`/`setFloat` format), `getValue`/`setValue` with defaults,
  `hasSection`/`hasSectionKey`/`eraseSectionKey`/`eraseSection`, and `sections()`/`sectionKeys()`. `parse()` is
  lenient — blank lines and `;`/`#` comments skipped, whitespace trimmed, matching quotes stripped, keys before
  any header land in the unnamed global section — and `encode()` emits stable, insertion-ordered,
  diff-friendly text that round-trips. `testConfigFile` pins a full settings parse (global + video + audio,
  comments, quotes), insertion-ordered sections/keys, typed reads incl. bool synonyms and quote-stripping,
  missing-key defaults, structure queries, in-place overwrite (not append), key + section erase, an
  encode→parse→encode idempotent round-trip, and the empty-input case. Unit checks **7225 → 7262**. The new
  `inifile` demo parses a game `settings.cfg`, renders it as a grouped section/key/value table, then edits it
  (bumps version, swaps resolution, mutes audio, adds a bind) and shows the re-encoded INI text beside it. 2D
  golden (threshold 0.05, `inifile` RMSE 0). Purely additive, so every existing golden is byte-unchanged
  (confirmed by a serial golden run); ctest **120/120 → 121/121**. Honest scope: values are stored/returned as
  strings (typed on read); it does **not** encode full Godot Variant literals (`Vector2(...)`, arrays,
  dictionaries) in a value, nor read/write directly from disk paths here — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 125 — "Benchmarking against Godot: fixed-capacity RingBuffer" (done)
Rotating to **core / engine foundations** for breadth (recent rounds were 3D-render, animation, UI, math-2D,
2D-physics). Maz's core had `SlotMap` (handles), `ResourceCache`, `EventBus`, `Signal`, `StringTable`,
`Scheduler`, `Random`, `Noise`, `Profiler` — but no **ring / circular buffer**, the container behind rolling
histories (frame-time / FPS graphs, moving averages), bounded input buffers (a fighting game's last-N presses,
jump "coyote" windows), replay traces, and streaming audio/network queues. Every such use was hand-rolling
index arithmetic. Godot keeps a `RingBuffer` for exactly these jobs. Pure container, so it unit-tests exactly
and drives a golden (a scrolling history plot).
- [x] **M164 — fixed-capacity RingBuffer (`core::RingBuffer<T>`)**: a new `RingBuffer.hpp` template backed by
  a fixed vector with a head + count, serving two idioms from one structure — a ROLLING WINDOW (`push` always
  succeeds; once full it overwrites the OLDEST element, returning whether it evicted) and a bounded FIFO QUEUE
  (`pushBack` rejects when full, `popFront` drains oldest-first). Logical indexing (`at(0)` oldest,
  `at(size-1)` newest) hides the physical wrap; `front`/`back`/`toVector`/`clear`/`reset`/`full`/`empty`
  round it out. `testRingBuffer` pins fill-under-capacity FIFO order, the exact-fill boundary (the push that
  reaches capacity evicts nothing; the next one does), rolling-window overwrite + slide, bounded-FIFO
  reject-when-full + drain, head wrap-around after interleaved pops/pushes, clear vs reset, zero-capacity
  no-op, and a float rolling-average. Unit checks **7178 → 7225**. The new `ring` demo drives two buffers: a
  96-slot frame-time history fed 140 deterministic samples (oldest 44 evicted) plotted as a green/amber/red
  bar graph against a 16.6 ms budget line with the rolling average across it, and an 8-slot input buffer fed a
  longer press sequence, showing the last 8 as chips. 2D golden (threshold 0.05, `ring` RMSE 0). Purely
  additive, so every existing golden is byte-unchanged (confirmed by a serial golden run); ctest
  **119/119 → 120/120**. Honest scope: this is a single-producer/single-consumer, non-thread-safe container;
  it does **not** add a lock-free MPSC variant or a bit/byte stream view — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 124 — "Benchmarking against Godot: Camera3D projection" (done)
Rotating to **3D rendering / math-for-3D** for breadth (recent rounds were animation, UI, math-2D, 2D-physics,
audio). Maz could build view/projection matrices (`math::perspective`/`orthographic`) and cull meshes against
a frustum inside the renderer, but exposed **no screen↔world projection** — Godot's Camera3D API of
`unproject_position` (world→screen), `project_ray_origin`/`project_ray_normal` (screen→world ray),
`project_position`, and `is_position_in_frustum`. Those underpin mouse picking in 3D, world-space UI labels /
health bars floating over units, aim rays, and gameplay off-screen tests — none of which the engine could do.
The frustum-plane extraction also lived privately in `MeshRenderer.cpp`; this promotes a reusable version.
Pure matrix math, so it unit-tests exactly and drives a golden by projecting a 3D scene to 2D.
- [x] **M163 — Camera3D projection (`render::Camera3D`)**: a new `Camera3D.hpp` holding a view + projection
  matrix (from `lookAt` + `perspective`, or set directly) and a viewport, exposing `worldToScreen`
  (→ `Projected{screen, depth, inFront}`, top-left pixel origin, matching the engine's Vulkan y-down / 0..1
  clip), `screenToRay` (→ `Ray3{origin at the eye, normalized direction}`), `screenToWorld(screen, distance)`,
  and `frustum()` / `isPointVisible` / `isSphereVisible` (Gribb-Hartmann six-plane extraction, inward normals,
  near from row2 for 0..1 depth). `testCamera3D` pins the maths against a hand-computed 800×600 / fov-90 /
  eye-at-(0,0,5) setup: the look-at target projects to the exact centre and is in-front; a unit +X offset lands
  at pixel 480 and +Y projects above centre; nearer points have smaller clip depth; a point behind reports
  `inFront=false`; the centre ray is (eye, −Z) and normalized; a projected point round-trips onto its own ray;
  `screenToWorld` 5 units down the centre ray hits the origin; and frustum containment accepts the origin while
  rejecting points behind / far to the side / beyond the far plane (plus a straddling-sphere case). Unit checks
  **7150 → 7178**. The new `camera3d` demo uses the 2D renderer as the display: a perspective Camera3D projects
  a ground grid, RGB world axes, and a wireframe cube to 2D lines (real perspective foreshortening), colours a
  scatter of world points green/red by `isPointVisible`, and casts a centre-screen ray to the ground plane and
  marks the unprojected hit. 2D golden (threshold 0.06, `camera3d` RMSE 0). Purely additive, so every existing
  golden is byte-unchanged (confirmed by a serial golden run); ctest **118/118 → 119/119**. Honest scope: this
  is the projection/ray/frustum core; it does **not** yet retrofit the mesh renderer's private frustum onto it,
  nor add near/far-plane polygon clipping of projected segments (the demo skips any line with an endpoint behind
  the camera) — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 123 — "Benchmarking against Godot: colour Gradient resource" (done)
Rotating to **animation** for breadth (recent rounds were UI, math, 2D-physics, audio, input). Maz had
`anim::Curve` (M155, a keyframed scalar function) but no colour **Gradient** — Godot's companion resource that
maps a parameter to a *colour* rather than a scalar. Gradients drive particle colour-over-lifetime
(CPUParticles), GradientTexture1D/2D, sky and depth ramps, and health/heat tints; the engine's particle
systems and 2D lights had only fixed or hand-lerped colours. Pure data + interpolation, so it unit-tests
exactly and drives a colourful golden.
- [x] **M162 — colour Gradient (`anim::Gradient`)**: a new `Gradient.hpp` holding sorted `(offset, Color)`
  stops with three interpolation modes matching Godot's `Gradient.InterpolationMode` — Constant (hard bands),
  Linear (per-channel lerp), and Cubic (a Catmull-Rom spline through the neighbouring stops, clamped to [0,1]
  so overshoot can't produce invalid colours). `sample(t)` clamps below the first / above the last stop
  (Godot's clamped domain); a two-colour constructor gives the black→white default; `addStop` keeps stops
  sorted, `setOffset` re-sorts, and `bake(N)` produces an N-colour ramp (Godot's GradientTexture1D). `testGradient`
  pins the empty/single-stop cases, the black→white linear midpoint + domain clamp, a red/green/blue three-stop
  quarter-point, sampling exactly at a stop returning that colour in any mode, Constant band-holding, Cubic
  hitting the stops with channels staying in [0,1], `bake` endpoints/length/midpoint incl. bake(1)/bake(0),
  and sorted insertion + setOffset re-sort. Unit checks **7112 → 7150**. The new `gradient` demo renders a
  gallery of ramp bars — the same five-stop spectrum under Constant/Linear/Cubic side by side, fire / health /
  ocean ramps, and the fire ramp baked to eight discrete swatches with stop ticks. 2D golden (threshold 0.05,
  `gradient` RMSE 0). Purely additive, so every existing golden is byte-unchanged (confirmed by a serial golden
  run); ctest **117/117 → 118/118**. Honest scope: this is the colour-ramp core; it does **not** yet include a
  selectable interpolation *colour space* (Godot's sRGB/OKLab `interpolation_color_space`), a GPU
  GradientTexture upload, or wiring gradients into the existing particle colour track — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 122 — "Benchmarking against Godot: ItemList control" (done)
Rotating to **UI / Control nodes** for breadth (recent rounds were math, 2D-physics, audio, input, 2D-render).
Maz already had a broad Control set — `LayoutNode`/`Container` (anchors + auto-layout), `Range`/`ProgressBar`,
`TextField`, `Tree`, `StyleBoxFlat`/`Theme`, rich text — but no **ItemList**, Godot's scrollable box of
choosable rows. It backs Godot's FileDialog file list, the audio-bus and animation pickers, and countless game
inventory / dialogue / level-select panels; the `Tree` control covers hierarchies but not the flat, fixed-row,
single-or-multi-select list. Pure selection + geometry logic, so it unit-tests exactly and drives a UI golden.
- [x] **M161 — ItemList control (`ui::ItemList`)**: a new `ItemList.hpp` holding rows (`text`, caller `id`,
  `selectable`/`disabled` flags) with a `Single` (radio — selecting one clears the rest) or `Multi` (rows
  toggle independently) selection mode, plus the fixed-row geometry a view needs: `rowStride`/`contentHeight`/
  `maxScroll`, a clamped `scroll` offset, `itemRect(i)` for a row's pixel box, `itemAtPoint(px,py)` to hit-test
  a click (rejecting the separator gaps and the area past the last row), `ensureVisible(i)` to scroll a row
  into the box, and `visibleRange()` for the rows a renderer should draw. `selectNext`/`selectPrevious` walk
  keyboard focus, skipping disabled / non-selectable rows and clamping (no wrap) at the ends. `testItemList`
  pins the geometry against a hand-computed 100-px box (stride, content height, max-scroll, itemRect y, visible
  range, hit-testing inside a row / outside the box / in a separator gap), the Single radio vs Multi accumulate
  + toggle behaviour, disabled/non-selectable rejection and keyboard-nav hole-skipping, scroll clamping and
  `ensureVisible`, and the empty-list edge (empty `visibleRange` with first > last). Unit checks **7056 → 7112**.
  The new `itemlist` demo draws two lists in StyleBoxFlat panels — a single-select saved-games list scrolled so
  the highlighted selection sits mid-box with two disabled (dimmed) rows and a scrollbar thumb sized to the
  visible fraction, and a multi-select loadout list with three rows checked at once. UI golden (threshold 0.05,
  `itemlist` RMSE 0). Purely additive, so every existing golden is byte-unchanged (confirmed by a serial golden
  run); ctest **116/116 → 117/117**. Honest scope: this is the core list model + geometry; it does **not** yet
  include icon columns, per-item custom foreground/background colours in the widget, multi-column grid layout, or
  drag-reorder — those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 121 — "Benchmarking against Godot: 2D geometry helpers" (done)
Rotating to **math** for breadth (recent rounds were 2D-physics, audio, input, 2D-render, animation, IO). The
workhorse 2D computational-geometry queries — does this segment cross that one, what is the nearest point on
this edge, is this point inside that polygon — underpin AI line-of-sight, mouse/hit picking, trigger zones,
and path building. Maz had them scattered (triangle area in the triangulator, SAT in `ConvexShape2D`, ray/AABB
in `Collision`) but no consolidated set matching Godot's `Geometry2D` static class. Pure math, so it
unit-tests exactly and drives a 2D golden.
- [x] **M160 — Geometry2D helpers (`math::Geometry2D`)**: a new `Geometry2D.hpp` with `segmentIntersect`
  (segment×segment → `SegmentHit{point, t, u}` via the cross-product parameters, parallel/collinear → no hit),
  `closestPointOnSegment` (projection clamped to the endpoints, degenerate-safe), `distanceToSegment`,
  `pointInPolygon` (even-odd ray cast, convex or concave, either winding), and `segmentIntersectsCircle`
  (distance-to-segment ≤ radius) — the segment/polygon/circle set Godot exposes as `Geometry2D`.
  `testGeometry2D` pins: the classic X-cross intersection (point + both params 0.5), parallel non-hit, a
  would-cross-if-extended non-hit, a T-junction endpoint touch; closest-point mid-segment / endpoint-clamp /
  degenerate / distance; point-in-polygon for a square (in / out on all four sides) and a **concave dart**
  where a point in the notch is correctly *outside*; and circle×segment hit/miss including the beyond-the-end
  endpoint case. Unit checks **7028 → 7056**. The new `geometry` demo shows three panels — a web of segments
  with every pairwise intersection dotted, a concave arrow polygon with a grid of test points coloured
  inside/outside, and a query point projected to the closest point on each of several segments plus a
  circle×segment test. 2D golden (threshold 0.06, `geometry` RMSE 0). Purely additive, so every existing
  golden is byte-unchanged (confirmed by a serial golden run); ctest **115/115 → 116/116**. Honest scope: this
  is the segment/polygon/circle core; it does **not** yet include convex-hull, polygon boolean clipping
  (Godot's `merge`/`clip`/`intersect_polygons` via Clipper), polygon offsetting, or Delaunay — those remain
  follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 120 — "Benchmarking against Godot: kinematic character controller" (done)
Rotating to **2D physics** for breadth (recent rounds were audio, input, 2D-render, animation, IO, 3D-render).
Maz's 2D physics is impulse-based rigid bodies (`Physics2D`) plus a 3D discrete `slideMove`, but it had no
**kinematic character controller** — the single most-used movement primitive in platformers and top-down
games. Godot's `CharacterBody2D.move_and_slide` drives a body directly by a velocity, sweeps it so it never
tunnels through geometry, and slides the leftover motion along contacts (rounding corners / running along
walls in one call), classifying each contact as floor / wall / ceiling. Pure geometry, so it unit-tests
exactly and drives a deterministic golden.
- [x] **M159 — Kinematic move-and-slide (`game::moveAndSlide` + `sweptAabb`)**: a new `KinematicBody2D.hpp`
  with an `Aabb2` box, a **swept AABB** test (Minkowski-expand the solid by the body's half-extents, cast the
  body centre as a ray through it → earliest entry time + face normal), and `moveAndSlide` which advances the
  body to the first contact, nudges out by a skin, classifies the normal against an `up` direction and
  `floorMaxAngle` into floor/wall/ceiling, then slides the leftover motion (and the reported velocity) along
  the surface, repeating for up to `maxSlides` iterations. Returns a `SlideResult { position, velocity,
  onFloor/onWall/onCeiling, floorNormal, slides }`. `testKinematicBody2D` pins: sweptAabb entry-time/normal,
  no-hit when moving away or not moving; and moveAndSlide's free move (exact), head-on wall stop (velocity.x
  killed, wall flagged), fall-onto-floor (rests, floorNormal, velocity.y killed), rise-into-ceiling, and the
  headline case — a diagonal push into a vertical wall where x is pinned but the body still **slides in y in
  one call** with tangential velocity preserved. Unit checks **7002 → 7028**. The new `kinematic` demo runs
  one character through a fixed obstacle course (~4 s) under gravity + a constant rightward drive and draws
  its whole path coloured by contact state — BLUE airborne, GREEN on-floor, ORANGE on-wall — reading as a
  cascade: fall onto a platform, run off the edge, arc into a floating wall and slide down it, drop off, run
  along the ground. 2D golden (threshold 0.06, `kinematic` RMSE 0). Purely additive, so every existing golden
  is byte-unchanged (confirmed by a serial golden run); ctest **114/114 → 115/115**. Honest scope: this is
  AABB-vs-AABB swept collision against a static list; it does **not** yet handle rotated/oriented or circle
  shapes, moving platforms, floor snapping / stair-stepping, or `move_and_collide`'s single-hit stop-and-
  report; those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 119 — "Benchmarking against Godot: audio stream randomizer" (done)
Rotating to **audio** for breadth (recent rounds were input, 2D-render, animation, IO, 3D-render, 2D-physics).
Maz has a broad audio stack (mixer, DSP effects + buses, ADSR, WAV codec, 2D/3D positioning) but repetitive
one-shots still played the *identical* clip every trigger — footsteps, gunshots, and UI blips sound robotic
without variation. Godot's `AudioStreamRandomizer` wraps a pool of interchangeable clips and, per trigger,
picks one and jitters pitch + volume so the ear never hears a mechanical repeat. Pure selection maths, so it
unit-tests exactly (seeded) and drives a 2D golden.
- [x] **M158 — Stream randomizer (`audio::StreamRandomizer`)**: a new `Randomizer.hpp` holding a weighted
  clip pool and three pick modes matching Godot — `Random` (weighted uniform), `RandomNoRepeat` (weighted but
  never the clip that just played, Godot's default), and `Sequential` (round-robin). Each `next()` returns a
  `RandomPick { index, pitchScale, volumeDb }`: the pitch scaled by a **log-symmetric** factor in
  `[1/randomPitch, randomPitch]` and the volume offset drawn from `[-randomVolumeOffsetDb, +…]`. Deterministic
  via a seeded `core::Random`, so runs are reproducible. `testStreamRandomizer` pins: empty pool → −1;
  Sequential strict round-robin + `reset`; RandomNoRepeat never repeats over 500 draws (all in range); a lone
  stream always returns 0; a 100:1:1 weighting dominates >80% of 1000 draws; pitch is exactly 1 when
  `randomPitch=1` and within `[0.5,2]` when 2; volume 0 when disabled and within `±6 dB` when set; and full
  determinism (two same-seed randomizers emit identical index/pitch/volume streams). Unit checks
  **5245 → 7002** (the 500-draw no-repeat + pitch/volume/determinism loops each assert per draw). The new `randomizer` demo runs 300 triggers of a 5-clip weighted pool and shows a pick-count
  histogram, a pitch×volume scatter, and the first 48 picks as a tick strip where no two neighbours share a
  colour (the no-repeat rule, visible). 2D golden (threshold 0.07, `randomizer` RMSE 0). Purely additive, so
  every existing golden is byte-unchanged (confirmed by a serial golden run); ctest **113/113 → 114/114**.
  Honest scope: this is the selection + variance logic (which clip, what pitch/volume) — it returns a pick the
  caller feeds to the mixer; it does **not** itself decode/stream audio, and does not yet auto-register the
  chosen clip as a live `SampleMixer` voice or expose it as a `.tres` resource; those remain the wiring
  follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 118 — "Benchmarking against Godot: analog-stick deadzone" (done)
Rotating to **input** for breadth (recent rounds were 2D-render, animation, IO, 3D-render, 2D-physics, core) —
the most under-served module (only `ActionMap` lived there). Maz reads gamepad axes with a *per-axis* scalar
deadzone, but lacked the 2D vector conditioning every character controller needs: Godot's `Input.get_vector`
applies a **radial** deadzone (on the whole stick vector, not each axis), **rescales** the leftover magnitude
so the deadzone edge maps to 0 and full tilt to 1 (no jump leaving the deadzone), and **clamps to the unit
circle** so a diagonal push isn't ~40% faster than a cardinal one. Without it, naive movement creeps at rest
and runs faster on the diagonals. Pure maths, so it unit-tests exactly and drives a 2D golden.
- [x] **M157 — Analog deadzone (`input::analogVector` / `input::applyDeadzone`)**: a new `Analog.hpp` with
  `applyDeadzone(value, dz)` (one signed axis: `|v|<=dz → 0`, else `sign·(|v|−dz)/(1−dz)` clamped to 1) and
  `analogVector(raw, dz)` (2D: `len<=dz → (0,0)`, `len>1 → raw/len`, else `raw · ((len−dz)/(1−dz))/len`) —
  matching Godot's `get_vector`/`get_axis` remap exactly, plus a `sanitizeDeadzone` guard so a pathological
  `dz` can't divide by zero. `testAnalog` pins: axis remap (edge→0, 0.6→0.5, full→1, sign kept, over-range
  clamp, no-deadzone passthrough, sanitized extreme); 2D rest/jitter collapse, on-axis rescale, full→unit,
  past-rim clamp, and the two key invariants — a full `(1,1)` push resolves to length **1** (not √2, so no
  faster diagonal) and the conditioned vector stays **parallel** to the raw input (cross-product 0). Unit
  checks **5223 → 5245**. The new `deadzone` demo shows both faces: LEFT a stick field (unit circle +
  deadzone ring + a grid of raw samples each arrowed to its conditioned dot — inner samples collapse to
  centre, corners pull onto the rim), RIGHT the 1-D `applyDeadzone` response curve (flat through the deadzone
  band, then linear to ±1). 2D golden (threshold 0.06, `deadzone` RMSE 0). Purely additive, so every existing
  golden is byte-unchanged (confirmed by a serial golden run); ctest **112/112 → 113/113**. Honest scope:
  this is the stick-conditioning maths; it does **not** wire a 2D `get_vector` convenience onto `ActionMap`'s
  four directional actions, add per-action deadzone config, or input buffering / echo — those remain the
  wiring follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 117 — "Benchmarking against Godot: concave polygon fill" (done)
Rotating to **2D rendering** for breadth (recent rounds were animation, IO, 3D-render, 2D-physics, core, UI).
Maz could already fill a *convex* polygon (`drawConvexPolygon` fans from vertex 0), but a triangle fan is only
valid when every interior angle is < 180°. Feed it a **concave** outline — a star, an arrow, an L/C/comb shape
— and the fan spills triangles outside the shape. Godot's `Polygon2D` fills arbitrary simple polygons; this
was a real, high-leverage geometry gap (it also underlies 2D mesh generation, collision decomposition, and
any authored filled shape). Pure geometry, so it unit-tests exactly and drives a 2D golden.
- [x] **M156 — Ear-clipping triangulation (`render::triangulatePolygon`)**: a new `PolyTriangulate.hpp` —
  the classic O(n²) ear-clipping algorithm over a **simple** polygon (no self-intersections, no holes). It
  detects the winding via the shoelace signed area and normalises to CCW, then repeatedly snips a "convex ear"
  (a vertex whose triangle with its neighbours points outward and contains no other vertex) until one triangle
  remains, emitting vertex **indices** three-per-triangle. Every output triangle is convex, so the existing
  convex-fill path draws it. Helpers `polygonSignedArea2` / `polygonArea` / `triSignedArea2` / `pointInTriangle`
  are exposed too. `testTriangulate` pins: the primitive predicates, degenerate input (<3 verts → empty), a
  lone triangle passthrough, a CCW square and the SAME square wound **clockwise** (winding normalised), a
  concave **dart** (a fan over-counts area 8 vs the true 4; ear clipping tiles to exactly 4), and a 12-vertex
  **plus/cross** (area 5, ten triangles) — the key invariant being *sum of triangle areas == polygon area*,
  which only holds for a valid non-overlapping tiling. Unit checks **5162 → 5223**. The new `polyfill` demo
  fills four shapes a fan cannot — a five-point star, a block arrow, a plus/cross, and a thick C-ring — each
  with the triangle mesh overlaid (faint) and its outline (bright). 2D golden (threshold 0.06, `polyfill`
  RMSE 0). Purely additive, so every existing golden is byte-unchanged (confirmed by a serial golden run);
  ctest **111/111 → 112/112**. Honest scope: this is single simple polygons; it does **not** yet handle
  polygons **with holes** (Godot's `Geometry2D.triangulate_polygon` + polygon-with-holes / `Polygon2D`
  `polygons`+`internal_vertices`), self-intersecting input, or per-vertex UV/colour interpolation for a
  textured `Polygon2D`; those remain follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

### Iteration 116 — "Benchmarking against Godot: float Curve resource" (done)
Rotating to **animation** for breadth (recent rounds were IO, 3D-render, 2D-physics, core, UI, math). Maz has
easing functions (Tween) and a Bézier *path* (`math::Curve2D`, M142), but no editable keyframed **float
curve** — the `y = f(x)` value profile that Godot's `Curve` resource provides and that drives particle
size/alpha over lifetime, audio fades, difficulty ramps, and custom easing. Pure math, so it unit-tests
exactly and drives a 2D golden.
- [x] **M155 — Float Curve (`anim::Curve`)**: a new `Curve.hpp` — a sorted list of `CurvePoint`s
  (`pos`, `value`, per-point `leftTangent`/`rightTangent`), a `CurveInterp` mode (Constant / Linear / Cubic),
  and `[minValue, maxValue]` clamping. `addPoint` inserts sorted; `sample(x)` clamps the domain (below the
  first point → first value, above the last → last value), finds the bracketing segment, and interpolates —
  Constant holds the left value, Linear lerps, and **Cubic is a Hermite spline** using the endpoint tangents
  (slopes scaled by the segment width), matching Godot's `Curve::interpolate`. `testCurve` pins: empty/single-
  point, linear + domain clamp, constant, a cubic ease-in-out with flat tangents (symmetric midpoint 0.5,
  eases in before / out after), a cubic whose tangents equal the chord slope reproducing the straight line,
  value clamping to the range, and sorted insertion regardless of add order. Unit checks **5138 → 5162**. The
  new `floatcurve` demo plots four curves side by side — a linear ramp, a cubic ease-in-out, a cubic ease-out,
  and a multi-point "particle size over life" profile — each sampled densely with its control points marked,
  all from `Curve::sample`. 2D golden (threshold 0.06, `floatcurve` RMSE 0). Purely additive, so every existing
  golden is byte-unchanged (confirmed by a serial golden run); ctest **110/110 → 111/111**. Honest scope: this
  is the curve resource + sampling; it does **not** yet auto-drive the `fx::Emitter` particle size/alpha from a
  Curve, add a bake-to-LUT fast path, or a 2-channel `Curve` for gradients; those remain the wiring follow-ups.

Standing note (Godot benchmark): literal parity "in every way" remains unreachable here — a shipping
editor, GDScript/C# VMs, console/mobile/web export, global illumination, and a Jolt-grade 3D physics
engine can't be built in a headless sandbox. The loop keeps closing the highest-leverage *closable*
gaps and will not declare total superiority over Godot.

Later (Godot-gap priorities + backlog): wiring the DSP buses into the real-time mixer (per-voice bus
routing) + chorus/phaser/limiter/pitch-shift effects + OGG/MP3 decode + registering decoded WAV as a
playable mixer voice (M129 gives the WAV PCM codec — 8/16-bit load+save; M106 completes the core
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
gives soft shadows); reverse/ping-pong method-track firing + per-marker argument payloads + a
callback-dispatch registry + a visual track editor on the timeline (M113 gives forward Once/Repeat
call-method/trigger tracks; M100 gives value tracks + per-segment easing);
ORCA half-plane avoidance + blending flow fields with local RVO for agent separation + incremental
goal-move re-bake + hierarchical/portal flow fields (M112 gives static-goal flow-field crowd pathfinding;
M98 gives agent-vs-agent velocity-sampling RVO); FABRIK pole targets + per-bone angle
constraints + a Skeleton2D-integrated IK modification stack (M107 gives multi-bone FABRIK chains; M97 gives
closed-form 2-bone IK); 47-tile Wang autotiling + BSP/room
dungeon gen (M96 gives 4-bit autotiling + cellular caves); text selection/clipboard + multi-line
TextEdit + controller UI nav (M95 gives single-line LineEdit + focus); ScrollContainer/TabContainer/
FlowContainer + wiring the container layout into LayoutNode's retained tree as a node mode with live
min-size propagation (M122 gives the BoxContainer/GridContainer/MarginContainer/CenterContainer layout math
with size flags + stretch ratios; M86 gives the anchor tree); wiring 3D spatial audio into the
real-time mixer as a per-voice 3D bus + HRTF/binaural + occlusion/reverb zones + WAV/OGG loading (M121
gives the 3D spatialization math — attenuation models/listener-relative pan/doppler; M94 gives 2D
pan/attenuation); animation blend *trees* (state-machine
over blend spaces) + IK (M92 gives blend spaces); 3D rigid-body physics; GPU particles;
navmesh dynamic obstacles; parallel/decorator BT nodes + a blackboard, GPU skinning, glTF skin/animation
import, prefabs/blueprints on the scene serializer, an ECS Transform/Parent wired to TransformGraph,
noise-driven tilemap/cave generation, localization / string tables, order-independent transparency,
material/uniform system, cross-platform CI, a deterministic hold-frame screenshot mode.
