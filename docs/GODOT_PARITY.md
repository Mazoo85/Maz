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
  application-negates; M381 adds roundv / floorv / ceilv for vec2+vec3 — Godot Vector2/Vector3.round /
  floor / ceil, component-wise, with round half-away-from-zero matching Godot's Math::round, verified
  against exact values incl. the .5 boundary — for pixel/tile-grid snapping) (M323 adds the Vector3 overload rotated(v,axis,angle)
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
  non-finite basis component; M380 adds get_origin plus **looking_at** — Godot's Transform2D.looking_at:
  a faithful port that returns a unit-scale copy at the same origin whose X axis aims at a world-space
  target (Godot's affine-inverse + scale-weighted angle formula), verified so the X-axis heading equals
  atan2(target - origin) for identity, translated, rotated and general unit-scale sources), **Transform3D** (M268,
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
  and exp of the zero vector == identity; M378 adds sphericalCubicInterpolate — Godot's
  Quaternion.spherical_cubic_interpolate (SQUAD-style cubic guided by neighbouring pre/post
  orientations: normalize + shortest-hemisphere flip, a scalar cubic on the log-map coordinates in
  both from- and to-tangent-spaces, then a slerp of the two Expmap results), verified for exact
  endpoints, always-unit output, monotonic same-axis sweep, and mixed-axis 3D control points; M379
  completes the pair with sphericalCubicInterpolateInTime — Godot's
  Quaternion.spherical_cubic_interpolate_in_time: the same SQUAD construction but the log-map cubic is
  the non-uniform (Barry-Goldman) cubicInterpolateInTime with per-sample times, verified to collapse
  onto sphericalCubicInterpolate at uniform times and to keep exact endpoints and unit output; M336 adds the validation predicates is_finite /
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
  angles, and take the forward-through-zero path from 350deg to 10deg; M377 completes this family with
  cubicInterpolateAngleInTime — Godot's @GlobalScope.cubic_interpolate_angle_in_time: the shortest-arc
  angle unwrap of cubicInterpolateAngle fed into cubicInterpolateInTime's non-uniform timing, verified
  to collapse onto cubicInterpolateAngle at uniform times and to keep the forward-through-zero wrap;
  M334 adds isFinitef/isNanf/
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
  remain out of scope); plus **convex decomposition** M384 — `decomposePolygonInConvex` (Godot's
  Geometry2D.decompose_polygon_in_convex: ear-clip into triangles then Hertel–Mehlhorn-merge
  edge-adjacent pieces while they stay convex, so a concave collider splits into convex parts;
  verified every piece is convex, areas sum to the input, a convex input collapses to one piece, and
  CW input is normalised to CCW); plus **convex polygon offsetting** M385 — `offsetPolygonConvex`
  (the convex mitre case of Godot's Geometry2D.offset_polygon: push each edge out along its outward
  normal by delta and re-intersect at the corners, for collision margins / selection outlines /
  grow-shrink; verified a side-2 square +1 -> area 16, -0.5 -> area 1, 0 -> unchanged, CW normalised,
  triangle grows and stays convex — general concave/round-join Clipper offsetting stays out of scope); plus **polyline simplification** M424 —
  `simplifyPolyline` (Ramer–Douglas–Peucker: drop points within a tolerance of the chord through a
  run's endpoints, keeping the shape while shedding redundant vertices — the workhorse behind cleaning
  hand-drawn strokes/gestures, thinning GPS/AI paths and reducing generated-outline vertex counts;
  non-recursive explicit-stack form so it is safe on very long inputs; endpoints always kept. Godot has
  no polyline simplify. Verified: trivial pass-through, a collinear run collapsing to 2 points, a bump
  kept/dropped either side of the tolerance, a preserved right-angle corner, endpoints-preserved with
  result never larger than input, a square outline keeping its 4 corners, and a shallow arc collapsing
  under a big tolerance but surviving a fine one); plus **polyline smoothing** M425 —
  `chaikinSmooth` (Chaikin corner-cutting: replace each corner with points 1/4 and 3/4 along its two
  edges, repeated N times, to round a coarse polyline into a smooth curve — the complement of
  simplifyPolyline for smoothing AI/nav paths, strokes and outlines without a spline fit; open paths
  keep both endpoints, `closed` cuts every vertex of a loop. Godot has no Chaikin. Verified: no-op on
  <3 points / iterations<=0, an open L cutting to 6 points with the exact 1/4–3/4 corner cuts, point
  count growing with endpoints preserved across iterations, a closed square cutting to 8 in-bounds
  positive-area points, and a straight run staying collinear); plus **polyline length + even resampling**
  M426 — `polylineLength` (total arc length, open or closed-loop) and `resamplePolyline` (redistribute a
  polyline into exactly N points spread evenly by ARC LENGTH, keeping the endpoints — the workhorse
  behind evenly spacing dashes/decorations/spawns along a route and uniform sampling for morphing; unlike
  Curve2D's Bézier baking this works on any raw polyline). Verified: open perimeter 12 / closed 16 / a
  3-4-5 leg = 5, a straight line resampled to 5 exact points, an L-shape resampled to uniform spacing-2
  points across the corner with endpoints kept, dense resampling staying within 2% of the source length
  (corner-cutting keeps it ≤ source), and degenerate count<2 / all-coincident handling; plus **Catmull-Rom
  spline through waypoints** M427 — `catmullRomSpline` (a smooth curve that PASSES THROUGH every waypoint,
  unlike a Bezier whose control points only pull it — what you want for a camera/object path that must hit
  exact points, or rounding a coarse route into a flowing curve. Each segment reuses the existing
  cubicInterpolate/Catmull-Rom with the two neighbours as tangents; boundaries clamp (open) or wrap
  (closed). Godot has cubic_interpolate per-segment but no spline-through-a-point-list builder. Verified:
  degenerate pass-through, an open spline of samplesPerSegment=8 hitting every waypoint at its k·S knot
  with count (n-1)·S+1, samplesPerSegment=1 reproducing the input, collinear waypoints staying collinear
  with monotonic x, and a closed loop of N·S points passing through every waypoint and starting at the
  first); plus **segment-vs-rect clipping** M303 — `clipSegmentToRect` (Liang–Barsky:
  clip a segment to an axis-aligned rectangle for viewport/bounds clipping of lines and rays); plus **point-in-circle test** M373
  — `pointInCircle` (Godot's Geometry2D.is_point_in_circle: squared-distance ≤ radius², boundary counts as inside); **Aabb3 method completeness** M272 —
  encloses / intersection / grow / expand / abs / longest-shortest-axis / intersectsSegment toward
  Godot's AABB; M324 adds `intersectsPlane` (Godot AABB.intersects_plane) — true when the box
  straddles a plane, faithfully reproducing Godot's asymmetric touch rule (a zero-distance corner
  counts as "under", so a box touching from the positive side intersects but one touching from below
  does not), tested via the two ±normal support corners; M318 adds the axis-vector forms `longestAxis`/`shortestAxis` (Godot get_longest_axis /
  get_shortest_axis, ties to the earliest axis) and `endpoint(i)` for the 8 corners
  (get_endpoint), verified against known corners and containment; M339 adds `isEqualApprox`
  (position + size approx) and `isFinite` — Godot AABB.is_equal_approx / is_finite, verified against
  nudged/differing boxes and a NaN component); **Color completeness** M273 (`render::blend` alpha compositing, `clampColor`,
  `isEqualApprox`, 32-bit pack/unpack `toRgba32`/`toArgb32`/`toAbgr32`/`fromRgba32`, `color8`,
  and M370 `r8`/`g8`/`b8`/`a8` 0..255 channel accessors (Godot Color.get_r8..a8, round+clamp per
  channel, verified incl. out-of-range clamp and color8 round-trip); M331
  adds 64-bit (16-bit-per-channel) `toRgba64`/`fromRgba64` — Godot's Color.to_rgba64 / Color.hex64
  for high-bit-depth packing, verified by known values, clamping, and a round-trip that preserves a
  fine difference 8-bit would collapse; plus **CPU Image** M386 (`render::Image` — a headless
  RGBA8 raster, the counterpart to Godot's Image class: construct at a size with a fill colour,
  getPixel/setPixel with clamped bounds, fill, flipX/flipY, and blitRect to copy a sub-rectangle
  between images; row-major, 8-bit-per-channel, top-left origin, raw bytes ready for
  createTexture — used to build procedural textures, icons and lookup tables on the CPU;
  verified by exact 8-bit round-trips through Color, fill, both flips, a clipped blit, and
  out-of-bounds safety); M387 extends Image with **region & compositing ops** (`fillRect`,
  `getRegion` — Godot Image.fill_rect / get_region — and `blendRect`, Godot Image.blend_rect:
  source-over alpha compositing of a sub-rectangle using the same operator as Color.blend, so
  semi-transparent pixels mix rather than overwrite; all clipped to bounds, verified against
  render::blend with a known base/over pair, region extraction across the edge, and a
  transparent-source no-op); M388 adds **geometric transforms** (`crop`, `rotate90(clockwise)`,
  `rotate180` — Godot Image.crop / rotate_90 / rotate_180, all in-place: crop resizes the canvas
  keeping the top-left corner and pads new area transparent, rotate90 swaps width/height either
  direction, rotate180 flips both axes; verified by exact pixel remaps on a coordinate-coded
  image, four-CW-rotations-is-identity, two-CW-equals-180, and crop shrink/grow); M389 adds
  **resize** (`resize(w, h, Interpolation)` — Godot Image.resize: nearest-neighbour or bilinear
  resampling, in place; bilinear samples at the destination pixel centre and blends the four
  surrounding texels with edge clamping; verified by exact nearest block mapping on an upscale/
  downscale and by exact known-value bilinear on a 2x1 gradient upscaled to 4x1 → 8-bit
  {0,64,191,255}, plus solid-colour preservation and non-positive-size → empty); M390 adds
  **getUsedRect** (Godot Image.get_used_rect — the smallest math::Rect2i enclosing all pixels with
  alpha > 0, zero rect when fully transparent) and **premultiplyAlpha** (Godot
  Image.premultiply_alpha — multiplies each pixel's RGB by its own alpha for premultiplied "over"
  GPU blending; verified by known 8-bit products, opaque no-op and transparent-zeroing); M391 adds
  **generateMipmapChain** (Godot Image.generate_mipmaps — a box-filtered mip chain: level 0 is the
  image, each level halves both dimensions floored (min 1) with every texel the 2x2 average below,
  ready for createTexture trilinear sampling; verified by exact 2x2 averaging, chain length/dims
  for square and non-square inputs, and colour/alpha preservation — documented as the standard box
  downsample, not byte-exact to Godot's internal filter); M392 adds a **headless TGA codec**
  (`render::encodeTga`/`decodeTga` in ImageCodecTga.hpp — dependency-free encode/decode between
  render::Image and the Truevision TGA byte format Godot's Image imports; encodes uncompressed
  32-bit BGRA top-left-origin, decodes uncompressed 24/32-bit either origin, rejecting
  colour-mapped/RLE/truncated blobs; unlike the runtime stb_image path this is pure CPU bytes,
  verified by an exact header + round-trip, a hand-built bottom-up blob, 24-bit alpha-fill, and
  malformed-input rejection); M393 adds a **QOI codec** (`render::encodeQoi`/`decodeQoi` in
  ImageCodecQoi.hpp — the fast lossless "Quite OK Image" format Godot 4 imports natively;
  spec-exact to qoiformat.org: 64-entry running index, per-channel DIFF/LUMA deltas, RLE runs,
  8-byte end marker; encodes 32-bit RGBA, decodes 3/4-channel; verified by exact header + chunk
  bytes (RUN/RGB/DIFF), round-trips over solid/gradient/alpha/repeated-palette content, and
  malformed-input rejection); M394 adds a **BMP codec** (`render::encodeBmp`/`decodeBmp` in
  ImageCodecBmp.hpp — uncompressed Windows BMP that Godot imports; encodes 32-bit BGRA with a
  BITMAPINFOHEADER and bottom-up rows, decodes 24/32-bit honouring the height sign (bottom-up vs
  top-down) and the 4-byte row padding 24-bit rows require; verified by exact header bytes, a
  32-bit round-trip, a hand-built padded 24-bit bottom-up blob, and malformed-input rejection);
  plus
  **named colours** M288 (`render::namedColor` / `colorFromString` — the full CSS3/Godot named-colour
  palette, 146 constants byte-for-byte, forgiving name lookup like Godot's, matching Godot's Color
  constants + Color.from_string) —
  Godot's Color.blend/clamp/is_equal_approx/to_*32/Color8);
  **minimal enclosing circle** M397 (`math::minEnclosingCircle` / `Circle2` — the smallest circle
  covering a point set, via Welzl's algorithm (Nayuki's deterministic incremental form); a
  beyond-Godot bounding-volume utility for culling, broadphase bounds and fit-view-to-points.
  Verified against exact known cases — two-point diameter, square corners → radius √2, collinear →
  span/2, an interior point not enlarging it, acute-triangle circumcircle — plus all-points-inside
  and boundary-tightness on a random cloud;
  **marching squares** M406 (`math::marchingSquares` / `ContourSegment` — extract the iso-contour
  (line segments) of a 2D scalar field at a threshold; the 2D sibling of marching cubes and the tool
  behind metaball outlines, fluid/lava surfaces, terrain contour lines and fog-of-war edges. 16-case
  corner classification, linear-interpolated edge crossings for a smooth contour, and the two saddle
  cases resolved by the cell-centre average. A beyond-Godot utility (Godot has no direct equivalent).
  Verified against exact cases — a single-cell crossing at known interpolated points, all-below /
  all-above → empty, a linear ramp → the exact vertical iso-line (3 segments), origin/cell-size
  transform, a saddle → 2 segments, a central hot corner → a closed 4-segment loop, and degenerate
  sizes → empty;
  **grid line + tile line-of-sight** M407 (`game::bresenhamLine` / `game::lineOfSight` in
  `maz/game/GridLine.hpp` — the integer-grid line rasteriser (Bresenham, 8-connected, inclusive
  endpoints) and the tile line-of-sight test built on it: laser/road/trajectory tile strokes and
  "can A see B?" across a blocking tilemap (roguelike FOV, guard sight, cover). LOS walks the Bresenham
  path and blocks only on a cell STRICTLY between the endpoints, so a viewer on or looking at a wall
  still sees up to it. Godot leaves grid line/LOS to the game, so this is a genuinely-useful utility.
  Verified against exact cases — single point, contiguous horizontal/vertical runs, a perfect diagonal,
  a 2:1 shallow-slope midpoint, forward/backward cell-set equality, clear vs blocked-between vs
  endpoints-never-block, and adjacent/identical always-visible;
  **flood fill + connected regions** M408 (`game::floodFill` / `game::connectedRegions` in
  `maz/game/FloodFill.hpp` — the grid paint-bucket (every cell reachable from a seed through passable
  cells, BFS, 4- or 8-connected) and its region-labelling companion (partition all passable cells into
  connected components). Uses for reachability ("can the player get here"), spill/water fill,
  enclosed-area detection, and counting rooms/islands or finding the biggest cavern of a procedural
  map. Caller-supplied passability predicate, deterministic BFS/scan order. Godot leaves this to the
  game. Verified against exact cases — full-grid fill, a wall splitting a grid into two fills,
  4- vs 8-connected diagonal joining, out-of-bounds/blocked seed → empty, two-blob region counts, an
  all-passable single region, all-blocked → zero, and a checkerboard (5 singletons 4-connected vs one
  region of 5 8-connected);
  **field of view (recursive shadowcasting)** M409 (`game::computeFov` in `maz/game/FieldOfView.hpp` —
  the roguelike "what can this tile see?" algorithm: given an origin, a sight radius and an opacity
  predicate, it returns every visible tile, occluded by walls. The lit region of a torch, a guard's
  sight cone, or the fog-of-war reveal as a unit moves. Processes the 8 octants tracking each opaque
  tile's cast shadow as a slope range and recursing the still-visible sub-ranges (cost ∝ visible area);
  opaque tiles are themselves visible but hide what's behind them. Godot leaves FOV to the game.
  Verified against invariants — origin always visible, radius 0 → origin only, an open field's axis
  extents visible with beyond-radius excluded and every cell inside the Euclidean radius, an axis wall
  casting a shadow (wall seen, tiles behind hidden, sides open), a wall span hiding the cells behind
  it, and a fully-enclosed room revealing only the origin's ring;
  **grid ray traversal (Amanatides–Woo DDA)** M410 (`game::traverseGrid` / `game::raycastGrid` in
  `maz/game/GridRaycast.hpp` — the "fast voxel traversal" DDA in 2D: marches a CONTINUOUS float-coord
  ray through the unit grid, visiting every cell it passes through in exact crossing order (sub-cell
  precision, unlike the integer Bresenham of M407). What tile raycasters (Wolfenstein walls), light/
  sound propagation, and precise "which tile does this shot hit first" queries need; raycastGrid stops
  at the first blocking tile. Godot leaves grid raycasting to the game. Verified against exact cases —
  horizontal/vertical/negative-axis cell sequences, zero-direction → origin only, the Amanatides
  no-diagonal-jump invariant (consecutive cells differ by exactly one orthogonal step) on a 2:1 ray,
  negative-coordinate flooring, and raycastGrid hit / no-hit / origin-blocked;
  **BSP dungeon generation** M411 (`game::generateBspDungeon` / `Dungeon` / `BspDungeonParams` in
  `maz/game/BspDungeon.hpp` — the classic roguelike rooms-and-corridors generator via binary space
  partitioning: recursively split the map into regions, carve a random room in each leaf, and join
  siblings with L-shaped corridors so the whole level is one connected space. A different flavour from
  the organic CellularCave (M96) — sharp rectangular rooms linked by straight halls. Deterministic for
  a seed (core::Pcg32). Godot leaves procedural level generation to the game. Verified against
  structural guarantees — determinism (same seed → identical tiles), rooms within the bordered
  interior, rooms pairwise non-overlapping, every room tile is floor, the preserved wall border, and
  crucially ALL floor forms a single connected region (proving corridors join every room, checked by
  reusing M408 connectedRegions) — plus different-seed divergence and degenerate-size → empty;
  **perfect-maze generation** M412 (`game::generateMaze` / `Maze` in `maz/game/MazeGen.hpp` — a perfect
  maze via the recursive-backtracker (DFS): exactly one path between any two cells, no loops, no
  isolated pockets. Rendered as a (2·W+1)×(2·H+1) tile grid where odd coords are cell centres and even
  ones are the walls between them; a wall is carved only when the DFS links its two cells. A different
  procedural flavour again from BSP dungeons (rooms) and cellular caves (blobs). Deterministic for a
  seed (core::Pcg32). Verified against the exact spanning-tree property — floor tile count is precisely
  2·W·H−1 (fully connected AND acyclic, the definition of a perfect maze), every cell centre is floor,
  all floor is one connected region (reusing M408), the wall border and even/even intersections stay
  wall, plus determinism, seed divergence, and degenerate-size → empty;
  **Dijkstra distance map** M413 (`game::buildDijkstraMap` / `DijkstraMap::descend` / `game::makeFleeMap`
  in `maz/game/DijkstraMap.hpp` — the Brogue-style scalar "desire map" behind roguelike monster AI:
  multi-source BFS fills every passable cell with its step-distance to the nearest of one-or-more goals
  (walls impassable); a monster walks to the lowest-valued neighbour to approach (`descend`), and
  negating+re-scanning the field (`makeFleeMap`) turns approach into flee. Complements the vector-field
  FlowField (M112, single-goal float integration baked into direction vectors for crowds) — this is a
  multi-source INTEGER field the game reads and recombines directly. Unit-cost BFS, 4- or 8-connected.
  Godot leaves this to the game. Verified against exact Manhattan distances on open ground, multi-source
  nearest-goal, a wall forcing a longer-than-Manhattan detour with walls unreachable, an enclosed cell
  staying unreachable, `descend` walking strictly downhill to the goal (and staying put at a goal), the
  flee map moving a pursuer away from the goal, and degenerate-size → empty;
  **LZSS byte compression** M414 (`io::lzCompress` / `io::lzDecompress` in `maz/io/Compression.hpp` — a
  small self-contained LZ77 sliding-window codec for shrinking save files, level data and network
  payloads: longest-match-in-a-4KB-window references replace repeated runs, short runs stay literals,
  packed as control-byte groups of 8 flag bits (literal vs 12-bit-offset/4-bit-length match). Godot's
  PackedByteArray.compress/decompress territory (a lossless general codec, round-trip-exact rather than
  a specific on-disk format). Verified round-trip-exact across empty/single-byte, highly-repetitive
  (which also compresses to <¼ size), a period-4 pattern (<½), repetitive English text via the string
  helpers (shrinks), all-256-distinct bytes, 5000 pseudo-random bytes, an overlapping long run
  (LZ-as-RLE), and a block larger than the 4KB window;
  **Huffman entropy compression** M470 (`io::huffmanCompress` / `io::huffmanDecompress` in
  `maz/io/Huffman.hpp` — the ENTROPY-coding companion to the LZSS dictionary codec above: it assigns
  short bit codes to frequent bytes and long codes to rare ones (optimal prefix coding), the piece LZ +
  Huffman combine into as "deflate". Reach for it on data with a lopsided byte histogram but few long
  repeats — packed tables, tile indices, quantised audio, text. Canonical Huffman: the stream stores
  one-byte code lengths per present symbol, so the decoder rebuilds the identical code table with no
  separate model. Godot bundles deflate/zstd; the engine had only a dictionary coder, so this adds the
  missing entropy path -> parity. Verified round-trip-exact across empty, single-byte, a 1000-byte
  single-symbol run, alternating two-symbol, repetitive English text (which also compresses below its
  size), all-256-distinct bytes, deterministic identical output on repeat, highly-skewed data (shrinks
  to <½), 5000 pseudo-random bytes (exact, no corruption), and a truncated stream (no crash);
  **fixed-point number** M415 (`core::Fixed` — a Q16.16 fixed-point type for DETERMINISTIC simulation.
  Floating point gives subtly different results across CPUs/compilers/optimisation levels, which
  silently desyncs lockstep multiplayer, replays and cross-platform physics; Fixed is pure integer
  arithmetic so identical inputs give bit-identical outputs everywhere. value = raw/65536; +−×÷ with
  64-bit intermediates, comparisons, abs/floor/ceil/round/frac, truncate-toward-zero, and a
  float-free deterministic integer sqrt. Godot has no fixed-point type. Verified against exact raw
  values, exact fraction arithmetic (½, ¼, ×, ÷), floor/ceil toward ±inf on negatives, round
  half-up, truncate-toward-zero, sqrt exact on perfect squares and ~1e-3 of √2, defined divide-by-zero,
  and a 1000-iteration integer-only simulation that is bit-identical every run;
  **fixed-point 2D vector** M416 (`math::FixedVec2` — a 2D vector of core::Fixed, the deterministic
  companion to vec2 for lockstep-multiplayer movement, replay-exact physics and cross-platform
  gameplay: add/sub/scale, dot, the scalar 2D cross, squared/true length (via Fixed's float-free
  sqrt), distance, and a zero-safe best-effort normalize, all bit-identical across machines. Godot has
  no fixed-point vector. Verified against exact add/sub/scale, exact dot=11 and cross=±1, the EXACT
  3-4-5 length (=5) and its 6-8-10 double (=10), exact distance, an approximate unit normalize
  (~0.6,0.8, len≈1) with a zero-safe zero vector, and a 500-iteration integer-only vector simulation
  that is bit-identical every run;
  **fixed-point 3D vector** M421 (`math::FixedVec3` — the 3D sibling of FixedVec2, a deterministic
  Q16.16 3D vector for 3D lockstep/replay/cross-platform play: add/sub/scale, dot, the true 3D cross
  product, squared/true length (via Fixed's float-free sqrt), distance and zero-safe normalize, all
  bit-identical across machines. Godot has no fixed-point vector. Verified with exact arithmetic, dot=32,
  the right-hand cross basis (x×y=z etc.) with parallel-is-zero and cross ⟂ both inputs, EXACT
  Pythagorean quadruples (2,3,6)→7 and (1,2,2)→3 (fixed sqrt of perfect squares), exact distance, an
  approximate unit normalize with a zero-safe zero vector, and a 500-iteration integer-only 3D vector
  simulation that is bit-identical every run;
  **fixed-point quaternion** M423 (`math::FixedQuat` — a deterministic unit quaternion for 3D rotation,
  the rotation capstone of the deterministic-sim toolkit: 3D orientation cannot be represented
  deterministically without it, so this is what lets a lockstep/replay 3D game turn/aim/spin with
  bit-identical results. Built on FixedVec3 (M421) and the integer-CORDIC FixedTrig (M417): the whole
  from-axis-angle → compose → rotate-a-vector path is pure integer math. Hamilton product, conjugate,
  normalize, and the fast rotation formula v + 2w(u×v) + 2u×(u×v). Godot has no fixed-point quaternion.
  Verified with an exact identity rotation, the cardinal 90°/180° axis rotations (+x→+y about z,
  +z→+x about y, +x→−x about z, ~1e-2), length preservation, two 45° rotations composing to 90° (both
  by rotating twice and by multiplying the quaternions), q·conj(q)≈identity, zero-axis→identity
  (no divide-by-zero), unit length from axis-angle, and a 300-step integer rotation run that is
  bit-identical every time;
  **fixed-point trig** M417 (`math::fixSin`/`fixCos`/`fixSinCos` + `fixPi`/`fixTwoPi`/`fixHalfPi` — a
  deterministic sine/cosine for core::Fixed computed by an integer CORDIC: it rotates a vector by a
  tiny table of precomputed arctangents using only shifts, adds and one integer scale, so there are no
  floats on the runtime path and identical inputs give bit-identical bits on every CPU/compiler. This
  is what lets a lockstep-multiplayer or replay-exact game rotate anything reproducibly — Godot has no
  fixed-point trig at all. Verified against std::sin/std::cos across an 800-sample sweep spanning
  several turns (worst error ~2e-4), exact-enough cardinal anchors at 0/±π/2/π, the Pythagorean
  identity sin²+cos²≈1 throughout, fixSinCos agreeing with the standalone fixSin/fixCos, 2π==2·π in the
  fixed representation, and a 256-step accumulating sin/cos run that is bit-identical every time;
  **fixed-point math helpers** M418 (`math::fixMin`/`fixMax`/`fixClamp`/`fixSign`/`fixLerp`/`fixMoveToward`
  for scalars and `fixLerp`/`fixMoveToward`/`fixRotated`/`fixFromAngle`/`fixClampLength` for FixedVec2 —
  the everyday clamp/interpolate/chase-toward/rotate operations gameplay leans on constantly, done in
  pure integer fixed-point so they are bit-identical on every machine. These are the deterministic twins
  of Godot's float lerp/clamp/move_toward and Vector2.rotated/from_angle/limit_length, which Godot has
  no deterministic form of; built on M415/M416/M417. Verified with exact endpoints and midpoints for
  lerp, no-overshoot exact stops for move_toward (scalar and along a 3-4-5 leg), quarter/half-turn and
  length-preserving rotation, from_angle unit/scaled vectors, length clamping, and a 400-step integer
  chase+rotate simulation that is bit-identical every run;
  **fixed-point atan2 / vector angle** M419 (`math::fixAtan2` + `math::fixAngle`/`fixAngleTo`/
  `fixAngleDifference` — the deterministic inverse of M417's sin/cos, recovering the angle of a vector.
  Computed by the SAME CORDIC run in "vectoring" mode: instead of rotating a vector BY an angle it
  rotates (x,y) onto the +x axis and accumulates how far it turned, using the same arctan table with
  only shifts and adds, so it is bit-identical everywhere. This completes the trig round-trip
  (angle→vector via fixFromAngle, vector→angle via fixAngle) that lockstep games need for facing/aiming;
  Godot has no fixed-point atan2 or Vector2.angle. Verified with cardinal anchors, a 1600-point
  all-quadrant sweep vs std::atan2 (worst error ~6e-5, compared on the circle to handle the ±π wrap), a
  fixFromAngle(fixAngle(v)) round-trip matching v's direction over a 25×25 grid, signed angle_to and
  shortest-wrap angle_difference, and a 500-point atan2 sweep that is bit-identical every run;
  **fixed-point rectangle** M420 (`math::FixedRect2` — Godot's Rect2 in deterministic fixed-point: an
  axis-aligned box in core::Fixed coords (position + size) for lockstep broadphase, replay-exact overlap
  tests and deterministic trigger volumes. Same half-open convention and API as Rect2/Rect2i —
  hasPoint / intersects (with a touching-counts flag) / intersection / merge / grow / expand / encloses
  / abs / clampPoint / center / area — all integer fixed-point so bit-identical everywhere; the last
  piece rounding out the deterministic-sim toolkit (M415 Fixed, M416 FixedVec2, M417 trig, M418 helpers,
  M419 atan2, M420 rect). Godot has no fixed-point rectangle. Verified with exact accessors/area, the
  half-open min-inclusive/max-exclusive containment, edge-touching intersects both ways, exact
  intersection/merge/grow/expand/encloses, negative-size abs normalization, corner clamping, and
  sub-integer (fractional) coordinates;
  **fixed-point 3D box** M422 (`math::FixedAabb3` — Godot's AABB in deterministic fixed-point and the 3D
  sibling of FixedRect2: an axis-aligned box in core::Fixed 3D coords (position + size) for 3D lockstep
  broadphase, replay-exact overlap and deterministic 3D trigger volumes. Same half-open convention and
  API as FixedRect2/Rect2i — hasPoint / intersects (touching-counts flag) / intersection / merge / grow
  / expand / encloses / abs / clampPoint / center / volume — all integer fixed-point so bit-identical
  everywhere. Godot has no fixed-point box. Verified with exact accessors/volume, half-open
  min-inclusive/max-exclusive containment on all three axes, face-touching intersects both ways, exact
  intersection/merge/grow/expand/encloses, negative-size abs normalization, corner clamping, and
  sub-integer coordinates. Deterministic-sim toolkit now spans 2D+3D: M415 Fixed, M416 FixedVec2, M417
  trig, M418 helpers, M419 atan2, M420 FixedRect2, M421 FixedVec3, M422 FixedAabb3;
  plus **OKHSL** M395 (`render::fromOkhsl`/`toOkhsl` + `Okhsl` struct — the perceptual
  hue/saturation/lightness space Godot 4.3's colour picker uses, Color.from_ok_hsl /
  ok_hsl_h/s/l; a faithful transcription of Ottosson's reference okhsl built on the M304 OKLab
  matrices, with gamut-aware saturation so s=1 is the most saturated in-gamut colour for the hue;
  kept in maz's linear-Color convention (Ottosson's final sRGB encode omitted). Verified by pure
  inverse round-trips, black/white/mid-grey anchors, an independent OKLab gray-axis cross-check
  (s=0 → OKLab L == toe_inv(l)), and distinct in-range primary hues),
  curves, easing, two RNGs (xoshiro + PCG32), SlotMap, SmallVector/SparseSet, RingBuffer,
  **dynamic bit set** (M430, `core::BitSet` — a resizable word-packed bit set with test/set/reset/flip,
  whole-set and/or/xor/not, hardware-popcount count/any/none/all, and O(1)-per-hit iteration over just
  the set bits via findFirst/findNext; the natural backing for entity flag sets, per-frame visited/dirty
  marks, tile occupancy grids and arbitrary-width layer masks where a plain int mask runs out of bits —
  std::bitset is fixed-size and vector<bool> lacks set-algebra + fast scan, and Godot exposes only fixed
  32-bit masks. Verified: single-bit set/reset/flip/count, tail-bit correctness (a 3-bit setAll counts 3
  not 64), and/or/xor/not exact, findFirst/findNext walking set bits across word boundaries in order,
  resize grow/shrink, and order-independent equality),
  string
  interning, reflection, JSON/CSV/XML/base64/INI, binary + text serialization, resource packs,
  virtual filesystem, **hashing** (M284, `core::sha256` / `sha256Hex` / `crc32` — Godot's
  HashingContext / crc32 for asset integrity, save checksums, content-addressed caches and network
  digests; verified against the published SHA-256 / CRC-32 test vectors; plus **SHA-1** M299,
  `core::sha1` / `sha1Hex` — Godot's HashingContext HASH_SHA1 (content integrity, WebSocket handshake),
  verified against the standard vectors; plus **MD5** M300, `core::md5` / `md5Hex` — Godot's
  HashingContext HASH_MD5 (legacy non-security checksums), RFC 1321 vectors verified — completing
  Godot's MD5/SHA-1/SHA-256 HashingContext trio; plus **HMAC-SHA256** M298,
  `core::hmacSha256` — keyed message authentication for signed saves / tamper-proof network messages /
  API tokens, Godot's Crypto.hmac_digest, verified against the RFC 4231 vectors); plus **hexDecode** M382 —
  `core::hexDecode`, the inverse of toHex (Godot's String.hex_decode): parses a hex string to bytes,
  case-insensitively, returning empty on odd length or any non-hex character (Godot's exact failure
  behaviour), round-trips with toHex both ways), **endian-aware byte stream** (M277, `io::StreamPeerBuffer` — Godot's
  StreamPeerBuffer: fixed-width little/big-endian put/get for u8..u64 signed+unsigned, float, double,
  length-prefixed strings, safe past-end reads; for network protocols and portable binary formats),
  semver, deterministic time, replay, checkpoints, profiler, **RNG distribution
  helpers** (M275, `core::Pcg32::rangef`/`gaussian`/`weighted` — Godot RandomNumberGenerator's
  randf_range / randfn Box-Muller normal / rand_weighted; distribution-verified over 200k samples),
  **O(1) weighted sampler** (M428, `core::AliasTable` — Vose's alias method: pay an O(n) build ONCE to
  precompute two tables, then every draw is a single random column + one coin flip, constant-time
  regardless of table size — the right tool for a loot/spawn/scatter table sampled thousands of times a
  frame, where Pcg32::weighted's O(n) per-draw rescan is wasteful. Deterministic from a caller Pcg32;
  negative weights count as 0; all-zero falls back to uniform. Godot's rand_weighted is the O(n) form,
  so the build-once alias table is beyond-Godot. Distribution-verified over ~360k samples: {1,3,6}→
  10/30/60%, uniform→25% each, zero-weight entries never drawn, single/empty/rebuild handled, and the
  same seed reproduces the same sequence),
  **low-discrepancy sequence** (M429, `core::radicalInverse`/`halton`/`halton2D` + `HaltonSequence` —
  the Halton quasi-random sequence: unlike a pseudo-random generator (which clumps and leaves gaps), each
  new point lands in the biggest remaining hole, filling space EVENLY. The right tool for procedural
  scatter that should look spread-out not blotchy, anti-aliasing/temporal jitter offsets, and evenly
  probing a search space; each coordinate is the van der Corput radical inverse of the sample index in a
  prime base — deterministic and stateless. Godot has no low-discrepancy sequence. Verified with exact
  radical-inverse values in base 2 (½, ¼, ¾, ⅛, …) and base 3 (⅓, ⅔, 1/9), all values in [0,1),
  base<2→0, the van-der-Corput property that the first 16 base-2 points hit each 1/16 bucket exactly
  once, a 4×4 grid fully covered by 64 2D points, and the stateful cursor matching the stateless calls),
  **reservoir sampling** (M431, `core::ReservoirSampler<T>` + `reservoirSample` — Vitter's algorithm R:
  uniformly pick k items from a stream of UNKNOWN length in a single pass and O(k) memory, feeding items
  one at a time while always holding k chosen so every item seen had an equal chance. The right tool for
  picking N random spawn points from a candidate stream, sampling events from a firehose, or keeping a
  bounded representative telemetry subset without unbounded memory. Deterministic from a caller Pcg32.
  Godot has no reservoir sampler. Verified: streams shorter than k keep all, k=0 stays empty, uniformity
  within 0.0015 of k/n over 300k trials, a long stream always holds exactly k valid items, the same seed
  reproduces the same reservoir, and reset() clears for reuse),
  **online running statistics** (M432, `core::RunningStats` — Welford's streaming algorithm: feed samples
  one at a time and read count / mean / population + sample variance / stddev / min / max / sum at any
  moment, in O(1) memory and a single pass without storing the samples. Welford's recurrence is
  numerically stable — no catastrophic cancellation from the naive sum-of-squares — so it stays accurate
  over millions of samples and large offsets. The tool for live frame-time/FPS stats, telemetry
  aggregates and adaptive-difficulty signals. Godot has no running-statistics accumulator. Verified
  against the textbook dataset {2,4,4,4,5,5,7,9} (mean 5, population variance 4, stddev 2, sample var
  32/7), single-element/constant-stream/empty edge cases, a 1e9-offset stream keeping variance exact,
  0..99 giving mean 49.5 and variance 833.25, and clear() reset),
  **histogram / distribution shape** (M433, `core::Histogram` — fixed-range equal-width binning: feed
  samples into a `[min, max)` range split into N bins and read back per-bin tallies, frequency, the mode
  (fullest bin), and interpolated percentiles / median — the distribution-shape questions a plain
  mean/variance cannot answer (e.g. "95th-percentile frame time"). O(bins) memory, nothing per sample.
  Out-of-range values clamp into the edge bins so the tallies always sum to the total, while separate
  below()/above() counters report how many spilled past each end. The tool for a frame-time/latency
  distribution readout, telemetry buckets, and damage/score-spread analysis. Godot has no histogram type.
  Verified: even one-per-bin layout, left-closed/right-open boundaries, out-of-range clamping with
  below/above accounting, mode tie-breaking, exact percentile interpolation (10-per-bin p25/median/p95 =
  2.5/5.0/9.5), uniform 0..999 median 500 and p90 900, clear() reset, degenerate-range safety, and a
  negative-range case proving true floor-not-truncation binning),
  **rolling-window statistics** (M434, `core::RollingWindow` — fixed-capacity sliding window that keeps
  only the last N samples and reports their sum / mean / min / max, dropping the oldest as each new one
  arrives. Unlike RunningStats (all-time) it answers "what have the last N samples been doing" — the
  shape for a live "average FPS over the last 60 frames" readout, a recent-input smoother, or a
  short-horizon trend signal. Running sum makes mean() O(1); min()/max() use monotonic index deques so
  they stay O(1) amortised as the window slides, with no per-eviction rescan. Godot has no
  rolling-window accumulator. Verified against fill-then-slide eviction, a duplicate-max stress case
  (a repeated maximum survives until both copies leave the window), capacity-1/0 edge cases, clear()
  reset, and a 3000-sample random stream cross-checked every step against a brute-force recompute of
  the window's sum/min/max),
  **PID feedback controller** (M435, `core::PidController` — the classic proportional-integral-derivative
  controller: given a target and a measurement each step it returns a control signal driving the
  measurement toward the target. P reacts to current error, I accumulates past error to erase
  steady-state offset (e.g. a constant disturbance P alone can't cancel), D damps via the error's rate
  of change. Includes anti-windup (integral accumulator clamp), output clamping, and a
  derivative-on-measurement mode that avoids the "derivative kick" a sudden setpoint change would cause.
  The workhorse behind self-correcting systems Godot has no built-in for: a turret tracking a moving
  target, a hovering vehicle holding altitude, an auto-throttle, a self-balancing biped, or an
  adaptive-difficulty signal chasing a target win-rate. Verified against pure-P/I/D behaviour, output
  and integral clamps, the derivative-kick removal, dt<=0 state-hold, reset(), and two closed-loop
  simulations against an integrator plant: P-only leaves the predicted steady-state offset under a
  constant disturbance while PI drives it to zero),
  **SmoothDamp critically-damped smoothing** (M436, `core::smoothDamp` + `core::SmoothDamp` — eases a
  value toward a possibly-moving target over an approximate time-to-reach, carrying velocity between
  frames, with no overshoot/ringing and an optional max-speed cap. The classic Game-Programming-Gems /
  Unity Mathf.SmoothDamp recurrence — the standard "buttery" follow for a trailing camera, a UI element
  gliding to rest, or a health bar catching up. Godot's Tween/lerp are fixed-duration, not
  velocity-carrying critically-damped springs, so this is a genuine gap-filler. Verified: converges
  with velocity settling to ~0, provably no overshoot from either side, max-speed rate limiting, dt<=0
  no-op, zero-smoothTime guard (finite, still converges), the stateful wrapper matching the free
  function bit-for-bit, reset() zeroing velocity, and stable tracking of a continuously moving target),
  **1€ (One Euro) adaptive filter** (M437, `core::OneEuroFilter` + `core::LowPassFilter` — Casiez/Roussel/
  Vogel 2012 adaptive low-pass for noisy human-driven input: it RAISES its cutoff as the signal speeds up,
  so nearly-still input is filtered hard (jitter vanishes) while fast input is filtered lightly (lag
  vanishes) — the two goals a fixed low-pass can't serve at once. Two intuitive knobs: minCutoff (smoothing
  at rest) and beta (how fast responsiveness ramps with speed). For denoising pointer/touch/stylus/VR-pose
  streams; distinct from SmoothDamp (chases a target, not denoises) and from a fixed EMA. Godot has no 1€
  filter. Verified: the LowPassFilter primitive (hold-first-sample, alpha 0/0.5/1 behaviour), constant
  input passing through unchanged, >10x variance reduction on a noisy stationary stream with the mean
  preserved, the core adaptivity property (on a fast ramp beta>0 lags strictly less than beta==0 while
  staying causal), and reset()/dt<=0 pass-through),
  **disjoint-set / union-find** (M438, `core::DisjointSet` — the near-O(1) structure for tracking which
  elements share a group as pairs merge, with path compression + union-by-rank (inverse-Ackermann
  amortised). The primitive behind connected-components queries, Kruskal MST, maze generation (carve a
  wall only when it joins two different regions), flood-region merging in a tile map, and island/cluster
  counting — all things games need and Godot ships no equivalent for. Maintains a live disjoint-set
  count and per-set sizes. Verified: singleton init, merge idempotence + self-union, component sizes, a
  1000-long chain exercising path compression, reset(), and a 60-element random-union run whose full
  partition is cross-checked against an independent BFS component labelling — every pair's connectivity
  and the total set count match),
  **Fenwick tree / binary indexed tree** (M439, `core::FenwickTree` — a mutable integer array that
  answers any prefix/range sum in O(log n) while still allowing O(log n) element updates (a plain array
  or a static prefix array can only make one of the two cheap). The key game use is DYNAMIC weighted
  random selection: findByPrefix() picks a bucket with probability proportional to its weight in
  O(log n), and unlike the build-once alias table (M428) the weights can change between draws — loot
  tables that shift with luck stats, spawn tables that deplete as a wave clears, cumulative-frequency
  sampling. Also serves running range sums over a mutable series. Godot has no Fenwick tree. Verified:
  add/at/prefix/range/total, exact findByPrefix bucket mapping (including that a zero-weight bucket is
  never selected), a 300k-draw weighted-sampling distribution that then re-checks the new proportions
  after a weight is zeroed live, and a 3000-op random add/set stream cross-checked against a brute-force
  array for element, prefix, and total),
  **trie / prefix tree** (M440, `core::Trie` — stores a set of words so exact-membership, "any word with
  this prefix?", prefix counts, and sorted autocomplete all run in time proportional to the query
  length regardless of how many words are held. The structure behind developer-console/chat command
  autocomplete, dictionary word validation (spelling, word games), and profanity/keyword filtering —
  none of which Godot ships a primitive for. Each node caches a subtree word count so prefix
  existence/counts stay exact even after erases (kept nodes, decremented counts). Verified: membership,
  prefix existence/counts, lexicographically-sorted collectWithPrefix autocomplete, erase updating both
  membership and prefix state, empty-string entries, and a full cross-check against a std::set over a
  dozen overlapping words),
  **fuzzy subsequence matching** (M441, `core::fuzzyMatch` + `core::longestCommonSubsequenceLength` —
  builds on the existing StringUtils edit-distance/bigram-similarity helpers (M290, reused not
  duplicated) with the two pieces a command palette actually needs: LCS length (longest in-order shared
  run, a distinct overlap measure), and an fzf/Sublime-style scorer that reports whether a short
  pattern's characters appear in order inside a candidate, where they landed, and how tight the match
  is — rewarding consecutive runs and matches on word boundaries (start / after a separator / camelCase
  hump), so "gw" ranks "getWidget" above scattered hits. Pairs with the Trie to rank
  abbreviation-style queries; Godot offers only a bigram similarity ratio. Verified: LCS known values,
  subsequence membership + landed positions, exact hand-computed scores (consecutive tight match beats
  the separated one, camelCase boundary bonus), case sensitivity, and a candidate-ranking pass that
  picks the exact match),
  **LRU cache** (M442, `core::LruCache<K,V>` — a fixed-capacity least-recently-used cache: holds up to N
  key->value entries and evicts the one untouched longest when a new key overflows, so the hot working
  set survives and the cold tail drops. O(1) get/put via a hash map into an intrusive recency list, with
  hit/miss counters. The standard bounded-memory memoization tool — caching decoded tiles/chunks,
  pathfinding results, or any expensive keyed computation — distinct from ResourceCache (ref-counted
  asset lifetimes, no eviction). Godot has no generic LRU. Verified: fill/order, get()/put() promotion
  with correct LRU eviction, value-update-on-reinsert, peek() leaving recency untouched, hit/miss stats,
  erase/clear, capacity-0 coercion, and a 20000-op random workload cross-checked every step against a
  reference LRU for size, MRU/LRU keys, per-value correctness, and full membership),
  **Bloom filter** (M443, `core::BloomFilter<T>` — a space-efficient probabilistic set: answers "have I
  definitely NOT seen this?" with certainty and "might I have?" with a small tunable false-positive
  chance, in a fraction of a real set's memory, with zero false negatives. The ideal fast pre-filter: a
  "visited" marker for the millions of cells/chunks in a huge procedural world, a duplicate suppressor
  for events/packets, or a cheap gate before an expensive exact lookup. k probes derived from a single
  std::hash by double-hashing, so it works for any hashable key; build by bit-count+probe-count or via
  optimal() from expected items + target false-positive rate. Godot has no Bloom filter. Verified: empty
  filter reports nothing present, the no-false-negatives invariant over 2000 keys, string keys, a
  5000-item filter whose measured false-positive rate lands near the 1% target with a Swamidass-Baldi
  count estimate within 10% of actual, clear(), and merge() unioning same-geometry filters while
  rejecting mismatched ones),
  **median-cut color quantization** (M444, `render::quantizePalette` + `mapToPalette` /
  `nearestPaletteIndex` — reduce an arbitrary set of RGB colors to a small representative palette by
  repeatedly splitting the color box with the widest channel spread at its median, then averaging each
  final box. The tool for retro/indexed-color looks (NES/GameBoy-style palettes), GIF-style export,
  texture palettization, and dominant-color swatches — none of which Godot provides. Verified:
  degenerate inputs, single-color -> one exact entry, near-lossless remap of well-separated inputs (mean
  squared error under threshold) with the palette respecting the budget, four jittered clusters
  resolving to four entries within 20 of each center with distinct nearest indices, and deterministic
  output),
  **dithering** (M445, `render::orderedDitherGray` + `render::floydSteinbergGray` + `render::bayerMatrix`
  — reduce a grayscale image to a few brightness levels while hiding the banding naive quantization
  produces. Ordered (Bayer-matrix) dithering adds a fixed tileable threshold pattern per pixel (the crisp
  retro/print look); Floyd-Steinberg error diffusion pushes each pixel's rounding error into its
  not-yet-processed neighbours (smoother, less patterned). The companion to color quantization for
  retro/1-bit/limited-palette looks — Godot has neither. bayerMatrix() also stands alone as a reusable
  threshold-matrix generator. Verified: exact Bayer matrices (2x2, the canonical 4x4, 1x1), flat extremes
  staying flat, a mid-gray field dithering to a 0/255 mix that averages back near the input on both
  methods (brightness preserved), output staying on the allowed level set, and determinism),
  **Worley / cellular noise** (M446, `core::WorleyNoise` — scatters one jittered feature point per unit
  grid cell and returns the distance to the nearest (F1) and second-nearest (F2) feature point for any
  sample. F1 makes rounded cell blobs (stone, scales, cracked mud, bubbles); F2 - F1 traces the ridges
  between cells (crack/vein networks). The scalar-texture cousin of the engine's geometric Voronoi
  diagram, filling the gap that Maz's Perlin-only `Noise` module (M85) left — reaching parity with
  Godot's FastNoiseLite cellular mode. Deterministic from a seed. Verified: determinism, F2 >= F1 >= 0
  with F1 bounded under sqrt(2), a dense scan landing within 0.05 of a feature point, crackle(F2-F1) >= 0
  hitting ~0 on cell boundaries, distinct fields per seed, and a sane mean-F1 band),
  **curl noise** (M447, `core::CurlNoise` — a divergence-free ("incompressible") 2D flow field: the flow
  vector is the perpendicular of the Perlin noise's gradient, so it runs along the potential's contour
  lines and its divergence is ~0. Particles carried by it swirl and fold without ever bunching or
  thinning — the look of smoke, wind, magic, fluid, and flowing hair that plain random/radial forces
  cannot give. Built on the existing Noise (M85, reused not duplicated); distinct from FlowField
  (goal-seeking pathfinding). Godot has no curl-noise primitive. Verified: determinism, EXACT
  perpendicularity to the gradient (curl . gradient == 0), a non-trivial field, a measured
  mean-|divergence| under 0.05 on a 40x40 grid (the incompressibility property), a finite non-trivial
  fbm variant, and distinct fields per seed),
  **color temperature (Kelvin -> RGB)** (M448, `render::kelvinToColor` — the Tanner Helland blackbody fit
  (~1000-40000 K): warm reddish tints at low temperatures (candle/tungsten), neutral daylight white near
  6500K, cool blue at high temperatures (overcast/shade). The tool for physically-plausible light tints:
  day/night cycles shifting the sun dawn-orange to noon-white, lamp/torch/fire glows, and
  white-balance-style grading. Godot has no built-in Kelvin->RGB helper. Verified: every output in [0,1]
  with alpha 1, warm 1500K reddish (r>g>b, blue ~0), 6600K near-neutral white, cool 10000K blue-dominant
  (b>=g>=r), monotone trends (blue rises and warmth r-b falls with temperature) across the range, and
  clamping outside [1000, 40000]),
  **value noise** (M449, `core::ValueNoise` — the sibling of the Perlin gradient noise in `core::Noise`:
  where Perlin interpolates random *gradients*, value noise interpolates random *values* stored at the
  integer lattice, giving a cheaper, rounder field for clouds/soft terrain/cheap textures. Mirrors
  Godot FastNoiseLite TYPE_VALUE (`value2`, quintic-smoothstep bilinear) and TYPE_VALUE_CUBIC
  (`value2Cubic`, Catmull-Rom bicubic), plus `fbm2` fractal layering. A self-contained integer hash makes
  each seed reproducible with no shared state. Verified: determinism (equal seeds agree exactly, distinct
  seeds decorrelate), value2 provably stays within [-1,1] and spans it, both variants pass EXACTLY through
  the lattice samples (interpolating-spline property), input continuity has no discontinuities, cubic is
  smooth and bounded, and single-octave fbm equals value2),
  **space-filling curves** (M450, `core::SpaceFilling` — Morton/Z-order (`mortonEncode2/Decode2`,
  `mortonEncode3/Decode3`, bit-interleaving up to 32 bits in 2D / 21 bits per axis in 3D) and the 2D
  Hilbert curve (`hilbertXY2D`/`hilbertD2XY`): they map grid coordinates to a single locality-preserving
  scalar index and back, the standard tool for cache-coherent grid iteration, spatial hash keys,
  quadtree/octree node ordering, and texture swizzling. Hilbert has strictly better locality than Morton
  — consecutive indices are ALWAYS orthogonal grid neighbours. Godot exposes no space-filling curve, so
  this is beyond-Godot. Verified as exact bijections: Morton2D roundtrip over 256x256 + full-32-bit
  extremes + known interleave constants, Morton3D roundtrip over 40^3 + 21-bit extremes, and Hilbert over
  orders 1-6 — every cell visited exactly once (bijection), inverse agrees exactly, and every consecutive
  step is Manhattan-distance 1),
  **CPU image blur** (M451, `render::gaussianBlurGray`/`boxBlurGray`/`gaussianKernel1D` — separable
  Gaussian and box blur over a row-major grayscale float image with clamp-to-edge borders: the
  offline/CPU counterpart to the GPU bloom/SSAO passes, for softening procedural textures and
  heightmaps, anti-aliasing SDFs, and baking soft AO/shadow into textures headlessly. Godot only blurs
  on the GPU, so a deterministic CPU blur is beyond-Godot. Verified with exact invariants: the kernel is
  positive, symmetric, and sums to 1; a constant image is returned unchanged (partition of unity); the
  separable result EXACTLY matches a full 2D outer-product convolution reference; a centred impulse
  conserves energy, peaks at the centre, and is 4-fold symmetric; and a box-blurred impulse is an exact
  uniform (2r+1)^2 block),
  **Kalman filtering** (M452, `core::Kalman1D` + `core::KalmanCV` — the statistically-optimal recursive
  estimator that fuses a noisy measurement with a model prediction while carrying its own uncertainty.
  Distinct from the engine's other smoothers (OneEuroFilter is a low-pass, SmoothDamp a spring,
  PidController a controller — none model measurement noise): Kalman1D is a scalar random-walk filter
  (predict inflates variance by Q, update folds in a measurement with gain K=P/(P+R)); KalmanCV is a
  constant-velocity tracker that recovers a smooth position AND an inferred velocity from noisy position
  samples (2x2 covariance carried as explicit symmetric terms). The right tool for de-noising jittery
  analog/sensor input, smoothing network-replicated positions, and light sensor fusion. Godot ships no
  Kalman filter -> beyond-Godot. Verified deterministically: gain always in [0,1]; R->0 snaps to the
  measurement while R->inf keeps the prediction; predict grows and update shrinks variance; over 4000
  seeded noisy samples of a constant the filter cuts error energy by >2x and tracks the truth; and the
  constant-velocity tracker recovers both position and velocity from clean linear motion with a
  positive-definite covariance),
  **ballistic aiming** (M453, `game::solveLaunchAngle`/`solveLaunchVelocity`/`projectilePosition`/
  `projectileApex`/`maxRangeFlat` — closed-form projectile-motion helpers for lobbing grenades, arcing
  arrows, and ranging artillery onto a target under gravity. solveLaunchAngle returns the two firing
  angles (flat direct shot + high mortar arc) for a fixed speed, or reports the target out of range;
  solveLaunchVelocity gives the exact throw velocity to land on a target in a chosen flight time. Godot
  has no ballistic solver -> beyond-Godot. Verified analytically: both returned angles pass exactly
  through the target height, flat-ground angles are complementary (sum to 90 deg) with the range gate at
  v^2/g, the solved velocity lands precisely on target at the requested time, pos(0) is the launch point,
  and the apex matches both the closed form and a densely-sampled trajectory peak),
  **quaternion swing-twist + nlerp** (M454, `math::swingTwist` + `math::nlerp` — swing-twist splits any
  rotation into a twist about a chosen axis and a perpendicular swing (q == swing * twist), the standard
  primitive for joint limits: a shoulder/knuckle twists freely about the bone while its swing cone is
  clamped, each limited independently after decomposing; nlerp is normalized linear interpolation, a
  cheaper torque-minimal alternative to slerp for per-frame blends. Godot's Quaternion has slerp/slerpni
  but neither swing-twist nor nlerp -> beyond-Godot. Verified: swing * twist reconstructs the rotation
  exactly across a spread of orientations and axes; the twist axis is parallel and the swing axis
  perpendicular to the chosen axis; a pure twist yields identity swing and a pure perpendicular swing
  yields identity twist; the 180-degree perpendicular singularity degrades to all-swing; and nlerp hits
  its endpoints, stays unit-length, and approaches the target monotonically),
  **reaction-diffusion** (M455, `game::ReactionDiffusion` — a Gray-Scott simulation that grows organic
  Turing patterns (spots, stripes, mazes, coral, mitosis) from two diffusing/reacting chemicals on a
  toroidal grid: the procedural source for animal-coat textures, rust/lichen growth, and alien-surface
  detail that static noise can't produce. Explicit-Euler step with a 5-point Laplacian; feed/kill/
  diffusion presets (mitosis, coral). Godot has no reaction-diffusion -> beyond-Godot. Verified: pure
  diffusion (F=K=0, V=0) conserves total U mass EXACTLY (the toroidal Laplacian sums to zero) and reduces
  variance; the step is fully deterministic; a seed grows real V structure while staying finite and
  bounded in ~[0,1]; a centre-symmetric seed keeps the field mirror-symmetric; and a zero-size grid is a
  safe no-op),
  **natural cubic spline** (M456, `math::CubicSpline` — the globally-smooth interpolating spline: given
  knots (x_i, y_i) it builds the unique piecewise cubic that passes exactly through every knot and is
  C2-continuous everywhere (continuous value, slope, AND curvature) with natural (zero-curvature) ends,
  solved with the O(n) Thomas tridiagonal algorithm. Distinct from Curve2D (Bezier, not interpolating),
  anim::Curve (keyframe track), and the C1-local Catmull-Rom in VectorOps — this is the tool for a smooth
  camera dolly through waypoints or a terrain cross-section through samples. Godot's curves are
  Bezier-based, so a true natural cubic spline is beyond-Godot. Verified: it passes exactly through every
  knot; second derivative is exactly zero at both ends (natural BC); slope and curvature are continuous
  across interior knots; linear data is reproduced exactly (value, slope, zero curvature); even data
  yields a symmetric curve; two knots reduce to a clamped straight line; and malformed inputs are
  rejected),
  **minimum spanning tree** (M457, `game::minimumSpanningTree` — Kruskal's algorithm over a weighted
  undirected graph: the cheapest edge set that keeps every node connected without cycles, reusing
  core::DisjointSet. The procgen tool for wiring up scattered dungeon rooms with the shortest total
  corridor length, laying out road/river/power networks, or clustering; disconnected input yields the
  minimum spanning FOREST. Godot has no MST -> beyond-Godot. Verified: a hand-computed graph gives the
  exact expected weight; the result is always a valid tree (n-1 edges, all connected, cycle-free) and its
  total weight matches an independent Prim reference across 40 random graphs; the globally-lightest edge
  is always chosen (cut property); a disconnected graph yields the right forest (edges = n - components);
  the output is deterministic; and self-loops / out-of-range edges are ignored),
  **minimax + alpha-beta** (M458, `game::minimax` + `game::GameRules` — generic adversarial game-tree
  search for perfect-information two-player games (tic-tac-toe, connect-four, reversi, checkers-likes):
  supply move-generation / apply / terminal / evaluate callbacks and it returns the optimal move and its
  value `depth` plies ahead, with alpha-beta pruning that explores far fewer nodes for the identical
  result. Godot ships no game-tree search -> beyond-Godot. Verified on tic-tac-toe against known game
  theory (perfect play from empty = draw; immediate wins taken by both max and min players; the forced
  block is found and skipping it loses) and cross-checked against an unpruned reference minimax across
  300 random reachable positions — identical value every time, never visiting more nodes, and strictly
  fewer on the opening),
  **L-system** (M459, `game::LSystem` + `game::interpretTurtle` — a Lindenmayer grammar rewriter plus a
  turtle-graphics interpreter: expand an axiom by symbol->replacement rules, then walk the result with a
  turtle (F/G draw, f move, +/- turn, [ ] branch push/pop) to emit line segments. The compact procedural
  source for plants, trees, roots, and space-filling fractals (Koch, dragon, Sierpinski), and for growing
  branching corridors or river networks. Godot has no L-system -> beyond-Godot. Verified: Lindenmayer's
  algae (A->AB, B->A) produces the exact expected strings with Fibonacci lengths (1,2,3,5,8,13,21,34);
  rule-less symbols pass through as constants; quadratic Koch yields exactly 5^n drawn segments; turtle
  geometry is exact (F+F traces an L, '+' turns left / '-' right); brackets return the turtle to the fork
  so branches don't displace the trunk; 'f' moves without drawing; unknown symbols and an unbalanced ']'
  are safe no-ops; and generation is deterministic),
  **wave function collapse** (M471, `game::wfcGenerate` + `game::WfcRules` -> `game::WfcResult` —
  constraint-based procedural tile generation. Given a palette of up to 64 tile types and adjacency
  rules ("water may touch sand, sand may touch grass, but water never touches grass"), it fills a grid
  so EVERY neighbouring pair obeys the rules, producing coherent non-repeating layouts from a few local
  constraints. Distinct from the engine's other generators (CellularCave carves, L-systems grow, noise
  makes fields): WFC solves a constraint problem. Each cell holds a bitset superposition of possible
  tiles; the solver repeatedly collapses the lowest-entropy cell to a single weighted-random tile and
  propagates the consequences to neighbours by arc-consistency; a contradiction triggers a re-seeded
  retry. Godot has no WFC -> beyond-Godot. Deterministic under a fixed seed. Verified: a land/coast/sea
  rule set where land must never touch sea produces, across 20 seeds, grids where every adjacency is
  permitted and no land cell is 4-adjacent to sea; the same seed yields identical output; fully-permissive
  rules always succeed; per-tile weights that zero out all but one tile force that tile everywhere; a 1x1
  grid collapses to one valid tile; and zero-size or empty-rule inputs fail cleanly),
  **mesh subdivision** (M472, `render::subdivideMesh` (linear) + `render::subdivideMeshLoop` (smooth) ->
  `render::SubdivMesh` — refine an indexed triangle mesh into a denser one. Linear (midpoint) splits every
  triangle into four by its edge midpoints, leaving the surface unchanged — extra vertices for
  displacement, vertex lighting or wave deformation. Loop subdivision is the standard subdivision surface:
  new points are weighted averages (interior edge point 3/8(a+b)+1/8(c+d), boundary 1/2(a+b), even
  vertices re-weighted by the Loop beta) that round a blocky low-poly cage toward the smooth limit surface
  — the runtime counterpart to a modelling package's subdivision modifier. Both share one topology step
  (every triangle -> four, one new vertex per unique edge). Godot has no runtime subdivision -> beyond-
  Godot. Verified: linear split of a triangle gives 4 tris / 6 verts with the three exact edge midpoints;
  counts obey the V+E / 4T rule across three iterations on a quad; a planar mesh stays planar under both
  linear and Loop; Loop on a flat square stays inside the square (convex weights); Loop on a cube pulls
  every vertex strictly inside the cube's bounding sphere (smoothing inward); and zero-iteration / empty
  inputs are safe no-ops),
  **vertex welding** (M473, `render::weldVertices` -> `render::WeldedMesh` — merge coincident (or
  near-coincident) vertices of an indexed triangle mesh into one, remapping the indices and dropping
  triangles that collapse to a line. The cleanup pass a mesh needs after being built face-by-face (each
  quad emitting its own corners), from CSG or marching cubes, or an import that duplicated shared vertices
  along every seam — a shared corner must be ONE vertex for smoothing groups, subdivision, and normal
  generation to average correctly. This is Godot's SurfaceTool.index() with a distance threshold (Godot
  welds only bit-exact duplicates; the threshold is the extra step -> parity-or-better). A spatial hash
  keyed by the weld cell finds candidates in O(n) expected, checking the 3x3x3 neighbourhood so points
  either side of a cell boundary still merge; the first occurrence of each cluster is the representative
  so surviving positions are unchanged. Verified: a cube built face-by-face (24 duplicated corners) welds
  to exactly 8 unique vertices with all 12 triangles surviving and every kept position matching an
  original corner; the threshold merges within epsilon (collapsing a degenerate triangle, counted in
  removedTriangles) and keeps beyond it; two points straddling a cell boundary still merge; an
  already-unique mesh is unchanged; epsilon<=0 welds bit-exact duplicates; empty input is safe),
  **mesh smoothing** (M474, `render::smoothMeshLaplacian` + `render::smoothMeshTaubin` — relax the
  vertices of an indexed triangle mesh toward the average of their neighbours, ironing out noise and
  faceting WITHOUT changing topology (vertices moved, none added or removed). The denoise pass for a mesh
  built from noisy data: a marching-cubes isosurface, an fbm-perturbed heightfield, a voxelised blob.
  Plain Laplacian smoothing shrinks the shape (every point drifts inward); the Taubin lambda|mu two-pass
  alternates a positive smoothing step and a slightly larger negative unshrinking step so the surface
  relaxes while its volume is preserved — the standard low-pass mesh filter. Boundary vertices can be
  pinned so open edges keep their shape. Godot exposes no runtime mesh smoothing to gameplay code ->
  beyond-Godot. Verified: a flat mesh stays flat and keeps its vertex count; Laplacian cuts interior
  z-noise on a bumpy grid by more than half; on a closed icosahedron plain Laplacian collapses the shape
  inward (average radius under 20% of the original) while Taubin resists the shrinkage (over 40%, more
  than 5x better volume retention); pinned boundary vertices don't move; and empty input is safe),
  **sparse table (RMQ)** (M460, `core::SparseTable<T, Op>` — O(1) range min/max (or any idempotent
  associative op: gcd, bitwise and/or) over a STATIC array after an O(n log n) build, by overlapping two
  power-of-two blocks. Complements FenwickTree (dynamic prefix sums with point updates) with far faster
  read-only range extremes — the tool for "tallest terrain height in this span" and static interval
  min/max. Godot exposes no RMQ structure -> beyond-Godot. Verified against brute force over EVERY (l, r)
  pair across 100 random arrays for both min and max, plus known values, float payloads, single-element
  ranges, and empty/one-element tables),
  **all-pairs shortest paths** (M461, `game::allPairsShortestPaths` — Floyd-Warshall: the shortest
  distance between EVERY pair of nodes in a weighted graph in one O(V^3) pass, plus a next-hop matrix for
  path reconstruction. Unlike the engine's single-source pathfinders (AStar2D, DijkstraMap) it fills a
  whole distance matrix — the tool for a precomputed routing/influence table on a small graph, "which of
  my bases is nearest to each threat", static AI cost tables. Handles negative edges (and detects
  negative cycles), directed or undirected. Godot's AStar is single-pair, so all-pairs is beyond-Godot.
  Verified: self-distance 0 and unreachable = infinity; a hand-computed graph gives exact distances and
  the reconstructed path; every source's distances match an independent Dijkstra reference across 40
  random graphs; reconstructed paths start/end correctly with matching total weight; a negative edge is
  used correctly and a negative cycle is detected),
  **polynomial root solvers** (M462, `math::solveQuadratic` + `math::solveCubic` — closed-form real roots
  of quadratics (numerically-stable form) and cubics (Cardano + the trigonometric three-real-root case),
  sorted and de-duplicated. The exact building blocks behind ray/sphere and ray/quadric intersection,
  solving for the time a projectile reaches a height, and inverting a cubic ease. Godot has no general
  polynomial solver -> beyond-Godot. Verified: built from 400 random known-root quadratics and 400 random
  cubics, both recovered exactly with near-zero residuals; plus edge cases — no-real-root, double and
  triple roots, the linear/quadratic degrade paths, and a one-real-root cubic),
  **numerical ODE integrators** (M463, `math::integrateRK4` / `integrateRK2` / `integrateEuler` /
  `integrateRK4Steps` — classic Runge-Kutta and Euler steppers that advance any continuous system state
  by dt given its derivative callable deriv(state, t). The engine's physics uses semi-implicit Euler for
  contacts; these are the accurate general-purpose integrators for smooth continuous motion where Euler
  drifts — orbital/n-body motion, springs and pendulums, projectiles with air drag, any custom equation of
  motion. RK4 is 4th-order (its error shrinks ~16x each time the step halves) and works on plain floats, a
  vec, or a small phase-space struct. Godot exposes no general integrator to gameplay code -> beyond-Godot.
  Verified: y'=-y and y'=y recovered to e^-1 and e^1; constant derivative integrated exactly; a harmonic
  oscillator conserves energy and tracks the analytic cosine over 6+ periods; measured 4th-order
  convergence (error ratio near 16 as the step halves); and RK4 beats Euler at the same step),
  **least-squares curve fitting** (M464, `math::fitLine` / `math::fitPolynomial` / `math::evalPolynomial`
  — recover the line or polynomial that best matches a cloud of (x, y) samples in the ordinary
  least-squares sense, with the R^2 goodness-of-fit reported for each. fitLine gives slope/intercept in
  closed form; fitPolynomial fits any degree via the normal equations (A^T A c = A^T y) solved with an
  internal Gaussian-elimination-with-partial-pivoting dense solver. The tool for turning noisy
  measurements into a smooth trend — fit a straight line through a scatter of points, calibrate an
  analog-stick or sensor response curve, model a difficulty ramp from playtest data, or smooth a jittery
  signal with a low-degree polynomial. Godot exposes no curve-fitting to gameplay code -> beyond-Godot.
  Verified: perfectly collinear data recovers slope/intercept with R^2==1; 100 random noisy lines recover
  their trend (slope/intercept within tolerance, R^2>0.98); exact quadratic and cubic coefficients
  recovered at the matching degree; the degree-1 polynomial fit agrees with the closed-form line fit;
  higher degrees fit genuinely curved (sine) data strictly better than a line (R^2 rising to >0.999);
  degree-0 returns the mean; and all degenerate inputs — too few points, negative degree, mismatched
  lengths, all-x-identical — are rejected via the ok flag),
  **numerical quadrature** (M465, `math::integrateTrapezoid` / `integrateSimpson` /
  `integrateAdaptiveSimpson` / `integrateRomberg` — compute the definite integral (signed area under
  the curve) of any scalar function f(x) supplied as a callable, over [a, b]. The counterpart to the ODE
  integrator (which advances a state through time); quadrature sums a function's area. The tool for arc
  length of a parametric path (integrate the speed |r'(t)|), work done by a varying force over a
  distance, a cumulative distribution from a density, or any total-accumulated-quantity where the
  integrand is known as code. Four rules, cheapest to most accurate: composite trapezoid (O(h^2)),
  composite Simpson (O(h^4), exact for cubics), adaptive Simpson (recursively refines only where the
  integrand is hard, to a requested tolerance), and Romberg (Richardson extrapolation on the trapezoid
  rule for fast convergence on smooth integrands). Godot exposes no general function integrator to
  gameplay code -> beyond-Godot. Verified against closed-form integrals: x^2->1/3, x^3 exact under
  Simpson with n=2, sin over [0,pi]=2, e^x=e-1, constants, reversed limits negate; adaptive Simpson
  resolves a Runge-type spike far better than a coarse trapezoid; Romberg error shrinks with more levels;
  a Gaussian bell integrates to sqrt(pi); and the arc length of y=x^2 matches its analytic value),
  **root finding** (M466, `math::findRootBisection` / `findRootNewton` / `findRootSecant` /
  `findRootBrent` — solve f(x)=0 for any scalar function supplied as a callable, returning the root plus
  a converged flag and iteration count. The inverse-problem companion to Polynomial.hpp (which solves
  *known* quadratics/cubics in closed form): many gameplay questions reduce to "find the x where this
  custom function crosses zero" — the launch angle that lands a projectile on a moving target, the time a
  value curve first hits a threshold, the path parameter nearest a point, or inverting any monotonic
  response curve. Bisection is bracketing and utterly reliable (linear); Newton-Raphson uses the
  derivative for quadratic convergence; secant is derivative-free and superlinear; Brent is a bracketing
  hybrid of bisection + secant + inverse-quadratic interpolation — robust like bisection but usually much
  faster, the recommended default. Godot exposes no root finder to gameplay code -> beyond-Godot.
  Verified: sqrt(2), cos(x)=x, x^3-x-2, and e^x=3x+1 recovered by all applicable methods to matching
  values (Newton/Brent converging in fewer iterations than bisection); an exact-root endpoint detected
  immediately; a no-bracket interval and a flat Newton derivative both reported as not converged (no
  false root, no divide-by-zero); and a decaying-value threshold-crossing time matches its closed form),
  **descriptive statistics** (M467, `math::mean` / `variance` / `standardDeviation` / `median` /
  `quantile` / `minValue` / `maxValue` / `range` / `covariance` / `correlation` — summarize a dataset of
  samples held in memory. Where core::RunningStats is a streaming single-pass estimator and
  core::Histogram gives binned approximate percentiles of a distribution's shape, this computes EXACT
  order statistics (median and arbitrary quantiles by linear interpolation, NumPy's default method) plus
  the two-variable measures neither provides: covariance and Pearson correlation between a pair of
  datasets. Sample vs population vari/covariance are both offered. The tool for analyzing playtest and
  telemetry samples ("median session length", "95th-percentile damage", "does accuracy correlate with
  score"), or validating a procedural generator against a target distribution; it pairs with LeastSquares
  (fit a trend) — correlation quantifies how linear that trend is. Godot exposes no statistics helpers to
  gameplay code -> beyond-Godot. Verified: the classic {2,4,4,4,5,5,7,9} textbook set (mean 5, population
  variance 4, sample variance 32/7, median 4.5); linear-interpolated quantiles including clamped
  out-of-range q; covariance(x,x) equals variance(x); perfect linear data gives correlation +/-1
  (symmetric, and invariant under positive scale+shift); a noisy upward trend gives strong bounded
  positive correlation; and all degenerate inputs — empty, single-point sample variance, constant input,
  length mismatch — return 0 without NaN),
  **3D bounding sphere** (M468, `math::boundingSphere` -> `math::Sphere` — the smallest sphere enclosing
  a cloud of 3D points, the 3D companion to Geometry2D's minEnclosingCircle. A tight bounding sphere is
  the cheapest proxy for an object's extent: one sphere-vs-plane test per object for frustum culling,
  distance-to-sphere for broad-phase overlap and LOD selection, or a trigger/aggro radius fitted to an
  actual mesh. Computed with Welzl's incremental minimal-enclosing-sphere algorithm — the EXACT 3D
  analog of the 2D minidisk, not an approximation — with a final growth pass guaranteeing every point is
  enclosed under round-off. Godot computes AABBs but exposes no bounding-sphere fit to gameplay code ->
  beyond-Godot. Verified: the 8 cube corners give the exact minimum (origin, radius sqrt(3)); 300 points
  sampled on a known sphere recover its centre and radius within ~1%; and across 50 random clouds every
  point is enclosed while the radius stays within the hard bounds (diameter >= widest pair, radius <=
  point spread); degenerate empty/single/pair inputs handled),
  **oriented bounding box fit** (M469, `math::fitObb` -> `math::Obb` — the tightest *rotated* box
  around a 3D point cloud, found by principal component analysis. An axis-aligned box wastes space on a
  slanted object; an OBB aligns its own axes to the cloud's dominant directions for a far tighter proxy —
  the fit behind tight collision proxies, oriented editor gizmos, and better broad-phase bounds than an
  AABB. Method: centre the points, form their 3x3 covariance matrix, take its eigenvectors (symmetric
  Jacobi solve) as the box axes, project onto those axes to size the box; reuses the engine's Obb type
  (contains / aabb / SAT overlap). Godot fits neither spheres nor oriented boxes to point sets in
  gameplay code -> beyond-Godot. Verified: an axis-aligned box recovers its exact extents and centre; a
  box rotated about two axes recovers its tight half-extents (sorted) and yields a box volume far below
  the AABB volume of the same rotated cloud; the fitted axes are orthonormal; every point is enclosed;
  degenerate empty/single inputs handled),
  **number formatting for HUDs** (M475, `core::groupThousands` / `clockDuration` / `compactDuration` /
  `abbreviateNumber` / `formatBytes` — turn raw numbers into the strings a game shows: a score with
  thousand separators ("1,000,000"), a timer as a clock ("1:23:45") or a compact span ("1h 23m 45s"), a
  big idle-game count abbreviated ("1.2M"), or an asset size in bytes ("1.5 MiB"). The engine had
  DateTime::formatTime for a wall-clock instant but nothing for elapsed durations or grouped/abbreviated
  magnitudes; Godot's String offers num/pad but not these -> parity-or-better. Locale-independent (the
  separator is an explicit argument). Verified exact strings across grouping (incl. INT64_MAX, negatives,
  a European '.' separator), clock durations (M:SS vs H:MM:SS split, negative clamp, fractional floor),
  compact durations, K/M/B/T abbreviation (trailing zeros trimmed, custom precision, negatives), and
  binary IEC byte sizes),
  **RPG stat / modifier system** (M476, `game::Stat` + `game::StatModifier` — a character attribute
  (health, damage, move-speed) as a base value plus a stack of modifiers that combine in the standard
  game-design order: all FLAT bonuses add first (base + 5 + 10), then all ADDITIVE-PERCENT modifiers sum
  and apply once (+10% +20% = x1.3), then each MULTIPLICATIVE-PERCENT modifier applies in turn (a separate
  x1.5 on top), so buffs and gear stack predictably instead of order-dependently. Each modifier carries a
  `source` id so a whole buff or an unequipped item's bonuses drop in one `removeModifiersFromSource`
  call. Godot ships no stat system — games hand-roll this every time -> beyond-Godot gameplay utility.
  Verified: an empty stack equals the base; flat modifiers add; additive percents sum to one multiplier;
  multiplicative percents chain; the canonical worked example (100 -> +20 -> 120 -> x1.3 -> 156 -> x1.5 ->
  234) is exact; the final value is independent of insertion order; source removal restores the prior
  value and reports whether anything was removed; `valueClamped` floors/ceils correctly; `setBase`
  recomputes),
  **slot-based stackable inventory** (M477, `game::Inventory` + `game::ItemStack` — the loot bag / chest /
  hotbar backbone: a fixed array of slots, each empty or holding a stack of one item id up to a
  per-inventory stack limit (Minecraft's 64, an RPG's 99). `addItem` fills existing partial stacks of the
  same id first, then spills into empty slots, so a scattered bag consolidates naturally, and returns any
  leftover that did not fit; `removeItem` pulls from every stack of an id and returns how many it actually
  took; plus `count` / `has` / `freeSpaceFor` / `firstEmptySlot` / `isFull` / `isEmpty` / `swapSlots` /
  `clear`. Godot ships no inventory system — every game hand-rolls one -> beyond-Godot gameplay utility.
  Verified: adding within/over a stack consolidates then spills (25 of stack-10 -> 10/10/5); capacity
  overflow reports exact leftover and a full bag rejects other items; `freeSpaceFor` exactly predicts what
  `addItem` accepts (leftover 0); remove draws across stacks, caps at what is present, frees emptied slots
  for reuse; `swapSlots` reorganises without changing totals and ignores out-of-range; degenerate
  constructor args clamp to >=1 slot / >=1 stack; negative/zero add args are safe no-ops),
  **weighted loot table** (M478, `game::LootTable` + `game::LootEntry` / `game::LootDrop` — the drop
  system behind chests, defeated enemies, and treasure rolls: a list of entries, each an item id with a
  relative weight and a `[minCount, maxCount]` quantity range. `roll(rng)` picks ONE entry with
  probability proportional to its weight and yields a uniform count within that entry's range; weights are
  relative (never need to sum to 1) and an entry with id < 0 models a "nothing dropped" outcome that can
  win the roll and reports `empty()`. Driven by the engine's deterministic `core::Pcg32`, so a seed
  reproduces the exact loot (replays, shareable seeds); `rollMany` batches independent rolls. Godot ships
  no loot-table resource — games hand-roll weighted drops every time -> beyond-Godot gameplay utility.
  Verified: a single entry always drops its item with count spanning both range extremes; fixed count
  (min==max) is exact; same seed reproduces an identical 1000-roll sequence; over 200k rolls a 1/3/6
  weight split lands within 1% of 10%/30%/60%; zero and negative weights never drop; a "no drop" entry
  wins ~50% and reports empty; `rollMany` returns exactly N (and empty for N<=0); a reversed count range
  is tolerated; `clear` resets),
  **experience / leveling system** (M479, `game::LevelCurve` + `game::ExperienceTrack` — the
  character-progression backbone behind XP bars, "level up!" popups, and difficulty pacing. A LevelCurve
  defines the XP cost to advance each level via one of three shapes — `linear` (arithmetic growth),
  `geometric` (each level a fixed percentage more than the last — the classic RPG feel), or an explicit
  hand-authored `table` — and answers `costToNext`, `cumulativeToReach`, and `levelForTotalXp`. An
  ExperienceTrack is the live counter: `addXp` accumulates (or removes) XP and returns the change in
  level, and `level` / `xpIntoLevel` / `xpForNextLevel` / `progress` / `isMaxLevel` drive the UI. Costs
  and totals are integers so level boundaries never drift, and every cost is clamped to >= 1 so a curve
  can never grant infinite instant levels. Godot ships no leveling system — games hand-roll XP curves
  every time -> beyond-Godot gameplay utility completing the stat / inventory / loot RPG suite. Verified:
  linear cost/cumulative/threshold arithmetic exact; `cumulativeToReach` and `levelForTotalXp` are perfect
  inverses at every one of 50 level thresholds (and one XP below each lands the previous level); geometric
  growth rounds correctly (100 -> 110 -> 121 -> 133 at +10%); table caps at size+1 and stops earning at
  max; zero-cost curves clamp to 1 (no infinite levels); the track levels up/down on add/remove, floors
  total at 0, and reports progress 1.0 / next-cost 0 at max level),
  **ability cooldown manager** (M480, `game::CooldownManager` — the timer bank behind spell cooldowns,
  dashes, the global cooldown, and any "you can't do that yet" gate. Abilities are keyed by an integer id;
  `tryUse(id, duration)` fires an ability and puts it on cooldown in one call (returning false if it is
  still recharging), `tick(dt)` advances every active timer by the frame delta, and `isReady` /
  `remaining` / `fraction` drive the greyed-out button and the radial cooldown sweep. `reduce` applies a
  haste / cooldown-reduction effect, `reset` / `clear` free abilities, and timers that reach zero are
  dropped so an idle manager holds nothing. Godot ships Timer nodes but no ability-cooldown abstraction —
  games wire this up by hand every time -> beyond-Godot gameplay utility. Verified: a fresh manager is
  ready everywhere; `tryUse` fires once then gates while recharging; `tick` counts down, clamps at zero
  (no negative on overshoot), and frees the ability, with `fraction` tracking 1.0->0.0; `start` refreshes
  a running timer to full; multiple ids stay independent; `reduce` shortens and can free (and no-ops on a
  ready ability); `reset` / `clear` work; a non-positive duration means immediately ready (and clears an
  active timer); non-positive dt is ignored),
  **crafting system** (M481, `game::Recipe` + `game::RecipeBook` + `canCraft` / `craft`, built directly on
  `game::Inventory` — turns a bench of recipes plus an inventory into "combine these items to make that
  item." A Recipe lists the input ids/quantities it consumes and the output id/quantity it produces;
  `canCraft` checks the inventory has every ingredient and room for the result, and `craft` atomically
  consumes the inputs and adds the output (leaving the inventory untouched on failure). A RecipeBook stores
  many recipes and reports which ones an inventory can currently make (`craftable` / `firstCraftable`) —
  the data behind a crafting menu that greys out what you can't yet build. Crafting and carrying share one
  item model (ItemStack ids/quantities). Godot ships no crafting system — games hand-roll it every time ->
  beyond-Godot gameplay utility. Verified: `canCraft` flips false->true as the last ingredient arrives;
  `craft` consumes the exact inputs and adds the output, stacks a repeated craft's output, and fails
  leaving the inventory byte-for-byte unchanged when short an input OR when the inventory has no room for
  the result; output that stacks into an existing partial stack counts as room; a zero-quantity input
  demands nothing; recipes with no output id / zero output count are rejected; the book lists craftable
  indices in order and reports -1 when none can be made),
  **quest / objective tracker** (M482, `game::QuestLog` + `game::Quest` / `game::Objective` — the journal
  behind "kill 5 goblins", "collect 3 keys", and the "Quest Complete!" banner. A quest is a set of counted
  objectives (each with a target and progress) that carries a state (Inactive -> Active -> Completed /
  Failed) and auto-completes when every objective is met. The canonical driver is `advance(objectiveId,
  amount)`: the game reports an event once and every ACTIVE quest with a matching objective moves forward,
  returning how many quests finished on that call so the UI can celebrate; `addProgress` targets a single
  quest, `failQuest` abandons one, and `progress` / `objective` / `activeQuests` / `completedQuests` drive
  the journal screen. Godot ships no quest system — games hand-roll it every time -> beyond-Godot gameplay
  utility. Verified: the Inactive -> Active -> Completed lifecycle (dup ids and unknown ids rejected);
  `advance` fires only on active quests, clamps at the target, completes exactly when the last objective is
  met, and returns the newly-completed count (with no re-trigger afterward); several active quests sharing
  an objective id all advance; a multi-objective quest completes only when ALL objectives are met (progress
  0.5 midway); `addProgress` moves one quest without touching another; `failQuest` freezes progress; an
  objective-less quest completes on start; the active/completed lists and all unknown-quest queries are
  safe),
  **status-effect system** (M483, `game::StatusEffectSystem` + `game::StatusEffect` / `game::StatusTick` —
  the timed buff / debuff layer behind poison, regeneration, haste, and burning. Each effect has a type id,
  a remaining duration, a stack count, and an optional periodic interval; `update(dt)` counts every effect
  down, fires a "tick" each time an effect's interval elapses (carrying the sub-interval remainder), and
  drops effects whose duration runs out. Ticks are reported with the effect's current stack count so the
  game multiplies per-stack damage/heal without owning any timer. Re-applying follows a StackMode —
  Refresh (reset timer, replace stacks), Add (add stacks up to a cap, refresh timer), or Keep (ignore while
  active). This is deliberately DISTINCT from the untimed stat-modifier stack (M476) and the one-timer-per-
  ability cooldown bank (M480): status effects are the durational, pulsing on-entity effects. Godot ships
  no status-effect system — games hand-roll it every time -> beyond-Godot gameplay utility. Verified: apply
  / query with bad args rejected; duration counts down and the effect auto-expires; periodic ticks fire on
  exact interval boundaries and carry the remainder (0.5+0.5 -> one tick, 2.5 -> two ticks + 0.5 carried);
  ticks report the live stack count; Refresh resets duration and replaces stacks; Add accumulates to the
  cap; Keep leaves a running effect untouched; multiple effects tick independently and expire on their own
  schedules; remove / clear work; non-positive dt is a no-op),
  **shop / merchant economy** (M484, `game::Shop` + `game::ShopItem` + `game::TradeResult`, built on
  `game::Inventory` — the vendor counter behind "buy" and "sell." A Shop is a catalogue of items, each with
  a gold price and an optional stock count (-1 = unlimited); `buy` charges the player's gold wallet, hands
  over the item, and draws down stock, while `sell` takes the item back and pays a fraction of its price
  (the sell margin — shops pay less than they charge). The wallet is passed by reference so one purse
  visits many shops; every trade is atomic and fully guarded, returning a typed TradeResult (NotForSale /
  OutOfStock / NotEnoughGold / NoInventoryRoom / NotEnoughItems) and leaving wallet and bag untouched on
  any failure. Godot ships no shop/economy system — games hand-roll it every time -> beyond-Godot gameplay
  utility. Verified: pricing (buy price, sell price = round(price*margin), -1 for unlisted); a successful
  buy debits gold, adds the item, and drops finite stock; each buy failure mode (too little gold, out of
  stock, no inventory room, not for sale) changes nothing; unlimited stock never depletes; sell pays the
  margin price, removes the item, and grows finite stock; selling more than owned or an unlisted item is
  rejected; the margin clamps to [0,1] and rounds; restock and catalogue re-add overwrite; a zero-quantity
  trade is a safe no-op),
  **branching dialogue system** (M485, `game::DialogueTree` + `game::DialogueRunner` + `game::DialogueNode`
  / `game::DialogueChoice` — the conversation graph behind NPC talk, cutscene lines, and "[1] Accept /
  [2] Decline" prompts. A DialogueTree is a set of nodes, each with a speaker, a line of text, and either a
  list of player CHOICES (each linking to the next node) or a single auto-advance `next` link for a
  straight run of lines. A DialogueRunner walks the tree: `current` is the line on screen, `choose(i)`
  follows a branch, and `advance()` steps a choice-less line forward, ending the conversation when a link
  points to -1. The tree is pure data, so one tree drives many NPCs at once via independent runners. Godot
  ships no dialogue system (games hand-roll it or add a third-party plugin) -> beyond-Godot gameplay
  utility. Verified: a yes/no branch reaches the correct terminal line on each path; an out-of-range choice
  and `advance()` on a line that still has choices are both no-ops that leave the cursor put; a linear
  `setNext` chain walks A->B->C and then ends; a choice whose link is -1 ends the conversation immediately;
  an invalid start id begins finished with safe choose/advance; bad authoring node ids are ignored),
  **combat damage resolver** (M486, `game::resolveDamage` + `game::DamageInfo` / `game::DamageResult` +
  `game::armorMultiplier` — the "how much damage actually lands" math behind every hit. Given a raw amount
  and the defender's mitigation (scaling armor, a fractional resistance, optional flat reduction) plus an
  optional crit, it returns the final integer damage and whether the blow crit or was fully blocked. Armor
  uses the classic diminishing-returns curve 100/(100+armor) (100 armor halves, 200 armor thirds, never
  negative), resistance scales by (1-resist), and a `True` damage type bypasses all mitigation. Crit is a
  caller-supplied flag (roll it with the engine's Pcg32) so the resolver stays pure and deterministic.
  Godot ships no damage/combat system -> beyond-Godot gameplay utility pairing with the stat and
  status-effect systems. Verified: the armor curve (0->x1, 100->x0.5, 200->x1/3, negative clamps); no-
  mitigation passthrough; crit multiplies the raw amount before armor (and a custom multiplier); resistance
  scaling and its clamp to a full block; flat reduction after scaling clamping to a full block; the full
  pipeline (100 -> crit x2 -> armor 100 -> resist .25 -> flat 5 = 70); True damage ignoring armor/resist/
  flat while still critting; zero/negative amounts yielding 0 without a false "blocked" flag; and
  round-to-nearest on fractional results),
  **turn-order / initiative scheduler** (M487, `game::TurnOrder` — the initiative queue behind tactics and
  JRPG combat, where each combatant acts in order of a speed / initiative score, highest first, looping
  round after round. `start` opens round 1 by sorting combatants (ties broken toward the lower id for
  determinism), `current` names whose turn it is, and `advance` steps to the next combatant, rolling into a
  fresh round — re-sorting to pick up any initiative changes — once everyone has acted. Combatants can be
  removed mid-battle (a defeated enemy is skipped immediately, and if it was the active one the turn passes
  cleanly to the next) and added between rounds (a summon joins the next round). Godot ships no
  turn/initiative system — games hand-roll it every time -> beyond-Godot gameplay utility. Verified:
  descending-initiative order with a round wrap that bumps the round counter; the lower-id tie-break;
  removing a later combatant skips it; removing the active combatant shifts the turn to the next; removing
  an already-acted combatant leaves the cursor on the same live one; mid-round adds and initiative changes
  apply only from the next round; a duplicate id updates initiative; and empty / removed-to-empty queues
  report -1 safely),
  **faction reputation system** (M488, `game::Reputation` + `game::Standing` — the standing meter behind
  "the guards now attack you on sight" and "the merchants give you a discount." Each faction carries a
  reputation value (clamped to a configurable range, default -100..100) that deeds nudge up or down via
  `modify` / `set`; that value maps through configurable thresholds to a Standing tier — Hostile /
  Unfriendly / Neutral / Friendly / Allied — which the game reads to decide who fights, trades, or opens
  doors. Factions auto-register on first use and an unknown faction reads as neutral. Godot ships no
  reputation/faction system — games hand-roll it every time -> beyond-Godot gameplay utility pairing with
  the quest and dialogue systems. Verified: registration and unknown-faction-is-neutral; every tier
  boundary on the default scale (-50 and -15 inclusive downward, 15 and 50 inclusive upward, Neutral
  between); `modify` accumulating and clamping to [min,max] with auto-registration; `set` clamping and
  `addFaction` resetting a duplicate; a custom 0..1000 range with custom thresholds mapping all five tiers
  and clamping out-of-range; independent factions and clear),
  **achievement system** (M489, `game::Achievements` — the unlock tracker behind "Achievement Unlocked!"
  toasts and a save file's completion percentage. Each achievement has a target count (a one-shot trophy
  uses target 1; a grind like "defeat 100 enemies" uses target 100); `progress(id, amount)` advances toward
  it and returns true only on the call that crosses the target — so a popup fires exactly once — while
  `unlock` fills it outright, `fraction`/`progressOf`/`isUnlocked` drive the list, and `completion` gives
  the overall unlocked/total ratio. Progress clamps at the target, unlocking is one-way until `reset`, and
  unknown ids read as locked-and-empty. Godot ships no achievement system (games call Steam/console SDKs or
  hand-roll one) -> beyond-Godot gameplay utility. Verified: a one-shot trophy unlocks on first progress
  and never re-fires; a counter reports fractions, unlocks exactly when the target is met, and clamps
  further progress; an overshoot crosses in one call and clamps; `unlock` fires once; non-positive amounts
  and unknown ids are safe no-ops; completion and `unlockedIds` track across several achievements; `reset`
  and a duplicate add re-lock; and a zero/negative target clamps to 1),
  **enemy wave spawner** (M490, `game::WaveSpawner` + `game::Wave` — the wave director behind
  tower-defense, survival, and horde modes. Waves run in sequence: each waits a start delay, then releases
  its enemies one at a time on a fixed interval, and the NEXT wave begins only once the current wave is
  fully spawned AND every enemy is dead (the game reports kills). `update(dt)` returns the enemy-type ids to
  spawn this tick (handling a large dt that spans several intervals in one call), `reportKilled` feeds the
  clear condition, and the spawner tracks the current wave, alive count, and all-waves-finished. Purely
  time- and event-driven, no rendering. Godot ships no wave/spawner system — games hand-roll it every time
  -> beyond-Godot gameplay utility. Verified: a two-wave sequence spawns wave 0's three enemies on interval,
  holds at "clearing" until all three are reported killed, then honors wave 1's start delay before spawning
  and only finishes after the last kill; the first enemy comes out on the first positive update; a large dt
  releases a whole wave in one call; start delay gates the first spawn; an empty (count-0) wave is skipped;
  a no-waves spawner finishes on start; and reset / clear / pre-start-kill / non-positive-dt are safe),
  **platformer jump-assist (coyote time + jump buffering)** (M491, `game::JumpAssist` — the two forgiving-
  input timers that separate a stiff platformer from a great-feeling one: COYOTE TIME lets a jump land for
  a moment after the character walks off a ledge, and JUMP BUFFERING makes a jump pressed just before
  landing fire the instant the ground is touched. Feed `update(dt, grounded)` each frame and `pressJump` on
  the button; `tryJump` returns true (consuming the buffered press) exactly when a jump should start — a
  live press AND grounded-or-within-coyote. Both windows are configurable. Godot leaves this feel tuning
  entirely to the game -> beyond-Godot gameplay utility. Verified: a grounded press fires once and is
  consumed; a jump within the coyote window after leaving the ground succeeds while one past it fails; a
  press while airborne is held and fires on landing within the buffer window but is dropped if the buffer
  expires first; `pressJump` refreshes the buffer; `reset` clears; and a zero coyote window forbids jumping
  the instant the character leaves the ground),
  **combo / score-chain meter** (M492, `game::ComboMeter` + `game::ComboTier` — the score-chain tracker
  behind arcade multipliers, fighting-game combo counters, and rhythm-game streaks. Each `hit` bumps the
  count and refreshes a countdown; if the countdown runs out (or the game calls `breakCombo` on a miss) the
  chain resets to zero. The current count maps through configurable tiers to a score MULTIPLIER, `scoreFor`
  applies it to a base point value, and the best combo reached is remembered for an end-of-run stat. Godot
  ships no combo/score-chain system -> beyond-Godot gameplay utility. Verified: hits grow the combo and
  refresh the timer; the default tiers step 1x / 1.5x@5 / 2x@10 / 3x@25 / 4x@50; a timeout breaks the combo
  while preserving the max-combo record; `breakCombo` resets immediately; the max tracks the peak across
  several chains; `scoreFor` scales and rounds (101 at 2x -> 202); custom tiers are sorted from unsorted
  input; an empty tier set falls back to a flat 1x; `reset` keeps the record while `resetAll` clears it;
  and non-positive hits / dt are safe no-ops),
  **day/night cycle** (M493, `game::DayNightCycle` + `game::DayPhase` — a looping time-of-day clock for
  open-world lighting, shop hours, and spawn schedules. One in-game day spans a configurable number of real
  seconds; `update(dt)` advances the clock, wraps it at midnight, and ticks a day counter (a large dt spans
  multiple days). It reports the normalized time [0,1), the in-game hour [0,24), a DayPhase (Night / Dawn /
  Day / Dusk) from configurable thresholds, and a sun elevation in [-1,1] (`-cos(2*pi*t)`: -1 at midnight,
  0 at sunrise/sunset, +1 at noon) the renderer can feed straight into a directional light. Godot ships no
  day/night system -> beyond-Godot gameplay utility. Verified: midnight and noon report the right hour /
  sun elevation / phase; the sun curve hits -1/0/+1/0 at the four quarters; every phase boundary classifies
  correctly (0.2 dawn, 0.3 day, 0.7 dusk, 0.8 night); wrapping increments the day counter for both a single
  overflow and a multi-day jump; `setHour` wraps a 30h input to 6h; custom thresholds reclassify; and
  non-positive dt / a zero day-length (clamped to 1) are safe),
  **STL (`.stl`) mesh import** (M509, `render::parseStl` / `loadStl` — closes another import-format gap versus
  Godot's asset pipeline. STL is the universal 3D-printing / CAD interchange format (every slicer, SolidWorks,
  Blender export): a flat triangle soup where each triangle carries a face normal and three corner positions,
  with no shared vertices, UVs, or materials. The importer reads BOTH encodings — ASCII (`solid` / `facet
  normal` / `vertex` keywords) and binary (80-byte header + uint32 triangle count + 50-byte records) — and
  auto-detects which a buffer is via the exact `84 + 50·count == size` identity (more robust than sniffing a
  leading "solid", which binary headers can also carry). It emits a `shapes::MeshData` — three vertices per
  triangle, sequential indices — ready for `Renderer::createMesh`. Because STL normals are frequently absent
  or wrong, a zero-length stored normal (or the `recomputeNormals` option) derives the geometric normal from
  the winding. Pure CPU byte/string work, unit-tested headlessly; `loadStl` wraps it for files. Honest scope:
  triangle geometry + per-face normals only — STL stores no texcoords or standard color, so UVs are zero and
  every vertex takes the fallback tint. Verified against a *reference* struct-packed binary buffer: a two-
  triangle binary quad decodes to six unshared vertices with sequential indices, exact corner positions, +Y
  normals, and the tint option overriding the default white; an ASCII triangle reads its declared +Z normal
  and positions; a zero stored normal is recomputed from the winding to +Z; `recomputeNormals` overrides even
  a (wrong) stored normal; and non-STL / empty / null input is rejected),
  **Adam7-interlaced PNG decode** (M508, extends `render::decodePng` — closes the "no Adam7 yet" follow-up
  the M500 PNG decoder honestly flagged. Interlaced PNGs (common for progressive web loading) don't store
  pixels row-by-row; they store SEVEN successively-finer passes, each a sparse sub-grid of the image with its
  own filtered scanlines. The decoder now inflates the IDAT once, then walks the seven passes with the
  standard Adam7 {startX,startY,stepX,stepY} table, unfilters each pass as its own little image (reusing the
  same five-filter reconstruction as the progressive path), and scatters every recovered sample to its true
  (x,y). A shared `writeSample` lambda expands grayscale / RGB / RGBA / gray+alpha / palette samples so both
  paths agree bit-for-bit. Pure CPU, verified against a *reference* zlib+Adam7 encoding. Verified: an
  8×8 interlaced RGBA image whose pixel (x,y) = (x·32, y·32, (x+y)·16, 255) decodes with all 64 pixels exact —
  which forces correct handling of every one of the seven passes — while a plain non-interlaced RGBA image
  still round-trips unchanged),
  **reflection probe influence + box projection** (M507, `render::ReflectionProbe` — the CPU math behind
  Godot's ReflectionProbe. A probe captures the surroundings into a cubemap inside an axis-aligned box;
  reflective surfaces in that box sample it. Two pieces are pure math: `influenceWeight` (how strongly a
  probe affects a point — full inside, fading to zero across a blend margin near the faces, so overlapping
  probes cross-fade) and `boxProjectDirection` (the parallax correction that makes a box-captured cubemap look
  right off flat walls — it intersects the reflection ray with the box and re-aims the sample from the probe
  center to that hit point). `probeSample` composes it with the M506 cubemap mapping to give the face/UV a
  surface reads. Fully unit-tested; the cubemap *capture* is a GPU pass and blending probe *colors* is the
  shader's job. Honest scope: axis-aligned box probes, linear face blend, box-projection parallax (no capture,
  roughness prefiltering, or color compositing). Verified: influence is 1 deep inside, exactly 0.5 half a
  blend-distance from a face, and 0 at/outside the surface; blendDistance 0 gives a hard inside/outside
  cutoff; box projection from the center returns the ray direction unchanged, while an off-center viewer's ray
  is correctly re-aimed from the center to the box exit point; and probeSample resolves to the expected cube
  face/UV),
  **cubemap direction mapping** (M506, `render::directionToCube` / `cubeToDirection` — the sampling math
  shared by reflection probes, skyboxes, and image-based lighting. A cubemap stores a 360° environment across
  six square faces; `directionToCube` converts a 3D direction into "which face + where on it (u,v)" and
  `cubeToDirection` inverts it, using the standard OpenGL/Vulkan cube mapping (major-axis selection with the
  conventional per-face s/t axes) so a Maz cubemap matches what artists author elsewhere. Pure math,
  unit-tested headlessly; a GPU samples the actual texels and reflection-probe capture is a separate pass.
  Honest scope: the direction<->face/uv convention only (no texture allocation/sampling, seamless edge
  filtering, or roughness prefiltering). Verified: the six axis directions land centered on their expected
  faces; major-axis selection picks the dominant component by magnitude (a -Z-dominant vector maps to NegZ);
  each face center inverts back to its axis; a spread of directions round-trips direction->face/uv->direction
  to the normalized input with UVs staying in [0,1]; and every face's UV corners yield unit-length directions),
  **3D navigation mesh pathfinding** (M505, `game::NavMesh3D` — the query side of Godot's
  NavigationServer3D / NavigationRegion3D. The walkable world is convex 3D polygons (floors, ramps,
  platforms) that share edges, and `findPath` returns a smoothed list of 3D waypoints from a start to a goal.
  Because a walkable surface is effectively 2D, it projects every polygon to the ground plane and reuses the
  tested 2D `NavMesh` (M87) for the corridor A* + funnel string-pulling — no duplicated pathfinding — then
  lifts each waypoint's height back onto the polygon it lands on (`sampleHeight`), so a path correctly climbs
  ramps and steps. Deterministic and GPU-free, unit-tested headlessly. Honest scope: pathfinding + height
  reconstruction over supplied walkable polygons (no Recast-style baking from raw geometry, overlapping
  multi-level surfaces at one XZ, or dynamic obstacle avoidance — follow-ups). Verified: a floor+ramp mesh
  routes across the shared edge and snaps the goal onto the ramp at the exact interpolated height (1.5 at
  x=7 of a 0→2 ramp); `sampleHeight` returns the plane height inside a polygon and the fallback outside; an
  L-shaped mesh of three edge-matched quads produces a path that bends around the inner corner (strictly
  longer than the straight line); and disconnected islands return no path),
  **analytic volumetric fog** (M504, `render::fogOpticalDepth` / `fogFactor` / `applyFog` — the CPU evaluation
  behind Godot's height/volumetric fog: "how much fog is between the camera and this point, and what color
  does it leave the pixel?" It integrates the fog density along a view segment via the Beer-Lambert law, with
  optional exponential height falloff (thicker low, thinner high, like real mist) solved in closed form,
  converts the optical depth to a blend factor `1 - exp(-optical)`, and mixes the scene color toward the fog
  color. Godot's volumetric fog raymarches a froxel volume on the GPU; this is the exact analytic form a
  renderer can use directly for simple exponential fog or as a reference. Pure math, fully unit-tested.
  Honest scope: homogeneous or exponential-height density with a single fog color (no GPU froxel scattering,
  per-light in-scatter, or noise/wind). Verified: homogeneous density gives optical depth density×distance and
  factor 1-e^-1 over the classic case; zero density and zero-length segments give no fog; the height-falloff
  closed form matches the hand-integrated value for a vertical ray and reduces to local-density×length for a
  horizontal one; higher rays are strictly less foggy than lower ones; and applyFog reproduces the exact
  color lerp, approaching the fog color at long range and leaving the scene untouched at zero distance),
  **decal projection** (M503, `render::projectDecal` — the math behind Godot's Decal node, which stamps a
  texture onto whatever surface lies inside an oriented box (bullet holes, blood, posters, tire tracks).
  Given a decal box (center + orthonormal right/up/forward frame + half extents) and a world-space surface
  point with its normal, it returns the texture UV to sample plus a blend alpha — or nothing when the point
  falls outside the box or the surface faces away from the projector (a normal-fade cutoff matching Godot's
  `normal_fade`). The footprint is the right×forward plane, projection depth runs along up. Pure math, so it
  unit-tests headlessly; the texture blend itself is the GPU's job. Honest scope: axis-aligned-in-local-space
  projection + linear normal fade (no GPU blend, per-decal albedo/emission mix, or depth-fade curves yet).
  Verified: a centered hit yields UV (0.5,0.5) with alpha 1; offsets map to the right UVs with the far edge
  inclusive; points beyond any of the three half-extents are rejected; a back-facing normal is rejected while
  a 60°-tilted one yields alpha 0.5 (= N·up); the normalCutoff gate rejects insufficiently-aligned surfaces;
  and a rotated decal frame maps world points correctly through its axes),
  **CPU lightmap baker** (M502, `render::bakeLightmap` — the offline "burn the lighting into a texture" step
  behind Godot's LightmapGI, letting static geometry look lit without paying for lights at runtime. For each
  surfel (a world-space point + normal — the unwrapped texel centers in a full pipeline) it sums each light's
  direct contribution (N·L, with linear range falloff for point lights) and traces a shadow ray against the
  occluder triangles via the header-only `math::segmentIntersectsTriangle`, so geometry casts hard shadows
  into the map. Pure CPU, so the *bake* is fully unit-testable here even though *sampling* the map at draw
  time needs a GPU — a first §5 (high-end rendering) item from `GODOT_GAPS_ROADMAP.md`. Honest scope: direct
  light + hard shadows only (no bounce/indirect GI, area-light softness, or UV-atlas unwrap yet). Verified:
  a surfel facing a directional light gets N·L=1, a 60°-tilted normal gets 0.5, a back-facing normal is unlit
  (ambient still applies); an occluder triangle shadows both directional and point lights (and castShadows=off
  ignores it); point-light linear range falloff gives 0.5 at half range and 0 beyond range; colored light and
  multiple surfels resolve independently; and empty input yields an empty result),
  **font fallback chain** (M501, `ui::FontFallback` — the "which font can draw this character?" resolver
  behind mixed-script text. Godot lets a Font carry an ordered list of fallback fonts and picks, per glyph,
  the first that has the character; Maz's atlas font covers ASCII, so anything beyond it needs this. It holds
  an ordered set of fonts (by int id), each with the Unicode ranges it covers, and answers `fontFor(codepoint)`
  — the first covering font in priority order, or a configured default — plus `runs(text)`, which splits a
  UTF-32 string into contiguous runs that resolve to the same font (the unit a shaper/renderer draws in one
  pass). Deterministic, std-only, GPU-free, so it unit-tests headlessly; real per-font coverage wires into it
  later. Verified: Latin / Cyrillic / emoji fonts resolve their own codepoints and report kNone for an
  uncovered snowman; a default font catches uncovered codepoints; on overlapping coverage the earlier
  (higher-priority) font wins; `runs` splits "A Б 😀 B C" into four runs merging the trailing Latin; uncovered
  spans carry kNone and an empty string yields no runs; and coverCodepoint / removeFont / clear behave),
  **PNG (`.png`) decode** (M500, `render::decodePng` / `loadPng` — closes the single most important image
  import gap versus Godot, which imports PNG everywhere. Built on the M499 inflate: it walks the PNG chunk
  stream (IHDR / PLTE / tRNS / IDAT / IEND), inflates the concatenated IDAT data, reverses all five
  per-scanline filters (None / Sub / Up / Average / Paeth), and expands grayscale, RGB, RGBA, grayscale+alpha,
  and 8-bit palette (with optional tRNS alpha) samples into an RGBA8 `Image` ready for
  `Renderer::createTexture`. Pure CPU, and verified against PNGs produced by a *reference* encoder. Honest
  scope: 8-bits-per-channel, both progressive and Adam7-interlaced (M508) — no 1/2/4/16-bit depths yet. Verified:
  a reference-encoded RGBA image decodes with exact colors and alpha; an RGB image whose five rows each use a
  different filter (None/Sub/Up/Average/Paeth) reconstructs every row correctly — exercising all filter paths;
  grayscale and 8-bit palette (index → PLTE color) images decode to the right RGBA; and non-PNG / empty input
  yields an empty Image),
  **DEFLATE / zlib inflate** (M499, `io::inflateRaw` / `io::zlibInflate` — the header-only, dependency-free
  decompressor that was the missing building block under PNG import, gzip/zlib assets, and KTX2 ZLIB
  supercompression. Godot leans on zlib for all of these; Maz had no inflate at all. `inflateRaw` expands a
  raw RFC 1951 DEFLATE stream (stored, fixed-Huffman, and dynamic-Huffman blocks with LZ77 back-references,
  via the canonical bit-at-a-time "puff" Huffman walk); `zlibInflate` validates and strips the RFC 1950
  2-byte header (and optional preset dictionary) first. Pure CPU, and — importantly for trust — verified
  against golden streams produced by the *reference* zlib, not just self-consistency. Honest scope: it
  decompresses only (no compression) and does not verify the trailing Adler-32. Verified: three real
  zlib-compressed payloads (a repetitive string exercising back-references, 64 varied bytes, and a 300-byte
  run) round-trip exactly through both the raw and zlib-wrapped paths; a hand-framed stored (uncompressed)
  block decodes; and a bad zlib header plus a reserved/truncated block are rejected. This unblocks PNG,
  next on `GODOT_GAPS_ROADMAP.md`),
  **PLY (Stanford `.ply`) mesh import** (M498, `render::parsePly` / `loadPly` — closes another Godot import
  gap: PLY is the standard output of 3D scanners and tools like MeshLab and CloudCompare. It reads both the
  ASCII and the binary (little- and big-endian) encodings, parsing the header's element/property
  declarations, then loading the vertex list (mapping x/y/z, nx/ny/nz, red/green/blue with 0–255 colors
  normalized, and s/t or u/v texcoords) and the face list into `shapes::MeshData`, fan-triangulating faces
  with more than three corners. A sequential reader pulls typed scalars from either an ASCII token stream or
  a binary byte cursor, so both paths share the property logic. Pure CPU, fully unit-tested; another
  gap-closing milestone from `GODOT_GAPS_ROADMAP.md`. Honest scope: the common `vertex`+`face` elements with
  scalar properties and one face index list (no custom elements, edge lists, or material blocks). Verified:
  an ASCII colored triangle loads the right positions and normalized RGB; an ASCII quad fan-triangulates to
  two triangles with the expected winding and default white tint; a binary little-endian triangle
  round-trips positions through the byte cursor; the flipV option inverts the texcoord; and non-PLY /
  header-truncated input fails gracefully),
  **COLLADA `.dae` mesh import** (M497, `render::parseCollada` / `loadCollada` — closes a real Godot import
  gap: Godot imports Collada out of the box, while Maz previously read only OBJ and glTF. COLLADA is an XML
  interchange format many DCC tools (Blender, Maya, SketchUp) still export, so supporting it widens what
  artists can bring in. Built on the M174 `io::XmlParser` pull parser, it gathers every `<source>` float
  array, the `<vertices>` POSITION mapping, and the `<triangles>` / `<polylist>` index streams with their
  per-semantic input offsets, then de-interleaves them into position/normal/uv `MeshVertex` data ready for
  `Renderer::createMesh`; polygons with more than three corners are fan-triangulated. Pure CPU string work,
  so it is fully unit-tested headlessly — this is a first gap-closing milestone from `GODOT_GAPS_ROADMAP.md`.
  Honest scope: it reads the common exported-mesh case (first geometry, triangle/polylist primitives with
  VERTEX/NORMAL/TEXCOORD) and does not yet apply node transforms, skinning, materials, or trifan/tristrip
  primitives. Verified: a two-triangle quad with distinct position/normal/texcoord sources at separate input
  offsets de-interleaves to the right positions/normals/UVs; the default flipV inverts V while flipV=false
  leaves it; the vertex tint applies; a one-polygon polylist quad fan-triangulates to two triangles with the
  expected index winding; and malformed / geometry-less documents fail gracefully to an empty mesh),
  **skill / talent tree** (M496, `game::SkillTree` — the point-buy progression graph behind "spend a
  talent point to unlock this node." Each node has a point cost and a max rank (buy once, or several times
  for a stacking bonus), and can be gated behind prerequisite nodes that must already be unlocked (multiple
  prerequisites are ANDed). The player earns points (e.g. from the M479 Leveling track) via `grantPoints`,
  and `unlock` spends them when `canUnlock` confirms the node exists, isn't maxed, is affordable, and all
  prerequisites are met; `respec` refunds every spent point and zeroes all ranks so the build can be
  re-planned while the tree layout is kept. It is deliberately the unlock *graph* only — what each node
  grants (a Stat bonus via M476, an ability, etc.) is left to the game, keeping it distinct from the stat
  math and the XP curve. Godot ships no skill-tree system — it's hand-rolled every RPG — so this is a
  beyond-Godot gameplay utility. Verified: nodes register with clamped cost/maxRank and unknown-id queries
  are safe; unlock is blocked by insufficient points and by unmet prerequisites, then succeeds once both are
  satisfied and spends the cost; multiple prerequisites are ANDed; a multi-rank node buys up to its cap then
  refuses; chained prerequisites (A→B→C) gate correctly; respec refunds and resets ranks while keeping the
  layout; degenerate cost/maxRank clamp and negative point grants are ignored; and clear wipes everything),
  **aggro / threat table** (M495, `game::AggroTable` — the bookkeeping behind "which target does this
  enemy attack?" Every action a would-be target takes against the owner (`addThreat` from damage, healing an
  ally, a taunt) accumulates threat keyed by an int source id, and the enemy attacks whoever holds the most.
  Two touches make it feel right rather than naive: sticky aggro (a challenger must exceed the current
  target's threat by a configurable switch threshold — default 110% — before it steals aggro, so the enemy
  doesn't flip-flop target every hit) and `taunt`, which forces a target immediately and tops its threat up
  to the current highest so it holds. Optional per-second `decay` bleeds threat back toward zero for
  out-of-combat forgetting, and `removeSource` retargets when the current target dies or flees. Godot ships
  no aggro/threat system — enemy targeting is hand-rolled every game — so this is a beyond-Godot gameplay
  utility. Verified: an empty table reports no target; the first threat establishes the target; threat floors
  at 0 on a large negative; a 105-vs-100 challenger is the raw leader yet aggro sticks to the incumbent until
  it passes 110% (115); a 1.0 switch threshold flips immediately and sub-1.0 clamps to 1.0; taunt forces the
  target and tops up its threat; equal threat breaks the tie to the lower id; removeSource retargets to the
  next and empties cleanly; uniform decay preserves ordering, floors at 0, and is a no-op for non-positive dt
  or a zero rate; and setThreat clamps while clear resets),
  **health component** (M494, `game::Health` — the hit-point pool behind health bars, death, and
  post-hit invulnerability frames. It holds current / max HP with `takeDamage` and `heal` both clamped
  (HP never drops below 0 or climbs above max) and each returning the amount actually applied, reports
  the `fraction` for a health bar plus `isDead` / `isFull`, and pairs directly with the M486 damage
  resolver — feed its result straight into `takeDamage`. A connecting hit can start a window of
  invulnerability (the classic post-damage i-frames) during which further damage is ignored; you can also
  grant a window manually (it keeps the longer of the current / new window). `update(dt)` ticks that
  window down and applies optional passive regeneration. `setMax` can refill to full or clamp down,
  `setCurrent` clamps to [0, max], and `kill` / `revive` handle the down-and-back-up cases. Godot ships
  no health system — games hand-roll it every time — so this is a beyond-Godot gameplay utility. Verified:
  a fresh pool starts full; damage clamps to remaining HP and kills at 0; a dead pool ignores further
  damage and healing; healing caps at max; on-hit i-frames block a second hit and expire after their
  window; a manual grant keeps the longer window; passive regen heals over time and stops at full; `setMax`
  with and without heal-to-full and `setCurrent` clamp correctly; `kill` / `revive` (full and partial)
  work; and a degenerate max (< 1, clamped to 1) plus non-positive dt are safe),
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
  (M371 adds isValidHexNumber — Godot String.is_valid_hex_number: hex integer with an optional +/- sign
  and optional "0x" prefix, replicating Godot's algorithm and its edge cases (empty and bare "0x" ->
  false), verified with/without prefix and against non-hex input)
  (M344 adds trimPrefix / trimSuffix — Godot String.trim_prefix / trim_suffix: strip a prefix or
  suffix ONLY when it is actually present (returns the string unchanged otherwise), verified on
  res://user:// scheme + extension stripping and no-op cases)
  (M345 adds indent / dedent — Godot String.indent (prefix every non-empty line, leaving truly empty
  lines alone) and String.dedent (strip ALL leading spaces/tabs per line, Godot's per-line rule, not
  the common-minimum), verified incl. an indent→dedent round-trip)
  (M350 adds getSlice / getSliceCount — Godot String.get_slice / get_slice_count: index-based access
  to the Nth piece when split on a (possibly multi-char) delimiter, without building the whole array;
  empty pieces preserved, out-of-range/negative/empty returns "" / 0, verified; M368 adds getSlicec —
  Godot String.get_slicec: the single-character-delimiter form of get_slice, verified)
  (M351 adds countN / findN — Godot String.countn / findn: case-insensitive (ASCII) occurrence count
  and first-match search from an offset (npos if none), verified against mixed-case inputs;
  M354 adds rfind / rfindN — Godot String.rfind / rfindn: reverse search returning the LAST match
  at/before a position (npos default = whole string, matching Godot's -1 "from the end"), verified
  against strings with two occurrences and case-insensitive inputs;
  M365 adds hashString — Godot String.hash: 32-bit djb2 (seed 5381, h*33+c), the hash Godot's HashMap
  uses; empty string → 5381, verified against exact djb2 values ("abc"=193485963). NOTE: hashes bytes,
  so equals Godot exactly for ASCII; multi-byte UTF-8 differs (Godot hashes code points) — documented.
  M355 adds validateNodeName — Godot String.validate_node_name: removes the six characters Godot
  forbids in SceneTree node names ('.', ':', '@', '/', '"', '%'), all other chars incl. spaces kept,
  verified against mixed/clean/all-forbidden inputs; M360 adds insert / erase — Godot String.insert /
  String.erase: insert a substring at a byte index (negative pos unchanged, past-end clamps to append)
  and remove N chars at a byte index (pos/count clamped, out-of-range removes nothing), round-trip
  verified; M369 adds validateFilename — Godot String.validate_filename: trims edges then replaces the
  filesystem-invalid characters (: / \ ? * " | % < >) with '_', dots/spaces preserved, verified; M356 adds posmodv (vec2/vec3) — Godot
  Vector2/Vector3.posmodv: per-component positive modulo with a per-component modulus vector,
  complementing the scalar-modulus posmod, verified per-component incl. negative modulus sign;
  M357 adds Vector2i/Vector3i maxAxisIndex / minAxisIndex — Godot Vector2i/Vector3i.max_axis_index /
  min_axis_index (axis index of the largest/smallest component), with Godot's exact tie-breaking
  (max: earliest axis wins; min: latest axis wins), verified incl. all-equal components; M358 adds
  intToBase (StringUtils) — Godot String.num_int64: integer→text in an arbitrary base 2..36 (0-9 then
  a-z or A-Z), negatives prefixed '-', bases out of range clamp to 10, INT64_MIN handled via unsigned
  magnitude, verified across bases/signs; M372 adds uintToBase — Godot String.num_uint64: the unsigned
  companion (never negated, so the full 64-bit range prints, e.g. UINT64_MAX base16 = ffffffffffffffff),
  verified across bases incl. the max value; M374 adds casecmpTo / nocasecmpTo — Godot String.casecmp_to /
  nocasecmp_to (three-way -1/0/1 lexicographic compare, shorter-prefix sorts first; nocase folds ASCII case;
  cross-checked against std::string ordering), toward String sorting; M375 adds isValidIpAddress — a faithful
  port of Godot String.is_valid_ip_address (four decimal octets 0..255 for IPv4; a permissive split-on-':'
  IPv6 check accepting hex groups 0..0xffff, '::' empties, and a trailing embedded IPv4 — matching Godot's
  behaviour exactly rather than being a strict RFC validator, documented as such); M376 adds chr — Godot
  String.chr: encodes a Unicode code point to its 1–4 byte UTF-8 sequence (verified against exact bytes for
  ASCII 'A', 'é' U+00E9, '☺' U+263A, '😀' U+1F600, and the U+10FFFF edge; > U+10FFFF yields empty);
  M361 adds Vector4 isFinite / isZeroApprox — Godot
  Vector4.is_finite / is_zero_approx (all components finite / within epsilon of zero), completing the
  is_finite family across vec2/vec3/vec4/quat, verified against inf/NaN and tiny/nonzero components;
  M362 adds float vec2/vec3 maxAxisIndex / minAxisIndex — Godot Vector2/Vector3.max_axis_index /
  min_axis_index (float companions to the M357 integer versions), using Godot's exact nested-ternary
  form so tie-breaking matches its float type (all-equal → max X, min Z), verified; M363 adds
  Vector4 maxAxisIndex / minAxisIndex — Godot Vector4.max_axis_index / min_axis_index (max scans
  strict '>' earliest-wins, min scans '<=' latest-wins), completing the axis-index family across
  2i/3i/vec2/vec3/Vector4, verified incl. all-equal; M366 adds Vector2i/Vector3i scalar-bound clampi /
  snappedi / mini / maxi — Godot Vector2i/Vector3i.clampi/snappedi/mini/maxi (one int applied to every
  component, companions to the vector-arg clamp/snapped/min/max), verified; M367 adds Vector4 scalar-
  float clampf / snappedf / minf / maxf — Godot Vector4.clampf/snappedf/minf/maxf (one float per
  component, the float analogue of M366), verified incl. snappedf step 0; M359 adds Rect2.getSupport — Godot Rect2.get_support: the
  rectangle corner farthest along a direction (per axis max edge when dir>0 else min; dir==0 picks min,
  matching Godot's strict >0), the GJK/SAT broadphase primitive, verified all quadrants + zero-axis),
  plus **path helpers** M285 — getExtension/getBasename/getFile/getBaseDir/pathJoin/simplifyPath
  matching Godot String's get_extension/get_basename/get_file/get_base_dir/path_join/simplify_path
  (M364 adds isAbsolutePath / isRelativePath — Godot String.is_absolute_path / is_relative_path:
  absolute = leading '/'/'\\' or a ":/"/":\\" drive/scheme root so "C:/x" and "res://x" both count,
  empty is relative; verified across posix/windows/scheme/relative/empty),
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
  picking / line-of-sight / ballistics; M383 adds segmentIntersectsCylinder — Godot's
  Geometry3D.segment_intersects_cylinder (finite Y-axis cylinder centred at origin, height + radius):
  clips the segment against the radial slab and the two Y caps and returns the entry crossing
  (forward exit when the segment starts inside, mirroring segmentIntersectsSphere), verified with
  side-hit, cap-hit, offset-side, starts-inside, above-cap/outside-radius/too-short cases; M322 adds buildBoxPlanes and segmentIntersectsConvex —
  Godot's Geometry3D.build_box_planes / segment_intersects_convex: represent a convex volume as its
  outward-facing half-space planes and find where a segment first enters it (frustum / convex-region
  clipping and picking). Verified with straight-through, diagonal-corner, off-centre, miss,
  starts-inside (no entry, matching Godot) and stops-short cases; M396 adds **buildCylinderPlanes** —
  Godot's Geometry3D.build_cylinder_planes: the `sides` radial side planes (each at distance radius)
  plus the two axis caps at +/- height/2 of an axis-aligned cylinder (0=X/1=Y/2=Z, default Z),
  bounding the faceted prism with the same negative-side-is-inside convention. Verified by
  inside/outside membership (centre, near-radius, near-cap, beyond-radius, above/below caps), exact
  cap-plane distances, a segment entering through the top cap (via segmentIntersectsConvex), an
  X-aligned variant, and out-of-range-axis fallback to Z; M332 adds
  closestPointToSegmentUncapped — Godot's Geometry3D.get_closest_point_to_segment_uncapped
  (projection onto the infinite line, no clamping); M398 adds **closestPointOnTriangle** — the
  point on a triangle nearest an arbitrary 3D point via Ericson's Voronoi-region method (all seven
  regions: three vertices, three edges, interior face), the bedrock of sphere-vs-mesh collision,
  decal projection and snap-to-surface. Verified against exact cases (interior projection from
  above/below, each vertex/edge region, on-surface identity) and a tilted triangle where an
  interior projection lands on the centroid perpendicular to the face; M399 adds **barycentric** —
  the (u,v,w) coordinates of a point w.r.t. a 3D triangle (Ericson's area/Cramer method, projecting
  off-plane points), the standard tool for interpolating a per-vertex attribute (colour, UV, normal)
  at a ray hit or arbitrary surface point. Verified by vertex→unit-basis, centroid→(1/3,1/3,1/3),
  edge midpoints, sum-to-one + reconstruction, a negative weight outside, off-plane projection, an
  attribute-interpolation example, and degenerate-triangle safety; M400 adds **frustumIntersectsAabb**
  (render::frustumIntersectsAabb — the positive-vertex frustum-vs-AABB culling test on a
  render::FrustumPlanes, the AABB companion to Camera3D::isPointVisible/isSphereVisible that scene
  culling actually needs; extract the six planes once, reject boxes wholly outside any plane).
  Verified with an ahead/behind/off-to-side/beyond-far/near-clip spread, a huge enclosing box, an
  edge-straddling box, and degenerate-box agreement with the verified isPointVisible; M401 adds
  **buildCapsulePlanes** (math::buildCapsulePlanes — the capsule companion to buildBoxPlanes/
  buildCylinderPlanes, completing the plane-builder family: `sides` radial facets around the axis
  plus per-ring tangent planes for each hemispherical cap, so a capsule collision/culling volume
  can feed segmentIntersectsConvex like any other convex hull. Documented honestly as a
  *conservative* faceted containment — the plane set fully encloses the true capsule (every point
  inside the capsule is inside all planes) but the faceting means it is not byte-exact to Godot's
  capsule shape. Verified by 200+ interior sample points (all contained), pole/wall anchors,
  clearly-outside rejections, an X-aligned variant, and axis-out-of-range fallback to Z); M402 adds
  **computeConvexMeshPoints** (math::computeConvexMeshPoints — Godot's Geometry3D.compute_convex_mesh_points,
  the INVERSE of the plane-builder family: given a set of bounding planes it returns the corner
  vertices of that convex polytope, so a box/cylinder/capsule bound — or any half-space set — becomes
  the drawable hull corners you can feed to a hull builder, a debug renderer, or a bounds computation.
  Every corner is where three planes meet (via the existing Plane::intersect3) and survives only if it
  lies on or inside all the other planes; coincident corners are merged within eps. Verified with a box
  (6 planes -> exactly 8 corners at ±extent, off-centre translation, origin-not-a-corner), an
  inside-all-planes property check, redundant/duplicate-plane merging (still 8), the fewer-than-4-planes
  empty case, and a cylinder (2·sides corners, each on a cap and between the inscribed radius and the
  circumradius)); M403 adds **clipPolygon** (math::clipPolygon — Godot's Geometry3D.clip_polygon,
  single-plane Sutherland-Hodgman clipping of a 3D polygon ring against a plane, keeping the part on
  the negative side (inside the volume) and inserting the exact edge/plane intersection at each
  crossing. Same "inside = negative side" convention as the plane-builder family, so a face can be
  clipped plane-by-plane against a convex bound — the classic way to build the polygon faces that pair
  with computeConvexMeshPoints' corners. Faithful port of Godot's inside/outside/boundary location
  cache. Verified: a square clipped at x=1 -> exact trimmed rectangle, wholly-inside returns the ring
  unchanged, wholly-outside returns empty, a diagonal cut yields the correct triangle, empty input ->
  empty, and two successive clips trim to a smaller rectangle); M404 adds **buildConvexMeshFaces**
  (math::buildConvexMeshFaces — the planes->polygon-faces construction that closes the loop with M402
  computeConvexMeshPoints (planes->corners) and M403 clipPolygon (trim a face against a plane): for
  each plane it starts with a large outward-wound quad on that plane and clips it against every other
  plane, so whatever survives with ≥3 vertices is that plane's convex face. Yields a watertight,
  correctly-wound set of polygon faces ready for a debug renderer, a collision proxy, or triangulation
  — the standard "planes to mesh" idea Godot uses internally to turn its build_*_planes output into a
  MeshData; documented honestly as a composing utility over the verified M402/M403 primitives, not a
  distinct Godot public API. Verified: a box -> 6 quad faces whose vertices are exactly the 8 corners,
  each face wound so its Newell normal matches its plane and every vertex inside all planes; a
  cylinder -> `sides` quad side-faces + 2 cap `sides`-gons; and an unbounded set -> no faces); M405
  adds **triangulateConvexFaces / triangulateConvexPlanes** (math::ConvexMesh3 — the final step of the
  planes -> corners -> faces -> mesh pipeline: fan-triangulate the convex polygon faces from
  buildConvexMeshFaces into an indexed triangle mesh (shared vertex list + 3 indices per triangle,
  vertices merged within eps, winding preserved outward), and a one-call triangulateConvexPlanes that
  goes straight from bounding planes to a drawable/collidable convex mesh. Verified: a box -> 8
  vertices / 12 triangles / 36 indices with every index in range and every triangle wound outward
  (normal·centroid > 0), edge-sharing faces deduped to a shared vertex list, a quad -> 2 triangles, a
  cylinder -> 2·sides vertices / 4·sides−4 triangles, and degenerate/empty/unbounded inputs -> empty.
  Documented as a composing utility over the verified M402–M404 primitives) — plus Plane completeness
  has_point / get_center /
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
