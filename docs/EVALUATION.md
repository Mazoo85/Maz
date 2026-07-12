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

Later: box rotation (angular impulse) in Physics2D,
parallel/decorator BT nodes + a blackboard, GPU skinning, glTF skin/animation import, back
TextureStore/mesh loading with the cache, parallelize a hot loop, prefabs/blueprints on top of the
scene serializer, id-preserving load for entity-referencing components, action-map rebinding persisted
via config, an ECS Transform/Parent component wired to TransformGraph, dirty-flag caching for the
scene graph, wire the camera controller's shake to game::Shake in a real game, cooldowns/ability
timers on the scheduler, retrofit existing demos onto core::Random, localization / string tables, UI
layout / text input, order-independent transparency, material/uniform system, GPU-driven / indirect
instancing, cross-platform CI, a deterministic hold-frame screenshot mode, wire the profiler's
ScopedZone into an app's real frame loop.
