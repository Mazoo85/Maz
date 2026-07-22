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
