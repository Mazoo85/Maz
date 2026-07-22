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
- [ ] **Ogg Vorbis / MP3 decode to PCM** — large, patent-adjacent pure-CPU decoders feeding the existing
  mixer; QOA (above) already covers the compressed-audio need dependency-free. Optional follow-up. [VERIFIABLE HERE]
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
- [ ] **Basic complex-script shaping hooks** (mark positioning, ligature substitution tables). [VERIFIABLE HERE]
- [ ] **Localization tooling** — POT/PO extract + import beyond the current CSV tables. [VERIFIABLE HERE]
- [ ] **Video container/codec decode** to frames (display is GPU-side). [CODE HERE / SEE IT ON YOUR MACHINE]

### §6 Scripting & language — [VERIFIABLE HERE]
- [ ] **C# / .NET-style second binding** OR deepen the existing script VM toward GDScript-grade tooling
  (autocomplete data, doc tooltips, live debug protocol). [VERIFIABLE HERE]
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
- [ ] **Desktop export/packaging** — extend `tools/package.sh` into a real per-OS bundler
  (assets + launcher + config). [VERIFIABLE HERE] (the packaging logic; running the packaged game is manual)
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
- [ ] **SSIL pass** (screen-space indirect light).
  Shaders/passes written & compiled here; visual confirmation is on your GPU. [CODE HERE / SEE IT ON YOUR MACHINE]
- [x] **3D navigation mesh pathfinding** (`game::NavMesh3D`) — DONE (M505); path query + surface height over
  supplied walkable polygons (reuses the 2D corridor A*+funnel). Follow-up: bake from geometry + dynamic
  obstacles. [VERIFIABLE HERE]

### §3 Editor as an application — [CODE HERE / SEE IT ON YOUR MACHINE]
The editor *logic* already exists (`maz/editor/`). Turning it into a running GUI app (dockable panels,
live viewport, visual shader/animation/theme editors, debugger GUI) requires a GPU to render the editor
itself, so it is written here and run on your machine. [CODE HERE / SEE IT ON YOUR MACHINE]

### §1 Foundational — the honest core
- [ ] **Renderer proven on real hardware** — [NEEDS YOUR HARDWARE/TOOLCHAIN] (a GPU). This is the one
  gap only your machine can close; the code exists and is what everything in §5/§3 builds on.
- [ ] **Audio device output path** — SDL audio callback wiring is [CODE HERE / SEE IT ON YOUR MACHINE]
  (compiles here; a speaker confirms it).
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
