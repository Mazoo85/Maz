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
core/       Log, Assert, Time (fixed-timestep clock), Config/args, KeyValueStore (save/load),
            CVarRegistry (named typed tunables: bool/int/float/string + range clamp + string coercion),
            Profiler (hierarchical scoped CPU timing zones: inclusive + self time, EMA-smoothed),
            Scheduler (time-based timers: after/every/cancel) + Sequence (ordered wait/call/span script),
            Random (deterministic PRNG: xoshiro256** — ranges/chance/weighted/shuffle/gaussian),
            Noise (seeded Perlin gradient noise2 + fractal-Brownian-motion fbm2 for procgen),
            EventBus (type-safe publish/subscribe for decoupled systems),
            JobSystem (worker thread pool: submit/parallelFor for data-parallel work),
            ResourceCache (generic ref-counted, dedup-by-key asset cache),
            SceneStack (game-state stack: push/pop/replace + overlay-aware update/render)
              — zero dependencies beyond the standard library
platform/   Window, Input (keyboard/mouse/gamepad), event pump, prefPath   (depends on: core, SDL3)
input/      ActionMap — semantic action mapping: named button actions (any-of bound Key/MouseButton/
            PadButton sources, pressed/held/released edges) + axis actions (key pairs + analog pad axes,
            clamped -1..1); SDL-free (update() takes sampler callbacks)   (header-only)
math/       maz::math = GLM re-export + helpers     (header-only)
render/     Renderer (interface) + Vulkan backend   (depends on: core, platform, math, Vulkan)
              VulkanContext  — instance, device, queues, debug messenger
              VulkanSwapchain— swapchain + offscreen HDR scene pass (16-bit float, MSAA ≤4×
                               resolving to a sampled sceneColor) + composite pass to the swapchain
              BloomChain     — bright-pass + ½-res separable Gaussian blur of the HDR scene
              PostProcess    — fullscreen composite: HDR sceneColor + bloom -> swapchain, with
                               optional ACES tonemap/exposure
              TextureStore   — shared texture registry (one descriptor layout, used by 2D + 3D)
              VulkanBuffer/Texture, SpriteRenderer — batched textured 2D sprites + convex-polygon fill
                               (drawConvexPolygon: triangle-fan flat shapes via a 1×1 white texture) +
                               per-vertex-color gradient fans (drawPolygonFan: 2D light pools / shadows);
                               alpha + additive blend pipelines, batched per BlendMode (additive lights)
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
ui/         Font (TTF atlas: drawText/drawTextCentered/textWidth), DebugOverlay (FPS/draw stats),
            Context (immediate-mode widgets: panel/label/button/toggle/slider/textField, hot/active),
            LayoutNode (retained layout — Godot-style anchors/margins + HBox/VBox/Center containers,
              computed rects, resolution-responsive),
            TextField (single-line edit model: caret + insert/erase/move + max length) + FocusChain
              (ordered focusable ids, Tab/Shift+Tab wraparound) — Godot LineEdit + Control focus,
            ninePatch (StyleBox nine-slice: slice a dest rect into a 3×3 grid by border insets —
              fixed corners, stretching edges/center — mapping to source regions), Godot StyleBoxTexture-style,
            StyleBoxFlat + Theme (procedural rounded-corner panel — fill/border/per-corner radius/soft drop
              shadow via roundedRectPolygon + drawStyleBoxFlat layering shadow→border→fill; Theme names
              styles/colours per control class+state with type/state→type/normal→default fallback), Godot
              StyleBoxFlat/Theme-style,
            Rect (shared screen rectangle)
ecs/        World — entity-component system (sparse-set pools, each/view)   (header-only)
scene/      TransformGraph — 2D transform hierarchy: local pos/rot/scale per node + parent, update()
            propagates world transforms parent-first (decomposed TRS); localToWorld   (header-only)
game/       Tilemap, FlyCamera (first-person camera), Collision (AABB slide + ray/AABB queries),
            Shake (camera juice), SpatialGrid (uniform X/Z broadphase hash),
            NavGrid (8-directional A* grid pathfinding for moving AI),
            NavMesh (convex-cell navigation mesh: A* over cells + funnel string-pull for smooth paths),
            Steering (seek/flee/arrive/separation/path-follow forces + integrate),
            rvoVelocity (RVO local collision avoidance: reciprocal-velocity-obstacle candidate
              scoring on time-to-collision), Godot NavigationAgent2D-avoidance-style,
            StateMachine (generic FSM: enter/update/exit + guarded transitions),
            BehaviorTree (bt:: reactive Sequence/Selector/Inverter + Action/Condition leaves +
              Blackboard shared memory + Parallel composite + Repeater/AlwaysSucceed/AlwaysFail/Tap decorators),
            Goap (goap:: goal-oriented action planning — A* over a 64-bit-bitmask world state, Actions as
              precondition/effects/cost triples, returning the cheapest action sequence to a goal Condition;
              a planner beyond the behaviour tree, no Godot built-in equivalent),
            Physics2D (circle + box rigid bodies: gravity + impulse/friction collisions + stacking;
            opt-in oriented-box ROTATION: orientation + spin + moment of inertia, SAT contacts,
            rotational impulses about the contact point, linear/angular damping; JOINTS: Joint2D
            Pin (point constraint) + damped Spring + Groove/slider (a body pinned to a line, free to
            slide along it), sequential-impulse solved — Godot PinJoint2D/DampedSpringJoint2D/GrooveJoint2D;
            opt-in TWO-point contact manifolds (reference/incident-face clipping, solveManifolds) for
            torque-balanced stable box stacks),
            CameraController2D (2D follow camera: deadzone + smoothing + world-bounds clamp + shake),
            Visibility2D (angle-sweep visibility polygon for 2D lights + shadows: cast rays to occluder
            corners, keep nearest hits; point-in-polygon test),
            SoftShadow2D (area-light soft/penumbra shadows: diskSamples Vogel-spiral across the light +
            softVisibility = fraction of the disc a point can see), Godot Light2D-soft-shadow-style,
            CellularCave + autotileMask4 (seeded cellular-automata cave generation + 4-bit edge-mask
            tilemap autotiling — Godot TileMap terrains)
anim/       Tween — easing curves (15) + time-cursor (once/repeat/ping-pong) + generic sample;
            Timeline — keyframe sequencer: named Tracks of Keyframes (time→value + per-segment easing) +
              a once/repeat/ping-pong playhead, Godot AnimationPlayer-style;
            SpriteAnim — sprite-sheet flipbook playback (gridFrames + fps-timed loop/one-shot);
            Skeleton — joint hierarchy + bind/inverse-bind + skinning matrices for mesh deformation;
            AnimClip — per-joint TRS keyframe tracks: sample (lerp/slerp) + loop + blendPoses +
              blendPosesWeighted (N-way weighted pose mix);
            Animator — named-clip library + timed cross-fade controller (play/update/pose);
            BlendSpace1D/2D — blend animations by a 1-D/2-D parameter (linear / barycentric-over-
              triangulation weights), Godot AnimationTree-style;
            AnimStateMachine — named states + cross-fading transitions (fade + condition + travel);
              active() returns weighted states (blend-space-shaped), so states compose with blend spaces,
              Godot AnimationNodeStateMachine-style;
            solveTwoBoneIK — 2-bone inverse kinematics (law-of-cosines elbow solve + bend select +
              straight-arm overreach), Godot SkeletonModification2DTwoBoneIK-style;
            solveFabrik — multi-bone FABRIK IK (backward/forward reaching over an N-joint chain, bone
              lengths preserved), Godot SkeletonModification2DFABRIK-style
            (header-only; animate any float/vector/color, a sprite through frames, or a skinned mesh)
io/         Serialize — ByteWriter/ByteReader (POD/string/vector, versioned headers, bounds-checked)
            + file read/write   (header-only; save games, level files);
            Json — JsonValue (null/bool/number/string/array/object, insertion-ordered) + never-throwing
            recursive-descent parseJson (line/col errors) + dump (compact/pretty) + file IO
            (readTextFile/writeTextFile, parseJsonFile/writeJsonFile)  (header-only;
            human-editable configs, data-driven scenes/levels/tuning);
            Config — the JSON<->CVarRegistry bridge (loadConfig/configToJson + file convenience), so
            core stays zero-dependency while apps get "config.json drives the engine";
            SceneSerializer — reflection-lite ECS save/load: register per-component JSON converters,
            then saveWorld/loadWorld a live ecs::World to/from JSON (save games, prefabs, editor)
fx/         ParticleSystem — pooled 2D particles   (on top of Renderer)
audio/      Audio — SDL3 device + real-time STEREO synth mixer (SFX + music, per-voice L/R pan);
            Spatial2D — 2D positional audio math (listener/source distance attenuation + constant-power
            stereo pan → per-channel gain), Godot AudioStreamPlayer2D-style;
            Dsp — DSP effects + mix buses (Biquad RBJ low/high/band-pass + Delay feedback echo +
            Reverb Schroeder/Freeverb (comb+allpass) + Distortion tanh waveshaper + Compressor + Bus
            ordered effect chain), Godot AudioEffectFilter/Delay/Reverb/Distortion/Compressor-style  (depends on: core, SDL3)
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
  catcher/  CATCHER — a full 2D game wiring the engine's own systems together: SceneStack
              (menu/play/game-over), EventBus (catch/miss -> score + particle burst + shake),
              2D contact tests (paddle vs falling coins/hazards), ParticleSystem, Shake, and a
              KeyValueStore high score; deterministic attract-mode AI so the render is golden-stable
  data/     Data-driven scene — an embedded JSON document (clear color + sprites: shape/pos/size/
              tint/bob/spin) parsed at runtime with io::parseJson and rendered; nothing hard-coded
  level/    On-disk JSON level — reads assets/levels/arena.json from disk (io::parseJsonFile) into a
              game::Tilemap + pickups + palette, renders top-down, and round-trips the level back to
              the save dir (io::writeJsonFile); the editable-content pipeline end to end
  config/   CVar/config demo — registers typed tunables, applies a JSON config (io::loadConfig), and
              renders a scene driven entirely by cvars (orb count/speed/hue/brightness/grid) + a live
              cvar table
  profiler/ CPU profiler view — feeds a fixed synthetic frame into core::Profiler and draws the zone
              tree as an indented bar chart (inclusive vs self ms per nested zone)
  ecsave/   ECS save/load — builds an entity world, serializes it to JSON (io::SceneSerializer),
              reloads that JSON into a fresh world, and renders the reload (proves the round-trip)
  actions/  Input action map — an avatar driven by named actions (MoveX/MoveY axes, Fire/Dash buttons)
              bound to keyboard + gamepad; deterministic scripted self-play OR'd with real input
  solar/    Transform hierarchy — a solar system (sun -> planets -> moons) from a scene::TransformGraph;
              only pivot rotations are set, update() sweeps planets around the sun and moons around planets
  camera/   2D follow camera — a large world + moving avatar; game::CameraController2D tracks it with a
              deadzone, smoothing, and world-bounds clamp (world-space pass + pixel-space HUD)
  fireworks/Scheduler demo — core::Scheduler timers spawn rockets (every) that each explode after a
              delay (after) into particle bursts; a looping core::Sequence pulses the title glow
  scatter/  Procedural RNG demo — a seeded core::Random scatters a token field (uniform-in-disc), each
              token's rarity chosen by weighted(); a legend tallies the resulting distribution
  noise/    Procedural terrain — a heightmap texture generated from core::Noise fbm2, colored by a
              water/sand/grass/forest/rock/snow ramp with a slope hillshade; same seed, same continent
  uilayout/ Retained UI layout — a responsive app UI (top bar + sidebar VBox of buttons + content panel
              + centered modal) laid out entirely by ui::LayoutNode anchors + containers, no fixed pixels
  navmesh/  Navigation mesh — a room with a pillar as convex cells; game::NavMesh A*+funnel string-pulls
              a smooth path that hugs the pillar's corner (polygon nav, beyond grid A*)
  vectors/  Filled polygons — regular N-gons, a 64-gon "circle", and overlapping translucent triangles
              via Renderer::drawConvexPolygon (Godot Polygon2D-style vector shapes, alpha-composited)
  lights2d/ 2D lights + shadows — a dark room lit by three colored lights, each a visibility polygon
              (game::Visibility2D) rendered as an ADDITIVE gradient fan (overlaps brighten), with
              solid boxes casting real shadows
  tumble/   2D rigid-body rotation — tilted rectangles dropped into a bin tumble on their corners and
              settle into a leaning pile (Body2D::enableRotation + the oriented PhysicsWorld2D solver)
  blendspace/ Animation blend space — a grid of stick-figure skeletons whose pose is blended across a
              2D parameter space from four corner poses (anim::BlendSpace2D + blendPosesWeighted)
  joints/   Physics joints — a pin-jointed rope bridge sagging into a catenary + damped-spring-hung
              masses of increasing stiffness (game::Joint2D Pin + Spring)
  spatial2d/ Positional audio — a listener + sound sources with per-source distance attenuation + stereo
              pan visualized as gain halos + L/R bars + a master meter (audio::spatialize)
  form/     UI text input — an editable account-settings form: click/Tab to focus a field (accent
              border + caret), type to edit (ui::TextField + ui::FocusChain + Context::textField)
  cave/     Procedural cave — a seeded cellular-automata cavern with autotiled wall borders (walls inset
              per their edge bitmask) (game::CellularCave + game::autotileMask4)
  reach/    Inverse kinematics — a grid of 2-bone arms whose elbows are solved so each hand reaches its
              target (out-of-reach targets shown extended) (anim::solveTwoBoneIK)
  avoid/    RVO local avoidance — 14 agents crossing a circle to antipodal goals, their trails bulging
              around the crowded centre as reciprocal velocity obstacles route them apart (game::rvoVelocity)
  bus/      Audio DSP buses — one plucked-sawtooth note scoped as four stacked waveforms (source,
              low-pass, high-pass, low-pass→delay bus) (audio::Biquad / audio::Delay / audio::Bus)
  reverb/   Reverb/distortion/compressor — one note (loud + quiet) scoped through a Schroeder reverb, a
              tanh distortion, and a compressor as stacked waveforms (audio::Reverb/Distortion/Compressor)
  tentacle/ Multi-bone FABRIK IK — a row of 8-bone chains reaching for targets; reachable ones curl to
              touch (green), out-of-reach ones straighten and point (red) (anim::solveFabrik)
  timeline/ Keyframe timeline — an arrow driven by keyed x/y/rotation/scale/colour tracks, shown as an
              onion-skin trail plus an editor track panel with keyframe dots + a playhead (anim::Timeline)
  softshadow/ Soft 2D shadows — the same box+light drawn hard (point light) vs soft (area light, 24
              samples) so the shadow edge feathers into a penumbra (game::SoftShadow2D)
  groove/   Groove/slider joints — three boxes pinned to tilted rails, each sliding down its incline (not
              straight down) and settling against a stop (game::Joint2D::Groove)
  stylebox/ Nine-patch StyleBox — differently-sized themed panels + a button row from one style; fixed
              corners, stretching edges/center (ui::ninePatch)
  theme/    StyleBoxFlat + Theme — a dark theme drawing a button in each state (normal/hover/pressed/
              disabled) + a gallery of rounded/bordered/shadowed/pill/tab panels (ui::StyleBoxFlat + Theme)
  stack/    Stable box stacks — two identical five-box towers dropped side by side; two-point manifolds on
              keeps one square, off lets the other topple (PhysicsWorld2D::solveManifolds)
  blackboard/ Behavior-tree blackboard — a sentry's tree drawn twice (patrol vs engage), each node
              coloured by live per-tick status as one blackboard flag flips the branch (game::bt)
  statemachine/ Animation state machine — a locomotion machine's active-state weights as stacked cross-
              fading colour bands over a scripted timeline (anim::AnimStateMachine)
  goap/     GOAP planner — a survival agent plans "make fire" from an action library; the optimal plan is
              drawn as a flow with the world-state changing fact-by-fact until fire lights (game::goap)
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
