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
            Interpolated<T> + interpolate (render interpolation — Godot physics interpolation: a
              previous/current pair blended by Clock::interpolationAlpha so motion stays smooth between
              fixed steps; lerpAngle shortest-arc + a Transform2DState pose blend),
            Noise (seeded Perlin gradient noise2 + fractal-Brownian-motion fbm2 for procgen),
            EventBus (type-safe publish/subscribe for decoupled systems),
            Signal (per-object named channels — Godot signal/connect/emit: typed Signal<Args...> with
              connect/disconnect/isConnected, immediate emit, one-shot + deferred(+flushDeferred) connections),
            StringId (interned strings — Godot StringName: StringTable intern/find/str/hash dedups each
              unique name to a stable 32-bit id so name equality is an int compare; + FNV-1a-32 hash),
            SlotMap (generational-handle object pool — Godot RID: insert->SlotHandle{index,generation},
              get/contains/erase with stale-handle (ABA) detection via generation bump on free + free-list
              slot reuse; forEach over live values),
            JobSystem (worker thread pool: submit/parallelFor for data-parallel work),
            ResourceCache (generic ref-counted, dedup-by-key asset cache),
            SceneStack (game-state stack: push/pop/replace + overlay-aware update/render)
              — zero dependencies beyond the standard library
platform/   Window, Input (keyboard/mouse/gamepad), event pump, prefPath   (depends on: core, SDL3)
input/      ActionMap — semantic action mapping: named button actions (any-of bound Key/MouseButton/
            PadButton sources, pressed/held/released edges) + axis actions (key pairs + analog pad axes,
            clamped -1..1); SDL-free (update() takes sampler callbacks)   (header-only)
            Analog    — stick conditioning (Godot Input.get_vector/get_axis): applyDeadzone (1 signed
            axis, rescaled) + analogVector (2D radial deadzone + rescale + unit-circle clamp — no drift,
            no faster diagonals); stateless, header-only
math/       maz::math = GLM re-export + helpers (Vulkan-correct perspective + orthographic/orthographicSize
              3D projections (parallel, isometric — Godot Camera3D Orthogonal) + ortho2D)  (header-only);
            Curve2D — cubic Bézier path (Godot Curve2D/Path2D): points with in/out handles, sample/tangent/
              length + arc-length bake -> sampleBaked(distance) for constant-speed travel;
            Rect2 — axis-aligned rectangle (Godot Rect2): position+size with hasPoint (min-incl/max-excl),
              intersects/intersection (clip), merge (union), encloses, grow/growIndividual, expand, abs;
            Geometry2D — 2D geometry queries (Godot Geometry2D): segmentIntersect (segment×segment→point+t/u),
              closestPointOnSegment / distanceToSegment, pointInPolygon (even-odd, concave-safe),
              segmentIntersectsCircle
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
              shapes         — procedural box / sphere / plane geometry, + Shapes3D (makeCylinder /
                               makeCone / makeTorus / makeCapsule: outward-normal + UV solids toward
                               Godot CylinderMesh/CapsuleMesh/TorusMesh; additive, box/sphere/plane untouched)
              buildPolyline  — 2D polyline stroking (Godot Line2D): thicken a point list to a ribbon of a
                               given width with Miter/Bevel/Round joints + None/Box/Round caps + closed
                               loops → a triangle soup for drawConvexPolygon (header-only, no GPU dep)
              MultiMesh2D    — 2D multi-mesh instancing (Godot MultiMeshInstance2D): one convex base polygon +
                               a per-instance buffer (Instance2D pos/rot/scale/colour); transformInstance TRS,
                               transformedPolygon(i) world polygon, bakeTriangles() one soup (header-only, no GPU dep)
              triangulatePolygon — concave polygon fill via ear clipping (Godot Polygon2D): tiles an arbitrary
                               simple polygon into triangles (vertex indices), winding-normalised; a fan is convex-
                               only. polygonArea/triSignedArea2/pointInTriangle helpers (header-only, no GPU dep)
              buildBillboard — billboard model matrix (Godot SpriteBase3D/GeometryInstance3D): orient a quad
                               toward the camera from the view matrix — Disabled/Enabled(full)/YBillboard(upright)
                               (header-only, no GPU dep)
              buildGrid      — 3D editor reference geometry (Godot Node3D viewport): an XZ-plane ground grid
                               (buildGrid) + the X=red/Y=green/Z=blue origin gizmo, and buildWireBox (12 edges
                               of a placeable AABB) — colored Line3 lists drawn via DebugDraw/drawLine (no GPU dep)
              Camera3D       — screen<->world projection (Godot Camera3D): view+proj+viewport -> worldToScreen
                               (unproject_position), screenToRay (project_ray_origin/normal), screenToWorld
                               (project_position), frustum()/isPointVisible/isSphereVisible (Gribb-Hartmann 6
                               planes) — 3D mouse picking, world-space labels, aim rays (header-only, no GPU dep)
              loadGltf       — glTF 2.0 model import (cgltf) -> ModelData (mesh + base-color + normal map)
              loadGltfScene  — glTF 2.0 scene import -> SceneData (per-node mesh + transform + textures)
              Renderer       — beginFrame / drawSprite / drawMesh / endFrame
ui/         Font (TTF atlas: drawText/drawTextCentered/textWidth), DebugOverlay (FPS/draw stats),
            Context (immediate-mode widgets: panel/label/button/toggle/slider/textField, hot/active),
            LayoutNode (retained layout — Godot-style anchors/margins + HBox/VBox/Center containers,
              computed rects, resolution-responsive),
            Container (auto-layout — Godot's BoxContainer/GridContainer/MarginContainer/CenterContainer:
              per-axis SizeFlag Fill/Expand/ShrinkBegin/Center/End + stretch ratios; hbox/vbox/grid/margin/
              center write child rects; hboxMinSize/vboxMinSize/gridMinSize for bottom-up sizing),
            TextField (single-line edit model: caret + insert/erase/move + max length) + FocusChain
              (ordered focusable ids, Tab/Shift+Tab wraparound) — Godot LineEdit + Control focus,
            layoutText (word-wrap + alignment — Godot Label autowrap: greedy-wrap a paragraph to a max
              width via an injected measure callback, honor \n hard breaks, align each line L/C/R →
              positioned TextLine list; renderer-independent, no Font dependency),
            ninePatch (StyleBox nine-slice: slice a dest rect into a 3×3 grid by border insets —
              fixed corners, stretching edges/center — mapping to source regions), Godot StyleBoxTexture-style,
            StyleBoxFlat + Theme (procedural rounded-corner panel — fill/border/per-corner radius/soft drop
              shadow via roundedRectPolygon + drawStyleBoxFlat layering shadow→border→fill; Theme names
              styles/colours per control class+state with type/state→type/normal→default fallback), Godot
              StyleBoxFlat/Theme-style,
            Range + ProgressBar (clamped/stepped scalar value model — min/max/step/page → a 0..1 ratio,
              setRatio/step_ + allow-greater/lesser; ProgressBar exposes fillFraction/percent), the Godot
              Range base behind ProgressBar/HSlider/ScrollBar/SpinBox,
            Tree + TreeItem (hierarchical collapsible rows: heap-owned children + a collapsed flag,
              visibleRows() flattens expanded items depth-first into rows with depth + hasChildren), Godot
              Tree-control-style (scene dock / inspector / file browser),
            ItemList (scrollable box of selectable rows: text/id + selectable/disabled flags, Single/Multi
              select, fixed row height + separation + clamped scroll → itemRect/itemAtPoint (gap-aware) /
              ensureVisible / visibleRange, selectNext/Previous keyboard nav skipping disabled), Godot
              ItemList-style (file lists / inventory / level-select),
            parseBBCode + RichSpan (BBCode rich text → resolved styled runs: [b]/[i]/[u], [color=hex|name],
              [size=N], nested/lenient — unclosed-to-end, stray-close ignored, [lb]/[rb] literal, unknown-tag
              passthrough; stripBBCode → plain text; renderer-independent), Godot RichTextLabel-style,
            Rect (shared screen rectangle)
ecs/        World — entity-component system (sparse-set pools, each/view)   (header-only)
scene/      TransformGraph — 2D transform hierarchy: local pos/rot/scale per node + parent, update()
            propagates world transforms parent-first (decomposed TRS); localToWorld;
            Prefab — prefabs / instancing (Godot PackedScene): a PrefabNode tree of named nodes with an
            exported PropBag (PropValue tagged union: Float/Int/Bool/Vec2/Color/Text); instantiate(prefab,
            overrides) deep-copies the tree and applies per-node-path overrides → an independent instance;
            GroupRegistry — node groups (Godot SceneTree add_to_group/get_nodes_in_group/call_group): tag any
              integer node id into named groups (unique, insertion-ordered) with a reverse node→groups index;
              nodesInGroup/isInGroup/groupsOf/removeNode + call(group, fn) broadcasting over a snapshot so the
              callback may add/free members mid-walk
            (header-only)
game/       Tilemap, FlyCamera (first-person camera), Collision (AABB slide + ray/AABB queries),
            CollisionLayers (32-bit layer/mask filtering: directional detects() + symmetric interact() +
              CollisionObject2D + a named-layer LayerRegistry), Godot collision_layer/collision_mask-style,
            Shake (camera juice), SpatialGrid (uniform X/Z broadphase hash),
            NavGrid (8-directional A* grid pathfinding for moving AI),
            NavMesh (convex-cell navigation mesh: A* over cells + funnel string-pull for smooth paths),
            FlowField (flow-field / vector-field pathfinding: one Dijkstra from the goal → an integration
            cost field + a baked per-cell flow direction, so a whole crowd routes to a shared goal from a
            single search — the crowd technique per-agent A* lacks),
            GravityArea2D + gravityAt (area gravity fields — Godot Area2D gravity override: a Rect2 zone that
              overrides gravity inside it, Directional (wind/updraft) or Point (inverse-square pull), combined
              by priority with Replace/Add modes),
            KinematicBody2D (moveAndSlide + sweptAabb — Godot CharacterBody2D.move_and_slide: velocity-driven
              AABB swept against static AABBs, slides along contacts over several iterations, floor/wall/ceiling
              classification → SlideResult),
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
            PhysicsQuery2D (2D physics-space queries — Godot PhysicsDirectSpaceState2D: queryRay/querySegment
            return the nearest hit (t/point/normal/id) among circle+oriented-box shapes filtered by a
            collision mask; queryPoint/pointInShape = intersect_point for mouse picking — pure geometry,
            no sim step),
            ConvexShape2D (arbitrary convex-polygon collision via SAT — Godot ConvexPolygonShape2D:
            satOverlap returns overlap + the minimum-translation vector (axis+depth), polyContains =
            point-in-poly, makeRegularPoly/makeBoxPoly builders),
            CameraController2D (2D follow camera: deadzone + smoothing + world-bounds clamp + shake),
            Visibility2D (angle-sweep visibility polygon for 2D lights + shadows: cast rays to occluder
            corners, keep nearest hits; point-in-polygon test),
            NormalLight2D (normal-mapped 2D lighting — PointLight2D shaded by a 3D N·L Lambert term with the
            light at a height above the plane + smooth distance falloff + ambient; shadeSurface sums lights,
            decodeNormal unpacks a normal-map texel), Godot Light2D normal-map-style,
            Parallax (scrolling backgrounds — ParallaxLayer motionScale/motionOffset/mirroring; layerOffset
            scrolls a layer by its motion scale, firstTile/tileCount/pmod tile a mirrored layer across the
            viewport), Godot ParallaxBackground/ParallaxLayer-style,
            OneWayPlatform2D + resolveOneWayPlatform(s) (one-way platforms — a ledge solid only from above;
              a swept resolve lands a body that crosses the surface from above while descending and passes a
              body launched from below straight through; resolveOneWayPlatforms picks the topmost landing),
              Godot one_way_collision-style,
            SoftShadow2D (area-light soft/penumbra shadows: diskSamples Vogel-spiral across the light +
            softVisibility = fraction of the disc a point can see), Godot Light2D-soft-shadow-style,
            CellularCave + autotileMask4 (seeded cellular-automata cave generation + 4-bit edge-mask
            tilemap autotiling — Godot TileMap terrains),
            TileSet (per-tile resource: TileDef maps a tile id to an atlas source cell + a None/Full/Box
              sub-cell collision; collectSolids/solidAt/dropY turn a Tilemap+TileSet into world collision
              boxes, a point-solidity test, and a drop-to-ground helper), Godot TileMap/TileSet-style,
            Area2D + AreaMonitor (sensor / trigger regions: a circle-or-box zone that detects overlap
              without applying force — overlaps() covers circle-circle, box-box AABB, and mixed circle-box
              via closest-point; AreaMonitor diffs each frame's overlapping set to emit enter/exit events),
              Godot Area2D-monitoring-style)
anim/       Tween — easing curves (15) + time-cursor (once/repeat/ping-pong) + generic sample;
            TweenPlayer — tween sequencer / property animator (Godot SceneTreeTween): chains Property/
              Interval/Callback tweeners into sequential groups (parallel within a group), loops, and writes
              bound float setters every update(dt);
            Timeline — keyframe sequencer: named Tracks of Keyframes (time→value + per-segment easing) +
              a once/repeat/ping-pong playhead, Godot AnimationPlayer-style;
            TriggerTrack + MethodTimeline — call-method / trigger tracks: timed markers that FIRE as a
              playhead sweeps (fire-once half-open, loop-wrap-safe), the event half of AnimationPlayer;
            SpriteAnim — sprite-sheet flipbook playback (gridFrames + fps-timed loop/one-shot);
            Skeleton — joint hierarchy + bind/inverse-bind + skinning matrices for mesh deformation;
            AnimClip — per-joint TRS keyframe tracks: sample (lerp/slerp) + loop + blendPoses +
              blendPosesWeighted (N-way weighted pose mix);
            additiveBlend — additive/layered pose blending (Godot AnimationNodeAdd2): makeAdditiveDelta
              (additive vs reference: translation subtract / rotation inverse(ref)*add / scale ratio) +
              applyAdditiveDelta (layer on a base at a weight; zero delta leaves the base untouched);
            Animator — named-clip library + timed cross-fade controller (play/update/pose);
            BlendSpace1D/2D — blend animations by a 1-D/2-D parameter (linear / barycentric-over-
              triangulation weights), Godot AnimationTree-style;
            AnimStateMachine — named states + cross-fading transitions (fade + condition + travel);
              active() returns weighted states (blend-space-shaped), so states compose with blend spaces,
              Godot AnimationNodeStateMachine-style;
            BlendTree — a node graph nesting the above: Input leaves + Blend2 (cross-fade) / Add2
              (additive layer) / BlendSpace1 interior nodes, each driven by a named blend parameter,
              evaluated recursively into one pose, Godot AnimationNodeBlendTree-style;
            solveTwoBoneIK — 2-bone inverse kinematics (law-of-cosines elbow solve + bend select +
              straight-arm overreach), Godot SkeletonModification2DTwoBoneIK-style;
            solveFabrik — multi-bone FABRIK IK (backward/forward reaching over an N-joint chain, bone
              lengths preserved), Godot SkeletonModification2DFABRIK-style;
            Curve — float-curve resource (Godot Curve): keyframed y=f(x) with per-point tangents +
              Constant/Linear/Cubic-Hermite modes + range clamp, sample(x) — particle size/alpha over life,
              fades, custom easing (distinct from math::Curve2D, a Bézier path);
            Gradient — colour-ramp resource (Godot Gradient): sorted (offset, Color) stops sampled over [0,1]
              with Constant/Linear/Cubic (Catmull-Rom, clamped) modes, setOffset re-sort, bake(N) → an
              N-colour ramp (GradientTexture1D) — particle colour-over-life, sky/health/heat tints;
            RootMotionTrack — root motion (cumulative clip-local position+heading; delta with loop-seam sum;
              advance() applies a step to a world pose, rotating clip-local travel by the current facing so
              feet don't slide), Godot AnimationMixer root-motion-track-style
            (header-only; animate any float/vector/color, a sprite through frames, or a skinned mesh)
io/         Base64 — base64Encode/base64Decode (RFC 4648 raw<->text — Godot Marshalls; whitespace-tolerant,
              validating decode) for embedding binary in JSON/.tres/URLs;
            Serialize — ByteWriter/ByteReader (POD/string/vector, versioned headers, bounds-checked)
            + file read/write   (header-only; save games, level files);
            Json — JsonValue (null/bool/number/string/array/object, insertion-ordered) + never-throwing
            recursive-descent parseJson (line/col errors) + dump (compact/pretty) + file IO
            (readTextFile/writeTextFile, parseJsonFile/writeJsonFile)  (header-only;
            human-editable configs, data-driven scenes/levels/tuning);
            Config — the JSON<->CVarRegistry bridge (loadConfig/configToJson + file convenience), so
            core stays zero-dependency while apps get "config.json drives the engine";
            SceneSerializer — reflection-lite ECS save/load: register per-component JSON converters,
            then saveWorld/loadWorld a live ecs::World to/from JSON (save games, prefabs, editor)
            PrefabText — savePrefabText/loadPrefabText: round-trip a scene::Prefab tree to Godot-.tscn-style
              text ([node name/parent] sections + typed key = TYPE value lines), diffable + version-control-
              friendly (Godot .tscn/.tres)
            Localization — parseCsv (RFC-4180: quoted fields, embedded commas/newlines, "" escapes, CRLF/LF)
              + TranslationTable (Godot Translation CSV: key + per-locale columns; setLocale + tr(key) with
              empty-cell→source and unknown-key→key fallback);
            ResourcePack — a .pck-style archive (Godot PackedData): packResources bundles named blobs into one
              byte stream (magic+version header, (path, offset, size) directory, concatenated data);
              ResourcePack::load reads them back by path (contains/get/getString/paths/count) with bounds-
              checked, fail-clean parsing (store-only, no compression yet)
fx/         ParticleSystem — pooled 2D particles   (on top of Renderer);
            Emitter — a particle emitter RESOURCE (Godot CPUParticles2D): emission shape (point/disk/
              ring/rect) + per-lifetime scale/alpha Curve + multi-stop colour Gradient + direction/spread/
              speed/gravity/explosiveness; simulate(seed,t) returns every live particle deterministically
audio/      Audio — SDL3 device + real-time STEREO synth mixer (SFX + music, per-voice L/R pan);
            Wav — RIFF/WAVE PCM codec: decodeWav (8-bit unsigned + 16-bit signed → float samples) +
              encodeWav (float → 16-bit .wav bytes), byte-in/out, Godot AudioStreamWAV-style;
            SampleMixer — offline sample-playback mixer: plays decoded WavData clips as voices
              (play(gain/pan/loop/speed)→id, stop/activeVoices/clear) and mix()es them into an
              interleaved-stereo buffer (linear-interpolated resample/pitch, auto-stop/loop-wrap),
              Godot AudioStreamPlayer-over-AudioStreamWAV-style;
            Spatial2D — 2D positional audio math (listener/source distance attenuation + constant-power
            stereo pan → per-channel gain), Godot AudioStreamPlayer2D-style;
            Spatial3D — 3D positional audio math (Listener3D forward/up basis + Source3D; four
            attenuation models None/Linear/Inverse/InverseSquare + listener-orientation-relative pan +
            doppler pitch → left/right/pitch SpatialMix), Godot AudioStreamPlayer3D-style;
            Dsp — DSP effects + mix buses (Biquad RBJ low/high/band-pass + Delay feedback echo +
            Reverb Schroeder/Freeverb (comb+allpass) + Distortion tanh waveshaper + Compressor + Bus
            ordered effect chain + Lfo/fracTap + Chorus (multi-voice detuned modulated delay) + Flanger
            (swept feedback comb) + Phaser (swept all-pass cascade)), Godot
            AudioEffectFilter/Delay/Reverb/Distortion/Compressor/Chorus/Phaser-style;
            Envelope — ADSR (attack/decay/sustain/release amplitude contour as a note-on/off gated state
            machine, process(dt)→level), the shape every synth voice is multiplied by;
            Randomizer — StreamRandomizer: weighted clip pool + Random/RandomNoRepeat/Sequential pick
            modes + per-trigger pitch (log-symmetric) & volume (dB) jitter → RandomPick, seeded/
            deterministic, Godot AudioStreamRandomizer-style  (depends on: core, SDL3)
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
  emitter/  Particle emitter resource — a gravity fountain (point), an omnidirectional ring burst, and
              angled rect rain, each an authored fx::Emitter simulated deterministically (fx::simulate)
  spatial3d/ 3D positional audio — a top-down radar of six sources around one listener; each disc sized
              by distance attenuation, tinted by doppler pitch, with a velocity arrow + L/R stereo meter
              (audio::computeSpatialMix)
  containers/ Auto-layout containers — four cards showing HBox (Fill/Expand stretch ratios), a 3-column
              Grid, a VBox (header/body-Expand/footer), and a MarginContainer framing a CenterContainer
              (ui::hbox/vbox/grid/margin/center)
  choreo/    Tween choreography — five property-tween sequences (sequential/parallel/delay/loop/bounce)
              snapshotted at a fixed time, each dot placed by its anim::TweenPlayer through a bound setter
  prefab/    Prefabs / instancing — one turret prefab (chassis>turret>barrel) instanced six times with
              per-node overrides (body/turret colour, barrel length, body width) via scene::instantiate
  signals/   Named signals — a Button.pressed → Player.hpChanged → Player.died wiring graph (connectors via
              render::buildPolyline) + an event log showing immediate/one-shot/deferred (core::Signal)
  wav/       WAV load/save — synthesize → encodeWav → decodeWav → draw the reconstructed waveform as an
              oscilloscope beside the parsed RIFF/WAVE header (audio::Wav)
  grid3d/    3D editor viewport reference — a fixed camera over an XZ ground grid + the RGB origin gizmo +
              a wireframe box on the floor, all built by render::buildGrid / buildWireBox and drawn via drawLine
  rayquery/  2D physics queries — a muzzle fans hitscan rays that pass through a glass layer and stop on the
              first solid circle/box (game::queryRay, mask-filtered), with contact normals + a point-picked shape
  strtable/  String interning — a stream of repeated tag references interned into a core::StringTable, shown as
              the reference stream (name -> #id) beside the deduplicated pool (id -> text -> FNV hash)
  locale/    CSV localization — one translation CSV renders the same game menu in four languages side by side
              (io::TranslationTable::tr per locale), with the empty German QUIT cell falling back to English
  textwrap/  Text layout — one prose paragraph fit into three fixed-width panels (left/center/right aligned)
              via ui::layoutText measured with Font::textWidth, plus a \n-delimited quest log with hard breaks
  addblend/  Additive pose blending — a 3-joint arm layers an elbow-bend additive onto a fixed base pose at
              five rising weights (anim::additiveBlend + Skeleton FK); shoulder held, only the elbow folds
  primitives/ Mesh primitives — a lit gallery of the four new procedural solids (cylinder/cone/torus/capsule
              from render::shapes::make*) under a fixed camera on the existing 3D mesh path
  slotmap/   Generational handles — a live core::SlotMap<char> driven through insert/free/reuse; the slot
              array (occupied/free + generation) beside the handle table showing which handles are live vs stale
  polycollide/ Convex polygon collision — a probe pentagon tested (game::satOverlap) against a ring of convex
              shapes; overlaps drawn red with the MTV push-arrow, clear shapes green
  sampler/   Sample-playback mixer — two decoded WAV clips (a tone + a noise blip) played as audio::SampleMixer
              voices panned L/R, mixed offline into one stereo buffer drawn as L/R oscilloscopes
  respack/   Resource pack — four resources (level JSON, text, a synthesized WAV, a raw blob) bundled into one
              io::ResourcePack archive, loaded back, and shown as a directory table + header hex + round-trip check
  richtext/  BBCode rich text — six BBCode source strings (bold/italic/underline, hex+named colours, three
              sizes, nesting, literal/unknown-tag passthrough) each shown above its ui::parseBBCode-formatted result
  curve/     Cubic Bézier path — a math::Curve2D wavy path drawn as a smooth spline with its control points +
              handles, arc-length-baked constant-speed dots (green), and a traveller with its tangent arrow
  multimesh/ 2D multi-mesh — one dart base shape stamped 540× through a render::MultiMesh2D as a colour-swirled
              spiral field (per-instance rotation/scale/colour), demonstrating one-shape-many-instances
  billboard/ 3D billboard modes — three rows of flat cards (Enabled/YBillboard/Disabled via render::buildBillboard)
              under an elevated camera, so the full-facing / stays-upright / fixed modes differ visibly
  oneway/    One-way platforms — three balls drop onto solid-from-above ledges and rest on top while a fourth is
              launched up through one (its trail crosses the bar), via game::resolveOneWayPlatforms
  modfx/     Chorus / flanger / phaser — one sustained note scoped through the three LFO-swept modulated-delay
              effects as stacked waveforms (audio::Chorus/Flanger/Phaser)
  rootmotion/ Root motion — a walk clip drives a character along a swept arc with left/right footprints planted
              on the path (no foot sliding), via anim::RootMotionTrack::advance
  groups/    Node groups — a 6x6 grid of tagged nodes; nodesInGroup("vip") rings one diagonal and
              call("hazard", ...) stamps the other, with live group-size counts (scene::GroupRegistry)
  rects/     Rect2 geometry — overlapping rectangles with their intersection (clip) filled, the union (merge)
              outlined, a grow() halo, and hasPoint probe dots, all from math::Rect2
  progress/  Range / ProgressBar — six bars (plain fills, a ratio-tinted health bar, a custom-range mana bar,
              a step-snapped bar) drawn from ui::ProgressBar::fillFraction/percent
  interp/    Render interpolation — four motions (translate/rotate/scale/combined) showing previous+current
              ghost poses with the alpha-0.35 interpolated pose solid between (core::interpolate)
  gravzones/ Area gravity fields — six balls dropped through a wind field, an updraft, and a point attractor;
              their fixed-step trails drift/U-turn/orbit per game::gravityAt
  ortho3d/   Orthographic camera — a 7x7 iso field of lit cube columns (a mound) via math::orthographicSize;
              every column reads the same width regardless of depth (no vanishing point)
  base64/    Base64 — a text string, a UTF-8 string, and a byte buffer each shown with their io::base64Encode
              output + a decode(encode(x))==x round-trip check
  floatcurve/ Float curves — four anim::Curve shapes plotted (linear / cubic ease-in-out / ease-out / a
              multi-point particle-size profile) with control points marked
  gradient/  Colour gradients — a spectrum ramp under Constant/Linear/Cubic modes side by side, plus fire /
              health / ocean ramps and the fire ramp baked to 8 swatches (anim::Gradient)
  camera3d/  Camera3D projection — a 3D scene (ground grid + RGB axes + wireframe cube) projected to 2D by
              render::Camera3D, points coloured by frustum containment, a centre ray unprojected to the ground
  polyfill/  Polygon fill — four concave shapes (star / block arrow / plus / thick C-ring) ear-clipped by
              render::triangulatePolygon and filled, with the triangle mesh + outline overlaid
  deadzone/  Analog deadzone — a stick field (raw samples arrowed to their input::analogVector result) +
              the 1-D applyDeadzone response curve, showing the radial deadzone and unit-circle clamp
  randomizer/ Audio stream randomizer — 300 triggers of a 5-clip weighted pool (audio::StreamRandomizer)
              as a pick histogram + pitch×volume scatter + a no-repeat tick strip
  kinematic/ Kinematic character — one body run through an obstacle course by game::moveAndSlide, its path
              coloured by contact state (blue airborne / green on-floor / orange on-wall)
  geometry/  Geometry2D — three panels: a segment web with pairwise intersections, a concave polygon with an
              inside/outside test grid, and closest-point projections + a circle×segment test
  itemlist/  ItemList — two lists in StyleBoxFlat panels: a scrolled single-select saved-games list (selected
              row highlighted, disabled rows dimmed, scrollbar thumb) and a multi-select checked loadout (ui::ItemList)
  restext/   Text resources — a prefab serialized to Godot-.tscn-style text (io::savePrefabText), rendered,
              then parsed back (io::loadPrefabText) with a live round-trip readout
  line2d/    2D polylines — a gallery of strokes: the same zig-zag under miter/bevel/round joints, a bar
              under none/box/round caps, a sampled sine curve, and a closed star (render::buildPolyline)
  parallax/  Parallax backgrounds — the same five-layer scene in three strips at different camera scrolls;
              far layers barely move, near layers sweep, every layer mirror-tiled (game::layerOffset/
              firstTile/tileCount)
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
  blendtree/ Animation blend tree — a stick figure driven by a nested node graph (gait blend-space +
              additive wave + cross-fade to jump); a grid sweeps gait x air (anim::BlendTree)
  joints/   Physics joints — a pin-jointed rope bridge sagging into a catenary + damped-spring-hung
              masses of increasing stiffness (game::Joint2D Pin + Spring)
  spatial2d/ Positional audio — a listener + sound sources with per-source distance attenuation + stereo
              pan visualized as gain halos + L/R bars + a master meter (audio::spatialize)
  form/     UI text input — an editable account-settings form: click/Tab to focus a field (accent
              border + caret), type to edit (ui::TextField + ui::FocusChain + Context::textField)
  cave/     Procedural cave — a seeded cellular-automata cavern with autotiled wall borders (walls inset
              per their edge bitmask) (game::CellularCave + game::autotileMask4)
  tileset/  TileSet & per-tile collision — one grid mixing full ground/wall tiles with half-height ledges,
              every tile's collision box drawn, probe balls resting on what they hit (game::TileSet)
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
  sequencer/ Trigger / method tracks — a four-lane drum machine whose kick/snare/hat/clap markers fire as
              one playhead sweeps the loop, with fire counts + a recent-fires strip (anim::MethodTimeline)
  envelope/ ADSR envelope — pluck/pad/stab presets, each as an attack/decay/sustain/release curve + the
              sine tone shaped by it, so one tone becomes three different notes (audio::ADSR)
  tree/     Tree widget — a project file tree in a StyleBoxFlat panel: indented rows, fold arrows, two
              collapsed folders, and a selected-row highlight (ui::Tree)
  area2d/   Area2D sensors — 14 agents stream through a circular aura + a box gate; each zone shows agents
              inside now + enter/exit counts, agents inside a zone lit + ringed (game::Area2D)
  layers/   Collision layers & masks — player/enemy/pickup species stream through a hurtbox watching only
              enemies + a magnet watching only pickups; matches ring, ignored overlaps go dashed
              (game::CollisionLayers + game::Area2D)
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
  normalmap/ Normal-mapped 2D lighting — a field of dome bumps lit by three coloured point lights, each
              dome shaded on the side facing a light so it reads as 3D relief (game::shadeSurface)
  flowfield/ Flow-field pathfinding — a cost heat map + baked flow arrows + 90 agents streaming around two
              barriers to a shared goal, all from one Dijkstra outward from the goal (game::FlowField)
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
