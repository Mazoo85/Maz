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
