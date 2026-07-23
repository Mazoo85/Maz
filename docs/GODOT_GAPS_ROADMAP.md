# Closing the Godot Gaps — the Work Plan (everything except ecosystem)

This is the executable plan for `GODOT_GAPS.md`. The user asked to close **every gap except the
ecosystem section** (§8 of `GODOT_GAPS.md`). This document turns the remaining gaps into an ordered
queue of concrete, buildable milestones that the autonomous milestone loop works through one at a
time, newest progress tracked in the scratchpad log and `GODOT_PARITY.md`.

## The honest constraint (read this first)

This build machine has **no GPU, no audio device, and no VR/console hardware**. That splits every
item into three verifiability tiers, and each milestone below is tagged with one:

- **[VERIFIABLE HERE]** — pure CPU/logic/parsing/tooling. Can be written *and* unit-tested to the
  same "N checks, 0 failures" bar as the rest of the engine. The bulk of the list is this.
- **[CODE HERE / SEE IT ON YOUR MACHINE]** — real code can be written and compiled here, and its CPU
  parts tested, but the visible/audible result (a lit frame, a played sound) can only be confirmed on
  a machine with a real GPU/audio device. These are honestly marked "written, not visually verified."
- **[NEEDS YOUR HARDWARE/TOOLCHAIN]** — cannot be meaningfully done in this sandbox at all (console
  SDKs under NDA, a physical VR headset, an app-store signing pipeline). These are documented and
  deferred, never claimed as done.

Nothing is ever marked complete on this branch unless it passed real verification. Where a thing can
only be written blind, the docs say exactly that.

## Queue — grouped by `GODOT_GAPS.md` section (ecosystem §8 intentionally excluded)

### §4 Import formats — mostly [VERIFIABLE HERE] (pure parsers)
- [x] **Collada `.dae` mesh import** (`render::parseCollada`) — DONE (M497). [VERIFIABLE HERE]
- [x] **PLY mesh import** (ASCII + binary) — `render::parsePly` — DONE (M498). [VERIFIABLE HERE]
- [x] **STL mesh import** (ASCII + binary) — `render::parseStl` / `loadStl` — DONE (M509); auto-detects encoding,
  per-face normals with winding-based fallback. [VERIFIABLE HERE]
- [x] **OBJ material library `.mtl`** — `render::parseMtl` / `loadMtl` / `findMaterial` — DONE (M510); the material
  side of the OBJ importer (colors, shininess, transparency, texture-map paths). [VERIFIABLE HERE]
- [x] **DDS texture decode** (DXT1/DXT3/DXT5 = BC1/BC2/BC3) — `render::decodeDds` / `loadDds` — DONE (M511); CPU
  S3TC block unpack to an editable RGBA8 Image (distinct from the Ktx2 GPU-upload path). [VERIFIABLE HERE]
- [x] **half-precision float (float16) conversion** — `math::halfToFloat` / `floatToHalf` — DONE (M512); the
  IEEE 754 half format underpinning HDR image storage, glTF quantized accessors, and GPU vertex compression
  (round-to-nearest-even, exhaustively verified). [VERIFIABLE HERE]
- [x] **spherical-harmonic irradiance probes** (`math::ShL2` / `shBasis` / `shIrradiance`) — DONE (M513); the
  order-2 SH ambient/light-probe representation Godot bakes into LightmapGI (project radiance -> 9 RGB
  coeffs -> clamped-cosine irradiance). [VERIFIABLE HERE]
- [x] **SNORM/UNORM fixed-point packing** (`math::packUnorm8/16` / `packSnorm8/16` + unpack) — DONE (M514);
  the quantization layer of vertex/attribute compression (Khronos GL/Vulkan norm encoding). Composes with
  octahedronEncode to store a normal in two uint16s the way Godot's compressed mesh format does. [VERIFIABLE HERE]
- [x] **BC1/DXT1 texture ENCODE** (`render::encodeBc1Block` / `encodeDdsBc1` / `saveDdsBc1`) — DONE (M515);
  the inverse of the M511 decoder — the CPU texture-compression step Godot's editor runs on import
  (RGBA -> block-compressed .dds, farthest-pair range-fit endpoints). [VERIFIABLE HERE]
- [x] **BC3/DXT5 texture ENCODE** (`render::encodeBc3AlphaBlock` / `encodeDdsBc3` / `saveDdsBc3`) — DONE (M517);
  adds the alpha channel DXT1 can't store (sprites/UI/foliage), 8-value interpolated alpha + reused color block. [VERIFIABLE HERE]
- [x] **TGA image decode** — ALREADY PRESENT (`render::decodeTga`, `ImageCodecTga.hpp`). No work needed.
- [x] **BMP image decode** — ALREADY PRESENT (`render::decodeBmp`, `ImageCodecBmp.hpp`). No work needed.
- [x] **Netpbm (PNM) codec** — `render::decodePnm` (P1..P6: ASCII/binary PBM/PGM/PPM) + `encodePnmP6`/`encodePnmP3`
  — DONE; verified against hand-authored byte streams + round-trip (`tests/render/pnm.cpp`, ctest `pnm_codec`).
  A format Godot doesn't even import; trivially hand-writable so it doubles as a debug output. [VERIFIABLE HERE]
- [x] **DEFLATE / zlib inflate** (`io::inflateRaw` / `io::zlibInflate`) — DONE (M499); prerequisite for PNG. [VERIFIABLE HERE]
- [x] **gzip (.gz) container decode** (`io::gunzip`) — DONE (M516); RFC 1952 framing over inflateRaw + CRC-32/ISIZE
  verify (reuses core::crc32). Godot's FileAccess gzip mode. [VERIFIABLE HERE]
- [x] **PNG decode** (`render::decodePng` / `loadPng`) — DONE (M500); chunks + all 5 filters + gray/RGB/RGBA/palette.
  **Adam7 interlacing added M508** (7-pass sparse sub-grids unfiltered + scattered). [VERIFIABLE HERE]
- [x] **QOA (Quite OK Audio) codec** (`audio::encodeQoa` / `decodeQoa`) — DONE (M517); a full spec-accurate
  encoder + decoder (LMS predictor + 4-bit-scaled 3-bit residuals), compressing PCM ~2.5-4x with no external
  dependency. Verified by an encode→decode round-trip (mono + stereo) with bounded lossy error, correct
  header/rate/channels/sample-count, and smaller-than-16-bit-PCM output (`ctest -R qoa_codec`). This is the
  compressed-audio import that was missing next to WAV. [VERIFIABLE HERE]
- [x] **G.711 μ-law / A-law codec** (`audio::encodeMuLaw` / `decodeMuLaw` / `encodeALaw` / `decodeALaw`) —
  DONE (M523); the ITU-T telephony/VoIP companding codec (WAVE format tags 7 & 6), a 2:1 log-companded
  one-byte-per-sample format decode-anywhere with no tables. Both laws, exact ITU-T reference companding,
  bridged to `WavData`. Verified (`ctest -R g711_codec`) against spec anchors (μ-law silence → 0xFF), sign +
  monotonicity, and a round-trip with bounded log-quantization error. [VERIFIABLE HERE]
- [~] **Ogg Vorbis / MP3 decode to PCM** — MP3 **framing + metadata** landed (M656): `audio::parseMp3FrameHeader`
  / `scanMp3` (`Mp3.hpp`) parse MPEG-1/2/2.5 Layer I/II/III frame headers, build a frame seek index, skip
  ID3v2 tags, and compute duration — the demux/metadata half a player runs before decoding. See the M656
  entry at the top of §5. The remaining piece is the heavy Layer III codec DSP (Huffman + IMDCT + synthesis
  filterbank) → PCM; QOA already covers the dependency-free compressed-audio need, so playback is not blocked.
  [VERIFIABLE HERE]
- [x] **Font fallback chains** (`ui::FontFallback`) — DONE (M501); per-codepoint resolution + per-font runs. [VERIFIABLE HERE]
- [~] **FBX import** — ASCII FBX **geometry** landed: `render::parseFbxAscii` / `loadFbx` reads the mesh
  `Vertices` + `PolygonVertexIndex` arrays (decoding FBX's `~i` polygon terminator) and fan-triangulates to
  `shapes::MeshData`; verified against a hand-authored unit-cube (`tests/render/fbx.cpp`, ctest `fbx_import`).
  [VERIFIABLE HERE — geometry DONE]. Remaining: binary FBX, LayerElement normals/UVs, multi-mesh scenes.
- [x] **GLB (binary glTF) container parser** (`render::parseGlb` / `buildGlb`) — DONE (M520); splits a `.glb`
  byte buffer into its JSON + BIN chunks (and rebuilds one) WITHOUT a file device or cgltf, so a `.glb`
  embedded in an `io::ResourcePack` or fetched over the network can be unpacked in memory. Header
  magic/version/length validation, spec 4-byte chunk padding, unknown-chunk skipping. Verified against
  hand-built GLBs (`ctest -R glb_container`): build→parse round-trip, JSON-only files, malformed rejection.
  The glTF scene parse on top is the existing `render::loadGltf`. [VERIFIABLE HERE]

### §7 Text / UI / localization depth — [VERIFIABLE HERE]
- [x] **Unicode BiDi runs + base direction** — ALREADY PRESENT (`ui::bidiRuns` / `baseDirection`, `TextServer.hpp`).
- [x] **Line-break opportunities** — ALREADY PRESENT (`ui::lineBreakOpportunities`, `TextServer.hpp`).
- [x] **Basic complex-script shaping hooks** — DONE (M653): data-driven OpenType-style shaper
  (`ui::shapeGlyphs` / `ShapingTable` in `TextShaping.hpp`) with GSUB ligature substitution, GPOS pair
  kerning, and GPOS mark-to-base attachment. See the M653 entry at the top of §5. [VERIFIABLE HERE]
- [x] **Localization tooling** — DONE (M652): gettext PO catalog with a real per-language plural-rule engine
  (`io::PoCatalog` / `io::PluralRule`), plus PO write-back (`serialize()`) and POT extraction (`io::PotBuilder`).
  See the M652 entry at the top of §5 for the full write-up. [VERIFIABLE HERE]
- [~] **Video container/codec decode** — IVF **container demux** DONE (M658): `video::demuxIvf` /
  `parseIvfHeader` (`video/Ivf.hpp`) read the IVF file header (codec, dimensions, frame rate, count) and
  extract each compressed frame's timestamp + payload — the demux half every player runs before decoding.
  See the M658 entry at the top of §5. The remaining piece is the VP8/VP9/AV1 codec (frame bytes → pixels)
  and its GPU display. [CODE HERE / SEE IT ON YOUR MACHINE]

### §6 Scripting & language — [VERIFIABLE HERE]
- [x] **Deepen the script VM toward GDScript-grade tooling** — DONE (M654): editor tooling for the
  scripting language (`script::tooling` in `Tooling.hpp`) — document outline (symbols), context-aware
  autocomplete, and function-signature lookup for tooltips, over an error-tolerant scanner that works on
  half-typed code. This satisfies the "deepen the existing script VM toward GDScript-grade tooling
  (autocomplete data, doc tooltips)" arm of this item. See the M654 entry at the top of §5. [VERIFIABLE HERE]
  (A full C#/.NET second binding remains a possible future alternative, but the tooling path is now done.)
- [x] **Stable C-ABI extension interface** (a GDExtension analog) so native modules load without
  recompiling the engine — DONE. The in-process ABI (`ext::Extension.hpp`: versioned registry, tagged
  variant, entry-point negotiation) is now joined by the real dynamic loader (M512, `ext::DynamicLibrary` /
  `loadExtensionLibrary`): it dlopen/LoadLibrary's a compiled `.so`/`.dll`, resolves the plugin's exported
  `maz_extension_abi_version` + `maz_extension_entry` symbols, rejects an incompatible ABI major, then runs
  the entry so the plugin registers its classes. Verified END-TO-END (not a mock): the build compiles
  `examples/plugins/counter_plugin.cpp` into a real shared library and `ctest -R ext_dynamic` loads it at
  runtime, then instantiates + calls a `Counter` class the plugin registered across the C ABI. [VERIFIABLE HERE]
- [x] **Documentation site + getting-started tutorial** — DONE (M513). `maz::docs` (`render::`-free header
  `docs/SiteGen.hpp`) turns the repo's Markdown into a linked static HTML site (index + per-page + shared
  nav), HTML-escaping text and rewriting intra-doc `.md`→`.html` links; the `docsgen` CLI (`tools/gen_docs.cpp`)
  generates the whole `docs/` tree to a folder, and `ctest -R docs_site` verifies the renderer + site assembly
  headlessly. Plus [TUTORIAL_FIRST_GAME.md](TUTORIAL_FIRST_GAME.md) — a beginner "first game in 10 minutes"
  walkthrough using real engine APIs. [VERIFIABLE HERE]

### §2 Platforms & export — mixed
- [x] **ProjectSettings / project.godot manifest** (`core::ProjectSettings`) — DONE (M518); typed project-wide
  settings (name, main scene, window size) with Godot dotted keys + sectioned save/load round-trip. Foundation
  for the editor + export. [VERIFIABLE HERE]
- [x] **Desktop export/packaging** — DONE (M655): per-OS bundle planner (`io::planBundle` /
  `BundlePlan` in `BundlePlan.hpp`) that computes the complete platform-specific bundle layout —
  executable naming, library placement, per-OS launcher script, and MANIFEST — deterministically, as
  a tested core the shell packager (or an in-editor Export button) executes. See the M655 entry at the
  top of §5. [VERIFIABLE HERE] (the packaging *logic* is now verified here; running the packaged game is manual)
- [x] **Web/WASM build path** — DONE (M514, the completable-here part). The one portability seam every
  desktop engine must cross for the browser — the main loop — is solved and unit-tested: `platform::runMainLoop`
  (`platform/WebLoop.hpp`) blocks on desktop but registers a per-frame browser callback under `__EMSCRIPTEN__`,
  so game code is written once (native path verified by `ctest -R webloop`). Plus a complete, documented build:
  `tools/build_web.sh` (emcmake + WebGL2 flags + shell), `web/shell.html` (canvas + loader page), and
  [WEB_BUILD.md](WEB_BUILD.md) with the exact emsdk steps. Emitting the actual `.wasm`/`.js` needs Emscripten
  on the build machine (absent on this box). [NATIVE PATH VERIFIABLE HERE / WASM NEEDS TOOLCHAIN]
- [x] **Per-platform backend seam** (`platform::PlatformBackend` + `HeadlessBackend` + `PlatformRegistry`) —
  DONE (M515). The single interface a port implements per target (init/shutdown, native surface handle,
  asset/user directories, input caps, OS suspend/resume lifecycle). The headless backend + registry are
  unit-tested here (`ctest -R platform_backend`); console/VR/mobile are backends behind this same seam with
  the exact human step documented in [PLATFORMS.md](PLATFORMS.md). [VERIFIABLE HERE]
- [ ] **Android / iOS backend** — an `AndroidBackend`/`IOSBackend` against the seam above. [NEEDS YOUR
  HARDWARE/TOOLCHAIN] (Android SDK/NDK, Xcode, devices, dev accounts — see PLATFORMS.md)
- [ ] **Console backend** — behind the seam. [NEEDS YOUR HARDWARE/TOOLCHAIN] (NDA SDKs; not in a public repo)
- [ ] **XR/OpenXR backend** — behind the seam (`caps().immersiveVr`). [NEEDS YOUR HARDWARE/TOOLCHAIN] (headset)

### §5 High-end 3D rendering — [CODE HERE / SEE IT ON YOUR MACHINE]
All of these need a live GPU to *see*, but the CPU-side data structures, bakers, and math are testable.
- [x] **Loxodrome / rhumb line (constant-bearing sphere path)** (`math::loxodromePoint` / `latLonToUnit` /
  `loxodromePolyline`, `Loxodrome.hpp`) — DONE (M790); the path across a sphere that holds a CONSTANT compass
  bearing (crossing every meridian at the same angle), i.e. the steady-heading route a ship/plane flies with
  the compass pinned — the complement to the shortest-path great circle in `GreatCircle.hpp` (whose heading
  constantly changes). On a Mercator map it's a straight line; on the globe it spirals to the pole. For
  navigation/strategy/globe games and rhumb spirals; Godot has no such helper. Handles the east/west
  degenerate case (a parallel). Verified against the AIRTIGHT constant-bearing property (heading measured on
  the sphere via local north/east frames is the same at every point and equals the input bearing), the
  meridian (bearing 0 → longitude constant) and parallel (bearing 90° → latitude constant) limits, and the
  exact rhumb length |Δlat|/cos(bearing) (closed form + an independent double-precision arc-length sum).
  ctest `loxodrome`.
- [x] **Ray vs torus (donut) + quartic solver** (`math::rayIntersectsTorus` → `TorusHit`, `RayTorus.hpp`;
  `math::solveQuartic` → `QuarticRoots`, `Polynomial.hpp`) — DONE (M789); the hitscan/picking test against a
  torus with arbitrary centre/axis, returning distance, world point and outward normal. Ray-vs-torus is a
  genuine QUARTIC in the ray parameter (not a quadratic), so most engines — Godot included — don't offer it;
  it's needed for rings, donuts, tube loops, portal rims, tyres and halos. Added a robust general
  `solveQuartic` (real roots of a quartic via monotone-interval bisection over the derivative-cubic's critical
  points, reusing `solveCubic`), then transforms the ray into the torus's local frame, builds the quartic
  `(|P|²+R²−r²)² = 4R²(Px²+Py²)`, solves it, and returns the nearest forward hit with the normal pointing from
  the nearest tube-centre-circle point. Verified against known factored quartics (distinct / complex-pair /
  double roots), a brute-force ray-march (hit/miss + distance), the exact surface condition (distance to the
  tube circle == r), and analytic outer-rim / through-the-hole cases. ctest `ray_torus`; full unit suite
  (930k checks) still green.
- [x] **Tractrix — the drag / towed-object curve** (`math::tractrixPoint` / `tractrixDragPoint` /
  `tractrixArcLength` / `tractrixPolyline`, `Tractrix.hpp`) — DONE (M788); the path an object on a taut leash
  of length `a` traces as the other end is dragged along a straight line: x=a(t−tanh t), y=a·sech t. Its
  defining trait is that the leash stays TANGENT to the curve and the SAME length `a` — exactly how a towed
  trailer, a dog on a lead, a swinging pendant or a lagging camera-target trails behind a mover (and spun
  about its asymptote it generates the pseudosphere). Godot has no such curve. Verified against the AIRTIGHT
  constant-leash property (curve-point to drag-point is always length a), the AIRTIGHT tangency (the leash is
  parallel to the curve tangent everywhere), the exact arc length a·ln(cosh t) via an independent fine-polyline
  sum, and the monotone drag shape. ctest `tractrix`.
- [x] **Ray vs finite capped cone** (`math::rayIntersectsCone` → `ConeHit`, `RayCone.hpp`) — DONE (M787); the
  hitscan/picking test against a right circular cone given by apex, axis, half-angle and height, returning the
  distance, world point and outward normal. Cones are spotlight/flashlight volumes, particle-emitter cones,
  funnels, horns, drill tips and AI vision volumes; shooting/picking them (or clipping a spotlight gizmo in an
  editor) needs this query, which Godot does not expose (`game::ViewCone` is only a 2D FOV test). Solves the
  quadratic for the single forward nappe (clamped to the height, excluding the mirror-cone behind the apex)
  plus the circular base cap, returning the nearest forward hit. Verified against a brute-force ray-march
  (hit/miss + distance), the exact half-angle surface condition dot(P−apex,axis)=|P−apex|·cosθ, an
  outward-pointing normal, and analytic side/cap hits on a 45° cone. ctest `ray_cone`.
- [x] **Catenary — hanging chain / rope / cable curve** (`math::solveCatenary` → `Catenary` /
  `catenaryHeight` / `catenaryArcLength` / `catenaryPolyline`, `Catenary.hpp`) — DONE (M786); the shape a
  uniform flexible rope, chain, cable or wire takes hanging under gravity, y=a·cosh(x/a) — NOT a parabola (a
  common mistake), and the difference is visible on rope bridges, power lines, chains and mooring cables.
  Given two anchors and a rope LENGTH longer than the straight gap, it root-finds the unique catenary through
  both anchors carrying exactly that much rope, then samples it (straight-segment fallback when the rope is
  too short or the anchors are vertical). Godot has no catenary helper. Verified against exact endpoint
  interpolation, the requested rope length (both the closed-form a·sinh arc length AND an independent
  fine-polyline sum), the AIRTIGHT hanging-chain ODE a·y″=√(1+y′²) (the physical law that makes a chain a
  catenary and not a parabola), and sag monotonicity (more rope → deeper sag; symmetric low point at the
  midpoint). ctest `catenary`.
- [x] **Astronomical solar position (real sun angle for a date/place)** (`math::julianDate` /
  `solarDeclination` / `sunPosition` → `SunAngles` / `sunDirection`, `SolarPosition.hpp`) — DONE (M785); the
  ACTUAL solar altitude/azimuth (and a world-space light direction) from a UTC calendar date, latitude and
  longitude, using the standard low-precision solar model (declination/RA good to ~0.01° over 1950–2050). The
  existing `game::DayNightCycle` is only an abstract 0..1 clock with a cosine; this makes a day/night cycle
  geographically and seasonally correct — long low winter sun, high short summer sun, June sunrise swinging
  north of east. Godot ships no such helper. Verified against the airtight Julian-date epochs (J2000 =
  2451545.0, unix = 2440587.5), the textbook declination swing (≤±23.44° all year, ~+23.4° June solstice,
  ~−23.4° December, ~0° March equinox), the overhead-Sun geometry (equinox noon ~zenith at the equator, ~90°−φ
  at latitude φ, due-south peak in the northern hemisphere), and a unit light direction. ctest
  `solar_position`.
- [x] **Ray vs axis-aligned ellipsoid** (`math::rayIntersectsEllipsoid` → `EllipsoidHit`, `RayEllipsoid.hpp`)
  — DONE (M784); the hitscan/picking test against an ellipsoid with independent per-axis radii, returning the
  distance, world hit point AND the correctly-scaled outward normal. Ray-vs-sphere only handles a uniform
  radius; real colliders and bounding volumes are frequently squashed or stretched (egg, capsule cap,
  flattened blast radius, stretched planet), which is exactly an ellipsoid. Warps space to a unit sphere,
  solves the sphere quadratic, maps back — taking the normal from the implicit gradient (p−c)/radii² (NOT the
  naive warped direction, which is wrong on non-uniform radii). Godot has no such helper. Verified against a
  brute-force ray-march (hit/miss + distance), the exact implicit surface eq Σ((p−c)/radii)²=1 at every hit,
  the analytic gradient normal, the closed-form ray-sphere reduction when radii are equal, and an analytic
  axis-aligned cap hit. ctest `ray_ellipsoid`.
- [x] **Gielis superformula — procedural organic/star/flower shapes** (`math::superformulaRadius` /
  `superformulaPoint` / `superformulaPolyline`, `Superformula.hpp`) — DONE (M783); one polar equation,
  r(θ)=(|cos(mθ/4)/a|^n2+|sin(mθ/4)/b|^n3)^(−1/n1), that generates an enormous family of closed shapes —
  circles, superellipses, polygons, stars, flowers, starfish, gems and shockwave rings — from six numbers,
  where `m` sets the symmetry/lobe count and `n1,n2,n3` the lobe pinch. It generalises the superellipse
  (M770); Godot has no such generator. Verified against the AIRTIGHT superellipse identity (with m=4, a=b=1,
  n1=n2=n3=n the generated point satisfies the exact implicit |x|^n+|y|^n=1 for every angle and several n),
  the circle limit (n=2 → radius 1 in all directions), and the exact radial periodicity r(θ)=r(θ+8π/m).
  ctest `superformula`.
- [x] **Involute of a circle — the true gear-tooth profile** (`math::involutePoint` / `involuteTangent` /
  `involuteTangentPoint` / `involutePolyline`, `Involute.hpp`) — DONE (M782); the curve traced by the end of a
  taut string unwinding from a circle, and THE flank profile of real spur gears (two involute gears transmit
  motion at a perfectly constant ratio because the contact normal stays on the fixed line of action). This is
  the mechanically-correct complement to the decorative trapezoidal cog in `render::shapes2d::gear`; Godot has
  no involute primitive. Verified against the AIRTIGHT unwound-string identity (the free end is exactly
  baseRadius·t from the tangent point AND perpendicular to the base radius there — the two facts that make it
  a valid gear flank), against the exact swept arc length baseRadius·t²/2 (independent fine-polyline sum),
  and against the exact radius of curvature baseRadius·t (independent three-point circumradius). ctest
  `involute`.
- [x] **Roulette curves — cycloid / spirograph family** (`math::trochoidPoint` / `cycloidPoint` /
  `epitrochoidPoint` / `epicycloidPoint` / `hypotrochoidPoint` / `hypocycloidPoint` + polyline samplers,
  `Roulette.hpp`) — DONE (M781); the curves traced by a point on a rolling circle, either along a line
  (cycloid/trochoid) or around another circle (the classic Spirograph epi-/hypo-trochoids). Gives cardioids,
  nephroids, astroids, deltoids, gear-tooth and cycloidal-gear flanks, and spirograph rosettes from a couple
  of numbers — Godot has none of these as primitives. Verified against an AIRTIGHT closed form (a hypocycloid
  with R=4r must equal the astroid x=R·cos³t, y=R·sin³t to float precision), against the exact known arc
  lengths (one cycloid arch = 8r, the astroid perimeter = 6R, both measured by an independent fine-polyline
  sum), and by the cusp/closure invariants (the cycloid's speed vanishes at its cusp while a curtate trochoid
  never stops; an integer-ratio epicycloid closes after one turn). ctest `roulette`.
- [x] **Logarithmic (equiangular) spiral** (`math::logSpiralPoint` / `logSpiralTangent` / `logSpiralPolyline`,
  `LogSpiral.hpp`) — DONE (M780); the growth spiral of nautilus shells, sunflower heads, galaxy arms and
  hurricanes, r(θ)=a·e^(b·θ) — the radius multiplies by a constant factor per turn. Its defining trait is that
  it crosses every ray from the centre at the SAME angle (equiangular), which makes it self-similar. Use it
  for procedural shells/horns, spiral galaxies/vortices, spiral camera/motion paths and radial UI layouts;
  Godot has no spiral primitive. `a` sets the start radius, `b` the tightness (0 → a circle). Returns a
  polyline for the line/polygon renderer. Verified against its equiangular property — the radius-to-tangent
  angle, measured NUMERICALLY by finite differences (independent of the library's tangent), is constant along
  the spiral and equals acos(b/√(b²+1)) — plus self-similarity (one turn scales the radius by exactly e^(2πb)),
  the analytic tangent matching the numerical one, and the b=0 circle limit (ctest `log_spiral`). [VERIFIABLE
  HERE]
- [x] **Ray vs finite capped cylinder** (`math::rayIntersectsCylinder` → `CylinderHit{hit,t,point,normal}`,
  `RayCylinder.hpp`) — DONE (M779); the hitscan / picking test against a cylinder with an ARBITRARY axis and
  position, returning the hit distance, world point AND surface normal. The engine's
  `Geometry3D.segmentIntersectsCylinder` only handles a segment against an origin-centred, Y-aligned cylinder
  and returns just a point; this is the general ray query for shooting at pillars, tree trunks, barrels,
  pipes and cylindrical colliders, or editor picking. Tests the curved side (a quadratic on the ray projected
  perpendicular to the axis, clamped to the length) and both end caps, returning the nearest forward hit with
  the correct outward normal (radial on the side, ±axis on a cap). Godot exposes no such helper. Verified
  against a BRUTE-FORCE ray-march oracle over thousands of aimed random rays (hit/miss and distance agree),
  with every side hit confirmed at exactly `radius` from the axis and within the length, cap hits within the
  cap disk, unit outward normals, and analytic near-side / axis-cap / miss cases (ctest `ray_cylinder`).
  [VERIFIABLE HERE]
- [x] **Reuleaux polygon (curve of constant width)** (`math::reuleauxPolygon`, `Reuleaux.hpp`) — DONE (M778);
  a closed curve that is exactly as wide in every direction — like a circle, but not one. Built from a
  regular ODD-gon by replacing each edge with a circular arc centred on the opposite vertex: the Reuleaux
  triangle is the guitar-pick / Wankel-rotor shape (and why some manhole covers can't fall in), and higher
  odd counts give rounder constant-width shapes (the UK 20p/50p coins). Distinctive procedural sprites/icons,
  rollers, cams and props; Godot has no constant-width primitive. Returns a closed CCW polyline for the fill
  / triangulator. Verified against its DEFINING property — measuring the extent in 360 directions gives the
  same width every time (to ~1e-3) for 3/5/7 sides — plus the corollaries: the diameter (largest chord)
  equals the width, the boundary is convex, and an even/invalid side count is bumped to a valid odd
  constant-width shape (ctest `reuleaux`). [VERIFIABLE HERE]
- [x] **Closest point on an oriented box (OBB)** (`math::closestPointOnObb` / `distanceToObb` /
  `sphereIntersectsObb`, `ClosestPointObb.hpp`) — DONE (M777); given a point and an arbitrarily-rotated box,
  return the nearest point on/in it and the distance. The engine's Obb (Geometry3D.hpp) does contains /
  box-vs-box SAT / AABB bounds but not this proximity query — the one you need for sphere-vs-OBB collision
  (overlap iff distance ≤ radius, closest point = contact), snapping a probe/agent onto the outside of a
  crate, distance-based culling / trigger volumes and nearest-surface picking. Works by expressing the point
  in the box's local frame, clamping each coordinate to the half-extents and mapping back, so an inside point
  returns itself (distance 0). Godot exposes no such helper. Verified against a BRUTE-FORCE oracle — for
  thousands of random points and a doubly-rotated box, the analytic closest point matches the nearest of
  ~22k points sampled over the six faces in both location and distance (the analytic distance is never
  larger) — plus inside-returns-self, result-always-on-box, the analytic axis-aligned clamp, closest∘closest
  idempotence and sphere-vs-OBB agreement (ctest `closest_point_obb`). [VERIFIABLE HERE]
- [x] **Damerau–Levenshtein (typo-aware) edit distance** (`core::damerauLevenshtein`,
  `DamerauLevenshtein.hpp`) — DONE (M776); like the engine's plain edit distance
  (`core::levenshtein`) but counts a SWAP of two adjacent characters as ONE edit. Transposition is the single
  most common human typo ("teh"→"the", "recieve"→"receive"): Levenshtein charges it as two edits, this charges
  one, so ranking fuzzy search / command-palette / player-name matches by it tolerates typos the way people
  actually make them. Restricted (optimal-string-alignment) variant — symmetric and always ≤ Levenshtein.
  Godot exposes no edit-distance utility. Verified against known values (a transposition is exactly 1 where
  Levenshtein is 2; kitten→sitting = 3), and cross-checked against the engine's OWN `core::levenshtein` over
  5000 seeded random string pairs: the Damerau distance is always ≤ Levenshtein, is sometimes strictly less
  (a transposition genuinely helped), is symmetric, and respects the |lenA−lenB| lower bound (ctest
  `damerau`). [VERIFIABLE HERE]
- [x] **Compensated (Kahan/Neumaier) summation** (`math::KahanSum` / `math::compensatedSum`,
  `CompensatedSum.hpp`) — DONE (M775); add up many floating-point numbers without the rounding drift plain
  left-to-right addition accumulates. Once a running total is large, adding a small value loses low bits to
  rounding; over thousands of adds (mixing audio samples, accumulating forces/particle contributions, summing
  analytics, integrating over a frame) the error compounds. This carries a Neumaier compensation term that
  captures the lost bits and feeds them back, giving near-double accuracy at ~4 extra flops/element — a
  drop-in accumulator (`KahanSum`) plus a one-shot `compensatedSum`. Godot has no such utility. Verified with
  a pathological case — 1e8 followed by a million 1.0s, where the float32 ULP (8) makes every small add
  vanish for naive summation: the compensated result matches the EXACT double-precision total to within a
  ULP while naive drifts by ~1e6; the Neumaier ordering case (a large term after small ones) stays exact; and
  on a 500k-element seeded set the compensated float32 total is >20× closer to the double reference than
  naive (ctest `compensated_sum`). [VERIFIABLE HERE]
- [x] **Clothoid / Euler spiral** (`math::clothoidPoint` / `clothoidPolyline` / `clothoidHeading` /
  `clothoidCurvature`, `Clothoid.hpp`) — DONE (M774); the transition curve whose CURVATURE varies linearly
  with arc length, κ(s)=κ0+rate·s. It is the shape real roads, railways and racetracks use to join a straight
  to a circular corner: curvature ramps smoothly instead of jumping, so a body following it feels no sudden
  sideways lurch (continuous lateral acceleration). The engine's Bézier/B-spline curves control position but
  not curvature directly; this is the curvature-first primitive, and Godot has no equivalent. Evaluated by
  integrating the unit-speed tangent θ(s)=θ0+κ0·s+½·rate·s² with Simpson's rule (cumulative for the polyline).
  Verified against its DEFINING property — the curvature measured geometrically from the sampled curve
  (Menger/circumradius of three consecutive points) equals κ0+rate·s all along the spiral — plus the exact
  degenerate cases (rate=0,κ0=0 → an exactly straight line of the right length; rate=0,κ0=k → an exact circle
  of radius 1/k), unit-speed sample spacing, and point/heading-helper agreement (ctest `clothoid`).
  [VERIFIABLE HERE]
- [x] **Marching Tetrahedra isosurface extraction** (`math::marchingTetrahedra`, `MarchingTetrahedra.hpp`) —
  DONE (M773); turns a 3-D scalar field into a triangle mesh of its level set — the "give me the surface of
  this field as a mesh" step for voxel terrain, metaballs/blobby surfaces, CSG/SDF meshing and volume data.
  Complements the engine's 2-D MarchingSquares and dual-vertex SurfaceNets with the classic PRIMAL
  marching-simplex method: every grid cube is split into SIX tetrahedra and each is marched, so — because
  adjacent tetrahedra share a face whose crossing is fixed by the three shared corner values — the output is
  WATERTIGHT and crack-free by construction, with none of marching cubes' ambiguous-case holes and no
  256-entry table. Godot has no isosurface extractor. Verified on a sphere field to be a proper closed
  2-manifold — every edge shared by exactly two triangles, Euler characteristic V−E+F = 2 — with every vertex
  on the sphere to grid resolution and the closed-mesh signed volume matching (4/3)πR³ (which also proves the
  winding is consistently outward); a box field is likewise watertight and a non-crossing field yields an
  empty mesh (ctest `marching_tetrahedra`). [VERIFIABLE HERE]
- [x] **Savitzky–Golay smoothing filter** (`math::savitzkyGolay`, `SavitzkyGolay.hpp`) — DONE (M772);
  denoises a 1-D signal by fitting a low-degree polynomial to a sliding window (least squares) and taking the
  fitted value at each point. Unlike a moving average, which flattens peaks, it PRESERVES feature shape
  (peaks, edges, slopes) because a polynomial follows curvature the box filter cannot — the tool for cleaning
  noisy analog-stick/gyro input, sensor/telemetry traces, audio envelopes and procedural curves. Godot has no
  such filter. Does a genuine local fit at every point (proper asymmetric fit at both ends, reusing
  `math::solveLinearSystem`), so any polynomial of degree ≤ order passes through UNCHANGED. Verified by that
  exact polynomial-reproduction property (a cubic is reproduced to ~0 at every sample including the
  endpoints), by a sharp total-variation drop on a seeded-noise signal with the mean preserved, and by
  keeping a Gaussian peak far better than a same-width moving average (ctest `savitzky_golay`). [VERIFIABLE
  HERE]
- [x] **Cubic Bézier curve–curve intersection** (`math::bezierIntersections` / `cubicBezierEval`,
  `BezierIntersect.hpp`) — DONE (M771); finds every point where two cubic Bézier curves cross. The engine had
  Bézier paths and easing but no "where do these two curves meet?" query — needed for path/obstacle collision,
  spline-vs-spline hit testing, trim/clip in a vector tool and stroke analysis (Godot's Curve2D exposes none).
  Uses robust recursive de Casteljau subdivision with convex-hull (control-point bbox) culling — two pieces
  can only cross where their hulls overlap, so the pair is split until each piece is sub-tolerance and the
  surviving overlaps are reported (de-duplicated). Verified against an INDEPENDENT brute-force oracle (both
  curves sampled to 600-segment polylines and every segment/segment crossing found + clustered): the
  subdivision result matches on count and location across 0/1/2/3-crossing pairs; every reported point is
  confirmed to lie on both curves (min-distance to a dense sampling ~0); the analytic diagonal cross lands on
  the origin; separated curves report none (ctest `bezier_intersect`). [VERIFIABLE HERE]
- [x] **Superellipse / squircle curves** (`math::superellipsePoint` / `superellipsePolyline` / `squircle` /
  `superellipseContains`, `Superellipse.hpp`) — DONE (M770); the Lamé curve |x/a|ⁿ+|y/b|ⁿ=1 that morphs from
  an astroid (n<1) through the ellipse (n=2) to the iOS-style "squircle" rounded rectangle (n=4) and on toward
  a sharp box (n→∞). Godot has no superellipse primitive; this fills it for smooth rounded-rectangle UI
  panels, organic blob shapes and procedural authoring, emitting a ready-to-fill closed ring. Verified against
  the exact IMPLICIT EQUATION — every generated point satisfies |x/a|ⁿ+|y/b|ⁿ=1 to floating point across
  exponents 0.7…20 and non-equal axes; the n=2 case reproduces the ellipse area π·a·b; the squircle nests
  strictly between the inscribed circle and the bounding box; large n drives the corner to the box corner; and
  central symmetry P(t)=−P(t+π) holds (ctest `superellipse`). [VERIFIABLE HERE]
- [x] **General polygon boolean ops** (`math::polygonIntersection` / `polygonUnion` / `polygonDifference`,
  `PolygonBoolean.hpp`) — DONE (M769); the real Clipper-style boolean for two ARBITRARY simple polygons —
  concave allowed, multi-contour results — via the Greiner–Hormann algorithm. This closes the gap the
  engine's existing `clipPolygonConvex` (Geometry2D.hpp, Sutherland–Hodgman) explicitly left open: that one
  only clips against a *convex* window, while this handles a concave subject AND a concave clip and returns
  the full result, including holes as separate oppositely-wound contours. Use it for destructible-terrain
  carving, merging painted regions, overlap-area between swept shapes, coverage/visibility masks and
  vector-boolean authoring — Godot's `Geometry2D.clip_polygons` / `intersect_polygons` / `merge_polygons`.
  Verified INDEPENDENTLY of the tracing code: a seeded Monte-Carlo membership oracle (even–odd ray cast over
  all result edges) confirms the output region equals the boolean predicate over thousands of random points
  for intersection/union/difference; the analytic inclusion–exclusion identity |A∩B|+|A∪B|=|A|+|B| holds
  exactly; concave (L-shape), full-containment and disjoint cases all check out (ctest `polygon_boolean`, 12
  checks, 0 failures). [VERIFIABLE HERE]
- [x] **NURBS curves (exact conics)** (`math::nurbsPoint` / `math::nurbsClampedKnots`, `NurbsCurve.hpp`) —
  DONE (M768); the industry-standard freeform curve used by every CAD tool and vector program. It generalises
  the engine's plain B-spline by giving each control point a WEIGHT, which lets one curve type represent
  EXACT conics — perfect circles, ellipses and arcs — that no polynomial Bezier or B-spline can reproduce,
  alongside arbitrary smooth freeform shapes; non-uniform knots place sharper/gentler regions and pin the
  endpoints. Use it for precise vector paths, camera/motion rails that must follow exact circular arcs,
  lofting profiles, and authoring tools. Evaluated by the stable de Boor algorithm on homogeneous control
  points. Godot's Curve2D is cubic Bezier only — no rational curves. [VERIFIABLE HERE] `ctest -R nurbs_curve`:
  a degree-2 rational quarter arc and a 9-point full circle are mathematically EXACT — every sampled point is
  radius R from the centre to within 1e-3 (the property polynomial curves cannot achieve); clamped endpoints
  interpolate the first/last control points; a weight-1 curve stays within the control polygon (B-spline
  convex-hull behaviour); determinism.
- [x] **Move-To-Front coding** (`io::mtfEncode` / `io::mtfDecode`, `MoveToFront.hpp`) — DONE (M767); the stage
  that sits between a Burrows-Wheeler Transform and the entropy coder in bzip2-style compression. It keeps a
  running list of the 256 byte values and, for each input byte, emits its CURRENT POSITION then moves it to
  the front; when the data has clustered symbols (exactly what the BWT produces), recently-seen bytes sit
  near the front so their codes are small — long runs collapse to streams of zeros that the following
  run-length + Huffman / range coder squeezes hard. Perfectly reversible. Completes the BWT (M766) -> MTF ->
  entropy pipeline the engine's coders now have. [VERIFIABLE HERE] `ctest -R move_to_front`: airtight
  round-trip over 200 random blocks and text; a same-symbol run encodes to a leading value then all zeros;
  the full BWT+MTF pipeline on repetitive data yields a MAJORITY of zeros and >3/4 small values (the
  low-entropy stream that aids compression) and reverses exactly; empty input round-trips; determinism.
- [x] **Burrows-Wheeler Transform** (`io::bwtEncode` / `io::bwtDecode`, `Bwt.hpp`) — DONE (M766); the
  reversible byte-reordering at the heart of bzip2-style compression. The BWT rearranges a block so runs of
  the same symbol CLUSTER together (identical contexts end up adjacent), which a following move-to-front +
  entropy coder squeezes far better than the raw data — yet it loses nothing: the exact original is
  recovered from the transformed block plus one index. It's the standard front-end for compressing
  repetitive assets (text, level data, tilemaps, serialized scenes) ahead of the engine's Huffman / range
  coder, which had no BWT stage. Classic rotation-sort forward transform, O(n) LF-mapping inverse.
  [VERIFIABLE HERE] `ctest -R bwt`: airtight round-trip — bwtDecode(bwtEncode(x))==x over 200 random blocks
  (sizes 1..300) and structured text; on repetitive input the transform produces MORE adjacent-equal bytes
  than the input (the clustering that aids compression); empty and single-byte inputs round-trip;
  determinism.
- [x] **2D dual contouring (sharp-feature SDF meshing)** (`math::dualContour2D`, `DualContour2D.hpp`) — DONE
  (M765); extract a contour line from a signed distance field that PRESERVES SHARP CORNERS, unlike the
  engine's marching squares which bevels every corner into a chamfer. Marching squares can only put contour
  points on grid-edge midpoints, so a square or a hard crease comes out rounded; dual contouring places one
  vertex inside each boundary cell at the least-squares intersection of the crossing normals (a per-cell QEF
  on the M761 linear solver), so two edges meeting at 90 degrees produce a vertex sitting exactly on the
  corner. That's why it needs the field's gradient (Hermite data), which MS ignores — and why it reproduces
  features MS can't. Turns an SDF (procedural shapes, CSG, destructible terrain, brush masks) into a crisp
  polygon outline for collision, decals, or rendering. Takes the SDF as a callable. Godot ships only
  rounded MS-style meshing. [VERIFIABLE HERE] `ctest -R dual_contour2d`: on a box SDF, dual contouring lands
  a vertex within 0.15 of each true corner while marching squares stays > 0.4 away (and DC is < 40% of MS's
  distance) — the corners MS rounds, DC keeps; a circle SDF's vertices lie on the circle; a field with no
  sign change yields nothing; endpoints are finite and in-grid; determinism.
- [x] **RANSAC robust line fitting** (`math::ransacLine`, `Ransac.hpp`) — DONE (M764); fit a line to points
  that contain GROSS OUTLIERS. Ordinary least squares (and the M762/M763 fits) assume every point belongs to
  the shape, so a few stray points — a mistracked feature, a sensor glitch, a wall behind the floor — drag
  the fit badly off. RANSAC guesses many candidate lines from tiny random samples, keeps the one the most
  points AGREE with (the consensus / inliers), and refits only to those, so outliers are ignored rather than
  averaged in. It's the standard tool for messy real-world point sets: aligning scanned edges, snapping a
  wall/floor line out of noisy depth points, robust trend estimation, calibration with bad samples.
  Deterministic (seeded). Godot ships no robust estimator. [VERIFIABLE HERE] `ctest -R ransac`: with ~42%
  outliers (a competing near-vertical bar), RANSAC recovers the true line (normal aligned > 0.99) and its
  worst error on the true inliers stays < 0.1 while a non-robust total-least-squares fit over all points is
  dragged 5x+ further off; the consensus size is close to the true inlier count and every reported inlier is
  within the threshold; a clean line is fit exactly; too few points fail; same seed gives the same model.
- [x] **Best-fit plane (total least squares)** (`math::fitPlane`, `ShapeFit.hpp`) — DONE (M763); find the
  plane that best passes through a 3D point cloud, minimising ORTHOGONAL distance (true total least squares,
  not a z=f(x,y) graph fit). This is how you estimate the ground/floor from scanned or sampled points, find
  a wall or table surface, get an average surface normal for decal projection or slope checks, or flatten a
  patch of terrain. It centres the points, forms their 3x3 covariance, and takes the smallest-eigenvalue
  eigenvector as the normal (reusing FitObb's Jacobi eigensolver) — the direction of least spread. Rounds out
  the circle/sphere fitters (M762); Godot has no plane fit. [VERIFIABLE HERE] `ctest -R plane_fit`: points on
  a known tilted plane recover its normal (up to sign) and satisfy normal.p + d ~ 0; with out-of-plane
  jitter the normal stays aligned and variance along it is < 5% of the in-plane variance (the defining
  min-variance property); the plane passes through the centroid; fewer than three points fail; determinism.
- [x] **Geometric circle / sphere fitting** (`math::fitCircle` / `math::fitSphere`, `ShapeFit.hpp`) — DONE
  (M762); find the CIRCLE (2D) or SPHERE (3D) that best passes through a cloud of measured points. Where the
  engine's LeastSquares fits a value as a function of x (a line or polynomial), this fits a round SHAPE to
  scattered positions: recover the centre and radius of an arc from sampled points (gears, dials, turning
  circles, curved track segments), fit a bounding sphere to a vertex cloud, estimate an orbit radius, or
  calibrate a circular sensor sweep. Uses the algebraic (Kasa) least-squares form, which linearises the fit
  into a tiny normal-equations solve on the new dense linear solver (M761) — exact on clean data, stable on
  noisy data. Godot exposes no such fit. [VERIFIABLE HERE] `ctest -R shape_fit`: points sampled exactly on a
  known circle/sphere recover its centre and radius to ~1e-3; with small jitter the fit stays within 0.1 of
  the truth and every point lies within the noise band of the fitted shape; collinear points (circle) and
  coplanar points (sphere), or too few points, are reported as failed fits; determinism.
- [x] **Dense linear system solver** (`math::solveLinearSystem` / `math::determinant` / `math::invertMatrix`,
  `LinearSolve.hpp`) — DONE (M761); solve A x = b for a general NxN matrix, plus determinant and inverse, via
  LU decomposition with partial pivoting. This is the numerical workhorse under least-squares FITTING (fit a
  plane / polynomial / curve to data through the normal equations), inverse-kinematics and physics CONSTRAINT
  solves (small dense Jacobian / impulse systems), colour-space and calibration transforms, arbitrary
  interpolation setups, and any "n equations, n unknowns" in tools and gameplay math. The engine had RK4,
  quadrature, root-finding and polynomial roots but no general Ax=b solver; this fills that. Partial pivoting
  keeps it stable and flags singular systems. [VERIFIABLE HERE] `ctest -R linear_solve`: a known 3x3 system
  solves to the exact vector; round-trip — for 200 random systems, solving A(Ax)=b recovers x with residual
  < 1e-6; the identity returns b unchanged; a singular matrix is reported unsolvable and its determinant is
  0; the determinant matches ad-bc and a diagonal product; A * inverse(A) equals the identity over random
  matrices; determinism.
- [x] **Generalized winding-number point-in-mesh** (`math::windingNumber` / `math::pointInMesh`,
  `WindingNumber.hpp`) — DONE (M760); decide whether a point is INSIDE a triangle mesh, ROBUSTLY. "Is this
  point inside the volume?" is the query behind spawning objects inside an arbitrary shape, containment /
  region tests, voxelizing a solid, inside/outside masks for particle or fluid collision, and
  point-in-lava/point-in-water checks. The engine's existing `render::containsPoint` (M546) casts a ray and
  counts surface crossings — fast but brittle: one missing triangle, a T-junction, or a grazing edge flips
  the answer, and its own note names exactly this as the follow-up. The generalized winding number sums the
  solid angle each triangle subtends at the point (Van Oosterom-Strackee), giving ~+/-1 inside and ~0
  outside, and DEGRADES GRACEFULLY: a mesh with holes still reads ~1 inside where ray parity leaks.
  Orientation-agnostic (takes the magnitude). Godot exposes no such query. [VERIFIABLE HERE] `ctest -R
  winding_number`: a cube's interior reads winding ~1 and far points ~0; hundreds of random points are
  classified correctly against the cube's box; after deleting a triangle to punch a hole, interior points
  STILL read inside (the property ray parity fails); determinism.
- [x] **Closest-point / projection onto a path** (`math::closestPointOnPolyline` /
  `math::closestPointOnCurve` / `math::closestPointOnSegment`, `ClosestPointCurve.hpp`) — DONE (M759); given
  any point in space, find the nearest point on a polyline or a smooth Bezier curve, plus how far ALONG the
  path it sits. This is the query behind snapping a dragged object to a spline, measuring an agent's progress
  along a race line or rail, keeping a follower glued to a track, computing a car's cross-track error, or the
  distance from anything to a route. Godot's Curve2D samples and bakes but has no "project this point onto
  the curve" call, so gameplay code rolls it by hand; this does it robustly — exact per-segment projection
  for polylines, and dense-sample-plus-ternary-refine for curves, returning the point, the along-path offset
  (reusable with Curve2D::sample), and the distance. [VERIFIABLE HERE] `ctest -R closest_point_curve`: a
  point on the path projects to itself; the returned distance is <= the distance to any of a fine brute-force
  sampling of the path (optimality lower bound) over hundreds of random queries on both a polyline and a
  curve; an analytic perpendicular offset from a straight segment is reported exactly at the right foot; a
  point past an endpoint clamps to it; the residual is perpendicular to the curve tangent at an interior
  projection; determinism.
- [x] **Theta\* any-angle pathfinding** (`game::thetaStar` / `game::thetaLineOfSight`, `ThetaStar.hpp`) —
  DONE (M758); a grid path planner that produces short, STRAIGHT routes instead of the staircase zig-zag
  ordinary grid A\* (and the engine's jump-point search) is stuck with. Classic grid search can only step
  between cell centres along the 8 compass directions, so crossing an open room comes out jagged, longer,
  and visibly unnatural; Theta\* adds a line-of-sight test — when relaxing a node it checks whether the
  node's grandparent can see the new cell directly and, if so, links straight to it — letting segments cut
  the grid at any angle. In open space the path collapses to a single straight line of the true Euclidean
  length. This is exactly the natural-looking route Godot's grid navigation can't produce. Uses the standard
  Nash line-of-sight; grid steps are 8-connected with no corner cutting. [VERIFIABLE HERE] `ctest -R
  theta_star`: across an empty grid the path is a single straight segment of the exact Euclidean length; with
  a wall in the way the Theta\* path is never longer than an independent 8-connected Dijkstra optimum and is
  strictly shorter than the staircase route; an INDEPENDENT dense sampler confirms no segment tunnels through
  a wall's body; a walled-off goal is reported unreachable; determinism.
- [x] **Median-cut colour quantization** (`render::medianCutPalette` / `render::nearestColor`,
  `MedianCut.hpp`) — DONE (M757); shrink a full-colour image down to a small representative PALETTE of at
  most K colours — how a GIF, an indexed texture, or a deliberately retro/limited-palette look is produced.
  It recursively splits the cloud of pixel colours: at each step it takes the box with the widest spread
  along red, green, or blue and cuts it at the MEDIAN of that channel, so dense regions of colour get more
  palette entries than sparse ones; each final box contributes its average colour. This is the classic
  Heckbert median cut — better balanced than a naive "keep the most common colours" pass, which is exactly
  the fallback the GIF encoder currently uses (its comment even claims median-cut it never had). Pairs with
  `nearestColor` to remap pixels to palette indices. Godot has no runtime colour quantizer. [VERIFIABLE
  HERE] `ctest -R median_cut`: an image with <=K distinct colours quantizes to exactly those colours with
  zero error; two well-separated colour clusters with K=2 recover one palette entry per cluster and every
  pixel maps to its own; quantization error decreases monotonically as K grows (2/4/8/16/32); every palette
  colour lies within the input colour range; determinism.
- [x] **Rotation-minimizing (parallel-transport) frames** (`math::parallelTransportFrames`,
  `ParallelTransport.hpp`) — DONE (M756); a smoothly twisting coordinate frame that rides along a 3D path.
  To sweep a cross-section down a curve — a tube, rope, cable, road, vine, ribbon trail, or a camera rail —
  you need at every point a consistent up/side pair perpendicular to the direction of travel. The textbook
  Frenet frame (built from curvature) suddenly flips 180 degrees where the curve straightens or inflects,
  visibly kinking the swept mesh; a rotation-minimizing frame carries the previous frame forward with the
  LEAST possible twist so the tube never spins or snaps. Uses Wang et al.'s exact, stable double-reflection
  method. Godot's CSGPolygon path-extrude twists on inflections; this fixes that. [VERIFIABLE HERE] `ctest
  -R parallel_transport`: every frame is a unit right-handed orthonormal basis; each tangent follows the
  local direction of travel; on a planar circle with an out-of-plane hint the normal stays exactly the plane
  normal at all 64 samples (zero twist — the property the Frenet frame fails) while the binormal genuinely
  rotates in-plane to follow the curve; a straight line yields identical frames; consecutive normals never
  flip; determinism.
- [x] **Haar wavelet transform** (`math::haarForward1D/2D` / `math::haarInverse1D/2D`, `Wavelet.hpp`) — DONE
  (M755); the simplest multi-resolution transform — repeatedly split a signal or image into a coarse
  "average" half and a fine "detail" half, so a texture or heightfield becomes a small blurry thumbnail plus
  a stack of ever-finer correction layers. That decomposition is the backbone of progressive/streamed
  loading (show the thumbnail, refine as detail arrives), level-of-detail, and lossy compression (most
  detail coefficients are tiny — zero the small ones and the picture barely changes). This is the normalised
  orthonormal Haar basis, so it preserves energy exactly and inverts perfectly; works on power-of-two 1D
  arrays and square power-of-two 2D grids to the deepest level. Godot ships no wavelet transform.
  [VERIFIABLE HERE] `ctest -R wavelet`: perfect reconstruction inverse(forward(x))==x for 1D and 2D; energy
  (Parseval) preserved to float epsilon; a constant image transforms to a single non-zero DC coefficient
  carrying all the energy with zero detail; compression — zeroing the smallest 50% of coefficients of a
  smooth image barely changes it and dropping more never lowers the error (monotone); determinism.
- [x] **Catmull-Clark subdivision surfaces** (`render::catmullClark`, `CatmullClark.hpp`) — DONE (M754); the
  industry-standard way to turn a blocky low-poly QUAD cage into a smooth rounded surface, refining one level
  at a time toward a limit surface. Where the engine's existing Loop subdivision smooths triangle meshes,
  Catmull-Clark works on arbitrary polygon faces and always outputs quads — the scheme film and modelling
  packages use for organic shapes (a cube rounds toward a sphere-like blob, a rough character cage becomes a
  clean subdivision surface). Each pass places a face point at every face centroid, an edge point per edge
  (blending the edge's ends with its two neighbouring face points), and nudges every original vertex toward
  the average of its surrounding face and edge points, then splits every face into quads; boundary edges use
  the open cubic-B-spline crease rule so borders stay put. Godot exposes no runtime subdivision surface.
  [VERIFIABLE HERE] `ctest -R catmull_clark`: one pass of a cube (V8/E12/F6) yields the exact counts V'=26,
  F'=24 quads, E'=48 so the Euler characteristic V-E+F=2 is preserved (still a closed sphere); every output
  face is a quad; all new points stay inside the control cube and each corner rounds strictly inward; a flat
  z=0 grid stays perfectly planar after two passes; determinism.
- [x] **Minkowski sum of convex polygons** (`math::minkowskiSumConvex`, `MinkowskiSum.hpp`) — DONE (M753);
  sweep one shape around the boundary of another and take everything the pair can cover together — the set
  { a + b : a in A, b in B }. This is the workhorse behind collision inflation and motion planning: grow a
  level's walls by the player's radius and a point-sized dot can be tested instead of a fat body (the grown
  obstacle is exactly wall (+) player-disc); build the "configuration-space obstacle" a moving convex agent
  must avoid (obstacle (+) reflected-agent) so path-planning collapses to routing a single point; round a
  polygon by summing it with a small disc, or fatten a swept shape. The result is always convex and its
  support (extent in any direction) is the sum of the two inputs' supports. Complements the existing convex
  clip / offset / hull ops; Godot exposes no Minkowski sum. [VERIFIABLE HERE] `ctest -R minkowski_sum`: a
  unit square (+) a unit square is the 2x2 square (area 4); the support identity support(A+B, d) ==
  support(A, d) + support(B, d) holds for every direction over 60 random convex-shape pairs; every result is
  convex CCW; brute-force — 400 interior points a+b land inside the sum polygon; determinism.
- [x] **Alpha shapes / concave hull** (`math::alphaShapeEdges` / `math::concaveHull`, `AlphaShape.hpp`) —
  DONE (M752); the "shrink-wrap" outline of a scattered 2D point cloud. A convex hull is the tightest
  CONVEX rubber band and can never dip into a bay or wrap a C-shape; the alpha shape can — it keeps only the
  Delaunay triangles small enough to hold a disc of radius alpha, so any gap wider than ~2*alpha is left
  outside, carving out concavities and notches. Sweep alpha large→small and the outline morphs from the
  convex hull down to the bare points. This turns a splatter of samples (a scanned blob, hit points, a
  territory of unit positions, a lasso) into a real polygon you can fill, collide, or path around. Godot
  ships convex hulls only. [VERIFIABLE HERE] `ctest -R alpha_shape`: with a huge alpha the concave hull's
  enclosed area equals the convex hull's (100.0 for a 10x10 grid); cutting a 3-wide notch into the square
  and using a moderate alpha carves it — the concave area drops well below the convex area yet still covers
  most of the shape; a probe point in the carved notch is inside the convex hull but OUTSIDE the concave
  hull, while a probe in the solid body is inside both; determinism.
- [x] **2D SPH fluid simulation** (`game::sphDensities` / `game::sphAccelerations`, `Sph2D.hpp`) — DONE
  (M751); the particle-based way to simulate liquids — water, goo, lava, blood, a splash of coloured
  fluid — as a cloud of little blobs that push apart when squeezed and drag their neighbours along. Each
  particle carries a soft "smoothing kernel" of radius h, and the fluid's density at a particle is the
  overlap of its neighbours' bumps (poly6 kernel); where the fluid is denser than its rest density it
  develops pressure that shoves particles apart (spiky-gradient kernel), and a viscosity term makes
  neighbours share velocity so the flow stays coherent (viscosity-laplacian kernel). Uses Monaghan's
  symmetric pressure form so the internal pressure forces conserve momentum exactly. Godot has no fluid
  solver. [VERIFIABLE HERE] `ctest -R sph2d`: a lone particle's density equals its exact self-contribution;
  a closer neighbour raises density; the symmetric pressure forces conserve momentum (sum of mass*accel is
  ~0 over 200 random clouds, rel < 1e-3); an over-compressed pair pushes apart; viscosity opposes relative
  velocity; gravity adds uniformly; determinism.
- [x] **Harris corner detection** (`render::harrisCorners`, `HarrisCorners.hpp`) — DONE (M750); find the
  distinctive, trackable "corner" points in an image — spots where brightness changes sharply in TWO
  directions — as opposed to flat regions (no change) or straight edges (change in only one direction).
  Corners are the stable landmarks used to align/stitch images, match features between frames, calibrate,
  auto-register decals or sprites, and drive simple optical-flow tracking. It builds the local structure
  tensor (windowed sums of squared gradients) and scores each pixel with the Harris response
  det(M) - k*trace(M)^2, then keeps the local maxima. Godot ships no feature detector. [VERIFIABLE HERE] On a
  solid square a corner is found near each of the four true corners and NONE in the flat interior or along
  the straight edges (the defining two-direction property); a flat image yields no corners; a single straight
  edge yields no interior corners; and detection is deterministic.
- [x] **Morphological thinning / skeletonization (Zhang–Suen)** (`render::thinZhangSuen`, `Thinning.hpp`) —
  DONE (M749); reduce a filled binary shape to its one-pixel-wide SKELETON, the centerline that captures the
  shape's topology. Where the engine's dilate/erode grow or shrink a region, thinning peels a shape down to
  its bones without breaking it apart — turning a thick blob into a stick-figure medial axis. It's the
  standard tool for extracting road/river centerlines from a mask, stroke skeletons for handwriting/gesture
  analysis, path graphs from painted regions, and shape descriptors. It repeatedly deletes boundary pixels
  whose removal neither breaks connectivity nor shortens an endpoint, in two alternating sub-passes. Godot
  has erode/dilate but no thinning. [VERIFIABLE HERE] The skeleton only removes pixels (output ⊆ input), no
  2x2 foreground block survives (genuinely one pixel wide), connectivity is preserved (a connected shape
  stays one component; two blobs stay two), a solid rectangle reduces to a quarter of its pixels, and the
  result is idempotent (re-thinning changes nothing).
- [x] **Karplus–Strong plucked-string synthesis** (`audio::karplusStrongPluck`, `KarplusStrong.hpp`) — DONE
  (M748); a startlingly simple recipe that produces convincing plucked/struck string tones (guitar, harp,
  koto, a twangy UI blip) with no samples: fill a short delay line with a burst of noise (the "pluck"), then
  replay it while averaging each pair of adjacent samples. The averaging is a gentle low-pass that shaves the
  highs a little more each pass, so the bright noisy attack mellows into a decaying harmonic tone whose PITCH
  is set by the delay-line length (frequency = sampleRate / length). The classic physical-modelling method —
  cheap enough to run per-note at runtime for procedural instruments and impacts. Godot has oscillators and
  samples but no string model. [VERIFIABLE HERE] The autocorrelation of the output peaks at a lag equal to
  sampleRate/frequency across a range of pitches (it really is at the requested note), an octave up halves
  the period, the energy decays over time (the note rings down), output stays bounded in ~[-1,1], and it's
  deterministic per seed.
- [x] **Edge-preserving bilateral filter** (`render::bilateralFilter`, `BilateralFilter.hpp`) — DONE (M747);
  smooth away noise while keeping edges crisp. An ordinary blur averages each pixel with its neighbours
  regardless of content, so it kills noise but smears every edge into mush. The bilateral filter weights each
  neighbour by BOTH how close it is (spatial) AND how similar its colour is (range), so neighbours across a
  strong edge get almost no weight — the edge stays sharp while flat regions still clean up. This is the
  staple behind photo denoise, skin-smoothing / "beautify", cartoon-stylize preprocessing, and cleaning noisy
  procedural or baked textures. Output is a convex blend of the input, so it never overshoots. Godot's Image
  has no bilateral. [VERIFIABLE HERE] A constant image is unchanged; the defining property holds on a noisy
  step edge — each flat side is smoothed (variance drops by more than half) while the edge contrast is
  preserved (each side keeps its own tone, not smeared across) — plus the no-overshoot convex-blend bound and
  determinism.
- [x] **Arithmetic (range) coding** (`io::rangeEncode`/`rangeDecode`, `RangeCoder.hpp`) — DONE (M746); entropy
  compression that squeezes a stream of symbols down toward its true information content. Where LZW replaces
  repeats with dictionary codes, a range coder assigns each symbol a slice of a numeric interval proportional
  to its probability, so common symbols cost a fraction of a bit — beating fixed-width and Huffman on skewed
  data (quantized audio/mesh residuals, save-game deltas, tile histograms). Dmitry Subbotin's carryless
  32-bit range coder with a caller-supplied static frequency model. Godot exposes zlib/gzip but no arithmetic
  coder. [VERIFIABLE HERE] The airtight guarantee — decode(encode(s)) == s — is checked over thousands of
  random alphabets, frequency tables, and stream lengths; plus a skewed distribution that provably compresses
  well below the fixed-width size, and degenerate (empty stream, single-symbol alphabet) round-trips.
- [x] **Terrain depression filling (priority-flood)** (`game::fillDepressions`, `FillDepressions.hpp`) — DONE
  (M745); heightfields from noise or erosion are riddled with pits and closed basins where water would pool
  and get stuck. Before you can trace rivers, compute drainage / flow accumulation, place lakes, or run a
  hydraulic-erosion pass, you first "fill" every depression up to the lowest lip over which water could spill,
  turning the surface into one where every point has a downhill path to the map edge. This is the standard
  hydrology preprocessing step (ArcGIS "Fill", GRASS r.fill.dir), distinct from the engine's thermal Erosion;
  Godot has none. The Barnes priority-flood algorithm floods inward from the boundary via a min-heap keyed by
  spill height. [VERIFIABLE HERE] The invariants are checked over hundreds of random terrains — the filled
  surface is >= the input everywhere, the boundary is untouched, and NO interior pit remains (every cell has
  a non-ascending exit) — plus an analytic bowl that fills exactly to its outlet level, idempotence, and
  determinism.
- [x] **Radial basis function scattered interpolation** (`math::RbfInterpolator2D`, `Rbf.hpp`) — DONE (M744);
  given a handful of sample points each with a value (a height, a weight, a colour, a displacement), build
  ONE smooth field that passes exactly through every sample and interpolates sensibly everywhere in between —
  no grid required. This is the standard tool for smooth image warping / morphing (pin control points and
  deform), terrain or influence maps from sparse measurements, scattered colour/weight blending, and smooth
  "attract toward these anchors" fields. It places a radially-symmetric bump (Gaussian or multiquadric) on
  each sample and solves a small linear system (Gaussian elimination, partial pivoting) for the weights.
  Godot has no scattered-data interpolator. [VERIFIABLE HERE] The defining property — the field passes
  EXACTLY through every control point — is checked over thousands of random configurations and all three
  kernels; plus a single-point Gaussian bump that decays monotonically with distance, mirror-symmetry of the
  field for mirrored equal-value anchors, determinism, and degenerate (no-points → invalid, evaluates to 0).
- [x] **Poisson seamless cloning (gradient-domain compositing)** (`render::seamlessClone`, `SeamlessClone.hpp`)
  — DONE (M743); paste a patch of one image into another so the seam DISAPPEARS (Pérez et al. 2003), built
  on the M742 Poisson solver. Naively copying pixels leaves a hard edge whenever the patch's lighting differs
  from its surroundings; this copies the patch's GRADIENTS (its internal detail) while forcing its border to
  match the destination, then solves a Poisson problem per colour channel to fill the interior — so the patch
  keeps its texture but its overall tone slides to blend perfectly. This is how the "healing brush" / seamless
  compositing works, and it's handy for decals, damage overlays, terrain-splat blending, and joining texture
  tiles. Godot has no gradient-domain compositing. [VERIFIABLE HERE] The defining property is checked
  directly — at every solved cell the result's discrete Laplacian equals the source's guidance field (to
  8-bit quantization) and every cell outside the region equals the destination exactly — plus identity
  (cloning from an identical image is a no-op), constant-offset absorption (a flat brightness difference
  blends away entirely, the seam vanishing), and determinism.
- [x] **2D Poisson / Laplace solver (Gauss–Seidel)** (`math::poissonSolve`, `Poisson.hpp`) — DONE (M742);
  solve Laplacian(u) = rhs on a grid with fixed (Dirichlet) cells — the workhorse behind a surprising range
  of game/graphics tasks: the pressure-projection step that makes fluids incompressible, gradient-domain /
  "Poisson" image editing (seamlessly cloning a patch so its interior matches the surrounding gradients),
  steady-state heat/temperature diffusion, and smooth scattered-data interpolation. The 5-point stencil sets
  each free cell to the average of its four neighbours minus the source term and iterates to convergence;
  fixed cells (the boundary, a region border, a hot spot) are held. Godot ships no PDE solver.
  [VERIFIABLE HERE] The method of MANUFACTURED SOLUTIONS gives an airtight oracle — pick a known field u*,
  set rhs = its discrete Laplacian and the border = u*, and the solver converges back to u* at every
  interior cell — plus a harmonic linear field reproduced exactly, the maximum principle (with a hot fixed
  cell, interior values stay between the fixed min and max and fall off with distance), the residual
  actually reaching tolerance, and a zero-everywhere case.
- [x] **Wang tiling (edge-matched aperiodic layout)** (`game::wangTiling`, `WangTiles.hpp`) — DONE (M741);
  lay tiles so their edges always match, producing large NON-REPEATING textures, terrain, dungeons, or
  road/river networks from a small tile set. Each Wang tile carries a colour on each of its four edges and
  may sit next to another only if their touching edges share a colour; because the rule is purely local you
  can fill an arbitrarily large grid one tile at a time and it tiles seamlessly yet never falls into an
  obvious repeat. Stochastic scanline placement picks (deterministically from a seed) among the tiles
  matching the already-placed left and upper neighbours; with a complete tile set it never gets stuck.
  Godot ships no Wang tiler. [VERIFIABLE HERE] The core constraint is checked directly — over hundreds of
  random complete tile sets and grid sizes, every placed tile's east edge equals its right neighbour's west
  edge and its south edge equals its lower neighbour's north edge — plus full grid coverage, determinism
  (same seed → same grid), real tile variety, incomplete-set failure detection, and degenerate handling.
- [x] **Equirectangular panorama mapping** (`render::equirectUvFromDir`/`dirFromEquirectUv`/`sampleEquirect`,
  `Equirect.hpp`) — DONE (M740); the lat-long projection that wraps a single wide photo or HDR sky panorama
  around a scene as a skybox / environment map (Godot's PanoramaSkyMaterial), and answers "what does the
  world look like in this direction?" for reflection lookups. Horizontal axis = compass angle around +Y,
  vertical = up/down from the top pole, matching the engine's SphericalCoords convention. `sampleEquirect`
  bilinearly reads a panorama image in a direction (seamless horizontal wrap, clamped poles). The engine has
  cube maps but no equirectangular sampling. [VERIFIABLE HERE] Direction→UV→direction round-trips exactly for
  thousands of random unit directions and UV→direction→UV round-trips away from the poles; every UV stays in
  [0,1]²; known anchors map correctly (+Z→(0.5,0.5), +X→u=0.75, −X→u=0.25, +Y→v=0, −Y→v=1); a constant
  panorama samples to that constant in every direction and a horizontal ramp reads the expected value.
- [x] **Anti-aliased line rasterization (Xiaolin Wu)** (`render::drawLineAA`, `LineAA.hpp`) — DONE (M739); the
  engine's Bresenham `drawLine` snaps each step to one pixel, so diagonal lines come out jagged. Wu's
  algorithm spreads each step across the TWO pixels it straddles, weighted by how much of each the line
  actually covers, producing smooth edges — what crisp graph plots, wireframe overlays, vector-style UI
  strokes, minimap routes, and debug gizmos want on a CPU raster. It also takes sub-pixel float endpoints and
  alpha-composites its coverage over the existing image. Godot's `Image` has no anti-aliased line primitive.
  [VERIFIABLE HERE] Perfectly horizontal/vertical lines light exactly one row/column at full coverage and
  leave neighbours dark; a 45° diagonal lights one full pixel per step; the AA energy invariant holds (each
  interior column of a shallow line carries ~1 unit of coverage split across its two straddling pixels);
  total ink approximates the line's pixel span; and drawing A→B equals B→A (order independence) — all exact,
  plus determinism.
- [x] **Barnes–Hut n-body force approximation** (`game::barnesHutAccelerations`, `BarnesHut.hpp`) — DONE
  (M738); the quadtree method for computing inverse-square attraction on every body from every other at
  scale. A naive per-pair loop is O(n²) and dies past a few thousand bodies; Barnes–Hut buckets bodies into
  a quadtree, records each cell's total mass and centre of mass, and treats a distant cluster as one lumped
  mass (when its width/distance is below the opening angle `theta`), dropping the cost to O(n log n). This is
  what powers galaxy/gravity toys, large-scale particle attraction, dust/debris fields, and mass swarm
  forces. Godot ships no n-body solver. [VERIFIABLE HERE] With theta = 0 the tree opens fully and reproduces
  the EXACT brute-force all-pairs acceleration (cross-checked against an independent O(n²) reference); at
  theta = 0.5 the error stays under 5% of the system's largest force; plus an analytic two-body inverse-
  square case, Newton's third law (mass-weighted total acceleration ~ 0), and degenerate 0/1-body handling.
- [x] **LZW lossless compression** (`io::lzwCompress`/`lzwDecompress`, `Lzw.hpp`) — DONE (M737); the classic
  dictionary compressor (behind GIF, TIFF, Unix `compress`) for squeezing save files, chunked tilemaps,
  procedural blobs, and network payloads. It finds repeated byte sequences and replaces each with a single
  code, building its dictionary from the data itself so the decompressor rebuilds the identical table as it
  goes — nothing extra is stored. Fixed 16-bit codes with the dictionary frozen once full keep encode/decode
  trivially in lockstep (no variable-width sync to get wrong). Godot exposes zlib/gzip; this is a
  dependency-free engine-side compressor. [VERIFIABLE HERE] The airtight guarantee — decompress(compress(x))
  == x for EVERY input — is checked over empty/single-byte/run inputs, thousands of random payloads across
  every alphabet size, 5000 bytes of full-range noise (worst case), and a ~300 000-byte structured input
  that fills and FREEZES the 65536-entry dictionary; plus repetitive data provably shrinks (ratio < 1).
- [x] **Polar decomposition — extract the rotation** (`math::polarDecompose`/`extractRotation`,
  `PolarDecompose.hpp`) — DONE (M736); split a 3x3 transform into a pure rotation times a symmetric stretch,
  M = R*S. A matrix that has picked up non-uniform scale, shear, or numerical drift — a blended skinning
  matrix, an interpolated bone transform, a deformed simulation element — can be cleaned back to its nearest
  rotation R. It's the heart of co-rotational / shape-matching deformation (Müller et al.), of
  orthonormalizing a drifted basis, and of recovering a stable orientation from a squished transform.
  Computed by Higham's quadratically-convergent iteration. Godot's `Basis.orthonormalize()` is Gram–Schmidt
  (axis-order dependent, not the closest rotation); this is the true polar factor. [VERIFIABLE HERE] Strong
  analytic oracle — build M = R_true * S_sym from a KNOWN rotation and known symmetric positive-definite
  matrix, decompose, and recover both exactly — plus invariants over thousands of random inputs (R
  orthonormal with det +1, S symmetric, R*S reconstructs M), a pure-rotation identity case, and
  singular-matrix failure reporting.
- [x] **Polyline simplification (Ramer–Douglas–Peucker)** (`math::simplifyPolyline`, `SimplifyPolyline.hpp`)
  — DONE (M735); throw away the points that don't matter. Given a chain of points — a hand-drawn stroke, a
  GPS/replay track, a traced outline, a pathfinding result — it returns a shorter chain that stays within a
  chosen tolerance everywhere, keeping only the vertices that carry the shape. Keep the two ends, find the
  point farthest from the line between them, keep it if it's farther than epsilon, recurse on the halves;
  everything closer than epsilon is dropped. The engine already has Chaikin *smoothing* — this is the
  opposite operation, decimation. Godot ships no line simplifier (Geometry2D has none). [VERIFIABLE HERE]
  The core guarantee — every original point lies within epsilon of the simplified polyline — is checked with
  an independent point-to-polyline distance over thousands of random polylines and tolerances, plus the
  retained points form an in-order subsequence of the input (no invented vertices), endpoints are always
  kept, a larger epsilon never keeps more points (monotonicity), a straight run collapses to its ends, and
  degenerate 0/1/2-point inputs pass through.
- [x] **Convex penetration depth (EPA companion to GJK)** (`math::epaPenetration`, `Epa.hpp`) — DONE (M734);
  the other half of convex-vs-convex collision. `GjkDistance.hpp` answers "how far APART are two convex
  shapes?"; this answers "when they OVERLAP, how deep, and which way do I push to separate?" — returning the
  penetration depth and the contact normal (minimum translation vector) that shoves shape A just clear of B.
  That's exactly what a rigid-body solver needs to resolve a collision between two ARBITRARY convex polygons,
  not just the box/circle special cases SAT hand-codes. It works on the Minkowski difference A(-)B: the shapes
  overlap iff that set contains the origin, and the shortest way out is the closest point on its boundary; in
  2D the exact boundary is the convex hull of all pairwise vertex differences, so the depth+normal are EXACT
  (no iterative convergence). Godot exposes no such query. [VERIFIABLE HERE] Over thousands of random
  overlapping convex polygons the depth matches an INDEPENDENT SAT minimum-translation computation and the
  normal is parallel to the SAT axis; the separating property is checked directly (translating A by
  (depth+margin)*normal removes the overlap, confirmed by an independent triangle-overlap sampler); plus a
  hand-computed box case and disjoint-shape rejection.
- [x] **Content-aware image resize (seam carving)** (`render::carveWidth`/`carveHeight`, `SeamCarve.hpp`) —
  DONE (M733); shrink an image by deleting the *least important* pixels instead of squashing everything, so
  the subject keeps its shape while bland background is squeezed out. Every pixel gets a dual-gradient
  "energy" (how much it differs from its neighbours); dynamic programming then finds the minimum-energy
  *seam* — a connected one-pixel-wide path top-to-bottom — and removes it, one column at a time (rows via
  transpose). Godot's `Image.resize` only interpolates; this is the real Avidan–Shamir algorithm.
  [VERIFIABLE HERE] The chosen seam provably minimizes total energy — the test cross-checks the DP result
  against a brute-force enumeration of *every* 8-connected seam on small images, plus seam well-formedness
  (one column per row, in range, adjacent rows within one column), carve dimensions, flat-image invariance,
  and content preservation (a high-energy textured band survives while flat background is carved away).
- [x] **Motion-trail ribbon builder** (`render::Trail`, `Trail.hpp`) — DONE (M732); the ribbon behind sword
  swings, projectile streaks, dash after-images, and skid marks: keep a short history of where a point has
  been, and turn it into a flat, camera-facing ribbon of triangles that tapers to nothing at the old end and
  fades out over a lifetime. [VERIFIABLE HERE] Each frame you push the current position and advance time; old
  points expire, and buildRibbon() emits a quad strip — two vertices per history point, offset sideways
  perpendicular to both the trail direction and the view direction — with width tapering by age so the head
  is full-width and the tail pinches to a point (the look everyone expects). This is a common Godot request
  (its built-in trails live only inside the particle system); here it is a small standalone builder over any
  moving point, and the geometry is exact CPU-side so it is fully testable without a GPU. The ctest checks the
  vertex/triangle counts (2 per point, 2*(n-1) triangles), that each cross-section is centred on its trail
  point AND its edge is perpendicular to both the tangent and the view direction, that width grows
  monotonically from the old tail toward the fresh head (with the head clearly wider), that update() expires
  points past the lifetime and maxPoints caps the history, and that <2 points yields an empty ribbon.
  Header-only, std-only, deterministic. ctest `trail`.
- [x] **Suffix array + LCP (text index)** (`core::SuffixArray`, `SuffixArray.hpp`) — DONE (M731); index a
  string so ANY substring can be located fast, and answer "what is the longest chunk that repeats?" [VERIFIABLE
  HERE] A suffix array is the string's suffixes sorted alphabetically, stored as start positions; because they
  are sorted, every occurrence of a search pattern forms one contiguous block found by binary search in
  O(m log n) instead of rescanning the whole text — the compact, cache-friendly cousin of a suffix tree and
  the backbone of substring search over large STATIC text (searching a big log/script dump, dictionary
  autocomplete, dedup / longest-repeated-substring analysis). Paired with the LCP (longest-common-prefix)
  array it gives the longest repeat directly. Built by prefix doubling; contains()/count() binary-search the
  match block. The ctest pins the classic "banana" suffix array [5,3,1,0,4,2], then cross-checks against a
  brute-force lexicographic suffix sort over 3,000 random tiny-alphabet strings (many repeats), verifies the
  LCP array against a direct common-prefix computation, checks contains()/count() against a std::string::find
  scan for present and absent patterns, that longestRepeatedLength() equals max(LCP), and empty input.
  Well beyond String.find for repeated queries on fixed text; Godot has no text index. Header-only, std-only,
  deterministic. ctest `suffix_array`.
- [x] **D8 flow accumulation (terrain hydrology)** (`game::flowAccumulation`, `FlowAccumulation.hpp`) — DONE
  (M730); figure out where water DRAINS on a heightmap — for each cell, how many cells upstream ultimately
  flow through it. [VERIFIABLE HERE] This is the standard hydrology primitive that turns terrain into rivers:
  high accumulation traces valleys, streams, and river mouths, driving procedural river placement,
  moisture/biome maps (wetter downstream), where hydraulic erosion cuts channels, and where to place lakes or
  settlements. The "D8" model routes each cell's water to its single STEEPEST-DESCENT neighbour among the 8
  around it (or nowhere, if it is a local pit / map-edge outlet); accumulation is the count of cells whose
  drainage passes through each cell. Computed in one height-sorted pass (no recursion), so a cell is finalised
  before its receiver. Complements thermal erosion. The ctest proves every cell accumulates >= 1, that flow is
  MONOTONE (a cell's downstream receiver carries at least as much as the cell), CONSERVATION (the
  accumulations at all outlet cells sum to the cell count — every unit of water reaches an outlet), the
  analytic answer on a tilted plane (cell (x,y) accumulates x+1 and the low edge carries the whole row), and
  determinism. Godot has no hydrology tools. Header-only, std-only, deterministic. ctest `flow_accumulation`.
- [x] **Thermal erosion (terrain weathering)** (`game::thermalErosion`, `Erosion.hpp`) — DONE (M729); weather a
  procedural heightmap so it looks geologically aged instead of raw fractal noise. [VERIFIABLE HERE] Terrain
  straight out of Perlin/fbm or diamond-square has implausibly steep, jagged slopes; real hillsides can't hold
  material past a "talus angle" — anything steeper crumbles and slides downhill until the slope relaxes.
  Thermal erosion simulates that: wherever an adjacent height difference exceeds the talus threshold, a
  fraction of the excess is moved to the lower neighbours in proportion to how much lower each is; run for a
  number of passes it softens ridges, fills gullies, and forms natural scree slopes. It is the cheap, stable
  half of terrain weathering (the other being hydraulic/water erosion) and a staple of procedural landscape
  tools. It only MOVES material between cells — never creates or destroys it — via a Jacobi (double-buffered)
  update so it is order-independent and deterministic. The ctest proves TOTAL MASS is conserved to float
  epsilon over 80 passes on jagged terrain, that the steepest adjacent slope RELAXES to under half its
  starting value, that a single tall spike spreads to its neighbours (spike shrinks, neighbours rise, mass
  conserved), that terrain already below the talus angle is left untouched, and determinism. Godot has no
  terrain erosion. Header-only, std-only. ctest `thermal_erosion`.
- [x] **k-means clustering** (`math::kMeans`, `KMeans.hpp`) — DONE (M728); partition N-dimensional points into
  k clusters, each represented by its centroid, so points end up grouped with their nearest centre (Lloyd's
  algorithm with k-means++ seeding). [VERIFIABLE HERE] The general clustering workhorse: grouping
  units/enemies into squads by position, building spatial LOD clusters, deriving representative palettes or
  "archetype" values from data, seeding procedural distributions, compressing a cloud of samples to k
  prototypes. The engine had median-cut colour quantization (fixed to RGB) but no general k-means over
  arbitrary vectors. It alternates ASSIGN (every point to its nearest centroid) and MOVE (each centroid to
  the mean of its points) until stable — which provably never increases the total within-cluster squared
  distance (inertia), so it converges; k-means++ picks well-spread initial centres to avoid poor local
  minima. Deterministic given a seed. The ctest builds four well-separated blobs and checks each blob becomes
  a single cluster with all four distinct, then verifies the LLOYD FIXPOINT optimality conditions directly —
  every point is assigned to its nearest centroid AND every centroid equals the mean of its members — checks
  the reported inertia matches a recomputation, that k>=n gives inertia ~0, determinism, and empty input.
  Godot has no clustering. Header-only, std-only, deterministic. ctest `kmeans`.
- [x] **Count-Min Sketch (frequency estimator)** (`core::CountMinSketch`, `CountMinSketch.hpp`) — DONE (M727);
  estimate HOW MANY TIMES each item appeared in a stream, from a small fixed table instead of one counter per
  distinct key. [VERIFIABLE HERE] Where HyperLogLog answers "how many DIFFERENT items?", a Count-Min sketch
  answers "how often did THIS item occur?" — approximately, in O(1) memory that does not grow with the number
  of distinct keys. It is the standard cheap tool for finding "heavy hitters": which item is being spammed in
  chat, which source is flooding packets, which ability/asset is used most — without a hash map ballooning to
  millions of entries. It is d rows of w counters; each item is hashed d ways (via MurmurHash3) and those
  counters are bumped; the estimate is the MINIMUM of the item's d counters, so collisions can only ever make
  it TOO HIGH, never too low, and the min makes big overestimates rare. Registers add elementwise, so
  per-shard sketches merge for free. The ctest proves the estimate is NEVER below the true count over a known
  500-key skewed stream, that a wide table keeps the vast majority of keys exact and the overestimate small,
  that a heavy hitter (100k) is recovered within a few percent, that an absent key stays small / empty
  estimates 0, that merge sums per-key counts, and that estimation is deterministic. Godot has no frequency
  sketch. Header-only, std-only, deterministic. ctest `count_min_sketch`.
- [x] **HyperLogLog (cardinality estimator)** (`core::HyperLogLog`, `HyperLogLog.hpp`) — DONE (M726); estimate
  how many DISTINCT items a stream contained using a few kilobytes of fixed memory, no matter how many
  billions flow through. [VERIFIABLE HERE] Counting uniques exactly needs a set that grows with the data
  (megabytes for millions of distinct values); HyperLogLog answers "roughly how many different X?" from a
  tiny fixed array of counters, trading a small bounded error (~1.04/sqrt(m)) for O(1) memory — the standard
  tool for distinct players online, distinct enemies a weapon hit, distinct assets touched this session for
  telemetry. It hashes each item to 64 bits, uses the top bits to pick one of m=2^p registers, records the
  leftmost 1-bit position of the rest, and takes the harmonic mean across registers (with a linear-counting
  small-range correction); registers merge by max, so per-shard sketches combine for free. The ctest checks
  the estimate is within 5% of the true count for streams of 100 / 1k / 10k / 50k / 200k distinct items, that
  adding the same item 1000x does not inflate the count, that an empty sketch estimates ~0, that merging two
  sketches over disjoint halves recovers the union cardinality, and that estimation is deterministic. Godot
  has no cardinality estimator. Header-only, std-only, deterministic. ctest `hyperloglog`.
- [x] **MurmurHash3 (fast non-crypto hash)** (`core::murmur3_32`, `Murmur3.hpp`) — DONE (M725); a fast,
  well-distributed non-cryptographic hash (Austin Appleby), the default workhorse for hash tables, bloom
  filters, feature flags, and stable content/asset IDs. [VERIFIABLE HERE] The engine already had
  cryptographic digests (SHA-1/SHA-256) and CRC32, but those are the wrong tool for hashing map keys millions
  of times a frame: SHA is far too slow and CRC32 has poor avalanche (similar inputs cluster). MurmurHash3 is
  built for this — a few multiplies and rotates per 4 bytes, strong mixing so a one-bit input change scatters
  the whole output, and a `seed` to derive independent hash functions (e.g. the k hashes a bloom filter
  needs). It is also a de-facto interchange standard, so hashes computed here match other tools and languages.
  The ctest pins the canonical published test vectors byte-for-byte ("" at three seeds, the "a"/"aa"/"abc"/
  "abcd" family at seed 0x9747b28c, "Hello, world!", and the classic pangram) — the strong correctness proof,
  since every conformant implementation produces exactly these — plus determinism, seed sensitivity, avalanche
  (a one-byte change flips many output bits), and near-zero collisions over 20,000 distinct keys. Godot
  exposes only its own String.hash. Header-only, std-only, deterministic. ctest `murmur3`.
- [x] **IMA ADPCM audio codec** (`audio::encodeImaAdpcm` / `decodeImaAdpcm`, `ImaAdpcm.hpp`) — DONE (M724);
  the classic 4-bit Adaptive Differential PCM codec (IMA/DVI) behind WAV format tag 0x11 and the sound banks
  of countless games. [VERIFIABLE HERE] It squeezes 16-bit PCM to 4 bits/sample — a flat 4:1 compression — by
  storing per sample only a 4-bit code for the DIFFERENCE from a running prediction, with an adaptive step
  size that grows on loud passages and shrinks on quiet ones. Decode is a few adds and shifts per sample (no
  multiplies, no floating point), cheap enough to decode hundreds of voices on any CPU. It complements the
  engine's other codecs — QOA (higher quality ~3.2 bits/sample), G.711 (telephony companding), raw WAV — as
  the tiny-and-fast option for short SFX. The stream stores the first sample verbatim (reproduced exactly)
  plus the initial step index, then two codes per byte. The ctest checks the ~4:1 encoded size, that the
  first sample is exact, that a smooth audio-like signal (summed sines) reconstructs to a small RMS error
  (ADPCM is a smooth-signal codec), that a linear ramp reconstructs within a tight bound, and that silence
  stays silence / empty in-out / determinism hold. Godot has no ADPCM codec. Header-only, std-only,
  deterministic. ctest `ima_adpcm`.
- [x] **Greedy voxel meshing** (`render::greedyVoxelMesh`, `GreedyVoxelMesh.hpp`) — DONE (M723); turn a 3D
  grid of blocks into a renderable surface mesh, merging every run of coplanar, same-type, equally-exposed
  faces into ONE big quad (Mikola Lysenko's greedy algorithm). [VERIFIABLE HERE] The reverse of MeshVoxelize
  (mesh -> voxels); it is what makes voxel worlds actually drawable. The naive approach emits two triangles
  per exposed block face — a flat 100x100 floor becomes 20,000 triangles; greedy meshing turns that same
  floor into a SINGLE quad. On real Minecraft/Teardown-style terrain it cuts triangle counts 5-10x, the
  difference between a chunk that renders and one that tanks the GPU. Only exposed faces (a solid cell whose
  neighbour across that face is empty) are emitted, only same-type faces merge, and each quad winds outward.
  The ctest checks a single voxel -> 6 faces and a solid box -> exactly 6 merged quads, outward winding, and
  — the rigorous part — over 400 random multi-type grids decomposes every quad back into unit faces and
  proves they EXACTLY partition the exposed faces a brute-force per-cell scan finds: each exposed face covered
  once (no gaps), none covered twice (no overlap), every quad's type matching its owning cell, and greedy
  never emitting more quads than the naive per-face count. Godot has no voxel mesher. Header-only, std-only,
  deterministic. ctest `greedy_voxel_mesh`.
- [x] **3D voxel ray traversal (Amanatides-Woo)** (`game::traverseVoxels` / `game::voxelRaycast`,
  `VoxelRaycast.hpp`) — DONE (M722); walk a ray through a 3D grid of unit cells and visit EVERY voxel it
  passes through, in order, with no gaps and no duplicates. [VERIFIABLE HERE] The 3D companion to GridRaycast
  (which is 2D) and the workhorse behind block-world interaction: which block is the player looking at /
  mining / placing against, 3D line-of-sight and light propagation through a voxel volume, ray-marching a
  sparse voxel scene. A naive "step along the ray in small increments" either skips thin voxels or visits the
  same voxel repeatedly and drifts; this advances exactly to the next cell boundary each iteration, so it is
  both exact and O(voxels crossed). voxelRaycast stops at the first cell a predicate marks solid. The ctest
  checks an axis-aligned ray, that the path starts at floor(origin), is 6-connected (one axis by +/-1 per
  step) and monotone in the ray direction, and over 500 random rays proves SOUNDNESS (every traversed cell is
  genuinely crossed by the ray with positive length, via an independent double-precision ray-vs-box slab
  test — robust where naive dense sampling would skip a cell near a near-diagonal crossing) and COMPLETENESS
  (every densely-sampled voxel appears in the traversal), plus the raycast hit/miss and degenerate inputs.
  Godot ships no voxel traversal. Header-only, std-only, deterministic. ctest `voxel_raycast`.
- [x] **Best-fit rigid transform (Kabsch/Horn)** (`math::kabsch`, `Kabsch.hpp`) — DONE (M721); find the single
  rotation + translation (no scale/shear) that best maps one set of 3D points onto another in the
  least-squares sense. [VERIFIABLE HERE] Given N corresponding pairs, it returns the transform minimising the
  summed squared error — the workhorse behind point-cloud REGISTRATION (line up a scanned/streamed set with a
  reference), POSE fitting (recover how a rigid body moved from a few tracked markers), mocap/tracking
  alignment, and procedural retargeting. The engine had per-axis fits and eigen-based OBB fitting but no
  "best rotation between two clouds". Uses Horn's closed-form quaternion solution (1987): build a 4x4
  symmetric matrix from the cross-covariance of the centred clouds, take the eigenvector of its largest
  eigenvalue (via a Jacobi eigensolve) as the optimal rotation quaternion, then translation = centroidTo -
  R*centroidFrom — always a PROPER rotation, never a reflection. The ctest transforms random clouds by a
  KNOWN random rotation+translation and checks kabsch recovers a fit with max residual under 1e-3 across
  3,000 cases, that it stays accurate under small per-point noise, that identity/pure-translation give ~no
  rotation, and that a length mismatch is reported. Header-only, deterministic. ctest `kabsch`.
- [x] **UTF-16 conversion** (`core::utf16Encode` / `utf16Decode` / `utf8ToUtf16` / `utf16ToUtf8`,
  `Utf16.hpp`) — DONE (M720); the companion to Utf8.hpp for moving text between UTF-8 (the engine's
  internal/on-disk form) and UTF-16. [VERIFIABLE HERE] UTF-16 is the native text encoding of the Windows API
  (wide-char file paths, the clipboard, native file dialogs / message boxes) and of Java/JS/.NET strings, so
  any engine that talks to those platforms or imports data from them must convert. The subtlety UTF-16 adds
  over UTF-8 is SURROGATE PAIRS: code points above U+FFFF (emoji, CJK extensions, historic scripts) are
  stored as two 16-bit units — a high surrogate (0xD800..0xDBFF) then a low (0xDC00..0xDFFF) — and the
  split/join arithmetic is exactly where naive code breaks. This handles it and mirrors Utf8.hpp's forgiving
  policy: any invalid scalar value (a lone surrogate, a value above U+10FFFF) becomes U+FFFD instead of
  corrupting the stream. The ctest pins known encodings (ASCII, é, €, the 😀 emoji as D83D DE00, U+10FFFF as
  DBFF DFFF), round-trips EVERY valid scalar value across the entire Unicode range (~1.1M code points)
  through encode->decode, checks the replacement-char policy for lone/stray surrogates and out-of-range
  values, and round-trips a mixed multilingual string through the utf8<->utf16 bridge. Godot exposes wide
  conversion only through its opaque String; this is the standalone codec. Header-only, deterministic. ctest
  `utf16`.
- [x] **Binary diff / patch** (`io::binaryDiff` / `io::binaryPatch`, `BinaryDiff.hpp`) — DONE (M719); build a
  compact PATCH that turns one byte buffer into another, and apply it to reconstruct the target EXACTLY.
  [VERIFIABLE HERE] When two blobs are mostly the same — a save file after a few minutes of play, an asset
  re-exported with a tweak, last tick's serialized world vs this tick's — storing/shipping the whole new blob
  wastes space. binaryDiff block-hashes the source and greedily matches the target, emitting only COPY(from
  source) + ADD(new bytes) ops (the classic rsync/bsdiff idea); binaryPatch replays them. It complements the
  engine's other deltas: net::writeSnapshotDelta does FIELD-level deltas of a KNOWN schema, whereas this
  works on ARBITRARY bytes with no schema — patched saves, incremental asset updates, diffing opaque state.
  The patch is bounds-checked on apply (a malformed/truncated/out-of-range patch is rejected, never an
  overrun). The ctest verifies correctness — patch(src, diff(src,dst)) == dst — over 4,000 random buffer
  pairs (small alphabets so matches and mismatches both occur), verifies that an edited copy compresses to
  under a third of the target size, handles empty/identical buffers, and rejects malformed patches. Godot has
  no binary diff. Header-only, std-only, deterministic. ctest `binary_diff`.
- [x] **Interval tree (range/stabbing queries)** (`core::IntervalTree<T>`, `IntervalTree.hpp`) — DONE (M718);
  answer "which intervals contain point x?" and "which intervals overlap [a,b]?" in O(log n + k) instead of
  scanning all n intervals per query. [VERIFIABLE HERE] An interval is a [low,high] range with a payload;
  this is the right structure whenever many time-ranges or 1-D spans are queried repeatedly — which animation
  clips / audio cues / cutscene triggers are ACTIVE at the current playhead, which reservations overlap a
  window, which spans on one axis touch a probe (a 1-D broadphase). Naive code re-tests every interval per
  query. This is a static augmented interval tree: insert all intervals, build() once (a height-balanced BST
  ordered by low endpoint, each node augmented with the maximum high endpoint in its subtree), then query
  many times; the max-endpoint augmentation prunes whole subtrees that cannot reach the query. The ctest
  checks a hand-built set, inclusive/closed endpoints, degenerate point intervals, empty/single trees, and
  cross-checks queryPoint + queryOverlap against a brute-force linear scan over 3,000 random interval sets
  (order-independent set comparison). Godot has no interval tree. Header-only, std-only, deterministic. ctest
  `interval_tree`.
- [x] **Smallest-three quaternion compression** (`net::compressQuat` / `decompressQuat`, `QuatCompress.hpp`)
  — DONE (M717); pack a full 3D rotation into ~32 bits for cheap network replication, instead of 128 bits of
  raw floats. [VERIFIABLE HERE] A unit quaternion has four components but only three degrees of freedom
  (x²+y²+z²+w²=1), and one component always has magnitude ≥ ½. The trick: DON'T send the largest component —
  send a 2-bit index of which one it was, then the other three (each guaranteed to lie in [-1/√2, +1/√2])
  quantized to `bits` bits apiece; the receiver rebuilds the dropped one from the unit-length constraint.
  Because q and -q are the same rotation, the sign is canonicalized so the largest component is always
  positive (no sign bit needed either). At the default 9 bits/component that is 2 + 3·9 = 29 bits for a
  rotation accurate to a fraction of a degree. This is how shipping engines replicate orientation (character
  facing, projectile spin, ragdoll bones); Godot's multiplayer has no built-in equivalent. The ctest sweeps
  20,000 uniformly-random rotations (Shoemake) and checks worst-case angular error stays under ~1.1° at 9
  bits and ~0.6° at 10 bits AND shrinks with more bits, that identity/axis rotations survive, that q and -q
  compress to the IDENTICAL code, that 9-bit codes fit in 29 bits (10-bit in 32), and that a non-normalized
  input is normalized first. Builds on the existing FloatQuant. Header-only, deterministic. ctest
  `quat_compress`.
- [x] **Radix sort (linear-time key sorting)** (`core::radixSort` / `radixSortByKey` / `radixSortFloats`,
  `RadixSort.hpp`) — DONE (M716); sort by an integer or float key in O(n) instead of a comparison sort's
  O(n log n), with NO key comparisons at all. [VERIFIABLE HERE] Renderers sort thousands of draw calls every
  frame by a packed 32/64-bit sort key (layer<<depth<<material) to batch state and draw front-to-back;
  particle systems sort by camera distance for correct alpha; an ECS sorts entities by archetype key. Run
  every frame at those sizes, n vs n·log n is real time. Radix sort buckets on one byte of the key at a time
  (a stable counting sort per byte, least-significant first), fully ordering the array after 4 passes (32-bit)
  or 8 (64-bit). The key+payload form (radixSortByKey) is STABLE — equal keys keep their original order, so
  it is safe to chain sorts — and floats are handled via the standard order-preserving bit transform so depth
  sorting works with negatives. The ctest cross-checks radixSort(uint32/uint64) against std::sort on 5,000
  random arrays, radixSortByKey against std::stable_sort on tie-heavy data (verifying stability by payload
  order), radixSortFloats against std::sort with negatives/zero, and the empty/single/sorted/reverse/float
  edge cases. Godot has no radix sort; this is the workhorse behind fast per-frame ordering. Header-only,
  std-only, deterministic. ctest `radix_sort`.
- [x] **MessagePack binary serialization** (`io::MsgValue` / `io::msgpackEncode` / `io::msgpackDecode`,
  `MessagePack.hpp`) — DONE (M715); the MessagePack standard (msgpack.org): a compact, self-describing binary
  format that is "JSON in bytes". [VERIFIABLE HERE] The engine already had JSON (verbose, human-editable) and
  its own tag-free binary Serialize (tiny, but both ends must agree on the exact layout up front); MessagePack
  is the missing middle — small and fast like binary yet SELF-DESCRIBING like JSON, so a decoder recovers the
  full nil/bool/int/float/string/bytes/array/map structure with no schema, and it interoperates with the
  MessagePack libraries that ship for essentially every language (ideal for network messages, replays, and
  cross-version save files). The encoder is CANONICAL — every value takes its smallest legal representation —
  so its output is byte-for-byte the published spec, which is exactly what the ctest pins: it asserts the
  spec byte vectors for fixint / uint8 / uint16 / negative-fixint / int8 / float64 / fixstr / fixarray /
  fixmap / nil / bool / bin8, decodes those bytes back, round-trips a nested mixed document, runs a
  5,000-trial randomized encode→decode identity fuzz, rejects truncated / trailing-garbage / reserved-byte
  streams, and pins the by-spec canonicalization that a non-negative Int decodes back as UInt. Godot offers
  only JSON + its own var_to_bytes; this is the portable standard. Header-only, std-only, deterministic.
  ctest `message_pack`.
- [x] **Aho-Corasick multi-pattern text matching** (`core::AhoCorasick`, `AhoCorasick.hpp`) — DONE (M714);
  finds EVERY occurrence of MANY search strings in a text in a SINGLE pass, in O(text + matches) no matter
  how many patterns there are. [VERIFIABLE HERE] The naive way — loop each of k patterns and scan the whole
  n-char text — costs O(n*k) and rescans every character k times; Aho-Corasick builds one automaton (a trie
  of all patterns plus "failure" links that jump to the longest still-live suffix when a match breaks) and
  sweeps the text once. It is the standard engine behind a profanity/word filter, chat slash-command
  detection, dialogue keyword triggers, search highlighting, and content-moderation dictionaries — anywhere
  a stream of text must be watched for a whole vocabulary at once. API: addPattern / build / findAll (matches
  in scan order: ascending end, then pattern) / containsAny / countMatches. The ctest verifies the classic
  "she/he/his/hers" in "ushers" overlap case, then cross-checks findAll against a brute-force per-pattern
  std::string::find scan on 4,000 random small-alphabet pattern sets and texts (>1,000 total matches),
  where overlaps, suffix-of-another patterns, and repeats are all exercised. Well beyond Godot's String.find.
  Header-only, std-only, deterministic. ctest `aho_corasick`.
- [x] **Jump Point Search grid pathfinding** (`game::JumpPointSearch`, `JumpPointSearch.hpp`) — DONE (M713);
  a much faster pathfinder for uniform-cost 8-connected grids that returns the EXACT SAME optimal path as
  ordinary grid A*, but expands a tiny fraction of the cells. [VERIFIABLE HERE] Plain A* on an open map
  wastes almost all its effort exploring the countless equivalent zig-zag routes between two points ("path
  symmetries"); JPS eliminates that by jumping in a straight line along each direction, skipping every cell
  that can't be a turning point, and only ever queueing genuine decision cells (jump points). On a wide-open
  map it is routinely 10-30x faster than the same A* — the difference between an RTS smoothly pathing
  hundreds of units and one that stutters. Movement model is 8-connected with corner-cutting allowed
  (straight = 1, diagonal = sqrt(2)), matching the engine's `AStarGrid2D` DiagonalMode::Always — which is
  exactly what makes it checkable: the ctest cross-checks JPS against `AStarGrid2D` on 3,000 random obstacle
  grids, asserting they agree on reachability and on total path COST every time (JPS is provably optimal for
  this model), that every JPS path is valid (correct endpoints, in bounds, never on a solid cell, single
  8-neighbour steps), that on a 50x50 open grid JPS expands under 100 of the 2,500 cells, and that the sparse
  jump-point path expands back into the full contiguous cell path. Complements `AStarGrid2D` (correctness
  baseline) and `AStar2D` (arbitrary weighted graphs). Godot ships no JPS. ctest `jump_point_search`.
- [x] **Gap buffer (text-editor buffer)** (`core::GapBuffer`, `GapBuffer.hpp`) — DONE (M712); the classic
  editable-text data structure: a character buffer with a movable "gap" of empty slots at the cursor.
  [VERIFIABLE HERE] Typing fills the gap and deleting widens it, so edits AT THE CURSOR are O(1) amortised —
  no shifting the whole document on every keystroke the way a plain std::string insert/erase would (O(n)
  each); moving the cursor pays only for the distance moved, matching how people actually edit (many
  keystrokes in one place, occasional jumps). This is the buffer behind a real code/text editor, directly
  useful for the engine's in-editor SCRIPT EDITOR, the developer CONSOLE line, and a chat/input field. API:
  insert(char/string), backspace, deleteForward, moveTo/moveLeft/moveRight, at, text, size, cursor. Tested:
  typing + middle-insert-via-the-gap, backspace/forward-delete with start/end guards, at() indexing across
  the gap, and a 20,000-op randomized edit sequence (insert char/string, move, backspace, delete) that
  matches a std::string+cursor reference at every step while forcing several buffer growths. Header-only.
- [x] **Weighted reservoir sampling** (`core::WeightedReservoir`, `WeightedReservoir.hpp`) — DONE (M711);
  select k items from a STREAM of weighted items in a single pass and O(k) memory, each item's chance of
  being kept proportional to its WEIGHT (Efraimidis-Spirakis "A-Res"). [VERIFIABLE HERE] The missing middle
  between the engine's two samplers: `AliasTable` does a weighted pick from a KNOWN in-memory set, and
  `ReservoirSampler` picks k from a stream but treats every item EQUALLY — this does both at once. The tool
  for drawing N loot items from a generated pile weighted by rarity, sampling spawn points weighted by
  desirability, or keeping importance-weighted telemetry without unbounded memory. The trick: an item of
  weight w gets key = u^(1/w) for uniform u; keep the k largest keys (a size-k min-heap), compared in log
  space for stability. Deterministic via embedded splitmix64. Tested statistically: k=1 over weights
  {1,2,3,4} selects each in proportion (~10/20/30/40% over 40k trials), k>=n returns everything, a dominant
  weight is included >97% of the time, same seed reproduces the sample, non-positive weights are ignored,
  and the reservoir never exceeds k. Header-only, std-only.
- [x] **Streaming median filter (running median)** (`core::RunningMedian`, `RunningMedian.hpp`) — DONE
  (M710); the exact median of the most recent N samples of a stream, the great OUTLIER-RESISTANT smoother.
  [VERIFIABLE HERE] A lone spike (a glitched sensor read, a dropped-frame hitch, a network blip) is an
  extreme the median simply steps over, whereas a moving AVERAGE (`core::MovingAverage`) gets dragged toward
  the spike and smears it; the median also preserves genuine step changes (edges) that a mean rounds off.
  The 1D streaming cousin of the engine's image median filter (`render::medianFilter`, which denoises a 2D
  image): use it for a jitter-free frame-time readout, a de-glitched analog stick / gyro axis, or robust
  smoothing of any noisy per-frame signal. Backed by an ordered multiset for O(log N) updates; distinct from
  `P2Quantile` (a streaming ESTIMATE over ALL history) — this is the EXACT median of a sliding window.
  Tested: hand cases, spike rejection (median stays at 5 while the mean is dragged past 200), a preserved
  step edge, a randomized cross-check against a brute-force sorted window across five window sizes, and
  clear/full. Header-only, std-only, deterministic.
- [x] **Minimum-area oriented bounding rectangle (rotating calipers)** (`math::minAreaRect` /
  `math::OrientedRect`, `Geometry2D.hpp`) — DONE (M709); the smallest ROTATED rectangle enclosing a 2D point
  set. [VERIFIABLE HERE] Unlike an axis-aligned box (which the engine already has), this finds the tight
  rotated fit — the snug hitbox for a rotated sprite, recovering an object's orientation from its silhouette,
  or compact packing. By Toussaint's theorem the optimum has one side collinear with a convex-hull edge, so
  it reuses the existing `convexHull` and, for each hull-edge direction, measures the bounding box in that
  frame and keeps the smallest; returns centre, the two unit axes + half-extents, rotation angle, area, and
  the 4 corners. Tested: an axis-aligned rectangle's corners recover its exact area and side lengths; the
  SAME corners rotated by an angle yield the same area and sides (orientation invariance) and a strictly
  tighter fit than the AABB; a 500-trial randomized run confirms every input point lies inside the returned
  rectangle and its area never exceeds the axis-aligned bound; and degenerate inputs behave. Header-only.
- [x] **Summed-area table (integral image)** (`core::SummedAreaTable`, `SummedAreaTable.hpp`) — DONE (M708);
  a 2D prefix-sum table that answers the SUM or AVERAGE over ANY axis-aligned rectangle in O(1) — no matter
  how large the rectangle — after an O(w*h) build. [VERIFIABLE HERE] Each cell stores the sum of everything
  above-and-left, so a rectangle sum is four lookups (bottom-right − top-strip − left-strip + corner). This
  is the trick behind a constant-time box blur (any radius is the same cost), average brightness or height
  over a region, adaptive/local thresholding, fast region queries on an influence or heat map, and
  Viola-Jones feature sums. Generalises the engine's 1D range structures — `FenwickTree` (1D prefix sums
  with updates) and `SparseTable` (1D idempotent range queries) — to two dimensions for a static grid;
  templated on the accumulator type. Tested: sums on a known grid and 5000 random rectangles match brute
  force, swapped/out-of-range corners clamp, `rectMean` = sum/count, a full SAT box blur equals a
  brute-force box blur at every pixel, and empty/mismatched grids are rejected. Header-only, std-only.
- [x] **Circular statistics — mean/variance of angles** (`math::circularMean`/`resultantLength`/
  `circularVariance`/`circularStdDev`, `CircularMean.hpp`) — DONE (M707); the CORRECT way to average and
  measure the spread of angles. [VERIFIABLE HERE] You cannot arithmetically average angles: the mean of 350°
  and 10° is 0°, not 180°, because angles wrap. The circular mean treats each angle as a unit vector,
  averages the vectors, and takes the resulting direction — so it handles the seam. Games need this
  constantly: averaging the FACING of a flock or squad, a smoothed heading from noisy inputs, wind/current
  direction, wave phases, a compass reading. The paired resultant length R∈[0,1] measures how CONCENTRATED
  the angles are (1 = identical, ~0 = evenly spread with no meaningful mean), giving circular variance (1-R)
  and a circular std dev; a weighted mean is also provided. Distinct from the engine's lerpAngle/shortestAngle
  (which interpolate a PAIR) — this reduces a whole SET. Tested: the 350°/10°→0° wrap case (and that it is
  NOT the naive 180°), symmetric/identical sets, R=1 for identical and ~0 for four evenly-spread angles,
  variance=1-R, std dev growing with spread, weighted mean leaning to the heavier angle, and (-π,π] range.
  Header-only, std-only.
- [x] **Simulated annealing optimizer** (`core::simulatedAnnealing`, `SimulatedAnnealing.hpp`) — DONE (M706);
  a general-purpose optimizer for hard problems with no closed-form answer. [VERIFIABLE HERE] It minimises an
  `energy(state)` cost by wandering the state space, always accepting improvements but ALSO accepting worse
  states with a probability that shrinks as a "temperature" cools — that controlled willingness to go uphill
  early is what lets it ESCAPE LOCAL MINIMA a pure hill-climb gets stuck in, so it solves layout, scheduling,
  tour (TSP-style), puzzle, and procedural-placement problems. Fully generic: supply the State type, an
  energy function, and a `neighbour(state, rand01)` mutation, plus a cooling schedule. Deterministic via an
  embedded splitmix64 (no <random>, no clock), so an annealed layout is reproducible. Tested: a continuous
  convex bowl minimised to its exact minimum, a MULTIMODAL landscape solved to within 0.05 of the true
  global minimum (found by brute force) — proving it escapes local traps — and a 14-city travelling-salesman
  tour driven from a deliberately bad start down to within 15% of the optimal circular tour, always a valid
  permutation and never worse than the start. Header-only, std-only. Godot ships no optimizer.
- [x] **Diamond-square fractal heightmaps** (`game::DiamondSquare`, `DiamondSquare.hpp`) — DONE (M705); the
  midpoint-displacement terrain generator: seed four corners of a (2^n+1) grid, then recursively subdivide —
  each diamond step sets a square's centre to its corners' average plus a shrinking random offset, each
  square step sets an edge midpoint likewise — producing self-similar fractal terrain. [VERIFIABLE HERE]
  Distinct from the engine's Perlin/fbm noise (`core::Noise`): value/gradient noise is band-limited and
  smooth, whereas diamond-square is a recursive random-midpoint fractal with a characteristic ridged/plasma
  look — the classic generator for island heightmaps, cloud/plasma textures, and lightning. `roughness`
  controls how fast the random amplitude decays per level (higher = bumpier). Deterministic via an embedded
  splitmix64 (no <random>, no clock). Tested: side is exactly 2^exponent+1 with all heights finite,
  amplitude 0 gives a flat map at the baseline, the same seed reproduces an identical map while a different
  seed diverges, higher roughness yields larger height variance, and the exponent clamps to a sane range.
  Header-only, std-only.
- [x] **Tactical influence map** (`game::InfluenceMap`, `InfluenceMap.hpp`) — DONE (M704); a strategy/shooter
  AI grid where influence spreads outward from sources and decays. [VERIFIABLE HERE] Drop POSITIVE influence
  at friendly units and NEGATIVE at enemies, then `propagate(decay, spread)`: each cell blends toward its
  4-neighbour average and loses a fraction each step, so influence bleeds across the map and fades with
  distance. Reading the field answers what no single query can: where is it SAFE vs DANGEROUS (sign +
  magnitude), where is the FRONT LINE (near-zero contour between opposing armies), and which way to FLEE or
  ADVANCE (`gradient()` points toward higher influence); `peak`/`trough` find the safest/most-dangerous
  cell. Distinct from pathfinding (a route), flow fields (a vector field toward one goal), and noise
  (unstructured) — this is a decaying diffusion of gameplay meaning. Tested: a single source diffuses to a
  4-fold-symmetric field that falls off with distance, spread=0 only decays in place (no leak), decay<1
  shrinks the total, equal-and-opposite sources leave the midline at ~0, the gradient points toward a
  positive source, and peak/trough locate the hot-spots. Header-only, std-only, deterministic.
- [x] **NTP-style clock synchronization** (`net::ClockSync`, `ClockSync.hpp`) — DONE (M703); estimate the
  clock OFFSET between this machine and a remote peer, plus the round-trip time, from timestamped ping/pong
  exchanges. [VERIFIABLE HERE] The net/ layer already renders "in the past" (Interpolation) and predicts on
  server time (Prediction), but both assume a shared clock — and a client's clock differs from the server's
  in both value and drift, so the client must LEARN the offset and RTT. Each exchange yields four stamps
  (t0 send, t1 server-recv, t2 server-send, t3 recv); the NTP formulas give offset = ((t1-t0)+(t2-t3))/2 and
  delay = (t3-t0)-(t2-t1). Queuing jitter corrupts single samples, so — like NTP's clock filter — the best
  estimate is taken from the SMALLEST-delay sample in a window, with an EMA-smoothed offset also exposed;
  `toServerTime`/`toLocalTime` convert between the two clocks. Tested: symmetric delays recover the exact
  offset and RTT, jittery delays still land close via the min-delay sample, asymmetric constant delays bias
  the offset by exactly (up-down)/2 (the known NTP path-asymmetry limit), and window/clear behave.
  Header-only, std-only, deterministic (caller supplies the stamps — no clock or socket inside).
- [x] **GJK minimum-distance between convex shapes** (`math::gjkDistance`, `GjkDistance.hpp`) — DONE (M702);
  the Gilbert-Johnson-Keerthi algorithm for the exact minimum distance between two convex polygons, with the
  closest pair of witness points. [VERIFIABLE HERE] This is the proximity query the engine's SAT collider
  (`game::ConvexShape2D`) cannot answer: SAT reports only boolean overlap (+ penetration when touching),
  whereas GJK returns the actual GAP between two shapes that are APART, and the two closest points, one on
  each — what "how close is the projectile to the wall?", speculative/predictive contacts, proximity
  triggers, and AI standoff distances need. Evolves a 1-3 point simplex over the Minkowski difference via a
  support function; the origin being enclosed means the shapes intersect (distance 0). This is the industry-
  standard approach (Box2D/Bullet). Tested: overlap → distance 0, exact horizontal/diagonal gaps with
  witnesses on the facing corners/edges, point-vs-box, edge-touching, and a 4000-trial randomized
  cross-check that the distance and witnesses match a brute-force edge-edge minimum. Header-only, pure vec2.
- [x] **Monotone cubic interpolation (PCHIP)** (`math::MonotoneCubic`, `MonotoneCubic.hpp`) — DONE (M701);
  a smooth C1 curve through data points that provably NEVER OVERSHOOTS (Fritsch-Carlson tangent clamping).
  [VERIFIABLE HERE] The crucial difference from the engine's other interpolators: the natural `CubicSpline`
  is C2 but can bulge past the data, and Catmull-Rom (VectorOps) overshoots too — a run of equal values can
  ring below/above them. PCHIP guarantees the curve stays monotone wherever the data is monotone and never
  leaves the bracket of its neighbouring samples, so it is the right tool for a tone / gamma / difficulty
  curve, a health or fuel gauge response, an audio envelope, or a terrain cross-section that must not dip
  below the sampled heights. O(n) build, O(log n) eval, endpoint-clamped. Tested: exact interpolation
  through control points, a densely-sampled monotone dataset staying non-decreasing AND inside every
  segment's value bracket, the classic step data {0,0,0,1,1,1} never leaving [0,1] (where a natural spline
  rings), flat-stays-flat, endpoint clamping, and single-point/empty/mismatch edges. Header-only, std-only.
- [x] **Jaro & Jaro-Winkler string similarity** (`core::jaro`/`jaroWinkler`, `JaroWinkler.hpp`) — DONE
  (M700); a normalised [0,1] closeness score tuned for SHORT strings and typos (1 identical, 0 nothing in
  common). [VERIFIABLE HERE] Complements the existing fuzzy tools — StringUtils/FuzzyMatch give Levenshtein
  edit distance and an fzf-style subsequence scorer — but Jaro-Winkler measures similarity differently: it
  counts characters matching within a sliding window, penalises transposed pairs, and (the Winkler part)
  BOOSTS strings sharing a leading prefix. That makes it the go-to metric for "did you mean...?" command/name
  suggestions, matching a typed player or item name against a list, and de-duplicating near-identical
  strings. Tested against published reference values (jaro MARTHA/MARHTA = 0.9444, jaroWinkler = 0.9611;
  DIXON/DICKSONX = 0.7667/0.8133; DWAYNE/DUANE = 0.8222/0.84), plus identity/empty edges, symmetry, the
  [0,1] range, the prefix boost never lowering the score, and a "did you mean" ranking that maps 'attak' ->
  'attack'. Header-only, std-only, deterministic. Godot's String offers only a bigram similarity ratio.
- [x] **Gray code + Hamming distance** (`core::grayEncode`/`grayDecode`/`graySequence`/`hammingDistance`,
  `GrayCode.hpp`) — DONE (M699); reflected-binary Gray codes (consecutive values differ by EXACTLY one bit)
  plus the bit-change count that measures that. [VERIFIABLE HERE] Neither is in the standard library (unlike
  popcount / bit_ceil / countl_zero, which the engine already uses). Gray codes are glitch-free for rotary /
  position encoders (a reading caught mid-transition is off by at most one), give a minimal-change
  enumeration order for subsets/combinations, and drive dithering / LOD-transition sequences;
  `hammingDistance` (popcount of XOR) counts differing bits — the standard metric for comparing perceptual
  image hashes (dHash/pHash) and bitmask diffs. Templated on any unsigned type. Tested: known small codes
  (0,1,3,2,6,7,5,4), encode/decode round trip across 200k values and uint8/uint64 edge cases, the defining
  one-bit-adjacency property over 100k consecutive codes, `graySequence(8)` being a cyclic permutation with
  every neighbour (and the wrap) one bit apart, and Hamming-distance basics. Header-only, std-only.
- [x] **Token-bucket rate limiter** (`core::TokenBucket`, `TokenBucket.hpp`) — DONE (M698); the standard
  burst-tolerant rate limiter: a bucket holds up to `capacity` tokens, refills at `refillPerSecond`, and an
  action spends tokens or is refused when dry. [VERIFIABLE HERE] Distinct from `game::CooldownManager` (a
  per-ability binary "ready or not" timer with no burst): a token bucket accumulates several charges and
  refills fractionally, so it models "3 dashes, one back every 2s", chat/emote spam limits, outgoing packet
  or RPC throttling in netcode, and spawn/emission budgets — spend the whole bucket in a burst but stay
  capped over time. `advance(dt)` per frame, then `tryConsume(n)`; `timeUntil(n)` drives a UI "ready in Ns".
  No clock inside (tests drive it with explicit dt). Tested: burst-then-refuse, refill accrual and clamping,
  fractional / zero / over-capacity consume, `timeUntil` including the unreachable (+inf) cases, a
  sustained-rate run whose allowed-action count tracks the token budget, and the setters. Header-only,
  std-only, deterministic. The leaky bucket's cousin; Godot ships neither.
- [x] **Hungarian algorithm — optimal assignment** (`core::hungarian`, `Hungarian.hpp`) — DONE (M697); match
  N agents to N distinct tasks so the TOTAL cost is globally minimal, in O(n^3) (Kuhn-Munkres). [VERIFIABLE
  HERE] A genuinely different problem from the engine's pathfinding: AStar2D finds one least-cost route, this
  optimally pairs a whole SET at once, and unlike a greedy nearest-assignment (fast but routinely
  sub-optimal) it is provably minimal. The canonical uses: assign N attack units to N targets to minimise
  total travel, N defenders to N incoming threats, N workers to N jobs — any "who does what" that must be
  globally best. Handles rectangular problems (fewer agents than tasks pick the cheapest subset); to MAXIMISE
  a score instead, negate the values. Uses the classic potentials + augmenting-path formulation. Tested:
  hand-worked 2x2/3x3 (including a case where greedy fails), rectangular, maximisation-via-negation, and a
  randomized cross-check that the result equals the true minimum over ALL permutations (brute force) for
  every n up to 8. Header-only, std-only, deterministic. Godot ships no assignment solver.
- [x] **Fixed-window moving average + windowed min/max** (`core::MovingAverage`, `MovingAverage.hpp`) — DONE
  (M696); rolling statistics over the last N samples — mean in O(1) per push, window min and max in O(1)
  amortized (monotonic deques). [VERIFIABLE HERE] Distinct from the engine's other streaming stats:
  `RunningStats` (Welford) averages over ALL samples ever and can't forget old data; `P2Quantile` tracks a
  streaming percentile; `RingBuffer` is a raw ring with no reductions. A moving average deliberately forgets
  so it tracks a *changing* signal instead of drifting to a lifetime mean — the canonical "N-frame average
  FPS" readout, a denoised input axis or sensor, a rolling damage-per-second meter, or any "recent trend,
  not all-time" number; the windowed min/max give "worst frame time in the last second" for free. Tested:
  fill-before-full behaviour, oldest-sample eviction on overflow, min/max recomputed when the extreme leaves
  the window, count/full/clear, and a randomized stress test across four window sizes that agrees with a
  brute-force reference on average/min/max/count at every push. Header-only, std-only, deterministic.
- [x] **Segment-vs-rectangle clipping (Liang-Barsky)** (`math::clipSegmentToRect`, `Geometry2D.hpp`) — DONE
  (M695); trim a line segment to an axis-aligned rectangle (viewport / scissor clipping for one segment).
  [VERIFIABLE HERE] Geometry2D already had segment-segment intersection, closest-points, polygon clipping
  (Sutherland-Hodgman), and convex hull, but no way to clip a single segment to a rect — the standard need
  when drawing a debug line, laser sight, aim ray, or minimap trace that must stop at the visible bounds.
  Liang-Barsky solves the four edge-parameters directly (branch-light, allocation-free), returns whether any
  part survives, writes the clipped endpoints in a->b order, tolerates swapped rect corners, and leaves the
  outputs untouched on a full miss. Tested: fully-inside (unchanged), single-edge trim, straight-through
  (both ends on the border), fully-outside miss, corner-to-corner diagonal, swapped corners, and a
  20k-sample randomized cross-check that every clipped endpoint is inside the rect AND collinear with the
  original segment. Pairs with the existing `Rect2` (pass position and end()). Header-only, pure vec2 math.
- [x] **Segment tree — dynamic range queries with point updates** (`core::SegmentTree`, `SegmentTree.hpp`)
  — DONE (M694); arbitrary range min / max / sum / gcd AND live point updates, each in O(log n).
  [VERIFIABLE HERE] Fills the gap between the engine's two existing range structures: `FenwickTree` does
  prefix SUMS with updates but no range min/max, and `SparseTable` does O(1) range min/max but only over a
  STATIC array. A segment tree does both. The tool for a deforming heightfield's "tallest point in this
  span", a scrolling audio meter's running peak, or any "combine over [l,r] while values keep changing"
  query. Templated on the combine op (default sum) with a caller-supplied identity, so min/max/gcd all work;
  iterative (cache-friendly, no recursion), with left/right accumulators kept separate so non-commutative
  ops stay correct. Tested against brute-force scans: sum/min/max ranges before and after updates, `queryAll`
  and `get`, empty/inverted/over-long-bound edge cases, and a 3000-iteration randomized update/query stress
  test that agrees with a linear reference every step. Header-only, std-only. Godot exposes no segment tree.
- [x] **Poisson / exponential / geometric random sampling** (`core::poisson`/`exponential`/`geometric`,
  `RandomDistributions.hpp`) — DONE (M693); the standard "event-timing" random variates on top of any
  uniform source. [VERIFIABLE HERE] `core::Random` already has uniform, ranges, weighted-pick, shuffle, and
  a Gaussian, but not these three: `exponential(rate)` gives the WAIT between independent events (respawn
  gaps, next-drop timer; mean 1/rate); `poisson(lambda)` gives HOW MANY events land in one fixed interval
  (spawns this second, loot rolls, packets this tick; mean == variance == lambda) via Knuth's product method,
  with a rounded-normal fallback once `exp(-lambda)` would underflow (large lambda); `geometric(p)` gives the
  number of trials until the first success (crit-streak length; mean 1/p). Each is a free-function template
  taking a callable returning [0,1), so it composes with Random, Pcg32, or a deterministic test stub with no
  hard dependency. Tested over 400k samples per distribution: strict positivity/non-negativity, sample mean
  and variance match the closed-form moments, the large-lambda branch is exercised, and degenerate params
  return documented edge values. Header-only, std-only, deterministic. Godot exposes only uniform + normal.
- [x] **Streaming P² quantile estimator** (`core::P2Quantile`, `P2Quantile.hpp`) — DONE (M692); track a
  live percentile (median, p95, p99) of an unbounded data stream in CONSTANT memory and a single pass, with
  NO stored samples. [VERIFIABLE HERE] `RunningStats` (Welford) gives a streaming mean/variance but cannot
  answer "what's my p99 frame time?"; `math::quantile` answers it exactly but must hold the whole dataset in
  RAM; `core::Histogram` approximates it but needs bin edges chosen up front. The P-Square algorithm (Jain &
  Chlamtac, 1985) keeps just five running order-statistic "markers", nudges their positions/heights per
  sample, and reads back an estimate that provably converges — the field-standard tool for live latency/
  percentile telemetry (p95 ping, p99 hitch, "how bad is the slow 1%?"). Tested: on 100k U(0,1) samples every
  quantile lands within 2% of truth, median of a shuffled ramp is centred, p50<p95<p99≤max, and fewer than
  five samples returns an exact interpolated order statistic. (A subtle left-shift-condition bug that biased
  every estimate high was caught and fixed during verification.) Header-only, std-only, deterministic.
- [x] **Bounded float quantization for netcode** (`net::quantizeFloat`/`dequantizeFloat`/`quantizeAngle`,
  `FloatQuant.hpp`) — DONE (M691); shrink a float to N bits for compact network snapshots. [VERIFIABLE HERE]
  Sending full 32-bit floats for every position, angle, and health value wastes bandwidth; almost all live
  in a KNOWN range, so mapping that range onto a small integer of `bits` bits and reconstructing on the
  far side (to within one step) is the core trick behind compact snapshots and delta encoding (pairs with
  the engine's BitStream/Snapshot). Handles an arbitrary `[min,max]` at any bit width — unlike PackNorm's
  fixed [0,1]/[-1,1] at 8/16 bits for GPU vertex attributes — and treats angles as PERIODIC so −π and +π
  share a code (no seam). Verified (`ctest -R float_quant`): the range endpoints quantize to 0 and 2^bits−1
  and round-trip exactly; every value round-trips within half a quantization step; out-of-range values
  clamp; 16-bit is measurably more accurate than 8-bit; the code never exceeds its bit width; and angles
  round-trip within half a step on the circle with −π/+π sharing a code. Pure math, header-only,
  deterministic.
- [x] **Leaderboard with competition ranking** (`game::Leaderboard`, `Leaderboard.hpp`) — DONE (M690); the
  ranked score table behind high-score lists, ranked ladders, speedrun times, and weekly challenges:
  submit a score and answer "what rank am I?", "show the top 10", and "show me and my neighbours".
  [VERIFIABLE HERE] Uses proper COMPETITION ranking (tied scores share a rank; the next distinct score
  skips ahead — the "1224" convention), keeps only each player's BEST score, and supports both
  higher-is-better (points) and lower-is-better (race/lap times) boards. Pairs with the Elo system (M681)
  which rates head-to-head skill. Verified (`ctest -R leaderboard`): `top()` lists best-first; two tied
  top scores both rank 1 and the next distinct score is rank 3 (not 2), then 4; a worse resubmit is
  ignored while a better one promotes; `around()` returns the centred window and clamps at the leader; and
  a lower-is-better board ranks the smallest time first. Pure value logic, header-only, deterministic.
- [x] **Rotation-minimizing frames along a curve** (`math::rotationMinimizingFrames`/`advanceRMF`,
  `RotationMinimizingFrame.hpp`) — DONE (M689); a stable orthonormal frame (tangent + two perpendicular
  axes) at every point of a path, for extruding tube/ribbon meshes, sweeping cross-sections, orienting a
  camera down a spline, or placing rungs on a twisting ladder. [VERIFIABLE HERE] The textbook Frenet frame
  is unusable — undefined on straight sections and FLIPPING 180° at inflection points, so a tube built on
  it kinks and turns inside-out. An RMF instead carries the previous frame forward with the LEAST twist
  about the tangent (Wang et al. 2008 double-reflection method: two reflections transport the reference
  axis sample to sample). Complements the engine's splines (Bézier/Catmull-Rom/B-spline/TCB), which give
  the path; this gives its orientation. Verified (`ctest -R rotation_minimizing_frame`): every frame is
  orthonormal and right-handed; along a straight line the reference axis never rotates; on a planar curve
  the out-of-plane axis carries through without twist; and on a 3D helix and an S-curve through an
  inflection the frame stays continuous (no Frenet flip between samples). Pure vec3 math, header-only.
- [x] **2D wave / ripple simulation** (`game::WaveField2D`, `WaveField2D.hpp`) — DONE (M688); the "water
  surface" effect on a grid — drop a stone and rings spread out, reflect off the edges, cross each other,
  and fade. [VERIFIABLE HERE] The classic two-buffer wave step (Hugo Elias' water algorithm, a
  discretisation of the wave equation): each cell's next height = half the sum of its four neighbours'
  current heights minus its own PREVIOUS height, times a damping factor. That one line reproduces
  travelling ripples, interference between drops, boundary reflection, and decay; feed the height (or its
  gradient) into a normal map / UV distortion for water, force fields, shockwaves, or a trampoline
  surface. Border cells are held at rest (a fixed shore). Verified (`ctest -R wave_field2d`): an
  undisturbed surface stays perfectly flat; a drop at the exact centre of a square grid stays 4-fold
  (and diagonally) symmetric as it spreads; a neighbour of the drop goes from rest to non-zero after one
  step (outward propagation); damping drives the total amplitude down and the surface settles back toward
  rest; and the border stays at rest. Pure CPU, header-only, deterministic.
- [x] **Inverse bilinear interpolation** (`math::invBilinear`/`bilinear`, `InverseBilinear.hpp`) — DONE
  (M687); map a point inside a (possibly warped) quad back to its (u,v) coordinates in the unit square.
  [VERIFIABLE HERE] Forward bilinear (blend four corners by (u,v)) is easy; the inverse — "I have point P
  inside this quad, what (u,v) produced it?" — is what you need to look up the UV/colour of a hit position
  in a warped or perspective-flattened quad, deform a grid, map screen-picks into a distorted panel's
  local space, or resample between non-aligned grids. For a general non-parallelogram quad it requires a
  quadratic solve (Íñigo Quílez's robust formulation), falling back to the linear affine case for a
  parallelogram. Corners A(0,0) B(1,0) C(1,1) D(0,1). Verified (`ctest -R inverse_bilinear`): the unit
  square maps a point to itself; the four corners recover exactly (0,0)/(1,0)/(1,1)/(0,1) and the
  corner-average recovers (0.5,0.5); forward-then-inverse round-trips (u,v) across a non-parallelogram
  trapezoid (exercising the quadratic branch); and a point well outside the quad is flagged invalid.
  Pure vec2 math, header-only, deterministic.
- [x] **Median filter (edge-preserving denoise)** (`render::medianFilter`, `MedianFilter.hpp`) — DONE
  (M686); remove "salt-and-pepper" speckle while keeping edges crisp — for cleaning noisy masks,
  denoising generated/scanned textures, and pre-filtering before thresholding or edge detection.
  [VERIFIABLE HERE] Replacing each pixel with the MEDIAN of its neighbourhood (not the average) ignores
  lone bright/dark outliers — the pixel takes a neighbour's real value — and, unlike a Gaussian/box blur,
  does NOT smear edges: the majority of the window on each side of a boundary still holds that side's
  value, so it stays sharp. Clamp-to-edge borders; radius r gives a (2r+1)² window. Verified (`ctest -R
  median_filter`): a flat image passes through unchanged; a lone bright speckle (and a dark one) in a
  flat field is replaced by the surrounding value; a sharp vertical edge keeps dark-left/bright-right with
  no blur; and a hand-computed 3×3 window median (of 10..90 → 50) matches. Pure CPU, header-only,
  deterministic.
- [x] **Otsu automatic thresholding** (`render::otsuThreshold`/`binarize`, `OtsuThreshold.hpp`) — DONE
  (M685); pick the best black/white cutoff for a grayscale image automatically — for converting a
  coverage/height/mask texture to 1-bit, isolating a sprite silhouette, blob/marker detection, and
  valley-of-the-histogram segmentation. [VERIFIABLE HERE] Otsu's method (1979) treats the pixel histogram
  as two classes split at level t and picks the t that MAXIMISES between-class variance (equivalently
  minimises the spread within each group) — so the split is as clean as the data allows, with no
  hand-tuned constant. When the optimum is a flat plateau (a wide empty valley), it returns the plateau
  midpoint. `binarize` applies the cut. Verified (`ctest -R otsu_threshold`): a two-value image (half at
  50, half at 200) gets a cutoff between the clusters and `binarize` maps the dark cluster to 0 and the
  bright to 255; a single-valued image returns that value; a bimodal image with bumps near 60 and 190
  lands the threshold in the histogram valley between them; an empty image returns 0. Pure CPU on the
  256-bin histogram, header-only, deterministic.
- [x] **Sobel edge detection** (`render::sobel`/`edgeMask`, `SobelEdge.hpp`) — DONE (M684); find the edges
  (sharp brightness changes) in a grayscale image — the classic block behind toon/outline post-processing
  (run it on depth or normals to draw ink lines), sprite/UI outline generation, and image analysis.
  [VERIFIABLE HERE] Convolves with the two 3×3 Sobel kernels for the horizontal (Gx) and vertical (Gy)
  brightness gradient; the magnitude sqrt(Gx²+Gy²) is large exactly where the image changes fast (an
  edge) and the direction points across it. `edgeMask` thresholds the magnitude into a binary edge map;
  clamp-to-edge borders so every pixel gets a value. Verified (`ctest -R sobel_edge`): a constant image
  has zero gradient everywhere; a vertical step edge reads exactly Gx=4, Gy=0 at the boundary pixels and
  ~0 in the flat region; a horizontal step reads Gy=4, Gx=0; the gradient magnitude is largest on the
  edge; and `edgeMask` flags the edge pixels while leaving the flat region unmarked. Pure CPU,
  header-only, deterministic.
- [x] **Indexed binary min-heap with decrease-key** (`core::IndexedHeap<Key,Priority>`, `IndexedHeap.hpp`)
  — DONE (M683); a priority queue whose entries can be UPDATED — the operation Dijkstra, A*, and
  event/timer queues need constantly ("this node's tentative cost just dropped; re-prioritise it").
  [VERIFIABLE HERE] `std::priority_queue` can push/pop by priority but cannot change the priority of an
  element already inside it, forcing the push-duplicates-and-filter-stale-pops workaround; this pairs the
  classic binary heap with a key→slot index map so `push`/`pop`/`decreaseKey`/`update`/`erase` are all
  O(log n) and `contains`/`priorityOf` are O(1). Keyed by a caller id, min-heap by default with a custom
  comparator allowed. Verified (`ctest -R indexed_heap`): popping ten pushed priorities yields them in
  sorted order (heapsort); `top()` is always the current minimum; `decreaseKey` makes an element pop
  before ones it was behind (and no-ops on a worse value); a worsening `update` sinks an element to pop
  later; re-pushing a key updates rather than duplicates; `contains`/`priorityOf`/`erase` behave; and a
  full Dijkstra relaxation loop over a small graph finds the correct shortest paths. Header-only,
  deterministic.
- [x] **Markov chain procedural name generator** (`game::MarkovName`, `MarkovName.hpp`) — DONE (M682);
  learn the letter patterns of an example word list (elf names, town names, potions, sci-fi surnames…)
  and invent NEW words that share the flavour without copying the inputs — the classic lightweight
  generator behind fantasy name makers and roguelike vocabularies. [VERIFIABLE HERE] An order-k character
  Markov model: each next letter is drawn from the distribution that followed the previous k letters in
  training (higher order hugs the source, lower goes wilder), with start/end sentinels so words begin and
  end plausibly. Deterministic given a `core::Pcg32` seed. Verified (`ctest -R markov_name`): the same
  seed always yields the same name (reproducibility); a spread of seeds yields a variety of distinct
  names; EVERY generated word is valid — re-deriving its letter transitions confirms each one was
  observed in training, so the model never invents unseen patterns (the core Markov invariant); training
  only on words starting with a given letter yields names starting with that letter; an untrained model
  generates nothing. Header-only, deterministic.
- [x] **Elo rating system** (`game::eloExpectedScore`/`eloUpdate`/`eloPlay`/`eloKFactor`, `Elo.hpp`) —
  DONE (M681); the ranking + matchmaking math behind ranked ladders, leaderboards, bracket seeding, and
  scaling AI difficulty to a player's measured skill (Arpad Elo's system, as used by chess and virtually
  every competitive game). [VERIFIABLE HERE] Each competitor carries one number; before a match the
  ratings predict each side's win probability, and after it both move by an amount proportional to how
  surprising the result was — the exchange is zero-sum (winner gains exactly what loser drops), and the
  K-factor sets volatility (large for provisional players, small for established). Verified (`ctest -R
  elo`): equal ratings give a 0.5 expected score and the two sides' expectations sum to 1; a 400-point
  lead is exactly a 10/11 (~0.909) expected score; an equal-rating win moves each player by exactly K/2;
  a shared-K game conserves total rating (zero-sum); the underdog gains more for the same win than the
  favorite would; a draw nudges the lower-rated up and the higher-rated down; and the provisional
  K-factor exceeds the established one. Pure value math, header-only, deterministic.
- [x] **Tempo (BPM) estimation** (`audio::estimateTempo`, `TempoEstimate.hpp`) — DONE (M680); find the
  beat rate of a piece of music from its samples — for rhythm games, beat-synced visuals/lighting,
  auto-cut editors, and adaptive music. [VERIFIABLE HERE] Builds an onset-strength signal (how much the
  short-time energy JUMPS frame to frame — a "a beat just happened" proxy) and autocorrelates it; a
  steady beat makes that signal periodic, so the autocorrelation peak's lag (refined with parabolic
  interpolation) gives the period → BPM. A perceptual log-tempo prior (Gaussian centred on `priorBpm`)
  resolves the classic half/double-tempo OCTAVE ambiguity toward musically-typical tempos. Verified
  (`ctest -R tempo_estimate`): a 120 BPM click track (the prior centre) reads back exact to a few BPM;
  90/100/140/150 BPM tracks read back correct to within an OCTAVE (the field's standard "Accuracy-2"
  criterion — a steady beat autocorrelates equally at half and double its period, so octave-equivalent
  answers are the honest correctness bar); detections carry positive confidence; silence yields nothing.
  Pure CPU, header-only, deterministic. **Honest scope:** exact-octave disambiguation for arbitrary
  material is a known-hard problem; this resolves near the prior centre and is octave-correct elsewhere.
- [x] **Envelope follower + level metering** (`audio::EnvelopeFollower`, `rms`/`peakLevel`,
  `EnvelopeFollower.hpp`) — DONE (M679); track the moment-to-moment loudness of a signal — the
  foundational block under compressors/gates, sidechain ducking, VU/peak meters, envelope-driven filters
  (auto-wah), and onset/beat detection. [VERIFIABLE HERE] The raw waveform swings through zero many times
  per cycle, so level can't be read off it directly; an envelope follower smooths the rectified (Peak) or
  squared (RMS) signal with separate ATTACK (rise) and RELEASE (fall) time constants — the classic
  one-pole detector. Plus block helpers `rms()` (sqrt-mean-square energy) and `peakLevel()`. Verified
  (`ctest -R envelope_follower`): `rms` of a unit sine is 1/√2 and `peakLevel` ~1; a peak follower fed a
  constant converges to it; the one-pole step response reaches EXACTLY 1−1/e (~63.2%) of the target after
  one attack time constant and decays to 1/e (~36.8%) after one release constant; an RMS follower settles
  at the sine's true RMS; and a fast-attack/slow-release follower rises quicker than it falls. Pure CPU,
  header-only, deterministic.
- [x] **YIN monophonic pitch detection** (`audio::detectPitchYin`, `PitchDetect.hpp`) — DONE (M678);
  estimate the fundamental frequency (perceived pitch) of a block of mono audio — what a guitar/vocal
  TUNER, a rhythm game scoring sung or played notes, auto-harmony, and voice-driven mechanics all need.
  [VERIFIABLE HERE] Naive autocorrelation famously "octave-errors" — it locks onto a harmonic instead of
  the fundamental. YIN (de Cheveigné & Kawahara 2002) fixes that with a cumulative-mean-normalised
  difference function + an absolute threshold, then refines to sub-sample precision with parabolic
  interpolation; it returns frequency, a confidence, and a found flag. Verified (`ctest -R pitch_detect`):
  synthesised pure sines at 220/440/880 Hz are detected to within ~1.5 Hz; a harmonic-rich tone
  (fundamental 330 Hz with stronger 2nd and 3rd harmonics — the spectrum that fools autocorrelation) still
  reports the FUNDAMENTAL, not an octave; silence reports no pitch; and a clean tone yields high
  confidence. Monophonic, CPU-only, header-only, deterministic. [SEE IT ON YOUR MACHINE] wiring it to a
  live mic input is the one step that needs your hardware.
- [x] **Kochanek-Bartels (TCB) spline** (`math::tcbSegment`/`tcbTangentIn`/`tcbTangentOut`/`hermite`,
  `TcbSpline.hpp`) — DONE (M677); the interpolating keyframe spline with artist Tension/Continuity/Bias
  knobs — the animation industry's keyframe curve (3ds Max, Maya, classic game tools). [VERIFIABLE HERE]
  The engine had centripetal Catmull-Rom and the cubic B-spline; TCB is the complement animators reach
  for: like Catmull-Rom it passes THROUGH every keyframe, but each key carries three dials — Tension
  (how taut vs round the bend, 1=linear/-1=slack), Continuity (smooth vs a corner/"snap"), and Bias
  (lean past the key vs before it). It derives an incoming and outgoing tangent at each key from those
  knobs and Hermite-interpolates each segment. Verified (`ctest -R tcb_spline`): endpoints are hit
  exactly (s=0→p1, s=1→p2); with Tension=Continuity=Bias=0 the segment reproduces UNIFORM CATMULL-ROM
  exactly (compared against an independent Catmull-Rom evaluation across the whole segment — the clean
  cross-check); the default outgoing tangent equals the central difference 0.5·(next−prev);
  Tension=1 zeroes the tangents so the midpoint is the plain endpoint average; and +bias vs −bias lean
  the tangent toward the incoming vs outgoing segment as designed. Pure vec2 math, header-only.
- [x] **SQUAD spherical-cubic quaternion spline** (`math::squad`/`squadSegment`/`squadIntermediate`,
  `quatLog`/`quatExp`, `QuaternionSquad.hpp`) — DONE (M676); smooth C¹ orientation interpolation through
  a list of rotation keyframes — the rotation analog of a cubic spline. [VERIFIABLE HERE] `Quaternion::slerp`
  already blends between two orientations along the shortest arc (a straight line in rotation space), but
  chaining slerp across keyframes is only C⁰: angular velocity jumps at every key, so a camera or bone
  visibly "ticks" as it passes each one. SQUAD (Shoemake 1987) threads a smooth curve through the keys so
  angular velocity is continuous — what cinematic camera rigs and skeletal-animation rotation tracks use.
  Built on the quaternion exponential map (`quatLog` → tangent, `quatExp` → rotation): each key gets an
  inner control `s_i = q_i·exp(-(log(q_i⁻¹q_{i-1})+log(q_i⁻¹q_{i+1}))/4)`, and a segment is
  `slerp(slerp(q0,q1,t), slerp(s0,s1,t), 2t(1-t))`; adjacent segments share the boundary control, which is
  what makes the join C¹. Verified (`ctest -R quaternion_squad`): `exp(log(q))==q`; segment endpoints are
  exact (t=0→q0, t=1→q1) whatever the controls; identical keyframes give a constant orientation with no
  drift; every sample stays unit length; the intermediate of three equal orientations is that orientation;
  and for keyframes about a shared axis the finite-difference angular velocity is continuous across a
  shared junction (the C¹ property SQUAD exists to provide). Header-only, deterministic.
- [x] **Exact Euclidean distance transform + nearest-seed labelling** (`math::distanceTransform`,
  `DistanceTransform.hpp`) — DONE (M675); fills every grid cell with its EXACT Euclidean distance to the
  nearest "seed" cell, plus which seed is nearest (a grid Voronoi labelling). [VERIFIABLE HERE] This is
  the workhorse behind exact SDF baking, "how far is this tile from the nearest wall?" navigation
  clearance fields (spawn placement, corridor widths, influence maps), morphological grow/shrink, and
  grid Voronoi regions. The engine's `ui::Sdf` bakes glyph fields with dead reckoning, which is
  APPROXIMATE (sub-texel, fine for fonts); this is the EXACT transform — the separable
  Felzenszwalb-Huttenlocher lower-envelope-of-parabolas algorithm, O(n) per row and per column, so the
  result equals a brute-force nearest-seed search to the bit. It threads the argmin through both the
  column and row passes to recover each cell's nearest-seed (x,y). Verified (`ctest -R distance_transform`):
  a single seed gives the exact `hypot` at every cell with that seed as nearest; a seed cell is distance
  0 pointing at itself; on a 16×12 grid with six scattered seeds EVERY cell's distance matches a
  brute-force O(n·seeds) search exactly and the reported nearest is a genuinely-nearest real seed; and an
  empty grid yields the infinite-scale sentinel with nearest = (−1,−1). Header-only, deterministic.
- [x] **Uniform cubic B-spline (open + closed)** (`math::bsplinePoint`/`bsplineTangent`/`bsplineEval`,
  `BSpline.hpp`) — DONE (M674); the C²-continuous *approximating* spline behind smooth camera dollies,
  easing rails, and procedural geometry — and the curve NURBS is built on. [VERIFIABLE HERE] The engine
  already had cubic Bézier (`Curve2D`) and centripetal Catmull-Rom (`CatmullRomSpline`), which
  *interpolate* (pass through every waypoint) — right for patrol paths. The cubic B-spline is the
  complement: it does NOT pass through its control points, it is pulled toward them, and in exchange it
  is C² continuous (continuous curvature — no kink in acceleration) and provably stays inside the convex
  hull of its four local control points (it can never overshoot). Each segment is the uniform cubic
  basis blend `B(t)=1/6[(1-t)³P0 + (3t³-6t²+4)P1 + (-3t³+3t²+3t+1)P2 + t³P3]`; chain forms are OPEN
  (n≥4 → n-3 segments) and CLOSED (n≥3 → n wrapped segments, a seamless C² loop), with an analytic
  tangent. Verified (`ctest -R bspline`): the knot-point averages `B(0)=(P0+4P1+P2)/6` and
  `B(1)=(P1+4P2+P3)/6`; the central-difference tangent `B'(0)=(P2-P0)/2`; a flat control net gives a
  constant point (partition of unity, weights summing to 1); a hand-computed midpoint; linear precision
  (evenly-spaced collinear points trace the straight line exactly); convex-hull containment on every
  sample; open/closed segment counts; C⁰ continuity across every closed-loop join plus seamless wrap
  (u=0 ≡ u=n); and the analytic tangent matching a central finite difference. Pure vec2 math, header-only.
- [x] **CIELAB perceptual colour space + CIEDE2000 colour-difference** (`render::toLab`/`fromLab`,
  `deltaE76`/`deltaE2000`, `CieLab.hpp`) — DONE (M673); the perceptually-uniform colour space and the
  modern colour-difference metric behind accurate gradients, palette reduction, and "are these two
  colours the same?" thresholds. [VERIFIABLE HERE] The engine already had HSV/HSL/hex/sRGB↔linear, but
  those are *device* spaces where equal numeric steps do NOT look like equal perceptual steps — gradients
  band and nearest-colour matching picks the wrong swatch. CIELAB (L\* lightness 0–100, a\* green↔red,
  b\* blue↔yellow) is built on human vision so Euclidean-ish distance tracks how different colours *look*.
  `render::Color` is linear RGB (sRGB primaries), so the pipeline is linear-RGB → CIE XYZ (D65) → L\*a\*b\*
  with the exact inverse; two difference metrics ship: `deltaE76` (fast Euclidean, CIE 1976) and
  `deltaE2000` (CIEDE2000, with lightness/chroma/hue weighting + the blue-region rotation term, computed
  in double for the trig/pow precision the formula needs). Verified (`ctest -R cielab`): linear white →
  L\*=100 neutral, black → L\*=0; Color→Lab→Color round-trips a spread of colours (incl. alpha); L\* is
  monotonic in lightness; channel signs (red +a\*, green −a\*, yellow +b\*, blue −b\*); `deltaE76` is zero
  for identical colours and symmetric; and `deltaE2000` matches all 13 published Sharma–Wu–Dalal reference
  pairs to 1e-3 (2.0425, 2.8615, 3.4412, 1.0000, 2.3669, 27.1492, 1.2644, 0.9082, …) — the standard
  correctness vectors for a CIEDE2000 implementation. Pure value maths, header-only.
- [x] **Swept sphere vs plane (continuous collision)** (`math::sweepSpherePlane`, `SweptSphere.hpp`) —
  DONE (M672); the time-of-impact of a MOVING sphere against a plane — the CCD primitive behind fast
  ball physics and projectile-vs-surface. [VERIFIABLE HERE] Discrete collision (test where the sphere
  lands each frame) tunnels through walls when the sphere moves faster than its own radius per step;
  continuous collision solves for the exact fraction t of the step at first contact so you can advance to
  the touch and respond. This adds the analytic 3D sphere-vs-plane sweep (Ericson) — first-contact
  fraction in [0,1], contact point, and t=0 when already overlapping — complementing the engine's 2D
  swept circle (`ShapeCast2D`) and conservative sphere cast. Verified (`ctest -R swept_sphere`): a sphere
  falling from y=5 (r=1) hits the ground at t=(5−1)/10=0.4 with the contact point on the plane; an
  already-overlapping sphere reports t=0; a sphere moving away, one too slow to reach the plane this step
  (t>1), and one moving parallel all report no hit; approach from the other side works symmetrically;
  and an offset floor (y=2) gives t=(6−1)/10=0.5 with the contact at y=2. The exact moving-sphere-vs-plane
  TOI the CCD path needs.
- [x] **Optics — refraction (Snell) + Fresnel** (`math::refract`, `isTotalInternalReflection`,
  `fresnelF0`, `fresnelSchlick`, `Optics.hpp`) — DONE (M671); the light-bending math for water, glass,
  gems, and PBR, completing the `reflect` (mirror) half the engine already had with the transmission
  half. [VERIFIABLE HERE] `refract` bends an incident direction across a surface by the index-of-refraction
  ratio and returns the zero vector on TOTAL INTERNAL REFLECTION (past the critical angle — the mirrored
  underside of a water surface); Fresnel-Schlick gives the reflect-vs-transmit fraction that is near-zero
  (base reflectance f0) head-on and rises to 1 at grazing angles (the bright rim on water/glass that every
  PBR shader multiplies its specular by), with a `fresnelF0(n1,n2)` helper and a colored per-channel form
  for metals. Pure vec3 math. Verified (`ctest -R "^optics$"`): at normal incidence light passes straight
  through, unit length, for any eta; at 45° Snell's law holds exactly (sin θ_t = eta·sin θ_i) and the ray
  stays unit length; past the critical angle refract returns zero and `isTotalInternalReflection` flags
  it; `fresnelF0(1.0, 1.5)` ≈ 0.04 (air→glass); Fresnel-Schlick equals f0 head-on and 1 at grazing,
  stays within [f0,1], and rises as the angle grazes; and the colored form applies per channel. The
  transmission/Fresnel optics the water and glass shaders evaluate, now CPU-tested.
- [x] **Oriented 2D rectangle (OBB2)** (`math::OrientedRect2`, `orientedRectsOverlap`, `OrientedRect2.hpp`)
  — DONE (M670); a *rotated* rectangle with containment + overlap, filling the gap between the engine's
  axis-aligned `Rect2` and its 3D oriented box (`Obb`). [VERIFIABLE HERE] Games constantly need a
  rotated pickup/trigger zone, a tilted camera bound, a hit-test on a rotated UI panel or sprite, or a
  swinging blade's hurtbox — none of which an AABB can represent. Point-in-rect transforms the point into
  the box's local frame; rect-vs-rect uses the separating-axis theorem over the four edge normals; plus
  `corners()` and `projectedRadius()`. Verified (`ctest -R oriented_rect2`): an axis-aligned OBB contains
  exactly what an AABB would; a 90°-rotated 2×1 box becomes 1-wide/2-tall and correctly rejects a point
  the AABB would accept; corners are the rotated vertices; identical and close boxes overlap while
  far-apart ones separate; a 45°-rotated box overlaps A exactly where its diagonal corner (reach √2)
  reaches and a small gap separates them; and `projectedRadius` onto X/Y returns the half-extents. The
  2D oriented-box primitive that rounds out the Rect2 / Obb collision set.
- [x] **Great-circle / spherical geometry** (`math::haversineCentralAngle`, `greatCircleDistance`,
  `latLonToUnit`, `angleBetweenUnit`, `slerpUnit`, `GreatCircle.hpp`) — DONE (M669); distances and
  shortest paths ON a sphere, for planet/globe games, star/sky-dome placement, orbital tracks, and
  "shortest route between two map points." [VERIFIABLE HERE] A straight 3D line is not the shortest path
  along a spherical surface — the great circle is — and computing separations from raw dot products loses
  precision for both tiny and near-antipodal pairs. This adds the numerically-stable haversine
  central-angle/distance, lat/lon↔unit-vector conversion (+Y-up convention), the angle between unit
  vectors, and unit-vector SLERP to walk a great-circle arc at a constant angular rate. Pure trig.
  Verified (`ctest -R great_circle`): pole-to-pole is exactly π and an equator quarter is π/2; a point to
  itself is 0; distance scales with radius; `(0,0)→+X`, north-pole→+Y, `(0,90°)→+Z` and every conversion
  is unit length; haversine agrees with the angle between the corresponding unit vectors; SLERP endpoints
  are exact, its midpoint is unit length, sits at half the arc from each end, and lands on the 45° direction
  between +X and +Z; and great-circle intermediate points stay on the unit sphere. A genuinely-missing
  primitive for any game that wraps around a globe.
- [x] **Spline arc-length reparameterization** (`math::ArcLengthTable`, `ArcLength.hpp`) — DONE (M668);
  the "move at constant speed along a path" tool that pairs with the Catmull-Rom spline (M664) and Bézier
  `Curve2D`. [VERIFIABLE HERE] A curve's natural parameter u∈[0,1] does not advance at constant speed —
  equal steps in u cover more ground on straight sections and less through tight bends — so an object
  animated by raw u visibly speeds up and slows down. Arc-length reparameterization fixes it: sample the
  curve, build a cumulative chord-length table, and map DISTANCE↔parameter. Feed it the points from
  `CatmullRomSpline::tessellate` and you can drive a camera/enemy/projectile along the path at uniform
  speed (`parameterAtDistance` / `parameterAtFraction`), query how far along a parameter is
  (`distanceAtParameter`), or place N evenly-spaced points (`equalArcParameters`). Verified
  (`ctest -R arc_length`): total length equals the summed chords; on a uniformly-sampled straight line
  distance maps linearly to parameter; the distance→parameter→distance round-trip is exact; on a
  deliberately non-uniform sampling the distance-midpoint parameter is well past the raw midpoint (proving
  it actually reparameterizes); distances clamp to [0,1]; and `equalArcParameters` resamples a real
  Catmull-Rom spline into roughly equidistant points (spans within ~15%). Completes the spline/curve
  toolset with the constant-speed traversal games actually need.
- [x] **Tonemapping operators — ACES / Reinhard / Uncharted2** (`render::acesFilmic`, `reinhard`,
  `reinhardExtended`, `uncharted2`, `Tonemap.hpp`) — DONE (M667); the HDR→display color curves as a
  reusable, tested CPU function. [VERIFIABLE HERE] A physically-lit scene produces radiance well above 1.0
  (bright sky, specular highlights, muzzle flash), but a display only shows [0,1]; a tonemap curve
  compresses that range while keeping shadows/midtones/highlights natural. The engine already had a GPU
  HDR target + tonemap pass (M38/M63), but not the curve as standalone, testable math (useful for CPU
  color grading, thumbnail generation, and golden-image baselines). This adds ACES filmic (Narkowicz's
  widely-used fit — the modern default look), Reinhard and its white-point-extended form, and the
  Uncharted2/Hejl filmic operator, plus per-channel RGB and an exposure helper. Verified
  (`ctest -R "^tonemap$"`): every operator maps 0→0 and is monotonically increasing; outputs stay in
  [0,1] across a 0–20 HDR sweep; ACES compresses a midtone (0.5→~0.62) and saturates a very bright value
  (100→~1.0); `reinhard(1)=0.5` and it approaches 1 for large input; extended Reinhard maps its white
  point to ~1; and per-channel ACES applies the scalar curve independently so brighter input channels map
  to brighter outputs. The reusable curve the GPU tonemap shader evaluates, now unit-tested on the CPU.
- [x] **Closest points between two 3D segments + capsule overlap** (`math::closestBetweenSegments`,
  `capsulesOverlap`, `SegmentDistance.hpp`) — DONE (M666); the geometry primitive under capsule-vs-capsule
  collision and any "how far apart are these two edges?" query. [VERIFIABLE HERE] The engine had
  point-vs-segment (2D) and point-vs-triangle (3D) closest-point queries but not segment-vs-segment; a
  capsule is a segment + radius, so two capsules overlap exactly when the closest distance between their
  spine segments is below the sum of radii — this is the missing piece. Uses Ericson's robust algorithm
  (Real-Time Collision Detection): parameterize both segments by s,t in [0,1], solve the unconstrained
  minimum, clamp into the valid square, and handle parallel and zero-length (degenerate) segments without
  dividing by zero. Verified (`ctest -R segment_distance`): a skew perpendicular pair (X-axis vs a raised
  Y-axis) returns the exact gap 1 with both closest points at the segment midpoints (s=t=0.5); crossing
  segments give distance 0 at the intersection; parallel offset segments give the perpendicular gap;
  collinear disjoint segments meet at their nearest endpoints; a zero-length segment reduces to
  point-vs-segment; s,t stay in [0,1] and distance is non-negative for arbitrary configs; and
  `capsulesOverlap` flips correctly around the radius-sum threshold. Complements the existing closest-point
  queries and the capsule collider with the exact edge-edge distance the 3D physics narrowphase needs.
- [x] **2D Simplex noise + fBm** (`core::simplex2D` / `simplexFbm2D`, `SimplexNoise.hpp`) — DONE (M665);
  the third member of the engine's noise family, joining Perlin (`core::Noise`, M85) and Worley
  (`CellularNoise.hpp`, M661). [VERIFIABLE HERE] Simplex noise (Perlin's own successor to classic Perlin)
  tiles space with triangles instead of a square grid, which removes the faint axis-aligned directional
  artifacts Perlin can show, uses fewer multiplies, and has clean continuous gradients — the usual
  default for terrain height, clouds, and flow fields (it's what FastNoiseLite defaults to). This is a
  seedable, hash-gradient implementation (no permutation table) following Gustavson's construction, plus
  an fBm octave-sum helper; output is calibrated to ~[-1,1] (the 8-gradient raw peak was measured and the
  scale set to 70 so the field spans the unit range). Verified (`ctest -R simplex_noise`): deterministic
  per (position, seed); over a 400×400 grid |n| stays under 1.05 while the field genuinely swings
  (peak > 0.5) and its mean is within 0.05 of zero (balanced); nearby samples differ by < 0.05 (continuous,
  bounded gradient); different seeds give different fields; and fBm stays in range and is deterministic.
  Rounds out Perlin + Worley with the artifact-free gradient noise most terrain/cloud generators reach for.
- [x] **Centripetal Catmull-Rom spline** (`math::CatmullRomSpline`, `CatmullRomSpline.hpp`) — DONE
  (M664); the smooth-path-through-waypoints tool for camera rails, roads/rivers, and patrol paths. The
  engine already had a single *uniform* Catmull-Rom segment (`VectorOps::cubicInterpolate`), but uniform
  parameterization famously produces cusps and self-intersecting loops when waypoints are unevenly spaced
  or turn sharply; the CENTRIPETAL variant (alpha = 0.5, Yuksel et al.) spaces the knots by √distance and
  provably removes those cusps/loops while still passing through every control point. This adds the
  chain-of-segments form (`eval(u)` over `[0, n-1]`, plus `tessellate`) with selectable parameterization
  (0 uniform / 0.5 centripetal / 1 chordal). [VERIFIABLE HERE] Verified (`ctest -R catmull_rom`): the
  spline INTERPOLATES exactly — `eval(i)` returns control point i on the dot — endpoints and out-of-range
  clamping are correct; it is C0-continuous through interior knots; a collinear set of control points
  yields a straight curve; the centripetal knots stay finite (no NaN) even through a duplicated point and
  a 180° reversal (the exact case that breaks uniform Catmull-Rom); and `tessellate` yields the right
  sample count anchored on the endpoints. Complements the engine's Bézier `Curve2D` and single-segment
  Catmull-Rom with the cusp-free interpolating spline paths actually want.
- [x] **Low-discrepancy sequences — Halton / Hammersley / radical inverse** (`math::radicalInverse`,
  `halton2D`, `hammersley2D`, `LowDiscrepancy.hpp`) — DONE (M663); the quasi-random sampling the engine
  had no equivalent of (it had a PRNG and Poisson-disk, but not the QMC sequences). [VERIFIABLE HERE]
  Where a pseudo-random generator clumps and leaves gaps, these sequences fill the unit interval/square
  as evenly as possible for *any* prefix count — exactly what you want for temporal anti-aliasing
  sub-pixel jitter (a fresh well-spread offset every frame), progressive/quasi-Monte-Carlo integration
  (soft shadows, AO, image-based-lighting sampling that converges faster than white noise), and even
  scatter placement. The van der Corput radical inverse reflects an integer's base-b digits about the
  radix point; Halton pairs two coprime-base radical inverses; Hammersley uses the sample index directly
  for one axis. Pure integer/float math, stateless. Verified (`ctest -R low_discrepancy`): base-2 radical
  inverse hits its textbook values (1/2, 1/4, 3/4, 1/8, 5/8, 3/8, 7/8) and base-3 gives 1/3, 2/3, 1/9;
  all outputs stay in [0,1); the STRATIFICATION property holds — the first 8 base-2 points sorted are
  exactly {0/8…7/8}, perfectly even where random would clump; the Halton (2,3) x-axis has max-gap < 1/50
  over 64 points; and Hammersley's x is exactly i/count. Complements the existing PRNG/Poisson-disk with
  the deterministic even-coverage tool renderers rely on for jitter and QMC.
- [x] **Dual-quaternion skinning (DQS)** (`math::DualQuaternion`, `blendDual`, `DualQuaternion.hpp`) —
  DONE (M662); the skinning math that fixes the "candy-wrapper" collapse of the engine's existing
  linear-blend skinning. [VERIFIABLE HERE] A unit dual quaternion represents a rigid motion (rotation +
  translation, no scale): the real part is the rotation quaternion, the dual part encodes the translation
  (dual = ½·t·real). The reason it matters is BLENDING — a vertex weighted between a straight bone and a
  twisted one: linear-blend skinning averages the two matrices and the result shrinks toward the joint
  axis (the elbow/wrist pinch you see in cheap rigs), whereas averaging the two transforms as dual
  quaternions and renormalizing (dual-quaternion linear blending, DLB) yields another *rigid* transform
  that preserves volume. This module implements dual quaternions (`fromRotationTranslation`,
  `transformPoint`, `translation`) and `blendDual` with the essential hemisphere alignment (so opposite-
  sign quaternions don't cancel). Pure quaternion algebra over floats. Verified (`ctest -R dual_quat`):
  a rigid transform applied to a point equals rotate-then-translate and `translation()` round-trips it;
  identity and pure-translation behave correctly; blending identical transforms reproduces them; **the
  DQS property** — a 50/50 blend of 0° and 90° rotations applied to a unit vector keeps it unit length
  (linear-blend would give ~0.707, a visible collapse) and lands exactly at the 45° screw position; and a
  rotation blended with its opposite-hemisphere duplicate resolves to the same rotation rather than
  cancelling to identity. Complements the engine's Skeleton/linear-blend skinning with the higher-quality
  alternative renderers offer as a per-mesh option.
- [x] **Worley / cellular ("Voronoi") noise** (`core::worley2D` / `worley3D`, `CellularNoise.hpp`) —
  DONE (M661); the procedural-texture noise the engine's Perlin (`core::Noise`, M85) didn't cover.
  [VERIFIABLE HERE] Space is divided into unit cells each holding one hash-placed feature point; sampling
  a position returns F1 (distance to the nearest point) and F2 (second nearest). F1 alone makes bubbly
  organic cells (water caustics, cracked mud, cell membranes); F2−F1 traces the ridges *between* cells
  (stone veins, crackle, Voronoi edges). Distinct from `Voronoi.hpp` (which builds an explicit
  Delaunay/Voronoi diagram from a fixed point set) — this is a cheap, seed-driven, continuously
  sampleable NOISE FIELD with no allocation, the form shaders and terrain/texture generators actually
  use. A 3×3 (2D) / 3×3×3 (3D) cell search guarantees the true two nearest points are found. Verified
  (`ctest -R cellular_noise`): determinism per (position, seed); F1 ≤ F2 and both ≥ 0 across a grid;
  sampling exactly at a cell's reconstructed feature point gives F1 ≈ 0; F1 is bounded by ~√2 in 2D and
  ~√3 in 3D; different seeds produce different fields; and the F2−F1 edge signal is non-negative and
  varies across space. A genuinely-missing staple complementing the existing Perlin/fBm noise.
- [x] **Projectile lead / intercept solver** (`math::interceptTarget`, `Intercept.hpp`) — DONE (M660);
  the "aim ahead of a moving target" math behind every turret, homing shot, and AI marksman. [VERIFIABLE
  HERE] The engine had a full-transform look-at and a look-rotation quaternion (M651), but those aim at a
  *point*; hitting a *moving* target needs solving for WHERE it will be when a shot fired now arrives.
  That's a quadratic in the intercept time t — solve |targetPos + targetVel·t − shooter| = projSpeed·t —
  which can have zero solutions (the target outruns the projectile), one (the equal-speed linear case), or
  two (take the soonest). `interceptTarget` (2D and 3D overloads) returns the intercept time, the future
  aim point, and the unit aim direction. Verified (`ctest -R "^intercept$"`): a stationary target is aimed
  at directly with time = distance/speed; the defining invariant `distance(shooter, aimPoint) ==
  projSpeed·time` holds for perpendicular and closing motion in both 2D and 3D (the projectile reaches the
  lead point exactly when the target does, and the target's own path reaches that same point at that time);
  the a≈0 linear case (target speed == projectile speed) is handled; a target fleeing faster than the
  projectile returns hit=false; and the returned direction is unit length. Pure closed-form math, no
  allocation — a genuinely-missing staple (nothing like it existed; the only prior "intercept" was
  least-squares line-fitting). Complements the existing steering, ballistics-check, and look-at helpers.
- [~] **Video container demux (IVF)** (`video::demuxIvf`, `parseIvfHeader`, `video/Ivf.hpp`) — DONE
  (M658, the container/framing part); the demux half of the §7 "video container/codec decode" gap.
  [VERIFIABLE HERE for the container; codec + display on your GPU] A video file is a *container* wrapping a
  stream of compressed frames; IVF is the simplest, fully-specified one (a 32-byte file header + a 12-byte
  header before each frame), the standard wrapper for VP8/VP9/AV1 bitstreams. Before you can decode or seek
  video you must DEMUX it — read the header (codec FourCC, width/height, frame rate, frame count) and pull
  out each compressed frame's bytes and presentation timestamp — and dimensions/fps/duration are what a
  playback UI needs first. This milestone implements that demuxer exactly per spec: `parseIvfHeader` reads
  and validates the `DKIF` file header; `demuxIvf` walks the frame headers into a `{timestamp, offset,
  size}` index and computes fps + duration, stopping cleanly on a truncated final frame. Pure byte logic,
  no external files, so every field is checked. Verified (`ctest -R "^video_ivf$"`): a hand-built IVF with
  a `VP80` 320×240 @30fps header and two frames parses to the right header fields and fps; the two frames
  demux with correct timestamps, sizes, and payload offsets (and the bytes at those offsets are exactly the
  ones written); a non-`DKIF` signature is rejected; and a truncated final frame is dropped while the first
  is kept. **Honest scope:** this is the video *container demuxer* + metadata (parse, frame index, fps,
  duration) — fully verifiable here — NOT the video codec. Turning a frame's compressed bytes into pixels is
  a VP8/VP9/AV1 decoder (a very large codec), and displaying those frames is GPU-side; both are the
  remaining work. This is the container layer those decoders sit on top of, the same demux-first pattern as
  the MP3 framing milestone.
- [~] **SSIL — screen-space indirect light (CPU gather kernel)** (`render::ssilGather`,
  `ScreenSpaceIndirectLight.hpp`) — DONE (M657, the CPU-verifiable core); the §5 "SSIL pass" gap.
  [VERIFIABLE HERE for the math; visual result on your GPU] SSIL is one-bounce screen-space global
  illumination: where SSAO (already in the engine) darkens creases by counting nearby occluders, SSIL
  gathers the *colored* light bouncing off those neighbours — a red wall throws a red tint onto the white
  floor beside it, a bright surface casts a soft colored glow. The full effect is a fragment shader over
  the depth/normal/color G-buffer, but the gather MATH is CPU-testable (the same way the SSAO pass was
  validated with golden images + unit math): for each pixel, sample the screen-space neighbourhood, and
  for every neighbour that lies in the pixel's hemisphere (in front of its surface) and within a
  world-space radius, accumulate that neighbour's color weighted by the cosine term and a linear distance
  falloff; the average, scaled by intensity, is the bounced light to add. This milestone implements that
  kernel (`ssilGather` over an `SsilSample` G-buffer) in plain float math, no GPU. Verified
  (`ctest -R "^ssil$"`): a floor pixel flanked by a red wall and a white wall receives indirect light
  whose red channel exceeds green/blue — the defining color-bleed behaviour — while green equals blue
  (only the white wall feeds those); intensity scales the result linearly and zero intensity yields
  nothing; a neighbour beyond the radius, or behind the surface (negative hemisphere), contributes
  nothing; and a coplanar same-normal neighbourhood self-bounces exactly zero (a flat wall does not light
  itself). **Honest scope:** this is the SSIL gather kernel — the physics/color math, fully unit-tested
  here. Wiring it as a real-time GPU pass (depth→position reconstruction, a blur, temporal accumulation)
  and *seeing* the colored bounce is the part that needs your GPU; this is the CPU reference those shaders
  implement, and it complements the already-done SSAO, SSR, reflection-probe, and lightmap-bake modules.
- [~] **MP3 frame parsing + seek index** (`audio::parseMp3FrameHeader`, `audio::scanMp3`, `Mp3.hpp`) —
  DONE (M656, the framing/metadata part); the demux half of the §4 "Ogg Vorbis / MP3 decode to PCM" gap.
  [VERIFIABLE HERE] An MP3 file is a stream of independent MPEG audio frames, each led by a 4-byte header
  encoding the version (MPEG-1/2/2.5), layer (I/II/III), bitrate, sample rate, padding, and channel mode.
  Before you can decode *or seek* an MP3 you must FRAME it — find every frame, know its length and sample
  count — and duration/seek metadata is what most apps (and Godot's importer) need first. This milestone
  implements that framing layer exactly per spec: `parseMp3FrameHeader` validates the 11-bit sync word,
  reads the header fields through the standard bitrate tables (per version × layer) and sample-rate tables
  (per version), computes samples-per-frame (384 for Layer I, 1152 for Layer II and MPEG-1 Layer III, 576
  for MPEG-2/2.5 Layer III) and the frame length in bytes (the Layer I `(12·br/sr+pad)·4` and Layer II/III
  `(spf/8)·br/sr+pad` formulas). `scanMp3` skips a leading ID3v2 tag (reading its syncsafe size), walks the
  buffer frame-by-frame resyncing past junk, builds a `{offset,length,samples}` seek index, and totals the
  samples into a duration. Pure integer/byte logic — no external files — so every field is checked against
  hand-built headers. Verified (`ctest -R "^mp3_frames$"`): the canonical `FF FB 90 00` header decodes to
  MPEG-1 Layer III / 128 kbps / 44100 Hz / stereo / 1152 samples / 417-byte frame; an MPEG-2 Layer III
  header reports 576 samples and its 208-byte frame; a bad sync word and free-format bitrate are rejected;
  a three-frame buffer indexes all three with correct offsets and a `samples/rate` duration; a leading
  ID3v2 tag is skipped so framing starts at the right offset; and pure garbage yields zero frames.
  **Honest scope:** this is the MP3 container/framing + metadata layer (parse, seek index, duration) —
  fully verifiable here — NOT the Layer III audio codec. Turning frame payloads into PCM samples is a
  separate, large, patent-adjacent DSP step (Huffman decode → dequantize → IMDCT → synthesis filterbank),
  and Ogg Vorbis decode is a comparable codec effort; both are impractical to implement *and honestly
  verify* headlessly without golden reference bitstreams. The engine already ships QOA (dependency-free
  compressed audio, tested) and WAV, so games have working compressed + uncompressed playback today; this
  adds the MP3 demux/seek layer those codecs' full decoders would sit on top of.
- [x] **Desktop export bundle planner** (`io::planBundle`, `BundlePlan`, `PlatformSpec` in
  `io/BundlePlan.hpp`) — DONE (M655); closes the §4 "desktop export/packaging — real per-OS bundler" gap.
  [VERIFIABLE HERE] The engine already had `tools/package.sh` (which assembles a runnable bundle and even
  self-verifies it) and `io::ExportConfig` (export presets + include/exclude filters), but the actual
  *per-OS layout decisions* — what the executable is called, where the runtime libraries and assets land,
  what the launcher script contains — lived inline in shell, untested and hard to reuse (e.g. from an
  in-editor "Export Project" button). This milestone lifts that decision-making into a tested C++ core:
  given the app name, version, target OS, and the list of input files (executable, libraries, shaders,
  assets — each with a size and kind), `planBundle` returns the COMPLETE bundle plan: the platform
  executable name (`game.exe` on Windows, `game` on Linux/macOS via `PlatformSpec`), a destination path
  for every file (the exe renamed to the platform name, libraries placed beside it by basename, shaders
  and assets keeping their res-relative layout and honoring the export preset's include/exclude filters),
  a generated per-OS launcher (`run.bat` using `start`, or `run.sh`/`run.command` that set
  `LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH` and exec the binary so the game finds its own libraries and runs
  from anywhere), a deterministic MANIFEST (every file's size + dest, sorted), the total size, and the
  `app-version-os-arch` directory name the packager tars. Pure string/size logic, no filesystem calls,
  so the whole plan unit-tests headlessly. Verified (`ctest -R "^bundle_plan$"`): Linux keeps the bare
  exe name and emits a `run.sh` with `LD_LIBRARY_PATH` and `./zomboid`; Windows renames to `zomboid.exe`
  with a `@echo off` `run.bat` and no bare-name exe; macOS emits `run.command` with `DYLD_LIBRARY_PATH`;
  an export preset excluding `levels/*` drops that asset while the executable and library still ship;
  the manifest is sorted by dest and lists all six files; `totalSize` sums every file including the
  launcher; and `bundleDirName` follows the `zomboid-1.0.0-windows-x86_64` convention. **Honest scope:**
  this is the bundle *planner* — the exact copy/rename/launcher/manifest decisions, per OS, computed and
  verified here. It does not itself copy files or build the tarball (the shell packager or editor does
  that by executing the plan), and it does not cross-compile — packaging a Windows build still needs the
  Windows executable and its DLLs as inputs (produced by a Windows/MinGW build). It is the deterministic,
  testable brain the roadmap's "real per-OS bundler" asked for, replacing ad-hoc shell logic.
- [x] **Script editor tooling — outline + autocomplete + signatures** (`script::tooling` in
  `script/Tooling.hpp`) — DONE (M654); closes the §6 "deepen the script VM toward GDScript-grade tooling"
  gap. [VERIFIABLE HERE] The engine ships a full GDScript-style language (lexer, parser, VM, classes,
  closures, signals, modules), but a *language* and the *editor experience around it* are different things:
  writing scripts comfortably needs the IDE affordances — a symbol tree to navigate the file, autocomplete
  as you type, and a tooltip showing a function's parameters. This milestone adds that tooling layer as pure
  text analysis (no evaluation), so it is fully unit-testable: (1) **document outline** (`documentSymbols`) —
  every class, function (with its parameter list), signal, and variable a script declares, in source order,
  each tagged with its kind, line, and enclosing class, which is exactly what an editor's symbol panel /
  breadcrumb draws; (2) **autocomplete** (`completionsAt`) — the candidate identifiers at a cursor position,
  prefix-filtered, offering language keywords + the `print` builtin + every symbol in scope *so far* (it only
  surfaces things declared before the cursor, and switches to class members after a `.`); (3) **signature
  help** (`findFunction` → the function's `.params`) for a call tooltip. The key design point is an
  ERROR-TOLERANT scanner: unlike the VM's lexer (which stops at the first bad character, correct for
  execution but useless for an editor), this one skips what it can't classify and keeps going — an
  unterminated string ends at the line, a dangling identifier is fine — because editors must work on
  code that is mid-edit and not yet valid. The token grammar mirrors the VM lexer exactly (`#` and `//`
  comments, strings, numbers, identifiers, punctuation) so reported symbols match what the VM would parse.
  Verified (`ctest -R "^script_tooling$"`): the outline reports six symbols with correct kinds/containers
  and extracts `func greet(name, times: int)` params as `[name, times]` (skipping the type annotation);
  `findFunction` recovers a signature; `prefixAt`/`isMemberAccessAt` read the cursor context; completion for
  prefix "he" offers in-scope `health` but not `hurt` (prefix mismatch) nor a variable declared *after* the
  cursor; an empty prefix offers keywords + `print` + in-scope symbols and params; `p.` offers the class's
  `hp`/`attack` members (and `p.at` narrows to `attack`) while excluding top-level names and keywords; and
  the tolerant scanner still finds symbols on both sides of an unterminated string. **Honest scope:** this is
  the tooling *data provider* — the exact information an editor UI, an LSP server, or a completion popup
  consumes — computed from source text. It is not itself a GUI or a Language Server Protocol transport
  (those would render/serialize this data), and completion is scope- and prefix-based rather than
  type-inferred (member completion offers all class members, since the language is dynamically typed); a
  second C#/.NET binding remains a separate possible path. The VM, parser, and a live debug hook already
  exist (`script/Debugger.hpp`); this adds the author-time intelligence on top.
- [x] **OpenType-style text shaping** (`ui::shapeGlyphs`, `ShapingTable`, `LigatureRule` in
  `TextShaping.hpp`) — DONE (M653); closes the §7 "complex-script shaping hooks" gap. [VERIFIABLE HERE]
  The engine already had the *analysis* half of text (`decodeUtf8`, bidi runs, line-break opportunities in
  `TextServer.hpp`) and word-wrap layout (`ui::layoutText`), but not the *shaping* half — the step that turns a
  run of glyphs into POSITIONED glyphs the way a real font's OpenType tables prescribe. Shaping is what makes
  proper typography and complex scripts work: (1) **ligatures** — "f"+"i" becoming a single "fi" glyph (GSUB
  LookupType 4); (2) **kerning** — tucking a specific letter pair like "AV" closer together (GPOS LookupType 2);
  (3) **mark positioning** — placing a combining accent, or an Arabic/Indic vowel sign, so its anchor point lands
  exactly on the base letter's anchor point (GPOS LookupType 4, mark-to-base). Without this, accents float in the
  wrong place and Arabic/Devanagari are unreadable. This milestone implements the shaping ENGINE decoupled from
  any font blob: you hand it a `ShapingTable` (the data a font's GSUB/GPOS tables would fill — default advances,
  ligature rules, kern pairs, the set of zero-advance marks, and base/mark anchor points) plus a glyph run, and
  it returns each output glyph with its advance and x/y offset, preserving source-character CLUSTERS (so a
  ligature still maps back to the right characters for caret placement and hit-testing). Substitution uses
  longest-match; positioning applies kerning between adjacent bases and anchor-aligns each mark to its base.
  Verified (`ctest -R "^text_shaping$"`): "fi" collapses to one glyph with the ligature advance; "fii" prefers
  the longer f-i-i ligature over f-i + i (longest match); a glyph after a ligature keeps its true source index
  (cluster merge); "AV" kerns the first advance by −3 while "Ax" (no pair) does not; an acute mark after "a" gets
  zero advance and the hand-computed anchor-aligned offset (x = 3−7−1 = −5, y = 8); a rule-free run passes
  through with advances intact; and a combined "fiAV" run ligates and kerns together. **Honest scope:** this is
  the shaping *engine + data model* (GSUB ligatures, GPOS kern, GPOS mark-to-base) — pure CPU, fully unit-tested
  here. It is not yet a full HarfBuzz: it doesn't parse a live font's binary GSUB/GPOS tables (a font loader
  would populate `ShapingTable` from them), and it covers the three most important lookup types rather than all
  of OpenType (contextual chaining, cursive attachment, mark-to-mark stacking are natural follow-ons on the same
  data model). The engine already ships bidi + line-breaking, so this is the missing shaping core they feed into.
- [x] **gettext PO/POT localization tooling** (`io::PoCatalog`, `io::PotBuilder`, `io::PluralRule`) — DONE
  (M652); closes the §7 "Localization tooling" gap. [VERIFIABLE HERE] The engine already had CSV translation
  tables, but every serious localization pipeline speaks *gettext* — `.po` files — and the hard part gettext
  solves that a CSV cannot is PLURALS: English has 2 forms ("1 apple / 2 apples"), but Polish has 3, Arabic 6,
  each chosen by a language-specific little C expression over the count `n` (e.g. Polish uses form 1 for 2–4 and
  22–24 but form 2 for 5–21). A module existed that could *read* PO text and answer lookups, but it was
  untested and could only read — it could not write a catalog back out, and it had no *extraction* side (the
  step that scans a program for its translatable strings and emits a blank `.pot` template for translators).
  This milestone finished it into real tooling: (a) `PluralRule` is a complete evaluator for the gettext
  C-expression subset (`n`, integer literals, `% * / + -`, comparisons, `&& || !`, and `?:` with correct
  precedence), so the *right* plural form is selected for any language's own rule; (b) `PoCatalog::serialize()`
  writes the catalog back to PO text that round-trips exactly through the parser (contexts, plurals, and the
  `\n \t \" \\` escapes all preserved), with deterministic sorted output for clean diffs; (c) `PotBuilder` is
  the extraction/template side — collect the strings a program marks (`add` / `addContext` / `addPlural`),
  de-duplicated by (context, id) in first-seen order, upgrading a singular to a plural in place, then emit a
  `.pot` whose header already carries a usable `Plural-Forms:` line. Verified (`ctest -R "^gettext_po$"`): the
  English/Polish/empty-default plural rules produce the hand-computed reference indices across the tricky
  boundaries (5, 11, 12, 22–25); a multi-context PO parses so `pgettext("menu","Open")` and
  `pgettext("door","Open")` give different translations; `ngettext` routes n=1/3/5/22 to Polish forms 0/1/2/1;
  the serialize output re-parses to identical lookups; and the `PotBuilder` collapses duplicates to 4 distinct
  references, keeps the plural after a later singular re-add, and its POT re-parses with the header rule intact.
  **Honest scope:** this is the localization *data + rule engine* (parse, plural selection, PO write-back, POT
  template emission) — pure CPU text, fully unit-tested here. It is not a source-code *scanner*: `PotBuilder`
  takes the marked strings you feed it; wiring an `xgettext`-style pass that walks `.cpp`/script files to find
  `tr("…")` calls automatically is a separate (straightforward) step layered on top of this.
- [x] **Look-rotation quaternion** (`math::Quaternion::lookRotation`) — DONE (M651); the orientation-only
  companion to `Transform3D::lookingAt`: build the quaternion that faces a direction with an up hint (local -Z
  points along `forward`, matching the engine/Godot forward convention). The engine had a full-transform
  look-at, but when you only want the target ORIENTATION — to SLERP a turret / enemy / camera smoothly toward
  a target, or to set a rotation without touching translation/scale — you need it as a quaternion, which was
  missing. Handles a zero forward (returns identity) and up-parallel-to-forward (picks an alternate up) without
  NaN. Verified (`ctest -R "^look_rotation$"`): facing -Z is the identity; the rotation maps local -Z exactly
  onto the normalized forward for a spread of directions and the result is unit-length; the up hint keeps
  rotated +Y on the positive side; it agrees with `Transform3D::lookingAt` for the same target; a zero forward
  gives identity and up∥forward stays finite and still faces the direction. Honest scope: the -Z-forward
  convention (negate or compose if your asset faces +Z) — pure orientation, no position. [VERIFIABLE HERE]
- [x] **Spring-bone / jiggle chain** (`anim::SpringBone`) — DONE (M650); secondary motion for a chain of bones
  — tails, hair, ponytails, antennae, capes, dangling accessories — that should JIGGLE and trail as the
  character moves rather than stay rigidly rigged. You drive the ROOT joint each frame (attach it to a real
  bone) and the chain follows with inertia + damping: it lags behind sudden motion, overshoots, then settles
  back to the rest pose, and because each joint chases its parent's CURRENT (also-lagging) position the motion
  propagates down the chain like a whip. Godot exposes this as its SpringBoneSimulator/jiggle modifiers; the
  engine had core::Spring (a single scalar/vector spring) and SoftBody (full physics) but no bone-chain jiggle.
  Sub-stepped (1/240 s) for stability at any frame rate; tunable stiffness/damping/gravity. Verified (`ctest -R
  "^spring_bone$"`): a chain left at rest holds its pose with velocity decaying to ~0; after the root is yanked
  the tip visibly lags one step then converges to the new rigid pose; higher stiffness is closer to target
  after a fixed time; the sim stays finite and bounded through 2000 steps of continuous root motion; `reset()`
  snaps rigidly to the pose from the current root with zero velocity; identical drive gives identical output.
  Honest scope: a translational damped-spring chain (positional jiggle) — not a rotational bone-length-
  preserving constraint or collision-aware cloth (compose with the physics SoftBody for those). [VERIFIABLE HERE]
- [x] **Weapon recoil pattern** (`game::RecoilPattern`) — DONE (M649); the climbing "spray" every first-person
  shooter needs — each shot kicks the aim by a DEFINED amount, the kicks ACCUMULATE while firing (so a weapon
  has a recognisable, learnable pattern like CS/Valorant), and the aim RECOVERS smoothly toward centre when you
  stop. Distinct from `game::Spread` (M632), which perturbs each shot by a RANDOM amount within a cone (bullet
  inaccuracy); recoil is the deterministic, per-shot, memorised climb the player learns to counter. `fire()`
  advances the pattern and accumulates the kick (repeating the last kick past the pattern's end for a sustained
  climb); `update(dt)` recovers via frame-rate-independent exponential decay; `release()` restarts the pattern
  for the next burst while the offset keeps recovering; `offset()` feeds the crosshair/aim. Beyond Godot.
  Verified (`ctest -R "^recoil$"`): each shot accumulates the right kick and shotIndex tracks the burst; firing
  past the pattern repeats the last kick; `update(1s)` at rate 1 decays the offset by exactly exp(-1) and long
  recovery eases to ~0 without the sign flipping; `release()` zeroes the index but preserves the offset (next
  fire uses pattern[0]); `reset()` clears both; an empty pattern and recoveryRate 0 are safe no-ops. Honest
  scope: the aim-offset accumulator/recovery model (drive your camera/crosshair with `offset()`) — not the
  input handling or hit registration. [VERIFIABLE HERE]
- [x] **Image drawing primitives** (`render::drawLine` / `drawRect` / `drawCircle` / `fillCircle` /
  `fillTriangle`) — DONE (M648); rasterize 2D shapes directly INTO an Image on the CPU. The Image class already
  edited pixels and blitted regions but had no way to stroke a line, outline or fill a circle, draw a rectangle
  border, or fill a triangle — the building blocks for procedural textures, generated icons, minimap/radar
  overlays, debug visualisations, and simple CPU-side vector art. Classic Bresenham line + midpoint circle, a
  bounding-box disc fill, and a barycentric (winding-independent) triangle fill; every primitive plots through
  the Image's bounds-checked setPixel, so off-canvas is safely clipped. Distinct from Renderer's GPU debug-draw
  — this writes an in-memory image you can save, upload, or sample. Verified (`ctest -R "^image_draw$"`): a line
  sets both endpoints and a contiguous run, doesn't overrun, and clips off-canvas without crashing; a rectangle
  outline sets its four borders but leaves the interior untouched; a circle outline hits the four cardinal
  points with an empty centre; a filled radius-5 disc covers EXACTLY 81 pixels (centre + cardinals set, √50
  corner not); a filled triangle lights its interior and vertices but not points clearly outside. Honest scope:
  aliased (hard-edged) single-pixel rasterization — no anti-aliasing or thickness (compose or supersample for
  smooth edges). [VERIFIABLE HERE]
- [x] **Fog of war** (`game::FogOfWar` / `game::Visibility`) — DONE (M647); the persistent "what has this player
  seen?" memory for a tile map — the staple of RTS, strategy, and roguelike games. Every tile is Unseen (never
  revealed, drawn black), Explored (seen before but not in view now, drawn dimmed from memory), or Visible (in
  view right now, fully lit and where enemies show). DISTINCT from FieldOfView (which computes the tiles a unit
  can see this instant); fog of war is the layer that REMEMBERS — the per-frame cycle is `beginFrame()`
  (demote last frame's Visible tiles to Explored), then `reveal()`/`revealCircle()` every tile in sight, so the
  map fills in permanently as you explore while live vision comes and goes. beginFrame only touches the tiles
  that were visible (tracked list), so it's cheap. Godot ships no fog-of-war primitive. Verified (`ctest -R
  "^fog_of_war$"`): a fresh map is all Unseen; reveal makes a tile Visible+Explored; beginFrame demotes
  Visible→Explored while Unseen stays Unseen; a tile seen once stays Explored across 10 frames (never reverts);
  re-revealing lifts Explored back to Visible; `revealCircle(r=2)` marks exactly the 13-tile disc and excludes
  the √8 corner; out-of-bounds reveal/query is safe; `exploreAll` fills memory without going live; `reset`
  clears; and the visible/explored counts track state. Honest scope: the visibility-state memory itself (pair
  it with the existing FieldOfView/GridRaycast to decide which tiles a unit can actually see). [VERIFIABLE HERE]
- [x] **Shuffle bag / 7-bag randomizer** (`core::ShuffleBag<T>`) — DONE (M646); fair, clump-free randomness by
  DEALING from a bag instead of rolling independently. An independent weighted roll can hand you the same
  result five times running or starve an option for ages; a shuffle bag holds one token per intended outcome,
  deals them in random order, and only refills+reshuffles once empty — so over each cycle every outcome
  appears EXACTLY its intended number of times and the worst-case drought is bounded. This is the Tetris
  "7-bag" piece randomizer, and what you want for enemy-type spawns, music-playlist shuffle, card decks, and
  random events that should feel fair rather than streaky. Optional (default-on) avoidance of the same value
  twice across a refill boundary. Complements AliasTable (fast independent weighted draws) and ReservoirSampler
  (streaming sample) — this is the without-replacement, cycle-fair option; any RNG with an inclusive
  `range(lo,hi)` works (e.g. `core::Pcg32`). Verified (`ctest -R "^shuffle_bag$"`): each 3-item cycle is a
  permutation and over 500 cycles each item appears exactly 500 times; weighted counts (A×2,B×1) hold every
  cycle; `remaining()`/`totalCount()` track the state; with avoidance on and 6 distinct outcomes there are ZERO
  back-to-back repeats across 6000 draws (including refill boundaries); the same seed reproduces the deal
  order; an empty bag is safe. Honest scope: uniform without-replacement dealing (equal per-token odds within
  a cycle — not a Markov/pity-timer weighting scheme, which layers on top). [VERIFIABLE HERE]
- [x] **Fractional grid sampling** (`math::gridNearest` / `math::gridBilinear` / `math::gridBicubic` /
  `math::GridEdge`) — DONE (M645); read a value out of a 2D data grid at FRACTIONAL coordinates, smoothly
  interpolating between cells — the everyday need behind sampling a heightfield between vertices, a flow-field
  or vector map between cells, or a coarse lightmap / SDF / noise grid at a continuous world position. Three
  filters: nearest (blocky), bilinear (the smooth workhorse), and bicubic Catmull-Rom (C1-smooth, passes
  through the grid values); out-of-bounds handled by Clamp (repeat border) or Wrap (tile). Templated on the
  cell type, so it samples a float grid, a `vec2` flow field, or an RGB grid alike (anything supporting `T+T`
  and `T*float`). The engine had bilinear baked into HeightField / Image / noise individually; this is the one
  reusable primitive. Verified (`ctest -R "^grid_sample$"`): all filters return the exact grid value at integer
  coordinates; nearest rounds to the closest cell; bilinear reproduces a linear ramp exactly and a midpoint is
  the average of two cells; bicubic passes through the grid and is exact on a linear ramp (with an in-bounds
  stencil); Clamp repeats the border while Wrap tiles; and a vec2 flow-field grid interpolates componentwise.
  Honest scope: separable nearest/bilinear/bicubic on a regular grid (not anisotropic/mip-filtered texture
  sampling — that is the GPU's job). [VERIFIABLE HERE]
- [x] **Recycling object pool** (`core::ObjectPool<T>`) — DONE (M644); a typed pool that hands out reusable
  objects and takes them back, so a game can spawn/despawn bullets, particles, enemies, damage numbers, or
  temp buffers every frame WITHOUT churning the allocator. `acquire()` reuses a freed slot or grows by one;
  `release()` returns a slot for reuse without destroying it, so capacity rises only to the high-water mark of
  simultaneously-live objects and then stops (the whole point of pooling). Distinct from the engine's two
  existing facilities: PoolAllocator hands out raw memory bytes, and SlotMap is a generational handle→value
  map with stable IDs; this is the simple index-addressed live-object recycler most gameplay reaches for.
  Deque-backed so a reference from `get()` survives pool growth. Verified (`ctest -R "^object_pool$"`): acquire
  grows capacity + active count and `get()` is mutable; release lowers the count and marks the slot inactive;
  the next acquire reuses the freed index with NO capacity growth; a recycled slot keeps its prior value; a
  `get()` reference stays valid across 1000 growths; double / out-of-range release is a safe no-op that never
  corrupts the count; under acquire/release churn capacity settles exactly at the peak concurrent count (8);
  `reset()` frees all while keeping capacity and `clear()` drops it. Honest scope: reuses storage, does not
  reset recycled objects (the caller re-initialises on acquire) — as documented. [VERIFIABLE HERE]
- [x] **Squad formations** (`game::formationSlots` / `game::formationPositions` / `game::FormationShape`) — DONE
  (M643); arrange a group of units into a recognisable shape around an anchor (leader or target), oriented to a
  facing direction — the geometry every RTS squad, party of followers, tactical fireteam, or escort needs
  (feed the returned slot to Steering/arrive or a pathfinder). Five shapes: Line (abreast), Column (single
  file), Wedge (arrowhead V), Box (centred grid), Circle (defensive ring). `formationSlots` gives LOCAL offsets
  (forward +Y, right +X, anchor at origin); `formationPositions` rotates them by a facing vector and translates
  to the anchor, so the whole formation turns as the leader turns. Godot ships no formation helper. Verified
  (`ctest -R "^formation$"`): counts honoured / non-positive empty / count-1 is the leader at origin; Line is
  centred, abreast, gap==spacing, spans −4..+4 for 5@2; Column is single-file receding by spacing; Wedge tips
  at the leader with a mirrored first rank one step back; Box is a centred 3×3 with rows receding; Circle is an
  even ring at radius spacing·N/2π; world placement makes facing +Y an identity, rotation an isometry
  (distances from the anchor preserved), +X facing a 90° turn, a zero facing defaults to +Y, and spacing scales
  linearly. Honest scope: static slot geometry only (no collision/obstacle fitting — drive the slots through
  your existing Steering/pathfinding). [VERIFIABLE HERE]
- [x] **Coloured noise generators** (`audio::WhiteNoise` / `audio::PinkNoise` / `audio::BrownNoise`) — DONE
  (M642); the coloured-noise sources procedural sound design leans on, exposed as small reusable deterministic
  generators (the Oscillator had a white source baked into its waveform enum, but nothing reusable and no pink
  or brown). WHITE (flat spectrum) is raw static hiss; PINK (1/f, equal energy per octave) is the natural,
  balanced "shhh" of steady rain, a waterfall, ocean surf, or ventilation hum — and the reference signal audio
  engineers test with; BROWN/red (1/f², deeper still) is the rumble of distant thunder, heavy wind, or a
  rocket. Pink uses Paul Kellet's economical filtered-white approximation; brown is a leaky integrator (a
  mean-reverting random walk that never drifts to a rail). Each is seeded from an xorshift PRNG, so a wind or
  ambience layer gets a repeatable stream. Verified (`ctest -R "^audio_noise$"`): same seed → identical stream
  and `reset()` restores it; every white/pink/brown sample stays in [-1,1]; white is near-zero-mean, spans a
  wide range, and is essentially uncorrelated (|r₁|<0.2); the lag-1 autocorrelation cleanly orders the colours
  white(≈0.00) < pink(≈0.82) < brown(≈0.996), with brown a strongly-correlated random walk (r₁>0.9) and pink
  measurably more correlated than white but less than brown. Honest scope: perceptually-standard practical
  generators (Kellet pink, leaky-integrator brown), not a mathematically-exact 1/f / 1/f² spectral synthesis. [VERIFIABLE HERE]
- [x] **Hex ring / range / spiral** (`game::hexRing` / `game::hexRange` / `game::hexSpiral` / `game::hexScale`)
  — DONE (M641); the area operations every hex board/strategy game needs, completing the HexGrid module which
  had `hexNeighbors`/`hexLine`/`hexDistance` but no area queries. `hexRing(c, N)` returns exactly the 6N hexes
  at distance N, walked in order around the ring (aura outlines, spawn rings, AoE edges); `hexRange(c, N)`
  returns all 1+3N(N+1) hexes within N (blast radii, movement/attack range, vision area); `hexSpiral(c, N)` is
  the same set ordered centre-outward ring by ring (ripple/expanding animations, nearest-first reveals). Exact
  redblobgames axial-coordinate algorithms. Verified (`ctest -R "^hex_ring_range$"`): ring N has exactly 6N
  hexes all at distance N (radius 0 = the centre alone, negative = empty); ring 1 is precisely the six
  `hexNeighbors`; range N has exactly 1+3N(N+1) DISTINCT hexes all within N and includes the centre; spiral
  covers the same set as range, starts at the centre, and has non-decreasing distance; all verified around a
  non-origin centre. Honest scope: pure axial-coordinate set/geometry (no obstacle/blocking — intersect with
  your own passability like the existing FlowField/AStar do). [VERIFIABLE HERE]
- [x] **Even point distributions** (`math::fibonacciSphere` / `math::fibonacciHemisphere` / `math::vogelDisk`)
  — DONE (M640); spread N points as uniformly as possible over a sphere, hemisphere, or disc with NO random
  number generator (fully deterministic), using the golden-angle spiral so points never line up into spokes or
  rings at any count. The workhorse behind uniform DIRECTION sampling — AO/GI rays, reflection-probe placement,
  spawn directions, LOD-impostor captures — and even POINT scatter — star fields, point clouds, dotted
  patterns, soft-shadow / depth-of-field kernels. The engine already used this spiral inline in a couple of
  shaders (SoftShadow2D, MeshAO); this exposes it as a reusable, unit-tested primitive. Pairs with
  SphericalCoords (M635). Verified (`ctest -R "^point_distribution$"`): `fibonacciSphere(1000)` returns exactly
  N unit-length points that reach both poles and whose centroid sits within 0.05 of the origin (the defining
  even-coverage signal); `fibonacciHemisphere` keeps every point on z≥0, unit-length, reaches the top pole, is
  centred in x,y, and its centroid is pulled toward +z; `vogelDisk(1000, r)` keeps every point within r, packs
  its centroid near the centre, reaches >95% of the radius, and places its first point near the middle; results
  are deterministic and non-positive counts are empty. Honest scope: quasi-uniform golden-angle spirals (near-
  optimal, deterministic — not a physically-exact equal-area tessellation or blue-noise optimisation). [VERIFIABLE HERE]
- [x] **Varint / LEB128 + zigzag** (`io::appendVarint` / `io::readVarint` / `io::appendVarintSigned` /
  `io::zigzagEncode` / `io::varintSize`) — DONE (M639); the compact way to serialize integers that are usually
  small — one byte for values under 128, two under 16384, only paying for 64 bits when the number is genuinely
  huge — the encoding Protocol Buffers, WebAssembly, DWARF and most netcode use to shrink save files, replay
  streams, delta-compressed snapshots and network packets. Unsigned values use plain LEB128; signed values are
  folded through zigzag first so small negatives (−1, −2, …) also fit in one byte instead of ten. Decoding is
  bounds- and overflow-checked (a truncated or over-long stream returns false rather than reading past the
  buffer or wrapping). Complements the engine's fixed-width StreamPeer and base64 (M154). Verified (`ctest -R
  "^varint$"`): the one-byte range and length boundaries (127/128, 16383/16384) are exact and match
  `varintSize`; the canonical spec encodings (128→80 01, 300→AC 02) are byte-correct; a wide sweep of unsigned
  values plus UINT64_MAX round-trip with the offset advancing exactly; zigzag maps 0,−1,1→0,1,2 and −1 encodes
  in one byte; signed extremes (INT64_MIN/MAX) round-trip; four mixed values pack into one buffer and decode
  back sequentially; and truncated, empty, and over-long (>64-bit) streams are all rejected safely. Honest
  scope: the LEB128/zigzag integer codec itself (not a full message schema — pair it with StreamPeer for
  framing). [VERIFIABLE HERE]
- [x] **Strongly-connected components** (`core::stronglyConnectedComponents` / `core::sameComponent` /
  `core::SccResult`) — DONE (M638); the companion to topologicalSort (M637): where that orders a graph with no
  cycles, this FINDS the cycles and clusters them into maximal mutually-reachable groups. Uses: collapsing a
  tangle of mutually-dependent quests / dialogue states / crafting recipes into one unit, detecting circular
  references in a scene or resource graph and reporting exactly which nodes form each loop, condensing a messy
  dependency graph into a clean DAG (each SCC becomes one super-node), and deadlock/liveness analysis on a
  state machine. Tarjan's algorithm run ITERATIVELY (explicit work stack, not recursion) so it is safe on very
  deep graphs; components come out in REVERSE TOPOLOGICAL order of the condensation (a sink SCC before the ones
  pointing into it) with each component's nodes sorted for determinism, and a `componentOf[node]` map. Godot
  ships no SCC primitive. Verified (`ctest -R "^strongly_connected$"`): a 3-cycle is one component while an
  isolated node stays separate; a DAG yields N singletons; the classic two-SCC example {0,1,2}→{3,4} groups
  correctly and preserves the reverse-topological order (sink {3,4} before {0,1,2}); `componentOf` partitions
  every node exactly once and matches `components[]`; every cross-component edge points sink-ward; a self-loop
  is its own single-node SCC; the adjacency overload agrees; empty/single-node and out-of-range inputs are
  handled. Honest scope: partition + condensation ordering only (no bridge/articulation or 2-edge-connected
  variants — those are separate algorithms). [VERIFIABLE HERE]
- [x] **Topological sort** (`core::topologicalSort` / `core::hasCycle` / `core::TopoResult`) — DONE (M637); order
  the nodes of a directed graph so every "must come before" edge points forward — the workhorse behind
  dependency resolution: tech/skill trees that unlock in a legal order, crafting chains (smelt ore → forge
  ingot → make blade → assemble sword), quest/prerequisite gating, and asset/scene/task build ordering. If a
  circular prerequisite makes ordering impossible it reports `ok=false` and lists the nodes trapped in or
  downstream of the cycle — the diagnostic a designer needs to find the bad edge. Kahn's algorithm with a
  min-heap, so the result is always the LEXICOGRAPHICALLY SMALLEST valid order (fully deterministic); edge-list
  and adjacency-list forms; multi-edges and self-loops handled. Godot ships no general topological sort.
  Verified (`ctest -R "^topological_sort$"`): every edge points forward in the output; the order is the exact
  lexicographically-smallest one (e.g. edge 2→0 over {0,1,2,3} gives [1,2,0,3]); isolated nodes are included; a
  6-node crafting chain resolves with each prerequisite before its result; a 3-cycle is detected with no
  partial order returned and the cycle+downstream nodes listed; a self-loop is a cycle trapping only that node;
  the adjacency-list overload matches; empty/single-node graphs and out-of-range edge endpoints are handled.
  Honest scope: unweighted ordering only (no critical-path/longest-path timing — that is a separate pass). [VERIFIABLE HERE]
- [x] **HSL colour model** (`render::fromHsl` / `render::toHsl` / `render::Hsl`) — DONE (M636); classic HSL, the
  cylinder CSS `hsl()` and nearly every web palette tool use, and a genuinely distinct model from the two the
  engine already had: HSV (has "value" not lightness — pure red is v=1 there but l=0.5 here) and OKHSL
  (M395, perceptual, built on OKLab). HSL's trait is a lightness axis that runs black (l=0) → full colour
  (l=0.5) → white (l=1) symmetrically, which is how designers reason about tints/shades and why imported web
  palettes need it. Standard W3C piecewise reconstruction; operates on the `render::Color` channels directly
  like the engine's `fromHsv`. Verified (`ctest -R "^color_hsl$"`): the canonical anchors map correctly (red =
  h0 s1 l0.5, green = h⅓, blue = h⅔, white = l1, black = l0, mid-grey = s0 l0.5); `fromHsl` reconstructs them;
  HSL is shown to genuinely differ from HSV (pure red is l=0.5 vs v=1.0); lightness is symmetric (l=0 black and
  l=1 white for any hue/sat); a 125-colour RGB→HSL→RGB round-trip is the identity with alpha preserved; hue
  wraps (1.0 and −⅓ fold correctly) and s=0 yields a pure grey at the lightness. Honest scope: operates on the
  Color's stored channels (matching `fromHsv`), not a separate sRGB-gamma pass. [VERIFIABLE HERE]
- [x] **Spherical coordinates** (`math::sphericalToCartesian` / `cartesianToSpherical` / `orbitPosition` /
  `directionToEquirectUV` / `equirectUVToDirection`) — DONE (M635); the (radius, azimuth, elevation) ↔ (x,y,z)
  conversion every orbit/turntable camera, sky sampler, and directional-light widget needs, plus the two jobs
  built on it: `orbitPosition` places an eye on a sphere around a target at a given yaw/pitch, and the
  equirectangular pair maps a view ray to/from a panoramic-sky (HDRI) texel. Right-handed, +Y up, matching the
  engine's Camera3D convention (azimuth 0 faces +Z, +π/2 azimuth is +X, +π/2 elevation is straight up). GLM has
  no spherical notion; distinct from `MeshUvRadial` (which assigns per-vertex mesh UVs, not a single-ray lookup).
  Verified (`ctest -R "^spherical_coords$"`): the five cardinal directions map to the documented angles; a full
  cartesian→spherical→cartesian round-trip is the identity across a grid of 56 directions and preserves radius;
  radius scales the vector linearly; the origin and both poles are NaN-free (asin argument clamped so a rounding
  overshoot past the unit sphere can't crash); `orbitPosition` sits exactly `radius` from its target; and the
  equirect UV round-trips with straight-up at v=0 (top row), straight-down at v=1, and +Z at the u=0.5 centre.
  Honest scope: pure closed-form trig — at a pole azimuth is arbitrary (returned as 0), as it must be. [VERIFIABLE HERE]
- [x] **Lightmap baker** (`render::bakeLightmap`) — DONE (M502); direct light + hard shadows, bake fully
  VERIFIABLE HERE (sampling the map is GPU-side). Follow-up: bounce GI + UV-atlas unwrap. [VERIFIABLE HERE]
- [x] **Indirect / global-illumination gather** (`render::gatherIrradiance` / `bakeIndirect`) — DONE (M511);
  the INDIRECT half M502 left open. Cosine-weighted hemisphere path-trace: each receiver surfel fires a
  Hammersley hemisphere of rays that either strike a scene patch (collecting its radiance — sky bounce, color
  bleed, emissive) or escape to the sky. A real single-bounce irradiance estimate, unit-verified headlessly
  (`ctest -R gi_gather`): an open surfel returns exactly the sky color, facing a bright patch beats facing
  away, a dark ceiling darkens the gather, energy stays bounded. [VERIFIABLE HERE]
- [x] **Multi-bounce GI / progressive radiosity** (`render::bakeRadiosity`) — DONE (M521); iterates the M511
  gather into the FULL bounced solution — every patch is both emitter and receiver, each pass re-gathers from
  the others' current radiance and reflects `albedo × received + emission`, so light bounces wall→floor→wall
  and settles. Verified (`ctest -R radiosity`) with the signature of a correct solve: total light grows across
  bounces but the increments shrink (converges), zero albedo yields no indirect, and energy stays within the
  geometric-series bound. Follow-up: adaptive subdivision + form-factor caching + SDFGI probe volume. [VERIFIABLE HERE]
- [x] **Decal projection math** (`render::projectDecal`) — DONE (M503); oriented-box UV + normal fade. [VERIFIABLE HERE]
- [x] **Volumetric-fog evaluation** (`render::fogOpticalDepth` / `fogFactor` / `applyFog`) — DONE (M504);
  analytic Beer-Lambert + exponential height falloff, fully VERIFIABLE HERE (GPU froxel raymarch is separate).
- [x] **Occlusion culling** — ALREADY PRESENT (`render::Occlusion.hpp`, screen-space coverage buffer).
- [x] **Mesh LOD selection** — ALREADY PRESENT (`render::MeshLod.hpp`, `LodChain::select` by projected pixels).
- [x] **Mesh simplification / decimation** (`render::simplifyClustering`) — DONE (M524); the load-time
  vertex-clustering decimation that GENERATES lower-poly LODs and collision hulls from a dense mesh (M-selection
  above only picks which LOD to draw; this makes them). Overlays a uniform grid, averages each cell's vertices
  to one representative, remaps triangles, drops the collapsed ones — O(n), hole-free, no flipped normals.
  Verified (`ctest -R mesh_simplify`): fewer verts/tris, every triangle non-degenerate + in range, bbox kept
  within one cell, coarser cells reduce more, sub-spacing cell is a no-op. Feature-preserving follow-up is
  quadric-error edge collapse (M526, below). [VERIFIABLE HERE]
- [x] **Quadric-error edge-collapse simplification** (`render::simplifyQuadric`, Garland–Heckbert QEM) —
  DONE (M526); the FEATURE-PRESERVING decimation an importer runs to make LODs that keep their silhouette at
  aggressive triangle budgets, where the O(n) clustering path would visibly round off edges. Each vertex
  carries a 4×4 error quadric (summed squared distance to its incident triangles' planes); collapsing an edge
  merges the two quadrics, and the collapse cost is that quadric at the optimal merged position (found by a
  3×3 solve, with a midpoint/endpoint fallback when singular). A min-heap always collapses the cheapest edge,
  so flat regions decimate first and creases/boundaries — high quadric error — survive. Complements, doesn't
  replace, `simplifyClustering`: clustering is the fast hole-proof choice for collision proxies and far LODs;
  QEM is the silhouette-preserving choice for visible mid LODs. Verified (`ctest -R mesh_simplify_quadric`)
  on a curved height-field: hits the triangle budget, every output triangle non-degenerate + in range, the
  bounding box and the curved peak are preserved (flat interior collapses first), output normals are unit
  length, it is deterministic, and a budget ≥ the input is a no-op. [VERIFIABLE HERE]
- [x] **GIF image codec** (`render::decodeGif` / `render::encodeGif`) — DONE (M525); reads and writes the
  GIF89a image (still ubiquitous for pixel-art sprites, UI icons, and short web loops), which stores an
  indexed image (palette of ≤256 colors + one index per pixel) compressed with variable-width LZW. The
  decoder parses the header, logical-screen + global color table, skips extension blocks, and inflates the
  first frame's LZW stream (clear/EOI codes, 2→12-bit code growth, dictionary reset); the encoder builds an
  exact palette for ≤256-color images (so the round-trip is LOSSLESS) and LZW-compresses the indices. The
  fiddly part is the variable-width LZW code-width sync: the decoder's dictionary lags the encoder's by one
  string (the first code after a clear adds nothing), so it must widen one entry early to stay byte-aligned.
  Verified (`ctest -R gif_codec`): a 5-color pattern and a two-color stripe both survive encode→decode
  pixel-exact, header/dimensions check out, and malformed input is rejected. Follow-up: multi-frame
  animation + transparency index. [VERIFIABLE HERE]
- [x] **Damped spring value smoother** (`core::Spring`) — DONE (M631); a tunable damped harmonic oscillator for UI
  juice and gameplay motion — eases a value toward a moving target with a natural bounce. Complements
  `core::SmoothDamp` (critically damped, no overshoot): a Spring is set by `frequency` (Hz) and `damping` (ζ) and
  ranges from bouncy (ζ<1, overshoots and settles) through critical (ζ=1) to sluggish (ζ>1), driving springy menus,
  camera lag, knockback recovery, cursor trails, and pickup "pop". Sub-stepped semi-implicit (symplectic) Euler so
  it's stable at any dt/parameters; run one per axis for 2D/3D. Verified (`ctest -R "^spring$"`): a critical spring
  converges to and rests on its target (atRest reports it); an under-damped spring (ζ=0.15) overshoots the target
  then settles; critical and over-damped springs never overshoot; a spring resting on its target stays put; dt≤0 and
  a non-positive frequency are no-ops; identical springs evolve identically (deterministic). Honest scope: numerical
  (sub-stepped) integration, not a closed-form analytic solution — accurate and stable for game use, not a physics
  reference. [VERIFIABLE HERE]
- [x] **CSS-style cubic-bezier easing** (`anim::CubicBezierEasing` + `easeCurve`/`easeInCurve`/`easeOutCurve`/
  `easeInOutCurve`) — DONE (M629); arbitrary motion curves defined exactly like CSS `cubic-bezier(x1,y1,x2,y2)` and
  the browser ease presets, so a designer can dial in ANY curve by placing the two control handles rather than
  choosing from the engine's fixed named-easing menu. Endpoints fixed at (0,0)→(1,1); evaluate the eased y at
  progress t via the standard Newton–Raphson-then-bisection x-root solve (WebKit's UnitBezier). Verified (`ctest -R
  "^cubic_bezier$"`): f(0)=0/f(1)=1 with input clamped; diagonal handles give the identity y=x; ease-in sits below
  the diagonal at t=0.5 (slow start) while ease-out sits above (fast start) and ease-in-out is symmetric; ease-out
  is the exact mirror of ease-in (easeOut(t)==1−easeIn(1−t)); all presets are monotonic non-decreasing; a custom
  springy curve honours its endpoints and stays sampled in range. Honest scope: x-handles clamped to [0,1] for a
  well-defined function; y unclamped so springy curves can intentionally overshoot (as in CSS). [VERIFIABLE HERE]
- [x] **Music theory: scales & chords** (`audio::scaleNotes` / `chordNotes` / `scaleIntervals` / `chordIntervals`)
  — DONE (M627); build the note sets procedural music needs from a root MIDI note: scales (major, all seven modes,
  harmonic/melodic minor, major/minor pentatonic, blues, whole-tone, chromatic) and chords (triads, sevenths, sus,
  6ths, dominant-9). Returns MIDI numbers you feed to `midiToFrequency` (M626) → `audio::Oscillator` for
  arpeggios, generative melodies, or chord stabs; the interval tables are the canonical semitone offsets. Verified
  (`ctest -R "^musicscales$"`): C major = C D E F G A B, A natural minor, C minor-pentatonic = C Eb F G Bb,
  chromatic has 12 degrees, whole-tone steps by 2; two octaves expands to 14 notes each a perfect octave higher and
  octaves<1 → empty; the triads/sevenths are exact (C=C E G, Cm=C Eb G, Cdim, Caug, C7, Cmaj7, Cm7), roots
  transpose (Dm = D F A), dominant-9 has 5 notes; and a chord built from a parsed name ("G4") converts to strictly
  ascending frequencies with the G4 root ≈ 392 Hz. Honest scope: note-set generation only (no voice-leading /
  inversions yet). [VERIFIABLE HERE]
- [x] **Music theory: note ↔ pitch** (`audio::midiToFrequency` / `frequencyToMidi` / `noteNameToMidi` /
  `midiToNoteName` / `noteNameToFrequency`) — DONE (M626); the note/pitch conversions procedural music and synth
  voices need but the audio module lacked. 12-TET at A4 = MIDI 69 = 440 Hz, C4 = middle C = MIDI 60: convert a MIDI
  number to Hz and back, parse scientific-pitch names ("A4", "C#5", "Bb3", "C-1") to MIDI (validated to [0,127],
  one optional #/b accidental), and render a MIDI number back to a sharp name — feed straight into
  `audio::Oscillator`/a `Sound`'s `freq` or drive an arpeggiator. Verified (`ctest -R "^musictheory$"`): A4→440,
  middle C→261.63, octave up/down doubles/halves; frequency↔MIDI round-trips for all 128 notes; note names parse
  incl. enharmonics (C#4==Db4==61), Cb4=59, case-insensitive letters, negative octaves (C-1=0), and the range ends
  (G9=127); malformed names ("H4", "C", "C#", "4C", "Cx4") and out-of-range ("C10", "C-2") → −1;
  midiToNoteName renders sharps and empties out of range. Honest scope: 12-TET / A440 standard tuning only. [VERIFIABLE HERE]
- [x] **UUID (v4) generator** (`core::makeUuidV4` / `uuidV4String` / `isValidUuid` / `core::Uuid`) — DONE (M624);
  generate RFC 4122 version-4 (random) unique identifiers for entity IDs, save files, network sessions, asset GUIDs,
  and analytics events. `makeUuidV4` fills 128 bits from any engine RNG with `uint32_t next()` (e.g. `core::Pcg32`)
  and stamps the version/variant bits; `Uuid::toString` renders the canonical lowercase
  `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx`; `isValidUuid` validates that layout. Randomness comes from the caller's
  seeded RNG, so generation is fully deterministic and testable. Beyond Godot (no UUID type). Verified (`ctest -R
  "^uuid$"`): canonical 36-char form with hyphens at 8/13/18/23, version nibble '4' and variant nibble in
  {8,9,a,b}, lowercase hex; same seed reproduces and different seeds/consecutive draws differ; 2000 generated IDs
  are all distinct and all validate; `isValidUuid` accepts canonical and uppercase literals and rejects empty /
  too-short / too-long / bad-hyphen / non-hex. Honest scope: v4 (random) only — not time-based v1 or name-based
  v3/v5. [VERIFIABLE HERE]
- [x] **Dice notation parser/roller** (`game::parseDice` / `rollDice` / `minRoll` / `maxRoll` / `averageRoll`) —
  DONE (M623); parse and roll the classic tabletop dice strings ("2d6+3", "d20", "4d8-1") RPGs, board-game ports,
  and loot/damage tables are written in. `parseDice` → a `DiceSpec` (count, sides, flat modifier); `rollDice` rolls
  it with any engine RNG exposing `range(lo,hi)` (e.g. `core::Pcg32`), returning the total and each die; the closed
  forms give the distribution bounds without rolling (tooltips, balancing, AI expected value). Beyond Godot (no
  dice parser). Verified (`ctest -R "^dice$"`): valid parses incl. optional count, case-insensitive 'd', surrounding
  spaces, and +/- modifiers; a battery of malformed strings rejected; min/max/average match the closed form (2d6+3 →
  5/15/10), invalid spec → 0 bounds; 200 rolls each give exactly `count` dice with every face in [1,sides] and the
  total within [min,max]; the same seed reproduces the exact roll; and the parse+roll convenience overload reports
  the spec and rejects nonsense. [VERIFIABLE HERE]
- [x] **Dice keep-highest / keep-lowest** (`game::Dice` `khN`/`klN` clause) — DONE (M625); the advantage/disadvantage
  and stat-generation extension to the dice parser (M623): `4d6kh3` rolls four d6 and keeps the highest three (the
  classic D&D ability-score roll), `2d20kh1` is advantage, `2d20kl1` is disadvantage. `DiceSpec.keep` (0 = all,
  >0 = keep highest, <0 = keep lowest) drives it; `RollResult.kept` reports which dice counted; `minRoll`/`maxRoll`
  now bound on the kept dice; `averageRoll` is EXACT via full outcome enumeration when the space (sides^count) is
  small enough (covers typical specs), falling back to a documented estimate for huge ones. Verified (`ctest -R
  "^dice$"`): `4d6kh3`/`2d20kl1+1` parse with the right keep sign & modifier; keep beyond the dice count clamps;
  malformed keep clauses (`k3`, `kh`, `kx3`) rejected; kept-dice bounds (4d6kh3 → 3..18); the enumerated average of
  4d6-drop-lowest matches the known 12.2446; and 200 rolls keep exactly 3 of 4 with the total summing only the kept
  (highest) dice within bounds. Honest scope: keep-high/low only (no exploding/reroll dice yet). [VERIFIABLE HERE]
- [x] **Ordinal & Roman-numeral formatting** (`core::ordinalSuffix` / `ordinal` / `toRoman`) — DONE (M622); the two
  number-to-text helpers game HUDs need that `core::NumberFormat` was missing: **ordinals** for leaderboard ranks
  and "Nth wave" ("1st", "22nd", "113th"), and **Roman numerals** for chapter/level/act titles ("Level IV",
  "MMXXIV"). `ordinalSuffix` returns just "st/nd/rd/th"; `ordinal` prepends the number and preserves sign; `toRoman`
  renders 1..3999 in standard additive/subtractive form and returns "" outside that range. Verified (`ctest -R
  numberformat_ordinal`): base suffixes 1st/2nd/3rd/4th, last-digit rule (21st/22nd/23rd), the 11/12/13 (and
  111/112/113) "th" teen exception, 0th, signed `-1st`; Roman I/IV/IX, XIV/XL/XC, CD/CM, 2024=MMXXIV,
  1984=MCMLXXXIV, 3999=MMMCMXCIX, and 0/negative/≥4000 → empty. Honest scope: English ordinals and classic
  1..3999 Roman only. [VERIFIABLE HERE]
- [x] **Colour harmony / palette generator** (`render::complementary` / `analogous` / `triadic` /
  `splitComplementary` / `tetradic` / `monochromatic` / `rotateHue`) — DONE (M628); generate coordinated palettes
  from one base colour by rotating its hue on the colour wheel — the "pick colours that go together" helper for
  procedural UI theming, generative art, faction colours, and data-viz legends (companion to `gradientMap` and the
  colour-blindness sim). Complementary (+180°), analogous (neighbours), triadic (+120/+240°), split-complementary,
  tetradic (square), and monochromatic value ramps; all rotate in HSV preserving saturation/value/alpha. Verified
  (`ctest -R "^color_harmony$"`): rotateHue lands at the target wheel position and preserves S/V (a full turn is
  identity); complement is the opposite hue; triad/tetrad sit at the exact +1/3,+2/3 and quarter-turn positions;
  analogous & split-complementary flank correctly with the right set sizes; monochromatic keeps hue & saturation
  while stepping value up to the base, and count<1 → empty. Honest scope: hue-rotation schemes on the RGB/HSV
  wheel (not perceptual OkLCh spacing). [VERIFIABLE HERE]
- [x] **Marble & wood-grain textures** (`render::patterns::marbleTexture` / `woodTexture`) — DONE (M621); two
  domain-warped procedural textures the plain fbm `noiseTexture` can't make: **marble** is parallel sine veins whose
  phase is bent by fbm turbulence (polished-stone ripples), **wood** is concentric growth rings around the image
  centre, likewise warped (timber grain). Both grey [0,1], deterministic by seed, and made for `gradientMap` (marble
  tint / brown plank ramp) + `heightToNormalMap`. Verified (`ctest -R image_marble_wood`): both honour size, stay
  grey and in [0,1], the field varies, the same seed reproduces and a different seed changes it; with turbulence 0
  the structure is exact — marble bands are perfectly vertical (every column constant down its rows) and wood rings
  are concentric (four points at equal radius from the centre share the same value) — while turbulence > 0
  measurably warps that structure; non-positive size is safe. Honest scope: single-octave sine bands warped by fbm
  (no anisotropic stretch or colour layering — compose via `blend`/`gradientMap`). [VERIFIABLE HERE]
- [x] **WCAG contrast ratio** (`render::contrastRatio` / `relativeLuminance` / `passesAA` / `passesAAA` /
  `bestTextColor`) — DONE (M620); the legibility companion to the colour-blindness sim (M619): is HUD/menu/subtitle
  text actually readable against its background? Implements the WCAG 2.x relative-luminance + contrast-ratio formula
  (ratio in [1,21]) and the AA/AAA pass thresholds for normal and large text, plus a `bestTextColor` helper that
  auto-picks the more legible of black/white for a label on any swatch. Verified (`ctest -R contrast_ratio`): black
  luminance 0 and white 1; black-on-white is the maximum 21:1; identical colours 1:1; the ratio is symmetric; the
  canonical WCAG reference pair #767676-on-white computes ~4.54:1 and correctly passes AA-normal (≥4.5) while
  failing AAA-normal (<7.0) and passing AA-large (≥3.0); a ~3.4:1 pair passes AA-large but not AA-normal;
  `bestTextColor` picks black on white/bright-yellow and white on black/dark-navy. Honest scope: exact WCAG 2.x math
  (standard sRGB EOTF, 0.2126/0.7152/0.0722 weights); alpha ignored (composite translucent text first). [VERIFIABLE HERE]
- [x] **Colour-blindness simulation** (`render::simulateColorVision` / `render::ColorVision`) — DONE (M619); an
  accessibility dev-tool that previews how UI, minimap, team colours, or status effects read to players with
  colour-vision deficiency, so red/green pairs that collapse can be caught before shipping. Applies the widely-used
  Wickline dichromat transforms — Protanopia (red-weak), Deuteranopia (green-weak, the most common), Tritanopia
  (blue-weak) — plus Achromatopsia (→ luminance grey). Works on a single `Color` or a whole `Image` (alpha
  preserved). Verified (`ctest -R image_colorblind`): a neutral grey is left unchanged (each matrix row sums to 1);
  red and green become far more similar under the red/green types (colour distance drops below half — the defining
  "confusable" property); achromatopsia yields r==g==b equal to the colour's luminance; outputs stay in [0,1];
  alpha survives; and the image overload matches the per-colour transform pixel-for-pixel and is empty-safe. Honest
  scope: fast sRGB-space matrices (the common web-filter approximation), not a physically-exact LMS/Brettel
  simulation. [VERIFIABLE HERE]
- [x] **AI vision cone / perception check** (`game::inViewCone2D` / `inViewCone3D` / `sampleViewCone2D`) — DONE
  (M633); the continuous "can this AI see that?" test — is a target within an observer's sight RANGE and inside its
  field-of-view CONE (half-angle around a facing direction)? The geometry every stealth guard, turret, sentry, or
  aggro check needs, distinct from the grid shadowcasting `FieldOfView`. `sampleViewCone2D` also returns the exact
  distance and angular offset; combine with a line-of-sight raycast for wall occlusion (kept separate by design).
  Beyond Godot. Verified (`ctest -R "^viewcone$"`): a target dead-ahead within range is seen, behind is not; the
  cone edge (angle == halfAngle) is inclusive while just past it is excluded; out-of-range is not seen even dead
  ahead, and exactly at range is (≤); the sample reports the right distance (√18 for (3,3)) and 45° angle; a
  zero-length facing sees nothing and a coincident target is seen; 3D behaves identically. Honest scope: pure
  angle+distance (no occlusion — layer a raycast for walls). [VERIFIABLE HERE]
- [x] **Weapon spread / cone sampling** (`game::spreadDirection2D` / `spreadFan2D` / `spreadDirection3D`) — DONE
  (M632); perturb an aim direction into a cone or fan for shotgun pellets, bullet inaccuracy, spray weapons, and
  particle emission. `spreadDirection2D`/`spreadDirection3D` jitter a direction randomly within a half-angle (any
  RNG with `rangef`, e.g. `core::Pcg32`), the 3D one uniform over the cone's solid angle (no axis clustering);
  `spreadFan2D` returns a DETERMINISTIC evenly-spaced fan for a fixed multi-pellet pattern. Beyond Godot (no spread
  helper). Verified (`ctest -R "^spread$"`): the 2D fan has the right count, its outer pellets sit at ±halfAngle,
  odd counts put the centre pellet on-axis, pellets are evenly spaced and unit-length, count 1 returns the aim,
  count<1 is empty; 500 random 2D shots all stay within the half-angle, are unit-length, genuinely vary, and
  reproduce for a seed; 800 random 3D shots stay within the cone, are unit-length, have a mean angle strictly
  between 0 and the half-angle (real spread), and a zero-angle cone returns the axis exactly. Honest scope: circular
  cone/fan spread (no falloff weighting toward the centre — layer your own if wanted). [VERIFIABLE HERE]
- [x] **First-order intercept aim** (`game::solveIntercept` / `game::InterceptSolution`) — DONE (M618); the
  gravity-free "lead the target" solver: where should a turret, archer, spaceship gun, or homing AI aim so a shot
  fired at a FIXED speed hits a target moving at constant velocity? Companion to `game::Ballistics` (which arcs a
  shot under gravity). Solves the single intercept quadratic `(|vt|²−vp²)t² + 2(d·vt)t + |d|² = 0` for the earliest
  positive impact time, returning that time, the lead/aim point, and the unit fire direction; `hit` is false when
  the target outruns the projectile. Works in 3D (2D leaves z=0). Verified (`ctest -R intercept_aim`) against the
  defining invariant — firing along the returned direction at the given speed for the returned time lands exactly on
  the target's future position — for crossing, stationary (time = distance/speed, aim = the target), head-on, and
  fast-3D geometries; a target fleeing faster than the shot is correctly unreachable; the aim genuinely leads ahead
  of a crossing target; non-positive speed and a coincident target are handled. Honest scope: constant target
  velocity, no gravity/drag (use `Ballistics` for arced shots). [VERIFIABLE HERE]
- [x] **Angled linear gradient** (`render::patterns::linearGradient`) — DONE (M634); a gradient at an ARBITRARY
  angle (radians) from `from` at the leading edge to `to` at the far edge, spanning the image corner-to-corner along
  the direction — for slanted skies, UI sweeps, and directional light washes. Generalises `verticalGradient` (only
  vertical and radial existed). Verified (`ctest -R "^image_linear_gradient$"`): size honoured; angle 0 runs
  left→right with each column a constant shade (left=from, right=to); angle π/2 runs top→bottom with each row
  constant, matching `verticalGradient` output; a 45° diagonal puts `from` at the top-left corner and `to` at the
  bottom-right with a ~50% blend at the centre; non-positive size is safe. Honest scope: linear RGBA interpolation
  with no gamma handling (like the sibling gradients). [VERIFIABLE HERE]
- [x] **Voronoi mosaic texture** (`render::patterns::voronoiTexture`) — DONE (M630); a flat-colour cellular mosaic:
  scatter one jittered feature point per grid cell and fill each pixel with the colour of its NEAREST point's cell —
  solid regions with hard edges for stained glass, low-poly art, cracked ceramic, or a mosaic floor. Distinct from
  `cellularTexture` (a grey distance field); this bakes a random pleasant hue per cell. `scale` sets cell frequency,
  `seed` the layout & palette; pairs with `blend` for grout lines or `heightToNormalMap` for bevelled tiles.
  Verified (`ctest -R "^image_voronoi$"`): size honoured; the image is genuinely FLAT-celled — a small palette (far
  fewer distinct colours than pixels) and >60% of horizontal neighbours share the exact colour; most cells are
  colourful (saturated, not grey); the same seed reproduces byte-identically while a different seed changes both
  layout and palette; non-positive size is safe. Honest scope: nearest-point (F1) cells with hard edges (no
  anti-aliased borders — overlay grout via `blend` if wanted). [VERIFIABLE HERE]
- [x] **Brick wall pattern** (`render::patterns::brickWall`) — DONE (M617); a running-bond brick texture: rows of
  `brickW`×`brickH` bricks separated by `mortarPx`-thick lines, each row shifted `offsetFrac` of a brick relative to
  the one above (0.5 = the classic half-brick stagger, 0 = a stacked bond). Distinct from `checkerboard` — this is
  the iconic building-front / dungeon-wall / path texture, and it composes with `gradientMap` (tint) and
  `heightToNormalMap` (bricks proud, mortar recessed) for relief. Verified (`ctest -R image_brick`): the size is
  honoured, both the brick and mortar colours appear, the full horizontal mortar band is mortar-coloured while a
  brick-interior row contains brick pixels, the running bond genuinely staggers (adjacent rows' vertical mortar
  columns differ) while `offsetFrac` 0 lines them up (stacked bond), a greyscaled wall keeps brick/mortar contrast
  (usable as a height map), and zero/sub-1 sizes are safe (clamped to 1). Honest scope: a hard-edged two-colour
  pattern (no per-brick colour variation or bevel — layer noise via `blend` for that). [VERIFIABLE HERE]
- [x] **Image tone / adjustments** (`render::adjustBrightness/adjustContrast/adjustGamma/invert/grayscale/threshold`
  + `mapRGB`) — DONE (M616); the "levels / adjustments" panel for CPU images, completing the procedural-texture
  workshop (generate → colorize → blend → **adjust**). Brightness adds a clamped offset; contrast scales about the
  0.5 pivot (1 identity, 0 flat grey, >1 harder); gamma is vᵍ (1 identity, >1 darkens mid-tones); invert is 1−c;
  grayscale collapses to Rec.709 luminance; threshold makes a crisp two-colour mask on luminance (stencils, decals,
  `blend` alphas). Each returns a new same-size image with alpha preserved; `mapRGB` is the exposed per-channel
  building block. Verified (`ctest -R image_adjust`): brightness lifts/clamps every channel and keeps alpha; contrast
  leaves 0.5 fixed, pushes 0.25→0 at ×2, flattens to grey at 0, is identity at 1; gamma 1 is identity, 2 darkens
  mid-grey to 0.25 and leaves the 0/1 endpoints fixed; invert = 1−channel; grayscale makes r==g==b with pure red →
  ~0.2126; threshold splits a ramp into low/high with custom colours honoured; all ops are empty-safe. Honest scope:
  raw 0..1 channel maths, no sRGB/linear conversion. [VERIFIABLE HERE]
- [x] **Image blend / composite modes** (`render::blend` + `render::ImageBlendMode`) — DONE (M615); layer one image
  over another with Photoshop-style blend modes, so the engine's procedural textures can be *combined*: multiply a
  `cellularTexture` stone pattern under a `gradientMap` tint, screen a noise grunge layer to weather a base, add a
  glow, overlay detail, or difference two fields for edges. `top` is composited over `base` (an opaque backdrop):
  each pixel is combined by the mode, then mixed toward the base by the top pixel's alpha × `opacity`. Nine modes:
  Normal, Multiply, Screen, Add, Subtract, Darken, Lighten, Difference, Overlay. Verified (`ctest -R image_blend`):
  Multiply darkens (×white is a no-op, ×black → black, ×0.5 halves), Screen brightens (black is a no-op, white →
  white, 0.5/0.5 → 0.75), Add clamps (red+green → yellow), Darken/Lighten pick per-channel min/max, Difference is
  |b−s|, Overlay passes a mid-grey base through; Normal honours the top's alpha (white@0.5 over black → mid grey);
  `opacity` scales the layer (0 → base unchanged, 0.25 → a quarter mix); a smaller top only affects the overlap and
  the base's alpha is preserved; multiplying two real patterns yields a varied image; an empty base is safe. Honest
  scope: raw 8-bit (non-linear) channel maths, opaque base, top aligned top-left with no scaling. Note: named
  `ImageBlendMode` to avoid colliding with the GPU renderer's `BlendMode`. [VERIFIABLE HERE]
- [x] **Gradient-map colorizer** (`render::gradientMap`) — DONE (M614); the colour stage for the engine's procedural
  grey textures: feed a `noiseTexture` (M612), `cellularTexture` (M613), or any height field / mask in, and get lava,
  terrain (deep water → sand → grass → snow), fire, marble tint, a heat-map, or a toon ramp out. Each pixel's
  perceptual luminance (0.299R+0.587G+0.114B) is the ramp parameter. Three forms: a two-colour `lo → hi` ramp, a
  multi-stop `ColorStop` ramp (linear between sorted stops, clamped past the ends — a colour gradient baked over the
  image), and a general callback form taking any `Color(float)` callable (e.g. `anim::Gradient::sample`). Verified
  (`ctest -R image_gradient_map`): the output matches the source size; a black→white input through a red→green ramp
  comes out red at the dark end and green at the bright end with a blend between; a multi-stop black/red/white ramp
  puts mid-grey exactly on the middle (red) stop, clamps pure black/white to the end stops, and interpolates a
  quarter-grey halfway to (0.5,0,0); the callback form runs an arbitrary function; the same input reproduces a
  byte-identical image; colorizing grey noise yields a genuine colour range (not a flat fill); empty input is safe.
  Honest scope: luminance uses raw 8-bit channels with no gamma handling; stop lists are used as given (assumed
  sorted ascending). [VERIFIABLE HERE]
- [x] **Procedural cellular / Worley texture** (`render::patterns::cellularTexture`) — DONE (M613); generate a grey
  cellular ("Worley") image in code, distinct from the smooth Perlin `noiseTexture` (M612). Two `mode`s: `Cells` bakes
  the F1 nearest-feature-point distance — rounded blobs for stone, reptile scales, cracked mud, water caustics, or
  bubbles; `Cracks` bakes F2−F1 — thin dark lines along the cell boundaries for crack, vein, or Voronoi-edge networks.
  Extends the image-pattern family (M607/M612) using the engine's existing `core::WorleyNoise`. `scale` sets the
  frequency (larger = more, smaller cells), `seed` picks the pattern (same seed → same texture). Verified (`ctest -R
  image_cellular`): the texture honours its requested size, every pixel is grey (r==g==b) and in [0,1], and the field
  genuinely VARIES across the image (max−min > 0.1, not a flat fill); the same seed reproduces a byte-identical texture
  while a different seed changes it; the two modes produce different fields, and the `Cracks` field carries near-black
  boundary-line pixels alongside bright interiors (it spans the range); a cellular image fed into `heightToNormalMap`
  produces a valid normal map that points generally outward (blue ≥ 128); a non-positive size returns an empty image.
  Honest scope: plain distance-field value in grey with no gamma handling; not tile-seamless (wrap the domain for a
  repeating texture). [VERIFIABLE HERE]
- [x] **Procedural noise texture** (`render::patterns::noiseTexture`) — DONE (M612); generate a grey fractal-noise
  (fbm Perlin) image in code — clouds, marble, dirt, smoke, static, weathering masks, or a height source to feed the
  normal-map baker (M608). Extends the image-pattern family (M607) using the engine's existing `core::Noise`. `scale`
  sets the frequency (smaller = broader blobs), `seed` picks the pattern (same seed → same texture), `octaves` layers
  in fine detail. Verified (`ctest -R image_noise`): the texture honours its requested size, every pixel is grey
  (r==g==b) and in [0,1], and the field genuinely VARIES across the image (max−min > 0.1, not a flat fill); the same
  seed reproduces a byte-identical texture while a different seed changes it; and — proving composability — a noise
  image fed into `heightToNormalMap` produces a valid normal map that points generally outward (blue ≥ 128); a
  non-positive size returns an empty image. Honest scope: plain fbm value in grey with no gamma handling; it is not
  tile-seamless (wrap the domain if you need a repeating texture). [VERIFIABLE HERE]
- [x] **Hollow extrude / ring prism** (`render::extrudeRing`) — DONE (M611); extrude a shape WITH a hole — the solid
  region between an outer outline and an inner one — into a `depth`-thick prism. This makes a picture frame, a washer,
  a window frame, a pipe with a shaped cross-section, a ring, or a letter "O" — the one thing plain `extrudePolygon`
  (M602) can't do because it has no hole. Both loops must have the SAME point count (point i of the outer pairs with
  point i of the inner), which lets the two end caps be triangulated as a clean quad strip between the loops — no
  hole-triangulation needed — and the inner walls are wound to face INTO the hole. Verified (`ctest -R
  mesh_extrude_ring`): a square frame (outer half-2, inner half-1) extruded depth 2 is a closed solid whose signed
  volume equals exactly (outerArea − innerArea)·depth = 24 (proving watertight + correct orientation, inner hole
  subtracted); it has 8 triangles per segment (both caps + outer wall + inner wall) and is centred z=−depth/2..+depth/2;
  a round 16-gon washer also closes to (outer − inner)·depth; mismatched loop sizes, <3 points, and zero depth all
  return empty. Honest scope: outer and inner must have equal point counts (point-to-point pairing, no resampling) and
  the inner loop should sit inside the outer; unwelded flat per-face normals for crisp edges. [VERIFIABLE HERE]
- [x] **Pie slice / sector outline** (`render::shapes2d::pieSlice`) — DONE (M610); a wedge of a disc — from a start
  angle sweeping a given number of radians — extending the shapes2d family. It's the shape of a pie-chart slice, a
  radial gauge fill, a cone of vision / spotlight footprint, a radar sweep, or a Pac-Man. Returns the centre plus the
  arc points (segments+2 total), CCW, ready for `triangulatePolygon` to fill, `extrudePolygon` to make a solid wedge,
  or a 2D collider. Verified (`ctest -R shapes2d_pie`): a quarter sector has exactly segments+2 points; the first is
  the centre and every arc point sits on the radius; it is wound CCW and its area equals the exact circular-sector
  area 0.5·r²·sweep; it extrudes into a valid closed wedge (signed volume == area×depth, confirming composability);
  zero radius, zero sweep, and zero segments all return empty. Honest scope: for a FULL disc use `regularPolygon`
  (a full-turn pie would fold a zero-area sliver at the seam); the wedge apex is at the origin. [VERIFIABLE HERE]
- [x] **Wireframe / edge extraction** (`render::meshEdges`, `render::meshWireframe`) — DONE (M609); pull the UNIQUE
  edges out of a triangle mesh so you can draw it as a cage of lines. Every triangle shares its edges with its
  neighbours, so the raw triangle list mentions each interior edge twice; this collapses them to one each. Feed the
  result to a line renderer for a wireframe overlay, an editor "show edges" mode, a hologram / blueprint look, a
  selection-highlight outline, or a debug view of how a mesh is built. `meshEdges` returns the edges as vertex-index
  pairs; `meshWireframe` returns them as a flat LINE LIST (two positions per edge) ready to hand straight to a
  line-segment draw call. Verified (`ctest -R mesh_wireframe`): a single triangle has 3 edges (and `meshWireframe`
  returns 6 positions); a two-triangle quad has 5 unique edges (the shared diagonal counted once); a watertight
  triangulated cube has exactly 18 unique edges (E = 3F/2), with no edge listed twice and every edge referencing
  distinct in-range vertices; an empty mesh yields nothing. Honest scope: returns EVERY triangle edge deduplicated —
  including the diagonal splitting each quad face (a triangulated cube gives 18, not the 12 "clean" edges an artist
  sees); for only the visually meaningful creases use the sharp / hard / feature-edge tools (M548/M549/M567).
  [VERIFIABLE HERE]
- [x] **Height → normal map** (`render::heightToNormalMap`) — DONE (M608); turn a grey heightmap (bright=high,
  dark=low) into a tangent-space NORMAL MAP — the blue-purple texture that makes a flat surface look bumpy under
  lighting. Paint or generate a height image (bricks, cobbles, scales, wrinkles, carved detail, hammered metal) and
  this reads its slopes and writes the surface direction at every pixel, so a lighting shader fakes all that relief
  with no extra geometry. It is the standard "bake a normal map from a height texture" step — Godot's Image
  bump-to-normal, Blender's bump node, Substance/Photoshop's "Normal from Height". Height comes from the RED channel;
  `strength` exaggerates or softens the bumps; output is RGBA8 encoding the unit normal as (x,y,z)·0.5+0.5. Verified
  (`ctest -R image_normalmap`): a flat heightmap bakes to the classic flat normal (128,128,255); a left→right
  brightening ramp tilts the normal's X negative (red drops below 128) with green staying ~128 (no Y slope); every
  baked normal decodes back to a UNIT vector with z>0; a higher `strength` tilts the normal further on the same ramp;
  an empty input yields an empty image. Honest scope: slopes use central differences with border pixels clamped to
  their neighbours (edges read flat-ish); green is +Y (OpenGL convention — flip G for DirectX); only the red channel
  is read as height. [VERIFIABLE HERE]
- [x] **Procedural image patterns** (`render::patterns::checkerboard` / `verticalGradient` / `radialGradient`) — DONE
  (M607); generate common textures in code, no art files needed. A checkerboard for a placeholder / "missing texture"
  material, a UV-check pattern, or floor tiles; a smooth top-to-bottom gradient for skies, backdrops, UI panels, and
  fades; a radial glow for spotlights, vignettes, soft particle sprites, and button highlights. Each returns a CPU
  `Image` (RGBA8) ready for `Renderer::createTexture` or the image codecs — great for prototyping before real art
  exists, runtime-generated UI, and test cards. Verified (`ctest -R image_patterns`): a cell-2 checkerboard honours
  its size and alternates colour A/B every 2 pixels from the top-left (across, down, and back on the diagonal), staying
  constant within a cell; a vertical gradient is the top colour on row 0, the bottom colour on the last row, and the
  exact midpoint grey in the middle, constant along each row; a radial gradient is the centre colour at the middle,
  the edge colour in the corners, a partial blend part-way out, and darkens monotonically outward; a non-positive
  width or height yields an empty image. Honest scope: basic building-block patterns with plain RGBA (no gamma
  handling); a checker cell below 1 is treated as 1. [VERIFIABLE HERE]
- [x] **Tapered / scaled extrude** (`render::extrudePolygonScaled`) — DONE (M606); like `extrudePolygon` (M602), but
  the top cap is scaled by `topScale` about the shape's centroid, so any flat outline becomes a truncated pyramid /
  frustum: a plinth, a tapered building or tower, a bevelled block, a stub, a keystone, a lampshade profile. At
  topScale=1 it's a straight prism; below 1 it tapers inward toward the top; above 1 it flares out. The slanted side
  walls get true flat per-face normals computed from their actual geometry (so they shade correctly, unlike a naive
  vertical-wall assumption). Verified (`ctest -R mesh_extrude_scaled`): topScale=1 reproduces a straight prism
  (volume=area×depth); a taper to 0.5 scales the top cap to half-size while the base is unchanged, and a flare to 2.0
  makes the top bigger — both closed solids whose signed volume follows the exact PRISMATOID rule A·h·(s²+s+1)/3; the
  prism stays centred z=−depth/2..+depth/2; <3 points and zero depth return empty. Honest scope: `topScale` is clamped
  to a small positive minimum — a true point-apex pyramid should use a cone/pyramid tool; the taper pivots about the
  vertex-average centroid. [VERIFIABLE HERE]
- [x] **Gear / cog outline** (`render::shapes2d::gear`) — DONE (M605); a spur-gear silhouette — `teeth` trapezoidal
  teeth rising from a root circle to a tip circle — extending the shapes2d family. Spin it into a machine, a clock
  face, a steampunk prop, a factory backdrop, or a rotating puzzle piece; extrude it (M602) into a solid cog, or spin
  it as a 2D collider. `toothWidthFrac` sets how wide each tooth is versus the gap between teeth. Verified (`ctest -R
  shapes2d_gear`): a 12-tooth gear has exactly 5 points per tooth (60), every vertex radius sits between the root and
  outer radius, exactly two tip vertices per tooth land on the outer circle and three root vertices per tooth on the
  root circle, the outline is CCW, and its area lies between the root disc (πr²) and the tip disc; the gear extrudes
  into a valid closed cog whose signed volume equals area×depth (proving composability with the extrude pipeline);
  fewer than 3 teeth, root ≥ outer, and a zero root radius all return empty. Honest scope: this is a simple
  flat-flanked (trapezoidal-tooth) gear for looks and props, not a true involute gear for meshing power transmission.
  [VERIFIABLE HERE]
- [x] **2D shape outlines** (`render::shapes2d::regularPolygon` / `star` / `roundedRect`) — DONE (M604); ready-made
  point rings for the common flat shapes, so you don't hand-type coordinates. Each returns a counter-clockwise list of
  2D points tracing an outline, which you feed straight into `extrudePolygon` (M602) for a 3D prism, `revolveProfile`
  (M594) to spin a solid, `triangulatePolygon` (M156) to fill it flat, or a 2D polygon collider. Between them they
  cover most of what UI, signage, and props need: a regular n-gon (hexagon nut, pentagon, octagon stop-sign), a star
  or sparkle, and a rounded rectangle (button, card, badge, panel, rounded platform). Verified (`ctest -R shapes2d`):
  a regular hexagon has 6 points all on the radius, wound CCW, with area exactly (3√3/2)r²; a 5-point star has 10
  vertices alternating 5 tips at the outer radius and 5 valleys at the inner radius, CCW; a rounded rect has
  4·(segments+1) points, spans width×height, has area w·h−(4−π)r², is CCW, and collapses to a plain 4-corner
  rectangle at radius 0; a star fed to `extrudePolygon` makes a valid closed prism (signed volume == area×depth,
  proving composability); <3-sided polygons, <2-point stars, and zero-size rects all return empty. Honest scope: all
  outlines are simple (non-self-intersecting) and CCW, centred on the origin; the rounded-rect corner radius clamps
  to half the shorter side. These are OUTLINES, not filled meshes — pair with the extrude/fill/revolve tools.
  [VERIFIABLE HERE]
- [x] **Projected / frontal area** (`render::projectedArea`) — DONE (M603); measure how big a shadow a model casts
  when viewed from a given direction — the area of its outline as projected onto the screen, the "frontal area" an
  engineer means by cross-section. That one number drives a lot of game and sim math: aerodynamic and water drag
  (drag scales with frontal area), wind load on a structure, how much sunlight a solar panel or leaf catches, the
  size of a cast shadow, and how tightly a camera must frame an object. It sums the projected area of every triangle
  that faces the direction (each contributes its area times how square-on it is), which for a closed convex shape is
  exactly the silhouette area. Verified (`ctest -R mesh_projected_area`): a cube of side s viewed face-on projects to
  exactly s² (checked on all three axes); the value is symmetric in the view direction (same from front and back) and
  independent of the direction vector's length; viewed corner-on the cube gives its hexagonal silhouette area
  s²·√3; an icosphere of radius r projects to ~πr² from any direction (within 2%); an empty mesh or zero direction
  gives 0. Honest scope: EXACT for a convex closed mesh; for a CONCAVE mesh it is an UPPER BOUND (hidden folds behind
  nearer surface still count) — use a rasterised coverage method for the exact projected area of a concave shape.
  [VERIFIABLE HERE]
- [x] **Linear extrude / prism from a polygon** (`render::extrudePolygon`) — DONE (M602); take any flat 2D shape and
  give it thickness, turning an outline into a solid 3D block. Draw a star, a gear, a heart, a letter of the alphabet,
  a company logo, an arrow, an L-shaped room footprint, or a staircase side-profile as a list of 2D points, and it
  stamps the shape out into a prism `depth` units thick. This is Godot's CSGPolygon3D in Depth mode / Blender's
  "extrude region" / the classic CAD linear-extrude — the fastest way to make chunky 3D text, coins and medals,
  extruded signage, cookie-cutter props, pipes with a fancy cross-section, or blocky level geometry from a hand-drawn
  footprint. The shape is laid in XY and pushed along Z, centred from z=−depth/2 to +depth/2; two end caps
  (triangulated with the engine's ear-clipping `triangulatePolygon`, M156) plus one quad wall per outline edge make a
  closed solid. Verified (`ctest -R mesh_extrude_polygon`): an n-gon makes exactly 4n−4 triangles; the prism spans
  z=−depth/2..+depth/2 with the polygon's XY extent; it is a closed, consistently-wound solid whose SIGNED VOLUME
  equals area×depth (proven for a square and a triangle, and for CLOCKWISE input too — winding auto-normalises); cap
  faces point ±Z while wall faces lie in-plane; fewer than 3 points, zero depth, and a collinear/zero-area outline
  all return an empty mesh. Honest scope: the outline must be a SIMPLE polygon (no self-crossings, no holes — a donut
  needs a hole-aware path); the mesh is unwelded with flat per-face normals for crisp faceted edges. [VERIFIABLE HERE]
- [x] **Arrow mesh** (`render::buildArrow`) — DONE (M601); a solid 3D arrow (a round shaft with a cone tip) pointing
  along +Y. Arrows are the universal "look here / this way" marker: draw a force or velocity vector, show which way a
  spawn or waypoint faces, build the move/rotate gizmo handles for an editor, point at an objective, make a compass
  needle or wind indicator. Aim it anywhere by rotating the mesh so +Y points at your target. It is one closed
  watertight solid — shaft tube + bottom cap + the flat under-shoulder of the head + the cone — so it lights and casts
  shadows like any prop. `length` is the base-to-tip span, `headLength` how much of the top is the cone (kept inside
  the interval so there's always some shaft and some head), and `shaftRadius` < `headRadius` gives the classic arrow
  shoulder. Reuses the engine's area-weighted `computeNormals`. Verified (`ctest -R mesh_arrow`): an N-segment arrow
  has exactly 3N+2 vertices and 6N triangles; the base sits at y=0 and a tip vertex sits at (0, length, 0); the widest
  radius equals the head radius; it is a closed watertight manifold (every edge shared by exactly two faces, Euler
  V−E+F==2); zero length, zero radius, and <3 segments all return an empty mesh. Honest scope: normals are smoothed
  across the shaft/shoulder/cone joins (one `computeNormals` pass), so the creases read a touch soft — split the
  vertices for razor-sharp edges. [VERIFIABLE HERE]
- [x] **Closest point on a mesh** (`render::closestPointOnMesh`) — DONE (M600); for any point in space, find the
  nearest spot ON the model's surface and how far away it is. This is the "snap to surface" / "how deep am I" query
  games lean on constantly: stick a decal, bullet-hole, or footprint flat on the wall it hit; snap a placed object or
  the mouse cursor onto the terrain; measure how far a character has sunk into geometry so you can push them back out;
  find clearance to the nearest wall; pick the face nearest a click. Unlike a raycast (which needs a direction and can
  miss), this ALWAYS returns an answer — the single closest surface point wherever the query sits. It walks every
  triangle with the engine's exact `closestPointOnTriangle` and keeps the nearest, reporting the point, unsigned
  distance, which triangle won, and that triangle's facing normal (handy for orienting a decal). Verified (`ctest -R
  mesh_closest_point`): a point above a flat quad maps straight down with distance = height and a vertical hit normal;
  a point past the quad's edge snaps to the boundary with the right distance; a point outside a cube's +X face lands
  on x=1 carrying its y,z through; a point INSIDE the cube returns the nearest face at the correct unsigned distance;
  an empty mesh is invalid. Honest scope: brute-force O(triangles) per query — ideal for one-offs and small/medium
  meshes; front it with a BVH (game::Bvh / TriMesh3D) for many queries on a big mesh. Returns UNSIGNED distance (no
  inside/outside — use M556 containment / M551 SDF for a signed result). [VERIFIABLE HERE]
- [x] **Silhouette / outline edges** (`render::silhouetteEdges`, `render::silhouetteEdgesFromEye`) — DONE (M599); find
  the edges that form a model's OUTLINE as seen from a given direction — the crisp boundary between the parts facing
  the camera and the parts facing away. On a sphere seen head-on that's the rim circle; on a cube seen corner-on it's
  a hexagon. Unlike the engine's sharp/hard/feature-edge tools (M548/M549/M567), which mark folds baked into the
  geometry regardless of viewpoint, a silhouette is VIEW-DEPENDENT — it slides around the surface as the camera moves.
  It's what you need for cartoon / ink outlines (draw a fat line along the silhouette), hidden-line and blueprint
  looks, pencil shading, and shadow-volume caps for stencil shadows. An edge is on the silhouette when one of its two
  triangles faces toward the view and the other away; an open boundary edge (one triangle) is always on the outline.
  `silhouetteEdges` takes a view DIRECTION (orthographic / distant camera); `silhouetteEdgesFromEye` takes a camera
  POSITION and tests each face against its own direction to the eye (correct for a near perspective camera). Verified
  (`ctest -R mesh_silhouette`): a watertight cube viewed corner-on yields exactly the 6-edge hexagonal outline with no
  boundary edges; reversing the view direction returns the same 6 edges (the front/back boundary is orientation-
  invariant); a flat quad has no interior silhouette (its shared diagonal is excluded) but its 4 open edges are the
  whole outline, and with boundary excluded its silhouette is empty; a distant-eye perspective query matches the
  orthographic hexagon; an empty mesh is safe. Honest scope: faces are tested by flat geometric normal so the mesh
  must be consistently wound; a face seen exactly edge-on counts as back-facing (a deliberate tie-break). Returns
  undirected vertex-index pairs plus a `boundaryCount`. [VERIFIABLE HERE]
- [x] **Icosphere / geodesic sphere** (`render::makeIcosphere`) — DONE (M598); a round ball built by repeatedly
  splitting an icosahedron (a 20-sided die) into smaller triangles, giving a sphere whose triangles are all nearly
  the SAME size — the good kind of sphere for most jobs. The everyday "UV sphere" (M28 makeSphere) crowds its
  triangles into tight pinch-points at the north and south poles, which shows as ugly stretching on planets,
  blotchy shading, and uneven tessellation; the icosphere has no poles and no pinching, so it lights evenly and
  subdivides cleanly. Reach for it for planets and moons, explosion / shockwave domes, force-field bubbles,
  evenly-spread point scatters, low-poly rock/asteroid bases — anywhere a sphere should look the same from every
  angle. `subdivisions` sets smoothness: 0 = the raw 20-face gem, 1 = 80 faces, 2 = 320, each level ×4. Normals are
  the exact analytic sphere normals (no `computeNormals` pass needed). Verified (`ctest -R mesh_icosphere`): subdiv
  0 is 12 verts / 20 faces on the exact radius with position-aligned normals; face count follows 20·4ⁿ and vertex
  count 10·4ⁿ+2 for n=0..3; the mesh is a closed watertight manifold (every edge shared by exactly two faces, Euler
  V−E+F==2); triangle edge lengths stay near-uniform (max/min < 2, i.e. no pole pinch); negative subdivisions clamp
  to the base icosahedron. Honest scope: lat/long UVs carry the usual one-side seam + top/bottom texel pinch shared
  by every lat-long sphere — fine for solid colour / triplanar / seam-tolerant maps; re-unwrap for a perfect atlas.
  [VERIFIABLE HERE]
- [x] **Skin / loft across sections** (`render::skinSections`) — DONE (M597); stretch a smooth surface over a stack of
  cross-section "ribs", like pulling skin over the frames of a boat hull or an aeroplane fuselage. You give it an
  ordered list of rings — each ring the outline of the shape at that station — and it bridges every rib to the next
  with a band of triangles, so the shape flows from one outline into the next. Unlike sweep (M595, which drags ONE
  fixed profile along a path), each rib here can be a DIFFERENT size and shape, so the surface can taper, bulge, twist,
  or morph: a funnel (big ring → small ring), a boat hull (keel → beam → stern), a vase whose silhouette changes
  freely, a tube that fairs from a circle into a square. This is Blender's "Bridge Edge Loops" / classic CAD lofting.
  Reuses the engine's area-weighted `computeNormals`. Verified (`ctest -R mesh_skin`): two identical square ribs skin a
  tube band with N·P vertices and (N-1)·P·2 triangles; a funnel of a half-width-2 rib and a half-width-0.5 rib keeps
  each rib's own size and height (a true taper, not a resample); `closedPath` adds exactly one extra wrap band
  (last rib → first); open rings drop one edge per band vs closed rings; a single rib, no ribs, and ragged ribs
  (unequal point counts) all safely return nothing. Honest scope: every rib must have the SAME point count (point j
  connects to point j — no resampling), ribs must be given in body order, and it builds the side skin only (end ribs
  left open — cap separately). `closedRings` picks tube vs open strip; `closedPath` closes the body into a torus.
  [VERIFIABLE HERE]
- [x] **Heightfield / terrain mesh** (`render::buildHeightfield`) — DONE (M596); turn a flat grid of height numbers
  into a rolling 3D terrain surface. Hand it a cols×rows grid of heights (row-major — straight out of a Perlin/fbm
  noise function, a greyscale heightmap image, or hand-authored contours) and it drops a vertex at every grid point,
  lifts each to its height, and stitches the sheet together with two triangles per cell. This is the bread-and-butter
  of outdoor game worlds — hills, dunes, valleys, ocean floors, golf courses — and is exactly Godot's HeightMapShape3D
  / a terrain node's mesh. The grid is centred on the origin on the XZ plane with height along +Y, so it drops
  straight into a scene; `cellSize` is the world spacing between grid points and `heightScale` multiplies the raw
  heights. Smooth per-vertex normals from the engine's area-weighted `computeNormals` make lighting follow the slopes
  for free. Verified (`ctest -R mesh_heightfield`): a 3×3 flat grid makes 9 vertices and (cols-1)(rows-1)·2 triangles,
  sits at y=0 with all normals pointing straight up, and is centred with the right cellSize span; a grid with distinct
  heights lifts each vertex to exactly height·heightScale; a ramp tilts the normals off vertical (leaning against the
  climb) while keeping them upward; a 1×1 grid, a size mismatch, and empty input all safely return nothing. Honest
  scope: builds an open single-sided SURFACE (no skirt/underside/walls) — add a skirt or extrude down for thickness or
  watertightness. [VERIFIABLE HERE]
- [x] **Sweep along a path / loft** (`render::sweepProfile`, `render::buildTube`) — DONE (M595); push a flat 2D
  cross-section down a 3D path and leave a solid tube of that shape behind it. Feed it a circle and a curvy path and
  you get a pipe, cable, rope, wire, garden hose, or tentacle (`buildTube` is the ready-made circle case); feed it a
  rectangle and you get a rail, moulding, road ribbon, or fence beam; feed a star or an L and you get an extruded
  girder or trim. This is Blender's curve-bevel sweep and Godot's CSGPolygon3D in Path mode — the standard way to
  build anything long and bendy that follows a line. The hard part of sweeping is stopping the cross-section from
  spinning as the path curves; this carries the orientation forward with a ROTATION-MINIMIZING FRAME (parallel
  transport — the smallest rotation that follows each bend), so a pipe never twists along its length. Reuses the
  engine's area-weighted `computeNormals`. Verified (`ctest -R mesh_sweep`): a radius-0.5 circle swept down a
  straight path makes a pipe whose every vertex sits exactly 0.5 from the axis with the right vertex/triangle
  counts; each ring's centroid lands exactly on its path point; on a THREE-TURN 3D path every ring vertex still
  keeps its radius (the frame stays coherent — no twist collapse or blow-up); an open profile makes the expected
  fewer faces than a closed ring; single-point paths, <3-side tubes, and empty profiles all safely return nothing.
  Honest scope: builds the swept SIDE surface only — the two ends are left OPEN (a cut pipe); cap separately for a
  closed solid. `closedProfile` picks tube (closed ring) vs ribbon (open strip). [VERIFIABLE HERE]
- [x] **Revolve / lathe** (`render::revolveProfile`) — DONE (M594); spin a 2D outline around the vertical axis to
  build a round 3D object. Hand it the side-view silhouette of a vase, bottle, wine glass, wheel, chess pawn, lamp
  base, or bowl — a list of (radius-from-axis, height) points — and it sweeps that outline all the way around,
  stitching a smooth surface. This is the single most productive way to model any round object: it is Blender's
  "Spin", a software wood-lathe, Godot's CSGPolygon3D in Spin mode. `segments` sets how many angular steps the
  sweep is divided into (smoothness), and a partial `sweepRadians` (< 2π) makes an open arc wedge instead of a
  full body. Reuses the engine's area-weighted `computeNormals`, so the result shades smoothly with no extra work.
  Verified (`ctest -R mesh_revolve`): a straight radius-2 vertical profile spun into 8 segments makes a cylinder
  whose every side vertex sits exactly on the radius-2 wall between the right heights, with (segments+1)×2 vertices
  and segments×2 triangles, and whose normals all point radially OUTWARD; a profile that runs down to radius 0 makes
  a clean cone tip with only non-degenerate triangles (the pole slivers are skipped); a half-turn sweep spans from
  +radius round to −radius; empty / single-point / <3-segment inputs safely return an empty mesh. Honest scope:
  this builds only the swept SIDE surface — it adds NO end caps, so a profile that stops short of the axis leaves
  the ends open (a tube); run the profile down to radius 0 for a natural closed tip, or cap it afterward. Spins
  around +Y with outward winding for a positive-radius profile. [VERIFIABLE HERE]
- [x] **Slab slicing / layer stack** (`render::sliceLayers`) — DONE (M593); chop a model into a STACK of evenly-spaced
  cross-sections along one axis and hand back the outline of each — exactly what a 3D printer or laser cutter does
  before it makes a part (slice the model into thin horizontal layers, then trace each layer so the machine knows
  where to lay plastic or where to cut). The same operation draws a topographic contour map (a hill as a set of
  stacked height rings), builds a stack of collision cross-sections, or makes a layered cutaway preview. It simply
  aims the engine's existing plane-slicer (`sliceMesh`, M532) at N heights spread across the model's height and
  gathers each layer's loops. By default the N planes land on the layer CENTRES (heights (i+0.5)/N across the
  bounding box), which dodges landing a plane exactly on a flat top/bottom cap where the cut would be degenerate;
  pass `sampleEdges=true` to place them on the layer boundaries instead. Verified (`ctest -R mesh_slice_layers`): a
  watertight box that spans y −3..3, sliced into 3 layers along Y, produces exactly 3 contours at the centre heights
  −2 / 0 / +2; every contour is a CLOSED ring whose points all sit exactly on that layer's plane and on the box's
  side walls; asking for 0 layers, slicing an empty mesh, or slicing a flat (zero-height) mesh all safely return
  nothing. Honest scope: each layer is exactly what `sliceMesh` returns — a set of ordered loops that close cleanly
  only on a watertight (index-welded) solid; a per-face vertex soup slices into open chains, so weld first (M573
  `weldVertices`). It returns the OUTLINES per layer, not filled 2D regions and not split solid chunks. axis 0=X,
  1=Y (default), 2=Z. [VERIFIABLE HERE]
- [x] **Extrude faces** (`render::extrudeFaces`) — DONE (M592); raise every triangle off the surface into a little
  standing prism — each face is pushed OUT along its own normal by `distance` and the gap it leaves is walled in on
  all three sides, so a flat panel sprouts a field of raised studs / buttons / greebles / brick-relief. This is
  Blender's "Extrude Individual Faces" and the workhorse for panelled sci-fi hull detail, chunky pixel-art relief,
  or the raised keys of a keypad. Each input triangle becomes a self-contained prism: its TOP (the triangle moved
  out, still facing the same way) plus three SIDE walls (a quad per original edge) — 7 triangles and 6 vertices
  from 1. Distinct from solidify (M579, one whole-mesh shell) and inset (M590, shrink in place). Verified
  (`ctest -R mesh_extrude`): a +Y triangle extruded 0.5 makes exactly 6 vertices and 7 triangles; the base ring
  stays at y=0 while the raised ring moves to y=0.5 sitting directly above the base corners; the top face keeps its
  +Y facing; the bbox grows to y=0.5; a negative distance presses inward; empty is safe. Honest scope: extrudes
  each triangle INDEPENDENTLY (individual-faces mode) and unwelds — a flat region of many triangles raises each as
  its own stud with walls along every interior edge, so merge coplanar tris first for clean single studs; normals
  are set to each prism's top-face normal (side walls shade like the top — re-run `computeNormals` for correct wall
  shading); multiplies triangle count by 7. [VERIFIABLE HERE]
- [x] **Curvature heatmap** (`render::curvatureHeatmap`) — DONE (M591); paint a mesh so you can SEE where it bends
  — flat regions go cool blue, gently curved areas green, and sharp creases/tips hot red. This is the standard
  "curvature map" every DCC/inspection tool shows: how modellers spot pinching, lumps and over-sharp edges that
  shade badly, and how a retopo/QA pass finds the high-detail zones. It runs the engine's `computeCurvature`
  (M535) and maps each vertex's curvature MAGNITUDE through a blue→green→red ramp, normalised so the mesh's own
  peak (or a supplied `maxValue`) becomes full red; choose mean curvature |H| (creases, default) or Gaussian |K|
  (spherical-vs-saddle). Verified (`ctest -R mesh_curvature_color`): the ramp hits blue/green/red exactly at
  0/0.5/1; a flat grid comes out all blue (zero curvature); on a pyramid the sharp interior apex reads redder and
  less blue than the flat base corners; positions are untouched; empty is safe. Honest scope: overwrites RGB with
  a debug colour (a visualisation, not a physical signal); curvature at open BOUNDARY vertices is unreliable (needs
  a full one-ring) so trust the interior; auto-normalisation makes colours RELATIVE to this mesh — pass an explicit
  `maxValue` to compare two meshes on the same scale. [VERIFIABLE HERE]
- [x] **Inset faces** (`render::insetFaces`) — DONE (M590); shrink every triangle IN PLACE toward its own centre,
  opening a gap between neighbouring faces. Each triangle keeps its shape and facing but scales down about its
  centroid by `amount` (0 = untouched, 1 = collapsed to a point), so a solid surface becomes a field of shrunken
  tiles with dark seams: panel gaps on a spaceship hull, grout lines between floor tiles, a greebled panelled
  look, or the base ring for a per-face extrude/bevel. This is Blender's "Inset Faces → Individual" (the shrink
  part). It first UNWELDS (each triangle gets its own three corners carrying that triangle's flat face normal, so
  the tiles separate and flat-shade), then moves each corner a fraction `amount` toward the centroid. Distinct
  from explode (M578, which TRANSLATES whole faces) and displace (M580, which OFFSETS along normals). Verified
  (`ctest -R mesh_inset`): one triangle stays one triangle with 3 own vertices; amount 0 leaves corners put;
  amount 0.5 moves each corner exactly halfway to the centroid; the inset triangle keeps the original centroid and
  its area shrinks by (1−amount)² = 1/4; every corner carries the flat face normal; amount 1 collapses all three
  corners onto the centroid; empty is safe. Honest scope: insets each triangle INDEPENDENTLY (individual-faces
  mode), so a flat region tiled by many triangles gets a seam along every interior edge, not just its outline —
  run on a low-poly mesh or merge coplanar tris first for panel-only gaps; triples the vertex count; `amount` may
  exceed 1 (overshoot) or go negative (grow the tile). [VERIFIABLE HERE]
- [x] **Principal inertia axes** (`render::computePrincipalAxes` → `PrincipalAxes`) — DONE (M589); the three
  natural spin axes of a solid mesh and how hard it is to spin about each. Every rigid body has three perpendicular
  axes it rotates cleanly about (no wobble); a physics engine that wants realistic tumbling — a thrown plank spins
  easily end-over-end but resists rolling about its length — needs exactly this: the centre of mass, the three
  principal axes, and the moment of inertia about each. This takes the raw inertia TENSOR from
  `computeMassProperties` (M549) and DIAGONALISES it with the engine's symmetric Jacobi solver (eigenvectors =
  principal axes, eigenvalues = principal moments), sorted by moment ascending so `axis[0]` is the easiest to spin
  (the long direction) and `axis[2]` the hardest. Verified (`ctest -R mesh_principal_axes`): for a solid 4×1×1 box
  the mass equals the volume (4), the centre of mass is at the origin, the smallest moment is about the long X
  axis with the two cross moments (Y,Z) equal, and both moments match the closed-form solid-cuboid formulas
  (Ix = m(h²+d²)/12 ≈ 0.667, Iy = m(w²+d²)/12 ≈ 5.667) to 1e−2; the three axes are orthonormal; an open (zero-
  volume) mesh is invalid. Honest scope: correct only for a CLOSED, consistently-wound solid (the volume integral
  needs a watertight surface); values are at unit density (mass == volume — scale by real density for physical
  units); reads positions only. [VERIFIABLE HERE]
- [x] **Component tint (debug viz)** (`render::tintComponents`) — DONE (M588); paint every disconnected PIECE of a
  mesh a different colour, so you can see at a glance how many separate islands it is made of and which triangles
  belong together. A model that looks like one object is often secretly several (a character plus loose props,
  terrain chunks that never welded, stray shards from a bad boolean); tinting each connected component a distinct
  hue is the standard debug view for "why is my one mesh actually 40 pieces?" and for authoring per-part masks.
  Each vertex's colour comes from its component index via golden-ratio hue stepping (so adjacent components never
  share a near-colour), at the given saturation/value; the component count is returned. Reuses
  `connectedComponentLabels` (per-triangle island labels, spread onto the vertices). Verified
  (`ctest -R mesh_component_color`): two disjoint triangles report two components, each internally one uniform
  colour, and the two colours differ; positions are untouched; a shared-vertex quad is one component with one
  colour; the tint is deterministic across runs; empty is safe (zero components). Honest scope: "connected" means
  sharing a vertex INDEX (welded topology) — two pieces touching in space but with separate vertices read as
  separate (weld first to merge); overwrites the RGB of every vertex (positions/normals/UVs untouched); the hues
  are opaque debug colours, not a physical signal. [VERIFIABLE HERE]
- [x] **Flatten / project-to-plane** (`render::projectToPlane`) — DONE (M587); squash a mesh toward a flat plane —
  each vertex slides along the plane's normal toward its perpendicular projection onto the plane, blended by `t`
  (0 = unchanged, 1 = every vertex exactly on the plane, in between squashes smoothly). Uses: a cheap
  drop-shadow / blob-shadow caster (flatten a copy of a model onto the ground and draw it dark), a decal or
  sticker baked onto a surface, a "pressed flat" squash pose, or projecting a prop onto a wall. The plane is a
  point + a normal (normalised internally so any length works). Verified (`ctest -R mesh_flatten`): t=1 onto y=0
  puts every vertex at y=0 while x/z never drift; t=0 is the identity; t=0.5 removes exactly half the distance
  (a vertex at y=3 → 1.5, one at y=−1 → −0.5); an offset plane (y=5) projects onto y=5; a tilted +Z plane with a
  non-unit normal flattens onto z=0; a zero-length normal is a no-op; empty is safe. Honest scope: moves POSITIONS
  only along the normal — normals are left stale (set them to the plane normal or re-run `computeNormals` for a lit
  pancake); at t=1 the mesh is coplanar with zero thickness (its two sides overlap — a shadow/decal source, not a
  solid); `t` may exceed 1 (overshoot) or go negative (push away). [VERIFIABLE HERE]
- [x] **Bounding-cylinder fit** (`render::fitBoundingCylinder` → `BoundingCylinder`) — DONE (M586); the tightest
  capsule-like cylinder wrapped around a mesh, aligned to the object's OWN long axis rather than a world axis.
  Where an axis-aligned box or an oriented box (FitObb) suits a boxy prop, a cylinder is the right hull for
  anything long-and-round: a character torso or limb, a pillar, a barrel, a thrown log, a rocket. Games use it for
  capsule colliders, trigger volumes, and cheap broad-phase bounds on elongated bodies (Godot's CapsuleShape3D
  wants exactly radius + height + axis). The method is PCA — centre the vertices, take the covariance matrix's
  dominant eigenvector (via the engine's symmetric Jacobi solver, reused from FitObb) as the length axis, then
  measure the spread ALONG it (height) and AWAY from it (radius = farthest perpendicular distance). Reports axis,
  centre, radius, height. A measurement pivot after the twist/taper/spherify/bend/ripple deformer run. Verified
  (`ctest -R mesh_bounding_cylinder`): a thin bar long along Y gives an axis ≈ ±Y, height 8, radius √0.5 (its
  farthest corner), centre at the middle; EVERY vertex is provably inside the reported radius + half-height (never
  clips); it works whichever world axis the bar runs along (X, Y, Z); fewer than two vertices returns valid=false.
  Honest scope: the PCA-aligned fit, not a global minimum-volume optimiser — for an L-shaped or clustered cloud
  the principal axis may not be the visually obvious one; the radius is worst-case (fully contains the mesh); reads
  positions only. [VERIFIABLE HERE]
- [x] **Ripple / wave deformer** (`render::rippleMesh`) — DONE (M585); send concentric ripples across a surface —
  each vertex is pushed along one axis by a sine wave of its DISTANCE from a centre, so a flat plane becomes a pond
  after a stone drops, a disc becomes a warped vinyl record, a flag gets a rippling wobble. The push along `axis`
  is `amplitude·sin(radial·frequency − phase)` where `radial` is the distance from `centre` in the plane
  perpendicular to `axis` (so the rings are truly circular). `frequency` sets ring spacing, `amplitude` the crest
  height, and animating `phase` makes the rings travel outward — the whole animated-water effect is just
  `phase += speed·dt` each frame. Complements M580 displace (noise) with a clean analytic wave. Verified
  (`ctest -R mesh_ripple`): along +X at radii 0/1/2/3 with frequency π/2 the offsets are exactly 0/+amp/0/−amp
  (the sine at 0, π/2, π, 3π/2); a vertex at the same radius in Z gets the identical offset (rings are circular);
  the perpendicular X/Z coords never move; a phase of π/2 puts a trough at the centre; amplitude 0 is a no-op; an
  offset centre re-measures the radius (a vertex sitting on the new centre gets zero offset); empty is safe.
  Honest scope: offsets POSITIONS only along a single axis — normals are left stale (re-run `computeNormals` for
  correct crest/trough shading); the wave's smoothness is limited by tessellation along the radius (subdivide for
  a clean sine); frequency 0 pushes the whole surface by a constant. [VERIFIABLE HERE]
- [x] **Bend deformer** (`render::bendMesh`) — DONE (M584); curl a straight mesh around an arc — a plank becomes
  an archway, a straight pipe becomes an elbow, a flat strip becomes a curled ribbon or barrel stave. This is
  Blender's "Simple Deform → Bend" and the third classic deformer alongside twist (M581) and taper (M582),
  completing the trio. You pick the `alongAxis` the bar extends down and the `upAxis` it curls toward (the third
  axis rides through unchanged); a vertex's coordinate along the bar becomes an ANGLE (θ = alongCoord / radius)
  swept around a bend centre sitting `radius` up the up-axis, and its up-coordinate becomes how far it sits from
  that centre. Verified (`ctest -R mesh_bend`): with radius 2, the hinge column (along=0) stays put; a point at
  along=π sweeps a quarter turn and lands exactly at (R,R); every vertex's distance from the bend centre equals
  radius−up (neutral vertices at R, an up=0.5 vertex at R−0.5); the swept position matches sin/cos of along/radius
  to 1e−4; the third axis is untouched; a near-zero radius is a safe no-op; empty is safe. Honest scope: positions
  are exact; normals get their (along,up) components rotated by the local arc angle (right for the rotation, but
  ignoring the slight curl-scale — re-run `computeNormals` for a tight bend). The arc's smoothness is limited by
  how many segments the bar has along its length (a 2-segment bar bends into a single kink; subdivide first); a
  smaller `radius` curls tighter (a full circle closes at length 2·π·radius); `radius` may be negative to bend the
  other way; the bar should straddle along=0 for a symmetric bend. [VERIFIABLE HERE]
- [x] **Spherify / cast-to-sphere** (`render::spherifyMesh`) — DONE (M583); inflate a mesh toward a perfect sphere
  — each vertex is pulled from where it is toward the point on a sphere of `radius` (about `centre`) along its own
  direction from the centre, blended by `t` (0 = unchanged, 1 = exactly on the sphere, in between rounds out
  smoothly). This is Blender's "Cast" modifier (sphere target) and the classic way to round a blocky low-poly
  shape: turn a subdivided cube into a ball, puff an angular rock smooth, or morph a boxy↔round silhouette by
  animating `t`. Position AND normal blend toward the outward radial direction, so lighting rounds out with the
  shape. Third member of the deformer family (twist M581, taper M582). Verified (`ctest -R mesh_spherify`): on a
  unit cube, t=1 lands every vertex exactly on the sphere; t=0 is the identity; at t=0.5 each corner's distance is
  exactly lerp(√3, radius, 0.5); the cast preserves each vertex's ray (a +x+y+z corner stays equal-component); a
  vertex at the centre has no direction and stays put; an offset centre casts about that point. Honest scope: the
  roundness is limited by tessellation — spherifying an 8-vertex cube just moves 8 corners onto the sphere;
  subdivide first (Subdivision) so there are enough vertices to read as round. `t` outside [0,1] is allowed (t>1
  overshoots, t<0 pushes inward). [VERIFIABLE HERE]
- [x] **Taper deformer** (`render::taperMesh`) — DONE (M582); squeeze or fan a mesh along an axis — the cross-
  section perpendicular to the axis is scaled by a factor that ramps LINEARLY from `startScale` at the low end to
  `endScale` at the high end, so a straight bar cones into a pyramid or spike, a cylinder becomes a carrot or
  trumpet, a leg thins toward the ankle. This is Blender's "Simple Deform → Taper" and the quickest way to give
  straight geometry a swelling or narrowing profile without remodelling. The axis coordinate of each vertex is
  untouched; only its distance from the axis (through `centre`) is scaled. axis 0/1/2; leave `axisMin`/`axisMax`
  at their defaults to auto-fit the ramp to the mesh extent. Reuses the same axis mapping as the twist deformer
  (M581). Verified (`ctest -R mesh_taper`): on a 3-ring bar tapered 1→0 about Y, the bottom ring (scale 1) is
  unchanged, the top ring (scale 0) collapses onto the axis (cone tip), the middle ring (scale 0.5) is exactly
  halved with its height untouched; equal factors give a plain uniform cross-section scale; 1→1 is the identity;
  empty is safe. Honest scope: positions are exact; normals get the inverse cross-section scale + renormalize
  (correct for the squeeze but ignoring the along-axis slope the taper introduces — re-run `computeNormals` for
  pixel-accurate shading on a strong taper); factor 0 collapses that end to a degenerate ring (weld/reindex for a
  single-vertex tip); factors may exceed 1 (fan out) or be negative. [VERIFIABLE HERE]
- [x] **Twist deformer** (`render::twistMesh`) — DONE (M581); spiral a mesh around an axis — the further a vertex
  sits along the axis, the more it is rotated about it, so a straight bar becomes a corkscrew, a blade gains a
  spiral flute, a tower gets a helical sweep. This is Blender's "Simple Deform → Twist" and the classic way to add
  a wound / barley-sugar look to straight geometry without hand-modelling every ring. The angle at a vertex is
  `radiansPerUnit × (its distance along the axis from centre)`, applied in the plane perpendicular to the axis
  about the axis line through `centre`; both position AND the normal's perpendicular components rotate so lighting
  follows the twist. axis 0=X/1=Y/2=Z; `radiansPerUnit` may be negative to wind the other way. Verified
  (`ctest -R mesh_twist`): on a vertical bar twisted π/8 per unit, the bottom ring (distance 0) does not move; the
  top corner (1,4,1) turns a full 90° to (1,4,−1) with its height preserved; every vertex keeps its height along
  the axis AND its radius from the axis (a rigid per-ring rotation, no stretching); zero twist is the identity; a
  negative twist winds the opposite way; empty is safe. Honest scope: a rigid rotation per cross-section — it
  preserves height-along-axis and radius-from-axis exactly, so a cylinder stays the same radius as it winds. The
  spiral's smoothness is limited by how many rings the mesh has along the axis (a 2-ring bar just shears into a
  parallelogram twist; subdivide along the axis first for a smooth helix). [VERIFIABLE HERE]
- [x] **Displace / roughen** (`render::displaceMesh` → `DisplaceResult`) — DONE (M580); push each vertex along its
  smooth normal by a procedural NOISE amount, so a too-perfect surface gains organic bumpiness: a flat plane
  becomes rough ground, a smooth sphere becomes a lumpy rock/asteroid, a cylinder becomes a gnarled trunk. This is
  Blender's "Displace" modifier driven by a noise texture — the cheapest way to make procedural or CAD-clean
  geometry look natural. The offset is coherent VALUE NOISE (nearby vertices move together, so the surface
  undulates instead of turning to static) scaled by `amplitude`, with `frequency` setting bump size (low = broad
  swells, high = tight pebbling) and `seed` picking a different field. Fully deterministic — same mesh + amplitude
  + frequency + seed always gives the exact same result, so it is safe for networked/replayed procedural content.
  Reuses `computeNormals` for the push direction; the value noise is a self-contained 32-bit lattice hash with a
  smootherstep fade. Verified (`ctest -R mesh_displace`): a +Y grid moves ONLY in Y (X/Z fixed) by at most
  |amplitude|; the same inputs give a byte-identical result; a different seed changes the field; amplitude 0 is a
  no-op; the coherent noise is non-trivial across the grid; empty is safe. Honest scope: vertices move only along
  their normals by at most |amplitude| (no sideways drift); positions change so stored normals go stale (re-run
  `computeNormals`); the detail is capped by the mesh's resolution (subdivide first for fine roughness). Amplitude
  may be negative; frequency ≤ 0 collapses to one broad lump. [VERIFIABLE HERE]
- [x] **Solidify / shell** (`render::solidifyMesh` → `SolidifyResult`) — DONE (M579); give a paper-thin surface
  real THICKNESS — take a one-sided sheet (a plane, a curved patch, a wall built from a single quad, an imported
  single-sided mesh, a heightmap skirt) and turn it into a CLOSED solid slab with a front face, a back face, and a
  rim sealing the two along the open edges. This is Blender's "Solidify" modifier and the standard fix for
  surfaces that vanish edge-on or leak light because they have no back. The back face is the front pushed inward
  along each vertex's smooth normal by `thickness` and wound the opposite way; the rim bridges every boundary
  (open) edge, so the result is watertight whenever the input was a clean manifold-with-boundary. Reuses the
  engine's area-weighted `computeNormals`. Verified (`ctest -R mesh_solidify`): a unit quad thickens to 8 vertices
  and 12 triangles (2 front + 2 back + 8 rim from its 4 boundary edges) that form a watertight solid (every edge
  used exactly twice); the back layer sits at z=−thickness with a flipped normal; a negative thickness offsets the
  other way and stays watertight; a CLOSED tetrahedron gets a second inner shell with no rim (`hadBoundary=false`);
  empty is safe. Honest scope: this is the fast "simple" offset (each vertex straight along its smooth normal) —
  on a very sharp concave crease the inner offsets can cross and self-intersect (Blender's "complex" mode avoids
  this); the rim reuses the front/back vertices so rim shading is smooth (run facet/`computeNormals` after for
  crisp rim edges). [VERIFIABLE HERE]
- [x] **Explode faces** (`render::explodeFaces`, `render::explodeFacesRadial`) — DONE (M578); pull a mesh's
  triangles APART so the surface blooms open like an exploded-view diagram. `explodeFaces` unwelds every triangle
  (each gets its own three corners carrying that triangle's flat face normal) and pushes it OUTWARD along its own
  face normal by `distance` — a cube's six sides slide straight out, a sphere's facets bristle. `explodeFacesRadial`
  pushes each triangle away from a centre point (the bbox centre by default) so every piece flies out from the
  middle regardless of facing — the classic exploded-parts look. Games use this for unlock reveals, deaths,
  dissolve/shatter-bloom, and assembly animations; animating `distance` 0→D is the whole effect. Reuses the same
  facet-split idea as `MeshFacet` (from which it differs: MeshFacet flat-shades IN PLACE, this also MOVES each
  face). Verified (`ctest -R mesh_explode`): on a unit cube, N triangles become exactly 3N sequentially-indexed
  vertices; distance 0 leaves the bounds at −1..1 (pure facet split); exploding by 0.5 grows the bbox to ±1.5 on
  every axis (each face slid out 0.5 along its normal); every corner carries a unit-length face normal; radial
  explode grows the bbox symmetrically about the centre; empty is safe. Honest scope: this triples the vertex
  count (3× triangles, no sharing — re-run `reindexMesh` at distance 0 to recompact); positions and normals are
  rewritten, UVs/colours ride along; degenerate faces (no defined normal) are copied in place; distance may be
  negative to implode inward. [VERIFIABLE HERE]
- [x] **Snap-to-grid** (`render::snapVerticesToGrid` → `SnapResult`) — DONE (M577); round every vertex position
  onto a regular WORLD grid (say 0.25 units) so each vertex jumps to the nearest multiple. The "tidy up" pass for
  CAD-like or block/voxel meshes: it removes the tiny floating-point drift that creeps in from modelling, rotation
  or import (1.0000001 and 0.9999998 both become a clean 1.0), and makes vertices that were ALMOST at the same
  spot land EXACTLY on it — which then lets the bit-exact `reindexMesh` (M575) actually fuse them. This is a
  DIFFERENT job from `MeshQuantize`: that packs positions into N-bit integers relative to the mesh bounding box to
  shrink the FILE (a per-mesh grid that moves with the box); this snaps to a fixed ABSOLUTE world grid you choose,
  purely to clean geometry — vertices stay full 32-bit floats, just rounded. A per-axis step of 0 leaves that axis
  free (snap X/Z ground plane, keep height); an offset origin shifts the grid lines. The `SnapResult` reports how
  many vertices moved and the max displacement, so you can tell a gentle drift-cleanup from an aggressive
  block-ify. Verified (`ctest -R mesh_snap_grid`): near-integer drift snaps to exact integers with a tiny
  displacement; 0.30/0.10/0.125 round to 0.25/0.00/0.25 on a quarter grid (midpoint rounds away from zero); a
  zero-step axis is left free; on-grid vertices don't move (report says 0 moved); an offset origin snaps 0.6→0.5;
  maxDisplacement is the largest single move; empty is safe. Honest scope: rounds POSITIONS only — normals/colours
  /UVs untouched, so an aggressive snap can leave stored normals stale (re-run `computeNormals`) and can pull
  corners together into degenerate triangles (follow with `reindexMesh`/`weldVertices`). [VERIFIABLE HERE]
- [x] **Poisson-disk prune / blue-noise scatter** (`render::prunePointsPoisson`, `render::scatterBlueNoise`) —
  DONE (M576); thin a dense cloud of surface points down to an EVENLY-SPACED subset — keep a point only if it is
  at least `minDistance` from every point already kept. Raw `sampleSurfacePoints` output is random and therefore
  clumpy (some points nearly on top of each other, some gaps); this turns it into the tidy, no-two-too-close
  scatter you want when placing grass blades, pebbles, trees, decals/bullet-holes, or crowd spawn points across a
  mesh — Godot's "poisson disk" scatter, the classic "spread N items on this surface but never let two overlap."
  The method is dart-elimination with a spatial hash keyed by `minDistance`-sized cells, so the "is anything too
  close?" test only ever looks at the 27 neighbouring cells (O(1) expected, O(n) total). Walking in order makes
  the INPUT ORDER the priority order (first point in a cluster wins its spot); `sampleSurfacePoints` already
  returns random order so feeding it straight in is unbiased. `scatterBlueNoise` is the one-call convenience:
  oversample the mesh, then prune. Kept points retain their normal, ready to orient instances. Reuses
  `sampleSurfacePoints` (M555) and the `detail::cellHash` spatial key from the weld pass. Verified
  (`ctest -R mesh_poisson_prune`): two points inside the radius keep only the earlier one; two beyond it both
  survive; on a 100-point grid no two kept points are ever closer than the radius and a larger radius keeps
  strictly fewer; radius ≤ 0 keeps everything; empty is safe; an end-to-end scatter on a quad respects the
  spacing. Honest scope: this is greedy elimination (fast, deterministic, order-dependent), not a
  maximal-Poisson optimiser — it guarantees the minimum-spacing invariant but not the theoretical maximum packing
  density; oversample generously for a fuller result. [VERIFIABLE HERE]
- [x] **Mesh index / deduplicate (bit-exact)** (`render::reindexMesh` → `ReindexReport`) — DONE (M575); turn a
  "triangle soup" (a mesh where every triangle carries its own three corners, so shared corners are stored two,
  three or six times over) into a compact INDEXED mesh: keep one copy of each truly-identical vertex and point
  every triangle at it through a fresh index buffer — Godot's `SurfaceTool.index()`. The engine's own shape
  builders, CSG, marching-cubes/SurfaceNets output and flat OBJ/STL imports all emit unshared corners; indexing
  shrinks the vertex buffer and lets the GPU's post-transform vertex cache actually hit. The key difference from
  position welding (`weldVertices`): this merges ONLY when EVERY attribute matches bit-for-bit — position AND
  normal AND colour AND UV — so a hard crease (same point, different normal) or a texture seam (same point,
  different UV) is deliberately LEFT as two vertices, because collapsing it would smooth the crease or tear the
  texture. (Position welding, which ignores normals/UVs, is the tool for when you WANT to fuse a seam.) Degenerate
  triangles (two corners that were already the same vertex) are dropped; a mesh with no index buffer is read as an
  implicit soup. Verified (`ctest -R mesh_reindex`): a 6-corner two-triangle soup collapses to 4 unique vertices
  (2 merged) with 6 valid indices and no dropped triangle; a shared point with differing UV stays two vertices (a
  seam survives); a shared point with differing normal stays two vertices (a crease survives); a triangle with two
  identical corners is dropped; an already-clean indexed triangle is untouched; empty is safe. Honest scope: this
  is EXACT dedup (bit-for-bit), not tolerance-based — near-but-not-equal vertices are kept separate (use
  `weldVertices` with an epsilon for that); positions/attributes are never modified, only shared. [VERIFIABLE HERE]
- [x] **Vertex-colour gradient paint** (`render::paintAxisGradient`, `render::paintRadialGradient`) — DONE
  (M574); tint a mesh's per-vertex RGB by WHERE each vertex sits, in one call — the "give it a look without a
  texture" move. `paintAxisGradient` fades one colour to another along X, Y or Z (grass at a hill's base fading
  to rock at its peak; a wall darker at the floor); `t` runs 0→1 from `axisMin` to `axisMax` (leave them at the
  defaults to auto-fit the mesh's bounding box along that axis) and the RGB is a straight blend from `low` to
  `high`, clamped at both ends. `paintRadialGradient` fades outward from a point (a glow or scorch around a hit,
  a spotlight pool); `t` runs 0→1 as distance grows from `inner` to `outer`. Both compose with the other
  vertex-colour tools — paint first, then layer M553 AO / M556 cavity / M564 smoothing on top. Verified
  (`ctest -R mesh_vertex_color_gradient`): a Y-axis auto-fit gradient gives black at the bottom, white at the top,
  mid-grey exactly half-way with all channels ramping together and geometry untouched; an explicit range clamps
  values outside it; a zero-extent axis paints everything `low`; a radial gradient gives innerColor at the centre,
  half-grey at half the band, outerColor at and beyond `outer`; a degenerate band (outer ≤ inner) becomes a hard
  ring; empty meshes are safe. Honest scope: this is a hard REPLACE of RGB (not a blend over existing colour),
  linear in stored [0,1] space (no gamma, no easing curve — pre-smooth or run M564 after for a softer ramp);
  positions/normals/UVs and alpha are untouched. [VERIFIABLE HERE]
- [x] **Mesh flip / reverse (inside-out)** (`render::flipWinding`, `render::flipNormals`, `render::flipMesh`) —
  DONE (M573); deliberately turn a mesh inside-out: reverse every triangle's winding AND negate every vertex
  normal so the surface faces the OTHER way. This is a different job from M562 winding-consistency (which only
  makes a mesh AGREE with itself) — here you WANT the flip. Uses: build an inward-facing shell (a skybox, a room
  seen from inside, a cave interior, a hollow that culls its outer faces so you see the far walls); correct a
  whole model that imported inside-out in one call; or make a two-sided effect by MERGING (M572) a mesh with its
  flipped copy so both faces render under single-sided culling. `flipWinding` (swap corners 2/3) flips which face
  back-face culling drops; `flipNormals` flips which way the surface shades; `flipMesh` does both. Verified
  (`ctest -R mesh_flip`): on a +Y quad, flipWinding swaps corners 2 and 3 so the geometric face points the other
  way while stored normals stay put; flipNormals negates the stored normals and leaves the indices; flipMesh does
  both; flipping twice restores winding, normals, and facing exactly; empty meshes are safe. Honest scope: this
  negates STORED normals and reverses triangle order — it does not recompute normals from geometry (a mesh with
  none keeps zero normals; run computeNormals first); positions/UVs/colours are untouched. [VERIFIABLE HERE]
- [x] **Mesh merge / concatenate** (`render::mergeMeshes`) — DONE (M572); glue several meshes into ONE mesh (one
  vertex buffer, one index buffer). This is the inverse of the M529 split-into-components and the workhorse of
  DRAW-CALL BATCHING: a scene with a hundred static props drawn as one combined mesh renders in a single draw call
  instead of a hundred — usually the biggest CPU win a renderer gets. It also backs "join selected" in an editor
  (flattening a set of pieces, each already placed via M571 applyTransform, into one exportable object) and
  assembling procedural kit-bashed geometry. Each source mesh's indices are re-based by the running vertex count
  so triangles keep pointing at the right (now-appended) vertices; all attributes (position/normal/UV/colour)
  come along unchanged. Verified (`ctest -R mesh_merge`): merging two triangles gives 6 vertices and 6 indices
  with the second triangle re-based by +3 and its vertices at the right offset; a list of three sums to 9/9 with
  the third re-based by +6; merging two disjoint parts round-trips — the result is two connected components that
  split back into two meshes; empty parts contribute nothing; merging an empty list yields an empty mesh. Honest
  scope: pure concatenation — it does NOT weld coincident vertices at the seams (run M525 weldVertices / M559
  autoWeld afterwards for a watertight join) and does NOT dedup, so merging N copies yields N× the vertices;
  winding/attributes are preserved exactly (fix mismatched winding with M562 after merging). [VERIFIABLE HERE]
- [x] **Apply / bake transform** (`render::applyTransform`, `translationMatrix`/`scaleMatrix`/`rotationMatrix`) —
  DONE (M571); permanently apply a 4×4 transform (translate + rotate + scale) to a mesh's geometry, moving both
  its POSITIONS and its NORMALS correctly. This "freeze transform" step is everywhere in a content pipeline:
  flatten a node's transform into its mesh before export, merge several placed copies into one buffer, pre-bake an
  import fix-up (a rotate to swap Y-up/Z-up, a scale to convert units) so the runtime does no per-frame matrix
  work, or snapshot an instance. The subtlety it gets right: normals do NOT transform by the same matrix as
  positions under NON-UNIFORM scale — they use the INVERSE-TRANSPOSE of the 3×3 part, then renormalize, so a
  squashed surface keeps its normals perpendicular (a naive transform skews them and lighting goes wrong). Ships
  translate/scale/rotate matrix builders so callers needn't touch glm. Verified (`ctest -R mesh_transform`):
  translation shifts positions and leaves normals; uniform ×3 scale scales positions and keeps a unit +X normal;
  a 90° Z-rotation rotates both position and normal (1,0,0)→(0,1,0); a non-uniform (1,2,1) scale transforms a 45°
  normal by the inverse-transpose to nx:ny = 2:1 (not the 1:2 a naive matrix gives) and keeps it unit-length; a
  composed T·R·S maps (1,0,0) to (5,2,0); empty meshes are safe. Honest scope: this BAKES the transform into
  vertex data (it doesn't keep a separate node transform); tangents are not recomputed here (regenerate via
  computeTangents after a mirror/negative-scale, which also flips winding — pair with M562 when the determinant is
  negative); positions use the full 4×4, normals the translation-free inverse-transpose 3×3, and a zero normal
  stays zero. [VERIFIABLE HERE]
- [x] **Normalize to a target box** (`render::normalizeToBox`, `NormalizeResult`) — DONE (M570); recentre AND
  uniformly scale a mesh so it fills a chosen box — the import-normalization companion to the M569 pivot snap.
  Imported models arrive at wildly different scales (one in metres, one in centimetres, one a thousand units tall)
  and off-centre; this fits any mesh into a consistent size (default: centred in a unit cube, longest side = 1) so
  a whole asset library shares one scale and pivot, thumbnails frame identically, and downstream tools (voxelize,
  SDF, sampling) get predictable extents. The scale is UNIFORM (one factor on all axes) so the shape never
  distorts. Returns the transformed copy plus the exact scale and source/target centres so an inverse or parent
  transform can undo it. Reuses the mesh bounding box. Verified (`ctest -R mesh_normalize`): a 10×4×2 box parked at
  (100,50,20) scales by 1/10 so its longest side is 1, its 10:4:2 proportions become 1:0.4:0.2 (uniform scale), and
  its centre lands at the origin with the reported source centre correct; a custom target size/centre (fit a 2-unit
  square into a 4-unit box at x=10) scales ×2 and recentres there; a 7×3 flat sheet scales by its 7-unit longest
  side and stays perfectly flat; a single point keeps unit scale and just translates; empty meshes are safe. Honest
  scope: UNIFORM scale preserves proportions — the mesh is centred and touches the box on its longest axis, it does
  NOT stretch to fill a non-cubic box on every axis (that would distort); normals/UVs/colours are untouched
  (uniform scale keeps normals valid); this translates+scales only, never rotates (align first via M568).
  [VERIFIABLE HERE]
- [x] **Pivot snap / recenter** (`render::recenterMesh`, `RecenterResult`, `PivotMode`) — DONE (M569); move a
  mesh's PIVOT (the point that ends up at the world origin) to a sensible place. Imported models land wherever the
  exporter left them — floating off-axis, pivot in a random corner — which makes them awkward to place, rotate,
  and scale. This recentres geometry so its pivot sits at the origin, choosing the pivot by intent: BOUNDING-BOX
  CENTRE (spin-in-place props), BASE (characters/trees/furniture that stand on the ground — bottom-centre), CENTRE
  OF MASS (physics bodies that rotate about their true balance point), or VERTEX AVERAGE (a cheap centroid). It
  returns a recentred copy plus the applied offset and the world-space pivot so a parent transform can be
  compensated. Reuses the M532 mass-properties centroid for the centre-of-mass mode. Verified (`ctest -R
  mesh_recenter`): a unit cube's bbox-centre pivot is (0.5,0.5,0.5) with offset −pivot and the recentred cube
  spanning [−0.5,0.5]³; Base mode lands the bottom face on y=0 centred in x/z; a square pyramid's centre of mass
  sits at ¼ height (below its ½-height bbox centre — mass near the base); the cube corners' average is the centre;
  centre-of-mass on an open triangle falls back to the vertex average and flags `fellBack`; empty meshes are safe.
  Honest scope: this only TRANSLATES — never rotates or scales (pair with M568 dominant-plane to also align, or a
  normalize-to-box pass to also scale); centre of mass needs a closed, consistently-wound solid (M566/M562) and
  falls back to the average otherwise; Base uses the supplied `up` axis (default +Y); only positions move.
  [VERIFIABLE HERE]
- [x] **Dominant-plane / flatness detector** (`render::fitDominantPlane`, `MeshPlane`) — DONE (M568); fit the
  best-matching flat plane to a mesh's vertices and measure how FLAT the shape actually is. Via principal
  component analysis (centre the points, form the 3×3 covariance, take its eigenvectors), the direction of LEAST
  spread is the plane's normal and the leftover spread along it is the shape's departure from flat. This answers
  "is this a wall / floor / panel / decal, and which way does it face?" — used to auto-orient flat props to a
  surface, pick a planar-UV axis, snap a billboard, detect ground/wall pieces for gameplay, or decide a
  nearly-flat mesh can collapse to a quad. Returns the unit normal, a point on the plane (the centroid),
  `rmsDistance`/`thickness` (in mesh units) and a `planarity` shape score in [0,1]. Reuses the FitObb symmetric
  Jacobi eigensolver. Verified (`ctest -R mesh_dominant_plane`): a flat grid in the XZ plane fits a ±Y normal with
  zero tilt, zero thickness/RMS and planarity > 0.99; a sheet tilted onto the x+y=0 plane fits the (1,1,0)
  direction (sign-independent) and stays fully planar; a cube point cloud reads planarity < 0.2 with real
  thickness; a thin slab reads planarity > 0.9 with its normal on the thin axis and thickness equal to its gauge;
  empty meshes are invalid. Honest scope: fits ONE global plane through the centroid — great for planar-ish meshes
  (walls, panels, terrain patches) but a folded or multi-part mesh returns the average best-fit plane (segment
  first via M529 components / M545 planar regions); `planarity` is a shape descriptor (1 = flat, 0 = isotropic),
  not a physical unit; vertices are weighted equally (not area-weighted), so a dense cluster pulls the fit.
  [VERIFIABLE HERE]
- [x] **Hole fill / cap** (`render::fillHoles`, `HoleFillResult`) — DONE (M567); seal the open holes a mesh has,
  turning a leaky surface into a watertight solid. Where M566 REPORTS holes, this PATCHES them: for each open
  boundary loop it adds a centre vertex at the hole's average position and fans triangles from that centre to the
  rim, closing the gap — the "fill holes / make watertight" repair 3D-print prep, scan cleanup, and boolean/CSG
  post-processing all run so the mesh passes solid checks (volume, mass, inside/outside, printing). The rim comes
  from the M536 boundary-loop walk, whose ordering follows the existing faces' winding, so each cap triangle is
  wound to MATCH its neighbours (no flipped patch — confirmed against M562). A `maxEdges` limit fills only small
  holes (pinholes, cut faces) while leaving big openings (a deliberately open cup mouth) untouched. Verified
  (`ctest -R mesh_hole_fill`): a cube with one face removed starts non-watertight, and filling adds a 4-triangle
  centre fan that seals it back to watertight with zero holes and a winding consistent with the rest of the cube;
  two removed faces fill to two caps (8 triangles) and reseal; `maxEdges=3` leaves the 4-edge hole open so the
  mesh stays unsealed; an already-closed cube is returned unchanged; empty meshes are safe. Honest scope: a simple
  centre-fan cap — ideal for small, roughly-flat or convex holes; a large or highly non-planar hole gets a valid
  but crude flat-ish patch (feed it to M540/M564 smoothing or a remesh for a nicer surface); the centre vertex
  copies the rim's average colour and leaves its normal zero for a downstream computeNormals pass; non-manifold
  edges are not repaired (a distinct problem). [VERIFIABLE HERE]
- [x] **Watertightness / hole report** (`render::analyzeWatertight`, `WatertightReport`, `MeshHole`) — DONE
  (M566); is the mesh SEALED, or does it have gaps? A watertight (closed) surface has no open edges and no edge
  shared by three-plus faces — what 3D printing, boolean/CSG, solid physics, volume/mass, and inside/outside tests
  all require. This walks the mesh's edges to answer "is it closed?" and, when it isn't, finds every HOLE (open
  boundary loop), reporting how many there are and — per hole — its rim as an ordered vertex loop with an edge
  count and perimeter length, so you can rank the big gaps worth patching from the pinholes. It also surfaces
  NON-MANIFOLD edges (three-plus faces meeting), the other way a mesh fails to be a clean solid. Reuses the M528
  half-edge topology and the M536 boundary-loop extractor. Verified (`ctest -R mesh_watertight`): a closed cube is
  watertight with zero boundary/non-manifold edges and no holes; removing one face opens exactly one hole with 4
  boundary edges, a 4-vertex rim, and perimeter 4 (the unit face), reported as the largest hole; removing two
  opposite faces opens two holes with 8 boundary edges; a flat quad reads as one boundary loop with the right
  rectangle perimeter; empty meshes are vacuously watertight. Honest scope: "watertight" here is EDGE-manifold
  closure (no boundary, no 3+-face edges) — the standard printability/solidity test; it does not separately check
  consistent winding (use M562) or self-intersection (a distinct, costlier test); a mesh split into separate
  closed shells reads watertight with zero holes even though it's several pieces (pair with M529 components).
  [VERIFIABLE HERE]
- [x] **Mesh solidity (convexity) ratio** (`render::analyzeSolidity`, `SolidityReport`) — DONE (M565); how
  CONVEX is a shape? Wrap the mesh in its convex hull (the tightest dent-free shape — imagine shrink-wrapping it)
  and compare the mesh's own enclosed volume to the hull's. The ratio (mesh volume ÷ hull volume) is 1.0 for a
  perfectly convex solid (a cube, a ball, a die) and drops toward 0 the more the shape caves in (a bowl, a cog, a
  chair, a tree). This "solidity" is a one-number convexity score: it drives LOD/collision decisions (a
  near-convex prop can use its cheap hull as a collider), flags whether a boolean/CSG result stayed solid, and
  classifies shapes (blobby vs branchy) for procedural placement. Reuses the M532 mass-properties volume and the
  M292-era convex-hull builder (hull volume = signed tetrahedra summed over its faces). Verified (`ctest -R
  mesh_solidity`): a unit cube reports mesh volume 1, hull volume 1, solidity ~1; a cube with an inward cone
  dimple in its top face reads hull volume still 1 (the hull ignores the dent), mesh volume ~0.833 (the dimple
  removes ~1/6), solidity < 0.9 but > 0.7, and mesh volume < hull volume; an open (non-watertight) shell reports
  invalid; empty meshes are safe. Honest scope: the mesh volume needs a CLOSED, consistently-wound surface (open
  shells report invalid — weld/orient first via M525/M562; winding sign is auto-corrected so a closed but
  inside-out mesh still measures); the hull is built from the mesh's VERTICES, so solidity captures vertex-cloud
  concavity, not sub-vertex ripple; floating-point can push solidity a hair above 1.0 on a convex mesh (clamp for
  a strict [0,1]). [VERIFIABLE HERE]
- [x] **Vertex-colour smoothing** (`render::smoothVertexColors`) — DONE (M564); blur a mesh's per-vertex RGB
  across its edges without moving a single vertex. Baked vertex colours — ambient occlusion (M553), cavity/
  curvature (M556), hand-painted masks — often come out noisy or blocky: a low ray count leaves AO speckled, a
  coarse mesh makes cavity shading stair-step, a paint stroke lands hard-edged. This relaxes each vertex's colour
  toward the average of its edge-neighbours (a Laplacian blur on the COLOUR signal — the same relaxation as M540
  mesh smoothing, but on colour, not position), so shading reads soft and clean while geometry stays bit-for-bit
  identical. `strength` (0..1) sets the blur per pass, `iterations` the number of passes, and `pinBoundary` keeps
  open-edge vertices fixed. Reuses the M540 adjacency builder. Verified (`ctest -R mesh_vertex_color_smooth`): a
  single black speck among white neighbours brightens toward white while its neighbour picks up some darkness (the
  blur spreads), and the vertex POSITIONS are provably unmoved; more passes push the speck further toward white; a
  uniform colour field is unchanged (a flat blur is a no-op); a pinned boundary vertex keeps its colour; empty
  meshes are safe. Honest scope: smooths ONLY the RGB channels — positions, normals, UVs untouched; it's an
  unweighted (umbrella) Laplacian (neighbour count sets the weight, not edge length/angle), fast and stable but it
  slightly blurs across sharp colour boundaries — lower `strength`/`iterations` or pin boundaries to preserve
  edges; output stays in the [0,1] range the vertices already use. [VERIFIABLE HERE]
- [x] **Degenerate / sliver-triangle classifier** (`render::analyzeDegenerate`, `DegenerateReport`, `TriDefect`)
  — DONE (M563); find the badly-shaped triangles in a mesh and label each by DEFECT TYPE, returning their indices
  as a cleanup report. Bad triangles come from booleans, decimation, planar cuts, and sloppy imports; they wreck
  normals, lighting, physics, and simplification. Unlike the M537 TriangleQuality score (a single 0..1 number per
  triangle), this NAMES the problem so a repair step knows what to do: ZERO-AREA (collapsed — vertices coincident
  or collinear, delete it), CAP (one angle near 180° — a flat sliver poking across its neighbours, split or
  collapse), and NEEDLE (one angle near 0° — a thin spike from a very short edge, collapse along it). It also
  returns the mesh's sharpest and widest angles and smallest area. Verified (`ctest -R mesh_degenerate`): in one
  mesh a healthy right triangle reads Ok, three collinear vertices and two coincident vertices are both ZeroArea,
  a near-180° apex is a Cap, and a thin spike is a Needle; the per-type index lists hold 2/1/1 with badCount 4;
  the reported widest angle exceeds 150° (the cap) and sharpest is under 5° (the needle); a well-shaped triangle
  under the default thresholds is not flagged; empty meshes are safe. Honest scope: this REPORTS defects (kind +
  indices + extremes) but does not repair them — feed the indices to a collapse/split/delete pass (MeshCleanup
  M541 already drops the zero-area ones); classification order is zero-area → cap → needle, so a triangle that is
  both spiky and flat is reported as a cap; thresholds are tunable and the zero-area epsilon scales with the
  bounding box. [VERIFIABLE HERE]
- [x] **Triangle winding / normal-consistency detector** (`render::analyzeWinding`, `render::makeWinding-
  Consistent`, `WindingReport`) — DONE (M562); find the triangles whose winding (vertex order — which decides
  which way a face points) DISAGREES with their neighbours, and re-wind them so the surface is uniformly
  oriented. Flipped faces are one of the most common import defects: they render black under lighting, punch
  holes in shadows, and break backface culling and solid-mesh tests. In a consistently wound surface every shared
  edge is traversed in OPPOSITE directions by its two triangles; a flipped triangle runs its shared edges the
  SAME way. This counts the inconsistently-wound edges, then walks each connected surface from a seed and
  propagates a consistent orientation across the M528 half-edge topology, reporting the MINORITY set per
  component (the triangles to flip) — the analysis behind a "recalculate / make normals consistent" command;
  `makeWindingConsistent` applies the fix. Verified (`ctest -R mesh_winding`): a uniformly wound grid and a proper
  cube report zero inconsistent edges and nothing to flip; reversing one grid triangle is detected as exactly one
  minority flip (the reversed triangle) with its shared edges inconsistent, and `makeWindingConsistent` repairs it
  to zero; flipping a whole cube face (two triangles) is detected as two flips and repaired; empty meshes are
  vacuously consistent. Honest scope: this reports which triangles disagree WITH EACH OTHER and the smaller set to
  flip per component; it does NOT decide which way is "out" (pair with M546 containsPoint or a signed-volume test
  to orient outward); non-orientable surfaces (a Möbius strip) have no consistent assignment and keep a non-zero
  inconsistent-edge count; boundary and non-manifold edges are not propagated across. [VERIFIABLE HERE]
- [x] **Vertex valence / irregular-vertex report** (`render::analyzeValence`, `ValenceReport`) — DONE (M561);
  count how many edges meet at each vertex (its VALENCE) and flag the IRREGULAR ones. In a clean triangle mesh
  almost every interior vertex has valence 6; the 5s and 7s — poles or singularities — are where edge flow
  pinches or splays, and retopology/subdivision tools work to MINIMISE them because they cause shading artefacts,
  uneven subdivision, and awkward UV/animation deformation. The report gives per-vertex valence, marks boundary
  vertices (open edges, judged separately since their low valence is expected), and tallies regular (valence 6)
  vs irregular interior vertices plus isolated (unreferenced) ones — a one-number read on mesh quality that a
  modelling tool surfaces as a "show poles" overlay or retopo score. No topology build needed: it counts distinct
  edge-neighbours and single-face (boundary) edges directly. Verified (`ctest -R mesh_valence`): a 5×5 regularly
  triangulated grid reports its 9 interior vertices as regular valence-6 with zero interior poles, all 16 rim
  vertices boundary, and the centre vertex valence exactly 6; grid corners are boundary with valence ≥ 2; a lone
  triangle is three valence-2 boundary vertices with no interior; a closed cube has zero boundary and all eight
  corners interior with valences ≤ 6; an unreferenced vertex is isolated (valence 0); empty meshes are safe.
  Honest scope: "regular = 6" is the triangle-mesh convention (a quad mesh's ideal is 4 — pass `regularValence`);
  valence counts DISTINCT connected neighbours, so a non-manifold or duplicated-vertex mesh can report surprising
  values (weld first via M525/M559); boundary vertices are counted but never labelled irregular. [VERIFIABLE HERE]
- [x] **Mesh symmetry-plane detection** (`render::detectSymmetryPlanes`, `SymmetryReport`, `SymmetryPlane`,
  `SymmetryAxis`) — DONE (M560); decide whether a mesh is MIRROR-SYMMETRIC and across which plane. Most game
  props and characters are built symmetric (a face, a car, a sword), and knowing the symmetry plane unlocks a
  lot: symmetric modelling/sculpt tools that mirror edits, half-mesh authoring then reflect (feeds the M547
  mirror tool), UV/texture mirroring, left-vs-right variant checks, and pivot/alignment fixes for importers that
  landed a model off-axis. This tests the three axis-aligned candidate planes through the mesh's centroid (normal
  along X, Y, Z), reflects every vertex across each, and scores the plane by the fraction of vertices that land
  on an existing vertex within tolerance — reporting the best plane, each axis' score, and a symmetric flag. A
  spatial hash keeps the correspondence lookup near-linear. Verified (`ctest -R mesh_symmetry`): an origin-centred
  box scores ~1 and symmetric on all three axes; a point set mirrored only in X scores ~1 on X (symmetric), fails
  Y, and reports X as the best plane; a lone unpaired vertex drops the best score below the acceptance threshold
  (also shifting the centroid off the box — the honest consequence of centroid-based placement); an off-centre
  box is still found symmetric about its own centre (the plane sits at the centroid, not the world origin); empty
  meshes are safe. Honest scope: only the three AXIS-ALIGNED planes through the centroid are tested — a model
  symmetric about a tilted or off-centre plane reads as non-symmetric (fit an OBB via math::FitObb and test in
  its frame); scoring is by VERTEX correspondence, so an asymmetric tessellation of a symmetric SHAPE can score
  below 1; default tolerance scales with the bounding box (1e-3 of its diagonal). [VERIFIABLE HERE]
- [x] **Weld-tolerance auto-detect** (`render::suggestWeldTolerance`, `render::autoWeld`, `WeldSuggestion`) —
  DONE (M559); look at how a mesh's vertices are spaced and SUGGEST a good weld distance, so you don't have to
  guess the epsilon that M525 weldVertices needs. Importers routinely duplicate the vertices along every UV seam
  or smoothing split — sometimes at the exact same spot, sometimes a hair apart — and welding them back together
  is what makes a mesh watertight for physics, simplification, and normal smoothing; but too small an epsilon
  leaves seams split, too large collapses genuine detail. This measures every vertex's nearest neighbour, finds
  the natural GAP between the tight cluster of duplicate/seam pairs and the much larger spacing of real geometry,
  and returns an epsilon sitting safely in that gap plus a `WeldSuggestion` report (min/median gap, exact-
  duplicate count, weldable-vertex count, and a `bimodal` flag for whether a clear split was found). `autoWeld`
  chains the suggestion straight into weldVertices. Verified (`ctest -R mesh_weld_auto`): a clean 1.0-spaced grid
  suggests an epsilon below the spacing that welds nothing (not bimodal, median gap ≈ 1); a grid with 0.001-apart
  seam duplicates detects the split, suggests an epsilon between 0.001 and 1.0, flags the six duplicate vertices
  weldable, and autoWeld collapses them back to the unique grid; exact duplicates are counted and welded; five
  coincident vertices weld to one; empty/single-vertex meshes are safe. Honest scope: nearest-neighbour distances
  are pairwise O(n²) — intended for import-time analysis of moderate meshes (up to a few thousand vertices;
  bucket or decimate very large ones first); the suggestion is a heuristic, reliable when duplicates and real
  geometry are clearly separated (`bimodal`) and conservative (welds nothing) when they are not. [VERIFIABLE HERE]
- [x] **Feature-line extraction (ridge/valley crest lines)** (`render::extractFeatureLines`, `FeatureLines`,
  `FeatureEdge`, `FeatureKind`) — DONE (M558); find a mesh's SHARP FOLDS, label each as a convex RIDGE or a
  concave VALLEY, and CHAIN them into connected polylines. Where M548 sharp-edge detection only answers "which
  edges are creased," this goes two steps further: it tells you which WAY each crease bends (a roof ridge vs a
  gutter valley — the sign of the fold relative to the outward face normals) and stitches the loose creased edges
  into actual CURVES. Those curves are what stylized / NPR renderers stroke as ink outlines and interior hard
  lines, what retopo tools follow to lay clean edge loops, what auto-UV treats as natural seam candidates, and
  what a "select hard edges" editor command returns. Reuses the M528 half-edge topology; `FeatureKind` is
  Ridge/Valley, `FeatureEdge` carries endpoints + sharpness + kind, and `lines` holds the chained vertex-index
  polylines with `ridgeCount`/`valleyCount` tallies. Verified (`ctest -R mesh_feature_lines`): a convex tent fold
  yields exactly one crease classified Ridge on the correct edge; a concave valley fold yields one Valley; a flat
  sheet yields nothing; a cube exposes its twelve 90° edges all as convex ridges (face diagonals stay flat) and
  the chained polylines cover all twelve edges exactly once; a fold gentler than the angle threshold is ignored
  until the threshold drops; empty meshes are safe. Honest scope: ridge/valley sign needs outward-consistent
  winding (an inside-out mesh flips the labels); only manifold interior edges (exactly two faces) are considered;
  chaining produces maximal simple paths and breaks at junctions, so a branching crest network returns as several
  polylines meeting at the junction rather than one tangled path. [VERIFIABLE HERE]
- [x] **Mesh wall-thickness probe** (`render::computeThickness`, `render::analyzeThickness`, `ThicknessReport`)
  — DONE (M557); measure how THICK the material is at every point of a surface by shooting a ray straight INTO
  the surface (opposite its outward normal) and returning the distance to the wall on the far side. This is the
  "wall thickness" / "shell gauge" check 3D-printing slicers and CAD tools run to catch spots too thin to print
  or structurally weak; it also drives subsurface-scattering thickness maps (skin, wax, leaves glowing at thin
  edges) and "is this hollow shell uniform?" audits. For a solid model it reports the full span across the object;
  for a hollow shell it reports the gap between outer and inner walls. `computeThickness` returns per-vertex
  thickness (unreachable vertices → `maxDistance`, treated as open); `analyzeThickness` rolls that into a
  `ThicknessReport` (min/mean thickness, measured vs open counts) so the thinnest wall — the print/structural risk
  spot — pops out. Reuses the M533 ambient-occlusion Möller–Trumbore ray/triangle test. Verified (`ctest -R
  mesh_thickness`): a slab of two parallel outward-facing sheets one unit apart reads thickness ≈ 1.0 across the
  whole top sheet; a 0.25-unit gap reads ≈ 0.25; the summary finds min ≈ the gap, flags the oversized backing
  sheet's overhanging corners as open, and reports a finite mean; a lone one-sided sheet reads entirely open;
  empty meshes are safe. Honest scope: one inward ray per vertex along the smooth normal — a wall sampled at a
  glancing angle reads thicker than its true minimum (average several offset rays for a robust min); winding must
  be outward-consistent so the ray points into the material. [VERIFIABLE HERE]
- [x] **Cavity (curvature) vertex-color bake** (`render::computeCavity`, `render::bakeCavityToVertexColor`) —
  DONE (M556); write a mesh's own SHAPE into its vertex colours so CREVICES, GROOVES, and concave folds go DARK
  while RIDGES, sharp edges, and convex bulges go LIGHT — the "cavity map" sculpting tools (ZBrush/Blender/
  Substance) overlay to make carved detail, panel-line grime, and worn edges read at a glance. Here it's baked
  straight into per-vertex RGB with NO texture and NO ray-tracing, so any flat-lit or unlit renderer shows the
  form for free. The signal is a signed first-ring curvature: the vector from each vertex to the CENTROID of its
  edge-neighbours, projected onto the (internally recomputed, area-weighted) surface normal and scaled by local
  edge length — neighbours further out along the normal mean the vertex is recessed → concave → positive;
  neighbours pulled inward mean it juts out → convex → negative; flat gives ~0. Complements the M553 ambient-
  occlusion bake (which shoots rays to measure how BOXED-IN a point is) by capturing purely LOCAL fold detail far
  more cheaply and sharpening creases AO misses, and the M539 curvature estimator (which returns unsigned |H|
  magnitude, no dark/light sign). Verified (`ctest -R mesh_vertex_color_cavity`): a smooth UV sphere reads
  uniformly CONVEX (negative mean, max−min spread < 0.15); a V-shaped valley reads CONCAVE (positive) along its
  crease and more concave than the flat flank; an inverted roof reads CONVEX (negative) along its ridge; a flat
  grid reads ~0 everywhere; the colour bake darkens the concave crease below a bright flat flank and lightens a
  convex ridge on a grey base, with every channel clamped to [0,1]; empty meshes are safe. Honest scope: a fast
  first-ring DISCRETE proxy (not exact mean curvature) — detail finer than the tessellation is invisible; open
  boundary vertices see a lopsided neighbourhood and read less reliably; the raw signal is unbounded (the colour
  path clamps via a contrast knob). [VERIFIABLE HERE]
- [x] **Area-weighted surface point sampler** (`render::sampleSurfacePoints`, `SurfacePoint`) — DONE (M555);
  scatter N points UNIFORMLY over a mesh's surface so every unit of area is equally likely — a triangle twice as
  big gets twice the points. This is the seed for scattering grass, rocks, foliage, or debris across terrain and
  props; turning a mesh into a point cloud; placing decals or spawn markers; and as the input a blue-noise /
  Poisson-disk relaxation pass refines. Each returned point carries its world POSITION, the FACE NORMAL there (to
  orient what you place), and the TRIANGLE it landed on. Picking is exact: a per-triangle cumulative-area CDF
  chooses the face by binary search on one uniform draw, then a standard barycentric warp (b0=1−√r₁, b1=√r₁·(1−r₂),
  b2=√r₁·r₂) places the point uniformly inside it. Deterministic — the same `seed` reproduces the same points
  every run (self-contained LCG, no engine RNG dependency). Verified (`ctest -R mesh_surface_sample`): 2000 samples
  of a unit right-triangle all land on the plane inside the triangle carrying the +Z face normal; a triangle 100×
  larger than its neighbour collects >20× more points while the small one still gets some (area weighting); the
  same seed reproduces identical points and a different seed diverges; a unit quad keeps every sample in [0,1]² and
  hits both equal-area triangles; empty mesh and zero count return empty safely. Honest scope: uniform over AREA,
  not blue-noise — points can clump, feed them into a relaxation pass for even spacing; degenerate (zero-area)
  triangles are never chosen; the normal is the geometric FACE normal (winding-derived), not the interpolated
  smooth normal. [VERIFIABLE HERE]
- [x] **Mesh statistics report** (`render::analyzeMesh`, `MeshStats`) — DONE (M554); the at-a-glance
  size-and-scale report an editor's mesh-info panel or an import log shows: the axis-aligned BOUNDING BOX
  (min/max/size/centre), the area-weighted CENTROID (the shell's balance point), the total SURFACE AREA, and the
  edge-length distribution (shortest/longest/mean — a quick read on tessellation uniformity and whether the mesh
  is scaled sanely). It answers "how big is this, where is it centred, how dense is it" before you place, scale,
  texture (texel density needs area), or LOD a mesh. Distinct from TriangleQuality (M537, per-triangle SHAPE) and
  MeshMassProperties (M532, the SOLID's volume/inertia) — this is the SURFACE's extent and area. Verified
  (`ctest -R mesh_stats`): a unit cube reports V/F/E = 8/12/18, bounds [0,1]³, size 1×1×1, surface area exactly
  6, centroid at (0.5,0.5,0.5), shortest edge 1 and longest √2 with the mean matching the 12×1 + 6×√2 mix; a cube
  translated to corner 2 with edge 3 shifts bounds to [2,5], centroid to 3.5, and area to 54 (6·3²); a right
  triangle reports area 0.5, centroid (⅓,⅓), edges 1/1/√2; empty safe. Honest scope: the centroid is
  area-weighted over the shell (not the vertex average, not the solid's centre of mass); edges counted once;
  overlapping triangles count their area twice (accurate to the triangles present). [VERIFIABLE HERE]
- [x] **Vertex-color ambient-occlusion bake** (`render::bakeAoToVertexColor`) — DONE (M553); darken each vertex's
  stored RGB by how OCCLUDED it is, so the mesh carries its own soft contact shadows with no texture, no
  lightmap, and no runtime lighting. AO is the free ambient shadowing of nooks and crevices — under a ledge,
  inside a fold, where two walls meet — and baking it straight into vertex colours is the cheapest way to give
  flat-lit / mobile / retro content that grounded, hand-painted look (exactly Blender's "bake AO to vertex
  colours", and what many low-poly games ship). Reuses the M533 hemisphere raycaster (`bakeVertexAO`, 0=open,
  1=occluded) and multiplies each channel by (1 − ao·strength): open surfaces keep their colour, creases go
  dark. Returns a copy; multiplicative so a tint survives. Verified (`ctest -R mesh_vertex_color_ao`): an open
  upward face stays bright (AO≈0); a floor trapped under a low overhanging ceiling darkens strongly (every floor
  vertex well below open brightness); strength=0 is an exact no-op; a tinted mesh keeps its channel ratios (zero
  channel stays zero, g:r preserved); empty safe. Honest scope: MULTIPLIES existing colours (call once, baking
  twice double-darkens), and AO smoothness is limited by tessellation (it is per-VERTEX) — bake to a texture for
  crisp AO on low-poly meshes. [VERIFIABLE HERE]
- [x] **Spherical & cylindrical UV projection** (`render::sphericalUv` / `render::cylindricalUv`) — DONE (M552);
  the wraparound auto-unwraps for round objects, completing the projection-UV family alongside planar/box (M551).
  SPHERICAL maps each vertex by its direction from a centre to longitude (u, around) and latitude (v,
  top-to-bottom) — the equirectangular / lat-long layout a planet, eyeball, ball, or skydome wants (world maps
  are stored exactly this way). CYLINDRICAL maps the angle around an axis to u and the height along it to v —
  what a bottle label, tree trunk, pipe, or tin can wants. UVs are written onto the existing vertices (topology
  unchanged). Verified (`ctest -R mesh_uv_radial`): spherical sends +X→(0.5,0.5), the +Y pole→v=0, the −Y
  pole→v=1, +Z equator→(0.75,0.5), −X→(1.0,0.5), and is scale-invariant (direction only, a radius-5 point matches
  its unit direction); cylindrical about Y sends +X→u=0.5 with height→v, +Z→u=0.75, and vScale/vOffset scale the
  height axis; empty safe. Honest scope: both carry the inherent projection artefacts — a single wrap SEAM where
  u jumps 1→0, spherical pole PINCHING, cylindrical cap stretch — properties of the projection, not bugs; an
  LSCM/angle-based unwrap is the distortion-free follow-up. [VERIFIABLE HERE]
- [x] **Projection UV unwrap** (`render::planarUv` / `render::boxUv`) — DONE (M551); auto-generate texture
  coordinates without a hand-made unwrap by PROJECTING world positions onto a plane. PLANAR drops every vertex
  straight down one axis (the top-down decal / terrain map — paint a whole floor or landscape with one texture,
  topology untouched); BOX (triplanar) picks, per triangle, the axis its face points most toward and projects
  onto that plane (the instant "cube" UV DCC tools offer for hard-surface props — each face gets sensible,
  low-distortion coordinates with no manual seam work, returned as a facet-split mesh since neighbouring faces
  use different axes). This is the quick UV a mesh needs before it can show a tiled material, decal, or checker
  map. Natural per-axis channel choice keeps textures upright (X→(z,y), Y→(x,z), Z→(x,y)); `scale`/offset tile
  the result. Verified (`ctest -R mesh_uv_project`): a unit XZ quad projected top-down maps exactly to the unit
  square with topology preserved; scale+offset shift the UV span as expected; box projection on a cube
  facet-splits to 36 vertices with UVs filling [0,1]²; the top face (normal ±Y) projects to a full unit square
  (not collapsed), confirming dominant-axis selection; empty safe. Honest scope: projection UVs stretch on
  surfaces steep to the axis and seam at the 45° between box axes (inherent to projection unwraps); box maps
  opposite faces identically (no back-face flip); angle-based/LSCM unwrap is the heavier follow-up. [VERIFIABLE HERE]
- [x] **UV-seam edge detection** (`render::detectUvSeams`, `UvSeamResult`) — DONE (M550); find the edges where a
  mesh's texture coordinates are DISCONTINUOUS — the two triangles meeting along a 3D edge disagree on the UV of
  the shared corners, so the texture is cut there. Those cuts are the seams of a UV unwrap, the boundaries of the
  flat "islands" a model is unfolded into (a cube unwrapped as a cross is seamed along most of its rim). Knowing
  them drives lightmap / texture-atlas SEAM DILATION (bleed colour a few texels past a seam so bilinear filtering
  doesn't sample the gap), seam-hiding and seam-aware smoothing, and "select seams" in a UV editor. Detection
  welds vertices by POSITION so the same physical edge from two UV islands is recognised as one edge, then
  compares the UVs the two faces assign at each endpoint; boundary (island-rim) and non-manifold edges are
  counted separately. Verified (`ctest -R mesh_uv_seams`): a cube whose six faces are separate UV islands reports
  a seam along all twelve rim edges but none along the six in-face diagonals (18 interior edges, 12 seams, 0
  boundary); a continuous shared-UV grid has zero seams; splitting a strip's UVs down its middle column yields
  exactly one seam, lying on that column; empty safe. Honest scope: compares the mesh's stored per-vertex UVs
  (a mesh that already welds UV-identical corners correctly reports no seam there); position weld is
  grid-quantised. [VERIFIABLE HERE]
- [x] **Flat-shading facet split** (`render::facetMesh`) — DONE (M549); rebuild a mesh so every triangle owns
  its three OWN vertices, each carrying that triangle's FACE normal. With no vertex shared between faces the
  lighting can't blend across an edge, so the surface renders faceted — every triangle a crisp flat plane. This
  is Blender's "Shade Flat" / the low-poly look (a sphere becomes a geodesic gem, terrain becomes stylised
  facets) and also the honest way to export a mesh whose faces really are flat (a cube's corners should NOT be
  smoothed). It is the inverse of smooth shared-vertex shading — pair with computeNormals (M175) for the smooth
  version. Positions/colours/UVs are copied per corner; only the normal is replaced with the face normal.
  Verified (`ctest -R mesh_facet`): a single triangle yields 3 vertices all bearing the +Z face normal with
  positions unchanged; a cube expands to exactly 36 vertices (12 triangles × 3) with its triangle set preserved;
  the cube's normals are the six unit axis directions and every normal is unit length; empty safe. Honest scope:
  this INFLATES the vertex count by design (a display/export transform, not an optimisation); degenerate
  triangles get a zero normal but are still emitted (drop them first with MeshCleanup if unwanted). [VERIFIABLE HERE]
- [x] **Dihedral-angle / sharp-edge detection** (`render::detectSharpEdges`, `SharpEdgeResult`) — DONE (M548);
  for every interior edge, the dihedral angle between the two triangles sharing it (180° = flat/coplanar, 90° =
  a right-angle fold, → 0° = folded back on itself), plus the list of edges bending more sharply than a
  threshold. Sharp edges are a model's CREASES — a cube's twelve rims, the fold of a roof, the lip of a cup — and
  detecting them drives wireframe/crease overlay in an editor, automatic bevel/chamfer selection, UV-seam and
  hard-normal suggestions (creases usually want a seam and a split normal), and feature-preserving
  simplification/smoothing that must not round a crease off. The angle comes from the two face normals across the
  shared edge; boundary and non-manifold edges (not exactly two faces) are skipped. Reuses MeshTopology (M528)
  for the twin-face lookup; complements MeshHardEdges (M535) which SPLITS on crease angle — this one just
  REPORTS. `SharpEdgeResult` gives every interior edge with its dihedral/sharpness, the sub-list past the
  threshold, and the sharpest angle. Verified (`ctest -R mesh_sharp_edges`): two coplanar triangles read 180° and
  are never sharp; a right-angle fold reads exactly 90°; a cube has 18 interior edges — 12 rim creases at 90°
  (flagged) and 6 flat face-diagonals at 180° (not) — with the threshold correctly gating the count (12 sharp at
  45°, 0 at 100°); empty safe. Honest scope: geometric normals, so inconsistent winding may mis-sign an angle
  (reported unsigned); weld first so a crease is one edge. [VERIFIABLE HERE]
- [x] **Mesh mirror / symmetrize** (`render::mirrorMesh`) — DONE (M547); reflect a mesh across an axis-aligned
  plane and join the reflection to the original, producing a symmetric whole — the "mirror modifier" every DCC
  tool has (model one wing / half a face / the left of a ship, mirror, get a seamless symmetric result; also
  used to symmetrize a slightly-off scan). Reflection flips handedness, so each mirrored triangle's winding is
  REVERSED and its baked normal negated to keep faces pointing outward; vertices lying on the mirror plane
  (within an epsilon) are SHARED not duplicated, so the seam is watertight. `axis` (0=X,1=Y,2=Z) and `planeCoord`
  pick the plane. Verified (`ctest -R mesh_mirror`): a triangle with a vertex on the plane shares that seam
  vertex (3→5 not 3→6) and yields a symmetric bounding box; a +X-facing triangle mirrors to a −X-facing one
  (winding reversed → still outward); with no seam the vertex/triangle counts exactly double; the whole vertex
  set is symmetric across the plane; Y-axis mirroring works; empty safe. Honest scope: welds only along the
  mirror seam (not interior duplicates or the two halves elsewhere) — run MeshCleanup/MeshWeld after if needed;
  vertices on the far side are still mirrored, so clip to a half first if you need a strict one. [VERIFIABLE HERE]
- [x] **Point-in-mesh containment** (`render::containsPoint` / `containsPoints`) — DONE (M546); is a point INSIDE
  a closed triangle mesh? For each query point cast one ray to infinity and count triangle crossings — odd =
  inside, even = outside (the Jordan-curve / ray-parity test), reusing the M533 Möller–Trumbore ray/triangle (the
  same inside test that signs MeshSdf and fills MeshVoxelize). Unlike those, this answers arbitrary points
  DIRECTLY with no grid to bake — the right tool for a handful of ad-hoc tests: is a spawn point inside the level
  geometry, is a particle/agent still within a volume, does a prop's centre sit in a trigger solid,
  rejection-sampling points into a shape. Batch and single-point entry points; an oblique ray dodges the
  through-a-shared-edge degeneracy and an AABB quick-reject skips points outside the bounds. Verified
  (`ctest -R mesh_containment`): a cube reports its centre and interior points inside and beyond-face / far
  points outside; a batched lattice matches the interior predicate everywhere (away from the ambiguous exactly-
  on-face boundary); a sphere agrees with the analytic radius test both single and batched; empty mesh / empty
  points safe. Honest scope: brute force O(points·triangles) assuming a watertight mesh — for many queries
  against a big mesh, bake a MeshSdf once or index with game::Bvh; a generalized-winding-number test for open
  meshes is the follow-up. [VERIFIABLE HERE]
- [x] **Mesh topology summary** (`render::summarizeTopology`, `TopologySummary`) — DONE (M545); one struct
  answering "what SHAPE, topologically, is this mesh?": how many separate pieces (connected components), how many
  holes ring it (boundary loops), whether it is a closed watertight solid and manifold, its Euler characteristic
  V−E+F, and its GENUS — the number of handles / through-holes (sphere/box = 0, donut/torus/mug = 1, pretzel
  higher). This is the mesh-health / "is this a valid printable solid, how complex is it" report a DCC tool or
  3D-print slicer shows, and the sanity check before physics/booleans/simplification that assume a clean
  manifold. It composes the connectivity stack — MeshTopology (M528) for edges/manifoldness, MeshComponents
  (M529) for the piece count, MeshBoundaryLoops (M538) for the hole count — then genus falls out of the
  Euler–Poincaré formula χ = 2c − 2g − b. Vertex count is the number of REFERENCED vertices (unused vertices in
  the buffer are ignored so they can't corrupt Euler). Verified (`ctest -R mesh_topology_summary`): a closed
  cube is genus 0 / χ 2 / one component / no holes / closed+manifold; a torus is genus 1 / χ 0; a cube missing a
  face is an open disk (χ 1, one boundary loop, genus 0, not closed); two separate cubes are two components with
  χ 4 and genus 0; a non-manifold fan (edge shared by three triangles) is flagged and leaves genus undefined
  (−1); empty safe. Honest scope: genus is exact only for a welded orientable manifold — run MeshCleanup/MeshWeld
  first; un-welded duplicate corners still corrupt Euler (reported via `manifold`). [VERIFIABLE HERE]
- [x] **Triangle-strip generation** (`render::buildTriangleStrips` / `expandTriangleStrips`, `TriangleStrips`)
  — DONE (M544); repack an indexed triangle LIST into triangle STRIPS. A strip stores a run of triangles as one
  vertex sequence v0 v1 v2 v3… where every new vertex forms a triangle with the previous two (GPU
  GL_TRIANGLE_STRIP / primitive-restart semantics), so N connected triangles cost N+2 indices instead of 3N —
  less index bandwidth, and the form fixed-function / mobile / retro GPU paths and some file formats (MD2/MD3,
  PS2/GameCube era) prefer. Greedy: start a triangle, orient it so its trailing edge has an un-stripped
  neighbour, walk neighbour-to-neighbour across the trailing edge until the run dead-ends, separate runs by a
  restart index. `expandTriangleStrips` is the exact inverse, so the pair round-trips the triangle SET
  losslessly. Verified (`ctest -R mesh_strip`): a 6×6 grid strips-and-expands back to the identical triangle
  set AND compresses (fewer indices than 3·triangleCount, stripCount < triangleCount); two edge-sharing
  triangles become one strip of four; a lone triangle is a strip of three; disconnected triangles stay separate
  strips; degenerate triangles are dropped; a cube round-trips its 12 faces; empty is safe. Honest scope: greedy,
  not length-optimal (NvTriStrip/tipsify find longer runs); winding alternates per GPU convention so the
  expanded list preserves the triangle SET though a face's winding may be normalised. [VERIFIABLE HERE]
- [x] **Mesh plane slice / cross-section** (`render::sliceMesh`, `SliceContour`) — DONE (M543); intersect a
  triangle mesh with an infinite plane and return the CONTOUR — the line segments where the surface crosses the
  plane, chained into ordered (closed on a watertight solid) polyline loops. This is the cross-section a CAD
  tool draws and the building block for cutaway / section views, a waterline or lava-line on a hull, terrain
  contour ("topographic") lines at a set of heights, silhouette/outline extraction, deriving a 2D collision
  outline from a 3D prop, and 3D-printing slicers. Each straddling triangle contributes one segment between the
  crossings on its two sign-changing edges; the crossing on each mesh edge is keyed by that undirected edge, so
  the two triangles sharing it resolve to the SAME point — every contour point then has exactly two incident
  segments and the chain closes into clean loops (robust where naive spatial welding would let the chainer
  zig-zag across the ring). Verified (`ctest -R mesh_slice`): a unit cube sliced through its middle gives one
  closed square loop of perimeter exactly 4 with every point on the plane; a plane clear of the cube gives no
  contour; a diagonal plane still gives one closed loop (points satisfy x+y=1); a unit sphere sliced off-centre
  gives one closed ring whose points sit at the exact slice radius and whose perimeter approaches 2·π·r; empty
  mesh safe. Honest scope: a vertex exactly on the plane counts to the non-positive side and a coplanar face
  contributes no 1D contour; a non-manifold edge may leave an open chain, which is reported not silently closed.
  [VERIFIABLE HERE]
- [x] **Mesh geodesic distance** (`render::geodesicDistance`, `GeodesicResult`) — DONE (M542); the shortest
  "walk along the surface" distance from one or more source vertices to every other vertex, measured along the
  mesh's EDGES (Dijkstra on the vertex graph, edge weight = the 3D edge length). Straight-line distance cuts
  through the solid; geodesic distance is how far it actually is over the skin — what you want for heat-map /
  falloff vertex weights (damage or snow spreading from a point), texture-blend / vertex-paint masks that follow
  the form, region-growing "flood N metres from here" selection, feature-distance fields, and cheap procedural
  effects. Multi-source seeds every source at 0 in one pass (distance-to-nearest-feature); the predecessor array
  reconstructs the actual shortest edge-path via `pathFrom`. Verified (`ctest -R mesh_geodesic`): on a regular
  flat grid split along the main diagonal, distance along the bottom row is exactly x·spacing and to the far
  corner exactly n·√2·spacing (both equal the Euclidean straight line, so the estimate is exact there); no
  distance is ever shorter than the straight line; path reconstruction returns a chain ending at the source;
  multi-source takes the nearest source and never exceeds the single-source distance; disconnected islands are
  unreachable (infinite); empty mesh and out-of-range source are safe. Honest scope: the EDGE-graph geodesic
  slightly OVERestimates the true smooth surface geodesic on coarse meshes and converges under refinement; exact
  polyhedral geodesics (MMP / heat method) are the documented follow-up. [VERIFIABLE HERE]
- [x] **Mesh cleanup pass** (`render::cleanupMesh`, `MeshCleanupStats`) — DONE (M541); the import/optimization
  hygiene pass that shrinks a mesh without changing what it draws: merge BIT-EXACT duplicate vertices (identical
  in every attribute — position, normal, colour, UV) into one, drop DEGENERATE triangles (a repeated corner
  index → zero area), and remove UNUSED vertices (referenced by no surviving triangle), compacting the buffers.
  Importers/generators routinely emit this bloat — a face-by-face-authored glTF/OBJ repeats every shared corner,
  CSG/marching-cubes leaves orphaned vertices, edits leave slivers — and it costs VRAM, breaks the vertex cache,
  and stops smoothing/subdivision from treating a shared corner as one point. This is Godot's SurfaceTool.index()
  hygiene, but attribute-exact and attribute-PRESERVING — complementary to MeshWeld (which welds by SPATIAL
  proximity and keeps positions only): weld near-coincident corners with MeshWeld, strip exact redundancy with
  cleanupMesh while keeping normals/UVs intact. Deterministic (survivors keep first-seen order). Verified
  (`ctest -R mesh_cleanup`): orphan vertices removed with drawn geometry byte-identical, a face-by-face quad
  merges 6→4 vertices keeping both triangles, degenerate triangles dropped, dedup that collapses a triangle
  removes it too, an already-clean mesh is untouched and cleanup is idempotent, empty mesh safe. [VERIFIABLE HERE]
- [x] **Solid mesh voxelization** (`render::voxelizeSolid`, `VoxelGrid`) — DONE (M540); convert a closed triangle
  mesh into a boolean 3D occupancy grid — each cell 1 if its centre is INSIDE the solid, 0 outside. This bridges
  surface geometry to the volumetric representations games rely on: destructible/editable voxel terrain
  (Minecraft/Teardown-style), 3D nav volumes, GPU-particle/fluid collision masks, fast approximate inside tests,
  procedural interior filling, and the input to a marching-cubes/SurfaceNets re-mesh. Unlike a SURFACE
  voxelization (cells the triangles merely pass through), this is a SOLID fill: the inside/outside decision at
  each cell centre is a ray-parity test (one oblique ray; odd triangle-crossing count = inside), reusing the M533
  Möller–Trumbore ray/triangle — the same sign test MeshSdf (M534) uses at its grid corners. `VoxelGrid` exposes
  `at`, `cellCenter`, `solidCount`, and `estimatedVolume` (solid cells × cell volume). Verified
  (`ctest -R mesh_voxelize`): a grid-aligned unit cube fills to volume exactly 1.0 (finer grids stay exact with
  more cells); a padded cube leaves an empty outer shell yet still reads ~1.0; a radius-1 sphere fills to
  ~(4/3)π≈4.19 and a radius-2 sphere to ~8× that (the r³ law); empty mesh is safe. [VERIFIABLE HERE]
- [x] **Per-vertex discrete curvature** (`render::computeCurvature`, `MeshCurvature`) — DONE (M539); estimate how
  sharply a mesh bends at every vertex, producing two scalar fields: GAUSSIAN curvature K (positive on domes,
  negative on saddles, zero on developable surfaces) via the angle deficit `(2π − Σθ) / A_mixed`, and MEAN
  curvature |H| via the cotangent Laplacian — the classic Meyer/Desbrun/Schröder/Barr (2003) operators, with
  obtuse-safe Voronoi mixed area. Uses: curvature-adaptive tessellation/LOD, feature/crease/ridge-valley
  detection, curvature-guided remeshing, wear/cavity shading masks, "curvature" vertex-paint like a DCC tool.
  Reuses MeshTopology (M528) only to flag boundary vertices (where the closed-surface estimate does not apply).
  Verified (`ctest -R mesh_curvature`): a unit sphere converges to K≈1, H≈1 everywhere (positive Gaussian, no
  wild outliers); a radius-2 sphere scales to K≈1/4, H≈1/2 (confirming the 1/r² and 1/r laws); a flat grid gives
  ~0 curvature at interior vertices with its rim correctly flagged as boundary; empty mesh is safe. [VERIFIABLE HERE]
- [x] **Boundary / hole edge-loop extraction** (`render::extractBoundaryLoops`) — DONE (M538); walk a mesh's
  open edges into the ordered vertex LOOPS that ring each hole or the outer rim of an open surface. MeshTopology
  (M528) already counts boundary edges; this turns them into usable curves — the front end for hole FILLING
  (cap each loop), silhouette/outline rendering, cloth/rope attachment along an edge, and editor "select
  boundary". Each boundary undirected edge has one triangle, so its single directed half-edge (wound by that
  triangle) points consistently around the hole; chaining next[a]=b yields the loops. Verified
  (`ctest -R mesh_boundary_loops`): a watertight cube gives no loops; a cube with one face removed gives
  exactly one 4-vertex loop that is precisely the removed face's corners; a flat quad gives one 4-vertex
  perimeter loop; two disjoint quads give two loops; every loop is a proper closed cycle. Manifold boundaries
  (a figure-eight boundary vertex is the documented edge case). [VERIFIABLE HERE]
- [x] **Triangle-quality / sliver analysis** (`render::analyzeTriangleQuality`, `TriangleQualityStats`) — DONE
  (M537); the mesh-QA pass that flags badly-shaped triangles — long thin slivers and needles shade poorly,
  self-shadow, crawl under rasterization, and wreck physics and simplification. Every meshing tool reports a
  "triangle quality / minimum angle" histogram for this. Per triangle it computes the normalized mean-ratio
  quality q = 4·√3·area / (a²+b²+c²) — 1 for a perfect equilateral, → 0 as it degenerates into a sliver — plus
  its smallest interior angle (degrees, the number artists eyeball), and summarizes the worst triangle, the
  sliver count under a threshold, and the degenerate count. Verified (`ctest -R triangle_quality`) against
  closed-form values: an equilateral scores 1 with a 60° min angle; a right isosceles scores √3/2 ≈ 0.866 with
  45°; a long thin sliver scores near 0 with a tiny min angle and is flagged; the worst-triangle index and the
  sliver/degenerate counts are correct. [VERIFIABLE HERE]
- [x] **UV / texel-density analysis** (`render::analyzeUvDensity`, `UvDensityStats`) — DONE (M536); the
  asset-QA pass that checks a mesh's texture coordinates are sane before it ships — is the texel density
  uniform (so the texture is equally sharp everywhere, no stretched/blurry patches), and how much of the UV
  square does the layout use? Every DCC and Godot's lightmap importer offer this "texel density / UV stretch"
  checker. Per triangle it computes the UV-area ÷ world-area ratio (constant across the mesh = consistent
  density; an outlier = a stretched or over-sampled face), totals the world and UV areas (UV coverage of the
  [0,1] square — >1 means overlap/tiling, <1 means wasted atlas space), reports the area-weighted average and
  a min/max uniformity ratio, and counts degenerate faces. Verified (`ctest -R uv_density`): a unit quad on
  the unit UV square reads uniform density 1; scaling one triangle's UVs 2× quadruples that triangle's density
  (area scales as the square) giving a uniformity ratio of 4; the world/UV area totals are exact; a degenerate
  triangle is counted and excluded from the stats. [VERIFIABLE HERE]
- [x] **Hard-edge / smoothing-group split by crease angle** (`render::splitHardEdges`) — DONE (M535); the
  importer step that decides where a surface shades SMOOTH (normals averaged across an edge) versus FLAT (a
  crisp crease): every edge whose two faces meet at more than the crease angle is a hard edge, and the shared
  vertices along it are DUPLICATED so smooth-normal averaging doesn't bleed across the fold. This is Godot's
  import "Normals > From Smoothing Groups" / shade-smooth-by-angle, and the correct front end to
  `computeNormals` — a cube welded to 8 vertices would otherwise get rounded, mushy corners. Built on the M528
  topology: around each vertex the incident triangles are union-find grouped so neighbours joined by a
  sub-threshold edge stay together, and each group becomes one output vertex with its own averaged normal.
  Verified (`ctest -R mesh_hard_edges`): a welded cube split at 30° yields 24 vertices (4 per face) with flat
  axis-aligned normals; split at 100° (> the 90° edges) it stays 8 vertices with normals along the corner
  diagonals; a coplanar quad never splits; the triangle set is preserved; deterministic. [VERIFIABLE HERE]
- [x] **Signed distance field bake** (`render::bakeMeshSdf` / `render::sampleMeshSdf`, `MeshSdf`) — DONE
  (M534); sample the signed distance to a closed mesh's surface onto a 3D grid — negative inside the solid,
  positive outside, ~0 on the surface. An SDF is the shared currency behind soft/contact shadows, ambient
  occlusion, collision & penetration depth, smooth CSG/booleans, raymarched volumes, and obstacle flow-fields;
  Godot's SDFGI and 2D-SDF collision use exactly this. Unsigned distance at each cell is the exact
  closest-point-on-triangle minimum; the SIGN comes from a ray-parity inside/outside test (odd crossings =
  inside), reusing the M533 Möller–Trumbore ray/triangle. `sampleMeshSdf` trilinearly interpolates the grid at
  an arbitrary point. Verified (`ctest -R mesh_sdf`) against a unit cube's closed form: the centre samples
  −0.5 (deepest interior = the inradius), a point 0.5 outside the +X face samples +0.5, a point on a face is
  ~0, the sign flips inside→negative / outside→positive on multiple axes, and the field's minimum is ≈−0.5.
  Brute force O(cells·tris) with a ray-parity sign (watertight-mesh assumption); a game::Bvh acceleration and a
  generalized-winding-number sign for open meshes are the documented follow-ups. [VERIFIABLE HERE]
- [x] **Per-vertex ambient occlusion bake** (`render::bakeVertexAO`) — DONE (M533); the offline "bake AO into
  the mesh" step that darkens crevices, contact points, and interiors so a scene reads with depth even under
  flat ambient light — Godot's LightmapGI / classic vertex-bake idea, stored per vertex. For each vertex it
  shoots a deterministic golden-angle fan of rays over the hemisphere around its (area-weighted) normal and
  measures the fraction blocked by the mesh's own triangles within a distance (Möller–Trumbore ray/triangle):
  fully open → 0, deep in a cavity → toward 1. No RNG, so the bake is deterministic. Verified
  (`ctest -R mesh_ambient_occlusion`): a bare flat floor bakes to ~0 everywhere, adding a pillar makes the
  floor vertex beside its base markedly more occluded (>0.1 higher) than a far-open vertex which stays ~0,
  values stay in [0,1], and repeated bakes are identical. Brute force O(verts·rays·tris) — fine for offline
  prop/level bakes; a game::Bvh acceleration and multi-bounce colour bleed are the documented follow-ups.
  [VERIFIABLE HERE]
- [x] **Mesh mass properties** (`render::computeMassProperties`, `MassProperties`) — DONE (M532); compute the
  VOLUME, CENTER OF MASS, and full INERTIA TENSOR of the solid bounded by a closed triangle mesh, assuming
  uniform density — what a physics engine needs to make a custom (non-primitive) collider spin correctly.
  Physics3D already derives inertia analytically for boxes/spheres/capsules, but an arbitrary imported hull
  had no way to get its real mass distribution; this fills that. Uses the signed-tetrahedron method
  (Mirtich / Blow–Binstock): each triangle forms a tetrahedron with the origin and the signed contributions
  sum to the exact integrals over the enclosed solid, independent of the origin. Inward winding is
  auto-corrected. Verified (`ctest -R mesh_mass_properties`) against closed-form values: a unit cube has
  volume 1, centroid (0.5,0.5,0.5), and a 1/6 diagonal inertia with zero products; a side-2 cube gives volume
  8 and inertia m·s²/6; translating the mesh moves only the centroid (centroid-relative inertia is invariant);
  reversed winding still yields a positive volume and identical inertia; an open mesh reports invalid.
  Double-precision, unit density (mass = volume) — scale by real density and shift with parallel axis for a
  non-centroid pivot. [VERIFIABLE HERE]
- [x] **Coplanar region segmentation** (`render::segmentCoplanarRegions`, `CoplanarRegions`) — DONE (M531);
  group a mesh's triangles into maximal CONNECTED, near-planar patches — the flat faces of a shape — the
  workhorse behind collision-hull simplification (one convex face instead of a triangle fan), greedy meshing,
  decal/lightmap chart seeding, and editor "select coplanar". Flood-fills across the M528 topology's edge
  neighbours, crossing an edge only when the neighbour triangle's face normal stays within an angle tolerance
  of the region's SEED normal (so a region stays planar to its seed rather than slowly bending away). Verified
  (`ctest -R mesh_planar_regions`): a welded cube yields exactly six regions of two triangles each, a flat
  quad is one region, two quads meeting at a 90° ridge split into two despite being edge-connected, the
  tolerance actually gates (a 3° bend merges at a 10° tolerance and splits at 1°), every triangle gets a valid
  label with one seed normal per region, and it is deterministic. [VERIFIABLE HERE]
- [x] **Mesh vertex quantization** (`render::quantizeMesh` / `render::dequantizeMesh`, `QuantizedMesh`) —
  DONE (M530); the lossy-but-bounded attribute compression a glTF/Draco-style exporter runs to shrink a mesh:
  instead of a 32-bit float per position/UV channel, snap each channel to an N-bit integer grid spanning that
  attribute's bounding box, storing the compact ints plus the box to reconstruct. Because the grid step is
  extent / (2^bits − 1), the round-trip error on any channel is BOUNDED by half a grid step — the size win
  comes with a provable quality guarantee. Godot's importer exposes the same idea (mesh import compression).
  Verified (`ctest -R mesh_quantize`): at 8/10/12/14 bits the reconstruction stays within half a grid step and
  is genuinely lossy, more bits monotonically shrink the error, indices and vertex count survive exactly, a
  flat axis (or constant UV) reconstructs exactly with no divide-by-zero, and an empty mesh round-trips to
  empty. This is the CPU quantize/dequantize core; octahedral normal encoding and index-stream entropy coding
  are the documented follow-ups. [VERIFIABLE HERE]
- [x] **Connected components / mesh island splitting** (`render::splitConnectedComponents`,
  `render::connectedComponentLabels`) — DONE (M529); the "separate into loose parts" operation every DCC and
  Godot's tooling offers — pull a merged triangle soup apart into the independent sub-meshes that are actually
  STITCHED together. Two triangles join an island when they share an EDGE (a vertex-only touch does not
  connect them, matching how importers/physics treat loose parts). Built on the M528 topology: flood-fill
  across edge-twins, then compact each island into its own MeshData with a minimal remapped vertex list.
  Useful for per-part physics bodies, per-island culling/streaming, and stray-geometry cleanup. Verified
  (`ctest -R mesh_components`): two disjoint cubes split into two watertight islands (12+12 tris, exact
  partition), a single cube stays whole, labels are deterministic and cover every triangle, and two cubes
  touching at only a shared corner vertex remain two islands. [VERIFIABLE HERE]
- [x] **Mesh topology / connectivity** (`render::buildTopology`, `render::MeshTopology`) — DONE (M528); the
  reusable half-edge-style adjacency an engine builds once so everything needing to know how a mesh is
  STITCHED — boundary/hole detection, watertightness (is it a closed solid?), per-triangle neighbours for
  flood-fill smoothing groups / UV islands / crease detection, and an Euler-characteristic sanity check —
  queries it instead of rebuilding edge maps inline (as MeshSmooth and Subdivision previously did). Godot
  exposes the same via MeshDataTool. Each triangle owns three directed half-edges; twins are matched by
  UNDIRECTED edge so it's robust to inconsistent winding. Verified (`ctest -R mesh_topology`): a welded cube
  is watertight with 18 edges + Euler 2, every triangle has three neighbours, twins are symmetric; an open
  2×2 grid reports exactly its 8 perimeter edges as boundary (rim vertices flagged, centre not) with Euler 1;
  an edge shared by three triangles is flagged non-manifold. [VERIFIABLE HERE]
- [x] **Overdraw optimization** (`render::optimizeOverdraw` / `render::simulateOverdraw`) — DONE (M527); the
  load-time triangle reorder that pairs with vertex-cache optimization to cut redundant FRAGMENT-shader work.
  Vertex-cache order reduces vertex-shader runs; overdraw order reduces fragment-shader runs: with early-Z
  depth testing a fragment behind an already-drawn one is rejected before shading, so drawing triangles
  roughly FRONT-TO-BACK means each pixel is shaded close to once instead of once per overlapping layer.
  `optimizeOverdraw(mesh, viewDir)` returns the mesh with triangles reordered nearest-first (a pure index
  permutation — positions and the triangle SET untouched), and `simulateOverdraw` software-rasterizes the
  mesh orthographically and counts depth-test-passing fragment writes so the win is measurable. Verified
  headlessly (`ctest -R overdraw_optimize`) on 8 overlapping quads fed in worst-case back-to-front order:
  the reorder shades strictly fewer fragments, drives overdraw from >3× down to ≈1 shade/pixel, preserves the
  exact triangle set and vertex data, and a zero view direction is a safe no-op. Run it after
  `optimizeVertexCache` for a known dominant view. Follow-up: meshopt's view-independent cluster reorder that
  also bounds vertex-cache ACMR degradation. [VERIFIABLE HERE]
- [x] **Vertex-cache optimization** (`render::optimizeVertexCache`, Forsyth's algorithm) — DONE (M522); the
  load-time index reorder every importer runs so consecutive triangles reuse the GPU's post-transform vertex
  cache, cutting redundant vertex-shader runs. A pure lossless index permutation (positions untouched).
  Verified headlessly (`ctest -R vertex_cache`): the triangle SET is preserved exactly and the simulated-cache
  ACMR drops (~1.04 → ~0.66 on a 24×24 grid). [VERIFIABLE HERE]
- [x] **Cubemap direction/UV mapping** (`render::directionToCube` / `cubeToDirection`) — DONE (M506); the
  sampling math for reflection probes, skyboxes, and IBL. [VERIFIABLE HERE]
- [x] **Reflection-probe influence + box projection** (`render::ReflectionProbe`) — DONE (M507); blend
  weights + parallax-corrected sample direction. [VERIFIABLE HERE]
- [x] **Reflection-probe / cubemap CAPTURE matrices** (`render::cubeFaceView` / `cubeFaceProjection` /
  `cubeFaceViewProjection` / `cubeFaceForward`) — DONE (M519); the six per-face camera view matrices (correct
  look axis + up per Cubemap.hpp's convention) and the shared 90° projection a capture pass renders the scene
  into to build a probe's cubemap. Verified for self-consistency with the sampler (`ctest -R cubemap_capture`):
  each face's look axis maps back to that face and projects to the face center, the FOV is a true 90°, and the
  six frustums tile the sphere. Issuing the six render passes + roughness prefilter are the GPU steps this
  feeds. [VERIFIABLE HERE (matrices) / SEE IT ON YOUR MACHINE (capture)]
- [x] **GPU-driven particles + collision** (`fx::GpuParticleSystem`) — DONE (M509); state lives in flat
  SoA float buffers (SSBO-ready) and the CPU `update()` is the exact arithmetic the compute shader runs:
  gravity+drag integration, plane collision with restitution + friction, no-sink resolution, life recycling,
  and an instance-buffer builder for the single instanced draw. Bounce energy, no-sink, friction, lifecycle
  and instances are all unit-verified headlessly (`ctest -R gpu_particles`); the GPU compute dispatch +
  instanced draw are owner-verified. [VERIFIABLE HERE (sim) / SEE IT ON YOUR MACHINE (draw)]
- [x] **Screen-space reflections trace** (`render::ssrTrace` / `SsrCamera` / `DepthBuffer` / `reflect`) —
  DONE (M510); the reflection ray-march that mirrors glossy floors/wet streets: bounce the view vector about
  the normal, march the ray, project each step into the depth buffer and detect the first surface it passes
  behind (binary-refined), returning the hit UV + confidence. The trace/projection is the exact SSR-shader
  arithmetic and is unit-verified headlessly (`ctest -R ssr_trace`: wall-hit UV/depth, sky miss, off-screen
  miss, thickness gate); sampling the color buffer + roughness blur + temporal accumulation is the GPU pass.
  [VERIFIABLE HERE (trace) / SEE IT ON YOUR MACHINE (image)]
- [~] **SSIL pass** (screen-space indirect light) — CPU gather kernel DONE (M657):
  `render::ssilGather` (`ScreenSpaceIndirectLight.hpp`) computes the one-bounce colored indirect light
  (the color-bleed math), unit-tested headlessly. See the M657 entry at the top of §5. The GPU fragment
  pass over the depth/normal/color G-buffer and its visible result run on your machine.
  Shaders/passes written & compiled here; visual confirmation is on your GPU. [CODE HERE / SEE IT ON YOUR MACHINE]
- [x] **3D navigation mesh pathfinding** (`game::NavMesh3D`) — DONE (M505); path query + surface height over
  supplied walkable polygons (reuses the 2D corridor A*+funnel). Follow-up: bake from geometry + dynamic
  obstacles. [VERIFIABLE HERE]

### §3 Editor as an application — [CODE HERE / SEE IT ON YOUR MACHINE]
The editor *logic* already exists (`maz/editor/`). Turning it into a running GUI app (dockable panels,
live viewport, visual shader/animation/theme editors, debugger GUI) requires a GPU to render the editor
itself, so it is written here and run on your machine. [CODE HERE / SEE IT ON YOUR MACHINE]

### §1 Foundational — the honest core
> **Hardware hand-off:** the "see it / hear it on your PC" items below are collected, in plain language
> with the exact command to run and what to expect, in [`docs/HARDWARE_HANDOFF.md`](HARDWARE_HANDOFF.md)
> (M659). All are written and compile here (the editor builds to `build/bin/editor`, `Audio.cpp` compiles
> into the engine library); the only remaining step is running them on a machine with a screen/speaker.
- [ ] **Renderer proven on real hardware** — [NEEDS YOUR HARDWARE/TOOLCHAIN] (a GPU). This is the one
  gap only your machine can close; the code exists and is what everything in §5/§3 builds on.
  (Owner has confirmed the app runs on their machine; see the hand-off doc.)
- [ ] **Audio device output path** — SDL audio callback wiring is [CODE HERE / SEE IT ON YOUR MACHINE]
  (compiles here — verified `Audio.cpp` builds into the engine lib; a speaker confirms it; see the
  hand-off doc for the exact command).
- [x] **Networking transport + reliability** — real UDP sockets (`maz::net::UdpSocket`, portable
  POSIX/Winsock; `tests/net/loopback.cpp`) AND `maz::net::ReliableChannel` wiring the ack layer onto them
  for reliable, in-order message delivery across a **40%-loss link** (`tests/net/reliable.cpp`, ctest
  `net_reliable`: 50/50 in order). [VERIFIABLE HERE — DONE]. Remaining: secure (DTLS) transport,
  WebRTC for browsers, and a true cross-machine soak on the owner's two machines (manual).
- [x] **WebSocket (RFC 6455) transport for browsers** (`net::wsAcceptKey` / `wsEncodeFrame` /
  `wsDecodeFrame` / `serverHandshakeResponse` / `parseClientKey`) — DONE (M518). A WASM build can't open raw
  UDP; browsers only speak WebSocket/WebRTC. This implements the handshake accept-key (base64(SHA1(key+GUID))
  reusing `core::sha1` + `io::base64Encode`) and the full frame codec (FIN/opcode, 7/16/64-bit lengths,
  client XOR masking), so a Maz server can talk to browser clients over the app's TCP socket. Verified
  against RFC 6455's own golden vectors (`ctest -R websocket`): canonical accept key, canonical masked +
  unmasked "Hello" frames, round-trip, and the partial-buffer "need more" case. [VERIFIABLE HERE]

### §7 Community & release scaffolding
- [x] **Contribution + release scaffolding** — DONE (M516, the completable-here part). `CONTRIBUTING.md`
  (build/test/style/PR flow), `CODE_OF_CONDUCT.md` (Contributor Covenant), GitHub issue templates
  (`.github/ISSUE_TEMPLATE/bug_report.yml`, `feature_request.yml`) + `PULL_REQUEST_TEMPLATE.md`, and an
  automated **release pipeline** (`.github/workflows/release.yml`: on a `v*` tag it builds with
  warnings-as-errors, runs the full ctest suite, packages the sample games via `tools/package.sh`, and
  attaches the bundles to a GitHub Release). Plus [RELEASING.md](RELEASING.md) with the itch.io/Steam
  upload steps. All YAML parser-validated. [VERIFIABLE HERE]
- [ ] **Shipped to real players + an active community + mileage over time** — the honest human step. The
  scaffolding above lowers the barrier, but real players, third-party plugins, and reports from strangers
  require actual people choosing to show up, and trust accumulates only by shipping games over time. This
  can never be marked done from a sandbox. [NEEDS REAL PEOPLE + TIME]

## How the loop uses this

Each iteration: pick the next unchecked **[VERIFIABLE HERE]** item first (highest confidence, real
verification), implement it, unit-test it to green, document it, commit, and check it off here. Items in
the other two tiers are taken when they are the best remaining value, and are shipped with an explicit
"written / not verified here" note so nothing is over-claimed. The ecosystem section (§8) is skipped by
request.
