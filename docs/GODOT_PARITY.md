# Maz vs Godot — complete parity plan

The goal: make the Maz Engine **equal to or better than Godot in every way**. This is the honest,
complete list of what that takes — what already matches or beats Godot, every remaining gap, and a
phased plan to close them. Each gap is tagged with how it can be verified in this project:

- **[CPU]** — has a pure, deterministic core we can implement and unit-test with no GPU (done here).
- **[GPU]** — needs a real graphics device to build/see; buildable in CI, visually verified only on
  real hardware.
- **[DESK]** — needs a real desktop OS surface (window manager, drivers).
- **[BIG]** — a large multi-milestone subsystem, not a single task.

> **Honest stance:** Maz is a from-scratch native C++20 engine. In the systems that are pure logic
> (math, physics solvers, animation, audio DSP, AI/pathfinding, ECS, serialization, tooling) it is
> already at or beyond Godot parity and fully unit-tested. Godot's remaining structural advantages
> are almost all **GPU-heavy rendering** and **breadth of platform/editor/ecosystem** — years of
> work by a large team. This plan does not pretend those are done; it sequences them.

---

## Part A — Where Maz already meets or beats Godot

These are implemented and tested in-tree (see `docs/ROADMAP.md` for the Mxxx milestone tags):

- **Core / math / containers:** vectors/matrices/quats, **Vector2/Vector3 gameplay helpers** (M267,
  `math::VectorOps` — Godot's move_toward / slide / bounce / reflect / limit_length / direction_to /
  angle_to / project / posmod / snapped / rotated / orthogonal (M326 adds Vector2.orthogonal —
  the 90-degree-clockwise perpendicular (y,-x), verified perpendicular + length-preserving + double-
  application-negates) (M323 adds the Vector3 overload rotated(v,axis,angle)
  — Godot's Vector3.rotated via Rodrigues' formula, cross-checked against the quaternion axis-angle
  path) (M341 adds fromAngle(radians) — Godot Vector2.from_angle, the unit vector (cos,sin) that
  inverts angle(), plus a cross(vec2,vec2) alias giving Godot's exact Vector2.cross spelling for the
  existing 2D scalar cross; both verified) (M342 adds the scalar-bound component ops clampf / minf /
  maxf / snappedf for vec2 and vec3 — Godot's Vector2/Vector3.clampf/minf/maxf/snappedf: apply one
  float to every component; deliberately distinct names so they never ADL-collide with GLM's
  vector clamp/min/max, verified component-wise) (M343 adds lengthSquared / distanceTo /
  distanceSquaredTo / lerp for vec2 and vec3 — Godot's Vector2/Vector3.length_squared /
  distance_to / distance_squared_to / lerp (component-wise, extrapolates past [0,1]); verified on
  3-4-5 / 2-3-6 triangles and lerp endpoints/midpoint/extrapolation) (M334 adds isFinite(vec2)/isFinite(vec3) — Godot's Vector2/Vector3.is_finite, true only when
  every component is finite, verified to reject a single NaN or inf component) (M335 adds
  isEqualApprox / isZeroApprox / isNormalized for vec2/vec3 — Godot's Vector2/Vector3.is_equal_approx
  (per-component relative CMP_EPSILON), is_zero_approx, and is_normalized (squared length within the
  looser absolute UNIT_EPSILON = 0.001, matching Godot bit-for-bit), verified against normalized and
  un-normalized inputs), plus **vector slerp** (M301 — `math::slerp` for
  vec2/vec3: arc-interpolate direction while lerping length, Godot's Vector2/Vector3.slerp, with
  lerp fallback for zero-length/colinear inputs), plus **octahedral normal encoding** (M302 —
  `math::octahedronEncode` / `octahedronDecode`, Godot's Vector3.octahedron_encode/decode: pack a unit
  normal into a vec2 for G-buffers / compressed vertex data, round-trips to ~1e-3), plus **cubic/bezier interpolation** (M279 —
  cubicInterpolate Catmull-Rom, bezierInterpolate, bezierDerivative for scalars + vec2/vec3, matching
  Godot's Vector2/3.cubic_interpolate / bezier_interpolate / bezier_derivative; M320 adds
  cubicInterpolateInTime — the time-parametrised (non-uniform) Catmull-Rom via the Barry-Goldman
  pyramid, Godot's cubic_interpolate_in_time, verified to collapse onto the uniform cubicInterpolate
  when the sample times are evenly spaced), semantics verified
  against Godot's own source so GDScript-ported logic behaves identically), Transform2D (incl. **apply-relative methods** M278 —
  translated/rotated/scaled with global & _local variants matching Godot's Transform2D; M328 adds
  basis_xform_inv — the transposed-basis direction transform, exact inverse of basis_xform for an
  orthonormal basis; M339 adds is_equal_approx (both basis columns + origin approx) and is_finite —
  Godot Transform2D.is_equal_approx / is_finite, verified against nudged/differing transforms and a
  non-finite basis component), **Transform3D** (M268,
  `math::Transform3D` — Godot's core Basis+origin spatial transform: xform / xform_inv, compose with
  `*`, affine + rigid inverse, translated/rotated/scaled with global & _local variants, orthonormalized,
  looking_at (-Z forward), interpolate_with (translation lerp + rotation slerp + scale lerp),
  basis_xform_inv (M328 — transposed-basis direction transform, Godot Basis.xform_inv),
  is_equal_approx / is_finite (M339 style, added M340 — every basis column + origin approx-equal /
  all-finite, Godot Transform3D.is_equal_approx / is_finite, verified against nudged/differing
  transforms and non-finite basis/origin components), and
  mat4 interop; semantics matched to Godot's Transform3D/Basis source), **Quaternion** (M269,
  `math::Quaternion` — Godot's rotation quaternion: axis-angle + Euler build/read in Godot's exact
  YXZ convention (from_euler/get_euler), xform, compose, inverse, slerp, angle_to, and mat3 interop,
  so orientations authored in Godot import identically; M306 adds Godot's shortest-arc two-vector
  constructor Quaternion(v0,v1) — the rotation taking one direction onto another — plus get_axis /
  get_angle, verified by rotating the source onto the target and by axis-angle round-trip; M319
  adds Godot's slerpni (spherical interpolation WITHOUT the shortest-path flip, so a caller-chosen
  winding is never silently reversed) plus log / exp — the quaternion logarithm/exponential that map
  a unit rotation to/from its axis*angle rotation vector and form the basis of quaternion spline
  interpolation. Verified as mathematical identities: slerpni endpoints and short-arc agreement with
  slerp, exp(log(q)) == q on unit quaternions, log of a 1-rad rotation == the pure axis quaternion,
  and exp of the zero vector == identity; M336 adds the validation predicates is_finite /
  is_equal_approx / is_normalized plus length_squared — Godot's Quaternion.is_finite/is_equal_approx/
  is_normalized: guard interpolation and physics state against NaN/inf orientations, compare
  orientations up to float rounding, and confirm a quaternion is a valid unit rotation (squared
  length within Godot's absolute UNIT_EPSILON = 0.001) before it is used as one, all verified),
  **Euler-order conversion** (M307, `math::basisFromEuler` / `basisGetEuler` with `EulerOrder` —
  Godot's Basis.from_euler / get_euler across ALL SIX rotation orders XYZ/XZY/YXZ/YZX/ZXY/ZYX
  (Godot's Node3D.rotation_order), so non-default rotation orders import/export identically; each
  order builds R as the ordered product of elementary rotations and get_euler inverts it with a
  gimbal-lock branch. Verified by round-tripping all six orders including gimbal lock, single-axis
  identities, and cross-checking YXZ against Quaternion::fromEuler),
  **Projection** (M292, `math::Projection` —
  Godot's 4x4 projection-matrix type: perspective/orthographic/frustum constructors (RH, depth 0..1)
  plus the near/far/fov/aspect/is-orthogonal queries recovered from the matrix, xform/project,
  compose and inverse), **scalar math helpers** (M295, `math::MathFuncs` — Godot's @GlobalScope
  numeric utilities: degToRad/radToDeg, lerpf/inverseLerp/remap, wrapf/wrapi, smoothstep, ease,
  moveTowardf, lerpAngle (shortest-arc), pingpong, snappedi, nearestPo2, isEqualApproxf; M311 adds
  angleDifference and rotateToward — Godot 4.2's @GlobalScope.angle_difference / rotate_toward for
  shortest-arc angle deltas and overshoot-free angular steering, verified across the +/-pi seam;
  M330 adds cubicInterpolateAngle — Godot's @GlobalScope.cubic_interpolate_angle: a Catmull-Rom
  between two angles that remaps the control angles to the nearest equivalent so interpolation crosses
  the +/-pi wrap the short way, verified to hit its endpoints, reduce to the plain cubic for in-range
  angles, and take the forward-through-zero path from 350deg to 10deg; M334 adds isFinitef/isNanf/
  isInff — Godot's @GlobalScope.is_finite / is_nan / is_inf scalar predicates for guarding physics
  and animation state against NaN/inf propagation after a bad divide or blow-up; M346 adds
  stepDecimals — Godot's @GlobalScope.step_decimals: the decimal-place count implied by a step value
  (0.01->2, 0.001->3, 1->0), via Godot's exact epsilon-guarded lookup table, for picking display
  precision; verified against integer/fractional/negative steps and the 10-decimal cap; M347 adds the
  scalar clampf / clampi — Godot's @GlobalScope.clampf / clampi (clamp a float / 64-bit int into a
  range), coexisting unambiguously with the vector clampf via scalar-first overloading, verified;
  M349 adds the integer posmod — Godot's @GlobalScope.posmod: positive modulo whose result carries the
  sign of the divisor (posmod(-1,3)==2), for wrapping possibly-negative tile/array indices, verified
  across positive/negative operands and full index ranges),
  **Vector2i/Vector3i** (M270,
  `math::VectorInt` — Godot's integer vectors for tile/grid coords, indices and pixel sizes: exact
  arithmetic with truncating integer division, abs/sign, clamp/min/max, overflow-safe 64-bit
  lengthSquared, length/distance, aspect, and float-vec conversion; M352 adds snapped(step) — Godot
  Vector2i/Vector3i.snapped: component-wise snap to a multiple of a step (round half away from zero,
  step 0 leaves the component), verified incl. negatives and step 0), **Vector4/Vector4i** (M286,
  `math::Vector4` — Godot's 4D vectors: float Vector4 with the full gameplay surface
  (length/normalized/dot/lerp/abs/sign/clamp/min/max/floor/ceil/round/snapped/distanceTo/directionTo/
  isEqualApprox) for RGBA/shader-uniform/homogeneous math, plus the exact integer Vector4i with
  truncating division and overflow-safe 64-bit lengthSquared; GLM `toVec4()` bridge), Rect2 (M326
  adds grow_side — grow/shrink a single edge via a Side enum L/T/R/B, verified per-side and that
  growing all four equals uniform grow; M338 adds is_equal_approx (component-wise approx of position
  AND size) and is_finite (every component finite) — Godot Rect2.is_equal_approx / is_finite, verified
  against nudged/differing rects and NaN/inf components), **Rect2i** (M281,
  `math::Rect2i` — Godot's integer rectangle for tile/atlas/pixel regions: half-open hasPoint,
  intersects/intersection/merge/encloses/grow/expand/abs, exact int math; M353 adds growSide
  (Godot Rect2i.grow_side — grow/shrink one edge, reusing Rect2's shared Side enum L/T/R/B),
  verified per-edge and that growing all four equals a uniform grow), Geometry2D/3D
  (incl. **polygon toolkit** M271, `math::convexHull` (Andrew's monotone chain), signed
  `polygonArea`, `isPolygonClockwise` (Godot's Y-down screen convention), area-weighted
  `polygonCentroid` — Godot's Geometry2D polygon statics; plus **more Geometry2D statics** M276 —
  closestPointOnLine (uncapped), lineIntersectsLine (infinite lines), pointInTriangle,
  closestPointsBetweenSegments (2D) matching Godot's Geometry2D; plus **convex polygon clipping** M296
  — `clipPolygonConvex` (Sutherland–Hodgman: clip a polygon to a convex region — viewport/FOV/scissor),
  the convex case of Godot's Geometry2D.clip_polygons/intersect_polygons (general Clipper boolean ops
  remain out of scope); plus **segment-vs-rect clipping** M303 — `clipSegmentToRect` (Liang–Barsky:
  clip a segment to an axis-aligned rectangle for viewport/bounds clipping of lines and rays); **Aabb3 method completeness** M272 —
  encloses / intersection / grow / expand / abs / longest-shortest-axis / intersectsSegment toward
  Godot's AABB; M324 adds `intersectsPlane` (Godot AABB.intersects_plane) — true when the box
  straddles a plane, faithfully reproducing Godot's asymmetric touch rule (a zero-distance corner
  counts as "under", so a box touching from the positive side intersects but one touching from below
  does not), tested via the two ±normal support corners; M318 adds the axis-vector forms `longestAxis`/`shortestAxis` (Godot get_longest_axis /
  get_shortest_axis, ties to the earliest axis) and `endpoint(i)` for the 8 corners
  (get_endpoint), verified against known corners and containment; M339 adds `isEqualApprox`
  (position + size approx) and `isFinite` — Godot AABB.is_equal_approx / is_finite, verified against
  nudged/differing boxes and a NaN component); **Color completeness** M273 (`render::blend` alpha compositing, `clampColor`,
  `isEqualApprox`, 32-bit pack/unpack `toRgba32`/`toArgb32`/`toAbgr32`/`fromRgba32`, `color8`; M331
  adds 64-bit (16-bit-per-channel) `toRgba64`/`fromRgba64` — Godot's Color.to_rgba64 / Color.hex64
  for high-bit-depth packing, verified by known values, clamping, and a round-trip that preserves a
  fine difference 8-bit would collapse; plus
  **named colours** M288 (`render::namedColor` / `colorFromString` — the full CSS3/Godot named-colour
  palette, 146 constants byte-for-byte, forgiving name lookup like Godot's, matching Godot's Color
  constants + Color.from_string) —
  Godot's Color.blend/clamp/is_equal_approx/to_*32/Color8),
  curves, easing, two RNGs (xoshiro + PCG32), SlotMap, SmallVector/SparseSet, RingBuffer, string
  interning, reflection, JSON/CSV/XML/base64/INI, binary + text serialization, resource packs,
  virtual filesystem, **hashing** (M284, `core::sha256` / `sha256Hex` / `crc32` — Godot's
  HashingContext / crc32 for asset integrity, save checksums, content-addressed caches and network
  digests; verified against the published SHA-256 / CRC-32 test vectors; plus **SHA-1** M299,
  `core::sha1` / `sha1Hex` — Godot's HashingContext HASH_SHA1 (content integrity, WebSocket handshake),
  verified against the standard vectors; plus **MD5** M300, `core::md5` / `md5Hex` — Godot's
  HashingContext HASH_MD5 (legacy non-security checksums), RFC 1321 vectors verified — completing
  Godot's MD5/SHA-1/SHA-256 HashingContext trio; plus **HMAC-SHA256** M298,
  `core::hmacSha256` — keyed message authentication for signed saves / tamper-proof network messages /
  API tokens, Godot's Crypto.hmac_digest, verified against the RFC 4231 vectors), **endian-aware byte stream** (M277, `io::StreamPeerBuffer` — Godot's
  StreamPeerBuffer: fixed-width little/big-endian put/get for u8..u64 signed+unsigned, float, double,
  length-prefixed strings, safe past-end reads; for network protocols and portable binary formats),
  semver, deterministic time, replay, checkpoints, profiler, **RNG distribution
  helpers** (M275, `core::Pcg32::rangef`/`gaussian`/`weighted` — Godot RandomNumberGenerator's
  randf_range / randfn Box-Muller normal / rand_weighted; distribution-verified over 200k samples),
  **performance
  budgets** (a formal alert layer Godot lacks), job system, **string utilities** (M265,
  `core::StringUtils` — Godot String's split/join/strip_edges/lpad-rpad/replace/begins-ends-with/
  contains/to_lower-upper/repeat/count (M329 adds character-set lstrip(chars)/rstrip(chars) overloads
  — Godot String.lstrip/rstrip taking the exact set of characters to strip from each end; M333 adds
  left(n)/right(n) — Godot String.left/right with negative-index semantics (a negative count drops
  that many characters from the far end), verified including recombination at every split point),
  plus **number parsing** M274 — toInt/isValidInt/toFloat/
  isValidFloat/hexToInt matching Godot String's to_int/is_valid_int/to_float/is_valid_float/hex_to_int
  (M327 adds binToInt — Godot String.bin_to_int: binary parse with optional 0b prefix + sign, halting
  at the first non-binary digit, verified against powers of two and edge cases)
  (M337 adds validation helpers — Godot String.is_valid_identifier (letter/underscore lead, then
  alnum/underscore), is_valid_html_color (optional # then 3/4/6/8 hex digits — RGB/RGBA/RRGGBB/
  RRGGBBAA), and is_subsequence_of / is_subsequence_ofn (case-insensitive) for fuzzy in-order matching;
  all verified against valid + malformed inputs)
  (M344 adds trimPrefix / trimSuffix — Godot String.trim_prefix / trim_suffix: strip a prefix or
  suffix ONLY when it is actually present (returns the string unchanged otherwise), verified on
  res://user:// scheme + extension stripping and no-op cases)
  (M345 adds indent / dedent — Godot String.indent (prefix every non-empty line, leaving truly empty
  lines alone) and String.dedent (strip ALL leading spaces/tabs per line, Godot's per-line rule, not
  the common-minimum), verified incl. an indent→dedent round-trip)
  (M350 adds getSlice / getSliceCount — Godot String.get_slice / get_slice_count: index-based access
  to the Nth piece when split on a (possibly multi-char) delimiter, without building the whole array;
  empty pieces preserved, out-of-range/negative/empty returns "" / 0, verified)
  (M351 adds countN / findN — Godot String.countn / findn: case-insensitive (ASCII) occurrence count
  and first-match search from an offset (npos if none), verified against mixed-case inputs;
  M354 adds rfind / rfindN — Godot String.rfind / rfindn: reverse search returning the LAST match
  at/before a position (npos default = whole string, matching Godot's -1 "from the end"), verified
  against strings with two occurrences and case-insensitive inputs;
  M355 adds validateNodeName — Godot String.validate_node_name: removes the six characters Godot
  forbids in SceneTree node names ('.', ':', '@', '/', '"', '%'), all other chars incl. spaces kept,
  verified against mixed/clean/all-forbidden inputs; M360 adds insert / erase — Godot String.insert /
  String.erase: insert a substring at a byte index (negative pos unchanged, past-end clamps to append)
  and remove N chars at a byte index (pos/count clamped, out-of-range removes nothing), round-trip
  verified; M356 adds posmodv (vec2/vec3) — Godot
  Vector2/Vector3.posmodv: per-component positive modulo with a per-component modulus vector,
  complementing the scalar-modulus posmod, verified per-component incl. negative modulus sign;
  M357 adds Vector2i/Vector3i maxAxisIndex / minAxisIndex — Godot Vector2i/Vector3i.max_axis_index /
  min_axis_index (axis index of the largest/smallest component), with Godot's exact tie-breaking
  (max: earliest axis wins; min: latest axis wins), verified incl. all-equal components; M358 adds
  intToBase (StringUtils) — Godot String.num_int64: integer→text in an arbitrary base 2..36 (0-9 then
  a-z or A-Z), negatives prefixed '-', bases out of range clamp to 10, INT64_MIN handled via unsigned
  magnitude, verified across bases/signs; M361 adds Vector4 isFinite / isZeroApprox — Godot
  Vector4.is_finite / is_zero_approx (all components finite / within epsilon of zero), completing the
  is_finite family across vec2/vec3/vec4/quat, verified against inf/NaN and tiny/nonzero components;
  M362 adds float vec2/vec3 maxAxisIndex / minAxisIndex — Godot Vector2/Vector3.max_axis_index /
  min_axis_index (float companions to the M357 integer versions), using Godot's exact nested-ternary
  form so tie-breaking matches its float type (all-equal → max X, min Z), verified; M363 adds
  Vector4 maxAxisIndex / minAxisIndex — Godot Vector4.max_axis_index / min_axis_index (max scans
  strict '>' earliest-wins, min scans '<=' latest-wins), completing the axis-index family across
  2i/3i/vec2/vec3/Vector4, verified incl. all-equal; M359 adds Rect2.getSupport — Godot Rect2.get_support: the
  rectangle corner farthest along a direction (per axis max edge when dir>0 else min; dir==0 picks min,
  matching Godot's strict >0), the GJK/SAT broadphase primitive, verified all quadrants + zero-axis),
  plus **path helpers** M285 — getExtension/getBasename/getFile/getBaseDir/pathJoin/simplifyPath
  matching Godot String's get_extension/get_basename/get_file/get_base_dir/path_join/simplify_path,
  plus **case conversion** M287 — capitalize/toSnakeCase/toCamelCase/toPascalCase matching Godot
  String's capitalize/to_snake_case/to_camel_case/to_pascal_case (Godot-style word splitting: breaks
  on separators and camelCase/acronym boundaries, acronyms normalized e.g. HTTPServer→HttpServer),
  plus **markup/URI escaping** M289 — xmlEscape/xmlUnescape (named + numeric char refs) and
  uriEncode/uriDecode (RFC 3986 unreserved set, '+' preserved) matching Godot String's
  xml_escape/xml_unescape/uri_encode/uri_decode, plus **fuzzy matching** M290 — bigrams + similarity
  (Sørensen–Dice bigram coefficient, matching Godot String's similarity) and a Levenshtein
  edit-distance utility (beyond Godot's String API),
  plus **wildcard glob matching** M317 — matchGlob (Godot String.match / matchn): `*` any run, `?`
  any single char except '.', case-sensitive or -insensitive, replicating Godot's exact recursion
  and quirks (empty pattern/subject -> false; `?` never matches a dot), verified across literal,
  star, question, extension-style, empty-guard and case cases,
  plus **number formatting** M305 — numToString/padDecimals/padZeros/humanizeSize matching Godot
  String's num/pad_decimals/pad_zeros and String.humanize_size, reproducing Godot's exact quirks
  (pad_decimals/pad_zeros are string surgery that truncate rather than round; humanize_size uses a
  strict `>` unit step so an exact 1024-multiple stays in the smaller binary unit),
  plus **C-string escaping** M308 — cEscape/cUnescape matching Godot String's c_escape/c_unescape
  (the backslash sequences Godot writes into text resources / C literals: \\ \a \b \f \n \r \t \v
  \' \", with c_unescape also accepting \?), verified to round-trip exactly),
  plus **split_floats** M325 — splitFloats(s, delim, allowEmpty) toward Godot String.split_floats:
  split on a delimiter and convert each token to a float via toFloat's leading-number rule (empty or
  non-numeric tokens become 0.0; allowEmpty=false drops empties before conversion). Verified on comma
  and custom delimiters, non-numeric tokens, empty-token handling, and single/empty-string edges,
  plus **natural-order comparison** M321 — naturalCompare/naturalCompareNoCase toward Godot
  String.naturalcasecmp_to/naturalnocasecmp_to: numeric-aware ordering where digit runs compare by
  value, so "file2" sorts before "file10" (plain lexicographic would not). Returns -1/0/+1, folds
  ASCII case for the nocase variant, orders equal numeric values by fewer leading zeros first, and
  places a digit before a letter at the same position. Verified by realistic filename sorting,
  antisymmetry across many pairs, and leading-zero/empty edge cases. (Honest scope note: this is the
  general numeric-aware comparison; Godot's extra leading-dot special case for hidden files is NOT
  reproduced.),
  **ISO-8601 date parsing** (M266, `core::parseIso` /
  `unixFromIso` — Godot Time's `get_datetime_dict_from_datetime_string` /
  `get_unix_time_from_datetime_string`: the inverse of the existing `formatIso`, accepts date-only or
  full datetime with T/space separator + optional Z, validates field ranges incl. leap-year days,
  round-trip + weekday verified),
  plus **Time string helpers** (M310, `core::formatDate` / `formatTime` / `formatDateTime(useSpace)` /
  `offsetString` — Godot Time's `get_date_string_from_unix_time` / `get_time_string_from_unix_time` /
  `get_datetime_string_from_unix_time(use_space)` / `get_offset_string_from_offset_minutes`:
  "YYYY-MM-DD", "HH:MM:SS", space- or T-separated datetime, and "+HH:MM" timezone offsets).
- **2D:** sprites/atlas/tilemaps, **isometric tile math** (M283, `game::IsoGrid` — diamond
  tile↔pixel conversion + neighbours toward Godot TileMap's Isometric layout, round-trip verified),
  **hex-grid math** (M280, `game::HexGrid` — axial coordinates:
  distance, six neighbours, pixel↔hex for pointy/flat-top, fractional rounding, hex line; the
  coordinate algebra behind Godot's TileMap hexagon layout and hex board games, round-trip verified),
  cameras, parallax, polygons (convex + concave ear-clip),
  polylines, multimesh, 2D lights + hard/soft/normal-mapped shadows, additive blending,
  **Delaunay triangulation** (M257, `math::triangulateDelaunay` — Bowyer-Watson, Godot's
  `Geometry2D.triangulate_delaunay`: empty-circumcircle-verified, robust double-precision predicate),
  **Voronoi cells** (M258, `math::voronoiCells` — the Delaunay dual, beyond Godot which has none;
  half-plane intersection clipped to a box, verified to partition the box and place each grid point in
  its nearest site's cell), **hex A* pathfinding** (M282, `game::hexFindPath` — shortest path across a
  hex grid with a blocked-cell predicate, admissible hex-distance heuristic, built on the M280 hex
  algebra; Godot's AStar2D needs every node/edge registered by hand, so this is a ready-made hex
  pathfinder), **Poisson-disk (blue-noise) sampling** (M259, `core::poissonDiskSample` —
  Bridson's algorithm, beyond Godot; deterministic min-distance scatter for natural object placement,
  verified min-distance + in-bounds + packing-bound + seed-determinism), **2D k-d tree** (M260,
  `core::KdTree2D` — balanced point index for nearest / k-nearest / radius queries, the standard
  structure for boids/RVO neighbour lists and waypoint snapping; median-split build, plane-pruned
  queries, verified against brute force), **Geometry3D segment/triangle/sphere helpers** (M262 —
  Godot's Geometry3D statics: closestPointToSegment, closestPointsBetweenSegments (skew lines),
  Moller-Trumbore rayIntersectsTriangle, segmentIntersectsTriangle, segmentIntersectsSphere for
  picking / line-of-sight / ballistics; M322 adds buildBoxPlanes and segmentIntersectsConvex —
  Godot's Geometry3D.build_box_planes / segment_intersects_convex: represent a convex volume as its
  outward-facing half-space planes and find where a segment first enters it (frustum / convex-region
  clipping and picking). Verified with straight-through, diagonal-corner, off-centre, miss,
  starts-inside (no entry, matching Godot) and stops-short cases; M332 adds
  closestPointToSegmentUncapped — Godot's Geometry3D.get_closest_point_to_segment_uncapped
  (projection onto the infinite line, no clamping) — plus Plane completeness has_point / get_center /
  normalized; M348 adds Plane is_equal_approx (normal + offset approx) and is_finite — Godot
  Plane.is_equal_approx / is_finite, verified against nudged/differing planes and non-finite
  normal/offset), **Curve3D + Path3D/PathFollow3D** (M263, `math::Curve3D` +
  `game::PathFollow3D` — 3D cubic-Bezier path with arc-length baking (Godot Curve3D) and constant-speed
  traversal with loop/clamp + forward tangent (Godot Path3D/PathFollow3D; rotation-mode/up-vector
  banking left to the caller); M315 adds `Curve2D::closestPoint` / `closestOffset` — Godot's
  Curve2D.get_closest_point / get_closest_offset for snapping a point onto the baked path and
  recovering its arc-length offset, verified against straight and L-shaped paths), **complete Tween transition set** (M264, `anim::Transition` — Godot's
  full 12 transition types (Linear/Sine/Quint/Quart/Quad/Expo/Elastic/Cubic/Circ/Bounce/Back/Spring)
  x 4 ease types (In/Out/InOut/OutIn) via the Penner equations, filling the gaps in the older partial
  `anim::Tween::Ease`; verified for endpoints, midpoints, known values, monotonicity, and overshoot).
- **3D rendering (CPU-verifiable parts):** PBR (Cook-Torrance GGX, metallic/roughness), analytic
  IBL, tonemap operators incl. **AgX**, bloom, SSAO, MSAA, fog, shadow mapping w/ PCF, frustum
  culling, instancing, billboards, procedural primitives, glTF scenes.
- **Physics:** full 2D solver (warm-started impulse, islands/sleeping, joints+motors, CCD, materials,
  many shapes) and a from-scratch **3D** solver (OBB SAT, capsules, SAP broadphase, character
  controller, joints). Parity-or-better with Godot's built-in 2D and competitive with Godot Physics
  3D.
- **Audio:** mixer, buses/bus graph, full DSP suite (EQ, reverb, delay, chorus/flanger/phaser,
  distortion modes, compressor, limiter), spatial 2D/3D, synth/oscillator, spectrum/FFT, WAV.
- **Animation:** skeletons + GPU-ready skinning, clips, blend spaces/trees, state machines,
  timelines, IK (2-bone, FABRIK), root motion, tweening.
- **AI/nav:** A* grid + AStar2D + AStar3D (M261, `game::AStar3D` — 3D weighted-graph twin of AStar2D:
  ids/positions, weights, one-/two-way links, id/point paths, closest-point / closest-in-segment) +
  **AStarGrid2D** (M291, `game::AStarGrid2D` — Godot's dense grid A*: solid/walkable cells, four
  DiagonalMode rules (never / only-if-no-obstacles / at-least-one-walkable / always) and four
  heuristics (Euclidean/Manhattan/Octile/Chebyshev)),
  navmesh, steering, flow fields, RVO avoidance, behavior trees +
  blackboard, GOAP planner.
- **Core value types:** a `core::Variant` (M293) — Godot's tagged any-value holding
  Nil/Bool/Int/Float/String/Vector2/Vector3 with Godot-style coercion (asInt/asFloat), truthiness
  (booleanize), value equality (numeric types compare across Bool/Int/Float) and stringify; with
  `core::varToStr` / `strToVar` (M314) — Godot's var_to_str / str_to_var round-trippable text encoding
  (the form written into .tres/.tscn): quoted+escaped strings, decimal-marked floats (so they never
  read back as ints), and `Vector2(x, y)` / `Vector3(x, y, z)` constructor syntax, with a parser that
  rejects malformed input; verified by known encodings and strToVar(varToStr(v)) == v round-trips;
  `core::arrayToStr` / `strToArray` and `dictToStr` / `strToDict` (M316) extend that to the container
  Variants — Godot's `[a, b, c]` and `{ "key": value }` forms with a quote- and paren-aware splitter
  so commas inside strings and inside `Vector2(x, y)` don't break elements (round-trip verified,
  malformed input rejected); plus the
  container Variants `core::Array` and `core::Dictionary` (M294) — Godot's ordered list
  (append/insert/find/slice/reverse) and ordered string->Variant map (has/get/set/erase/keys/values/
  merge, insertion order preserved); plus `core::formatWith` (M297) — Godot's String.format:
  `{0}`/`{1}` positional substitution from an Array and `{key}` named substitution from a Dictionary
  (values stringified via Variant; unknown placeholders left verbatim); plus `core::NodePath` (M309)
  — Godot's NodePath: parses "../Enemies/Boss:health:x" into an absolute flag, name components
  (split on "/", keeping "."/".."), and ":"-separated subnames, with get_name_count/get_name,
  get_subname_count/get_subname, is_absolute, get_concatenated_names/subnames, is_empty and exact
  string reconstruction (verified by round-trip); plus `core::Utf8` (M312) — UTF-8 <-> code-point
  conversion giving Maz Godot's code-point view of text: `utf8EncodeChar`/`utf8Encode` (String ->
  to_utf8_buffer), `utf8Decode` (parse_utf8), and `utf8Length` (String.length — code points, not
  bytes). Rejects overlong forms, surrogates and out-of-range values, and maps malformed bytes to
  U+FFFD with resync; verified by exact byte encodings, round-trips, and malformed-input handling;
  plus `core::stringHash32` / `stringHash64` (M313) — Godot's String.hash / hash64 (djb2, seed 5381,
  hash*33+c) hashing over CODE POINTS (via utf8Decode) so "é" hashes as one value, not its bytes;
  verified against hand-computed djb2 values and the code-point-vs-byte distinction.
- **Gameplay/scene:** ECS, SceneTree/Node2D, prefabs, groups, signals, scene serialization,
  a scripting VM (lexer→bytecode→GC, classes, closures, modules, hot reload, gradual typing).
- **Tooling:** in-engine editor (viewport, gizmos, inspector, undo/redo, save/load, asset browser,
  play-in-editor), CI (Linux/macOS/Windows), sanitizers, clang-tidy, golden-image tests, packaging,
  API docs, crash handler, telemetry, **a one-click Windows editor download**.

---

## Part B — The complete gap list (what Godot still has that Maz doesn't)

### 1. Rendering backends & platforms  [BIG]
Godot runs on Vulkan, D3D12, Metal, and OpenGL3 (desktop + web + mobile). Maz is Vulkan-only.
- [ ] **[GPU]** D3D12 backend (Windows-native)
- [ ] **[GPU]** Metal backend (macOS/iOS; today via MoltenVK only)
- [ ] **[GPU]** OpenGL ES 3 / WebGL2 "compatibility" backend
- [ ] **[BIG]** A Rendering Device abstraction so backends are pluggable (Maz already has a
  `Renderer` interface — this widens it to RDD level). *Design/interface work is [CPU]-reviewable.*

### 2. Global illumination & advanced lighting  [GPU][BIG]
Godot's headline 3D feature set. Maz has analytic IBL + shadow maps only.
- [ ] **[GPU]** SDFGI (signed-distance-field real-time GI)
- [ ] **[GPU]** VoxelGI
- [ ] **[GPU]** LightmapGI (baked) — *the lightmap UV-unwrap + bake math has [CPU] pieces*
- [ ] **[GPU]** Reflection probes (baked + real-time)
- [ ] **[GPU]** Volumetric fog / god rays
- [ ] **[GPU]** Screen-space reflections (SSR), SSIL
- [ ] **[CPU]** Cascaded shadow-map **split math** — ✅ done (M211); GPU passes remain
- [ ] **[GPU]** Point-light cube shadows

### 3. Rendering pipeline depth  [GPU]
- [ ] **[GPU]** Clustered Forward+ / deferred path (many lights)
- [ ] **[GPU]** TAA + FSR/AMD upscaling; FXAA
- [ ] **[GPU]** GPU-driven particles (Maz particles are CPU) + particle collision/attractors on GPU
- [ ] **[GPU]** Decals
- [~] **[CPU]** Mesh **LOD** selection + [GPU] auto-LOD generation
  — **LOD selection done** (M231): `render::LodChain` — screen-coverage LOD pick (projected pixel
  size vs per-level thresholds), lod_bias, cull-below-last, and switch hysteresis. [GPU] auto-LOD
  mesh *generation* (decimation) remains.
- [x] **[CPU]** Occlusion culling — **software occluder buffer done** (M234): `render::OcclusionBuffer`
  — conservative coarse depth grid (occluders write their farthest depth; a candidate is culled only
  when every covered cell is solid AND its nearest point lies at/behind that depth, so nothing visible
  is ever hidden), plus `projectAabb` (world AABB → screen rect + depth range through a view-proj).
  This is the CPU technique behind Godot's `OccluderInstance3D`. [GPU] HW occlusion queries remain.
- [ ] **[GPU]** VMA (Vulkan Memory Allocator) — replace manual allocations
- [ ] **[CPU]** Compressed textures **KTX2 container** — ✅ parsing done (M209); [GPU] transcode+upload remain
- [ ] **[GPU]** Anisotropic filtering, back-face-cull toggle, multiple/sub viewports, render-to-viewport

### 4. Shading / materials authoring  [BIG]
- [ ] **[BIG]** A shading language + compiler (Godot shader language → SPIR-V). *Parser/AST is [CPU].*
- [ ] **[GPU]** Visual shader graph (needs editor + the above)
- [ ] **[GPU]** Standard material feature parity (clearcoat, anisotropy, SSS, refraction, proximity fade)
- [ ] **[CPU]** SDF/MSDF font **generation** — ✅ done (M203); [GPU] sampling shader remains

### 5. Text & internationalization  [BIG]
- [~] **[BIG][CPU]** TextServer: complex-script shaping (HarfBuzz-class), BiDi, line breaking for
  CJK/Arabic/Indic. *Almost entirely CPU — a large but verifiable effort.*
  — **analysis subset done** (M247): `ui::TextServer` — UTF-8 decode, base-direction detection (UAX #9
  P2/P3, first strong char), bidirectional run segmentation (mixed LTR/RTL → runs; neutrals inherit),
  and line-break opportunities (UAX #14 subset: after spaces/hyphens, mandatory at newlines, between
  CJK ideographs, collapsing space runs). Verified across ASCII/Hebrew/CJK/emoji. The [BIG] pieces —
  complex-script *shaping* (HarfBuzz-class glyph sub/positioning) and full UAX #9 (embeddings/isolates,
  weak-type resolution) — remain.
- [x] **[CPU]** Translation/PO catalogs: `io::PoCatalog` + `io::PluralRule` — gettext PO parser
  (msgctxt/msgid/msgid_plural/msgstr[n], multi-line, escapes) with gettext/ngettext/pgettext/
  npgettext and a per-language plural-rule evaluator (the C subset: n, %*/+-, comparisons, && || !,
  ?:). M226. Correct pluralization for English/Polish/etc. beyond Maz's CSV tables.
- [ ] **[GPU]** SDF font rendering at draw time

### 6. Editor breadth  [BIG][GPU][DESK]
Maz has a minimal editor; Godot's is vast.
- [ ] **[GPU]** Dockable multi-panel editor shell (Dear ImGui or custom) — *layout logic [CPU]*
- [ ] **[GPU]** Dedicated editors: animation, tilemap/tileset, shader, particles, theme, navmesh bake
- [ ] **[GPU]** Full gizmo set, snapping, multi-viewport, 2D+3D edit modes
- [~] **[CPU]** Import dock + `.import` sidecar pipeline (Maz has reimport core; add settings UI/model)
  — **sidecar model done** (M237): `io::ImportFile` parses/encodes Godot's `.import` format
  ([remap] importer/type/uid/path, [deps] source_file/dest_files array, [params] options) with a clean
  round-trip; `io::ImportFile::cookedPath` builds the `res://.godot/imported/<base>-<hash>.<ext>`
  convention; `io::contentHashHex` (FNV-1a) + `io::ImportDatabase` answer "needs reimport?" from a
  source content hash (unknown → import, unchanged → skip, changed → reimport). The [GPU] import *dock
  UI* still remains.
- [ ] **[DESK]** Remote debugger / live scene inspection

### 7. Node & scene-system breadth  [CPU mostly]
Godot ships ~200 node types. Maz has the spine + many. Concrete missing high-value nodes:
- [x] **[CPU]** CanvasLayer, ParallaxLayer node, Path2D/PathFollow2D, RemoteTransform, VisibleOnScreenNotifier
  — **PathFollow2D done** (M221): `game::PathFollow2D` walks a Curve2D by progress/progress-ratio,
  loop-or-clamp ends, hOffset along the path normal, tangent-following rotation.
  — **VisibleOnScreenNotifier2D done** (M223): `game::VisibleOnScreenNotifier2D` fires screen
  entered/exited edge events as an object's rect crosses the camera view.
  — **RemoteTransform2D done** (M255): `scene::RemoteTransform2D` mirrors its transform onto a target
  node with Godot's per-channel toggles (position/rotation/scale) — full-copy when all three are on
  (skew preserved), otherwise recomposed from the selected components — plus the use-global-coordinates
  flag and a `globalToLocal(desiredGlobal, parentGlobal)` helper (`parent^-1 * global`) for the
  global-mode case. Built on `math::Transform2D`; verified across every channel combination and the
  global↔local round-trip.
  — **CanvasLayer done** (M256): `scene::CanvasLayer` — an independent 2D draw layer with its own
  transform (offset/rotation/scale) and a `layer` draw-order index. Screen-fixed by default (a HUD
  ignores the world camera); with follow_viewport on it tracks the viewport canvas transform scaled by
  follow_viewport_scale via `finalTransform(viewportCanvas)`. A `CanvasLayerStack` returns visible
  layers ordered ascending by index (low = behind), stable within ties. Built on `math::Transform2D`;
  verified for screen-fixed vs following behaviour and stack ordering. **This closes the node line** —
  all listed high-value nodes now have CPU models.
- [~] **[CPU]** Timer, Tween node, AnimationPlayer node wrapper, Marker2D/3D
  — **Timer done** (M222): `game::Timer` countdown with wait_time, one_shot/repeating (remainder-
  carrying so cadence never drifts), pause/stop/restart, start(override), timeout callback. Others remain.
- [~] **[CPU]** GridMap (3D tile map) — **data model done** (M224): `game::GridMap` sparse cell→
  (tileId, orientation) store, set/clear/has/query, occupied-bounds, world↔cell floor-div mapping,
  packed signed 64-bit keys. [GPU] mesh-library instancing render deferred until there's a display.
- [x] **[CPU]** CSG (constructive solid geometry) mesh ops — *pure mesh boolean math is [CPU]*
  — **boolean core done** (M238): `game::Csg` — signed-distance-field CSG (Godot's CSGCombiner3D
  union/intersection/subtraction semantics). Primitives (sphere, box, rounded box, plane half-space,
  cylinder), the three booleans (union=min, intersect=max, subtract=max(a,-b)) plus smooth/rounded
  variants, translate/scale transforms, `inside()` containment, and `sdfNormal()` gradient normals.
  Exactly unit-tested by sampling distances (analytic sphere/box distances, shell subtraction,
  fillet dip, radial normals). **Mesh output done** (M239): `render::surfaceNets` (Naive Surface Nets)
  turns any CSG field into a watertight triangle mesh with per-vertex normals — one vertex per
  surface-crossing cell, quads across sign-flipping edges. Verified: every meshed vertex lies on the
  isosurface (within a cell), correct ring radius/centroid, outward normals, no orphan vertices, and a
  sphere-minus-box CSG meshing into one connected surface. [GPU] upload of the resulting buffers is the
  usual mesh path.
- [x] **[CPU]** MultiplayerSpawner/Synchronizer scene nodes (needs networking, below)
  — **done** (M243): `net::MultiplayerSpawner` — Godot's MultiplayerSpawner. The authority assigns a
  network id per spawn (scene-type tag + args), queues spawn/despawn events, and serializes them to a
  BitStream (`writeEvents`); remotes apply via `onSpawn`/`onDespawn` and mirror the live set. `writeFull`/
  `readFull` give a late joiner a one-shot keyframe of the whole world; replay is idempotent (already-live
  ids don't double-spawn) and malformed streams are rejected. Pairs with `net::Synchronizer` (M218,
  MultiplayerSynchronizer) for per-node property sync thereafter. Verified end-to-end (event replication,
  args round-trip, late-join snapshot, idempotence, truncation). [DESK] real socket transport remains.

### 8. UI (Control) library  [CPU mostly]
Maz has a strong slice (LayoutNode, containers, Tree, ItemList, PopupMenu, TextField, StyleBox,
Theme, Range/ProgressBar, nine-patch, BBCode). Missing vs Godot:
- [x] **[CPU]** TabContainer, GraphEdit/GraphNode, RichTextLabel effects, FileDialog, ColorPicker,
  SpinBox, OptionButton, Tree editing, drag-and-drop between controls
  — **GraphEdit/GraphNode done** (M241): `ui::GraphEdit` — the node-graph model behind visual
  scripting / the shader graph / blend trees. Nodes with named input/output ports + canvas position;
  connect/disconnect with full validation (endpoints exist, no self-links, no duplicate wires,
  cycle rejection for acyclic graphs), many-to-one/one-to-many wiring, `wouldCreateCycle`, incident-wire
  cleanup on node removal, and a Kahn `topologicalOrder` (empty on cycle). Verified across all of those.
  **ColorPicker colour math done** (M242): `render::ColorOps` — HSV<->RGB, hex `#rrggbb`/`#rrggbbaa`
  (+shorthand) parse/format, lighten/darken/lerp/invert, Rec.709 luminance, and sRGB<->linear transfer
  functions, all verified against known colour identities. **OKLab / OKLCh perceptual space done**
  (M304): `render::linearToOklab`/`oklabToLinear`, `oklabToOklch`/`oklchToOklab`, and `oklabMix` — the
  Björn Ottosson OKLab space that underpins Godot 4's OKHSL colour picker (`Color.from_ok_hsl`) and CSS
  Color 4's `oklab()`/`oklch()`. Verified white→L≈1/a≈0/b≈0, black→0, monotonic lightness, opponent-axis
  signs (red +a, green −a, blue −b), exact linear↔OKLab↔OKLCh round-trips, grey→zero chroma, and
  perceptual `oklabMix` endpoints/midpoint lightness. (Full OKHSL gamut mapping is a planned follow-up.) **SpinBox / OptionButton / TabBar done**
  (M244): `ui::SpinBox` (a Range with step buttons + prefix/suffix text format/parse), `ui::OptionButton`
  (drop-down list with selected item, id lookup, disabled rejection, auto-select-first), and `ui::TabBar`
  (ordered tab strip with current tracking, disabled-skipping next/previous nav, removal that clamps
  current). Verified across clamp/snap, selection, and navigation edge cases. **drag-and-drop done**
  (M249): `ui::DragAndDrop` — Godot's Control drag/drop coordinator (get_drag_data / can_drop_data /
  drop_data): begin a drag with a typed payload, poll hover targets for acceptance, deliver to an
  accepting target on release (else keep/cancel), with single-drag and drop-when-idle guards.
  **ColorPicker widget done** (M251): `ui::ColorPicker` — the interactive model behind Godot's
  ColorPicker (colour math was M242). Stores H/S/V/A as the source of truth so the hue survives a
  value/saturation drag to an extreme (setColor to black keeps hue+saturation, to grey keeps hue),
  matching Godot; derives RGB/hex on demand. Carries the picker's editing state: RGB-channel setters,
  hex I/O (`#` optional in, none out; alpha byte gated by an edit-alpha toggle), slider mode
  (RGB/HSV/RAW with RAW allowing HDR >1), user preset swatches (dedup-to-end + erase), and a capped
  most-recent list (front-inserted, deduped). Verified across hue-preservation, round-trips, and
  preset/recent edge cases. **TabContainer done** (M252): `ui::TabContainer` — Godot's TabContainer,
  a container that owns several content panels and shows one at a time via an integrated tab strip.
  Each tab carries an opaque content id (the widget layer maps it to a panel); the container keeps
  exactly one *selectable* tab current (or -1), supports disabled tabs (shown-but-greyed) and hidden
  tabs (dropped from the strip), and re-points `current` to a selectable neighbour whenever a tab is
  disabled/hidden/removed. Verified across disable/hide/remove, navigation skipping, and clamp edge
  cases. **FileDialog done** (M253): `ui::FileDialog` — Godot's FileDialog model. To stay pure and
  disk-free it takes a directory-lister callback (dir → entries), so the widget/platform layer supplies
  the real filesystem and tests inject an in-memory tree. Handles current-directory navigation
  (enterDir/goUp with path normalisation), name filters (`*.png`, comma-separated `*.jpg,*.txt`,
  case-insensitive extension match, dirs always shown, all-files fallback), hidden-file toggle,
  dirs-before-files sorting, single/multi selection, and mode-specific confirmation for the four Godot
  file modes (OpenFile / OpenFiles / OpenDir / SaveFile, with SaveFile appending the active filter's
  extension when the typed name has none). Verified end-to-end over an in-memory tree.
  **RichTextLabel effects done** (M254): `ui::RichTextEffects` — the per-glyph animation maths behind
  Godot's RichTextLabel BBCode effects. Pure deterministic functions of (charIndex, time, params) that
  return a `CharFx` (positional offset + colour multiplier) or alpha: `rtWave` (vertical sine, phase
  per glyph), `rtTornado` (circular orbit), `rtShake` (hash-based bounded jitter that resteps at a
  given rate — no global RNG, fully reproducible), `rtRainbow` (hue cycle via the HSV math), `rtFadeAlpha`
  (linear fade across N chars), and `rtPulse` (breathing alpha). The BBCode parser (M141) tags which
  glyphs each effect covers; the widget layer calls these per glyph per frame. Verified across bounds,
  determinism, cycle-wrap, and ramp edge cases. **This closes the UI-widgets parity item** — every
  listed control (TabContainer, GraphEdit, RichTextLabel effects, FileDialog, ColorPicker, SpinBox,
  OptionButton, drag-and-drop, Tree editing) now has a CPU model.
- [x] **[CPU]** Full theme system (per-control theme overrides, theme types)
  (M250): extended `ui::Theme` with **theme type variations** (Godot's `theme_type_variation` /
  theme type inheritance) — `setTypeVariation(type, base)` chains a variation onto a base type, and
  typed lookups `themeColor` / `themeStyleBox` / `themeConstant` (+ `hasTheme*`) walk that chain
  (multi-level, cycle-safe via a visited set) before falling back. Added a numeric **constants**
  item class alongside colours/styleboxes. Plus **per-control theme overrides** (`ui::ThemeOverrides`
  — Godot's `add_theme_color_override` / `add_theme_stylebox_override` / …) and free `resolveColor` /
  `resolveStyleBox` / `resolveConstant` that give a local override precedence over the theme chain,
  with clear-to-restore. The old freeform-key `Theme` API (M109) is untouched.
- [ ] **[GPU]** Control clipping via viewport/backbuffer

### 9. Physics remaining  [CPU/BIG]
Maz's 2D is at parity; 3D is strong but not exhaustive.
- [~] **[CPU]** 3D convex-hull + trimesh (concave static) colliders, height-field collider
  — **convex hull done** (M227): `game::buildConvexHull` incremental hull of a point cloud →
  outward-wound triangular faces + hull vertex set (ConvexPolygonShape3D geometry).
  — **height-field done** (M228): `game::HeightField3D` — grid of height samples with bilinear
  heightAt/normalAt and a grid-DDA + Möller–Trumbore raycast (HeightMapShape3D).
  — **trimesh done** (M229): `game::TriMesh3D` — concave triangle-soup static collider with a
  BVH-accelerated nearest raycast (ConcavePolygonShape3D). All three 3D collider shapes now covered.
- [~] **[CPU]** 3D more joints (cone-twist, 6DOF, slider, generic), soft bodies, ragdolls
  — **slider done** (M230): `Joint3D::Slider` / `makeSliderJoint3` — prismatic joint locking the
  2 perpendicular linear + 2 perpendicular angular DOF, leaving slide+spin along one axis free
  (SliderJoint3D). **cone-twist done** (M235): `Joint3D::ConeTwist` / `makeConeTwistJoint3` —
  point-to-point pin plus a unilateral swing cone (b's twist axis held within `swingSpan` of a's
  cone axis) and a unilateral twist limit (rotation about the axis capped at +/- `twistSpan`), the
  ragdoll-limb joint (Godot ConeTwistJoint3D). Verified: a pendulum caught exactly at a 30-degree
  cone stop, a 170-degree cone swinging freely (no false constraint), and a spun body caught at its
  twist limit. **soft bodies done** (M246): `game::SoftBody` — Godot's SoftBody3D via Position-Based
  Dynamics (Verlet particles + distance constraints + pins), unconditionally stable. Verified: a
  stretched link relaxes to rest, a pinned link/rope hangs under gravity without over-stretching (<5%),
  pins stay fixed, and energy stays bounded over thousands of steps. 6DOF / generic joints and ragdolls
  (a skeleton driven by cone-twist joints) remain; soft-body-vs-rigid collision is a follow-up.
  **ragdoll done** (M248): `game::buildRagdoll` — Godot's PhysicalBone3D ragdoll: turns a bone list
  (segment + parent + per-joint swing/twist limits) into capsule bodies oriented along each bone, tied
  to their parents by cone-twist joints at the shared joint. Verified: bodies/joints created, capsules
  oriented along their bones, joint anchors coincident, a pinned chain stays connected while it settles
  (worst gap <0.1), and a free ragdoll dropped on the ground falls, lands above the floor, and stays
  connected. 6DOF / generic joints remain.
- [ ] **[CPU]** Cross-engine determinism audit vs Godot Jolt (fixed-point optional)

### 10. Networking / multiplayer  [BIG][CPU]
Godot has high-level multiplayer (RPC, MultiplayerSynchronizer, ENet/WebRTC/WebSocket). Maz has
**none** today. This is a whole subsystem — and almost entirely CPU-testable.
- [x] **[CPU]** Wire format: `net::BitStream` (BitWriter/BitReader) — bit-packed packet
  serialization (N-bit ints, quantizable floats, signed sign-extension, byte arrays, align,
  underflow-safe). M212. The compact encoding replication/RPC ride on.
- [x] **[CPU]** Reliability/ack layer: `net::Reliability` (seqGreaterThan wraparound comparator,
  AckReceiver producing ack + 32-bit ack-bitfield, AckSender resolving in-flight → newly-acked).
  M213. The reliable-over-UDP core (Fiedler/ENet model) — RTT + resend basis.
- [x] **[CPU]** Connection / packet framing: `net::Connection` (protocol-id + seq/ack/ackBits
  header over net::BitStream; pack payload → bytes, unpack bytes → payload + resolve acks +
  reject foreign/truncated). M219. The framing directly beneath a real UDP socket.
- [ ] **[DESK]** UDP socket binding (SDL_net or BSD sockets) wiring net::Connection to real
  datagrams — the one part that needs actual sockets (not unit-testable headless; the framing above is)
- [x] **[CPU]** Snapshot/delta replication: `net::Snapshot` (per-field-bit-width schema; full +
  changed-mask delta encode/decode over net::BitStream). M214. Only changed fields cross the wire.
- [x] **[CPU]** Interpolation buffer: `net::InterpolationBuffer` (time-ordered sample history;
  lerp the bracketing pair at a delayed render time; bounded velocity extrapolation for late
  packets). M215. Valve-style entity interpolation — smooth motion from discrete ticks.
- [x] **[CPU]** Client-side prediction + server reconciliation: `net::PredictionBuffer` (apply
  inputs locally + immediately; on an authoritative snapshot, drop acked inputs, snap to server
  state, re-simulate the unacked ones). M216. Valve/Gaffer prediction — instant-feeling netcode.
- [x] **[CPU]** RPC layer: `net::RpcDispatcher` (name-hashed method ids, reliable/unreliable/
  ordered transfer modes, header write + dispatch-to-handler over net::BitStream; unknown/malformed
  calls counted, not crashed). M217. Godot's @rpc / rpc()/rpc_id() model.
- [x] **[CPU]** Scene-replication nodes: `net::Replication` (`ReplicatedObject` declares get/set
  properties with wire widths; `Synchronizer` holds the per-peer baseline and does writeFull/
  writeDelta/readFull/readDelta over net::Snapshot). M218. Godot's MultiplayerSynchronizer.
- [x] **[CPU]** Network-condition simulator: `net::NetSim` (seeded-deterministic latency/jitter/
  loss/duplication queue; send/receive by caller time). M220. The "bad network" test harness that
  validates the ack/interpolation/prediction layers — beyond what Godot ships built-in.
- [ ] **[DESK]** WebSocket + WebRTC data channels (browser/native transports — need real sockets)

### 11. XR / VR  [GPU][DESK][BIG]
- [ ] **[GPU]** OpenXR integration, stereo rendering, XR controllers/hands

### 12. Export / platforms  [BIG][DESK]
Godot one-click exports to Win/mac/Linux/Web/Android/iOS/consoles. Maz builds native + has a
Windows editor download.
- [ ] **[DESK]** macOS + Linux packaged builds (extend the existing package pipeline)
- [ ] **[BIG]** Web export (Emscripten + WebGL/WebGPU) — depends on a GL/GPU backend
- [ ] **[BIG]** Android + iOS export
- [x] **[CPU]** Export templates + a project/export config model — `io::ExportConfig` /
  `io::ExportPreset` (M232): per-platform presets with feature tags + include/exclude glob filters
  and an `includes(path)` decision (Godot export presets), on a reusable `globMatch` core.

### 13. Asset import breadth  [CPU mostly]
- [~] **[CPU]** FBX, OBJ, Collada importers (Maz has glTF); [CPU] image formats beyond PNG/JPEG (WebP, HDR/EXR)
  — **OBJ done** (M225): `render::parseObj`/`loadObj` — Wavefront v/vt/vn/f, 1-based + negative
  indices, v//vn form, per-combo vertex dedup, n-gon fan triangulation → MeshData. FBX/Collada remain.
  — **HDR (Radiance RGBE) done** (M233): `io::decodeHdr` — .hdr header + new-RLE + raw scanlines →
  linear float RGB (skybox/IBL source). WebP/EXR remain.
- [ ] **[CPU]** OGG Vorbis / MP3 audio decode (Maz has WAV)
- [~] **[CPU]** Font: OTF/collection support, dynamic font sizing cache
  — **dynamic sizing cache done** (M240): `ui::GlyphCache` — the size-keyed glyph atlas Godot's
  FontFile keeps for dynamic fonts. Keys by font+codepoint+pixel-size, rasterizes each glyph once (via
  an injected rasterizer) into the skyline-packed atlas, returns the atlas rect + metrics, and
  evicts-all + repacks when the page fills. Verified: miss→rasterize / hit→cached, distinct entries per
  size, non-overlapping in-bounds packing, overflow eviction that keeps serving, and clear→re-raster.
  OTF/font-collection *parsing* remains (the current rasterizer is stb_truetype/TTF).
- [ ] **[GPU]** Video playback (Theora/WebM)

### 14. Scripting ecosystem  [CPU/BIG]
Maz has its own VM. Godot has GDScript + C# + GDExtension (native plugins).
- [~] **[CPU]** GDExtension-style C ABI so third parties add engine modules without recompiling
  — **ABI core done** (M245): `ext::ExtensionRegistry` — a stable, C-compatible interface (a tagged
  `ExtVariant` + plain function pointers, no std types in the payload) for registering classes + methods
  from a separate module, instantiating them, and dispatching calls by name. Includes ABI version
  negotiation (`abiCompatible` — major must match, plugin minor ≤ host) and a `loadExtension` entry-point
  that rejects incompatible plugins up front. Verified with a mock in-process "Counter" extension:
  register/instantiate/dispatch (with args + return), unknown-method/class → ok=false, duplicate/invalid
  registration rejection, and unregister. The real `dlopen`/`LoadLibrary` of a `.so`/`.dll` remains [DESK].
- [x] **[CPU]** Debugger protocol (breakpoints, step, variable inspection) for the maz::script VM
  — **done** (M236): `script::Debugger` drives the VM's per-statement hook and adds line breakpoints,
  the four stepping modes (into / over / out / continue) resolved from call-stack depth, a call-stack
  snapshot at each stop, and paused-frame variable inspection (`locals()` / `resolve()` /
  `valueString()`). Execution pauses synchronously via an `onPause` handler that returns the next step
  mode. Verified: breakpoint + local inspection (a/b/s inside a function with the right call stack),
  step-into descending into a call, step-over skipping a body, step-out returning to the caller,
  break-at-entry single-stepping, and zero pauses when nothing is armed. Wiring this to a remote IDE
  over a socket is the [DESK] transport on top; the decision + inspection core is complete.
- [ ] **[CPU]** Optional C# / other language hosting (large)

---

## Part C — Phased execution plan

Ordered so each phase is buildable and mostly verifiable, front-loading CPU work that lands now and
sequencing GPU/desktop work for a real machine.

- **Phase α — finish every CPU-verifiable slice (in progress, this loop).** Cascade splits ✅, KTX2 ✅,
  present-mode ✅, focus/DPI/multi-monitor ✅, text/clipboard ✅. Remaining CPU slices: LOD selection,
  occlusion math, CSG boolean math, GridMap data model, more UI controls, more importers (OBJ/OGG),
  PO localization, TextServer line-breaking, the shading-language **parser/AST**.
- **Phase β — networking (whole new subsystem, ~all CPU).** Transport → replication → prediction →
  RPC → scene nodes. Biggest single parity win that needs no GPU. Fully unit-testable.
- **Phase γ — rendering-device abstraction + Forward+ (GPU, on real hardware).** Widen `Renderer`
  to a device layer, then clustered Forward+, GPU particles, decals, SSR, TAA/FSR, VMA.
- **Phase δ — global illumination (GPU).** SDFGI → reflection probes → volumetric fog → LightmapGI.
- **Phase ε — editor breadth + shading language + visual shaders (GPU + desktop).**
- **Phase ζ — export/platform matrix (desktop + web + mobile).**

---

## Part D — "Better than Godot" opportunities (not just parity)

Places Maz can exceed Godot rather than match it:

- **Performance budgets & CI-enforced regression gates** — already in (core::PerfBudget); Godot has
  no formal budget/alert layer. Extend to per-scene budgets asserted in CI.
- **Determinism-first simulation** — Maz already has deterministic RNG, replay, fixed-step. Push to
  full cross-platform lockstep determinism (a networking/e-sports advantage Godot lacks out of box).
- **Header-only, dependency-light core** — trivial embedding vs Godot's monolith.
- **Golden-image + sanitizer + clang-tidy gates on every push** — a stricter quality bar than
  upstream Godot CI on the engine core.
- **Planner-grade AI (GOAP)** built-in — beyond Godot's BT-only offering.
- **A formal, unit-tested audio DSP suite** with dB-correct units and lookahead limiter.

---

*This document is the master parity tracker. As milestones land, update the matching `[ ]`/`[~]`/`[x]`
here and in `docs/ROADMAP.md`. The one rule: never mark something done that can't be verified —
GPU/desktop items stay honest until they run on real hardware.*
