# Maz Engine — API Reference

> Auto-generated from the engine headers by `tools/gen_api_docs.py`. Each module's summary is its header's own doc comment; the type and function lists are its public surface. This is a map — read the header for full signatures and semantics.

_651 headers across 20 subsystems._

## Contents

- [Core — foundation (time, jobs, events, RNG, resources, config)](#core)
- [Math — vectors, matrices, transforms, curves, geometry](#math)
- [Platform — window, input, filesystem, crash handling](#platform)
- [Render — Vulkan renderer, sprites, meshes, shapes, cameras](#render)
- [Scene — node tree, transforms, serialization](#scene)
- [Script — the maz::script language VM + engine binding](#script)
- [ECS — entity/component/system world](#ecs)
- [Game — physics, collision, AI, pathfinding, tilemaps](#game)
- [Anim — skeletons, clips, blending, tweening, curves](#anim)
- [Audio — mixer, DSP effects, spatialization, synthesis](#audio)
- [UI — controls, layout, theming, text](#ui)
- [FX — particles and force fields](#fx)
- [IO — JSON, config, serialization, resource packs](#io)
- [Input — action maps, analog helpers](#input)
- [Editor — scene model, gizmos, inspector](#editor)
- [`(root)`](#root)
- [`docs`](#docs)
- [`ext`](#ext)
- [`net`](#net)
- [`video`](#video)

<a name="core"></a>
## Core — foundation (time, jobs, events, RNG, resources, config)

### `AhoCorasick`
<sub>`engine/include/maz/core/AhoCorasick.hpp`</sub>

maz::core::AhoCorasick — find EVERY occurrence of MANY search strings inside a text in a SINGLE pass over that text (Aho & Corasick, 1975). The naive approach — loop each of k patterns and scan the whole n-char text — costs O(n*k) and rescans the same characters k times; Aho-Corasick builds one automaton from all the patterns up front and then sweeps the text once in O(n + total matches), no matter how many patterns there are. It is the standard engine behind a profanity/word filter, chat slash-command detection, dialogue keyword triggers ("the player mentioned the king AND the sword"), search highlighting, and content moderation dictionaries — anywhere you must watch a stream of text for a whole vocabulary at once. The construction is a trie of the patterns plus "failure" links: when a match breaks mid-way, the failure link jumps to the longest proper suffix that is still a live prefix of some pattern, so the scan never backs up. Header-only, std-only, deterministic (matches are returned in scan order — by end position, then by pattern index). Godot has no multi-pattern matcher; this is well beyond String.find.

**Types:** `AhoCorasick`

### `AliasTable`
<sub>`engine/include/maz/core/AliasTable.hpp`</sub>

maz::core::AliasTable — Vose's ALIAS METHOD for O(1) weighted random selection. Pcg32::weighted picks an index proportional to its weight in O(n) per draw, rebuilding the running total every time; that is fine for an occasional roll but wasteful for a table sampled thousands of times a frame (particle spawns, procedural scatter, big loot/encounter tables). AliasTable pays an O(n) build ONCE to precompute two small tables, after which every sample is a single random column + one coin flip — constant time regardless of table size. Deterministic: draws come from a caller-supplied Pcg32, so the same seed reproduces the same sequence (replays / lockstep). Negative weights count as 0. Godot's rand_weighted is the O(n) form; the build-once alias table is a beyond-Godot utility. Header-only.

**Types:** `AliasTable`

### `Assert`
<sub>`engine/include/maz/core/Assert.hpp`</sub>

### `AssetServer`
<sub>`engine/include/maz/core/AssetServer.hpp`</sub>

maz::core::AssetServer — an asynchronous, reference-counted asset loader, Maz's answer to Godot's ResourceLoader (load_threaded_request / load_threaded_get_status / load_threaded_get) plus the .import reimport pipeline, in one header.  The problem it solves: a game must not stall the frame decoding a texture or parsing a model. So loading is split in two, exactly like Godot: 1. request(path)  — enqueues a decode job on a background JobSystem thread and returns an AssetId immediately. The frame keeps running. 2. poll()         — called once per frame on the game thread; it harvests any jobs that finished on workers, moves their results into the cache, and fires onLoaded callbacks. This is where a real engine would do the GPU upload — on the main thread, off the worker — which is why finalization is a separate step. status(id) reports Queued / Loading / Loaded / Failed; progress() gives a loaded/total pair for a loading screen. Identical paths dedupe to one entry with a reference count, so requesting the same texture from a hundred places loads it once (the ResourceCache guarantee, now async).  Reimport / hot reload: each entry remembers the "stamp" of its source (a file mtime or content hash you supply). reimportChanged() re-decodes every entry whose stamp moved and bumps its version(), so a renderer can notice `version` changed and re-upload the new bytes — the runtime half of Godot's reimport. All CPU, thread-based, no GPU: templated on your decoded payload type T, so it unit-tests deterministically by driving request()->poll() to completion.

**Types:** `AssetId`, `AssetServer`

### `BitSet`
<sub>`engine/include/maz/core/BitSet.hpp`</sub>

maz::core::BitSet — a DYNAMIC bit set (resizable, word-packed). std::bitset is fixed-size and std::vector<bool> lacks the set-algebra and fast scanning games actually want; BitSet stores bits 64 to a word and gives the operations that matter: test/set/reset/flip, whole-set and/or/xor/not, popcount, any/none/all, and O(1)-per-hit iteration over just the set bits (findFirst/findNext). It is the natural backing for entity flag sets, per-frame "visited/dirty" marks, tile occupancy grids, and arbitrary-width collision layer masks — anywhere a plain int mask runs out of bits. Header-only, std-only (C++20 <bit> popcount/countr_zero). Godot exposes only fixed 32-bit masks.

**Types:** `BitSet`

### `BloomFilter`
<sub>`engine/include/maz/core/BloomFilter.hpp`</sub>

maz::core::BloomFilter — a space-efficient probabilistic set: it answers "have I definitely NOT seen this?" with certainty and "might I have seen this?" with a small, tunable false-positive chance, using a fraction of the memory a real set of the keys would cost. It never reports a false negative (anything added always tests present), so it is the ideal fast pre-filter: a "visited" marker for the millions of cells/chunks in a huge procedural world, a duplicate-suppressor for events or network packets, or a cheap gate in front of an expensive exact lookup ("skip the disk/db hit when the Bloom filter says it's certainly absent"). Uses k hash probes derived from a single std::hash by double-hashing (h1 + i*h2), so it works for any std::hash-able key. Build one directly by bit-count + probe-count, or via optimal() from an expected item count and target false-positive rate. Header-only, std-only. Godot has no Bloom filter.

**Types:** `BloomFilter`

### `CVars`
<sub>`engine/include/maz/core/CVars.hpp`</sub>

Config variables ("cvars"): a central registry of named, typed, self-describing tunables that any subsystem registers once and everyone can read or override — the engine's single source of truth for settings (render exposure, gameplay speed, UI scale, debug toggles). Each cvar carries a type, a default, a human-readable description, and — for numbers — an optional [lo, hi] clamp. Values can be set programmatically (typed setters clamp) or coerced from a string (for command-line flags and text configs), and the whole set can be iterated for a config UI or a settings file. This header is deliberately dependency-free (std only); JSON load/save lives in the io layer (io/Config.hpp) so core keeps zero dependencies.

**Types:** `CVarRegistry`

### `CellularNoise`
<sub>`engine/include/maz/core/CellularNoise.hpp`</sub>

maz::core Worley (cellular / "Voronoi") noise — the procedural-texture staple the engine's Perlin noise (core::Noise) doesn't cover. Space is divided into unit cells, each holding one deterministic feature point; sampling a position returns F1 (distance to the nearest feature point) and F2 (to the second nearest). F1 alone gives bubbly/organic cells (think water caustics, cracked mud, cell walls); F2−F1 traces the ridges *between* cells (stone veins, crackle, Voronoi edges). Unlike Voronoi.hpp (which builds an explicit Delaunay/Voronoi diagram from a point set), this is a cheap, seed-driven, continuously-sampleable NOISE FIELD with no allocation — a hash places each cell's point, so it is deterministic and unit-tests exactly.

**Types:** `CellularSample`

**Functions:**

- `inline uint32_t mix32(uint32_t x)`
- `inline uint32_t hashCell(int cx, int cy, uint32_t seed)`
- `inline uint32_t hashCell3(int cx, int cy, int cz, uint32_t seed)`
- `inline float unit01(uint32_t h)`
- `inline int floorInt(float v)`
- `inline CellularSample worley2D(float x, float y, uint32_t seed = 0)`
- `inline CellularSample worley3D(float x, float y, float z, uint32_t seed = 0)`

### `Checkpoints`
<sub>`engine/include/maz/core/Checkpoints.hpp`</sub>

maz::core::Checkpoints — game-state save slots plus a rolling rewind buffer. This is the "save the whole world and put it back" primitive: the game hands over an opaque byte blob of its serialized state (from io::Serialize, io::SceneSerializer, its script fields, whatever) and Checkpoints stores and returns it. Two complementary modes:  1. NAMED SLOTS — save("chapter2", bytes) / load("chapter2"). Classic manual + auto checkpoints and save files. serialize()/loadFile() round-trip ALL named slots to one versioned blob you write to disk, so a whole save file is one call. 2. REWIND RING — a fixed-capacity ring of recent snapshots keyed by frame. autosave(frame,bytes) pushes one and evicts the oldest past capacity; rewind(k) returns the k-th newest and rewindToFrame(f) the newest at-or-before frame f. That's the backbone of rewind mechanics (Braid / Prince of Persia), rollback netcode, and sandbox "undo" — something Godot has no built-in equivalent for, so this is engine-ahead, not just parity.  State is opaque bytes, so Checkpoints is serialization-scheme-agnostic and unit-tests without a game. Header-only, no GPU, no threads, no I/O (the caller owns the file). Little-endian on-disk.

**Types:** `Snapshot`, `Checkpoints`

### `Config`
<sub>`engine/include/maz/core/Config.hpp`</sub>

Startup configuration, populated from command-line arguments.

**Types:** `AppConfig`

### `Containers`
<sub>`engine/include/maz/core/Containers.hpp`</sub>

maz::core containers — the two data structures a game engine reaches for constantly but the STL doesn't ship: a small-buffer vector and a sparse set.  SmallVector<T, N> — a dynamic array that keeps its first N elements INLINE (no heap allocation) and only spills to the heap when it grows past N. Most engine lists are tiny and short-lived (a node's children, contacts on a body, hits from a query), so keeping them inline eliminates the malloc/free per list — the single biggest source of allocator churn. Same push_back / indexing / range-for API as std::vector.  SparseSet<T> — maps integer keys to values with O(1) insert, remove, and lookup AND a densely- packed value array you can iterate with no holes. Removal is a swap-with-last ("swap-erase"), so iteration stays cache-friendly. This is the backbone of an archetype-free ECS component store and any "set of entity ids with data" — exactly what Godot's servers keep internally.  Header-only, no GPU, no threads.

**Types:** `SmallVector`, `SparseSet`

### `CountMinSketch`
<sub>`engine/include/maz/core/CountMinSketch.hpp`</sub>

maz::core::CountMinSketch — estimate HOW MANY TIMES each item appeared in a stream, using a small fixed table instead of one counter per distinct key. Where HyperLogLog answers "how many DIFFERENT items?", a Count-Min sketch answers "how often did THIS item occur?" — approximately, in O(1) memory that does not grow with the number of distinct keys. It is the standard tool for finding "heavy hitters" cheaply: which item is being spammed in chat, which network source is flooding packets, which ability is used most, which asset is requested most — without a hash map that balloons to millions of entries. The structure is d rows of w counters; each item is hashed d different ways (one column per row) and those d counters are bumped; the estimate is the MINIMUM of the item's d counters. Because different keys can collide into the same counter, the estimate can only ever be TOO HIGH, never too low — and taking the min across rows makes large overestimates rare. Registers add elementwise, so per-shard sketches merge for free. Header-only, std-only, deterministic. Godot has no frequency sketch.

**Types:** `CountMinSketch`

### `CurlNoise`
<sub>`engine/include/maz/core/CurlNoise.hpp`</sub>

maz::core::CurlNoise — a divergence-free ("incompressible") 2D flow field derived from the engine's Perlin noise. The flow vector at a point is the PERPENDICULAR of the noise's gradient, so it always runs along the contour lines of the potential rather than up or down them. Because a perpendicular gradient has (mathematically) zero divergence, particles carried by this field swirl and fold without ever bunching together or thinning out — exactly the look of smoke, wind, magic, fluid, and flowing hair that plain random or radial forces can't give. Built on top of Noise (M85, reused not duplicated); distinct from FlowField (pathfinding toward a goal) and Worley (cellular texture). Deterministic from a seed. Godot has no curl-noise primitive. Header-only, std-only.

**Types:** `CurlNoise`

### `DamerauLevenshtein`
<sub>`engine/include/maz/core/DamerauLevenshtein.hpp`</sub>

maz::core Damerau–Levenshtein (optimal string alignment) distance — like the plain edit distance (core::levenshtein) but counts a SWAP of two adjacent characters as a SINGLE edit. That matters because transposition is the single most common human typo ("teh"→"the", "recieve"→"receive"): Levenshtein charges it as two edits (delete + insert), Damerau charges one, so ranking search results / command-palette matches / player-name lookups by this distance tolerates typos the way people actually make them. This is the restricted (OSA) variant — adjacent transpositions only, no substring re-edited twice — which is symmetric and always ≤ the Levenshtein distance. Godot exposes no edit-distance utility. Header-only, std-only, deterministic.

**Functions:**

- `inline std::size_t damerauLevenshtein(const std::string& a, const std::string& b)`

### `DateTime`
<sub>`engine/include/maz/core/DateTime.hpp`</sub>

maz::core — deterministic date/time utilities, Maz's answer to Godot's Time singleton. The whole point is DETERMINISM: nothing here reads the system clock. You pass in an epoch value (Unix seconds) or advance a GameClock by your fixed-step dt, and get back exact calendar fields — so an in-game clock, a day/night counter, a "day survived" tally, or a save-file timestamp all stay bit-reproducible across a replay and across machines (pair with core::Replay). Converting a real wall-clock reading into these fields is the caller's job (platform code), keeping the engine core clock-free.  The civil<->days conversion is the standard proleptic-Gregorian algorithm (Howard Hinnant's days_from_civil / civil_from_days), correct for any year, with no lookup tables. Header-only.

**Types:** `DateTime`, `GameClock`

**Functions:**

- `inline bool isLeapYear(int y)`
- `inline int daysInMonth(int y, int m)`
- `inline int64_t daysFromCivil(int64_t y, unsigned m, unsigned d)`
- `inline void civilFromDays(int64_t z, int& y, int& m, int& d)`
- `inline DateTime fromUnix(int64_t seconds)`
- `inline int64_t toUnix(const DateTime& dt)`
- `inline std::string formatIso(const DateTime& dt)`
- `inline std::string formatDate(const DateTime& dt)`
- `inline std::string formatTime(const DateTime& dt)`
- `inline std::string formatDateTime(const DateTime& dt, bool useSpace = true)`
- `inline std::string offsetString(int offsetMinutes)`
- `inline bool parseIso(const std::string& s, DateTime& out)`
- _…and 1 more_

### `Diff`
<sub>`engine/include/maz/core/Diff.hpp`</sub>

maz::core sequence diff / longest common subsequence — compare two sequences and produce the minimal edit script (keeps, deletions, insertions) that turns one into the other, built on the classic longest-common- subsequence dynamic program. This is the engine behind: showing what changed between two versions of a save/scene/config, computing a compact patch to send over the network or store in an undo stack, merging or reconciling lists, and text/line diffing in tools. Templated on the element type (chars, lines, ids, any equality-comparable T). Godot has no diff utility. Header-only, std-only, deterministic. O(n*m) time/space (fine for the moderate sequences a game/tool diffs).

**Types:** `DiffOp`, `DiffEntry`

**Functions:**

- `inline std::vector<int> lcsTable(const std::vector<T>& a, const std::vector<T>& b)`
- `inline std::vector<T> longestCommonSubsequence(const std::vector<T>& a, const std::vector<T>& b)`
- `inline std::vector<DiffEntry<T>> diff(const std::vector<T>& a, const std::vector<T>& b)`

### `DisjointSet`
<sub>`engine/include/maz/core/DisjointSet.hpp`</sub>

maz::core::DisjointSet — union-find over [0, n): the classic near-O(1) structure for tracking which elements belong to the same group as pairs get merged. Path compression plus union-by-rank keep find()/unite() effectively constant time (inverse-Ackermann amortised). The go-to tool for connected-components queries, Kruskal minimum-spanning-tree, maze generation (carve a wall only when it joins two different regions), flood-region merging in a tile map, island/cluster counting, and "are these two things reachable" checks — all things a game needs and Godot ships no primitive for. It also maintains a live count of disjoint sets and each set's size. Header-only, std-only.

**Types:** `DisjointSet`

### `Events`
<sub>`engine/include/maz/core/Events.hpp`</sub>

A type-safe publish/subscribe event bus — the decoupling glue between systems. A gameplay system emits an event value (any type) and every subscriber registered for THAT type is invoked, without emitter and listener knowing about each other (damage -> audio + particles + score + UI, all independent). Subscription returns a token you can later unsubscribe. Dispatch snapshots the listener list, so a handler may safely subscribe/unsubscribe or emit further events during a call. Header-only; no GPU. Not thread-safe — intended for the single game thread.

**Types:** `EventBus`

### `Expression`
<sub>`engine/include/maz/core/Expression.hpp`</sub>

Runtime math-expression parser + evaluator — Godot's Expression class. Parse a formula string ONCE (e.g. "sin(x*3) * amp + 0.5"), naming the free variables, then execute() it repeatedly with different variable values. This is the workhorse behind data-driven design: damage/difficulty/economy formulas in a config file, procedural-parameter curves, spawn weights, tool sliders — anything you'd otherwise hard-code and recompile. Recursive-descent parser -> a flat AST node pool (copyable, no owning pointers) -> a pure recursive evaluator. Deterministic and dependency-free, so it unit-tests exactly and drives a function-plot golden.  Grammar (standard precedence; '^' is right-associative and binds tighter than unary minus, so -2^2 == -(2^2) == -4, and 2^2^3 == 2^(2^3)): expr   := term (('+'|'-') term)* term   := unary (('*'|'/'|'%') unary)* unary  := ('+'|'-') unary | power power  := primary ('^' unary)? primary:= number | const | ident | ident '(' [expr (',' expr)*] ')' | '(' expr ')'  Constants: pi, tau, e. Functions: sin cos tan asin acos atan exp log log2 sqrt abs floor ceil round sign frac (1-arg); pow atan2 min max mod (2-arg); clamp lerp (3-arg). Unknown names / bad arity / syntax errors set an error string and make parse() return false.  Honest scope vs Godot's Expression: this is the numeric subset — doubles in, one double out. It does NOT evaluate Variant types, strings, booleans/comparisons, array/dictionary literals, or method calls on an arbitrary base object; those are tied to Godot's Variant/Object model and are out of scope here.

**Types:** `Expression`

### `FenwickTree`
<sub>`engine/include/maz/core/FenwickTree.hpp`</sub>

maz::core::FenwickTree — a Binary Indexed Tree: a mutable integer array that answers "sum of the first i elements" (and any range sum) in O(log n) while still allowing O(log n) updates to any element. A plain array gives O(1) update but O(n) prefix sums; a precomputed prefix array gives O(1) sums but O(n) update — the Fenwick tree is the standard structure that makes BOTH cheap at once. The key game use is DYNAMIC weighted random selection: findByPrefix() picks a bucket in O(log n) with probability proportional to its weight, and unlike a build-once alias table the weights can be changed between draws (loot tables that shift with luck stats, spawn tables that deplete as a wave clears, cumulative-frequency sampling). Also handy for running range sums over a mutable series — scoreboards, histograms, order statistics. 0-indexed API, header-only, std-only. Godot has no Fenwick tree.

**Types:** `FenwickTree`

### `Fixed`
<sub>`engine/include/maz/core/Fixed.hpp`</sub>

maz::core::Fixed — a Q16.16 fixed-point number for DETERMINISTIC simulation. Floating point gives slightly different results on different CPUs/compilers/optimisation levels, which silently desyncs lockstep multiplayer, replays, and cross-platform physics. Fixed point is pure integer arithmetic: the same inputs give bit-identical outputs everywhere, so a lockstep netcode game (or a deterministic replay) can run the simulation in Fixed and trust every client agrees. Value = raw / 65536; 16 bits of integer range (±32767) and 16 bits of fraction (~1.5e-5 resolution). Multiply/divide use 64-bit intermediates so they don't overflow. Header-only, no floats on the runtime path (float is used only for authoring conversions). Godot has no fixed-point type, so this is a genuinely-useful utility.

**Types:** `Fixed`

### `FuzzyMatch`
<sub>`engine/include/maz/core/FuzzyMatch.hpp`</sub>

maz::core fuzzy subsequence matching — the ranking primitive a command palette / search box needs, built on top of the edit-distance and bigram-similarity helpers StringUtils already provides (M290). Adds two things those don't: longestCommonSubsequenceLength() (the length of the longest in-order, not-necessarily-contiguous shared run — a diff/overlap measure distinct from edit distance), and fuzzyMatch() — an fzf/Sublime-style scorer that reports whether a short pattern's characters appear in order inside a candidate, where they landed, and how good the match is, rewarding runs of consecutive characters and matches on word boundaries (string start, after a separator, or a camelCase hump). Together with the Trie (prefix autocomplete) this ranks loose, abbreviation-style queries like "gp" -> "getPlayer". Godot's String offers only a bigram similarity ratio. Header-only, std-only, deterministic.

**Types:** `FuzzyResult`

**Functions:**

- `inline char fuzzyLower(char c)`
- `inline bool fuzzySeparator(char c)`
- `inline bool fuzzyBoundary(const std::string& text, std::size_t i)`
- `inline std::size_t longestCommonSubsequenceLength(const std::string& a, const std::string& b)`
- `inline FuzzyResult fuzzyMatch(const std::string& pattern, const std::string& text,`

### `GapBuffer`
<sub>`engine/include/maz/core/GapBuffer.hpp`</sub>

maz::core::GapBuffer — the classic text-editor data structure: a character buffer with a movable "gap" (a run of empty slots) sitting at the cursor. Typing fills the gap and deleting widens it, so edits AT THE CURSOR are O(1) amortised — no shifting the whole document on every keystroke, which a plain std::string insert/erase would do (O(n) each). Moving the cursor pays only for the distance moved, which matches how people edit (many keystrokes in one place, occasional jumps). This is the buffer behind a real code/text editor, and directly useful for the engine's in-editor script editor, the developer console line, and a chat/input field. Header-only, std-only, deterministic. Godot's TextEdit is a heavy node; this is the lightweight algorithmic core.

**Types:** `GapBuffer`

### `GrayCode`
<sub>`engine/include/maz/core/GrayCode.hpp`</sub>

maz::core Gray code + bit-change utilities. A Gray code (reflected binary) orders the integers so that each value differs from the previous by EXACTLY ONE bit — unlike ordinary binary, where e.g. 3->4 flips three bits at once. That single-bit-change property is what makes Gray codes glitch-free for rotary / position encoders (a reading caught mid-transition is off by at most one), and it gives a minimal-change enumeration order for combinations/subsets, dithering and LOD-transition sequences, and error-resilient counters. The paired `hammingDistance` counts how many bits differ between two values (popcount of XOR) — the same "how many bits changed" measure, also the standard metric for comparing perceptual image hashes (dHash/pHash) and bitmask diffs. None of these are in the standard library (unlike popcount / bit_ceil / countl_zero, which the engine already leans on). Header-only, std-only, deterministic; Godot ships none.

**Functions:**

- `inline std::vector<std::uint32_t> graySequence(int bits)`

### `Halton`
<sub>`engine/include/maz/core/Halton.hpp`</sub>

maz::core::Halton — the Halton low-discrepancy (quasi-random) sequence. Unlike a pseudo-random generator (Pcg32/Random), which can clump and leave gaps, a Halton sequence fills space EVENLY: each new point lands in the biggest remaining hole. That makes it the right tool for procedural scatter (trees, rocks, stars) that should look spread out rather than blotchy, for anti-aliasing / temporal jitter sample offsets, and for evenly probing a search space. Each coordinate is the van der Corput radical inverse of the sample index in a chosen (prime) base — deterministic and stateless, so the i-th point is always the same. Values lie in [0,1). Godot has no low-discrepancy sequence. Header-only.

**Types:** `HaltonSequence`

**Functions:**

- `inline float radicalInverse(std::uint32_t index, std::uint32_t base)`
- `inline float halton(std::uint32_t index, std::uint32_t base)`
- `inline std::pair<float, float> halton2D(std::uint32_t index, std::uint32_t baseX = 2,`

### `Hash`
<sub>`engine/include/maz/core/Hash.hpp`</sub>

maz::core hashing — CRC-32 (ISO-HDLC, the zip/PNG polynomial) and SHA-256, the checksums games use for asset integrity, save-file validation, content-addressed caches, and network message digests. This is Godot's HashingContext / crc32 territory. Both are exact, standard algorithms (verified against the published test vectors), header-only and dependency-free. CRC-32 is a fast non-crypto checksum; SHA-256 is the cryptographic digest.

**Functions:**

- `inline std::uint32_t crc32(const std::uint8_t* data, std::size_t len, std::uint32_t seed = 0)`
- `inline std::uint32_t crc32(const std::string& s)`
- `inline std::uint32_t rotr(std::uint32_t x, int n)`
- `inline std::array<std::uint8_t, 32> sha256(const std::uint8_t* data, std::size_t len)`
- `inline std::array<std::uint8_t, 32> sha256(const std::string& s)`
- `inline std::string toHex(const std::uint8_t* data, std::size_t len)`
- `inline std::vector<std::uint8_t> hexDecode(const std::string& s)`
- `inline std::string sha256Hex(const std::string& s)`
- `inline std::uint32_t rotl(std::uint32_t x, int n)`
- `inline std::array<std::uint8_t, 16> md5(const std::uint8_t* data, std::size_t len)`
- `inline std::array<std::uint8_t, 16> md5(const std::string& s)`
- `inline std::string md5Hex(const std::string& s)`
- _…and 6 more_

### `Histogram`
<sub>`engine/include/maz/core/Histogram.hpp`</sub>

maz::core::Histogram — fixed-range, equal-width histogram accumulator. Give it a value range [min, max) and a bin count; feed samples with add(). It keeps only the per-bin tallies (O(bins) memory, nothing per-sample), yet answers the questions a plain mean/variance can't: which value range is most common (mode), what fraction fell in a bin (frequency), and interpolated percentiles / median of the distribution's SHAPE. The distribution-shape complement to RunningStats (which gives mean/variance/min/max but no median or percentiles). Out-of-range samples are clamped into the edge bins so the tallies always sum to the total, while separate below()/above() counters report how many spilled past each end. The natural tool for a frame-time / latency distribution readout ("95th-percentile frame time"), telemetry buckets, damage/score spread analysis, and difficulty-tuning signals. Header-only, std-only. Godot has no histogram type.

**Types:** `Histogram`

### `Hungarian`
<sub>`engine/include/maz/core/Hungarian.hpp`</sub>

maz::core::hungarian — the Hungarian algorithm (Kuhn-Munkres) for the optimal ASSIGNMENT problem: given a cost matrix of `rows` agents against `cols` tasks (rows <= cols), match every agent to a DISTINCT task so the total cost is the minimum possible, in O(n^3). This is a genuinely different problem from the engine's pathfinding (AStar2D finds one least-cost route; this optimally pairs a WHOLE SET at once) and from a greedy nearest-assignment (which is fast but routinely non-optimal). The canonical uses: assign N attack units to N targets to minimise total travel, N defenders to N incoming threats, N workers to N jobs, or any "who does what" that must be globally best rather than locally greedy. Uses the classic potentials + augmenting-path formulation (Dijkstra-like, O(n^3)); deterministic, header-only, std-only. To MAXIMISE a score instead, negate the values before calling. Godot ships no assignment solver.

**Types:** `Assignment`

**Functions:**

- `inline Assignment hungarian(const std::vector<double>& cost, std::size_t rows, std::size_t cols)`

### `HyperLogLog`
<sub>`engine/include/maz/core/HyperLogLog.hpp`</sub>

maz::core::HyperLogLog — estimate how many DISTINCT items a stream contained using a few kilobytes of fixed memory, no matter how many billions of items flow through. Counting uniques exactly needs a set that grows with the data (megabytes for millions of distinct values); HyperLogLog answers "roughly how many different X did we see?" from a tiny fixed array of counters, trading a small, bounded error (standard error ~1.04/sqrt(m)) for O(1) memory. This is the standard tool for cardinality: distinct players online, distinct enemies a weapon has hit, distinct assets touched this session for telemetry, distinct chat authors — anywhere the count matters but storing every value would be wasteful. The trick: hash each item to 64 bits, use the top bits to pick one of m = 2^p registers, and record in that register the position of the leftmost 1-bit of the rest; a stream of n distinct items tends to produce a maximum leading-zero run of about log2(n), and averaging across registers (harmonic mean) sharpens the estimate. Registers merge by max, so per-shard sketches combine into a whole-stream one for free. Header-only, std-only, deterministic. Godot has no cardinality estimator.

**Types:** `HyperLogLog`

### `IndexedHeap`
<sub>`engine/include/maz/core/IndexedHeap.hpp`</sub>

maz::core indexed binary min-heap with decrease-key — a priority queue whose entries can be UPDATED.  A plain std::priority_queue can push and pop by priority but cannot change the priority of an element already inside it — the operation Dijkstra, A*, and event/timer queues need constantly ("this node's tentative cost just dropped; re-prioritise it"). Without it you push duplicates and filter stale pops; with it the queue stays tight. This is the classic binary heap paired with a key->slot index map, so push / pop / decreaseKey / update / contains / erase are all O(log n) (contains and priorityOf are O(1)). Keyed by a caller id (e.g. a node index), min-heap by default (smallest priority first) with a custom comparator allowed. Header-only, deterministic — unit-tested as a heapsort, and for decrease-key actually reordering the pops.

**Types:** `IndexedHeap`

### `Interpolate`
<sub>`engine/include/maz/core/Interpolate.hpp`</sub>

Fixed-timestep render interpolation — Godot's physics interpolation. Physics runs on the fixed step, but the display refreshes at its own (usually higher, non-dividing) rate; drawing the raw physics state makes motion stutter. The fix is to keep the PREVIOUS and CURRENT physics values and, each render frame, blend them by the leftover accumulator fraction (`Clock::interpolationAlpha()`), so a body glides smoothly between steps. `Interpolated<T>` is that snapshot pair; `push` is called once per fixed step, `sample(alpha)` once per render frame. Pure math, header-only, deterministic.

**Types:** `Interpolated`, `Transform2DState`

**Functions:**

- `inline double clampAlpha(double a)`
- `inline double interpLerp(double a, double b, double t)`
- `inline float interpLerp(float a, float b, double t)`
- `inline math::vec2 interpLerp(const math::vec2& a, const math::vec2& b, double t)`
- `inline math::vec3 interpLerp(const math::vec3& a, const math::vec3& b, double t)`
- `inline float lerpAngle(float a, float b, double t)`
- `inline Transform2DState interpolate(const Transform2DState& a, const Transform2DState& b, double alpha)`

### `IntervalTree`
<sub>`engine/include/maz/core/IntervalTree.hpp`</sub>

maz::core::IntervalTree — answer "which intervals contain point x?" and "which intervals overlap [a,b]?" in O(log n + k) instead of scanning all n intervals every query. An interval is a [low, high] range carrying a payload. This is the right structure whenever many time-ranges or 1-D spans must be queried repeatedly: which animation clips / audio cues / cutscene triggers are ACTIVE at the current playhead time; which reservations overlap a requested window; which spans on one axis touch a probe (a 1-D broadphase). The naive approach re-tests every interval per query — fine for a handful, wasteful for hundreds queried each frame. This is a STATIC augmented interval tree: insert all intervals, build() once (a height-balanced BST ordered by low endpoint, each node augmented with the maximum high endpoint in its subtree), then query many times; the max-endpoint augmentation prunes whole subtrees that cannot reach the query. Header-only, std-only, deterministic. Godot has no interval tree.

**Types:** `IntervalTree`

### `JaroWinkler`
<sub>`engine/include/maz/core/JaroWinkler.hpp`</sub>

maz::core Jaro & Jaro-Winkler string similarity — a normalised [0,1] closeness score tuned for SHORT strings and typos, where 1 is identical and 0 is nothing in common. It complements the fuzzy tools the engine already has: StringUtils/FuzzyMatch give Levenshtein edit distance and an fzf-style subsequence scorer, but Jaro-Winkler measures similarity differently — it counts characters that match within a sliding window, penalises transposed pairs, and (the "Winkler" part) BOOSTS strings that share a leading prefix. That makes it the go-to metric for "did you mean...?" command/name suggestions, matching a typed player or item name against a list, and de-duplicating near-identical strings, where a shared start matters and small transpositions ("MARTHA" vs "MARHTA") should barely count. Case-sensitive on raw bytes (lower-case the inputs first for case-insensitive matching). Header-only, std-only, deterministic.

**Functions:**

- `inline double jaro(const std::string& a, const std::string& b)`
- `inline double jaroWinkler(const std::string& a, const std::string& b, double prefixScale = 0.1,`

### `Jobs`
<sub>`engine/include/maz/core/Jobs.hpp`</sub>

A fixed-size worker thread pool with a task queue — the engine's parallelism foundation. submit() runs a callable on a worker and hands back a std::future for its result; parallelFor()/ parallelRanges() split an index range across the workers and block until the whole range is done, which is the common shape for data-parallel work (fractal/image gen, particle and transform updates, culling, batched pathfinding). The pool is created once and reused; the destructor drains outstanding tasks and joins. Not re-entrant: don't call parallelFor from inside a task (the caller thread blocks on the results, so nesting can starve the pool). Single-owner, single- submitter model — intended to be driven from the game thread.

**Types:** `JobSystem`

### `Kalman`
<sub>`engine/include/maz/core/Kalman.hpp`</sub>

maz::core Kalman filters — the statistically-optimal recursive estimator that fuses a noisy measurement with a model prediction while tracking its own uncertainty (variance/covariance). This is a different tool from the engine's other smoothers: OneEuroFilter is a tuned low-pass, SmoothDamp is a critically-damped spring, and PidController is a controller — none of them model measurement noise or carry an uncertainty estimate. A Kalman filter does, which is what makes it the right choice for de-noising jittery analog/sensor input, smoothing network-replicated positions, and light sensor fusion. Godot ships no Kalman filter, so this is a beyond-Godot utility. Header-only, std-only, deterministic.

**Types:** `Kalman1D`, `KalmanCV`

### `KdTree2D`
<sub>`engine/include/maz/core/KdTree2D.hpp`</sub>

maz::core 2D k-d tree — a balanced spatial index for point sets that answers nearest-neighbour, k-nearest, and radius queries fast. Maz already has broadphase structures (grid / quadtree / octree / BVH / sweep-and-prune) tuned for boxes and ray casts; a k-d tree is the standard choice for POINT proximity: "which N points are nearest this one" drives boids/flocking neighbour lists, RVO agents, nav waypoint snapping, and photon/sample gathering. Median-split build (O(n log n)); queries prune by splitting-plane distance so they stay ~O(log n) on balanced data. Header-only, deterministic; unit- tested against brute force.

**Types:** `KdTree2D`

### `KeyValueStore`
<sub>`engine/include/maz/core/KeyValueStore.hpp`</sub>

A tiny persistent key/value store backed by a line-based `key=value` text file. Standard- library only (no SDL/JSON dependency) — the caller supplies the full file path (see platform::prefPath for a cross-platform writable location). Handy for settings, high scores, and simple progress.

**Types:** `KeyValueStore`

### `Log`
<sub>`engine/include/maz/core/Log.hpp`</sub>

### `LogSinks`
<sub>`engine/include/maz/core/LogSinks.hpp`</sub>

maz::core log sinks — structured destinations for the log stream beyond the built-in coloured console. The engine's Log always prints to the console; setLogSink() adds ONE extra consumer. These helpers give that consumer real structure:  FileLogSink — append every line to a log file ("[LEVEL] message"), optionally timestamped and flushed each write, thread-safe. The persistent record a shipped game / server leaves behind for bug reports (Godot writes user://logs/godot.log; this is the same idea, engine-native). MultiSink   — fan one log stream out to several sinks at once, so a file sink and the editor's Output panel can both receive it (setLogSink only holds one).  levelName() maps LogLevel to a stable string. Install with: auto file = std::make_shared<FileLogSink>("game.log"); core::setLogSink(core::makeSink(file));            // file only, or core::setLogSink(MultiSink{ core::makeSink(file), editorPanelSink });  // file + panel  Header-only; no GPU. The console output is unaffected — these are additive.

**Types:** `FileLogSink`, `MultiSink`

**Functions:**

- `inline const char* levelName(LogLevel level)`

### `LruCache`
<sub>`engine/include/maz/core/LruCache.hpp`</sub>

maz::core::LruCache — a fixed-capacity least-recently-used cache. Stores up to `capacity` key->value entries; when a new key would overflow, the entry that has gone longest without being touched is evicted. Every get()/put() marks its key most-recently-used, so the hot working set survives and the cold tail is dropped. The standard bounded-memory memoization tool: caching decoded tiles/chunks, pathfinding results, procedural-generation outputs, or any expensive keyed computation you want to reuse without unbounded growth. Distinct from ResourceCache (ref-counted asset lifetimes, no eviction) — this is a pure size-capped recency cache. O(1) get/put via a hash map into an intrusive recency list. Tracks hit/miss counts for tuning. Header-only, std-only. Godot has no generic LRU.

**Types:** `LruCache`

### `Memory`
<sub>`engine/include/maz/core/Memory.hpp`</sub>

maz::core memory allocators — the linear/frame arena and fixed-size pool a game engine leans on to avoid per-object malloc/free churn (the source of fragmentation and frame-time spikes). Godot uses the same shapes internally; here they're first-class, header-only, and unit-tested.  LinearArena — a bump allocator over one contiguous buffer. allocate() just advances an offset (O(1), no per-allocation bookkeeping); you free EVERYTHING at once with reset(). That makes it the ideal *frame allocator*: reset() at the top of each frame, then scratch-allocate transient data (command lists, temp arrays, string builds) for free. It also supports stack-style scopes via marker()/rewind(m) — grab a marker, allocate, then rewind to release just that block.  PoolAllocator — a free-list of fixed-size blocks. allocate()/free() are O(1) and hand back the same slots repeatedly, so spawning/despawning thousands of same-sized objects (particles, bullets, entities) never fragments the heap. Exhaustion returns nullptr rather than growing.  Both are non-owning of object lifetime beyond the raw bytes: they do NOT call destructors, so use them for trivially-destructible data (or destroy objects yourself before reset/free). No GPU, no threads — drive from the game thread.

**Types:** `LinearArena`, `PoolAllocator`

**Functions:**

- `inline bool isPowerOfTwo(size_t x)`
- `inline size_t alignUp(size_t n, size_t align)`

### `MovingAverage`
<sub>`engine/include/maz/core/MovingAverage.hpp`</sub>

maz::core::MovingAverage — fixed-window rolling statistics over the last N samples: mean in O(1) per push, plus the window MIN and MAX in O(1) amortized (monotonic deques). This is distinct from the engine's other streaming stats: RunningStats (Welford) averages over ALL samples ever seen and can never "forget" old data; P2Quantile tracks a streaming percentile; RingBuffer is a raw ring with no reductions. A moving average deliberately forgets: it reports the mean of only the most recent N values, so it tracks a changing signal instead of drifting toward a lifetime average. The canonical tool for a smooth "N-frame average FPS" readout, denoising a jittery input axis or sensor, a rolling damage-per-second meter, or any "recent trend, not all-time" number. The windowed min/max come free for "worst frame time in the last second" style readouts. Header-only, std-only, deterministic. Godot ships no moving-average accumulator.

**Types:** `MovingAverage`

### `Murmur3`
<sub>`engine/include/maz/core/Murmur3.hpp`</sub>

maz::core MurmurHash3 (x86_32) — a fast, well-distributed NON-cryptographic hash (Austin Appleby, public domain), the default workhorse for hash tables, bloom filters, feature flags, and stable content/asset IDs. The engine already has cryptographic digests (SHA-1/SHA-256) and CRC32, but those are the wrong tool for hashing map keys millions of times a frame: SHA is far too slow, and CRC32 has poor avalanche (similar inputs cluster). MurmurHash3 is built for exactly this — a few multiplies and rotates per 4 bytes, strong mixing so one-bit input changes scatter the whole output, and a `seed` so you can derive independent hash functions (e.g. the k hashes a bloom filter needs). It is also a de-facto interchange standard: this implementation reproduces the canonical published test vectors byte-for-byte, so hashes computed here match those from other tools and languages. Header-only, std-only, deterministic. Godot exposes only its own String.hash and hash_djb2.

**Functions:**

- `inline std::uint32_t murmurRotl(std::uint32_t x, int r)`
- `inline std::uint32_t murmur3_32(const std::uint8_t* data, std::size_t len, std::uint32_t seed = 0)`
- `inline std::uint32_t murmur3_32(std::string_view s, std::uint32_t seed = 0)`

### `NodePath`
<sub>`engine/include/maz/core/NodePath.hpp`</sub>

maz::core NodePath — Godot's NodePath type: a parsed reference to a node (and optionally a property chain) in the scene tree, e.g. "../Enemies/Boss:health:x". Godot stores this as three pieces: an absolute flag (a leading "/"), a list of NAME components (split on "/", where "."/".." mean current/parent), and a list of SUBNAMES (the ":"-separated tail, addressing a property and sub-properties). AnimationPlayer tracks, get_node paths, and Tween property targets are all NodePaths. This mirrors Godot's parsing and its accessors (get_name_count/get_name, get_subname_count/get_subname, is_absolute, get_concatenated_names/subnames, is_empty) and reconstructs the same string. Header-only, pure, deterministic; unit-tested by round-trip.

**Types:** `NodePath`

### `Noise`
<sub>`engine/include/maz/core/Noise.hpp`</sub>

Procedural noise: smooth, seeded, reproducible pseudo-randomness over space — the primitive behind terrain heightmaps, cloud/marble textures, cave carving, and organic motion. This is classic Perlin gradient noise: a per-seed permutation table (shuffled with core::Random, so the same seed always gives the same field) plus fade/lerp interpolation of lattice gradients, yielding a value in about [-1, 1] that is 0 at integer lattice points and continuous everywhere. fbm2 layers octaves of it (fractal Brownian motion) for natural detail, normalized back into [-1, 1]. Header-only.

**Types:** `Noise`

### `NumberFormat`
<sub>`engine/include/maz/core/NumberFormat.hpp`</sub>

maz::core number formatting for HUDs and UI — turn raw numbers into the human-readable strings a game actually shows: a score with thousand separators ("1,000,000"), a timer as a clock ("1:23:45") or a compact span ("1h 23m 45s"), a big idle-game count abbreviated ("1.2M"), or an asset size in bytes ("1.5 MiB"). These are the display helpers every game re-implements; the engine had DateTime::formatTime for a wall-clock instant but nothing for elapsed durations or grouped/abbreviated magnitudes. Godot's String offers num/pad but not these, so this is parity-or-better. Header-only, std-only, deterministic, locale-independent (the separator is an explicit argument, never the C locale).

**Functions:**

- `inline std::string groupThousands(long long value, char sep = ',')`
- `inline std::string clockDuration(double seconds)`
- `inline std::string compactDuration(double seconds)`
- `inline std::string trimTrailingZeros(std::string s)`
- `inline std::string abbreviateNumber(double value, int decimals = 1)`
- `inline std::string formatBytes(unsigned long long bytes, int decimals = 1)`
- `inline std::string ordinalSuffix(long long n)`
- `inline std::string ordinal(long long n)`
- `inline std::string toRoman(int value)`

### `ObjectPool`
<sub>`engine/include/maz/core/ObjectPool.hpp`</sub>

maz::core::ObjectPool<T> — a typed recycling pool that hands out reusable objects and takes them back, so a game can spawn and despawn bullets, particles, enemies, damage numbers, or temporary buffers every frame WITHOUT churning the allocator. acquire() reuses a previously-released slot if one is free, otherwise grows by one; release() returns a slot to the free list (without destroying it) so the next acquire() reuses it. Capacity therefore rises only to the high-water mark of simultaneously-live objects and then stops — the whole point of pooling. Distinct from the engine's other two facilities: PoolAllocator hands out raw memory bytes, and SlotMap is a generational handle→value map with stable IDs across reuse; this is the simple, index-addressed live-object recycler most gameplay code actually reaches for. Backed by a std::deque so a reference returned by get() stays valid even as the pool grows. Recycled objects keep their previous value (the caller re-initialises on acquire) — pooling reuses storage, it does not reset it. Header-only, std-only.

**Types:** `ObjectPool`

### `OneEuroFilter`
<sub>`engine/include/maz/core/OneEuroFilter.hpp`</sub>

maz::core::OneEuroFilter — an adaptive low-pass filter for noisy, human-driven signals (Casiez, Roussel & Vogel 2012). Raw pointer/touch/stylus/VR-pose input is jittery when still yet needs to feel responsive when moving fast — two goals a fixed low-pass can't serve at once (smooth enough to kill jitter is always too laggy in motion). The 1€ filter resolves this by RAISING its cutoff frequency as the signal's speed rises: nearly stationary input is filtered hard (jitter vanishes), fast input is filtered lightly (lag vanishes). Two intuitive knobs: minCutoff sets the smoothing at rest (lower = smoother/laggier), beta sets how aggressively responsiveness ramps with speed (higher = less lag when moving). Distinct from SmoothDamp (which chases a target, not denoises a stream) and from a plain EMA (fixed cutoff). Header-only, std-only, deterministic. Godot has no 1€ filter.

**Types:** `LowPassFilter`, `OneEuroFilter`

### `P2Quantile`
<sub>`engine/include/maz/core/P2Quantile.hpp`</sub>

maz::core::P2Quantile — the P-Square (P²) algorithm of Jain & Chlamtac (1985) for estimating a single quantile (median, p95, p99, …) of a data stream in CONSTANT memory and a single pass, WITHOUT storing the samples. RunningStats (Welford) gives you a streaming mean/variance but cannot answer "what is the 99th-percentile frame time?"; math::quantile answers it exactly but must hold the whole dataset in RAM; core::Histogram approximates it but needs you to pick bin edges up front. P² needs neither: it keeps just five running "markers" (order statistics), nudges their positions and heights as each sample arrives, and reads back a live estimate that provably converges to the true quantile. The natural tool for live latency/percentile telemetry — p95 network ping, p99 frame time, "how bad is the slow 1%?" — over an unbounded stream. Header-only, std-only, deterministic. Godot ships no streaming quantile.

**Types:** `P2Quantile`

### `Pcg32`
<sub>`engine/include/maz/core/Pcg32.hpp`</sub>

maz::core::Pcg32 — the PCG (Permuted Congruential Generator) 32-bit random source, O'Neill's minimal `pcg32` (a 64-bit LCG state run through an xorshift+rotate output permutation). It sits beside the engine's xoshiro256** generator (core::Random): xoshiro is the default 64-bit source, PCG is the compact, statistically excellent 32-bit generator many tools/tests expect — and, unlike a bare LCG, it supports independent STREAMS (the `seq` selector), so different subsystems can draw from the same seed without correlating.  This is a faithful port of the reference pcg32 (multiplier 6364136223846793005), so it reproduces PCG's published test vectors exactly — a deterministic, portable RNG for replays, procedural generation, and shareable seeds. next() is the raw 32-bit output; helpers give a bounded int (unbiased, rejection-sampled) and a float in [0,1). Header-only, std-only.

**Types:** `Pcg32`

### `PerfBudget`
<sub>`engine/include/maz/core/PerfBudget.hpp`</sub>

maz::core::PerfBudget — performance budgets layered on the hierarchical Profiler. A game sets a per-section time budget ("physics ≤ 4 ms", "render ≤ 8 ms") and a whole-frame budget ("≤ 16.6 ms for 60 fps"); after each frame, check() compares the profiler's measured times against those budgets and reports every OVERAGE — which section blew its budget, by how much. That turns the profiler from a passive readout into an active gate: assert it in tests, log it in CI, or flash a warning on the debug overlay the instant a subsystem regresses.  This is the "performance budgets + profiling dashboards" backbone — Godot surfaces frame timings but has no formal budget/alert layer, so budget-driven regression detection is a capability beyond it. check() reads either this frame's inclusive time or the profiler's smoothed (EMA) time, so you can alert on a single spike or only on a sustained regression. Header-only, deterministic (pure comparison over profiler data), no GPU.

**Types:** `PerfBudget`

### `PidController`
<sub>`engine/include/maz/core/PidController.hpp`</sub>

maz::core::PidController — a classic proportional-integral-derivative feedback controller. Given a target (setpoint) and a measurement each step, it returns a control signal that drives the measurement toward the target: the P term reacts to the current error, the I term accumulates past error to erase steady-state offset (e.g. a constant disturbance the P term alone can't cancel), and the D term damps by reacting to the error's rate of change. The workhorse behind self-correcting systems Godot has no built-in for: a turret smoothly tracking a moving target, a hovering/thrusting vehicle holding altitude, an auto-throttle or cruise control, a self-balancing biped, or an adaptive-difficulty signal chasing a target win-rate. Includes anti-windup (the integral accumulator is clamped), output clamping, and an optional derivative-on-measurement mode that avoids the output spike ("derivative kick") a sudden setpoint change would otherwise cause. Header-only, std-only, deterministic.

**Types:** `PidController`

### `PoissonDisk`
<sub>`engine/include/maz/core/PoissonDisk.hpp`</sub>

maz::core Poisson-disk sampling (Bridson's algorithm) — "blue noise" point scatter where no two points are closer than a minimum radius, yet coverage stays dense and even. It looks far more natural than uniform-random scatter (which clumps) for placing grass, trees, rocks, spawn points, stipple dots, or sampling positions. Godot has no built-in blue-noise sampler, so this is a beyond-parity extra. Bridson runs in O(n) using a background grid for the neighbour check. Deterministic given a seed (uses core::Pcg32). Header-only, pure — unit-tests exactly (min-distance + in-bounds).

**Functions:**

- `inline std::vector<math::vec2> poissonDiskSample(math::vec2 boundsMin, math::vec2 boundsMax,`

### `Profiler`
<sub>`engine/include/maz/core/Profiler.hpp`</sub>

Hierarchical CPU profiler: named, nestable timing zones that answer "where did the frame go?". A zone is opened with begin(name) and closed with end(); zones nest, so the tree records both inclusive time (the whole span) and self time (inclusive minus the direct children) — the two numbers that actually locate a hotspot. Per frame each distinct zone name aggregates its call count, inclusive, and self microseconds; across frames an exponential moving average smooths the display so the numbers are readable instead of flickering.  The core is time-source-agnostic: begin/end take a monotonic microsecond timestamp, so tests and deterministic demos feed synthetic timestamps (no wall clock) and get reproducible output, while real code uses nowMicros() (steady_clock) or the ScopedZone RAII helper. Header-only, std only.

**Types:** `Profiler`

**Functions:**

- `inline uint64_t nowMicros()`

### `ProjectSettings`
<sub>`engine/include/maz/core/ProjectSettings.hpp`</sub>

maz::core ProjectSettings — the central, project-wide settings store behind Godot's ProjectSettings singleton and its `project.godot` file: the one place that answers "what is this game called, what scene does it start on, how big is the window" plus any number of typed key->value settings organized by Godot-style dotted paths (e.g. "application/config/name", "display/window/size/viewport_width"). Both the editor (which needs the main scene + window size) and the export/packaging step read from here. Values are typed (bool / int / float / string); `save` serializes to Godot's sectioned `project.godot` text format (the first path segment becomes a `[section]`), and `load` reads it back, so settings round-trip exactly. Pure CPU string/number work — no filesystem calls here (the caller reads/writes the text) — so it unit-tests headlessly. It composes with `core::ConfigFile` (generic INI) rather than replacing it: this layer adds typed values, Godot key conventions, and the project-manifest convenience accessors.

**Types:** `SettingValue`, `ProjectSettings`

### `RadixSort`
<sub>`engine/include/maz/core/RadixSort.hpp`</sub>

maz::core::RadixSort — sort by an integer (or float) key in LINEAR time, O(n), instead of the O(n log n) of a comparison sort. A renderer sorts thousands of draw calls every frame by a packed 32/64-bit sort key (layer << depth << material) to batch state and draw front-to-back; a particle system sorts by camera distance for correct alpha blending; an ECS sorts entities by a packed archetype key. At those sizes, run every frame, the difference between n·log n and n comparisons is real. Radix sort achieves it by bucketing on one byte of the key at a time (a stable counting sort per byte, least-significant byte first), so after 4 passes (32-bit) or 8 passes (64-bit) the array is fully ordered — with NO key comparisons at all. The key-plus-payload variant (radixSortByKey) is STABLE: items with equal keys keep their original order, which is what makes it safe to sort by successive keys. Floats are handled via the standard order-preserving bit transform, so depth sorting "just works" including negatives. Header-only, std-only, deterministic. Godot has no radix sort; this is the workhorse behind fast per-frame ordering.

**Functions:**

- `inline std::uint32_t floatSortKey(float f)`
- `inline float floatFromSortKey(std::uint32_t v)`
- `inline void radixSortFloats(std::vector<float>& a)`

### `Random`
<sub>`engine/include/maz/core/Random.hpp`</sub>

Deterministic random-number generator: one seeded, reproducible source of randomness for gameplay and procedural generation, so a given seed always produces the same world/loot/spread — essential for replays, tests, and shareable "seed" content. Replaces the ad-hoc xorshift each demo used to hand-roll. The core is xoshiro256** (fast, high quality) seeded through SplitMix64 so even a small or zero seed fills the state well. Everything derives from next(): floats in [0,1), inclusive int ranges, weighted picks, Fisher-Yates shuffles, a Gaussian, and an angle. std-only (no glm) so core keeps zero dependencies. Header-only.

**Types:** `Random`

### `RandomDistributions`
<sub>`engine/include/maz/core/RandomDistributions.hpp`</sub>

maz::core random distribution sampling — Poisson, exponential, and geometric variates on top of ANY uniform source. core::Random already gives you uniform floats, integer ranges, weighted picks, shuffles, and a Gaussian; these three fill the classic "event timing" gap that games lean on constantly: * exponential(rate)  — the WAIT between independent random events (respawn gaps, next-drop timer, radioactive-decay-style spacing). Memoryless: the mean wait is 1/rate. * poisson(lambda)    — HOW MANY independent events land in one fixed interval (enemies spawned this second, loot rolls, packets this tick). Mean and variance both == lambda. * geometric(p)       — how many Bernoulli(p) TRIALS until the first success (crit-streak length, "keep rolling until a hit"). Mean == 1/p. Each is a free function template taking a callable `u` that returns a double in [0,1) — so it works with Random (`[&]{ return rng.nextDouble(); }`), Pcg32, or a deterministic test stub, with no hard dependency. Header-only, std-only, deterministic given the source. Godot exposes only uniform + normal RNG.

### `Reflect`
<sub>`engine/include/maz/core/Reflect.hpp`</sub>

maz::core::TypeDesc — minimal, type-SAFE reflection: register a struct's fields once (by member pointer) and then read/write them generically by name. This is the small slice of Godot's ClassDB / property system that actually earns its keep: it powers reflection-driven serialization (walk the properties, emit each) and an editor inspector (list name+type, get/set live) WITHOUT hand-writing a save/load and an inspector row per field.  It uses member pointers (obj.*m), not raw byte offsets, so it's fully type-checked at registration and never invokes UB. Supported field types: bool, any integral, any floating-point, and std::string — the value scalars a data-driven game config needs. A PropValue is a tagged scalar; get()/set() convert between it and the real field. serialize()/deserialize() round-trip every registered field to a compact text blob. Header-only, no GPU, no threads.

**Types:** `PropValue`, `TypeDesc`

### `Replay`
<sub>`engine/include/maz/core/Replay.hpp`</sub>

maz::core::Replay — deterministic input record/replay for a fixed-step simulation, the foundation Godot leans on for reproducible runs (and the basis of replays, ghosts, netcode rollback, and automated play-tests). The idea: if the simulation is deterministic (fixed timestep + the seeded core::Random + no wall-clock reads), then the ONLY nondeterministic input is the player's controls. Record one small input snapshot per fixed step and you can replay the entire session bit-for-bit by feeding those snapshots back in the same order.  Replay<T> stores a stream of frames of a trivially-copyable input type T (e.g. a bitmask of held buttons + an analog stick). record() appends a frame; frame(i) reads one back. serialize() writes a compact, versioned, little-endian binary blob that RLE-compresses identical consecutive frames — which is most of them, since input rarely changes every 1/60 s — so a idle minute costs a handful of bytes, not thousands. load() restores it. Endianness is fixed (LE) so a replay records on one machine and plays on another. Header-only, no GPU, no threads, no I/O — the caller owns the bytes.

**Types:** `Replay`

### `ReservoirSampler`
<sub>`engine/include/maz/core/ReservoirSampler.hpp`</sub>

maz::core::ReservoirSampler — uniform random selection of k items from a stream of UNKNOWN length, in a single pass and O(k) memory (Vitter's "algorithm R"). You feed items in one at a time and, at any moment, hold k of them chosen so that every item seen so far had an equal chance of being kept — no need to store or even count the stream up front. That is the right tool for picking N random spawn points from a candidate stream, sampling a handful of events from a firehose, choosing random loot from a generated pile, or keeping a representative subset of telemetry without unbounded memory. Deterministic from a caller-supplied Pcg32 (replays / lockstep). Godot has no reservoir sampler. Header-only.

**Types:** `ReservoirSampler`

**Functions:**

- `inline std::vector<T> reservoirSample(const std::vector<T>& items, std::size_t k, Pcg32& rng)`

### `Resources`
<sub>`engine/include/maz/core/Resources.hpp`</sub>

A generic reference-counted resource cache — the core of an asset manager. Resources are keyed (typically by path or a string id); the first acquire() of a key builds the value via a loader callback and stores it, and every later acquire() of the same key returns the SAME instance and bumps a reference count. release() drops a reference and, when the count reaches zero, evicts the entry (optionally running an unload callback to free a GPU/file handle first). This is what lets a game request the same texture/mesh/sound a hundred times but load it once. Header-only, no GPU; works for any key/value pair, so it unit-tests without a renderer.  std::unordered_map is used for storage: references/pointers to a value stay valid across inserts and erases of OTHER keys, so a T& returned by acquire() remains valid until that key is evicted.

**Types:** `ResourceCache`

### `RingBuffer`
<sub>`engine/include/maz/core/RingBuffer.hpp`</sub>

RingBuffer<T> — a fixed-capacity circular buffer, the workhorse behind rolling histories (frame-time / FPS graphs, moving averages), input buffers (a fighting game's last-N button presses, jump "coyote" windows), replay traces, and streaming audio/network queues. It serves two idioms from one structure: * a ROLLING WINDOW — `push` always succeeds; once full it overwrites the OLDEST element, so the buffer always holds the most recent `capacity` items (the frame-time graph pattern). * a bounded FIFO QUEUE — `pushBack` reports whether it accepted (rejecting when full) and `popFront` drains oldest-first. Indexing is logical: `at(0)` is always the oldest live element, `at(size()-1)` the newest, regardless of where they physically wrap. Godot keeps a `RingBuffer` for exactly these jobs. Pure container, header-only, deterministic — it unit-tests exactly and drives a golden (a scrolling history plot).

**Types:** `RingBuffer`

### `RollingWindow`
<sub>`engine/include/maz/core/RollingWindow.hpp`</sub>

maz::core::RollingWindow — fixed-capacity sliding-window statistics. Keeps only the most recent N samples and reports their sum / mean / min / max, dropping the oldest sample as each new one arrives. Unlike RunningStats (all-time, never forgets) this answers "what have the last N samples been doing" — the natural shape for a live "average FPS over the last 60 frames" readout, a recent-input smoother, a scrolling telemetry graph, or a short-horizon trend signal. The running sum makes mean() O(1); the min() / max() use monotonic index deques so they stay O(1) amortised even as the window slides (no rescan of the window on eviction). Header-only, std-only. Godot has no rolling-window accumulator.

**Types:** `RollingWindow`

### `RunningMedian`
<sub>`engine/include/maz/core/RunningMedian.hpp`</sub>

maz::core::RunningMedian — a fixed-window streaming MEDIAN filter: feed samples one at a time and read the median of the most recent N at any moment. The median is the great outlier-resistant smoother: a lone spike (a glitched sensor reading, a dropped-frame time, a network hiccup) is an extreme value that the median simply steps over, whereas a moving AVERAGE (core::MovingAverage) gets dragged toward the spike and smears it across the window. Crucially the median also preserves genuine step changes (edges) that a mean rounds off. This is the 1D streaming cousin of the engine's image median filter (render::medianFilter, which denoises a 2D image): use it for a jitter-free frame-time readout, a de-glitched analog stick or gyro axis, or robust smoothing of any noisy per-frame signal. Backed by an ordered multiset for O(log N) updates. Header-only, std-only, deterministic. Distinct from P2Quantile (a streaming ESTIMATE over all history) — this is the EXACT median of a sliding window. Godot ships no running-median filter.

**Types:** `RunningMedian`

### `RunningStats`
<sub>`engine/include/maz/core/RunningStats.hpp`</sub>

maz::core::RunningStats — online (streaming) statistics via Welford's algorithm. Feed samples one at a time and read back count / mean / variance / standard deviation / min / max at any moment, in O(1) memory and a single pass, without storing the samples. Welford's recurrence is numerically stable (no catastrophic cancellation from the naive "sum of squares minus square of sum"), so it stays accurate even over millions of samples. The natural tool for live frame-time / FPS statistics, sensor and input smoothing diagnostics, telemetry aggregates, and adaptive-difficulty signals. Header-only, std-only. Godot has no running-statistics accumulator.

**Types:** `RunningStats`

### `SceneStack`
<sub>`engine/include/maz/core/SceneStack.hpp`</sub>

The application-framework layer: a stack of game "scenes" (menu, gameplay, pause, game-over) with a proper lifecycle. Pushing a scene pauses the one below and enters the new one; popping exits it and resumes the one revealed. Scenes can be transparent overlays (a pause menu drawn over the frozen game) via blocksUpdate()/blocksRender(). This is what ties menus and gameplay into a real game instead of a single-screen demo. render() is a hook (default no-op) so the core stays renderer-agnostic and unit-tests headless; the app overrides it. Stack mutations requested during update() are DEFERRED and applied afterwards, so a scene can safely pop or replace itself.

**Types:** `Scene`, `SceneStack`

### `Scheduler`
<sub>`engine/include/maz/core/Scheduler.hpp`</sub>

Time-based scheduling: the "do this later" and "do this on a beat" primitive nearly all gameplay needs — spawn a wave every few seconds, fire a callback after a delay, run a cooldown, drive a scripted sequence. Two pieces: * Scheduler — fire-and-forget timers: after(delay) runs a callback once; every(interval, count) runs it repeatedly (a finite count or forever); cancel() stops a pending one by handle. update() advances all timers and fires whatever came due, catching up if a big dt spans several intervals, and staying safe when a callback schedules or cancels timers mid-update. * Sequence — an ordered script of steps played over time: wait(seconds), call(fn), and span(duration, fn(progress 0..1)) for animated stretches; optionally loop. Built on the same fixed-step dt the rest of the engine runs on, so it's fully deterministic. Header-only.

**Types:** `Scheduler`, `Sequence`

### `SegmentTree`
<sub>`engine/include/maz/core/SegmentTree.hpp`</sub>

maz::core::SegmentTree — dynamic range queries with point updates for any associative operation. This fills the gap between the engine's two existing range structures: FenwickTree does prefix SUMS with point updates but cannot answer a range MIN/MAX; SparseTable answers range min/max in O(1) but only over a STATIC array that never changes. A segment tree does both — arbitrary range min / max / sum / gcd AND point updates, each in O(log n). The tool for a live heightfield's "tallest point in this span" that changes as terrain deforms, a scrolling audio meter's running peak, damage-over-segments, or any "combine over [l,r] while values keep changing" query. Templated on the combine op (default sum) with a caller-supplied identity, so min/max/gcd all work. Iterative (cache-friendly, no recursion); the query keeps left/right accumulators separate so it stays correct even for non-commutative operations. Header-only, std-only. Godot exposes no segment tree.

**Types:** `SegmentTree`

### `ShuffleBag`
<sub>`engine/include/maz/core/ShuffleBag.hpp`</sub>

maz::core::ShuffleBag<T> — a "deal from a bag" randomizer that gives FAIR, clump-free randomness. Unlike an independent weighted roll (which can hand you the same result five times in a row, or leave one option starved for a long stretch), a shuffle bag holds one token per desired outcome, deals them in random order, and only refills-and-reshuffles once the bag is empty — so over each cycle every outcome appears EXACTLY its intended number of times, and the worst-case drought is bounded. This is the Tetris "7-bag" piece randomizer, and exactly what you want for enemy-type spawns, music-playlist shuffle, card decks, or random events that should feel fair rather than streaky. It also (by default) avoids handing you the same value twice across a refill boundary, so no back-to-back repeats when the outcomes are distinct. Complements the engine's AliasTable (fast independent weighted draws) and ReservoirSampler (streaming sample) — this is the without-replacement, cycle-fair option. Any RNG with an inclusive `range(int lo, int hi)` works (e.g. core::Pcg32). Header-only, std-only, fully deterministic for a given seed.

**Types:** `ShuffleBag`

### `Signal`
<sub>`engine/include/maz/core/Signal.hpp`</sub>

Per-object named signals — Godot's `signal` / `connect` / `emit`. The engine already has a global, by-TYPE publish/subscribe bus (`core::EventBus`), but Godot's signals are a different, more granular pattern: each OBJECT owns its own named channels ("this button's `pressed`", "this health's `changed`") that carry typed arguments, and other objects connect callbacks to a SPECIFIC emitter's signal. On top of plain connect/emit it adds the two flavours Godot leans on constantly — ONE-SHOT connections (fire once, then auto-disconnect) and DEFERRED connections (the call is queued and run later at a flush point instead of re-entrantly mid-emit). Header-only, type-safe, no allocation beyond the connection list; unit-tests headlessly.

**Types:** `Signal`

### `SimplexNoise`
<sub>`engine/include/maz/core/SimplexNoise.hpp`</sub>

maz::core 2D Simplex noise — the gradient noise that complements the engine's Perlin (core::Noise) and Worley (core::CellularNoise). Simplex noise (Ken Perlin's successor to classic Perlin noise) tiles space with triangles rather than a square grid, which removes the axis-aligned directional artifacts Perlin can show, evaluates with fewer multiplies, and has well-defined continuous gradients — the usual default for terrain height, clouds, flow maps, and organic textures (it's what FastNoiseLite exposes as its default). This is a seedable, hash-gradient implementation (no big permutation table) following Gustavson's reference construction; output is approximately [-1, 1]. Pure float math, no allocation, deterministic — unit-tested by its range, continuity, zero-ish mean, and determinism (Simplex has no exact lattice values to check, unlike a radical inverse).

**Functions:**

- `inline uint32_t simplexMix(uint32_t x)`
- `inline uint32_t simplexHash(int i, int j, uint32_t seed)`
- `inline float simplexGrad(uint32_t h, float x, float y)`
- `inline float simplex2D(float x, float y, uint32_t seed = 0)`
- `inline float simplexFbm2D(float x, float y, int octaves, float lacunarity = 2.0f,`

### `SimulatedAnnealing`
<sub>`engine/include/maz/core/SimulatedAnnealing.hpp`</sub>

maz::core::simulatedAnnealing — a general-purpose optimizer for hard problems where you cannot enumerate every option: it searches for a state that minimises an "energy" (cost) function by wandering the state space, always accepting improvements but ALSO accepting worse states with a probability that shrinks as a "temperature" cools. That controlled willingness to go uphill early lets it escape local minima that a pure hill-climb would get stuck in — the reason it solves layout, scheduling, tour (TSP-style), puzzle, and procedural-placement problems that have no closed-form answer. It is generic: you supply the State type, an `energy(state)` cost, and a `neighbour(state, rand01)` that returns a slightly-mutated copy, plus a cooling schedule. Deterministic given a seed (embedded splitmix64 — no <random>, no clock), so a level or layout generated this way is reproducible. Header-only, std-only. Godot ships no optimizer.

**Types:** `AnnealResult`

### `SlotMap`
<sub>`engine/include/maz/core/SlotMap.hpp`</sub>

Generational-index slot-map / object pool — the data structure behind stable, safe handles (Godot's RID, an ECS's entity ids). The problem it solves: you want to hand out lightweight IDs to pooled objects, REUSE storage when an object is freed, and still DETECT a stale ID that refers to a slot whose original occupant is long gone (the classic "ABA" dangling-handle bug). A `SlotMap` stores values in a dense-ish slot array; each slot carries a GENERATION counter. `insert` returns a `SlotHandle{index, generation}`; freeing a slot bumps its generation, so any handle minted before the free no longer matches and `get` returns null — even after the slot is reused by a brand-new object. Freed slots are recycled through a free list, so memory doesn't grow unbounded. Header-only, std-only, unit-testable.

**Types:** `SlotHandle`, `SlotMap`

### `SmoothDamp`
<sub>`engine/include/maz/core/SmoothDamp.hpp`</sub>

maz::core::smoothDamp — critically-damped spring smoothing toward a (possibly moving) target. Given the current value, a target, and a persistent velocity, it eases the value toward the target over an approximate time-to-reach (smoothTime) without the overshoot/ringing an under-damped spring produces and without the abrupt arrival of a linear move. This is the standard "buttery" follow behaviour — a camera trailing the player, a UI element gliding to a resting position, a health bar catching up to a new value — where the target keeps changing and you want the motion to stay smooth and interruptible frame to frame. The classic Game-Programming-Gems / Unity Mathf.SmoothDamp recurrence: stable for any dt, converges monotonically (no overshoot), and an optional maxSpeed caps how fast the value may travel. Godot has no SmoothDamp equivalent (its Tween/lerp are fixed-duration, not velocity-carrying critically-damped springs). Header-only, std-only, deterministic.

**Types:** `SmoothDamp`

**Functions:**

- `inline double smoothDamp(double current, double target, double& velocity, double smoothTime, double dt,`

### `SpaceFilling`
<sub>`engine/include/maz/core/SpaceFilling.hpp`</sub>

maz::core — space-filling curve encoders: Morton (Z-order) and Hilbert. These map multi-dimensional integer grid coordinates to a single scalar index (and back) such that points close on the line tend to be close in space. They are the standard tool for cache-coherent grid traversal, spatial hash keys, quadtree/octree node ordering, texture swizzling, and locality-preserving sorts. Morton codes are the cheap workhorse (pure bit interleaving); the Hilbert curve is costlier but has strictly better locality — consecutive indices are ALWAYS grid neighbours (Manhattan distance exactly 1), which Morton's diagonal jumps do not guarantee. Godot exposes no built-in space-filling curve, so this is a beyond-Godot utility. All functions are exact bijections over their coordinate range. Header-only, std-only.

**Functions:**

- `inline uint64_t part1By1(uint32_t x)`
- `inline uint32_t compact1By1(uint64_t v)`
- `inline uint64_t part1By2(uint32_t x)`
- `inline uint32_t compact1By2(uint64_t v)`
- `inline uint64_t mortonEncode2(uint32_t x, uint32_t y)`
- `inline std::pair<uint32_t, uint32_t> mortonDecode2(uint64_t code)`
- `inline uint64_t mortonEncode3(uint32_t x, uint32_t y, uint32_t z)`
- `inline void mortonDecode3(uint64_t code, uint32_t& x, uint32_t& y, uint32_t& z)`
- `inline void hilbertRot(uint32_t n, uint32_t& x, uint32_t& y, uint32_t rx, uint32_t ry)`
- `inline uint64_t hilbertXY2D(uint32_t n, uint32_t x, uint32_t y)`
- `inline void hilbertD2XY(uint32_t n, uint64_t d, uint32_t& x, uint32_t& y)`

### `SparseTable`
<sub>`engine/include/maz/core/SparseTable.hpp`</sub>

maz::core::SparseTable — O(1) range queries over a STATIC array for any idempotent associative operation (min, max, gcd, bitwise and/or). After an O(n log n) build it answers "the combined value over [l, r]" in constant time by overlapping two power-of-two blocks. This complements FenwickTree (which does prefix sums with point updates): a sparse table can't be updated, but for a fixed array it answers min/max range queries far faster. The tool for "tallest terrain height in this span", static interval min/max, and range-minimum-query building blocks. Godot exposes no RMQ structure, so this is a beyond-Godot utility. Header-only, std-only.

**Types:** `MinOp`, `MaxOp`, `SparseTable`

### `Spring`
<sub>`engine/include/maz/core/Spring.hpp`</sub>

maz::core SPRING — a damped-harmonic-oscillator value smoother for UI juice and gameplay motion: eases a value toward a moving target with a natural bounce. Unlike `core::SmoothDamp` (critically damped — glides in with no overshoot), a Spring is tunable from bouncy (low damping, overshoots and settles) through critical to sluggish (over-damped), so it drives springy menus, camera lag, knockback recovery, cursor trails, and "pop" on pickups. Parameterised by `frequency` (Hz — how fast it oscillates) and `damping` (the damping ratio ζ: <1 bouncy, 1 critical, >1 sluggish). Integrated with sub-stepped semi-implicit (symplectic) Euler so it stays stable for any dt and parameters. Header-only, deterministic. Run one Spring per axis for 2D/3D motion.

**Types:** `Spring`

### `StringFormat`
<sub>`engine/include/maz/core/StringFormat.hpp`</sub>

maz::core string formatting — Godot's String.format: substitute {placeholder} tokens in a template with values drawn from an Array (positional {0},{1},...) or a Dictionary (named {key}). This is the data-driven text layer for localized strings, debug readouts, and templated UI: "Hi {name}, you have {0} gold". Values are stringified via Variant (so numbers/vectors/bools format consistently). Unknown or malformed placeholders are left verbatim, matching Godot. Header-only, pure, unit-tested.

**Functions:**

- `inline std::string formatImpl(const std::string& fmt, Resolver&& resolve)`
- `inline std::string formatWith(const std::string& fmt, const Dictionary& args)`
- `inline std::string formatWith(const std::string& fmt, const Array& args)`

### `StringHash`
<sub>`engine/include/maz/core/StringHash.hpp`</sub>

maz::core String hashing — Godot's String.hash() / hash64(): the classic djb2 hash (hash*33 + c, seeded at 5381). Godot's String is code-point based, so it hashes over Unicode CODE POINTS, not raw UTF-8 bytes — meaning "é" hashes as one value (U+00E9), not as its two encoded bytes. These reuse core::utf8Decode so a UTF-8 std::string hashes identically to the same text in Godot. Deterministic, header-only; used for fast string keying, dictionary bucketing, and content fingerprints.

**Functions:**

- `inline std::uint32_t stringHash32(const std::string& s)`
- `inline std::uint64_t stringHash64(const std::string& s)`

### `StringId`
<sub>`engine/include/maz/core/StringId.hpp`</sub>

Interned strings — Godot's StringName. A game refers to the same names constantly (node names, signal names, animation tracks, input actions, entity tags), and comparing/hashing those as raw std::strings is slow and allocation-heavy. INTERNING each unique string once, into a table, turns every later reference into a small integer HANDLE: comparison is an int compare, hashing is trivial, and the original text is one reverse lookup away. Maz had no such facility — every subsystem hand-hashed or string-compared. This adds a `StringTable` (own the pool) + a lightweight `StringId` handle, plus the FNV-1a hash the table uses internally. std-only, header-only, no engine deps.

**Types:** `StringId`, `StringTable`, `hash`

**Functions:**

- `inline uint32_t fnv1a32(std::string_view s)`

### `StringUtils`
<sub>`engine/include/maz/core/StringUtils.hpp`</sub>

maz::core string utilities — the everyday text helpers Godot's String type bundles (split, join, strip_edges, lpad/rpad, replace, begins_with/ends_with, to_lower/upper, repeat, count). Maz already has StringId/StringTable for INTERNING; this is the plain manipulation layer that data parsing, UI text, save formats, and command handling all reach for. Pure std::string, header-only, deterministic — unit-tests exactly. (Locale-aware casing and Godot's quirky capitalize() are out of scope; casing here is ASCII.)

**Functions:**

- `inline std::vector<std::string> split(const std::string& s, const std::string& delim,`
- `inline std::string join(const std::vector<std::string>& parts, const std::string& sep)`
- `inline bool beginsWith(const std::string& s, const std::string& prefix)`
- `inline bool endsWith(const std::string& s, const std::string& suffix)`
- `inline bool contains(const std::string& s, const std::string& needle)`
- `inline std::string trimPrefix(const std::string& s, const std::string& prefix)`
- `inline std::string trimSuffix(const std::string& s, const std::string& suffix)`
- `inline int getSliceCount(const std::string& s, const std::string& splitter)`
- `inline std::string getSlice(const std::string& s, const std::string& splitter, int slice)`
- `inline std::string getSlicec(const std::string& s, char splitter, int slice)`
- `inline std::string indent(const std::string& s, const std::string& prefix)`
- `inline std::string dedent(const std::string& s)`
- _…and 77 more_

### `StronglyConnected`
<sub>`engine/include/maz/core/StronglyConnected.hpp`</sub>

maz::core::stronglyConnectedComponents — group the nodes of a directed graph into its strongly-connected components (SCCs): maximal sets where every node can reach every other. The companion to topologicalSort (M637): where that orders a graph with no cycles, this one FINDS the cycles and clusters them. Uses: collapsing a tangle of mutually-dependent quests / dialogue states / crafting recipes into one unit, detecting circular references in a scene or resource graph and reporting exactly which nodes form each loop, condensing a messy dependency graph into a clean DAG (each SCC becomes one super-node), and deadlock/liveness analysis on a state machine. Tarjan's algorithm, run ITERATIVELY (an explicit work stack, not recursion) so it is safe on very deep graphs. Components come out in REVERSE TOPOLOGICAL order of the condensation — a "sink" component (one that depends on nothing further) appears before the components that point into it — and each component's node list is sorted ascending for determinism. A node with no cycle is its own singleton component. Godot ships no SCC primitive. Header-only, std-only.

**Types:** `SccResult`

**Functions:**

- `inline SccResult tarjan(const std::vector<std::vector<int>>& successors)`
- `inline SccResult stronglyConnectedComponents(int nodeCount, const std::vector<std::pair<int, int>>& edges)`
- `inline SccResult stronglyConnectedComponents(const std::vector<std::vector<int>>& successors)`
- `inline bool sameComponent(const SccResult& scc, int a, int b)`

### `SuffixArray`
<sub>`engine/include/maz/core/SuffixArray.hpp`</sub>

maz::core::SuffixArray — index a string so that ANY substring can be located fast. A suffix array is the list of all the string's suffixes sorted alphabetically, stored as their start positions; because the suffixes are sorted, every occurrence of a search pattern forms one contiguous block, found by binary search in O(m log n) instead of scanning the whole text. It is the compact, cache-friendly cousin of a suffix tree and the backbone of substring search over large static text: searching a big log or script dump, autocomplete over a dictionary, dedup/longest-repeated-substring analysis, and building block for bioinformatics-style matching. Paired with the LCP (longest-common-prefix) array — the overlap between each adjacent pair of sorted suffixes — it also answers "what is the longest chunk that repeats?" directly. Built by prefix doubling. Header-only, std-only, deterministic. Godot has no text index; this is well beyond String.find for repeated queries on fixed text.

**Types:** `SuffixArray`

### `SummedAreaTable`
<sub>`engine/include/maz/core/SummedAreaTable.hpp`</sub>

maz::core::SummedAreaTable — a 2D prefix-sum table (a.k.a. integral image, Crow 1984) that answers the SUM (or AVERAGE) over ANY axis-aligned rectangle in O(1), no matter how large the rectangle, after an O(width*height) build. Each cell of the table stores the sum of everything above-and-left of it, so a rectangle sum is just four table lookups (bottom-right - top-strip - left-strip + double-counted-corner). This is the trick behind a constant-time box blur (any radius costs the same), average brightness or height over a region, adaptive/local thresholding, fast region queries on an influence or heat map, and Viola-Jones-style feature sums. It generalises the engine's 1D range structures (FenwickTree does 1D prefix sums with updates; SparseTable does 1D idempotent range queries) to two dimensions for a STATIC grid. Templated on the accumulator type (double for real grids, a wide integer for exact counts). Header-only, std-only, deterministic. Godot ships no summed-area table.

**Types:** `SummedAreaTable`

### `Telemetry`
<sub>`engine/include/maz/core/Telemetry.hpp`</sub>

maz::core::Telemetry — an OPT-IN, privacy-first analytics/crash-reporting buffer, Maz's answer to Godot's opt-in usage reporting. The contract, in order of importance:  1. OFF BY DEFAULT. Nothing is recorded until enable(true) is called with explicit consent. Every event()/count()/timing() while disabled is silently dropped (and counted, so a game can show "N events withheld — telemetry is off"). There is no implicit or first-run opt-in. 2. LOCAL FIRST. Events accumulate in an in-memory buffer and serialize to newline-delimited JSON (JSONL) — the same shape you'd write to a local file or POST to a sink. There is no built-in network transport: flush() hands the batch to a sink callback YOU install (write a file, upload, or drop it), so the engine never phones home on its own. 3. NO PII. An event is a short name plus small string/number fields. The session id is a caller- supplied opaque token (use a random id, never a username/email/device id). Field values are whatever the game passes — keep them anonymous; this module makes it easy to stay clean but can't read minds.  Determinism: events carry a monotonically increasing sequence number (not a wall-clock time), so the JSONL is byte-stable and unit-tests exactly. A game can add its own timestamp field if it wants one. Header-only, no GPU, no threads — pumped from the game thread.

**Types:** `Telemetry`

### `Time`
<sub>`engine/include/maz/core/Time.hpp`</sub>

Fixed-timestep clock. Decouples the deterministic simulation step from render frame rate using the classic accumulator pattern (default step = 1/60 s).  Usage per frame: clock.beginFrame(); while (clock.consumeFixedStep()) { update(clock.fixedDelta()); } render(clock.interpolationAlpha());

**Types:** `Clock`

### `TokenBucket`
<sub>`engine/include/maz/core/TokenBucket.hpp`</sub>

maz::core::TokenBucket — the classic token-bucket rate limiter: a bucket holds up to `capacity` tokens, refills at a steady `refillPerSecond`, and an action spends tokens; if the bucket is dry the action is refused. This is the standard way to allow a controlled BURST (spend the whole bucket at once) while capping the sustained rate over time. Distinct from game::CooldownManager (a per-ability binary "ready or not" timer with no burst): a token bucket accumulates several charges and refills fractionally, so it models "3 dashes, one back every 2 seconds", chat/emote spam limits, outgoing packet or RPC throttling in netcode, and spawn/particle-emission budgets. Advance it by the frame delta each tick, then tryConsume(). Deterministic, header-only, std-only — no clock inside, so tests drive it with explicit dt. The leaky bucket's cousin; Godot ships neither.

**Types:** `TokenBucket`

### `TopologicalSort`
<sub>`engine/include/maz/core/TopologicalSort.hpp`</sub>

maz::core::topologicalSort — order the nodes of a directed graph so every "must come before" edge points forward. The workhorse behind dependency resolution: a tech tree or skill tree whose nodes unlock in a legal order, a crafting chain (smelt ore before forging the blade), quest/prerequisite gating, asset or scene build order, and any "run these tasks respecting their dependencies" scheduler. If the graph has a cycle (a circular prerequisite that can never be satisfied) it says so and reports the nodes trapped in / downstream of the cycle, which is exactly the diagnostic a designer needs to find the bad edge. Uses Kahn's algorithm and always returns the LEXICOGRAPHICALLY SMALLEST valid order (ties broken by lowest node index), so the result is fully deterministic. Godot ships no general topological sort. Header-only, std-only.

**Types:** `TopoResult`

**Functions:**

- `inline TopoResult kahn(const std::vector<std::vector<int>>& successors)`
- `inline TopoResult topologicalSort(int nodeCount, const std::vector<std::pair<int, int>>& edges)`
- `inline TopoResult topologicalSort(const std::vector<std::vector<int>>& successors)`
- `inline bool hasCycle(int nodeCount, const std::vector<std::pair<int, int>>& edges)`

### `Trie`
<sub>`engine/include/maz/core/Trie.hpp`</sub>

maz::core::Trie — a prefix tree over strings: stores a set of words so that "is this exact word present?", "is any word here starting with this prefix?", and "give me every word under this prefix (sorted)" are all answered in time proportional to the query length, independent of how many words are stored. The natural structure behind developer-console / chat command autocomplete, dictionary word validation (spelling, word games), profanity/keyword filtering, and prefix-indexed lookups — none of which Godot provides a primitive for. Each node caches a subtree word count, so prefix existence and counts stay O(len) even after erases. Header-only, std-only; children are kept in a std::map so traversals emit words in lexicographic order.

**Types:** `Trie`

### `Utf16`
<sub>`engine/include/maz/core/Utf16.hpp`</sub>

maz::core UTF-16 <-> code point <-> UTF-8 conversion, the companion to Utf8.hpp. UTF-16 is the native text encoding of the Windows API (wide-char file paths, the clipboard, native file dialogs and message boxes) and of Java/JavaScript/.NET strings, so an engine that talks to those platforms or imports data from them needs to move between UTF-8 (its internal/on-disk form) and UTF-16. The subtlety UTF-16 adds over UTF-8 is SURROGATE PAIRS: code points above U+FFFF (emoji, many CJK extensions, historic scripts) are stored as TWO 16-bit units — a high surrogate (0xD800..0xDBFF) followed by a low surrogate (0xDC00..0xDFFF) — and getting the split/join arithmetic right is exactly where naive code breaks. This handles it, and mirrors Utf8.hpp's forgiving policy: any invalid scalar value (a lone surrogate, a value above U+10FFFF) becomes U+FFFD, the replacement character, rather than corrupting the stream. Header-only, std-only, deterministic. Godot exposes wide-char conversion only through its opaque String; this is the standalone codec.

**Functions:**

- `inline std::u16string utf16Encode(const std::u32string& cps)`
- `inline std::u32string utf16Decode(const std::u16string& units)`
- `inline std::u16string utf8ToUtf16(const std::string& utf8)`
- `inline std::string utf16ToUtf8(const std::u16string& utf16)`
- `inline std::size_t utf16Length(const std::u32string& cps)`

### `Utf8`
<sub>`engine/include/maz/core/Utf8.hpp`</sub>

maz::core UTF-8 <-> code-point conversion. Godot's String is fundamentally a sequence of Unicode CODE POINTS (stored as UTF-32), with length() counting code points and to_utf8_buffer/parse_utf8 converting to/from UTF-8 bytes. This is the core layer that gives Maz the same code-point view of text: encode a code point (or a whole u32string) to UTF-8, decode UTF-8 to code points, and count code points in a UTF-8 string (String.length semantics — NOT the raw byte count). Malformed bytes and out-of-range/surrogate code points decode/encode as U+FFFD (the replacement character), so the functions never throw and always make progress. Header-only, pure, deterministic; unit-tested by round-trip and against known byte sequences.

**Functions:**

- `inline std::string utf8EncodeChar(char32_t cp)`
- `inline std::string utf8Encode(const std::u32string& cps)`
- `inline std::u32string utf8Decode(const std::string& s)`
- `inline std::size_t utf8Length(const std::string& s)`

### `Uuid`
<sub>`engine/include/maz/core/Uuid.hpp`</sub>

maz::core UUID (v4) — generate RFC 4122 version-4 (random) universally-unique identifiers for entity IDs, save files, network sessions, asset GUIDs, and analytics events. `makeUuidV4` fills 128 bits from any engine RNG that exposes `uint32_t next()` (e.g. `core::Pcg32`) and stamps the version + variant bits; `Uuid::toString` renders the canonical lowercase `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx` form; `isValidUuid` validates that layout. Because the randomness comes from the caller's seeded RNG, generation is fully deterministic and unit-testable. Godot has no UUID type, so this is a beyond-Godot utility. Header-only, std-only.

**Types:** `Uuid`

**Functions:**

- `inline Uuid makeUuidV4(Rng& rng)`
- `inline std::string uuidV4String(Rng& rng)`
- `inline bool isValidUuid(const std::string& s)`

### `ValueNoise`
<sub>`engine/include/maz/core/ValueNoise.hpp`</sub>

maz::core::ValueNoise — classic *value* noise, the sibling of the Perlin gradient noise in core/Noise.hpp. Where Perlin interpolates random *gradients* attached to lattice points, value noise interpolates random *values* stored at the lattice points themselves. It is cheaper, has a slightly blockier/rounder character (no zero-crossings forced through the lattice), and is the classic ingredient for clouds, soft terrain, and cheap organic textures. This mirrors Godot's FastNoiseLite TYPE_VALUE (quintic-smoothstep interpolation) and TYPE_VALUE_CUBIC (Catmull-Rom bicubic interpolation) noise types, which the engine did not previously have in any form.  A self-contained integer hash maps each (ix, iy, seed) lattice cell to a reproducible value in [-1, 1], so the same seed always yields the same field with no shared state. value2 is bilinear with a quintic fade and stays strictly within [-1, 1]; value2Cubic is bicubic (Catmull-Rom) and is smoother but may overshoot the input range slightly, as cubic interpolating splines do. Both are *interpolating*: at integer lattice coordinates they return the stored lattice value exactly. fbm2 layers octaves of value2 into fractal Brownian motion, normalized back to ~[-1, 1]. Header-only, std-only.

**Types:** `ValueNoise`

### `Variant`
<sub>`engine/include/maz/core/Variant.hpp`</sub>

maz::core Variant — Godot's Variant: one value that can hold any of the common gameplay types and convert between them the way Godot's dynamic typing does. Data-driven configs, script bridges, serialized properties, and generic containers all want a single "any" value with a known type tag and forgiving getters. This holds the everyday subset: Nil / Bool / Int (64-bit) / Float (double) / String / Vector2 / Vector3, with Godot-style coercion (asInt/asFloat/asBool), truthiness (booleanize), value equality (numeric types compare across Int/Float/Bool), and stringify. Pure, header-only, unit-tested. (Dictionary/Array and the full type zoo are out of scope here.)

**Types:** `Variant`

### `VariantContainerText`
<sub>`engine/include/maz/core/VariantContainerText.hpp`</sub>

maz::core Array/Dictionary text serialization — the container half of Godot's var_to_str / str_to_var. Arrays serialize as `[a, b, c]` and Dictionaries as `{ "key": value, ... }` (keys in insertion order, matching Godot's ordered dictionaries). Elements are the scalar Variant set (M293); nested containers are out of scope (the containers hold scalar Variants). The parser is quote- and paren-aware so commas inside quoted strings and inside `Vector2(x, y)` don't split elements. Deterministic, header-only; unit-tested by round-trip. (Whitespace may differ from Godot's exact bytes; the guarantee is strToArray(arrayToStr(a)) == a and likewise for dictionaries.)

**Functions:**

- `inline std::string arrayToStr(const Array& a)`
- `inline std::string dictToStr(const Dictionary& d)`
- `inline std::vector<std::string> splitTopLevelCommas(const std::string& s)`
- `inline std::size_t findTopLevelColon(const std::string& s)`
- `inline std::optional<Array> strToArray(const std::string& text)`
- `inline std::optional<Dictionary> strToDict(const std::string& text)`

### `VariantContainers`
<sub>`engine/include/maz/core/VariantContainers.hpp`</sub>

maz::core Array / Dictionary — Godot's container Variants. Array is an ordered, index-addressed list of Variants (append/insert/remove/find/slice/reverse); Dictionary is an ORDERED string->Variant map (Godot dictionaries preserve insertion order) with has/get/set/erase/keys/values/merge. These are the data-driven backbone Godot uses everywhere: config trees, save games, script args, JSON-shaped data. Header-only, deterministic, unit-tested. (Elements are the M293 Variant subset; keys are strings — Godot's arbitrary-Variant keys and nested container Variants are out of scope here.)

**Types:** `Array`, `Dictionary`

### `VariantText`
<sub>`engine/include/maz/core/VariantText.hpp`</sub>

maz::core Variant text serialization — Godot's var_to_str / str_to_var: a round-trippable text encoding of a Variant (the form Godot writes into .tres/.tscn and returns from var_to_str). Unlike Variant::stringify() (the human-facing str() form), this keeps enough type information to parse the value straight back: strings are quoted and escaped, floats always carry a decimal marker so they don't read back as ints, and vectors use Godot's `Vector2(x, y)` / `Vector3(x, y, z)` constructor syntax. Covers the Variant type set (Nil/Bool/Int/Float/String/Vector2/Vector3). Deterministic, header-only; unit-tested by round-trip. (Whitespace/precision may differ from Godot's exact bytes; the guarantee is that strToVar(varToStr(v)) == v.)

**Functions:**

- `inline std::string shortestG(double v)`
- `inline std::string floatToken(double v)`
- `inline std::string varToStr(const Variant& v)`
- `inline bool parseComponents(const std::string& s, std::size_t prefixLen, int n, float* out)`
- `inline std::optional<Variant> strToVar(const std::string& text)`

### `Version`
<sub>`engine/include/maz/core/Version.hpp`</sub>

**Types:** `Version`

**Functions:**

- `inline Version engineVersion()`

### `WeightedReservoir`
<sub>`engine/include/maz/core/WeightedReservoir.hpp`</sub>

maz::core::WeightedReservoir — select k items from a STREAM of weighted items in a single pass and O(k) memory, where each item's chance of being kept is proportional to its WEIGHT (the Efraimidis-Spirakis "A-Res" algorithm). This is the missing middle between the engine's two sampling tools: AliasTable does a weighted pick from a KNOWN, in-memory set, and ReservoirSampler picks k from a stream but treats every item EQUALLY. WeightedReservoir does both at once — stream in candidates you cannot all hold, each tagged with a weight, and keep k chosen in proportion to those weights. The natural tool for drawing N loot items from a generated pile weighted by rarity, sampling spawn points weighted by desirability, or keeping importance-weighted telemetry without unbounded memory. The trick: for an item of weight w draw a uniform u in (0,1) and give it key = u^(1/w); keep the k items with the largest keys (a size-k min-heap). Keys are compared in log space (log(u)/w) for numerical stability. Deterministic via an embedded splitmix64. Header-only, std-only. Godot ships no weighted reservoir sampler.

**Types:** `WeightedReservoir`

### `WorleyNoise`
<sub>`engine/include/maz/core/WorleyNoise.hpp`</sub>

maz::core::WorleyNoise — cellular / "Worley" noise: scatter one jittered feature point per unit grid cell, then for any sample point return the distance to the nearest feature point (F1) and the second-nearest (F2). F1 gives a field of rounded cell blobs (stone, scales, cracked mud, water caustics, bubbles); F2 - F1 traces the ridges BETWEEN cells (crack/vein networks, Voronoi edges). This is the scalar-texture cousin of the engine's geometric Voronoi diagram and complements the Perlin `Noise` (M85), which cannot make cellular patterns. Deterministic from a seed, tileable-free, header-only, std-only. (Reaches parity with Godot's FastNoiseLite cellular mode, which Maz's own Noise module previously lacked.)

**Types:** `WorleyNoise`


<a name="math"></a>
## Math — vectors, matrices, transforms, curves, geometry

### `AhrsFilter`
<sub>`engine/include/maz/math/AhrsFilter.hpp`</sub>

maz::math AHRS attitude filter — fuse a gyroscope and an accelerometer into a drift-corrected orientation quaternion (Madgwick's gradient-descent IMU filter, 2010). A raw gyro gives smooth short-term rotation but its integrated angle drifts without bound; a raw accelerometer gives an absolute "which way is down" but is noisy and useless while the device is being shaken. This filter blends them: the gyro drives the estimate forward each step and a single gradient step toward the measured gravity direction bleeds off the drift.  This is the standard sensor-fusion behind motion controls, phone/tablet tilt input, VR/AR controller tracking, and any "point the device and the game reacts" mechanic. Godot exposes raw Input.get_gyroscope()/get_accelerometer() but gives you no fusion — you get the noisy sensors and have to build this yourself. Header-only, std-only, deterministic, allocation-free (fine for a per-frame update).  Convention (Madgwick's): the accelerometer, at rest, reads the reaction to gravity — pointing UP, i.e. world +Z expressed in the body frame; gyro is body-frame angular velocity in rad/s. With only a gyro and an accelerometer, tilt (roll & pitch relative to gravity) is observable and corrected, but heading (yaw about the gravity axis) is unobservable and comes from gyro integration alone — add a magnetometer (a MARG filter) to reference heading. The quaternion maps body -> world.

**Types:** `MadgwickFilter`

**Functions:**

- `inline vec3 gravityDirectionBody(const quat& q)`

### `AlphaShape`
<sub>`engine/include/maz/math/AlphaShape.hpp`</sub>

maz::math alpha shapes / concave hull — the "shrink-wrap" outline of a scattered 2D point set. A convex hull is the tightest CONVEX rubber band around the points; it can never dip into a bay or wrap around a C-shape. The alpha shape does: it keeps only the Delaunay triangles small enough to fit a disc of radius `alpha`, so any gap wider than ~2*alpha is left OUTSIDE the shape — carving out concavities, notches and holes. Sweep alpha from large to small and the outline morphs from the convex hull down to the bare points. This is the standard tool for turning a cloud of samples (a scanned blob, a splatter of hit points, a territory of unit positions, a lasso selection) into a real polygon you can fill, collide, or path around. `alphaShapeEdges` returns the boundary edges; `concaveHull` walks the outer boundary into an ordered CCW polygon. Godot ships convex hulls only, so the concave case is a beyond-Godot geometry utility. Header-only, std-only, deterministic; builds on triangulateDelaunay.

**Types:** `AlphaEdge`

**Functions:**

- `inline float alphaCircumRadius(const vec2& A, const vec2& B, const vec2& C)`
- `inline std::uint64_t alphaEdgeKey(std::uint32_t lo, std::uint32_t hi)`
- `inline std::vector<AlphaEdge> alphaShapeEdges(const std::vector<vec2>& points, float alpha)`
- `inline std::vector<std::uint32_t> concaveHull(const std::vector<vec2>& points, float alpha)`

### `ArcLength`
<sub>`engine/include/maz/math/ArcLength.hpp`</sub>

maz::math arc-length reparameterization for curves — the "move at constant speed along a path" tool. A spline or Bézier is naturally parameterized by u in [0,1], but equal steps in u do NOT cover equal distance: an object animated by raw u races along straight sections and crawls through tight curves. Arc-length reparameterization fixes this: sample the curve densely, build a cumulative chord-length table, then map DISTANCE back to the curve parameter (and vice versa). Feed it the points from CatmullRomSpline::tessellate or Curve2D and you can drive a camera/enemy/projectile along the path at a uniform speed, or place N evenly-spaced points along it. Pure vec2 math over a precomputed table — exactly unit-testable (the distance<->parameter round-trip is exact, and a straight curve maps distance directly to parameter).

**Types:** `ArcLengthTable`

### `BSpline`
<sub>`engine/include/maz/math/BSpline.hpp`</sub>

maz::math uniform cubic B-spline — a smooth curve that APPROXIMATES a list of control points.  The engine already has cubic Bezier (Curve2D) and centripetal Catmull-Rom (CatmullRomSpline). Those INTERPOLATE — the curve passes through every control point — which is what you want for waypoints. The cubic B-spline is the other classic: it does NOT pass through its control points, it is pulled toward them, and in exchange it is C2 continuous (curvature is continuous, not just the tangent, so there are no visible kinks in acceleration) and provably stays inside the convex hull of the four local control points (it can never overshoot). That combination — smoothest possible motion, no overshoot — is why B-splines are the standard for camera dollies, easing rails, and procedural geometry, and why they are the foundation NURBS is built on.  Each segment i is a convex-combination blend of four consecutive control points P0..P3 with the uniform cubic basis (all weights >= 0 on [0,1], summing to 1 — the partition of unity). Two chain forms are provided: OPEN (n>=4 control points give n-3 segments; the curve starts near P1 and ends near P[n-2]) and CLOSED (n>=3 control points wrap into n segments forming a seamless C2 loop). Pure vec2 math, no per-eval allocation — deterministic and exactly unit-testable (partition of unity, the knot-point average (P0+4P1+P2)/6, the central-difference tangent (P2-P0)/2, linear precision, and convex-hull containment all hold on the dot).

**Functions:**

- `inline vec2 bsplinePoint(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t)`
- `inline vec2 bsplineTangent(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t)`
- `inline int bsplineSegmentCount(std::size_t n, bool closed)`
- `inline vec2 bsplineEval(const std::vector<vec2>& ctrl, float u, bool closed = false)`
- `inline vec2 bsplineEvalTangent(const std::vector<vec2>& ctrl, float u, bool closed = false)`

### `Ballistics`
<sub>`engine/include/maz/math/Ballistics.hpp`</sub>

maz::math ballistics — the two aiming problems every action game needs and neither GLM nor Godot ships: 1. LAUNCH ANGLE: given a projectile speed and gravity, at what angle do you fire to hit a target? Under gravity a reachable target has TWO solutions — a flat "direct" shot and a lobbed "mortar" arc — and beyond the range limit, none. This is artillery, grenades, basketball AI, catapults, trajectory previews. 2. INTERCEPT LEAD: where do you aim a constant-speed projectile to hit a target moving at constant velocity? Solve the quadratic in time-to-impact; this is turret lead, homing-missile launch, "aim ahead of the runner". Both are exact closed forms, verified by independently simulating the shot and confirming it lands on the target. The 3D helpers assume +Y is up (the engine convention) and gravity pulls along −Y. Header-only, std-only, deterministic.

**Types:** `LaunchAngles`, `LaunchVelocities`, `Intercept`

**Functions:**

- `inline LaunchAngles ballisticAngles(float speed, float x, float y, float gravity)`
- `inline LaunchVelocities ballisticVelocities(const vec3& from, const vec3& to, float speed, float gravity)`
- `inline Intercept interceptLead(const vec3& shooter, float projectileSpeed, const vec3& targetPos,`

### `BezierIntersect`
<sub>`engine/include/maz/math/BezierIntersect.hpp`</sub>

maz::math cubic Bézier curve–curve intersection — find the points where two cubic Bézier curves cross. The engine has cubic Bézier paths (Curve2D) and easing, but no way to ask "where do these two curves meet?" — the query you need for path/obstacle collision, self-intersection and trim/clip checks in a vector tool, spline-vs-spline hit testing, and gesture/stroke analysis. Godot's Curve2D exposes no such query. This uses robust recursive DE CASTELJAU SUBDIVISION with convex-hull (control-point bounding-box) culling: two curve pieces can only cross where their control hulls overlap, so the pair is recursively split until each piece is smaller than a tolerance, and the surviving overlaps are reported as crossing points (de-duplicated). It finds all TRANSVERSAL crossings; curves that overlap along a shared arc are a degenerate case left out of scope. Pure vec2 math, deterministic, header-only.

**Types:** `CubicBezier2`

**Functions:**

- `inline vec2 cubicBezierEval(const CubicBezier2& c, float u)`
- `inline void bezBounds(const CubicBezier2& c, vec2& lo, vec2& hi)`
- `inline bool boxesOverlap(const vec2& lo0, const vec2& hi0, const vec2& lo1, const vec2& hi1, float eps)`
- `inline void bezSplit(const CubicBezier2& c, CubicBezier2& left, CubicBezier2& right)`
- `inline float bezBoxDiag(const CubicBezier2& c)`
- `inline void bezRecurse(const CubicBezier2& a, const CubicBezier2& b, float tol, int depth,`
- `inline std::vector<vec2> bezierIntersections(const CubicBezier2& a, const CubicBezier2& b,`

### `BezierSurface`
<sub>`engine/include/maz/math/BezierSurface.hpp`</sub>

maz::math bicubic Bézier surface patch — the tensor-product cubic Bézier, the smooth free-form surface a 4×4 grid of control points sculpts (the primitive the Utah teapot, car bodies, and font/vector surfaces are built from). Unlike the Coons patch (CoonsPatch.hpp, which fills in FOUR boundary curves), a Bézier patch is shaped by an interior control net that pushes/pulls the surface like clay — the standard way to author organic hulls, terrain sculpts, cloth rest shapes and deformation targets. The 16 control points are row-major, index = i*4 + j, so B_i(u)·B_j(v)·P[i*4+j]; the patch interpolates its four CORNER control points and its four boundary curves are the cubic Béziers of the edge control points. Godot has no Bézier-surface primitive. Includes the analytic surface normal (from the u/v tangents). Header-only, std-only, deterministic.

**Functions:**

- `inline std::array<float, 4> basis(float t)`
- `inline std::array<float, 4> dbasis(float t)`
- `inline vec3 bezierSurfacePoint(const std::array<vec3, 16>& cp, float u, float v)`
- `inline void bezierSurfaceTangents(const std::array<vec3, 16>& cp, float u, float v, vec3& du, vec3& dv)`
- `inline vec3 bezierSurfaceNormal(const std::array<vec3, 16>& cp, float u, float v)`
- `inline std::vector<vec3> bezierSurfaceGrid(const std::array<vec3, 16>& cp, int nu, int nv)`

### `BoundingSphere`
<sub>`engine/include/maz/math/BoundingSphere.hpp`</sub>

A 3D sphere (centre + radius).

**Types:** `Sphere`, `DSphere`

**Functions:**

- `inline bool solve3(const double m[3][3], const double b[3], double x[3])`
- `inline DSphere sphere1(const vec3& a)`
- `inline DSphere sphere2(const vec3& a, const vec3& b)`
- `inline DSphere sphere3(const vec3& a, const vec3& b, const vec3& c)`
- `inline DSphere sphere4(const vec3& a, const vec3& b, const vec3& c, const vec3& d)`
- `inline Sphere boundingSphere(const std::vector<vec3>& points)`

### `Catenary`
<sub>`engine/include/maz/math/Catenary.hpp`</sub>

maz::math catenary — the shape a uniform flexible chain, rope, cable or wire takes when hung between two points under gravity: y = a·cosh(x/a). It is NOT a parabola (a common mistake); the difference is visible on rope bridges, power lines, hanging chains, tent ridges, mooring cables and swinging vines. Given two anchor points and a rope LENGTH longer than the straight-line gap, this solves for the unique catenary that passes through both anchors with exactly that much rope, then samples it — everything a game needs to draw a sagging rope/cable procedurally. Godot has no catenary helper. The solve is a 1-D root-find on the catenary parameter `a`; the sag grows as the rope lengthens. Header-only, std-only, deterministic.

**Types:** `Catenary`

**Functions:**

- `inline float catenaryHeight(const Catenary& cat, float x)`
- `inline float catenaryArcLength(const Catenary& cat, float xa, float xb)`
- `inline bool solveCatenary(const vec2& p1, const vec2& p2, float ropeLength, Catenary& out)`
- `inline std::vector<vec2> catenaryPolyline(const vec2& p1, const vec2& p2, float ropeLength, int samples)`

### `CatmullRomSpline`
<sub>`engine/include/maz/math/CatmullRomSpline.hpp`</sub>

maz::math centripetal Catmull-Rom spline — a smooth curve that passes THROUGH a list of control points, the standard tool for camera rails, roads/rivers, and enemy patrol paths laid out as waypoints. The engine already has a single uniform Catmull-Rom segment (VectorOps cubicInterpolate), but uniform parameterization famously produces cusps and self-intersecting loops when waypoints are unevenly spaced or turn sharply. The CENTRIPETAL variant (alpha = 0.5), from Yuksel et al., spaces the knots by the square-root of the distance between points, which provably removes those cusps and loops while still interpolating every control point exactly. This is the chain-of-segments form with selectable parameterization (0 = uniform, 0.5 = centripetal, 1 = chordal). Pure vec2 math, no allocation per eval — deterministic and exactly unit-testable (it hits each control point on the dot).

**Types:** `CatmullRomSpline`

**Functions:**

- `inline vec2 knotLerp(const vec2& a, const vec2& b, float ta, float tb, float t)`

### `CircularMean`
<sub>`engine/include/maz/math/CircularMean.hpp`</sub>

maz::math circular statistics — the CORRECT way to average and measure the spread of angles. You cannot arithmetically average angles: the mean of 350° and 10° is NOT 180°, it is 0°, because angles wrap. The circular mean fixes this by treating each angle as a unit vector, averaging the vectors, and taking the resulting direction (atan2 of the summed sin/cos). Games need this constantly: averaging the FACING of a flock or squad, a smoothed heading from noisy inputs, wind or current direction, wave phases, or a gyroscope/compass reading. The paired "resultant length" R in [0,1] measures how CONCENTRATED the angles are (1 = all identical, 0 = evenly spread with no meaningful mean), giving circular variance (1-R) and a circular standard deviation. Distinct from the engine's lerpAngle / shortestAngle (which interpolate a PAIR): this reduces a whole SET of angles. Radians in, radians out (result in (-pi, pi]). Header-only, std-only, deterministic. Godot has no circular-statistics helper.

**Functions:**

- `inline float resultantLength(const std::vector<float>& angles)`
- `inline float circularMean(const std::vector<float>& angles)`
- `inline float circularMeanWeighted(const std::vector<float>& angles, const std::vector<float>& weights)`
- `inline float circularVariance(const std::vector<float>& angles)`
- `inline float circularStdDev(const std::vector<float>& angles)`

### `Circumsphere`
<sub>`engine/include/maz/math/Circumsphere.hpp`</sub>

maz::math circumsphere of a tetrahedron — the unique sphere passing through four 3D points (its centre is equidistant from all four). This is the 3D companion to a triangle's circumcircle, and the core predicate of 3D Delaunay tetrahedralisation: a tetralisation is Delaunay iff no vertex lies inside any tetrahedron's circumsphere (the "in-sphere test"). Also used for bounding spheres of simplices, mesh-quality metrics (radius-edge ratio), and sphere-fitting. The engine has a Ritter-style bounding sphere (BoundingSphere.hpp) but no exact sphere-through-4-points; Godot has neither. Returns the centre, radius, and a validity flag (false when the four points are coplanar — no finite sphere). Header-only, std-only, deterministic.

**Types:** `Circumsphere`

**Functions:**

- `inline Circumsphere circumsphere(const vec3& a, const vec3& b, const vec3& c, const vec3& d)`
- `inline bool insideCircumsphere(const vec3& a, const vec3& b, const vec3& c, const vec3& d, const vec3& p)`

### `ClosestPointCurve`
<sub>`engine/include/maz/math/ClosestPointCurve.hpp`</sub>

maz::math closest-point / projection onto a path — given any point in space, find the nearest point on a polyline or on a smooth Bezier curve, plus HOW FAR ALONG the path that nearest point sits. This is the query behind snapping a dragged object to a spline, measuring an agent's progress along a race line or rail, keeping a follower glued to a track, computing a car's cross-track error, or finding the distance from anything to a route. Godot's Curve2D exposes sampling and baking but no "project this point onto the curve" call, so gameplay code has to roll it by hand. This does it robustly: exact per-segment projection for polylines, and a dense-sample-plus-local-refine search for curves. Header-only, std-only, deterministic.

**Types:** `SegmentProjection`, `CurveProjection`

**Functions:**

- `inline SegmentProjection closestPointOnSegment(const vec2& a, const vec2& b, const vec2& p)`
- `inline CurveProjection closestPointOnPolyline(const std::vector<vec2>& pts, const vec2& p)`
- `inline CurveProjection closestPointOnCurve(const Curve2D& curve, const vec2& p, int samplesPerSegment = 16)`

### `ClosestPointObb`
<sub>`engine/include/maz/math/ClosestPointObb.hpp`</sub>

maz::math closest point on an oriented bounding box (OBB) — given a point and an arbitrarily-rotated box, return the point on (or in) the box nearest to it, and the distance. The engine's Obb (Geometry3D.hpp) already does contains / box-vs-box SAT / AABB bounds, but not this proximity query, which is what you need for sphere-vs-OBB collision (overlap iff distance ≤ radius, with the closest point as the contact), snapping a probe/agent to the outside of a crate, distance-based culling and trigger volumes, and nearest- surface queries in an editor. It works by expressing the point in the box's local frame, clamping each local coordinate to the box's half-extents, and mapping back to world — so a point inside the box returns itself (distance 0) and a point outside returns the nearest surface point. Godot exposes no such helper. Pure vec3 math, deterministic, header-only.

**Functions:**

- `inline vec3 closestPointOnObb(const vec3& p, const Obb& box)`
- `inline float distanceToObb(const vec3& p, const Obb& box)`
- `inline bool sphereIntersectsObb(const vec3& center, float radius, const Obb& box, vec3& contact)`

### `Clothoid`
<sub>`engine/include/maz/math/Clothoid.hpp`</sub>

maz::math clothoid / Euler spiral — the transition curve whose CURVATURE varies LINEARLY with arc length, κ(s) = κ0 + rate·s. It is the shape real roads, railways and racetracks use to connect a straight to a circular corner: because curvature ramps smoothly instead of jumping, a body following it feels no sudden sideways lurch (continuous lateral acceleration). Use it for smooth road/track geometry, camera and motion rails that must not snap between straight and curved sections, and procedural spiral shapes. The engine's Bézier/B-spline curves control position but not curvature directly; the clothoid is the curvature-first primitive, and Godot has no equivalent. Evaluated by integrating the unit-speed tangent θ(s) = θ0 + κ0·s + ½·rate·s² (Simpson's rule); with rate = 0 it degenerates exactly to a straight line (κ0 = 0) or a circular arc (κ0 ≠ 0). Header-only, std-only, deterministic.

**Functions:**

- `inline vec2 clothoidPoint(const vec2& p0, float theta0, float kappa0, float rate, float s)`
- `inline float clothoidHeading(float theta0, float kappa0, float rate, float s)`
- `inline float clothoidCurvature(float kappa0, float rate, float s)`
- `inline std::vector<vec2> clothoidPolyline(const vec2& p0, float theta0, float kappa0, float rate,`

### `ColorLab`
<sub>`engine/include/maz/math/ColorLab.hpp`</sub>

maz::math CIE color science — convert LINEAR RGB (the engine's working colour space) to CIE XYZ and then to CIELAB (L*a*b*), plus the CIE76 perceptual colour difference (Delta-E). CIELAB is the classic PERCEPTUALLY-UNIFORM colour space: equal numeric steps look like equal visual steps, so it is the right space for measuring "how different do these two colours look?" — palette matching, colour quantisation / nearest-swatch, gradient generation, and accessibility (perceptual contrast). L* is lightness 0..100, a* is green(−)↔red(+), b* is blue(−)↔yellow(+). This complements the engine's Oklab (ColorOps.hpp) with the long-standing CIE standard. Godot exposes no Lab/Delta-E. Uses the sRGB/Rec.709 primaries at the D65 white point. Header-only, std-only, deterministic.

**Functions:**

- `inline vec3 linearRgbToXyz(const vec3& c)`
- `inline vec3 xyzToLinearRgb(const vec3& c)`
- `inline float f(float t)`
- `inline float fInv(float t)`
- `inline vec3 xyzToLab(const vec3& xyz)`
- `inline vec3 labToXyz(const vec3& lab)`
- `inline vec3 linearRgbToLab(const vec3& c)`
- `inline vec3 labToLinearRgb(const vec3& lab)`
- `inline float deltaE76(const vec3& labA, const vec3& labB)`

### `CompensatedSum`
<sub>`engine/include/maz/math/CompensatedSum.hpp`</sub>

maz::math compensated summation — add up many floating-point numbers WITHOUT the rounding drift that plain left-to-right addition accumulates. When a running total grows large, adding a small value loses low bits to rounding; over thousands of adds (mixing audio samples, accumulating forces or particle contributions, summing analytics/metrics, integrating over a frame) the error piles up. Kahan's algorithm carries a separate "compensation" term that captures the lost low bits and feeds them back on the next add, and Neumaier's refinement also handles the case where the next value is LARGER than the running sum — together they give a result close to what you'd get in much higher precision, at ~4 extra flops per element. Use KahanSum as a drop-in accumulator, or compensatedSum for a one-shot total. Header-only, std-only, deterministic. (Godot has no compensated-summation utility.)

**Types:** `KahanSum`

**Functions:**

- `inline T compensatedSum(const std::vector<T>& xs)`

### `CoonsPatch`
<sub>`engine/include/maz/math/CoonsPatch.hpp`</sub>

maz::math bilinearly-blended Coons patch — a smooth surface that fills in the interior given only its FOUR boundary curves. You describe the edges (the two u-edges P(·,0) and P(·,1), the two v-edges P(0,·) and P(1,·)) and the patch interpolates a natural surface between them, reproducing each boundary curve exactly. It is the standard way to build procedural surfaces from edge curves: lofted track/road ribbons, terrain patches stitched to their neighbours' edges, cloth/sail panels, tent canopies and swept shapes. Godot has no surface-from-curves primitive. The boundary curves are passed as callables `f(t) -> vec3` on [0,1] that must agree at the shared corners. Header-only, std-only, deterministic.

**Functions:**

- `inline vec3 coonsPatchPoint(C0&& c0, C1&& c1, D0&& d0, D1&& d1, float u, float v)`
- `inline std::vector<vec3> coonsPatchGrid(C0&& c0, C1&& c1, D0&& d0, D1&& d1, int nu, int nv)`

### `CubicSpline`
<sub>`engine/include/maz/math/CubicSpline.hpp`</sub>

maz::math natural cubic spline — the classic globally-smooth interpolating spline: given a set of knots (x_i, y_i) with strictly increasing x, it builds the unique piecewise-cubic curve that passes exactly through every knot and is C2-continuous (continuous value, slope, AND curvature) everywhere, with the "natural" boundary condition (zero curvature at both ends). This differs from the engine's other curves: Curve2D is a Bezier (control points, not interpolation), anim::Curve is a keyframe track, and Catmull-Rom (in VectorOps) is only C1 and local. A natural cubic spline is the right tool for a smooth camera dolly through waypoints, a terrain cross-section through samples, or any "draw the smoothest curve through these points" need. Second derivatives are solved with the Thomas tridiagonal algorithm (O(n)). Header-only, std-only. Build a 2D/3D path by splining each component against a shared parameter.

**Types:** `CubicSpline`

### `Curve2D`
<sub>`engine/include/maz/math/Curve2D.hpp`</sub>

Cubic Bézier path — Godot's Curve2D / the spline a Path2D holds and a PathFollow2D walks. Maz had easing curves (anim) for scalar interpolation, but no *spatial* path: an authored smooth curve through a set of points that something can travel along at constant speed. That is what Curve2D provides. Each point carries a position plus `in`/`out` control handles (offsets relative to the point, exactly like Godot's Curve2D), and consecutive points are joined by a cubic Bézier. `sample`/`tangent` evaluate the geometric curve; `bake` walks it and lays down points spaced evenly by ARC LENGTH, so `sampleBaked(distance)` moves along the path at uniform speed (naive Bézier `t` bunches up where the curve bends). Header-only, math-only (no renderer), so it unit-tests headlessly; the app draws the curve, its handles, and the constant-speed points.

**Types:** `CurvePoint2D`, `Curve2D`

**Functions:**

- `inline vec2 cubicBezier(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float t)`

### `Curve3D`
<sub>`engine/include/maz/math/Curve3D.hpp`</sub>

3D cubic-Bezier path with arc-length baking — Godot's Curve3D, the 3D twin of Curve2D (M142). Each point carries a position plus in/out control handles (relative offsets, like Godot); consecutive points join by a cubic Bezier. `sample`/`tangent` evaluate the geometric curve; `bake` lays down points spaced evenly by ARC LENGTH so `sampleBaked(distance)` moves along the path at CONSTANT speed (naive Bezier `t` does not). Backs Path3D / PathFollow3D, camera rails, and 3D spline motion. Header-only, deterministic, GPU-free — unit-tests exactly. (Curve3D's per-point tilt/up-vector for full PathFollow3D banking is not modelled here; PathFollow3D reports the forward tangent.)

**Types:** `CurvePoint3D`, `Curve3D`

**Functions:**

- `inline vec3 cubicBezier(const vec3& p0, const vec3& p1, const vec3& p2, const vec3& p3, float t)`

### `Delaunay`
<sub>`engine/include/maz/math/Delaunay.hpp`</sub>

maz::math Delaunay triangulation — Godot's Geometry2D.triangulate_delaunay. Given a set of 2D points it returns the Delaunay triangulation as a flat list of vertex indices (three per triangle, indexing the input). The Delaunay property (no point lies inside any triangle's circumcircle) yields the "roundest" triangles, which is what you want for terrain meshes, procedural tessellation, path graphs, and dual-Voronoi work. Implemented with Bowyer-Watson incremental insertion and a robust double-precision in-circle predicate. Pure, header-only, deterministic — unit-tests exactly.

**Types:** `DelTri`

**Functions:**

- `inline bool delInCircle(const vec2& a, const vec2& b, const vec2& c, const vec2& p)`
- `inline double delCross(const vec2& o, const vec2& a, const vec2& b)`
- `inline std::vector<std::uint32_t> triangulateDelaunay(const std::vector<vec2>& points)`

### `DistanceTransform`
<sub>`engine/include/maz/math/DistanceTransform.hpp`</sub>

maz::math exact Euclidean distance transform (Felzenszwalb & Huttenlocher, 2004).  Given a grid where some cells are "seeds", the distance transform fills EVERY cell with its exact Euclidean distance to the nearest seed, and (as a byproduct) which seed is nearest — a grid Voronoi labelling. This is the workhorse behind exact SDF baking, "how far is this tile from the nearest wall?" navigation clearance fields (spawn placement, corridor widths, influence maps), morphological grow/shrink, and grid Voronoi regions.  The engine's ui::Sdf builds glyph fields with dead reckoning, which is APPROXIMATE (accurate to under a texel, fine for fonts). This is the EXACT transform: the separable Felzenszwalb-Huttenlocher lower- envelope-of-parabolas algorithm, O(n) per row and per column, so the result equals a brute-force nearest-seed search to the last bit — which is exactly what the tests check. It also threads the argmin through both passes to recover each cell's nearest-seed coordinates. Header-only, deterministic, no allocation beyond the output + scratch.

**Types:** `DistanceField`

**Functions:**

- `inline void dt1d(const float* f, int n, float* d, int* arg, int* v, float* z)`
- `inline DistanceField distanceTransform(const std::vector<uint8_t>& seed, int w, int h)`

### `DualContour2D`
<sub>`engine/include/maz/math/DualContour2D.hpp`</sub>

maz::math 2D dual contouring — extract a contour line from a signed distance field (SDF) that PRESERVES SHARP CORNERS, unlike marching squares which bevels every corner into a chamfer. Marching squares can only put contour points on the midpoints of grid edges, so a square or a hard crease comes out rounded; dual contouring instead places ONE vertex inside each boundary cell at the point that best satisfies the surface NORMALS of that cell's edge crossings (a tiny per-cell least-squares "QEF" solve), so two edges meeting at 90 degrees produce a vertex sitting exactly on the corner. That is why it needs the field's gradient (Hermite data), which marching squares ignores — and why it can reproduce features MS can't. Turn an SDF (procedural shapes, boolean CSG, destructible terrain, brush masks) into a clean polygon outline with crisp features for collision, decals, or rendering. Reuses the dense linear solver for the QEF. Godot ships only rounded marching-squares-style meshing. Header-only, std-only, deterministic. `sdf` is any callable taking (float x, float y) and returning the field value (negative inside).

**Types:** `DcSegment`

**Functions:**

- `inline std::vector<DcSegment> dualContour2D(F&& sdf, int nx, int ny, float iso = 0.0f)`

### `DualQuaternion`
<sub>`engine/include/maz/math/DualQuaternion.hpp`</sub>

maz::math dual quaternions + dual-quaternion linear blending (DQS) — the skinning math that fixes the "candy-wrapper" collapse of linear-blend skinning. A unit dual quaternion represents a rigid motion (rotation + translation) with no scale/shear: the REAL part is the rotation quaternion, the DUAL part encodes the translation (dual = ½·t·real). Its point is BLENDING: averaging several bone transforms as dual quaternions and renormalizing yields another *rigid* transform, so a vertex weighted between a straight and a twisted bone keeps its volume instead of pinching toward the joint axis (which is exactly what linear-blend skinning gets wrong). This complements the engine's Skeleton/skinning (linear-blend) path. Pure quaternion algebra over floats — no GPU, no allocation — so it unit-tests exactly against hand-computed rigid motions.

**Types:** `Q4`, `DualQuaternion`

**Functions:**

- `inline Q4 quatMul(const Q4& a, const Q4& b)`
- `inline Q4 quatConj(const Q4& a)`
- `inline DualQuaternion blendDual(const DualQuaternion* dqs, const float* weights, int count)`
- `inline DualQuaternion blendDual(const std::vector<DualQuaternion>& dqs,`

### `DubinsPath`
<sub>`engine/include/maz/math/DubinsPath.hpp`</sub>

maz::math Dubins path — the SHORTEST path for a forward-only vehicle with a minimum turning radius, from a start pose (position + heading) to a goal pose. A Dubins car can drive straight or turn left/right at a fixed radius but never reverse; the optimum is always one of six primitive words — three arc/straight/arc (CSC: LSL, LSR, RSL, RSR) and two arc/arc/arc (CCC: RLR, LRL) — and the shortest valid one is the answer. This is the standard motion primitive for steering cars, boats, planes and any agent that "can't turn on a dime": planning a lane change, an approach curve, a patrol turn, or the reference path a pursuit controller tracks. Godot has no Dubins/curvature-constrained planner. Returns the chosen word, its three segment lengths, and a sampler that walks the path — following the returned controls lands exactly on the goal. Deterministic, header-only, std-only.

**Types:** `Pose2`, `DubinsPath`

**Functions:**

- `inline float mod2pi(float x)`
- `inline std::array<DubinsSeg, 3> wordSegs(DubinsWord w)`
- `inline bool solveWord(DubinsWord w, float alpha, float beta, float d, std::array<float, 3>& out)`
- `inline DubinsPath dubinsComputeWord(const Pose2& start, const Pose2& goal, float radius, DubinsWord w)`
- `inline DubinsPath dubinsShortestPath(const Pose2& start, const Pose2& goal, float radius)`
- `inline Pose2 dubinsSample(const DubinsPath& path, float s)`

### `EllipseDistance`
<sub>`engine/include/maz/math/EllipseDistance.hpp`</sub>

maz::math closest point on / distance to an axis-aligned ellipse — the robust query "what is the nearest point on the ellipse boundary to p, and how far is it?", plus the SIGNED distance (negative inside). The distance-to-an-ellipse is a genuinely hard little problem: unlike a circle there is no closed form, and the naive "project along the radius" is wrong everywhere except the axes. This uses Eberly's robust method — reduce to the first quadrant, then bisect a single monotone function for the foot of the perpendicular — which stays accurate even near the flat sides and the sharp ends where iterative solvers usually degrade. The engine has parametric ellipse points (Superellipse.hpp) but no distance/closest-point query; Godot has none either. Uses: snapping to elliptical orbits/tracks, elliptical soft-body/collision response, GUI hit-testing against ovals, and an exact elliptical signed-distance field. Header-only, std-only, deterministic.

**Functions:**

- `inline float getRoot(float r0, float z0, float z1, float g)`
- `inline float distancePointEllipseQ1(float e0, float e1, float y0, float y1, float& x0, float& x1)`
- `inline vec2 closestPointOnEllipse(float a, float b, const vec2& p)`
- `inline float distanceToEllipse(float a, float b, const vec2& p)`
- `inline float signedDistanceEllipse(float a, float b, const vec2& p)`

### `Epa`
<sub>`engine/include/maz/math/Epa.hpp`</sub>

maz::math penetration query — the companion to GJK distance that answers the OTHER half of convex-vs-convex collision: when two convex polygons OVERLAP, how deep, and which way do you push to separate them? GjkDistance.hpp reports the gap between shapes that are APART (and 0 when they touch); this takes over once they interpenetrate and returns the PENETRATION DEPTH and CONTACT NORMAL — the minimum translation vector (MTV) that pushes shape A just clear of shape B. That is exactly what a rigid-body solver needs to resolve a collision between two arbitrary convex shapes (not just the box/circle special cases SAT hand-codes). It works on the Minkowski difference A (-) B = { a - b }: the two shapes overlap iff that (convex) set contains the origin, and the shortest way out is the closest point on its boundary. In 2D the exact boundary is cheap — the convex hull of all pairwise vertex differences — so instead of the iterative EPA polytope expansion this computes the difference polygon directly and takes the closest edge to the origin, giving an EXACT depth and normal (no convergence tolerance). Pure vec2 math, deterministic, header-only. Godot exposes no such query. (Convex inputs assumed; any vertex order works.)

**Types:** `EpaResult`

**Functions:**

- `inline std::vector<vec2> convexHullCcw(std::vector<vec2> pts)`
- `inline EpaResult epaPenetration(const std::vector<vec2>& a, const std::vector<vec2>& b)`

### `EulerOrder`
<sub>`engine/include/maz/math/EulerOrder.hpp`</sub>

maz::math Euler-order conversions — Godot's Basis.from_euler / get_euler across ALL SIX rotation orders (Godot's EulerOrder / Node3D.rotation_order: XYZ, XZY, YXZ, YZX, ZXY, ZYX). The existing Quaternion type already covers Godot's default YXZ; this adds the other five so orientations authored in Godot with a non-default rotation order import and export identically.  A basis here is a 3x3 rotation matrix (GLM column-major, the same type Transform3D::basis uses). from_euler builds R as the ordered product of elementary axis rotations (matching Godot's source: e.g. XYZ = Rx*Ry*Rz), where each elementary rotation is the standard right-handed one. get_euler inverts that, returning angles in radians as a vec3 (x=rotation about X, y=about Y, z=about Z), with a gimbal-lock branch (middle axis at +/-90 deg) that still reconstructs the input basis. Header-only, pure, deterministic; unit-tested by round-tripping all six orders (including lock) and cross-checking YXZ against Quaternion::fromEuler.

**Functions:**

- `inline mat3 basisFromEuler(const vec3& e, EulerOrder order)`
- `inline vec3 basisGetEuler(const mat3& m, EulerOrder order)`

### `FitObb`
<sub>`engine/include/maz/math/FitObb.hpp`</sub>

**Functions:**

- `inline void jacobiEigen3(double a[3][3], double v[3][3], double d[3])`
- `inline Obb fitObb(const std::vector<vec3>& points)`

### `FixedAabb3`
<sub>`engine/include/maz/math/FixedAabb3.hpp`</sub>

maz::math::FixedAabb3 — Godot's AABB as DETERMINISTIC fixed-point: an axis-aligned box in core::Fixed 3D coordinates (position = min corner + size), the 3D sibling of FixedRect2 (M420) and the deterministic companion to the float AABB. Where the float AABB is right for rendering/culling, FixedAabb3 is right for the parts of a 3D game that must agree bit-for-bit across machines: lockstep-multiplayer broadphase, replay-exact overlap tests, deterministic 3D trigger volumes. Same half-open convention as FixedRect2/Rect2i (min inclusive, max exclusive), same API — hasPoint / intersects / intersection / merge / grow / expand / encloses / abs / clampPoint / center / volume. All arithmetic is integer fixed-point. Header-only, pure. Godot has no fixed-point box.

**Types:** `FixedAabb3`

### `FixedMath`
<sub>`engine/include/maz/math/FixedMath.hpp`</sub>

maz::math::FixedMath — the everyday DETERMINISTIC helpers for core::Fixed and FixedVec2: the clamp / lerp / move-toward / rotate operations gameplay leans on constantly, but done in pure integer fixed-point so they give bit-identical results on every machine (lockstep multiplayer, replays, cross-platform play). These are the fixed-point twins of Godot's float lerp/clamp/ move_toward and Vector2.rotated / from_angle / limit_length — none of which Godot offers in a deterministic form. Built on M415 Fixed (scalar), M416 FixedVec2 (vector) and M417 FixedTrig (integer CORDIC sin/cos). Header-only, pure, no floats on the runtime path.

**Functions:**

- `inline constexpr Fixed fixMin(Fixed a, Fixed b)`
- `inline constexpr Fixed fixMax(Fixed a, Fixed b)`
- `inline constexpr Fixed fixClamp(Fixed v, Fixed lo, Fixed hi)`
- `inline constexpr Fixed fixSign(Fixed v)`
- `inline Fixed fixLerp(Fixed a, Fixed b, Fixed t)`
- `inline Fixed fixMoveToward(Fixed from, Fixed to, Fixed delta)`
- `inline FixedVec2 fixLerp(FixedVec2 a, FixedVec2 b, Fixed t)`
- `inline FixedVec2 fixMoveToward(FixedVec2 from, FixedVec2 to, Fixed delta)`
- `inline FixedVec2 fixRotated(FixedVec2 v, Fixed angle)`
- `inline FixedVec2 fixFromAngle(Fixed angle, Fixed length = Fixed::one())`
- `inline FixedVec2 fixClampLength(FixedVec2 v, Fixed maxLength)`
- `inline Fixed fixAngle(FixedVec2 v)`
- _…and 2 more_

### `FixedQuat`
<sub>`engine/include/maz/math/FixedQuat.hpp`</sub>

maz::math::FixedQuat — a unit quaternion of core::Fixed (Q16.16) for DETERMINISTIC 3D rotation, the deterministic companion to the float Quaternion. 3D orientation can't be represented deterministically without it: this is what lets a lockstep-multiplayer or replay-exact 3D game turn, aim and spin things with bit-identical results on every machine. Built on FixedVec3 (M421) and the integer-CORDIC FixedTrig (M417), so the whole from-axis-angle → compose → rotate-a-vector path is pure integer math. Hamilton product, conjugate, normalize, and the fast vector-rotation formula (v + 2w(u×v) + 2u×(u×v)). Fixed-point precision means rotations are accurate to ~1e-2, plenty for gameplay. Header-only. Godot has no fixed-point quaternion. The rotation capstone of the deterministic-sim toolkit (M415-M422).

**Types:** `FixedQuat`

### `FixedRect2`
<sub>`engine/include/maz/math/FixedRect2.hpp`</sub>

maz::math::FixedRect2 — Godot's Rect2 as DETERMINISTIC fixed-point: an axis-aligned rectangle in core::Fixed coordinates (position = min corner + size), the deterministic companion to the float Rect2. Where Rect2 is right for on-screen UI, FixedRect2 is right for the parts of a game that must agree bit-for-bit across machines — lockstep-multiplayer broadphase, replay-exact overlap tests, deterministic trigger volumes. Same half-open convention as Rect2/Rect2i (left/top inclusive, right/bottom exclusive), same API — hasPoint / intersects / intersection / merge / grow / expand / abs / clampPoint. All arithmetic is integer fixed-point. Header-only, pure. Godot has no fixed-point rectangle.

**Types:** `FixedRect2`

### `FixedTrig`
<sub>`engine/include/maz/math/FixedTrig.hpp`</sub>

maz::math::FixedTrig — DETERMINISTIC fixed-point sine and cosine for core::Fixed (Q16.16), computed by an integer CORDIC (COordinate Rotation DIgital Computer) with no floats on the runtime path. Where std::sin/std::cos give subtly different bits on different CPUs/compilers/optimisation levels — silently desyncing lockstep multiplayer and replays — this rotates an integer vector by a table of precomputed arctangents using only shifts, adds, and one integer scale, so identical inputs give identical bits everywhere. Angles are in radians (Fixed). Accuracy is ~1e-4 (limited by 16 fractional bits), plenty for gameplay rotation. Godot has no fixed-point trig at all, so this is a genuine gap. Header-only.

**Types:** `FixSinCos`

**Functions:**

- `inline constexpr std::int32_t kFixPiRaw = 205887;               // round(pi * 65536)`
- `inline constexpr std::int32_t kFixHalfPiRaw = 102944;           // round((pi/2) * 65536)`
- `inline constexpr std::int64_t kCordicGainRaw = 39797;           // round(0.6072529350 * 65536)`
- `inline FixSinCos fixSinCos(Fixed angle)`
- `inline Fixed fixSin(Fixed angle)`
- `inline Fixed fixCos(Fixed angle)`
- `inline Fixed fixPi()`
- `inline Fixed fixTwoPi()`
- `inline Fixed fixHalfPi()`
- `inline Fixed fixAtan2(Fixed y, Fixed x)`

### `FixedVec2`
<sub>`engine/include/maz/math/FixedVec2.hpp`</sub>

maz::math::FixedVec2 — a 2D vector of core::Fixed (Q16.16) for DETERMINISTIC 2D simulation. Where vec2 (float) is right for rendering, FixedVec2 is right for the parts that must agree bit-for-bit across machines: lockstep-multiplayer movement, replay-exact physics, cross-platform gameplay. All arithmetic is the underlying integer math of core::Fixed, so identical inputs give identical bits everywhere. Provides the vector staples — add/sub/scale, dot, the scalar 2D cross, squared/true length (via the float-free Fixed::sqrt), distance, and a best-effort normalize. Header-only. Godot has no fixed-point vector.

**Types:** `FixedVec2`

### `FixedVec3`
<sub>`engine/include/maz/math/FixedVec3.hpp`</sub>

maz::math::FixedVec3 — a 3D vector of core::Fixed (Q16.16) for DETERMINISTIC 3D simulation, the deterministic companion to vec3. Where vec3 (float) is right for rendering, FixedVec3 is right for the parts of a 3D game that must agree bit-for-bit across machines: lockstep-multiplayer movement, replay-exact physics, cross-platform gameplay. All arithmetic is the underlying integer math of core::Fixed, so identical inputs give identical bits everywhere. Provides the vector staples — add/sub/scale, dot, the true 3D cross product, squared/true length (via the float-free Fixed::sqrt), distance, and a best-effort normalize. Header-only. Godot has no fixed-point vector. The 3D sibling of M416 FixedVec2.

**Types:** `FixedVec3`

### `Geometry2D`
<sub>`engine/include/maz/math/Geometry2D.hpp`</sub>

2D computational-geometry helpers — Godot's Geometry2D static class. These are the workhorse primitives behind AI line-of-sight, mouse/hit picking, path building, trigger zones, and collision pre-checks: does this segment cross that one, what is the nearest point on this edge, is this point inside that polygon. Maz had these scattered (triangle area in the triangulator, SAT in ConvexShape2D, ray/AABB in Collision); this consolidates the segment/polygon/circle set Godot exposes as one namespace. Pure math over vec2 — no allocation, no renderer — so it unit-tests exactly and drives a 2D golden.

**Types:** `SegmentHit`, `Circle2`, `OrientedRect`

**Functions:**

- `inline SegmentHit segmentIntersect(vec2 a, vec2 b, vec2 c, vec2 d)`
- `inline vec2 closestPointOnSegment(vec2 p, vec2 a, vec2 b)`
- `inline float distanceToSegment(vec2 p, vec2 a, vec2 b)`
- `inline bool pointInPolygon(vec2 p, const std::vector<vec2>& poly)`
- `inline bool segmentIntersectsCircle(vec2 a, vec2 b, vec2 center, float radius)`
- `inline bool pointInCircle(vec2 p, vec2 center, float radius)`
- `inline vec2 closestPointOnLine(vec2 p, vec2 a, vec2 b)`
- `inline std::optional<vec2> lineIntersectsLine(vec2 fromA, vec2 dirA, vec2 fromB, vec2 dirB)`
- `inline bool pointInTriangle(vec2 p, vec2 a, vec2 b, vec2 c)`
- `inline void closestPointsBetweenSegments(vec2 p1, vec2 q1, vec2 p2, vec2 q2, vec2& c1, vec2& c2)`
- `inline float polygonArea(const std::vector<vec2>& poly)`
- `inline bool isPolygonClockwise(const std::vector<vec2>& poly)`
- _…and 16 more_

### `Geometry3D`
<sub>`engine/include/maz/math/Geometry3D.hpp`</sub>

maz::math::Geometry3D — the first-class 3D primitive types every engine leans on for spatial math: a Plane, a Ray3, an axis-aligned box (Aabb3), and an oriented box (Obb), with the exact, closed-form intersection tests that culling, picking, physics broadphase, and level queries are built from. These mirror Godot's `Plane` / `AABB` / geometry helpers (and add an OBB with the separating-axis test Godot only has internally), so gameplay code has a portable, dependency-light vocabulary for "where is this, and does it touch that?".  Conventions match the rest of Maz math: right-handed, GLM vectors, world units. Everything is a pure value type — no allocation, no GPU — so it is trivially unit-testable and deterministic.

**Types:** `Plane`, `Ray3`, `Aabb3`, `Obb`, `ConvexMesh3`

**Functions:**

- `inline vec3 closestPointToSegment(const vec3& p, const vec3& a, const vec3& b)`
- `inline vec3 closestPointToSegmentUncapped(const vec3& p, const vec3& a, const vec3& b)`
- `inline void closestPointsBetweenSegments(const vec3& p1, const vec3& p2, const vec3& q1,`
- `inline std::optional<vec3> rayIntersectsTriangle(const vec3& from, const vec3& dir, const vec3& a,`
- `inline std::optional<vec3> segmentIntersectsTriangle(const vec3& from, const vec3& to, const vec3& a,`
- `inline std::optional<vec3> segmentIntersectsSphere(const vec3& from, const vec3& to,`
- `inline std::optional<vec3> segmentIntersectsCylinder(const vec3& from, const vec3& to, float height,`
- `inline std::vector<Plane> buildBoxPlanes(const vec3& extents, const vec3& center = vec3(0.0f))`
- `inline std::vector<Plane> buildCylinderPlanes(float radius, float height, int sides, int axis = 2)`
- `inline std::vector<Plane> buildCapsulePlanes(float radius, float height, int sides, int rings,`
- `inline vec3 closestPointOnTriangle(const vec3& p, const vec3& a, const vec3& b, const vec3& c)`
- `inline vec3 barycentric(const vec3& p, const vec3& a, const vec3& b, const vec3& c)`
- _…and 8 more_

### `GjkDistance`
<sub>`engine/include/maz/math/GjkDistance.hpp`</sub>

maz::math GJK distance — the Gilbert-Johnson-Keerthi algorithm for the MINIMUM DISTANCE between two convex polygons (2D), with the closest pair of witness points. This is the proximity query the engine's SAT collider (game::ConvexShape2D) cannot answer: SAT reports only a boolean overlap (+ penetration when touching), whereas GJK returns the exact gap between two shapes that are APART, and the two closest points, one on each. That is what "how close is the projectile to the wall?", speculative/predictive contacts, proximity triggers, and AI standoff distances need. GJK works on the Minkowski difference via a support function (the farthest vertex in a direction), evolving a 1-3 point simplex toward the origin; if the origin is enclosed the shapes intersect (distance 0). Pure vec2 math, deterministic, header-only. Godot exposes no GJK distance query. (Convex inputs assumed; any vertex order works.)

**Types:** `GjkResult`

**Functions:**

- `inline vec2 gjkSupport(const std::vector<vec2>& hull, vec2 d)`
- `inline GjkResult gjkDistance(const std::vector<vec2>& A, const std::vector<vec2>& B)`

### `GreatCircle`
<sub>`engine/include/maz/math/GreatCircle.hpp`</sub>

maz::math great-circle / spherical geometry — distances and shortest paths ON a sphere, for planet and globe games, star/sky-dome positions, orbital/satellite tracks, and "fly the shortest route between two map points." A straight line through 3D space is not the shortest path along a spherical surface; the great-circle (the arc of the plane through both points and the sphere's center) is. This provides the haversine central-angle/distance between two lat/lon points (numerically stable for both tiny and antipodal separations), lat/lon <-> unit-vector conversion, the angle between unit vectors, and unit-vector SLERP so you can walk the great-circle arc at a uniform rate. Pure trig over floats — exactly unit-testable against known angles (pole-to-pole = pi, equator quarter = pi/2).

**Functions:**

- `inline float haversineCentralAngle(float lat1, float lon1, float lat2, float lon2)`
- `inline float greatCircleDistance(float lat1, float lon1, float lat2, float lon2, float radius)`
- `inline vec3 latLonToUnit(float lat, float lon)`
- `inline float angleBetweenUnit(const vec3& a, const vec3& b)`
- `inline vec3 slerpUnit(const vec3& a, const vec3& b, float t)`
- `inline vec3 greatCirclePoint(float lat1, float lon1, float lat2, float lon2, float f)`

### `Grid3DSample`
<sub>`engine/include/maz/math/Grid3DSample.hpp`</sub>

maz::math 3D grid sampling — read a value out of a VOLUME grid at fractional coordinates, the 3D companion to GridSample.hpp (which handles 2D). This is the primitive under sampling a density / fog / SDF / 3D-noise volume, a baked GI light-probe grid, or a 3D LUT at a continuous world position: integer coordinates land on cell centres and in between you pick nearest (blocky) or trilinear (smooth — the workhorse). Trilinear blends the 8 surrounding cells and reproduces any affine or multilinear field EXACTLY. Out-of-bounds reads use the edge mode (Clamp repeats the border, Wrap tiles). Templated on the cell type, so it samples a float density grid, a vec3 vector field, or an RGB volume alike — any type with `T + T` and `T * float`. The engine had trilinear baked into MeshSdf / noise individually; this is the one reusable primitive. Header-only, pure, deterministic.

**Functions:**

- `inline T gridAt3D(const std::vector<T>& data, int w, int h, int d, int x, int y, int z, GridEdge edge)`
- `inline T grid3DNearest(const std::vector<T>& data, int w, int h, int d, float x, float y, float z,`
- `inline T grid3DTrilinear(const std::vector<T>& data, int w, int h, int d, float x, float y, float z,`

### `GridSample`
<sub>`engine/include/maz/math/GridSample.hpp`</sub>

maz::math grid sampling — read a value out of a 2D grid at FRACTIONAL coordinates, smoothly interpolating between cells. The everyday need behind sampling a heightfield between vertices, a flow-field or vector map between cells, a downsampled lightmap / SDF / noise texture, or any coarse data grid a game wants to read at a continuous world position. Integer coordinates land on cell centres; in between you choose nearest (blocky), bilinear (smooth, the default workhorse), or bicubic Catmull-Rom (smoother, passes through the grid values). Out-of-bounds reads are handled by the edge mode: Clamp (repeat the border) or Wrap (tile). Templated on the cell type, so it samples a float grid, a vec2 flow field, or an RGB grid alike — any type that supports `T + T` and `T * float`. The engine had bilinear baked into HeightField / Image / noise individually; this is the one reusable primitive. Header-only, pure, deterministic.

**Functions:**

- `inline int gridWrapIndex(int i, int n, GridEdge edge)`
- `inline T gridAt(const std::vector<T>& data, int w, int h, int x, int y, GridEdge edge)`
- `inline T lerpT(const T& a, const T& b, float t)`
- `inline T gridNearest(const std::vector<T>& data, int w, int h, float x, float y, GridEdge edge = GridEdge::Clamp)`
- `inline T gridBilinear(const std::vector<T>& data, int w, int h, float x, float y, GridEdge edge = GridEdge::Clamp)`
- `inline T catmullRom(const T& p0, const T& p1, const T& p2, const T& p3, float t)`
- `inline T gridBicubic(const std::vector<T>& data, int w, int h, float x, float y, GridEdge edge = GridEdge::Clamp)`

### `HalfFloat`
<sub>`engine/include/maz/math/HalfFloat.hpp`</sub>

maz::math IEEE 754 half-precision (float16) conversions — the 16-bit floating-point format Godot exposes as `Math::half_to_float` / `Math::make_half_float` and leans on for HDR image storage, glTF quantized vertex accessors, and GPU vertex/attribute compression (half the bandwidth of float32). Maz had no way to pack or unpack halves, so any code touching a .hdr/.exr-style buffer, a KTX2 float texture, or a half-packed vertex stream had to hand-roll the bit twiddling. `floatToHalf` rounds to nearest, ties to even (the IEEE default and what GPUs do), saturating out-of-range magnitudes to infinity and flushing the tiniest values through the subnormal range to zero; `halfToFloat` is exact. Header-only, pure integer bit work — unit-tested headlessly against the reference half encoding produced by Python's `struct` module.

**Functions:**

- `inline float halfToFloat(std::uint16_t h)`
- `inline std::uint16_t floatToHalf(float f)`

### `Integrate`
<sub>`engine/include/maz/math/Integrate.hpp`</sub>

maz::math numerical integration — estimate the definite integral of a function you can only evaluate pointwise. The everyday need behind measuring the ARC LENGTH of a curve (integrate the speed), the AREA under a response/response-time curve, the WORK done by a varying force, the expected value of a distribution, or any "sum up a continuous quantity" the engine can't do in closed form. Two methods: composite Gauss-Legendre (5-point per panel — spectacularly accurate for smooth functions, and EXACT for polynomials up to degree 9) and adaptive Simpson (spends samples where the function wiggles, to a caller tolerance). Both are templated on any callable double(double). Computes in double for accuracy. Godot has no general numeric integrator. Header-only, std-only, deterministic.

**Functions:**

- `inline double gl5(F&& f, double a, double b)`
- `inline double simpson(double a, double b, double fa, double fb, double fm)`
- `inline double adaptive(F&& f, double a, double b, double fa, double fb, double fm, double whole, double tol,`
- `inline double integrateGauss(F&& f, double a, double b, int panels = 1)`
- `inline double integrateAdaptiveSimpson(F&& f, double a, double b, double tol = 1e-8, int maxDepth = 40)`

### `Integrator`
<sub>`engine/include/maz/math/Integrator.hpp`</sub>

maz::math numerical ODE integrators — advance a continuous system state by dt given its derivative. The engine's physics uses semi-implicit Euler (fast, stable enough for contacts); these are the accurate general-purpose integrators for smooth continuous motion where Euler drifts: orbital and n-body motion, spring/pendulum simulation, projectiles with air drag, and any custom equation of motion. RK4 (classic 4th-order Runge-Kutta) is the workhorse — its error shrinks ~16x each time the step halves. Generic over any State type that supports `State + State` and `State * float` (plain float, a vec, or a small phase-space struct), with the derivative supplied as a callable deriv(state, t). Godot exposes no general integrator to gameplay code, so this is a beyond-Godot numerics utility. Header-only, std-only, deterministic.

### `Intercept`
<sub>`engine/include/maz/math/Intercept.hpp`</sub>

maz::math projectile lead / intercept solver — "aim ahead of a moving target." Given a shooter, a target's current position and velocity, and how fast the projectile travels, find WHERE to aim so a shot fired now meets the target: the time-to-intercept, the future point to aim at, and the unit aim direction. This is the math behind every turret, homing shot, AI marksman, and "lead the duck" mechanic. It's a quadratic in the intercept time t — solve |targetPos + targetVel·t − shooter| = projSpeed·t — which can have zero solutions (the target outruns the projectile), one, or two (pick the soonest). Pure closed-form math over vec2/vec3, no allocation, so it unit-tests exactly.

**Types:** `Intercept2D`, `Intercept3D`

**Functions:**

- `inline float smallestPositiveRoot(float a, float b, float c)`
- `inline Intercept2D interceptTarget(vec2 shooter, vec2 targetPos, vec2 targetVel, float projSpeed)`
- `inline Intercept3D interceptTarget(vec3 shooter, vec3 targetPos, vec3 targetVel, float projSpeed)`

### `InverseBilinear`
<sub>`engine/include/maz/math/InverseBilinear.hpp`</sub>

maz::math inverse bilinear interpolation — map a point inside a (possibly warped) quad back to its (u,v) coordinates in the unit square.  FORWARD bilinear is easy: given (u,v) in [0,1]^2 and the four quad corners, blend them. The INVERSE — "I have a point P inside this quad; what (u,v) produced it?" — is what you need to look up the UV/colour of a hit position in a warped or perspective-flattened quad, to deform a grid, to map screen-picks into a distorted panel's local space, or to resample between two non-aligned grids. For a general (non-parallelogram) quad the answer requires solving a quadratic, which this does robustly (falling back to the linear affine case when the quad is a parallelogram). Corners are A(0,0) B(1,0) C(1,1) D(0,1) going around. Pure vec2 math, header-only, deterministic — unit-tested by round-tripping the forward map and against the exact corner/centre coordinates.

**Types:** `InvBilinearResult`

**Functions:**

- `inline vec2 bilinear(const vec2& a, const vec2& b, const vec2& c, const vec2& d, float u, float v)`
- `inline float ibCross(const vec2& u, const vec2& v)`
- `inline InvBilinearResult invBilinear(const vec2& p, const vec2& a, const vec2& b, const vec2& c,`

### `Involute`
<sub>`engine/include/maz/math/Involute.hpp`</sub>

maz::math involute of a circle — the curve traced by the end of a taut string as it unwinds from a circle. It is THE tooth-flank profile of real spur gears: two involute gears transmit rotation at a perfectly constant ratio because the contact normal always lies on the fixed "line of action" tangent to the base circles. Use it for mechanically-correct gears (as opposed to the decorative trapezoidal cog in render::shapes2d::gear), clock escapements, cam profiles, and unwinding-cable animation. Godot has no involute primitive. Parameterised by the unwinding angle `t` (radians): at t the string has unwound to the tangent point at angle t on the base circle and the free end sits a distance baseRadius·t away, perpendicular to that radius — so the swept arc length grows as baseRadius·t²/2 and the local radius of curvature is exactly baseRadius·t. Header-only, std-only, deterministic.

**Functions:**

- `inline vec2 involutePoint(const vec2& center, float baseRadius, float t)`
- `inline vec2 involuteTangent(float baseRadius, float t)`
- `inline vec2 involuteTangentPoint(const vec2& center, float baseRadius, float t)`
- `inline std::vector<vec2> involutePolyline(const vec2& center, float baseRadius, float tStart, float tEnd,`

### `KMeans`
<sub>`engine/include/maz/math/KMeans.hpp`</sub>

maz::math::kMeans — partition N-dimensional points into k clusters, each represented by its centroid, so that points end up grouped with their nearest centre (Lloyd's algorithm with k-means++ seeding). This is the general clustering workhorse: grouping units/enemies into squads by position, building spatial LOD clusters, deriving a representative palette or set of "archetype" values from data, seeding procedural distributions, or compressing a cloud of samples down to k prototypes. The engine had median-cut colour quantization (fixed to RGB) but no general k-means over arbitrary vectors. It works by alternating two steps until stable: ASSIGN every point to its nearest centroid, then MOVE each centroid to the mean of its assigned points — which provably never increases the total within-cluster squared distance (the "inertia"), so it converges. k-means++ picks well-spread initial centres (proportional to squared distance) so it converges fast and avoids poor local minima. Deterministic given a seed. Header-only, std-only. Godot has no clustering.

**Types:** `KMeansResult`

**Functions:**

- `inline std::uint64_t kmSplitmix(std::uint64_t& s)`
- `inline double kmRand01(std::uint64_t& s)`
- `inline double kmDistSq(const std::vector<double>& a, const std::vector<double>& b)`
- `inline KMeansResult kMeans(const std::vector<std::vector<double>>& points, int k, std::uint64_t seed,`

### `Kabsch`
<sub>`engine/include/maz/math/Kabsch.hpp`</sub>

maz::math::kabsch — find the rigid transform (rotation + translation, NO scale/shear) that best maps one set of 3D points onto another in the least-squares sense. Given N corresponding point pairs (from[i] should land near to[i]), it returns the single rotation and translation minimising the summed squared error. This is the workhorse behind point-cloud REGISTRATION (line up a scanned/streamed set of points with a reference), POSE fitting (recover how a rigid body moved from a few tracked markers), mocap/tracking alignment, and procedural retargeting. The engine had per-axis fits and eigen-based OBB fitting but no "best rotation between two clouds". This uses Horn's closed-form quaternion solution (1987): build a 4x4 symmetric matrix from the cross-covariance of the centred clouds, take the eigenvector of its largest eigenvalue as the optimal rotation quaternion (via a Jacobi eigensolve), then translation = centroidTo - R*centroidFrom. Always yields a PROPER rotation (never a reflection). Header-only, deterministic.

**Types:** `RigidTransform`

**Functions:**

- `inline void jacobi4(double a[4][4], double v[4][4], double d[4])`
- `inline RigidTransform kabsch(const std::vector<vec3>& from, const std::vector<vec3>& to, bool* ok = nullptr)`

### `LeastSquares`
<sub>`engine/include/maz/math/LeastSquares.hpp`</sub>

Straight-line fit y = slope * x + intercept.

**Types:** `LineFit`, `PolyFit`

**Functions:**

- `inline bool solveDense(std::vector<double>& a, std::vector<double>& b, std::size_t m)`
- `inline float evalPolynomial(const std::vector<float>& coeffs, float x)`
- `inline LineFit fitLine(const std::vector<float>& xs, const std::vector<float>& ys)`
- `inline PolyFit fitPolynomial(const std::vector<float>& xs, const std::vector<float>& ys, int degree)`

### `LinearSolve`
<sub>`engine/include/maz/math/LinearSolve.hpp`</sub>

maz::math dense linear system solver — solve A x = b for a general NxN matrix, plus determinant and matrix inverse, via LU decomposition with partial pivoting. This is the numerical workhorse under least-squares FITTING (fit a plane/polynomial/curve to data through the normal equations), inverse-kinematics and physics CONSTRAINT solves (small dense Jacobian/impulse systems), colour-space and calibration transforms, barycentric/anywhere-interpolation setups, and any "n equations, n unknowns" that shows up in tools and gameplay math. The engine has RK4, quadrature, root-finding and polynomial roots but no general Ax=b solver; this fills that. Partial pivoting keeps it numerically stable and detects singular systems. Row-major matrices, double precision. Header-only, std-only, deterministic.

**Functions:**

- `inline bool luDecompose(std::vector<double>& A, int n, std::vector<int>& piv, int& sign)`
- `inline std::vector<double> luSolve(const std::vector<double>& lu, const std::vector<int>& piv, int n,`
- `inline bool solveLinearSystem(std::vector<double> A, const std::vector<double>& b, int n, std::vector<double>& x)`
- `inline double determinant(std::vector<double> A, int n)`
- `inline bool invertMatrix(const std::vector<double>& A, int n, std::vector<double>& inv)`

### `LogSpiral`
<sub>`engine/include/maz/math/LogSpiral.hpp`</sub>

maz::math logarithmic (equiangular) spiral — the growth spiral of nautilus shells, sunflower seed heads, galaxy arms and hurricanes: r(θ) = a·e^(b·θ), so the radius multiplies by a constant factor for every fixed turn. Its defining trait is that the curve crosses every ray from the centre at the SAME angle (hence "equiangular"), which also makes it SELF-SIMILAR — zooming in reproduces the same spiral rotated. Use it for procedural shells/horns, spiral galaxies and vortices, spiral camera or motion paths, and radial UI layouts. Godot has no spiral primitive. `a` sets the starting radius (at θ = 0) and `b` the tightness (b = 0 degenerates to a circle; larger |b| unwinds faster; sign flips the winding direction). Returns a polyline ready for the line/polygon renderer. Header-only, std-only, deterministic.

**Functions:**

- `inline vec2 logSpiralPoint(const vec2& center, float a, float b, float theta)`
- `inline vec2 logSpiralTangent(float a, float b, float theta)`
- `inline std::vector<vec2> logSpiralPolyline(const vec2& center, float a, float b, float thetaStart,`

### `LowDiscrepancy`
<sub>`engine/include/maz/math/LowDiscrepancy.hpp`</sub>

maz::math low-discrepancy (quasi-random) sequences — Halton, Hammersley, and the van der Corput radical inverse they are built on. Unlike a pseudo-random generator (which clumps and leaves gaps), these sequences fill the unit interval/square as EVENLY as possible for any prefix count, which is exactly what you want for: temporal anti-aliasing sub-pixel jitter (a different well-spread offset each frame), progressive/quasi-Monte-Carlo sampling (soft shadows, AO, IBL importance sampling that converges faster than white noise), and even scatter placement. The radical inverse reflects an integer's digits (in some base) about the decimal point; Halton pairs two coprime-base radical inverses; Hammersley uses the sample index directly for one axis. Pure integer/float math, no state, no allocation — deterministic and exactly unit-testable against the sequence's known values.

**Functions:**

- `inline float radicalInverse(uint32_t base, uint32_t i)`
- `inline float vanDerCorput2(uint32_t i)`
- `inline vec2 halton2D(uint32_t i, uint32_t baseX = 2, uint32_t baseY = 3)`
- `inline vec2 hammersley2D(uint32_t i, uint32_t count)`

### `Loxodrome`
<sub>`engine/include/maz/math/Loxodrome.hpp`</sub>

maz::math loxodrome (rhumb line) — the path across a sphere that holds a CONSTANT compass bearing, crossing every meridian at the same angle. It is the "steady heading" route a ship or plane follows when it just keeps the compass pinned (as opposed to the great-circle shortest path in GreatCircle.hpp, whose heading constantly changes). On a Mercator map a rhumb line is a straight line; on the globe it spirals toward the pole. Use it for navigation/strategy/globe games, constant-heading travel, or drawing rhumb spirals. Godot has no such helper. Latitudes/longitudes are in radians; bearing is measured clockwise from north (N=0, E=π/2). Header-only, std-only, deterministic.

**Types:** `LatLon`

**Functions:**

- `inline vec3 latLonToUnit(const LatLon& p)`
- `inline LatLon loxodromePoint(const LatLon& start, float bearing, float distance, float radius = 1.0f)`
- `inline std::vector<LatLon> loxodromePolyline(const LatLon& start, float bearing, float totalDistance,`

### `MarchingSquares`
<sub>`engine/include/maz/math/MarchingSquares.hpp`</sub>

maz::math marching squares — extract the iso-contour (a set of line segments) of a 2D scalar field at a given threshold. This is the 2D sibling of marching cubes and the standard tool behind metaball outlines, fluid/lava surfaces, terrain contour lines, and fog-of-war edges: sample any scalar function on a grid, ask "where does it cross value T?", and get back the polyline pieces of that boundary. Each grid cell is classified by which of its four corners are >= the threshold (16 cases), edge crossings are placed by linear interpolation for a smooth contour, and the two ambiguous saddle cases are resolved with the cell-centre average. Pure CPU math, deterministic, header-only. Godot has no direct equivalent (its CSG uses marching cubes internally), so this is a genuinely-useful beyond-Godot utility with an exact, testable output.

**Types:** `ContourSegment`

**Functions:**

- `inline std::vector<ContourSegment> marchingSquares(const std::vector<float>& field, int width,`

### `MarchingTetrahedra`
<sub>`engine/include/maz/math/MarchingTetrahedra.hpp`</sub>

maz::math Marching Tetrahedra — extract a triangle mesh of an isosurface (field == iso) from a scalar field sampled on a 3-D grid. The engine has 2-D MarchingSquares and the dual-vertex SurfaceNets mesher; this is the classic PRIMAL marching-simplex method and fills the "give me the level set of a 3-D field as triangles" gap for voxel terrain, metaballs/blobby surfaces, CSG/SDF meshing and scientific/medical volumes. It splits every grid cube into SIX tetrahedra and marches each one: because adjacent tetrahedra share a triangular face and the crossing on that face is fixed by the three shared corner values, the output is WATERTIGHT and crack-free by construction — no ambiguous 256-case cube table, and none of marching cubes' hole artefacts. Surface vertices are interpolated along grid edges and de-duplicated by edge, so the mesh is a proper 2-manifold. Godot has no isosurface extractor. The field is passed as a callable `float(vec3)`. Header-only, std-only, deterministic.

**Types:** `IsoMesh`

**Functions:**

- `inline IsoMesh marchingTetrahedra(Field&& field, const vec3& mn, const vec3& mx, int res,`

### `Math`
<sub>`engine/include/maz/math/Math.hpp`</sub>

**Functions:**

- `inline mat4 perspective(float fovYRadians, float aspect, float zNear, float zFar)`
- `inline mat4 orthographic(float left, float right, float bottom, float top, float zNear, float zFar)`
- `inline mat4 orthographicSize(float verticalSize, float aspect, float zNear, float zFar)`
- `inline mat4 ortho2D(float width, float height)`

### `MathFuncs`
<sub>`engine/include/maz/math/MathFuncs.hpp`</sub>

maz::math scalar math helpers — Godot's @GlobalScope numeric utilities that gameplay code reaches for constantly but the C++ standard library does not spell: remap / inverse_lerp, wrapf / wrapi (looping values), smoothstep, ease (Godot's tunable easing curve), lerp_angle (shortest-arc angle blend), pingpong, nearest_po2, deg<->rad, and approximate comparisons. Semantics match Godot's exactly (verified against its source). Header-only, pure, unit-tested. (Vector variants of move_toward / snapped live in VectorOps; this is the scalar layer.)

**Functions:**

- `inline float degToRad(float deg)`
- `inline float radToDeg(float rad)`
- `inline float signf(float x)`
- `inline int signi(std::int64_t x)`
- `inline float lerpf(float a, float b, float t)`
- `inline float inverseLerp(float a, float b, float v)`
- `inline float remap(float v, float iMin, float iMax, float oMin, float oMax)`
- `inline float clampf(float v, float lo, float hi)`
- `inline std::int64_t clampi(std::int64_t v, std::int64_t lo, std::int64_t hi)`
- `inline float wrapf(float value, float minv, float maxv)`
- `inline std::int64_t wrapi(std::int64_t value, std::int64_t minv, std::int64_t maxv)`
- `inline std::int64_t posmod(std::int64_t x, std::int64_t y)`
- _…and 16 more_

### `MinkowskiSum`
<sub>`engine/include/maz/math/MinkowskiSum.hpp`</sub>

maz::math Minkowski sum of two convex polygons — "sweep one shape around the boundary of another and take everything the pair can cover together": the set { a + b : a in A, b in B }. It is the workhorse behind collision inflation and motion planning. Grow a level's walls by the radius of the player and a point-sized dot can be tested instead of a fat body (the grown obstacle is exactly wall (+) player-disc). Build the "configuration-space obstacle" a moving convex agent must avoid (obstacle (+) reflected-agent), so path-planning collapses to routing a single point. Round a polygon by summing it with a small disc, or fatten a swept shape. The result is always convex, and its support (extent in any direction) is the sum of the two inputs' supports — the identity this module is verified against. Inputs are treated as point sets and reduced to their convex hull first, so any convex shape (or cloud of samples) works. Godot exposes no Minkowski sum, so this is a beyond-Godot geometry utility. Header-only, std-only, deterministic; O(n+m) edge merge on CCW hulls.

**Functions:**

- `inline std::vector<vec2> minkowskiSumConvex(const std::vector<vec2>& A, const std::vector<vec2>& B)`

### `MonotoneCubic`
<sub>`engine/include/maz/math/MonotoneCubic.hpp`</sub>

maz::math::MonotoneCubic — piecewise cubic Hermite interpolation with the Fritsch-Carlson tangent correction (PCHIP): a smooth C1 curve through your data points that provably NEVER OVERSHOOTS. This is the crucial difference from the engine's other interpolators: a natural CubicSpline is C2 but can bulge past the data (a run of equal values can dip below or rise above them), and Catmull-Rom (VectorOps) overshoots too. PCHIP guarantees the curve stays monotone wherever the data is monotone and never leaves the bracket of its neighbouring samples — so it is the right tool for a tone / gamma / difficulty curve, a health or fuel gauge response, an audio envelope, or a terrain cross-section that must not dip below the sampled heights. Tangents are chosen from secant slopes then clamped so no segment can overshoot. Build once from strictly-increasing x with matching y; eval() clamps to the endpoints outside the range. O(n) build, O(log n) eval. Header-only, std-only, deterministic. Godot has no monotone interpolant.

**Types:** `MonotoneCubic`

### `NelderMead`
<sub>`engine/include/maz/math/NelderMead.hpp`</sub>

maz::math Nelder-Mead downhill simplex — minimise a scalar function of several variables WITHOUT needing its derivatives. You hand it a function f(x) (x is a vector of parameters) and a starting guess; it walks a simplex (a blob of n+1 points) downhill — reflecting, expanding, and contracting — until it settles in a minimum. Derivative-free means f can be anything you can evaluate: a simulation score, a fit error, a procedural-generation quality metric, a physics residual.  This complements what the engine already has — RootFind (1-D root finding), SimulatedAnnealing (stochastic global search), Minimax (game trees) — with the workhorse for small, smooth, deterministic local minimisation: fitting a parametric curve/easing to sampled data, auto-tuning controller or spring gains, solving a few-parameter constraint (aim/IK-style) by minimising a residual, or calibrating a model. Neither Godot nor Unity ships a general optimiser. Header-only, std-only, deterministic (no RNG), double precision.

**Types:** `NelderMeadResult`, `NelderMeadOptions`

**Functions:**

- `inline double dist2(const std::vector<double>& a, const std::vector<double>& b)`
- `inline NelderMeadResult nelderMead(F&& f, const std::vector<double>& x0, const NelderMeadOptions& opt =`

### `NurbsCurve`
<sub>`engine/include/maz/math/NurbsCurve.hpp`</sub>

maz::math NURBS curve (Non-Uniform Rational B-Spline) — the industry-standard freeform curve used by every CAD tool and vector program. It generalises the engine's plain B-spline by giving each control point a WEIGHT, which lets a single curve type represent EXACT conics — perfect circles, ellipses and arcs — that no polynomial Bezier or B-spline can reproduce, alongside arbitrary smooth freeform shapes. Non-uniform knots let you place sharper or gentler regions and pin the endpoints. Use it for precise vector paths, smooth camera/motion rails that must pass through exact circular arcs, lofting profiles, and authoring tools. Evaluated by the numerically stable de Boor algorithm on homogeneous (weighted) control points, then perspective-divided back. Godot's Curve2D is cubic Bezier only — no rational curves. Header-only, std-only, deterministic.

**Functions:**

- `inline vec2 nurbsPoint(const std::vector<vec2>& ctrl, const std::vector<float>& weights,`
- `inline std::vector<float> nurbsClampedKnots(int nCtrl, int degree)`

### `ObbDistance`
<sub>`engine/include/maz/math/ObbDistance.hpp`</sub>

maz::math closest point on / distance to an oriented bounding box (OBB) — the query the engine's Obb was missing. Obb already answers "does it contain this point?" and "do two boxes overlap?" (SAT), but not "what is the nearest point on this box, and how far is it?" — the workhorse for sphere-vs-box collision, character-vs-crate resolution, editor picking, and proximity/trigger tests against a rotated box. Works by dropping the point into the box's local frame (projection onto its axes), clamping per-axis, and mapping back. Also provides the EXACT oriented-box signed distance field (negative inside), which the cheap "gradient-scaled implicit" only approximates. Godot exposes no OBB closest-point helper. Header-only, std-only, deterministic.

**Functions:**

- `inline vec3 closestPointOnObb(const Obb& box, const vec3& p)`
- `inline float sqDistanceToObb(const Obb& box, const vec3& p)`
- `inline float distanceToObb(const Obb& box, const vec3& p)`
- `inline float signedDistanceObb(const Obb& box, const vec3& p)`
- `inline bool obbIntersectsSphere(const Obb& box, const vec3& sphereCenter, float radius)`

### `OctahedralNormal`
<sub>`engine/include/maz/math/OctahedralNormal.hpp`</sub>

maz::math octahedral unit-vector encoding — pack a unit vector (a normal, a direction) into just TWO numbers in [-1,1], and unpack it back, with barely any error. This is the standard way modern renderers store normals compactly: in a G-buffer (deferred shading), a compressed normal map, a light-probe direction, or any place three floats per normal is too much bandwidth/memory. Octahedral mapping unfolds the sphere onto a square and is far more uniform (less error) than the old "store xy, reconstruct z" hemisphere trick. Two variants: full-sphere `octEncode`/`octDecode` (any direction) and `octEncodeHemi`/ `octDecodeHemi` (a +Z hemisphere, e.g. view-space normals — uses the whole square for extra precision). Godot does this inside shaders only; this is the reusable CPU-side pair for baking and tools. Header-only, std-only, deterministic.

**Functions:**

- `inline float signNotZero(float v)`
- `inline vec3 norm3(const vec3& v)`
- `inline vec2 octEncode(const vec3& nIn)`
- `inline vec3 octDecode(const vec2& f)`
- `inline vec2 octEncodeHemi(const vec3& nIn)`
- `inline vec3 octDecodeHemi(const vec2& e)`

### `Optics`
<sub>`engine/include/maz/math/Optics.hpp`</sub>

maz::math optics — refraction (Snell's law) and Fresnel reflectance, the light-bending math behind water, glass, gems, and physically-based rendering. The engine already has `reflect` (VectorOps), the mirror half; this adds the transmission half. `refract` bends an incident direction as it crosses a surface between media with a given index-of-refraction ratio, and returns the zero vector on TOTAL INTERNAL REFLECTION (past the critical angle, e.g. looking up through water from below). The Fresnel-Schlick term gives the fraction of light that REFLECTS versus transmits as a function of viewing angle — near-zero (its base reflectance f0) head-on, rising to 1 at grazing angles (the bright rim you see on water and glass edges), which every PBR shader multiplies its specular by. Pure vec3 math — exactly unit-testable against Snell's law and known Fresnel limits.

**Functions:**

- `inline vec3 refract(const vec3& i, const vec3& n, float eta)`
- `inline bool isTotalInternalReflection(const vec3& i, const vec3& n, float eta)`
- `inline float fresnelF0(float n1, float n2)`
- `inline float fresnelSchlick(float cosTheta, float f0)`
- `inline vec3 fresnelSchlick(float cosTheta, const vec3& f0)`

### `OrientedRect2`
<sub>`engine/include/maz/math/OrientedRect2.hpp`</sub>

maz::math oriented 2D rectangle (OBB2) — a rectangle with a rotation, and the containment/overlap tests that go with it. The engine already has an axis-aligned Rect2 and a 3D oriented box (Obb), but not the 2D oriented rectangle games constantly need: a rotated pickup or trigger zone, a tilted camera bound, hit-testing a rotated UI panel or sprite, a swinging blade's hurtbox. Point-in-rect transforms the point into the box's local frame; rect-vs-rect uses the separating-axis theorem over the four edge normals (two per box). Pure vec2 math — exactly unit-testable (an axis-aligned OBB matches an AABB; a rotated one accepts/rejects points and boxes the AABB would get wrong).

**Types:** `OrientedRect2`

**Functions:**

- `inline bool orientedRectsOverlap(const OrientedRect2& a, const OrientedRect2& b)`

### `PackNorm`
<sub>`engine/include/maz/math/PackNorm.hpp`</sub>

maz::math fixed-point normalized packing (UNORM / SNORM) — the quantization layer of GPU vertex and attribute compression, converting a normalized float to an 8- or 16-bit integer and back. This is the piece that actually shrinks the data: paired with `octahedronEncode` (VectorOps) it stores a surface normal in two 16-bit integers instead of three 32-bit floats, exactly how Godot's compressed mesh format keeps normals/tangents, and it is the standard Vulkan/OpenGL vertex-attribute encoding. UNORM maps [0,1] to [0, 2^bits-1]; SNORM maps [-1,1] to [-(2^(bits-1)-1), +(2^(bits-1)-1)] (the Khronos symmetric form, so -1 and +1 are exact and 0 lands on 0). Rounding is round-half-away-from-zero (the GL `round()` rule). Pure integer/float math — no GPU — so it unit-tests headlessly by round-trip and exact endpoints.  Scope note (honest): 8/16-bit UNORM/SNORM scalar packing. The caller composes these per channel (e.g. the two components of an octahedral normal); vector overloads and the packed 10/10/10/2 formats are documented follow-ups.

**Functions:**

- `inline std::uint8_t packUnorm8(float f)`
- `inline float unpackUnorm8(std::uint8_t u)`
- `inline std::uint16_t packUnorm16(float f)`
- `inline float unpackUnorm16(std::uint16_t u)`
- `inline std::int8_t packSnorm8(float f)`
- `inline float unpackSnorm8(std::int8_t i)`
- `inline std::int16_t packSnorm16(float f)`
- `inline float unpackSnorm16(std::int16_t i)`

### `ParallelTransport`
<sub>`engine/include/maz/math/ParallelTransport.hpp`</sub>

maz::math rotation-minimizing (parallel-transport) frames — a smoothly twisting coordinate frame that rides along a 3D path. To sweep a cross-section down a curve — a tube, rope, cable, road, vine, ribbon trail, or a camera rail — you need, at every point, a consistent "up"/"side" pair perpendicular to the direction of travel. The textbook Frenet frame (built from the curve's curvature) works until the curve straightens or inflects, where it suddenly flips 180 degrees and the swept mesh visibly kinks. A rotation-minimizing frame instead carries the previous frame forward with the LEAST possible twist, so the tube never spins or snaps. This uses Wang et al.'s double-reflection method, which is exact and stable. Godot has no such utility (its CSGPolygon path-extrude twists on inflections). Header-only, std-only, deterministic.

**Types:** `Frame`

**Functions:**

- `inline vec3 ptNormalize(const vec3& v)`
- `inline float ptDot(const vec3& a, const vec3& b)`
- `inline vec3 ptCross(const vec3& a, const vec3& b)`
- `inline std::vector<Frame> parallelTransportFrames(const std::vector<vec3>& points,`

### `PointDistribution`
<sub>`engine/include/maz/math/PointDistribution.hpp`</sub>

maz::math even point distributions — spread N points as uniformly as possible over a sphere, a hemisphere, or a disc, with no random number generator (fully deterministic). The trick is the golden angle (pi*(3-sqrt5) ≈ 137.5°): stepping each successive point by that angle never lets points line up into spokes or rings, so coverage stays even at any count. This is the workhorse behind uniform DIRECTION sampling — ambient-occlusion / global-illumination rays, reflection-probe placement, spawn directions, LOD impostor captures — and even POINT scatter — star fields, point clouds, dotted patterns, blue-noise-ish sample kernels. The engine already used this spiral inline in a couple of shaders (SoftShadow2D, MeshAO); this exposes it as a reusable, unit-tested primitive. Pairs naturally with SphericalCoords (M635). Header-only, pure, deterministic — same N always yields the same points.

**Functions:**

- `inline std::vector<vec3> fibonacciSphere(int n)`
- `inline std::vector<vec3> fibonacciHemisphere(int n)`
- `inline std::vector<vec2> vogelDisk(int n, float radius = 1.0f)`

### `Poisson`
<sub>`engine/include/maz/math/Poisson.hpp`</sub>

maz::math — Poisson / Laplace equation solver on a 2D grid by Gauss-Seidel relaxation. Solving (discrete) Laplacian(u) = rhs with fixed (Dirichlet) values on chosen cells is the workhorse behind a surprising range of game/graphics tasks: the pressure-projection step that makes fluids incompressible, gradient-domain / "Poisson" image editing (seamlessly cloning a patch so its interior matches the surrounding gradients), steady-state heat/temperature diffusion, and smooth scattered-data interpolation. The 5-point stencil sets each free cell to the average of its four neighbours minus the source term; the iteration converges to the unique solution consistent with the fixed cells. Any cell flagged `fixed` holds its value (the boundary / the region border / a hot spot); all other cells are relaxed. Godot ships no PDE solver. Header-only, std-only, deterministic. Iterate to a residual tolerance or an iteration cap.

**Types:** `PoissonResult`

**Functions:**

- `inline PoissonResult poissonSolve(int width, int height, std::vector<float>& u, const std::vector<float>& rhs,`

### `PolarDecompose`
<sub>`engine/include/maz/math/PolarDecompose.hpp`</sub>

maz::math::polarDecompose — split a 3x3 transform into a pure ROTATION times a symmetric STRETCH: M = R * S, where R is a proper rotation (orthonormal, det +1) and S is symmetric (the scale/shear part). This is the "extract the rotation" operation animation and simulation keep needing: a matrix that has accumulated non-uniform scale, shear, or numerical drift (blended skinning matrices, interpolated bone transforms, a deformed element) can be cleaned back to its nearest rotation. It is the heart of co-rotational / shape-matching deformation (Muller et al.), of orthonormalizing a drifted basis, and of recovering a stable orientation from a squished transform. Computed by Higham's quadratically-convergent iteration R_{k+1} = 1/2 (gamma R_k + gamma^-1 (R_k^-T)), which drives R to the orthogonal polar factor; S = R^T M is then symmetrized. Godot's Basis has orthonormalize() but no true polar decomposition (its Gram-Schmidt depends on axis order and does not give the closest rotation). Header-only, glm-backed, deterministic. Assumes det(M) > 0 (a right-handed transform); returns false otherwise.

**Functions:**

- `inline float frobeniusNorm(const mat3& m)`
- `inline bool polarDecompose(const mat3& m, mat3& rotation, mat3& stretch)`
- `inline mat3 extractRotation(const mat3& m)`

### `PolygonBoolean`
<sub>`engine/include/maz/math/PolygonBoolean.hpp`</sub>

maz::math general polygon boolean operations (Greiner–Hormann) — intersection, union and difference of two ARBITRARY simple polygons, including CONCAVE ones, producing possibly several output contours. This is the real Clipper-style boolean the engine's existing clipPolygonConvex (Geometry2D.hpp, Sutherland–Hodgman) explicitly leaves out of scope: that one only clips against a *convex* window, while this handles concave subject and concave clip and returns the full multi-contour result. Use it for destructible-terrain carving, merging painted regions, computing overlap area between two swept shapes, visibility/coverage masks and vector-boolean authoring tools. Godot exposes this via Geometry2D.clip_polygons / intersect_polygons / merge_polygons (its own Clipper backend); this is the header-only, std-only, deterministic equivalent for the maz engine.  Inputs are simple polygons given as CCW or CW rings (they are internally normalised to CCW). The classic Greiner–Hormann algorithm assumes GENERIC position — no vertex of one polygon lying exactly on an edge of the other and no collinear overlapping edges. Such degeneracies are out of scope here (nudge a coordinate to resolve them), matching the original 1998 algorithm. Results are returned as a list of contours; holes are emitted as separate, oppositely-wound contours (standard even–odd fill).

**Types:** `GhNode`

**Functions:**

- `inline float ghSignedArea(const std::vector<vec2>& poly)`
- `inline bool ghPointInRing(const vec2& pt, const std::vector<vec2>& ring)`
- `inline bool ghSegCross(const vec2& P1, const vec2& P2, const vec2& Q1, const vec2& Q2,`
- `inline void ghInsertSorted(std::vector<GhNode>& pool, int startIdx, int newIdx)`
- `inline std::vector<vec2> ghToCcw(const std::vector<vec2>& poly)`
- `inline std::vector<std::vector<vec2>> ghClip(const std::vector<vec2>& subjectIn,`
- `inline std::vector<std::vector<vec2>> polygonIntersection(const std::vector<vec2>& a,`
- `inline std::vector<std::vector<vec2>> polygonUnion(const std::vector<vec2>& a,`
- `inline std::vector<std::vector<vec2>> polygonDifference(const std::vector<vec2>& a,`

### `PolygonNewell`
<sub>`engine/include/maz/math/PolygonNewell.hpp`</sub>

maz::math Newell's method for a 3D polygon — the robust normal, area, and area-weighted centroid of an arbitrary planar (or nearly-planar) polygon with any number of vertices. The naive "cross two edges" face normal fails on n-gons: pick a near-collinear vertex pair and it collapses, and a slightly non-planar polygon has no single edge-pair that represents the whole face. Newell's method sums a signed contribution over EVERY edge, so it always yields a stable, area-weighted normal (its magnitude is exactly twice the polygon area) and is the standard way engines compute face normals for lightmap/collision meshes, CSG, and importers. The engine only had triangle face normals; this handles quads and general polygons. Godot exposes no such helper. Header-only, std-only, deterministic.

**Types:** `PolygonInfo`

**Functions:**

- `inline vec3 newellVector(const std::vector<vec3>& poly)`
- `inline PolygonInfo polygonInfo3D(const std::vector<vec3>& poly)`
- `inline vec3 polygonNormal3D(const std::vector<vec3>& poly)`
- `inline float polygonArea3D(const std::vector<vec3>& poly)`

### `PolylineStroke`
<sub>`engine/include/maz/math/PolylineStroke.hpp`</sub>

maz::math polyline stroking — turn an OPEN path (a list of points) into a filled polygon outline of a given width, the CPU-side of what Godot's Line2D does on the GPU. This is how you render thick lines, drawn strokes/gestures, trails, roads/rivers, wires and route ribbons as an actual fillable/collidable polygon (feed the result to a triangulator or a polygon collider). The existing Geometry2D.offsetPolygonConvex inflates a CLOSED convex polygon; this handles the open-path case with end caps (butt / square / round) that the closed-polygon offset can't express. Bevel joins at interior vertices keep the outline valid for any turn angle. Header-only, std-only, deterministic.

**Types:** `StrokeOptions`

**Functions:**

- `inline std::vector<vec2> strokePolyline(const std::vector<vec2>& pts, const StrokeOptions& opt)`

### `Polynomial`
<sub>`engine/include/maz/math/Polynomial.hpp`</sub>

maz::math analytic polynomial root solvers — closed-form real roots of quadratics and cubics. These are the exact building blocks behind ray/sphere and ray/quadric intersection, solving for the time a projectile reaches a height, inverting a cubic ease, and other "at what value does this curve equal zero" queries where an iterative solver would be slower and less precise. Godot exposes no general polynomial solver, so this is a beyond-Godot math utility. Quadratics use the numerically-stable form; cubics use Cardano with the trigonometric method for the three-real-root case. Roots come back sorted ascending and de-duplicated. Header-only, std-only.

**Types:** `QuadraticRoots`, `CubicRoots`, `QuarticRoots`

**Functions:**

- `inline QuadraticRoots solveQuadratic(float a, float b, float c)`
- `inline CubicRoots solveCubic(float a, float b, float c, float d)`
- `inline QuarticRoots solveQuartic(float a, float b, float c, float d, float e)`

### `Projection`
<sub>`engine/include/maz/math/Projection.hpp`</sub>

maz::math Projection — Godot's Projection: a 4x4 matrix specialized for camera projections, with the projection-specific constructors (perspective / orthographic / frustum) and the queries you actually ask of one (near/far plane, vertical FOV, aspect, is-it-orthographic) that a raw mat4 does not spell. Conventions match the rest of maz::math: right-handed, clip-space depth 0..1 (Vulkan). NOTE: unlike `math::perspective` in Math.hpp — which flips [1][1] for the swapchain's inverted Y and is what the renderer feeds the GPU — Projection is the pure math type (no Y flip), so its queries invert cleanly. Header-only, deterministic — unit-tested by round-tripping the constructor inputs back out.

**Types:** `Projection`

### `Quadrature`
<sub>`engine/include/maz/math/Quadrature.hpp`</sub>

Composite trapezoidal rule over [a, b] with n subintervals (clamped to >= 1). O(n) evaluations.

### `Quaternion`
<sub>`engine/include/maz/math/Quaternion.hpp`</sub>

maz::math Quaternion — a Godot-style rotation quaternion, the companion to Transform3D. GLM already has the raw quat type, but gameplay/animation code wants Godot's Quaternion API: build from an axis+angle or from Euler angles, read Euler angles back, rotate a vector (xform), compose with `*`, slerp between orientations, and query dot / length / angle_to / inverse. Euler conversion uses Godot's exact YXZ convention (from_euler / get_euler) so orientations authored in Godot import identically. Header-only, pure, deterministic; convert to/from a 3x3 basis with toMat3 / fromMat3.  Storage is a normalized-on-demand GLM quat (w,x,y,z). Unit-tested by round-tripping Euler angles, composing rotations, and comparing xform against explicitly composed Y*X*Z elementary rotations.

**Types:** `Quaternion`

### `QuaternionAverage`
<sub>`engine/include/maz/math/QuaternionAverage.hpp`</sub>

maz::math quaternion averaging — the correct "mean rotation" of a set of orientations. You cannot average rotations by averaging their components and re-normalising (that biases toward whichever hemisphere the signs happen to land in and breaks entirely for spread-out rotations). The principled answer (Markley et al. 2007) is the dominant eigenvector of the 4x4 matrix M = sum w_i q_i q_i^T — which this computes by power iteration. Because each term q q^T is identical for q and -q, the result is correctly insensitive to quaternion double-cover sign. Uses: blending several bone/IK orientation targets, smoothing a noisy tracked orientation over a window, fusing orientation sensors, computing a representative rotation for an LOD or a cluster. Godot's Quaternion has slerp but no averaging. Header-only, std-only, deterministic.

**Functions:**

- `inline void jacobiDominant4(double A[4][4], double out[4])`
- `inline quat averageQuaternions(const std::vector<quat>& qs, const std::vector<float>& weights =`

### `QuaternionSquad`
<sub>`engine/include/maz/math/QuaternionSquad.hpp`</sub>

maz::math SQUAD — Spherical-and-QUADrangle quaternion interpolation (Shoemake, 1987).  Quaternion::slerp already blends BETWEEN TWO orientations along the shortest arc — the rotation analog of a straight line. But playing a sequence of orientation keyframes with slerp gives a path that is only C0: it snaps direction at every keyframe (angular velocity jumps), so a camera or bone visibly "ticks" as it passes each key. SQUAD is the rotation analog of a cubic spline: it threads a SMOOTH (C1-continuous) curve through a list of orientation keyframes, so angular velocity is continuous and the motion glides. This is what cinematic camera rigs and skeletal animation use for rotation tracks.  The construction needs the quaternion exponential map: quatLog turns a unit quaternion into its tangent (a pure quaternion), quatExp turns a tangent back into a rotation. From those, each keyframe gets an "inner" control quaternion s_i = q_i * exp(-(log(q_i^-1 q_{i-1}) + log(q_i^-1 q_{i+1}))/4), and a segment is squad(q0,q1,s0,s1,t) = slerp(slerp(q0,q1,t), slerp(s0,s1,t), 2t(1-t)). Adjacent segments share the boundary control, which is exactly what makes the join C1. Endpoints are hit exactly (t=0 -> q0, t=1 -> q1) regardless of the controls. Header-only, deterministic — unit-tested for exact endpoints, unit length, the degenerate all-equal case, and finite-difference C1 continuity.

**Functions:**

- `inline Quaternion quatLog(const Quaternion& a)`
- `inline Quaternion quatExp(const Quaternion& a)`
- `inline Quaternion squadIntermediate(const Quaternion& prev, const Quaternion& curr,`
- `inline Quaternion squad(const Quaternion& q0, const Quaternion& q1, const Quaternion& s0,`
- `inline Quaternion squadSegment(const Quaternion& prev, const Quaternion& q0, const Quaternion& q1,`

### `QuaternionSwingTwist`
<sub>`engine/include/maz/math/QuaternionSwingTwist.hpp`</sub>

maz::math swing-twist decomposition + nlerp — rotation-math the Quaternion type did not yet have. Swing-twist splits any rotation into a "twist" about a chosen axis and a "swing" perpendicular to it, so that q == swing * twist. It is the standard tool for joint limits: a shoulder or knuckle can twist freely about the bone while its swing (cone) is clamped, and each part is limited independently after decomposing. nlerp is normalized linear interpolation — cheaper than slerp and torque-minimal for the small-angle blends animation code does every frame (Godot exposes slerp/slerpni but neither swing-twist nor nlerp). Header-only, deterministic, reuses the existing Quaternion type.

**Types:** `SwingTwist`

**Functions:**

- `inline Quaternion nlerp(const Quaternion& a, const Quaternion& b, float t)`
- `inline SwingTwist swingTwist(const Quaternion& rot, const vec3& axis)`

### `Ransac`
<sub>`engine/include/maz/math/Ransac.hpp`</sub>

maz::math RANSAC line fitting — fit a line to points that contain GROSS OUTLIERS, robustly. Ordinary least-squares (and the total-least-squares plane/circle fits) assume every point belongs to the shape, so a handful of stray points — a mistracked feature, a sensor glitch, a wall behind the floor — drags the fit badly off. RANSAC (RANdom SAmple Consensus) instead guesses many candidate lines from tiny random samples, keeps the one the most points AGREE with (the "consensus" / inliers), and refits only to those — so outliers are ignored rather than averaged in. It is the standard tool for fitting to noisy real-world point sets: aligning scanned edges, snapping a wall/floor line out of messy depth points, robust trajectory or trend estimation, calibration with bad samples. Deterministic (seeded), header-only, std-only. Godot ships no robust estimator.

**Types:** `RansacLine`

**Functions:**

- `inline void ransacTlsLine(const std::vector<vec2>& pts, const std::vector<int>& idx, vec2& centroid, vec2& dir)`
- `inline RansacLine ransacLine(const std::vector<vec2>& pts, float threshold, int iterations = 200,`

### `RayCapsule`
<sub>`engine/include/maz/math/RayCapsule.hpp`</sub>

maz::math ray vs capsule — the hitscan / picking test against a capsule (a line segment "swept" by a sphere of radius r: every point within distance r of the segment a→b), returning the hit distance, the world hit point AND the surface normal. The capsule is the workhorse collider for characters, limbs, pills and rounded pipes; this is the general ray query you shoot at them or use to pick them in an editor. It tests the CYLINDRICAL SIDE (a quadratic on the ray projected perpendicular to the axis, clamped to the segment's length) and the two HEMISPHERICAL end caps (the spheres at a and b, each restricted to its own hemisphere so the caps meet the side exactly with no double surface), and returns the nearest forward hit with the correct outward unit normal — radial on the side, and (p − nearest-endpoint)/r on a cap, which is exactly normalize(p − closestPointOnSegment). Godot's ray query lives inside its physics server and needs a live body; this is a free-standing vec3 helper. Deterministic, header-only, std-only.

**Types:** `CapsuleHit`

**Functions:**

- `inline CapsuleHit rayIntersectsCapsule(const vec3& from, const vec3& dir, const vec3& a, const vec3& b,`

### `RayCone`
<sub>`engine/include/maz/math/RayCone.hpp`</sub>

maz::math ray vs finite (capped) right circular cone — the hitscan / picking test against a cone given by its apex, axis direction, half-angle and height, returning the hit distance, world point AND outward surface normal. Cones show up as spotlight/flashlight volumes, particle-emitter cones, funnels, horns, wizard hats, drill tips and AI vision volumes; picking or shooting at them (or clipping against a spotlight gizmo in an editor) needs exactly this query, which Godot does not expose. It solves the quadratic for the single cone nappe (clamped to the height so the infinite mirror-cone behind the apex is excluded) and also tests the circular base cap, returning the nearest forward hit. Pure vec3 math, deterministic, header-only.

**Types:** `ConeHit`

**Functions:**

- `inline ConeHit rayIntersectsCone(const vec3& from, const vec3& dir, const vec3& apex, const vec3& axis,`

### `RayCylinder`
<sub>`engine/include/maz/math/RayCylinder.hpp`</sub>

maz::math ray vs finite capped cylinder — the hitscan / picking test against a cylinder with an ARBITRARY axis and position, returning the hit distance, the world hit point AND the surface normal. The engine's Geometry3D.segmentIntersectsCylinder only handles a segment against an origin-centred, Y-aligned cylinder and returns just a point; this is the general ray query you need to shoot at pillars, tree trunks, barrels, pipes and cylindrical colliders, or to pick them in an editor. It tests the curved side (a quadratic on the ray projected perpendicular to the axis, clamped to the cylinder's length) and both end caps, and returns the nearest forward hit with the correct outward normal (radial on the side, ±axis on a cap). Godot exposes no such helper. Pure vec3 math, deterministic, header-only.

**Types:** `CylinderHit`

**Functions:**

- `inline CylinderHit rayIntersectsCylinder(const vec3& from, const vec3& dir, const vec3& base,`

### `RayEllipsoid`
<sub>`engine/include/maz/math/RayEllipsoid.hpp`</sub>

maz::math ray vs axis-aligned ellipsoid — the hitscan / picking test against an ellipsoid with independent per-axis radii, returning the hit distance, the world hit point AND the correctly-scaled surface normal. A ray-vs-sphere only handles a uniform radius; real colliders and bounding volumes are often squashed or stretched (a capsule cap, an egg, a flattened blast radius, a stretched planet), which is exactly an ellipsoid. The trick is to warp space so the ellipsoid becomes a unit sphere (divide by the radii), solve the sphere quadratic there, then map the hit back — but the normal must be taken from the implicit gradient (p-c)/radii^2, NOT the naive warped direction, or it comes out wrong on non-uniform radii. Godot exposes no such helper. Pure vec3 math, deterministic, header-only.

**Types:** `EllipsoidHit`

**Functions:**

- `inline EllipsoidHit rayIntersectsEllipsoid(const vec3& from, const vec3& dir, const vec3& center,`

### `RayPlanar`
<sub>`engine/include/maz/math/RayPlanar.hpp`</sub>

maz::math ray vs flat planar shapes — disk, annulus (ring) and oriented rectangle, each an arbitrarily placed and oriented plane patch, returning the hit distance, world point and a normal that faces the ray. These are the pick/hit tests you need for flat things a game puts in 3D space: circular platforms, jump pads and portals (disk), ring pickups / hula-hoops (annulus), floating UI panels, billboards, doors and signboards (oriented rectangle). Godot has no direct ray-vs-disk / ring / oriented-quad helper. Each test intersects the ray with the shape's plane, then does a cheap in-plane containment check. The returned normal always points back toward the ray so lighting/decals read correctly from either side. Pure vec3 math, deterministic, header-only.

**Types:** `PlanarHit`

**Functions:**

- `inline bool rayPlane(const vec3& from, const vec3& dir, const vec3& center, const vec3& normal, float& t,`
- `inline vec3 faceRay(const vec3& normal, const vec3& dir)`
- `inline PlanarHit rayIntersectsDisk(const vec3& from, const vec3& dir, const vec3& center, const vec3& normal,`
- `inline PlanarHit rayIntersectsAnnulus(const vec3& from, const vec3& dir, const vec3& center,`
- `inline PlanarHit rayIntersectsRect(const vec3& from, const vec3& dir, const vec3& center, const vec3& uAxis,`

### `RayTorus`
<sub>`engine/include/maz/math/RayTorus.hpp`</sub>

maz::math ray vs torus (donut) — the hitscan / picking test against a torus with an arbitrary centre and axis, returning the hit distance, world point AND outward surface normal. Tori are rings, donuts, tube loops, portal rims, tyres, halos and orbit bands; shooting or clicking one has no closed-form quadratic answer — it is a genuine QUARTIC in the ray parameter — so most engines (Godot included) simply do not offer it. This transforms the ray into the torus's local frame (axis = local z), builds the quartic (|P|^2 + R^2 - r^2)^2 = 4 R^2 (Px^2 + Py^2), solves it exactly with math::solveQuartic, and returns the nearest forward hit with the correct normal (pointing from the nearest point on the tube's centre circle to the surface point). `R` is the major radius (centre to tube centre), `r` the minor radius (tube thickness). Deterministic, header-only.

**Types:** `TorusHit`

**Functions:**

- `inline TorusHit rayIntersectsTorus(const vec3& from, const vec3& dir, const vec3& center, const vec3& axis,`

### `Rbf`
<sub>`engine/include/maz/math/Rbf.hpp`</sub>

maz::math::RbfInterpolator2D — radial basis function interpolation of scattered 2D data. Given a handful of sample points, each with a value (a height, a weight, a colour channel, a displacement), it builds a single smooth field that passes EXACTLY through every sample and interpolates sensibly everywhere in between — no grid required. This is the standard tool for smooth image warping / morphing (pin control points and deform), terrain or influence maps from sparse measurements, scattered colour/weight blending, and smooth "attract toward these anchors" fields. It works by placing a radially-symmetric bump (Gaussian or multiquadric) on each sample and solving a small linear system for the bump weights so the sum hits every target value. Godot has no scattered-data interpolator. Header-only, std-only, deterministic; solves the weight system with Gaussian elimination (partial pivoting) at construction.

**Types:** `RbfInterpolator2D`

**Functions:**

- `inline bool rbfSolve(std::vector<double>& A, std::vector<double>& b, int n)`

### `Rect2`
<sub>`engine/include/maz/math/Rect2.hpp`</sub>

Rect2 — Godot's Rect2. An axis-aligned rectangle given by `position` (the min corner) + `size`, with the full set of geometric operations that UI layout, view/camera culling, tilemap regions, and broadphase queries all reach for: point and overlap tests, intersection (clip), union (merge), containment (encloses), per-side grow/shrink, and expand-to-include-a-point. By convention (matching Godot) the right/bottom edges are EXCLUSIVE for point tests, and the operations assume a non-negative size — call abs() first if a size may be negative. Header-only, pure math, deterministic.

**Types:** `Rect2`

### `Rect2i`
<sub>`engine/include/maz/math/Rect2i.hpp`</sub>

Rect2i — Godot's Rect2i: an axis-aligned rectangle in INTEGER coordinates (position = min corner + size), the whole-number companion to Rect2. Tile regions, texture-atlas sub-rects, pixel windows, and grid selections are all naturally integer, so this keeps them exact (no float drift on edges). Same API as Rect2 — hasPoint / intersects / intersection / merge / encloses / grow / expand / abs — with Godot's half-open convention (left/top inclusive, right/bottom exclusive). Header-only, pure.

**Types:** `Rect2i`

### `Reuleaux`
<sub>`engine/include/maz/math/Reuleaux.hpp`</sub>

maz::math Reuleaux polygon — a curve of CONSTANT WIDTH: no matter which direction you measure it, the distance between the two parallel lines that just touch it is the same, exactly like a circle (but it is not a circle). Built from a regular polygon with an ODD number of vertices by replacing every edge with a circular arc centred on the OPPOSITE vertex. The Reuleaux triangle (3 sides) is the guitar-pick / Wankel- rotor shape and the reason some manhole covers can't fall through their hole; higher odd counts (5, 7, …) give rounder constant-width shapes (the shape of the UK 20p/50p coins). Use it for distinctive procedural sprites/icons, rollers, cams and mechanisms, and gameplay props. Godot has no constant-width primitive. Returns a closed CCW polyline ready for the polygon fill / triangulator. Header-only, std-only, deterministic.

**Functions:**

- `inline std::vector<vec2> reuleauxPolygon(int sides, float width, const vec2& center = vec2(0.0f, 0.0f),`

### `RootFind`
<sub>`engine/include/maz/math/RootFind.hpp`</sub>

**Types:** `RootResult`

### `RotationMinimizingFrame`
<sub>`engine/include/maz/math/RotationMinimizingFrame.hpp`</sub>

maz::math rotation-minimizing frames (RMF) along a curve — a stable "which way is up" at every point.  To extrude a tube or ribbon along a path, sweep a cross-section, sway a camera down a spline, or place rungs on a twisting ladder, you need an orthonormal frame (tangent + two perpendicular axes) at each point. The textbook Frenet frame is unusable in practice: it is undefined on straight sections and FLIPS 180° at inflection points, so a tube built on it kinks and turns inside-out. A rotation- minimizing frame instead carries the previous frame forward with the LEAST possible twist about the tangent, giving a smooth, non-flipping sweep. This uses Wang et al.'s (2008) double-reflection method: two reflections transport the reference axis from one sample to the next, exactly and cheaply. Pure vec3 math, header-only, deterministic — unit-tested for orthonormality, no rotation on a straight line, a constant bi-normal on a planar curve, and stability where a Frenet frame would flip.

**Types:** `Frame`

**Functions:**

- `inline float rmfDot(const vec3& a, const vec3& b)`
- `inline vec3 rmfReflect(const vec3& x, const vec3& axis, float c)`
- `inline Frame advanceRMF(const Frame& f, const vec3& x0, const vec3& x1, const vec3& t1)`
- `inline std::vector<Frame> rotationMinimizingFrames(const std::vector<vec3>& points,`

### `Roulette`
<sub>`engine/include/maz/math/Roulette.hpp`</sub>

maz::math roulette curves — the family of curves traced by a point attached to a circle that ROLLS, either along a straight line (trochoid / cycloid) or around another circle (epi-/hypo-trochoid, i.e. the classic "Spirograph" curves). These give you cardioids, astroids, deltoids, gear-tooth flanks, cycloidal gear profiles, spirograph rosettes and rolling-wheel motion paths from a couple of numbers — none of which Godot offers as a primitive. Conventions: `r` is the rolling circle's radius, `R` the fixed circle's radius, `d` the distance of the traced point from the rolling circle's centre (d = r → the point is on the rim, giving a cycloid/epicycloid/hypocycloid; d < r "curtate", d > r "prolate"), and `t` the roll angle in radians. Header-only, std-only, deterministic.

**Functions:**

- `inline vec2 trochoidPoint(float r, float d, float t)`
- `inline vec2 cycloidPoint(float r, float t)`
- `inline vec2 epitrochoidPoint(float R, float r, float d, float t)`
- `inline vec2 epicycloidPoint(float R, float r, float t)`
- `inline vec2 hypotrochoidPoint(float R, float r, float d, float t)`
- `inline vec2 hypocycloidPoint(float R, float r, float t)`
- `inline std::vector<vec2> trochoidPolyline(float r, float d, float tStart, float tEnd, int samples)`
- `inline std::vector<vec2> epitrochoidPolyline(float R, float r, float d, float tStart, float tEnd,`
- `inline std::vector<vec2> hypotrochoidPolyline(float R, float r, float d, float tStart, float tEnd,`

### `Sampling`
<sub>`engine/include/maz/math/Sampling.hpp`</sub>

maz::math Monte-Carlo sampling warps — map a pair of uniform [0,1) random numbers onto a disk, triangle, sphere or hemisphere with the CORRECT distribution. These are the building blocks of every stochastic rendering / simulation task the engine does on the CPU: scattering rays for a GI/AO bake, sampling an area light or an environment, jittering a lens for depth-of-field / bokeh, emitting particles uniformly over a surface, or blue-noise-ish placement. Getting the warp right matters — a naive (r=u, theta=2*pi*v) disk clumps points at the centre; the concentric and sqrt maps here are area-uniform, and the cosine-hemisphere map concentrates samples toward the pole exactly as importance sampling a Lambertian surface needs. Godot exposes none of these. Deterministic, header-only, std-only. (+Z is "up" for the hemisphere maps.)

**Functions:**

- `inline vec2 sampleConcentricDisk(float u1, float u2)`
- `inline vec2 sampleUniformDisk(float u1, float u2)`
- `inline vec2 sampleUniformTriangle(float u1, float u2)`
- `inline vec3 sampleCosineHemisphere(float u1, float u2)`
- `inline vec3 sampleUniformHemisphere(float u1, float u2)`
- `inline vec3 sampleUniformSphere(float u1, float u2)`

### `SavitzkyGolay`
<sub>`engine/include/maz/math/SavitzkyGolay.hpp`</sub>

maz::math Savitzky–Golay smoothing — denoise a 1-D signal by fitting a low-degree polynomial to a sliding window of samples (least squares) and taking the fitted value at each point. Unlike a moving average, which flattens peaks and troughs, an S–G filter PRESERVES the shape of features (peaks, edges, slopes) because a polynomial can follow curvature the box filter cannot — the standard tool for cleaning noisy sensor/telemetry traces, analog-stick or gyro input, audio envelopes, and procedurally generated curves before further processing. Godot offers no such filter. This implementation does a genuine local least-squares fit at every point (including a proper asymmetric fit at the two ends), so any polynomial of degree <= `order` passes through completely unchanged. Header-only, std-only, deterministic.

**Functions:**

- `inline std::vector<float> savitzkyGolay(const std::vector<float>& y, int halfWindow, int order)`

### `Sdf2D`
<sub>`engine/include/maz/math/Sdf2D.hpp`</sub>

maz::math analytic 2D signed distance functions — for each point, the exact distance to a shape's outline, NEGATIVE inside and positive outside. SDFs are the workhorse of crisp procedural 2D: resolution-independent UI shapes and icons, soft/glow/outline effects, dynamic masks, metaballs, 2D soft shadows, and analytic distance-based collision — all from a formula, no texture needed. The engine has a glyph-SDF *baker* (ui::Sdf) and a 3D SDF-CSG set (game::Csg); this is the missing library of exact 2D shape primitives (the Inigo Quilez collection). Shapes are centred at the origin in their own frame — translate/rotate the query point into local space before calling (sdSegment / sdOrientedBox / sdTriangle take explicit points). Godot exposes none of these. Every function returns a true distance field (unit gradient), so results compose with min (union) / max (intersection) / negation (subtraction). Header-only, std-only, deterministic.

**Functions:**

- `inline float len2(const vec2& v)`
- `inline float dot2(const vec2& a, const vec2& b)`
- `inline float clampf(float x, float lo, float hi)`
- `inline float sgn(float x)`
- `inline float sdCircle(const vec2& p, float r)`
- `inline float sdBox(const vec2& p, const vec2& b)`
- `inline float sdRoundedBox(const vec2& p, const vec2& b, float r)`
- `inline float sdSegment(const vec2& p, const vec2& a, const vec2& b)`
- `inline float sdOrientedBox(const vec2& p, const vec2& a, const vec2& b, float thickness)`
- `inline float sdEquilateralTriangle(const vec2& p, float r)`
- `inline float sdTriangle(const vec2& p, const vec2& p0, const vec2& p1, const vec2& p2)`
- `inline float sdHexagon(const vec2& p, float r)`
- _…and 1 more_

### `SdfOps`
<sub>`engine/include/maz/math/SdfOps.hpp`</sub>

maz::math signed-distance combination operators — the composition layer that turns individual distance fields (Sdf2D.hpp, or any distance value) into compound shapes. Hard boolean ops (union/intersect/subtract) come from min/max; the SMOOTH variants blend two shapes with a rounded seam of width `k` — exactly what gives metaballs, soft merges, blobby creatures, welded UI shapes and organic terrain their look. Plus the per-shape modifiers: `round` (fillet every edge by r), `annular` (turn a solid into a hollow shell/outline of thickness 2r), and `interpolate` (morph between two shapes). These are plain float combinators, so they work on 2D or 3D distances alike; game::Csg wraps the 3D field case as std::function, this is the light value-level primitive Godot has no equivalent for. Header-only, std-only, deterministic.

**Functions:**

- `inline float sdfClamp(float x, float lo, float hi)`
- `inline float opUnion(float a, float b)`
- `inline float opIntersect(float a, float b)`
- `inline float opSubtract(float a, float b)`
- `inline float opRound(float d, float r)`
- `inline float opAnnular(float d, float r)`
- `inline float opInterpolate(float a, float b, float t)`
- `inline float opSmoothUnion(float a, float b, float k)`
- `inline float opSmoothIntersect(float a, float b, float k)`
- `inline float opSmoothSubtract(float a, float b, float k)`

### `SegmentDistance`
<sub>`engine/include/maz/math/SegmentDistance.hpp`</sub>

maz::math closest points between two 3D line segments — the geometry primitive at the heart of capsule-vs-capsule collision (a capsule is a segment + radius, so two capsules overlap exactly when the closest distance between their spine segments is < the sum of radii), and generally the answer to "how far apart are these two edges, and where?" The engine already has point-vs-segment (2D) and point-vs-triangle (3D) closest-point queries; this fills in segment-vs-segment. It uses Ericson's robust algorithm (Real-Time Collision Detection): parameterize each segment by s,t in [0,1], solve the unconstrained minimum, then clamp into the valid square, handling parallel and degenerate (zero-length) segments without dividing by zero. Pure vec3 math — exactly unit-testable against hand-computed configurations.

**Types:** `SegmentClosest`

**Functions:**

- `inline float clamp01(float v)`
- `inline float dot3(const vec3& a, const vec3& b)`
- `inline SegmentClosest closestBetweenSegments(const vec3& p1, const vec3& q1, const vec3& p2,`
- `inline bool capsulesOverlap(const vec3& a0, const vec3& a1, float ra, const vec3& b0, const vec3& b1,`

### `ShapeFit`
<sub>`engine/include/maz/math/ShapeFit.hpp`</sub>

maz::math geometric primitive fitting — find the CIRCLE (2D) or SPHERE (3D) that best passes through a cloud of measured points. Where the engine's LeastSquares fits a value as a function of x (a line or a polynomial), this fits a round SHAPE to scattered positions: recover the centre and radius of an arc from a few sampled points (gears, dials, turning circles, curved track segments), fit a bounding sphere to a vertex cloud, estimate a planet/orbit radius, or calibrate a circular sensor sweep. It uses the algebraic (Kasa) least-squares form, which linearises the fit so it reduces to a tiny normal-equations solve on the new dense linear solver — exact on clean data, stable and fast on noisy data. Godot exposes no such fit. Header-only, std-only, deterministic.

**Types:** `CircleFit`, `SphereFit`, `PlaneFit`

**Functions:**

- `inline PlaneFit fitPlane(const std::vector<vec3>& pts)`
- `inline CircleFit fitCircle(const std::vector<vec2>& pts)`
- `inline SphereFit fitSphere(const std::vector<vec3>& pts)`

### `SimplifyPolyline`
<sub>`engine/include/maz/math/SimplifyPolyline.hpp`</sub>

maz::math::simplifyPolyline — Ramer-Douglas-Peucker polyline simplification: throw away the points that don't matter. Given a chain of points (a hand-drawn stroke, a GPS/replay track, a traced outline, a pathfinding result), it returns a shorter chain that stays within a chosen tolerance `epsilon` of the original everywhere, keeping only the vertices that carry the shape. It works by keeping the two ends, finding the point farthest from the straight line between them, and — if that point is farther than epsilon — keeping it and recursing on the two halves; points closer than epsilon to their spanning segment are dropped. The result's guarantee: every original point lies within `epsilon` of the simplified polyline. This is the standard tool for path/stroke decimation, network/track compression, and reducing collision polylines; Godot ships no equivalent (Geometry2D has no line simplifier). The engine already has Chaikin *smoothing* (the opposite operation); this is the decimator. Header-only, std-only, deterministic; retained points are a subsequence of the input so no new vertices are ever invented.

**Functions:**

- `inline float pointSegmentDistance(vec2 p, vec2 a, vec2 b)`
- `inline std::vector<std::size_t> simplifyPolylineIndices(const std::vector<vec2>& pts, float epsilon)`
- `inline std::vector<vec2> simplifyPolyline(const std::vector<vec2>& pts, float epsilon)`

### `SolarPosition`
<sub>`engine/include/maz/math/SolarPosition.hpp`</sub>

maz::math astronomical solar position — where the real Sun is in the sky for a given calendar date/time and place on Earth. The existing game::DayNightCycle is only an abstract 0..1 clock with a cosine "elevation"; this computes the ACTUAL solar altitude and azimuth (and a world-space light direction) from a UTC date, latitude and longitude, so a day/night cycle can be geographically and seasonally correct — long low-angle winter sun, short high summer sun, sunrise swinging north of east in June, the works. Uses the standard low-precision solar model (accurate to ~0.01° for the Sun's declination/right-ascension over 1950–2050), which is far better than any game needs. Angles are radians unless a name ends in `Deg`. Godot ships no such helper. Header-only, std-only, deterministic.

**Types:** `SunAngles`

**Functions:**

- `inline long julianDayNumber(int year, int month, int day)`
- `inline double julianDate(int year, int month, int day, double hourUtc)`
- `inline double solarDeclination(double jd)`
- `inline SunAngles sunPosition(double jd, double latitudeDeg, double longitudeDeg)`
- `inline vec3 sunDirection(const SunAngles& a)`

### `SphericalCoords`
<sub>`engine/include/maz/math/SphericalCoords.hpp`</sub>

maz::math spherical coordinates — the conversion every orbit camera, sky sampler, and directional-light widget needs but that raw GLM does not spell: a 3D point as (radius, azimuth, elevation) and back. GLM gives dot/cross/normalize; it has no notion of "put the camera on a sphere around the target at this yaw and pitch" or "which pixel of the sky does this ray hit". Those are exactly the everyday jobs here.  Convention (right-handed, +Y up, matching the engine's Camera3D and MeshUvRadial): * radius    r >= 0            — distance from the origin. * azimuth   theta             — yaw around +Y, measured in the XZ-plane from +Z toward +X. 0 faces +Z. * elevation phi in [-pi/2,pi/2] — pitch up from the XZ-plane toward +Y. +pi/2 is straight up (+Y). So sphericalToCartesian(1, 0, 0) == (0,0,1) [+Z forward], (1, pi/2, 0) == (1,0,0) [+X right], (1, 0, pi/2) == (0,1,0) [+Y up]. cartesianToSpherical is its exact inverse (up to the r==0 / pole degeneracy where azimuth is arbitrary). Header-only, pure, deterministic.

**Types:** `Spherical`

**Functions:**

- `inline vec3 sphericalToCartesian(float radius, float azimuth, float elevation)`
- `inline vec3 sphericalToCartesian(const Spherical& s)`
- `inline Spherical cartesianToSpherical(const vec3& p)`
- `inline vec3 orbitPosition(const vec3& target, float radius, float azimuth, float elevation)`
- `inline vec2 directionToEquirectUV(const vec3& dir)`
- `inline vec3 equirectUVToDirection(float u, float v)`

### `SphericalHarmonics`
<sub>`engine/include/maz/math/SphericalHarmonics.hpp`</sub>

maz::math order-2 spherical harmonics (9 coefficients) for ambient / diffuse irradiance — the compact "light probe" representation Godot bakes into LightmapGI and uses for ambient lighting. Instead of storing a full environment cubemap, an entire low-frequency lighting environment is captured in 9 RGB numbers per probe: cheap to store, cheap to evaluate, and smooth to interpolate between probes. You project directional radiance samples (from a captured environment, a sky model, or point lights) into an `ShL2` with `addSample`, then reconstruct the diffuse irradiance arriving on any surface normal with `irradiance`, which applies the standard clamped-cosine (Lambert) convolution — the same band scaling (A0 = pi, A1 = 2pi/3, A2 = pi/4) Godot and the Ramamoorthi/Hanrahan formulation use. Pure math, no GPU: projecting and evaluating are fully unit-testable here (a lit frame from the result is the GPU's job).  Scope note (honest): order-2 (L2, 9 coefficients) real SH with the cosine-lobe irradiance convolution. Rotation of an SH set and windowing/deringing are documented follow-ups.

**Types:** `ShL2`

**Functions:**

- `inline std::array<float, 9> shBasis(const vec3& d)`
- `inline vec3 shIrradiance(const ShL2& sh, const vec3& n)`

### `SphericalTriangle`
<sub>`engine/include/maz/math/SphericalTriangle.hpp`</sub>

maz::math spherical triangle — the area (equivalently, the SOLID ANGLE) of a triangle drawn on a sphere from three directions/vertices. This is what you need for the solid angle a triangle light or window subtends at a point (form factors, importance sampling, soft shadows), the fraction of the sky/globe a region covers, geodesic-dome face areas, and spherical coverage tests — none of which Godot exposes. A flat triangle's angles sum to π; a spherical one's sum EXCEEDS π, and that excess IS the area (Girard's theorem). The area is computed with the numerically-robust Van Oosterom–Strackee formula directly from the three unit vectors. All inputs are treated as unit directions from the sphere centre. Header-only, std-only, deterministic.

**Functions:**

- `inline vec3 cross3(const vec3& u, const vec3& v)`
- `inline float sphericalTriangleArea(const vec3& a, const vec3& b, const vec3& c)`
- `inline float sphericalTriangleAngle(const vec3& at, const vec3& u, const vec3& v)`
- `inline float sphericalExcess(const vec3& a, const vec3& b, const vec3& c)`
- `inline float sphericalPolygonArea(const std::vector<vec3>& verts)`

### `Statistics`
<sub>`engine/include/maz/math/Statistics.hpp`</sub>

Sum of all samples (0 for an empty set).

**Functions:**

- `inline double sum(const std::vector<float>& v)`
- `inline double mean(const std::vector<float>& v)`
- `inline double variance(const std::vector<float>& v, bool sample = true)`
- `inline double standardDeviation(const std::vector<float>& v, bool sample = true)`
- `inline double quantile(std::vector<float> v, double q)`
- `inline double median(const std::vector<float>& v)`
- `inline double minValue(const std::vector<float>& v)`
- `inline double maxValue(const std::vector<float>& v)`
- `inline double range(const std::vector<float>& v)`
- `inline double covariance(const std::vector<float>& x, const std::vector<float>& y, bool sample = true)`
- `inline double correlation(const std::vector<float>& x, const std::vector<float>& y)`

### `Superellipse`
<sub>`engine/include/maz/math/Superellipse.hpp`</sub>

maz::math superellipse / squircle — the Lamé curve |x/a|^n + |y/b|^n = 1, a one-parameter family that morphs smoothly from a pinched astroid (n<1), through the ellipse (n=2), to a rounded "squircle" (n=4, the iOS-style rounded rectangle) and on toward a sharp rectangle (n→∞). Godot has no superellipse primitive; this fills the gap for smooth rounded-rectangle UI panels, organic blob shapes, camera/motion easing regions and procedural authoring. The parametric form used here places every returned point EXACTLY on the curve (|x/a|^n + |y/b|^n = 1 to floating-point), so the ring can be fed straight to the polygon fill / triangulator. Pure vec2 math, deterministic, header-only.

**Functions:**

- `inline vec2 superellipsePoint(float a, float b, float n, float t)`
- `inline std::vector<vec2> superellipsePolyline(float a, float b, float n, int segments)`
- `inline std::vector<vec2> squircle(float r, int segments)`
- `inline bool superellipseContains(float a, float b, float n, const vec2& p)`

### `Superformula`
<sub>`engine/include/maz/math/Superformula.hpp`</sub>

maz::math Gielis superformula — a single polar equation that produces an enormous family of natural-looking closed shapes: circles, superellipses, polygons, stars, flowers, starfish, snowflakes and diatom outlines, all from six numbers. It generalises the superellipse (Superellipse.hpp) by adding an m-fold angular term, so it is the go-to procedural generator for organic sprites, petals, gems, shields and shockwave rings — none of which Godot offers as a primitive. The radius at polar angle θ is r(θ) = ( |cos(mθ/4)/a|^n2 + |sin(mθ/4)/b|^n3 ) ^ (-1/n1), where `m` sets the angular symmetry / lobe count, `n1,n2,n3` the "pinch" of the lobes, and `a,b` the axis scales. m = 4, a = b = 1, n1 = n2 = n3 = 2 degenerates to the unit circle; with m = 4 and n1=n2=n3=n it is exactly the unit superellipse |x|^n + |y|^n = 1. Header-only, std-only, deterministic.

**Functions:**

- `inline float superformulaRadius(float m, float n1, float n2, float n3, float a, float b, float theta)`
- `inline vec2 superformulaPoint(const vec2& center, float m, float n1, float n2, float n3, float a, float b,`
- `inline std::vector<vec2> superformulaPolyline(const vec2& center, float m, float n1, float n2, float n3,`

### `Superquadric`
<sub>`engine/include/maz/math/Superquadric.hpp`</sub>

maz::math superquadric (superellipsoid) — the 3D family of shapes that morph between a box, a sphere, a cylinder-ish barrel, a rounded cube, and a double-cone/octahedron by turning two "squareness" knobs. It is the 3D generalisation of the engine's 2D superellipse (Superellipse.hpp): semi-axes (a,b,c) set the size, and two exponents (e1 along the poles, e2 around the equator) set the roundness. Superquadrics are a classic procedural-modelling and shape-fitting primitive (rounded crates, pebbles, capsule-ish hulls, point-cloud fitting). This gives the parametric surface point, the exact surface normal, and the inside-outside function (a scalar that is <1 inside, ==1 on the surface, >1 outside). Godot has no superquadric primitive. Header-only, std-only, deterministic.

**Functions:**

- `inline float signpow(float x, float p)`
- `inline vec3 superellipsoidPoint(float a, float b, float c, float e1, float e2, float u, float v)`
- `inline float superquadricInsideOutside(float a, float b, float c, float e1, float e2, const vec3& p)`
- `inline vec3 superquadricNormal(float a, float b, float c, float e1, float e2, const vec3& p)`

### `SweptSphere`
<sub>`engine/include/maz/math/SweptSphere.hpp`</sub>

maz::math swept-sphere continuous collision — the time-of-impact of a MOVING sphere against a plane. Discrete collision (test the sphere where it lands each frame) tunnels through thin/fast geometry: a bullet or bouncing ball moving faster than its own radius per step can start one side of a wall and end the other with no overlap ever sampled. Continuous collision solves for the exact fraction t of the step at which the sphere first touches the plane, so you can advance to the contact and respond. This is the plane case (walls, floors, ground) — the primitive behind CCD, ball physics, and projectile-vs-surface. The engine has a 2D swept circle (ShapeCast2D) and a conservative 3D sphere cast; this adds the exact analytic 3D sphere-vs-plane sweep (Ericson, Real-Time Collision Detection). Pure vec3 math — exactly unit-testable against hand-computed impact times.

**Types:** `SphereSweepHit`

**Functions:**

- `inline SphereSweepHit sweepSpherePlane(const vec3& c, float r, const vec3& vel, const vec3& n,`

### `TcbSpline`
<sub>`engine/include/maz/math/TcbSpline.hpp`</sub>

maz::math Kochanek-Bartels (TCB) spline — the interpolating keyframe spline with artist controls.  The engine already has centripetal Catmull-Rom (CatmullRomSpline) and the uniform/cubic B-spline (BSpline). Kochanek-Bartels is the animation industry's keyframe spline (3ds Max, Maya, classic game tools): like Catmull-Rom it passes THROUGH every keyframe, but each keyframe carries three knobs an animator dials in per key: * Tension    — how sharply the curve bends through the key (1 = taut/linear, -1 = slack/round). * Continuity — how smoothly it passes through (0 = smooth; ±1 introduces a corner / "snap"). * Bias       — which side the curve leans toward (+1 = overshoots past, -1 = undershoots before). With Tension=Continuity=Bias=0 it is EXACTLY uniform Catmull-Rom — which is the exact cross-check the tests use. It works by deriving an incoming and an outgoing tangent at each key from those three knobs, then Hermite-interpolating each segment. Pure vec2 math, header-only, deterministic.

**Types:** `TcbParams`

**Functions:**

- `inline vec2 tcbTangentOut(const vec2& prev, const vec2& curr, const vec2& next, const TcbParams& p)`
- `inline vec2 tcbTangentIn(const vec2& prev, const vec2& curr, const vec2& next, const TcbParams& p)`
- `inline vec2 hermite(const vec2& p0, const vec2& m0, const vec2& p1, const vec2& m1, float s)`
- `inline vec2 tcbSegment(const vec2& p0, const vec2& p1, const vec2& p2, const vec2& p3, float s,`

### `Tetrahedron`
<sub>`engine/include/maz/math/Tetrahedron.hpp`</sub>

maz::math tetrahedron utilities — signed volume, barycentric coordinates, point-in-tetrahedron and the closest point on/in a tetrahedron. Tetrahedra are the 3D analogue of triangles: they are the cells of volumetric (tet) meshes, the interpolation stencil for scattered 3D data (FEM, fluid/soft-body sims, volumetric fields), the containment primitive for deformation "cages", and the building block GJK/EPA walk through. Barycentric coordinates express any point as a weighted blend of the four corners (weights summing to 1, all non-negative exactly when the point is inside) — the natural way to interpolate a colour, weight or field value stored at the vertices. Godot exposes none of this. Header-only, std-only, deterministic.

**Types:** `BaryTet`

**Functions:**

- `inline vec3 tetCross(const vec3& u, const vec3& v)`
- `inline float sixVolume(const vec3& a, const vec3& b, const vec3& c, const vec3& d)`
- `inline float tetrahedronVolume(const vec3& a, const vec3& b, const vec3& c, const vec3& d)`
- `inline BaryTet barycentricTetrahedron(const vec3& p, const vec3& a, const vec3& b, const vec3& c,`
- `inline bool tetrahedronContains(const vec3& p, const vec3& a, const vec3& b, const vec3& c, const vec3& d,`
- `inline vec3 closestPointTetrahedron(const vec3& p, const vec3& a, const vec3& b, const vec3& c,`

### `Tractrix`
<sub>`engine/include/maz/math/Tractrix.hpp`</sub>

maz::math tractrix — the "drag curve": the path traced by an object on a taut leash of length `a` as the hand holding the other end is dragged along a straight line (the directrix). Its defining property is that the leash is always TANGENT to the curve and always the same length `a` to the drag line — which is exactly how a towed trailer, a dog on a lead, a swinging pendant or a trailing camera-target lags behind a mover. (Spun around its asymptote it also generates the pseudosphere, the constant-negative-curvature surface.) Standard parametrisation, hand dragged along +x, object starting at (0, a): x(t) = a·(t − tanh t),   y(t) = a·sech t = a / cosh t,   t >= 0. Godot has no such curve. Header-only, std-only, deterministic.

**Functions:**

- `inline vec2 tractrixPoint(float a, float t)`
- `inline vec2 tractrixDragPoint(float a, float t)`
- `inline float tractrixArcLength(float a, float t)`
- `inline std::vector<vec2> tractrixPolyline(float a, float tStart, float tEnd, int samples)`

### `Transform2D`
<sub>`engine/include/maz/math/Transform2D.hpp`</sub>

Transform2D — Godot's Transform2D: the 2x3 affine matrix behind every Node2D. It stores two basis columns (`x`, `y`) plus an `origin` translation; a point maps as `x*p.x + y*p.y + origin`. This is the value type the engine's 2D world runs on — placing/parenting sprites, converting between local and world/screen space (xform / xformInv), composing a parent's transform with a child's (operator*), and reading back a node's rotation / scale / skew. It complements `scene::TransformGraph` (a hierarchy of decomposed TRS nodes): this is the flat matrix those nodes ultimately bake to. Pure math, header-only, deterministic — it unit-tests exactly and drives a golden (a shape drawn under several transforms).

**Types:** `Transform2D`

### `Transform3D`
<sub>`engine/include/maz/math/Transform3D.hpp`</sub>

maz::math Transform3D — the core spatial transform Godot builds every 3D node on: a 3x3 `basis` (rotation + scale + shear, columns = the transformed X/Y/Z axes) plus a `vec3 origin`. The engine renders with GLM mat4s, but gameplay/tools code wants Godot's ergonomic API — xform / xform_inv, affine_inverse, compose with `*`, translated / rotated / scaled (global and _local variants), looking_at, and interpolate_with (translation lerp + rotation slerp + scale lerp). This is that type, semantics matched to Godot's Transform3D/Basis source. Header-only, pure, deterministic; convert to/from the renderer's mat4 with toMat4 / fromMat4. Unit-tested to the bit.  Convention note (matches Godot): looking_at aims the -Z axis at the target (Godot's "forward"), and xform_inv / inverse take the fast orthonormal path (transpose) — use affineInverse when the basis carries scale or shear.

**Types:** `Transform3D`

### `Vector4`
<sub>`engine/include/maz/math/Vector4.hpp`</sub>

maz::math 4D vectors — Godot's Vector4 / Vector4i. The float Vector4 carries the everyday API (length / normalized / dot / lerp / abs / sign / clamp / min / max / floor / ceil / round / snapped / distanceTo / directionTo / isEqualApprox), the same surface vec2/vec3 gameplay code already relies on, extended to four components — RGBA colour math, shader-uniform packing, homogeneous points, quaternion storage, and 4-wide data. Vector4i is the whole-number companion (grid/index math with truncating integer division, exact 64-bit lengthSquared, no float drift). Both are header-only, pure and exact — unit-tested component by component. A `toVec4()` bridges to GLM for the rendering path.

**Types:** `Vector4`, `Vector4i`

**Functions:**

- `inline int isigni4(int v)`
- `inline float fsignf4(float v)`

### `VectorInt`
<sub>`engine/include/maz/math/VectorInt.hpp`</sub>

maz::math integer vectors — Godot's Vector2i / Vector3i, the whole-number companions to vec2/vec3. Grid and tile coordinates, array indices, pixel sizes, and hash keys all want exact integer math, not floats that drift. These give the Godot API: component/scalar arithmetic (integer division, truncating like Godot), abs / sign, component-wise clamp / min / max, length (as a double) and exact 64-bit lengthSquared (no overflow), distance queries, aspect (2i), and float-vec conversion. Header-only, pure, exact — unit-tested to the integer.

**Types:** `Vector2i`, `Vector3i`

**Functions:**

- `inline int isigni(int v)`
- `inline int isnappedi(int v, int step)`

### `VectorOps`
<sub>`engine/include/maz/math/VectorOps.hpp`</sub>

maz::math vector helpers — the everyday Vector2 / Vector3 methods Godot game code reaches for constantly but that raw GLM does not spell the same way: move_toward, slide / bounce / reflect (wall sliding + projectile ricochet), limit_length, direction_to, angle_to, posmod, snapped, rotated, project, and component-wise lerp / clamp. Each mirrors Godot's exact semantics (verified against Godot's Vector2/Vector3 source), so gameplay logic ported from GDScript behaves identically. Header-only, pure, branch-for-branch deterministic — unit-tested to the bit.  A note on reflect vs bounce, because Godot's convention trips people up: Godot's `reflect(n)` mirrors the vector ABOUT the normal direction (2*n*(v·n) - v), while `bounce(n)` is the physical "ricochet off a surface with normal n" (v - 2*n*(v·n)) — i.e. bounce == -reflect. We keep Godot's spelling so ported code matches. All functions taking a normal `n` assume it is unit length.

**Functions:**

- `inline bool isFinite(const vec2& v)`
- `inline bool isFinite(const vec3& v)`
- `inline bool isEqualApprox(const vec2& a, const vec2& b)`
- `inline bool isEqualApprox(const vec3& a, const vec3& b)`
- `inline bool isZeroApprox(const vec2& v)`
- `inline bool isZeroApprox(const vec3& v)`
- `inline bool isNormalized(const vec2& v)`
- `inline bool isNormalized(const vec3& v)`
- `inline float fposmod(float x, float y)`
- `inline float snappedf(float x, float step)`
- `inline T cubicInterpolate(const T& from, const T& to, const T& pre, const T& post, float w)`
- `inline T cubicInterpolateInTime(const T& from, const T& to, const T& pre, const T& post, float w,`
- _…and 64 more_

### `Voronoi`
<sub>`engine/include/maz/math/Voronoi.hpp`</sub>

maz::math Voronoi diagram — the dual of Delaunay (M257). Each input "site" gets the region of the plane closer to it than to any other site; clipped to a bounding box you get one convex polygon per site. Voronoi cells drive region maps, procedural biome/territory generation, influence maps, and point-nearest queries. Godot ships no built-in Voronoi, so this is a beyond-parity extra. Computed robustly by half-plane intersection (Sutherland-Hodgman clip the box by every perpendicular bisector) rather than a fragile sweep — O(n^2) but exact and branch-simple. Header-only, pure, deterministic; unit-tests exactly (cells partition the box, each site sits in its own cell).

**Functions:**

- `inline double polyArea2(const std::vector<vec2>& p)`
- `inline std::vector<vec2> clipHalfPlane(const std::vector<vec2>& poly, vec2 anchor, vec2 normal)`
- `inline std::vector<std::vector<vec2>> voronoiCells(const std::vector<vec2>& sites, vec2 boundsMin,`
- `inline float cellArea(const std::vector<vec2>& cell)`

### `Wavelet`
<sub>`engine/include/maz/math/Wavelet.hpp`</sub>

maz::math Haar wavelet transform — the simplest multi-resolution transform: it repeatedly splits a signal (or image) into a coarse "average" half and a fine "detail" half, so a texture or heightfield becomes a small blurry thumbnail plus a stack of ever-finer correction layers. That decomposition is the backbone of progressive/streamed loading (show the thumbnail, refine as detail arrives), level-of-detail, and lossy compression (most detail coefficients are tiny — zero the small ones and the picture barely changes). This is the normalised (orthonormal) Haar basis, so it PRESERVES ENERGY exactly and inverts perfectly. Works on power-of-two 1D arrays and square power-of-two 2D grids, decomposing to the deepest level. Godot ships no wavelet transform. Header-only, std-only, deterministic.

**Functions:**

- `inline bool isPow2(std::size_t n)`
- `inline void haarStepFwd(std::vector<float>& g, std::size_t start, std::size_t stride, std::size_t size)`
- `inline void haarStepInv(std::vector<float>& g, std::size_t start, std::size_t stride, std::size_t size)`
- `inline std::vector<float> haarForward1D(const std::vector<float>& signal)`
- `inline std::vector<float> haarInverse1D(const std::vector<float>& coeff)`
- `inline std::vector<float> haarForward2D(const std::vector<float>& grid, std::size_t n)`
- `inline std::vector<float> haarInverse2D(const std::vector<float>& coeff, std::size_t n)`

### `WindingNumber`
<sub>`engine/include/maz/math/WindingNumber.hpp`</sub>

maz::math generalized winding number — decide whether a point is INSIDE a closed triangle mesh, robustly. "Is this point inside the volume?" is the query behind spawning objects inside an arbitrary shape, containment/region tests, voxelizing a solid, inside/outside masks for particle or fluid collision, and point-in-lava/point-in-water gameplay checks. The naive way (cast a ray and count surface crossings) is brittle: one missing triangle, a T-junction, or a ray grazing an edge flips the answer. The generalized winding number sums the solid angle each triangle subtends at the point (Van Oosterom-Strackee), giving ~+/-1 for interior points and ~0 for exterior ones, and — crucially — it DEGRADES GRACEFULLY: a mesh with small holes still reads ~1 inside, where ray parity would leak. MeshVoxelize's own note calls for exactly this. Winding sign follows triangle orientation, so pointInMesh takes the magnitude and is orientation -agnostic. Header-only, std-only, deterministic; O(triangles) per query (fine for offline bakes / probes).

**Functions:**

- `inline float windingNumber(const std::vector<vec3>& verts, const std::vector<std::uint32_t>& indices,`
- `inline bool pointInMesh(const std::vector<vec3>& verts, const std::vector<std::uint32_t>& indices,`


<a name="platform"></a>
## Platform — window, input, filesystem, crash handling

### `AppFocus`
<sub>`engine/include/maz/platform/AppFocus.hpp`</sub>

maz::platform app-focus policy — what the engine should do each frame when the window loses keyboard focus or is minimized. A real engine doesn't keep burning a CPU core (and the laptop battery, and the GPU) rendering at full speed to a window nobody is looking at; it throttles the frame rate, optionally pauses the simulation, and skips rendering entirely while minimized (there is no visible surface to draw to). Godot exposes this via project settings (run/pause when unfocused, low-processor mode); here it's a small, explicit, deterministic policy.  The SDL side (tracking focus/minimize from window events) lives in platform::Window; this header is the PURE decision — given the current activation state and a policy, decide whether to advance the sim, whether to render, and how long to sleep after the frame to hit the background frame rate. Pure and header-only, so it is unit-tested without a window or a GPU.

**Types:** `WindowActivation`, `FocusPolicy`, `FrameAction`

**Functions:**

- `inline double frameMsForFps(double fps)`
- `inline FrameAction decideFrame(const WindowActivation& act, const FocusPolicy& policy)`

### `Clipboard`
<sub>`engine/include/maz/platform/Clipboard.hpp`</sub>

maz::platform system clipboard — read/write the OS clipboard as UTF-8 text (SDL3-backed). This is what lets a text field do copy/cut/paste against other applications (Godot's DisplayServer clipboard_set/get). Thin by nature; implemented in Window.cpp where SDL is already linked. Safe to call before/without a window (returns empty / does nothing) so headless code never crashes.

### `CrashHandler`
<sub>`engine/include/maz/platform/CrashHandler.hpp`</sub>

maz::platform::CrashHandler — a last-resort crash reporter, Maz's answer to Godot's CrashHandler. When the process hits a fatal signal (SIGSEGV / SIGABRT / SIGFPE / SIGILL / SIGBUS) it prints a labelled banner and a symbolized backtrace to stderr AND to a crash-log file, then restores the default handler and re-raises so the OS can still produce a core dump. That backtrace is often the only clue for a bug that only reproduces on a player's machine — the exact role Godot's handler plays.  Signal handlers may only call async-signal-safe functions, so the crash path deliberately uses raw write() + backtrace_symbols_fd() (both safe) rather than std::string formatting. The rich, std::string-based pieces — demangling a mangled frame into a readable C++ name, naming a signal, capturing the current stack — live as separate free functions used off the crash path (startup diagnostics, tests), so the whole module is verifiable without actually crashing the test runner.  POSIX (Linux/macOS) is fully supported via <execinfo.h>; on other platforms install() is a safe no-op and the helpers degrade gracefully, so engine code can call them unconditionally.

**Types:** `CrashConfig`, `CrashHandler`

**Functions:**

- `inline const char* signalName(int sig)`
- `inline std::string demangleSymbol(const std::string& line)`
- `inline std::vector<std::string> captureBacktrace(int maxFrames = 64, int skip = 1)`
- `inline std::string formatCrashBanner(const std::string& appName, const std::string& version,`

### `DisplayScale`
<sub>`engine/include/maz/platform/DisplayScale.hpp`</sub>

maz::platform HiDPI display scaling — the pure math for turning between LOGICAL coordinates (the size you lay UI out in, e.g. "a 200pt-wide button") and PHYSICAL pixels (what the GPU actually rasterizes). On a HiDPI / Retina display the OS reports a content scale > 1 (1.5, 2.0, …); a window that is 1280×720 logical points is 2560×1440 real pixels at 2.0. Getting this wrong makes UI microscopic on a 4K laptop or blurry when it's up-scaled. Godot handles this via `content_scale_factor` / display DPI; here it's a small, deterministic, unit-tested helper — the SDL side (querying the live scale) lives in platform::Window (`contentScale()`).

**Types:** `PixelSize`

**Functions:**

- `inline float sanitizeScale(float scale)`
- `inline int logicalToPixels(float logical, float scale)`
- `inline float pixelsToLogical(int pixels, float scale)`
- `inline PixelSize scaledSize(int logicalW, int logicalH, float scale)`

### `Displays`
<sub>`engine/include/maz/platform/Displays.hpp`</sub>

maz::platform multi-monitor helpers — the pure geometry behind "which display is this window on?" and "put this window on that monitor." A multi-monitor desktop lays every display out in one virtual coordinate space (monitor 2 might start at x=1920); an OS decides a window's "current" monitor by which display its rectangle overlaps most. Games need this to open on the right screen, go fullscreen on the display the window is on, and center dialogs. Godot exposes it via DisplayServer.get_screen_* ; here the decision logic is a pure, unit-tested function set (the SDL side — enumerating live displays — lives in platform::Window).

**Types:** `DisplayInfo`, `Point2i`

**Functions:**

- `inline int displayContainingPoint(const std::vector<DisplayInfo>& displays, int px, int py)`
- `inline long long overlapArea(const DisplayInfo& d, int x, int y, int w, int h)`
- `inline int displayForRect(const std::vector<DisplayInfo>& displays, int x, int y, int w, int h)`
- `inline Point2i centerRectOnDisplay(const DisplayInfo& d, int w, int h)`

### `Input`
<sub>`engine/include/maz/platform/Input.hpp`</sub>

Gamepad axis/button ids. Values match SDL_GamepadAxis / SDL_GamepadButton ordering, so gameplay code can name inputs semantically without including SDL headers.

**Types:** `Input`

### `Paths`
<sub>`engine/include/maz/platform/Paths.hpp`</sub>

A writable, per-user, per-application directory with `file` appended (created if needed). Wraps SDL_GetPrefPath, e.g. ~/.local/share/<org>/<app>/<file> on Linux. Falls back to the bare filename (current directory) if the platform path can't be resolved.

### `PlatformBackend`
<sub>`engine/include/maz/platform/PlatformBackend.hpp`</sub>

maz::platform per-target backend seam — the single abstraction an engine crosses to reach a NEW platform (a console, a VR headset, a phone) without touching game or renderer code. Every target differs in exactly the same handful of ways: how it boots and tears down, what native surface handle the GPU renderer binds to, where its readable (bundled assets) and writable (save data) directories live, which input sources exist, and whether the OS can suspend/resume the app under you (mobile/console) versus running uninterrupted (desktop). `PlatformBackend` names that seam; a concrete backend implements it for one target. The engine talks only to the interface, so porting to a new platform is "write one backend", which is how Godot/Unity keep one codebase across a dozen devices.  This box can implement + unit-test the HEADLESS backend (pure CPU, no device) and the registry that selects a backend by id — that is verified here. The console/VR/mobile backends are stubs behind the same interface plus the exact human/hardware step to finish each (NDA SDK, physical headset, device + paid dev account), documented in docs/PLATFORMS.md — those can never be marked 100% from this environment.

**Types:** `PlatformId`, `PlatformCaps`, `PlatformBackend`, `HeadlessBackend`, `PlatformRegistry`

**Functions:**

- `inline PlatformRegistry defaultRegistry()`

### `WebLoop`
<sub>`engine/include/maz/platform/WebLoop.hpp`</sub>

maz::platform web/native main-loop driver — the ONE portability seam a desktop engine must cross to run in a browser. On desktop the game loop is an ordinary blocking `while (running) { frame(); }`. In a WebAssembly build that pattern DEADLOCKS: the browser is single-threaded and cooperative, so a C++ loop that never returns starves the event loop and the tab hangs. Emscripten's answer is to hand the browser a per-frame callback (`emscripten_set_main_loop_arg`) and RETURN from main; the browser then calls back once per animation frame. This header hides that split behind a single `runMainLoop(step, user)` so game code is written once: on `__EMSCRIPTEN__` it registers the callback, everywhere else it runs the blocking loop — identical `step` semantics on both. That is what lets the same Maz game target desktop and the web.  Honest tag (see docs/GODOT_GAPS_ROADMAP.md + docs/WEB_BUILD.md): this abstraction + the native path are compiled and unit-tested HERE; producing the actual `.wasm`/`.js` requires the Emscripten toolchain (emcc/em++), which this box does not have — the documented `tools/build_web.sh` runs on a machine that does. [NATIVE PATH VERIFIABLE HERE / WASM NEEDS TOOLCHAIN]

**Types:** `LoopContext`

**Functions:**

- `inline void emscriptenStep(void* ctxPtr)`
- `inline void runMainLoop(MainStepFn step, void* user, int fps = 0)`
- `inline int runMainLoopBounded(MainStepFn step, void* user, int maxIterations)`

### `Window`
<sub>`engine/include/maz/platform/Window.hpp`</sub>

**Types:** `WindowConfig`, `Window`


<a name="render"></a>
## Render — Vulkan renderer, sprites, meshes, shapes, cameras

### `AtlasPacker`
<sub>`engine/include/maz/render/AtlasPacker.hpp`</sub>

Rectangle bin packer for texture atlases — the layout step behind Godot's atlas/sprite-sheet importer and dynamic font glyph caches. Give it a bin of fixed width×height and a set of rectangle sizes; it places each without overlap and reports where (or that it did not fit). It uses the **Skyline Bottom-Left** heuristic (track the upper contour of what's placed; drop each rect at the position whose resulting top is lowest, ties broken to the left), which packs tightly and, crucially, is fully deterministic — same inputs always give the same layout — so it unit-tests exactly and renders a golden-stable atlas. `pack()` height-sorts a batch first (the standard heuristic) while returning placements in the caller's original order.  Honest scope: single-bin, axis-aligned, no rotation and no inter-rect padding (add spacing to your sizes if you need a gutter). It does NOT auto-grow the bin, pack across multiple pages, or do MaxRects / guillotine variants — those remain follow-ups.

**Types:** `PackSize`, `Placement`, `AtlasPacker`

### `BilateralFilter`
<sub>`engine/include/maz/render/BilateralFilter.hpp`</sub>

maz::render::bilateralFilter — edge-preserving smoothing. An ordinary blur (box / Gaussian) averages each pixel with its neighbours regardless of content, so it kills noise but also smears every edge into mush. The bilateral filter weights each neighbour by TWO things: how close it is (spatial) AND how similar its colour is (range). Neighbours across a strong edge have a very different colour, so they get almost no weight — the edge stays crisp while flat regions still get cleaned up. This is the staple behind photo denoise, "beautify"/skin-smoothing, cartoon/stylize preprocessing, and cleaning noisy procedural or baked textures before use. Godot's Image has no bilateral (only whole-image resize/blur-free ops). `spatialSigma` sets the neighbourhood size, `rangeSigma` how different a colour must be to be ignored. Output is a convex blend of the input, so it never overshoots. Header-only, std-only, deterministic.

**Functions:**

- `inline Image bilateralFilter(const Image& src, float spatialSigma, float rangeSigma)`

### `Billboard`
<sub>`engine/include/maz/render/Billboard.hpp`</sub>

Billboarding — Godot's SpriteBase3D / GeometryInstance3D billboard modes. A billboard is a flat quad that turns to face the camera every frame, so a 2D image reads as a 3D object: trees, grass, smoke, health bars, distant-object impostors. This builds the model matrix that orients such a quad. Godot exposes three modes and so does this: `Disabled` (no turning — an ordinary placed quad), `Enabled` (full billboard — the quad's plane always squarely faces the camera), and `YBillboard` (fixed-Y — the quad yaws to face the camera horizontally but stays perfectly upright, the right choice for trees/characters that shouldn't tip back when you look down at them). It reads the camera's basis straight out of the view matrix, so it needs no camera object — just the same `view` you feed the renderer. Header-only, math-only; unit-tests the matrix without a GPU.

**Types:** `BillboardMode`

**Functions:**

- `inline math::mat4 buildBillboard(const math::vec3& position, const math::vec3& scale,`

### `Camera3D`
<sub>`engine/include/maz/render/Camera3D.hpp`</sub>

Camera3D — the projection helper behind Godot's Camera3D: it turns a view + projection matrix (built from a look-at + perspective, or supplied directly) plus a viewport size into the screen↔world queries games lean on every frame: * worldToScreen   — project a 3D world point to 2D screen pixels        (Godot unproject_position) * screenToRay     — unproject a screen pixel to a world-space pick ray  (Godot project_ray_origin/normal) * screenToWorld   — a world point a given distance down that ray        (Godot project_position) * frustum / isPointVisible / isSphereVisible — the six view planes + containment tests (Godot is_position_in_frustum) These drive mouse picking, world-space UI labels / health bars over 3D units, off-screen culling, and look-at aiming. It uses the engine's Vulkan clip convention (`math::perspective`: y-down NDC, depth 0..1), so screen coordinates have a top-left origin and match what the renderer draws. Pure matrix math, header-only, deterministic — it unit-tests exactly and drives a golden via a 2D projection of a 3D scene.

**Types:** `Ray3`, `Projected`, `FrustumPlanes`, `Camera3D`

**Functions:**

- `inline bool frustumIntersectsAabb(const FrustumPlanes& f, math::vec3 boxMin, math::vec3 boxMax)`

### `CascadeSplits`
<sub>`engine/include/maz/render/CascadeSplits.hpp`</sub>

maz::render cascaded shadow-map splits — a single directional-light shadow map can't cover a huge view distance without either blurring near geometry or wasting all its resolution far away. Cascaded Shadow Maps (CSM) fix this by slicing the camera frustum along depth into N cascades, each with its OWN shadow map: the near cascade gets crisp, tight coverage, far cascades cover more world per texel. The key decision is WHERE to cut — the "split distances". This is the standard Practical Split Scheme (Zhang et al., the PSSM `lambda` blend), pure and unit-tested; the actual per-cascade depth passes + shader selection are the GPU half (Godot's DirectionalLight3D shadow "4 splits" is the same idea).

**Types:** `CascadeRange`

**Functions:**

- `inline std::vector<float> cascadeSplits(float nearZ, float farZ, int count, float lambda = 0.5f)`
- `inline std::vector<CascadeRange> cascadeRanges(float nearZ, float farZ, int count,`

### `CatmullClark`
<sub>`engine/include/maz/render/CatmullClark.hpp`</sub>

maz::render Catmull-Clark subdivision — the industry-standard way to turn a blocky, low-polygon "control cage" of QUADS into a smooth, rounded surface, refining it one level at a time toward a limit surface. Where the engine's existing Loop subdivision smooths TRIANGLE meshes, Catmull-Clark works on arbitrary polygon faces and always outputs quads — the scheme film and modelling packages use for organic shapes (a cube becomes a sphere-like blob, a rough character cage becomes a clean subdivision surface). Each pass places one FACE point at every face's centroid, one EDGE point per edge (blending the edge's ends with its two neighbouring face points), and nudges every original vertex toward the average of its surrounding face and edge points; then every face is split into quads around those new points. Boundary edges/vertices use the open cubic-B-spline crease rules so borders stay put. Godot exposes no runtime subdivision surface, so this is a beyond-Godot geometry utility. Header-only, std-only, deterministic.

**Types:** `PolyMesh`, `CcEdge`

**Functions:**

- `inline std::uint64_t ccEdgeKey(std::uint32_t a, std::uint32_t b)`
- `inline PolyMesh catmullClarkOnce(const PolyMesh& in)`
- `inline PolyMesh catmullClark(const PolyMesh& in, int iterations = 1)`

### `CieLab`
<sub>`engine/include/maz/render/CieLab.hpp`</sub>

maz::render CIELAB perceptual colour space + colour-difference (Delta-E).  The engine already has HSV/HSL, hex, and sRGB<->linear (ColorOps.hpp). Those are *device* spaces: equal numeric steps do NOT look like equal perceptual steps, so gradients band, palette reduction picks the wrong "nearest" colour, and "are these two colours the same?" has no honest threshold. CIELAB (CIE L*a*b*, 1976) is a *perceptual* space built on human vision: L* is lightness 0..100, a* is green(-)/red(+), b* is blue(-)/yellow(+), and Euclidean-ish distance tracks how different two colours actually look. This is what professional tools use for accurate gradient interpolation, perceptual colour quantisation, palette matching, and accessibility contrast work.  render::Color here is LINEAR RGB in the sRGB primaries (see ColorOps.hpp), so the pipeline is linear-RGB -> CIE XYZ (D65) -> L*a*b*, with the exact inverse for the round trip. Two difference metrics are provided: deltaE76 (fast Euclidean, CIE 1976) and deltaE2000 (CIEDE2000, the modern perceptual standard with lightness/chroma/hue weighting and the blue-region rotation term). Pure value maths, header-only, deterministic — unit-tested against sRGB identities, round trips, and the published Sharma-Wu-Dalal CIEDE2000 reference pairs.

**Types:** `Xyz`, `Lab`

**Functions:**

- `inline float labF(float t)`
- `inline float labFinv(float t)`
- `inline Xyz linearRgbToXyz(const Color& c)`
- `inline Color xyzToLinearRgb(const Xyz& v, float alpha = 1.0f)`
- `inline Lab xyzToLab(const Xyz& v)`
- `inline Xyz labToXyz(const Lab& lab)`
- `inline Lab toLab(const Color& c)`
- `inline Color fromLab(const Lab& lab, float alpha = 1.0f)`
- `inline float deltaE76(const Lab& p, const Lab& q)`
- `inline float deltaE2000(const Lab& c1, const Lab& c2)`

### `ColladaLoader`
<sub>`engine/include/maz/render/ColladaLoader.hpp`</sub>

maz::render COLLADA (.dae) mesh importer — closes a gap versus Godot, which imports Collada out of the box while Maz previously read only OBJ and glTF. COLLADA is an XML interchange format many DCC tools (Blender, Maya, SketchUp) still export, so supporting it widens what artists can bring in. `parseCollada` reads the text form into a `shapes::MeshData` ready for `Renderer::createMesh`, built on the header-only `io::XmlParser` pull parser (M174). It gathers every `<source>` float array, the `<vertices>` POSITION mapping, and the `<triangles>` / `<polylist>` index streams (with their per-semantic input offsets), then de-interleaves them into position/normal/uv vertices. Polygons with more than three corners are fan-triangulated. It is pure CPU string work — no GPU — so it unit-tests headlessly from an in-memory string; `loadCollada` wraps it for files.  Scope note (honest): this reads the common exported-mesh case — the first `<geometry>`'s triangle or polylist primitives with VERTEX/NORMAL/TEXCOORD inputs. It does not apply node transforms, skinning, materials, multiple geometries, or `<trifans>`/`<tristrips>`; those are documented follow-ups. Malformed or unsupported documents return false rather than throwing.

**Types:** `ColladaLoadOptions`

**Functions:**

- `inline std::vector<float> parseFloatList(const std::string& s)`
- `inline std::vector<long> parseIntList(const std::string& s)`
- `inline std::string stripHash(const std::string& s)`
- `inline bool parseCollada(const std::string& text, shapes::MeshData& out,`
- `inline bool loadCollada(const std::string& path, shapes::MeshData& out,`

### `ColorHarmony`
<sub>`engine/include/maz/render/ColorHarmony.hpp`</sub>

maz::render COLOUR HARMONY — generate coordinated palettes from a single base colour by rotating its hue on the colour wheel: complementary, analogous, triadic, split-complementary, tetradic (square), and monochromatic shades. This is the "pick colours that go together" helper for procedural UI theming, generative art, team/faction colours, and data-viz legends — the tasteful companion to `gradientMap` and `simulateColorVision`. All rotations happen in HSV (hue in [0,1)); saturation, value, and alpha are preserved. Header-only, deterministic, headless.

**Functions:**

- `inline Color rotateHue(const Color& c, float turns)`
- `inline std::vector<Color> complementary(const Color& base)`
- `inline std::vector<Color> analogous(const Color& base, float spread = 1.0f / 12.0f)`
- `inline std::vector<Color> triadic(const Color& base)`
- `inline std::vector<Color> splitComplementary(const Color& base, float spread = 1.0f / 12.0f)`
- `inline std::vector<Color> tetradic(const Color& base)`
- `inline std::vector<Color> monochromatic(const Color& base, int count)`

### `ColorNames`
<sub>`engine/include/maz/render/ColorNames.hpp`</sub>

maz::render named colours — Godot's Color constants (Color.RED, Color.SKY_BLUE, ...) and Color.from_string(). Godot's constant set is the CSS3 / X11 named-colour palette; this reproduces it exactly (byte-for-byte sRGB), so a colour authored by name in Godot resolves identically here. Name lookup is forgiving the way Godot's is: case-insensitive, and underscores/spaces are ignored ("SKY_BLUE", "sky blue" and "skyblue" all match). Header-only, pure, unit-tested against the palette.

**Types:** `NamedColorEntry`

**Functions:**

- `inline const NamedColorEntry* namedColorTable(std::size_t& count)`
- `inline std::string normalizeColorName(const std::string& s)`
- `inline Color fromRgba8888(std::uint32_t v)`
- `inline std::optional<Color> namedColor(const std::string& name)`
- `inline Color colorFromString(const std::string& str, const Color& fallback)`

### `ColorOps`
<sub>`engine/include/maz/render/ColorOps.hpp`</sub>

maz::render colour operations — the maths behind Godot's Color type and ColorPicker. render::Color is a plain linear RGBA float; this adds the conversions and tweaks a picker/theme needs: HSV <-> RGB, hex (#rrggbb / #rrggbbaa) parse+format, lighten/darken, lerp, invert, perceptual luminance, and the sRGB<->linear transfer functions. All pure value maths, header-only, deterministic — unit-tests exactly against known colour identities.

**Types:** `Hsv`, `Hsl`, `Oklab`, `Oklch`, `Okhsl`

**Functions:**

- `inline Color fromHsv(float h, float s, float v, float a = 1.0f)`
- `inline Color fromHsv(const Hsv& hsv)`
- `inline Hsv toHsv(const Color& c)`
- `inline Color fromHsl(float h, float s, float l, float a = 1.0f)`
- `inline Color fromHsl(const Hsl& hsl)`
- `inline Hsl toHsl(const Color& c)`
- `inline int hexNybble(char ch)`
- `inline char hexDigit(int v)`
- `inline bool fromHtml(const std::string& text, Color& out)`
- `inline std::string toHtml(const Color& c, bool withAlpha = false)`
- `inline Color lightened(const Color& c, float amount)`
- `inline Color darkened(const Color& c, float amount)`
- _…and 38 more_

### `ColorQuantize`
<sub>`engine/include/maz/render/ColorQuantize.hpp`</sub>

maz::render color quantization — reduce an arbitrary set of RGB colors down to a small representative palette via the classic MEDIAN-CUT algorithm. Repeatedly split the color box with the widest channel spread at its median along that channel, then average each final box to a palette entry. The tool for retro/indexed-color looks (NES/GameBoy-style palettes), GIF-style export, texture palettization, and "dominant colors of this image" swatches — none of which Godot provides. Works in 0-255 RGB space; pair quantizePalette() with mapToPalette()/nearestPaletteIndex() to remap an image onto the reduced palette. Header-only, std-only, deterministic.

**Types:** `Rgb8`

**Functions:**

- `inline std::size_t nearestPaletteIndex(Rgb8 c, const std::vector<Rgb8>& palette)`
- `inline std::vector<Rgb8> quantizePalette(const std::vector<Rgb8>& pixels, std::size_t maxColors)`
- `inline std::vector<std::size_t> mapToPalette(const std::vector<Rgb8>& pixels,`

### `ColorTemperature`
<sub>`engine/include/maz/render/ColorTemperature.hpp`</sub>

maz::render::kelvinToColor — convert a color temperature in Kelvin to an approximate RGB tint, using the well-known Tanner Helland blackbody fit (valid roughly 1000-40000 K). Low temperatures are warm (candle ~1900K, tungsten ~2700-3200K, orange), ~6500K is neutral daylight white, and high temperatures are cool/blue (overcast/shade ~7000-10000K). The tool for physically-plausible light tints — day/night cycles that shift the sun from dawn-orange to noon-white, lamp/torch/fire glows, and camera white-balance-style grading. Godot has no built-in Kelvin->RGB helper. Header-only, std-only. Returns a Color with components in [0,1] and alpha 1.

**Functions:**

- `inline Color kelvinToColor(float kelvin)`

### `ContrastRatio`
<sub>`engine/include/maz/render/ContrastRatio.hpp`</sub>

maz::render WCAG CONTRAST RATIO — the companion accessibility check to `simulateColorVision`: is text (or an icon) actually legible against its background? Implements the WCAG 2.x relative-luminance + contrast-ratio formula (ratio in [1, 21]) and the pass/fail thresholds for AA and AAA, normal and large text. Use it while theming a UI to guarantee readable HUDs, menus, and subtitles, or to auto-pick the more legible of black/white for a label on a coloured button. Header-only, deterministic, headless — pure colour math.  Scope note (honest): follows the WCAG 2.x definition exactly (sRGB → linear via the standard EOTF, luminance weights 0.2126/0.7152/0.0722, ratio = (Llight+0.05)/(Ldark+0.05)); alpha is ignored (contrast is defined for opaque colours — composite over the real backdrop first if your text is translucent).

**Functions:**

- `inline float relativeLuminance(const Color& c)`
- `inline float contrastRatio(const Color& a, const Color& b)`
- `inline bool passesAA(const Color& fg, const Color& bg, bool largeText = false)`
- `inline bool passesAAA(const Color& fg, const Color& bg, bool largeText = false)`
- `inline Color bestTextColor(const Color& bg, const Color& dark = Color`

### `Cubemap`
<sub>`engine/include/maz/render/Cubemap.hpp`</sub>

maz::render cubemap direction mapping — the sampling math shared by reflection probes, skyboxes, and image-based lighting. A cubemap stores a 360° environment across six square faces; to look one up you convert a 3D direction into "which face + where on it (u,v)", and to bake or debug it you go the other way. This is the standard OpenGL/Vulkan cube mapping (major-axis selection with the conventional per-face s/t axes), so a Maz cubemap matches what artists author elsewhere. Pure math — unit-tested headlessly; a GPU samples the actual texels, and reflection-probe capture is a separate render pass.  Scope note (honest): the direction<->face/uv convention only. It does not allocate or sample real cubemap textures, do seamless edge filtering, or prefilter a mip chain (roughness) — those are the renderer's job.

**Types:** `CubeSample`

**Functions:**

- `inline CubeSample directionToCube(const math::vec3& dir)`
- `inline math::vec3 cubeToDirection(CubeFace face, float u, float v)`

### `CubemapCapture`
<sub>`engine/include/maz/render/CubemapCapture.hpp`</sub>

maz::render reflection-probe / cubemap CAPTURE math — the render-side counterpart to the SAMPLING helpers in Cubemap.hpp (`directionToCube` / `cubeToDirection`). To build a reflection probe or a dynamic environment map, the engine renders the scene SIX times from the probe's position, once down each cube axis (+X,-X,+Y,-Y,+Z,-Z), into the six faces of a cubemap. That needs, per face, a camera VIEW matrix (look direction + up vector) and a 90° field-of-view PROJECTION so the six frustums tile the whole sphere with no gaps or overlap. This header provides exactly those — `cubeFaceView(face, center)` and `cubeFaceProjection(near, far)` — using the same axis convention as the sampler, so a direction that `directionToCube` says belongs to face F really is the face captured by `cubeFaceView(F, ...)`. That consistency is the thing cubemap capture most often gets wrong, and it is exactly what the unit test pins down. Pure matrix math — no GPU — so it verifies headlessly; the actual six render passes run on the GPU.  Honest tag: the view/projection MATRICES are CPU-verified here; issuing the six render passes into a cubemap render target and prefiltering the result for roughness are the GPU steps this feeds.

**Functions:**

- `inline math::mat4 cubeFaceProjection(float zNear = 0.05f, float zFar = 1000.0f)`
- `inline math::mat4 cubeFaceView(CubeFace face, const math::vec3& center)`
- `inline math::mat4 cubeFaceViewProjection(CubeFace face, const math::vec3& center, float zNear = 0.05f,`
- `inline math::vec3 cubeFaceForward(CubeFace face)`

### `Decal`
<sub>`engine/include/maz/render/Decal.hpp`</sub>

maz::render decal projection — the math behind Godot's Decal node: an oriented box that stamps a texture onto whatever surface lies inside it (bullet holes, blood, posters, tire tracks). This is the CPU core: given a decal box and a world-space surface point + normal, `projectDecal` returns the texture UV to sample and a blend alpha, or nothing when the point falls outside the box or the surface faces away from the projector. The box is defined by a center, an orthonormal frame (right / up / forward), and half extents; it projects along its local -up, so the footprint is the right×forward plane and `up` is the projection depth. A normal-fade cutoff rejects surfaces that don't face the projector, matching Godot's `normal_fade`. Pure math — unit-tested headlessly; the actual texture blend happens on the GPU.  Scope note (honest): axis-aligned-in-local-space projection with edge-inclusive bounds and a linear normal fade. It does not do the GPU decal blend, per-decal albedo/emission mixing, or upper/lower depth fade curves; those are the renderer's job. Axes are assumed orthonormal and unit length.

**Types:** `Decal`, `DecalSample`

**Functions:**

- `inline std::optional<DecalSample> projectDecal(const Decal& decal, const math::vec3& point,`

### `Dither`
<sub>`engine/include/maz/render/Dither.hpp`</sub>

maz::render dithering — reduce a grayscale image to a few brightness levels while HIDING the banding that naive quantization produces, by trading spatial noise for tonal accuracy. Two classic methods: ordered (Bayer-matrix) dithering, which adds a fixed, tileable threshold pattern per pixel (the crisp, deterministic look of old console/print art), and Floyd-Steinberg error diffusion, which pushes each pixel's rounding error into its not-yet-processed neighbours (smoother, less patterned). The companion to color quantization (M444) for retro/1-bit/limited-palette looks, e-ink-style output, and stylized post effects — none of which Godot provides. bayerMatrix() also stands alone as a reusable threshold-matrix generator. Header-only, std-only, deterministic.

**Functions:**

- `inline std::vector<int> bayerMatrix(int level)`
- `inline std::vector<std::uint8_t> orderedDitherGray(const std::vector<std::uint8_t>& px, int w, int h,`
- `inline std::vector<std::uint8_t> floydSteinbergGray(const std::vector<std::uint8_t>& px, int w, int h,`

### `Equirect`
<sub>`engine/include/maz/render/Equirect.hpp`</sub>

maz::render — equirectangular (lat-long) panorama mapping: convert a 3D view direction to a (u,v) texture coordinate on a 360x180 panorama image, and back. This is how a single wide photo or HDR sky panorama is wrapped around a scene as a skybox / environment map (Godot's PanoramaSkyMaterial), how reflection lookups read a lat-long environment, and how you sample "what does the world look like in this direction?". The horizontal axis is the compass angle (azimuth) around +Y, the vertical axis is the up/down angle (elevation) from the top pole to the bottom; matching the engine's SphericalCoords convention (+Z forward at u=0.5, +Y up at v=0). The engine has cube maps but no equirectangular sampling. Direction<->UV round- trips exactly (away from the poles, where the seam meridian is arbitrary). Header-only, deterministic.

**Functions:**

- `inline math::vec2 equirectUvFromDir(const math::vec3& dir)`
- `inline math::vec3 dirFromEquirectUv(const math::vec2& uv)`
- `inline Color sampleEquirect(const Image& img, const math::vec3& dir)`

### `FbxLoader`
<sub>`engine/include/maz/render/FbxLoader.hpp`</sub>

maz::render FBX (.fbx) geometry importer — closes the biggest remaining import gap versus Godot's asset pipeline. FBX is the de-facto interchange format out of Maya/3ds Max/Blender/mixamo, so reading even its geometry lets those tools' meshes drop straight into Maz. This importer handles the **ASCII** FBX form: it locates the mesh's `Vertices` array (flat x,y,z triples) and its `PolygonVertexIndex` array (FBX packs each polygon's final index as the bitwise-complement, ~i, i.e. a negative value, to mark the polygon boundary — so polygons of any size are delimited without a separate count), fan-triangulates every polygon, and emits a `shapes::MeshData` (three vertices per triangle, sequential indices) ready for `Renderer::createMesh`. Pure CPU string/number work — no GPU — so it unit-tests headlessly from an in-memory buffer; `loadFbx` wraps it for files.  Scope note (honest): ASCII FBX, geometry of the FIRST mesh only, positions + fan-triangulation. Normals are derived from the winding (FBX's LayerElementNormal mapping/reference modes are not yet decoded), and UVs/materials/skins/animation are out of scope for this milestone. Binary FBX, multi-mesh scenes, and LayerElement normals/UVs are documented follow-ups. Malformed or non-mesh input returns false.

**Types:** `FbxLoadOptions`

**Functions:**

- `inline bool fbxGrabArray(const std::string& s, const std::string& key, std::vector<double>& out)`
- `inline bool parseFbxAscii(const std::string& text, shapes::MeshData& out,`
- `inline bool loadFbx(const std::string& path, shapes::MeshData& out, const FbxLoadOptions& opt =`

### `GlbContainer`
<sub>`engine/include/maz/render/GlbContainer.hpp`</sub>

maz::render GLB (binary glTF) container parser — splits a `.glb` byte buffer into its JSON and BIN chunks WITHOUT a file device or the cgltf dependency. A `.glb` is the single-file, self-contained form of glTF (the `.gltf`+`.bin`+textures packed into one blob) — the format most exporters and asset stores ship. The engine's `loadGltf` reads `.glb` from disk via cgltf, but pulling the two chunks out of an in-MEMORY buffer (say, an entry inside a `io::ResourcePack` or a byte array fetched over the network / WebSocket) needs the container framing itself. That framing is tiny and fixed (a 12-byte header + length-prefixed chunks), so this parses it directly and unit-tests headlessly against a hand-built GLB. Callers hand the extracted JSON to their glTF parser and index the BIN blob for buffer views.  Scope note (honest): the GLB *container* (glTF 2.0, little-endian: header magic/version/length + JSON chunk `JSON` + optional BIN chunk `BIN\0`). Parsing the JSON scene graph and decoding the accessors is the glTF layer on top (`render::loadGltf`); this delivers the two raw chunks it consumes.

**Types:** `GlbChunks`

**Functions:**

- `inline std::uint32_t glbReadU32(const std::uint8_t* p)`
- `inline bool parseGlb(const std::uint8_t* d, std::size_t n, GlbChunks& out)`
- `inline bool parseGlb(const std::vector<std::uint8_t>& bytes, GlbChunks& out)`
- `inline std::vector<std::uint8_t> buildGlb(const std::string& json, const std::vector<std::uint8_t>& bin =`

### `GlobalIllumination`
<sub>`engine/include/maz/render/GlobalIllumination.hpp`</sub>

maz::render one-bounce global-illumination gather — the INDIRECT half of Godot's LightmapGI/SDFGI, the piece the M96 direct-only `bakeLightmap` explicitly left as a follow-up. Direct lighting only tells a surface how much it sees the lights; GI is what makes a red wall bleed pink onto the floor beside it and a shadowed nook still read softly lit from the open sky. This bakes that: for each receiver surfel it fires a cosine-weighted hemisphere of rays; each ray either strikes a scene PATCH (a triangle carrying an emitted+reflected radiance — e.g. the floor's direct-lit color, or a glowing surface) and collects that radiance, or escapes to the SKY and collects the sky color. The average of those samples is the incoming radiance (irradiance / pi) arriving at the surfel — multiply by albedo for the reflected color, or add it to the direct lightmap term for full GI. Cosine weighting makes the estimator exact for a constant field (an open surfel under a uniform sky returns exactly the sky color), and the samples come from a deterministic Hammersley sequence, so the bake is reproducible and unit-testable without a GPU.  Honest tag (see docs/GODOT_GAPS_ROADMAP.md): the gather + visibility math is CPU-verified here and is a real (if single-bounce) path-traced irradiance estimate. Iterating it to convergence over a full unwrapped UV atlas, multi-bounce, and streaming the result into an SDFGI probe volume are the heavier offline/GPU stages this provides the kernel for.

**Types:** `GiPatch`, `GiBakeOptions`

**Functions:**

- `inline float radicalInverseVdC(std::uint32_t bits)`
- `inline bool nearestPatch(const math::vec3& from, const math::vec3& dir, float maxDist,`
- `inline math::vec3 gatherIrradiance(const Surfel& s, const std::vector<GiPatch>& patches,`
- `inline std::vector<math::vec3> bakeIndirect(const std::vector<Surfel>& surfels,`

### `GreedyVoxelMesh`
<sub>`engine/include/maz/render/GreedyVoxelMesh.hpp`</sub>

maz::render greedy voxel meshing — turn a 3D grid of blocks into a renderable surface mesh, merging every run of coplanar, same-type, equally-exposed block faces into ONE big quad (Mikola Lysenko's "greedy" algorithm). This is the reverse of MeshVoxelize (mesh -> voxels); it is what makes voxel worlds actually drawable. The naive approach emits two triangles per exposed block face — a flat 100x100 floor becomes 20,000 triangles; greedy meshing turns that same floor into a SINGLE quad (2 triangles). On real Minecraft/Teardown-style terrain it routinely cuts triangle counts by 5-10x, which is the difference between a chunk that renders and one that tanks the GPU. Only faces that are actually exposed (a solid cell whose neighbour across that face is empty) are emitted, and only faces of the same block type merge. Output is a list of axis-aligned quads with outward-facing winding, ready to expand into triangles. Header-only, std-only, deterministic. Godot has no voxel mesher.

**Types:** `VoxelQuad`

**Functions:**

- `inline std::vector<VoxelQuad> greedyVoxelMesh(int nx, int ny, int nz,`

### `Grid3D`
<sub>`engine/include/maz/render/Grid3D.hpp`</sub>

3D reference grid + gizmo axes — the ground grid and RGB axis marker every 3D editor viewport draws (Godot's Node3D editor). Maz could draw lit meshes and debug lines, but had no builder for the two spatial-reference primitives you constantly want when placing things in 3D: a WORLD-SPACE GROUND GRID (so you can read scale and position on the XZ plane) and an ORIGIN GIZMO (the X=red / Y=green / Z=blue axes that show which way is which). This is pure geometry — it emits a list of colored line segments — so it has no renderer dependency and unit-tests headlessly; the app draws each segment through the existing debug-line path (Renderer::drawLine).

**Types:** `Line3`, `GridSpec`

**Functions:**

- `inline std::vector<Line3> buildGrid(const GridSpec& spec)`
- `inline std::vector<Line3> buildWireBox(math::vec3 mn, math::vec3 mx, math::vec4 color)`

### `HarrisCorners`
<sub>`engine/include/maz/render/HarrisCorners.hpp`</sub>

maz::render::harrisCorners — the Harris & Stephens corner detector: find the distinctive, trackable "corner" points in an image (where brightness changes sharply in TWO directions), as opposed to flat regions (no change) or straight edges (change in only one direction). Corners are the stable landmarks used to align/stitch images, match features between frames, calibrate, auto-register decals or sprites, and drive simple optical-flow tracking. It works from the local structure tensor — sums of squared image gradients over a small window — whose two eigenvalues are both large only at a true corner; the Harris response det(M) - k*trace(M)^2 captures that without an eigen-solve. Non-maximum suppression then keeps only the strongest response in each neighbourhood. Godot ships no feature detector. Header-only, std-only, deterministic; operates on the luminance of a maz::render::Image.

**Types:** `Corner`

**Functions:**

- `inline std::vector<Corner> harrisCorners(const Image& img, float k = 0.04f, float relThreshold = 0.01f,`

### `Image`
<sub>`engine/include/maz/render/Image.hpp`</sub>

maz::render CPU image — a headless RGBA8 raster, the counterpart to Godot's Image. Games use it to build procedural textures, icons and lookup tables on the CPU, edit pixels, flip/blit regions, and then hand the raw bytes to the GPU (createTexture). Storage is 8-bit-per-channel, row-major, with a TOP-LEFT origin (y grows downward, matching Godot Image). Pure value type, header-only, unit-tested.

**Types:** `Image`

### `ImageAdjust`
<sub>`engine/include/maz/render/ImageAdjust.hpp`</sub>

maz::render IMAGE ADJUSTMENTS — the "levels / adjustments" panel for CPU images: brightness, contrast, gamma, invert, greyscale, and threshold. These are the tone-and-value operations every texture pipeline needs, and they compose with the procedural generators (noise, cellular, gradients) and `blend`: brighten or add contrast to a noise height map before baking a normal map, gamma-correct a gradient, desaturate a colour texture, or threshold a field into a crisp black/white mask (for stencils, decals, or a `blend` alpha). Each returns a NEW image the same size as the source; alpha is preserved throughout. Header-only, deterministic, headless — pure pixel math.  Scope note (honest): all maths is in the raw 0..1 channel space with no sRGB/linear conversion (matching Godot's non-linear Image ops); results are clamped to [0,1]. An empty input yields an empty image.

**Functions:**

- `inline Image mapRGB(const Image& src, Fn&& fn)`
- `inline Image adjustBrightness(const Image& src, float delta)`
- `inline Image adjustContrast(const Image& src, float factor)`
- `inline Image adjustGamma(const Image& src, float gamma)`
- `inline Image invert(const Image& src)`
- `inline Image grayscale(const Image& src)`
- `inline Image threshold(const Image& src, float t, const Color& high = Color`

### `ImageBlend`
<sub>`engine/include/maz/render/ImageBlend.hpp`</sub>

maz::render IMAGE BLEND / COMPOSITE — layer one image over another with the Photoshop-style blend modes. This is what lets the engine's procedural textures be *combined*: multiply a `cellularTexture` stone pattern under a `gradientMap` colour to tint it, screen a noise "grunge" layer over a base to weather it, add a glow sprite, overlay detail, or difference two fields for edges. The `top` image is composited over `base` (treated as an opaque backdrop): for each pixel the two colours are combined by the chosen mode, then mixed toward the base by the top pixel's alpha times `opacity`. The result is `base`'s size; where `top` is smaller (or a pixel is fully transparent) the base shows through unchanged. Header-only, deterministic, headless — pure pixel math.  Scope note (honest): channels are blended in raw 8-bit space with no gamma/linear conversion (matching Godot's non-linear Image ops and most 2D paint tools); the base is treated as opaque and its alpha is preserved; `top` is aligned to the base's top-left origin (no scaling — resize first if you need a fit). An empty base yields an empty image.

**Types:** `ImageBlendMode`

**Functions:**

- `inline float clampUnit(float v)`
- `inline float blendChannel(ImageBlendMode mode, float b, float s)`
- `inline Image blend(const Image& base, const Image& top, ImageBlendMode mode, float opacity = 1.0f)`

### `ImageBlur`
<sub>`engine/include/maz/render/ImageBlur.hpp`</sub>

maz::render CPU image blur — separable Gaussian and box blur over a row-major grayscale float image. This is the offline/CPU counterpart to the engine's GPU blur passes (bloom, SSAO): the tool for softening procedurally-generated textures and heightmaps, anti-aliasing signed-distance fields, baking soft ambient occlusion or shadow into a texture, and building properly-filtered mip levels — all headlessly, with no GPU. Godot exposes image blur only on the GPU (via shaders/compositor), so a deterministic CPU blur is a genuinely-useful beyond-Godot utility. Boundaries use clamp-to-edge, so a constant image is returned unchanged (partition of unity). Header-only, std-only.

**Functions:**

- `inline std::vector<float> gaussianKernel1D(int radius, float sigma = 0.0f)`
- `inline std::vector<float> separableBlur(const std::vector<float>& src, int width, int height, int radius,`
- `inline std::vector<float> gaussianBlurGray(const std::vector<float>& src, int width, int height,`
- `inline std::vector<float> boxBlurGray(const std::vector<float>& src, int width, int height, int radius)`

### `ImageCodecBmp`
<sub>`engine/include/maz/render/ImageCodecBmp.hpp`</sub>

maz::render BMP codec — a headless, dependency-free encoder/decoder between render::Image and the uncompressed Windows BMP (.bmp) byte format Godot imports. Encodes 32-bit BGRA with a BITMAPINFOHEADER, bottom-up rows (the BMP default). Decodes uncompressed 24- or 32-bit BMPs, honouring the sign of the height field (bottom-up vs top-down) and the 4-byte row padding that 24-bit rows require. Pure CPU bytes, no GPU upload.

**Functions:**

- `inline std::vector<std::uint8_t> encodeBmp(const Image& img)`
- `inline Image decodeBmp(const std::uint8_t* d, std::size_t size)`
- `inline Image decodeBmp(const std::vector<std::uint8_t>& bytes)`

### `ImageCodecDds`
<sub>`engine/include/maz/render/ImageCodecDds.hpp`</sub>

maz::render DDS (.dds) decoder — unpacks the block-compressed DirectDraw Surface textures that games ship by the thousand (DXT1/DXT3/DXT5, a.k.a. BC1/BC2/BC3) into an editable RGBA8 `Image`. Godot's Image importer reads DDS; Maz's Ktx2 path keeps compressed blocks for direct GPU upload and never unpacks them on the CPU, so there was no way to get DDS pixels into an `Image` for procedural editing, thumbnails, or software sampling. This decoder does exactly that: it reads the 128-byte DDS header, walks the 4x4 block grid, and reverses the S3TC/BC block math (565 color endpoints + 2-bit selectors, plus BC2's explicit 4-bit alpha or BC3's interpolated 3-bit alpha). Pure CPU byte work — no GPU — so it unit-tests headlessly against blocks produced by a reference decoder; `loadDds` wraps it for files.  Scope note (honest): the three S3TC block formats (DXT1/DXT3/DXT5) with the classic 128-byte header, the mip-0 top surface only. Uncompressed-RGB DDS, DX10-extended-header formats (BC4-7, ASTC), cubemaps, and mip chains are documented follow-ups. Colour interpolation uses the standard (2a+b)/3 rule on the expanded 8-bit endpoints; exact rounding of interpolated texels is hardware-defined and may differ by ±1. A malformed or unsupported file returns an empty Image.

**Functions:**

- `inline std::uint32_t ddsU32(const std::uint8_t* p)`
- `inline void dds565(std::uint16_t c, int& r, int& g, int& b)`
- `inline void ddsColorBlock(const std::uint8_t* blk, int rgb[16][3], int alpha[16], bool dxt1)`
- `inline void ddsAlphaBlockBc3(const std::uint8_t* blk, int alpha[16])`
- `inline void ddsAlphaBlockBc2(const std::uint8_t* blk, int alpha[16])`
- `inline Image decodeDds(const std::uint8_t* data, std::size_t size)`
- `inline Image decodeDds(const std::vector<std::uint8_t>& bytes)`
- `inline Image loadDds(const std::string& path)`

### `ImageCodecDdsEncode`
<sub>`engine/include/maz/render/ImageCodecDdsEncode.hpp`</sub>

maz::render DDS (.dds) BC1/DXT1 + BC3/DXT5 encoder — the inverse of the M511 decoder, and the CPU texture-compression step Godot's editor runs on import (RGBA -> block-compressed GPU texture, 1/4-1/6th the memory of RGBA8). Maz could decode DXT but not produce it, so there was no way to author or re-pack a compressed texture on the CPU. `encodeBc1Block` compresses one 4x4 RGB block to 8 bytes using "farthest-pair" range-fit endpoints (the two most distant texels in RGB as the 565 endpoints, then each texel snapped to the nearest of the four interpolated palette colours); `encodeBc3AlphaBlock` compresses a 4x4 alpha block to 8 bytes (min/max endpoints + 8-value interpolation + 3-bit indices). `encodeDdsBc1` writes an opaque DXT1 `.dds`; `encodeDdsBc3` writes a DXT5 `.dds` that also carries the alpha channel (for sprites, UI, foliage cut-outs); `saveDdsBc1` / `saveDdsBc3` wrap them to files. Pure CPU byte work — no GPU — so it unit-tests headlessly by compressing then decoding with the (independently-verified) M511 decoder and checking the result stays within block-compression tolerance.  Scope note (honest): BC1/DXT1 (opaque 4-colour) and BC3/DXT5 (RGB + interpolated alpha); dimensions padded up to a multiple of 4 by clamping edge texels; mip-0 only. It is a fast range-fit encoder (not the optimal least-squares / cluster-fit an offline tool like NVTT uses), and does not emit BC2 or BC4/5/6/7. Good enough for authoring and round-trip; documented follow-ups for higher quality.

**Functions:**

- `inline std::uint16_t ddsFrom565Round(int r, int g, int b)`
- `inline void ddsExpand565(std::uint16_t c, int& r, int& g, int& b)`
- `inline void encodeBc1Block(const std::uint8_t rgb[48], std::uint8_t out[8])`
- `inline std::vector<std::uint8_t> encodeDdsBc1(const Image& img)`
- `inline bool saveDdsBc1(const std::string& path, const Image& img)`
- `inline void encodeBc3AlphaBlock(const std::uint8_t a[16], std::uint8_t out[8])`
- `inline std::vector<std::uint8_t> encodeDdsBc3(const Image& img)`
- `inline bool saveDdsBc3(const std::string& path, const Image& img)`

### `ImageCodecGif`
<sub>`engine/include/maz/render/ImageCodecGif.hpp`</sub>

maz::render GIF codec — decode/encode the GIF (Graphics Interchange Format) image, still ubiquitous for pixel-art sprites, UI icons, and short loops on the web. GIF stores an INDEXED image (a palette of up to 256 RGB colors + one index per pixel) and compresses the index stream with variable-width LZW. This implements that: `decodeGif` reads the header + logical-screen + global color table + the first image frame's LZW data into an RGBA8 Image, and `encodeGif` writes a single-frame GIF89a — building a palette from the image (using the exact colors when there are ≤256, else median-cut via ColorQuantize) and LZW-compressing the indices. Pure CPU bytes (no GPU), so it unit-tests headlessly by an ENCODE→DECODE round-trip that is LOSSLESS for ≤256-color images (the common case for GIF's target content).  Scope note (honest): GIF89a, the FIRST frame, global color table, no interlace, opaque (GIF transparency index + multi-frame animation are documented follow-ups). The variable-width LZW (clear/EOI codes, code growth 2→12 bits, dictionary reset) is complete.

**Types:** `GifBitWriter`, `GifBitReader`

**Functions:**

- `inline void putU16(std::vector<std::uint8_t>& v, unsigned x)`
- `inline unsigned readU16(const std::uint8_t* d, std::size_t p)`
- `inline Image decodeGif(const std::uint8_t* d, std::size_t n)`
- `inline Image decodeGif(const std::vector<std::uint8_t>& bytes)`
- `inline std::vector<std::uint8_t> encodeGif(const Image& img)`

### `ImageCodecPng`
<sub>`engine/include/maz/render/ImageCodecPng.hpp`</sub>

maz::render PNG (.png) decoder — closes the single most important image-import gap versus Godot, which imports PNG everywhere. Built on the M499 `io::zlibInflate` decompressor: it walks the PNG chunk stream (IHDR / PLTE / tRNS / IDAT / IEND), inflates the concatenated IDAT data, reverses the five per-scanline filters (None / Sub / Up / Average / Paeth), and expands the samples into an RGBA8 `Image` ready for `Renderer::createTexture`. Grayscale, RGB, RGBA, grayscale+alpha, and 8-bit palette (with optional tRNS alpha) color types are supported. Pure CPU byte work — unit-tested headlessly against PNGs produced by a reference encoder.  Scope note (honest): 8-bits-per-channel, both progressive and Adam7-interlaced. It does not handle 1/2/4/16-bit depths or ancillary color-management chunks; those are documented follow-ups. A malformed or unsupported file returns an empty Image.

**Functions:**

- `inline std::uint32_t pngU32(const std::uint8_t* p)`
- `inline int pngPaeth(int a, int b, int c)`
- `inline Image decodePng(const std::uint8_t* data, std::size_t size)`
- `inline Image decodePng(const std::vector<std::uint8_t>& bytes)`
- `inline Image loadPng(const std::string& path)`

### `ImageCodecPnm`
<sub>`engine/include/maz/render/ImageCodecPnm.hpp`</sub>

maz::render Netpbm (PNM) image codec — decode/encode between render::Image and the PBM/PGM/PPM family (magic P1..P6). Netpbm is the simplest, most universal raster interchange: GIMP, ImageMagick, netpbm tools, scientific/CV pipelines and many render farms emit it, and it is trivially hand-writable, which makes it valuable both as an import format and as a debugging output. Decodes all six variants — ASCII bitmap/graymap/pixmap (P1/P2/P3) and binary bitmap/graymap/pixmap (P4/P5/P6) — into an RGBA8 Image; encodes to binary P6 (RGB) or ASCII P3. Pure CPU bytes (no GPU), so it unit-tests headlessly from an in-memory buffer.  Scope note (honest): 8-bit maxval (the ubiquitous case) — a `maxval > 255` (16-bit) sample is not rescaled. Bitmaps map 1->black, 0->white (PBM convention). Alpha is always opaque (PNM has no alpha).

**Functions:**

- `inline bool pnmReadUint(const std::uint8_t* d, std::size_t n, std::size_t& p, long& out)`
- `inline bool pnmReadBit(const std::uint8_t* d, std::size_t n, std::size_t& p, int& out)`
- `inline Image decodePnm(const std::uint8_t* d, std::size_t n)`
- `inline Image decodePnm(const std::vector<std::uint8_t>& bytes)`
- `inline std::vector<std::uint8_t> encodePnmP6(const Image& img)`
- `inline std::vector<std::uint8_t> encodePnmP3(const Image& img)`

### `ImageCodecQoi`
<sub>`engine/include/maz/render/ImageCodecQoi.hpp`</sub>

maz::render QOI codec — a headless, dependency-free encoder/decoder between render::Image and the "Quite OK Image" (.qoi) byte format, the fast lossless format Godot 4 imports natively. Encodes 32-bit RGBA; decodes 3- or 4-channel QOI. Pure CPU bytes (no GPU upload), exact to the QOI specification (https://qoiformat.org): running-array index, per-channel diff / luma deltas, and run-length runs, with the canonical 8-byte end marker.

**Functions:**

- `inline int qoiHash(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)`
- `inline std::vector<std::uint8_t> encodeQoi(const Image& img)`
- `inline Image decodeQoi(const std::uint8_t* d, std::size_t size)`
- `inline Image decodeQoi(const std::vector<std::uint8_t>& bytes)`

### `ImageCodecTga`
<sub>`engine/include/maz/render/ImageCodecTga.hpp`</sub>

maz::render TGA codec — a headless, dependency-free encoder/decoder between render::Image and the Truevision TGA (.tga) byte format Godot's Image can import. Unlike the runtime stb_image path (which needs the GPU texture upload), this works purely on CPU bytes: encode an Image you built procedurally to a .tga blob, or decode a .tga blob back into an editable Image. Uncompressed true-colour (type 2), 24- or 32-bit; encoding always writes 32-bit BGRA with a top-left origin.

**Functions:**

- `inline std::vector<std::uint8_t> encodeTga(const Image& img)`
- `inline Image decodeTga(const std::uint8_t* data, std::size_t size)`
- `inline Image decodeTga(const std::vector<std::uint8_t>& bytes)`

### `ImageColorBlind`
<sub>`engine/include/maz/render/ImageColorBlind.hpp`</sub>

maz::render COLOUR-BLINDNESS SIMULATION — an accessibility dev-tool: preview how the game's UI, minimap, team colours, or status effects look to players with colour-vision deficiency, so you can catch red/green pairs that become indistinguishable BEFORE shipping. Applies the widely-used Wickline dichromat transforms for the three common types (protanopia = red-weak, deuteranopia = green-weak, tritanopia = blue-weak) plus achromatopsia (total colour blindness → luminance grey). Works on a single `Color` or a whole `Image` (alpha preserved). Pair it with the procedural texture pipeline or run it over a UI screenshot. Header-only, deterministic, headless.  Scope note (honest): the dichromat matrices operate on non-linear sRGB channels directly (the common, fast approximation used by web colour-blindness filters), not a physically-exact LMS/Brettel simulation; results are clamped to [0,1]. Rows of each matrix sum to 1, so a pure grey is left unchanged. An empty image → empty.

**Types:** `ColorVision`

**Functions:**

- `inline const float* cvdMatrix(ColorVision mode)`
- `inline Color simulateColorVision(const Color& c, ColorVision mode)`
- `inline Image simulateColorVision(const Image& src, ColorVision mode)`

### `ImageDraw`
<sub>`engine/include/maz/render/ImageDraw.hpp`</sub>

maz::render image drawing primitives — rasterize 2D shapes directly INTO an Image on the CPU. The Image class already edits pixels and blits regions, but it had no way to stroke a line, outline or fill a circle, draw a rectangle border, or fill a triangle — the building blocks for procedural textures, generated icons, minimap/radar overlays, debug visualisations, and simple CPU-side vector art. These free functions plot through the Image's bounds-checked setPixel, so anything off-canvas is safely clipped. Integer pixel coordinates; the classic Bresenham line and midpoint circle, a bounding-box disc fill, and a barycentric triangle fill (winding-independent). Header-only, pure, deterministic. (Distinct from Renderer's GPU debug-draw — this writes into an in-memory image you can then save, upload, or sample.)

**Functions:**

- `inline void drawLine(Image& img, int x0, int y0, int x1, int y1, const Color& c)`
- `inline void drawRect(Image& img, int x, int y, int w, int h, const Color& c)`
- `inline void drawCircle(Image& img, int cx, int cy, int r, const Color& c)`
- `inline void fillCircle(Image& img, int cx, int cy, int r, const Color& c)`
- `inline int edge(int ax, int ay, int bx, int by, int px, int py)`
- `inline void fillTriangle(Image& img, int x0, int y0, int x1, int y1, int x2, int y2, const Color& c)`

### `ImageGradientMap`
<sub>`engine/include/maz/render/ImageGradientMap.hpp`</sub>

maz::render GRADIENT MAP (colorize) — recolour a greyscale image by running each pixel's brightness through a colour ramp. This is the natural colour stage for the procedural grey textures in this engine: feed a `patterns::noiseTexture` or `patterns::cellularTexture` (or any height field / mask) in, and get lava (black → red → yellow), terrain (deep → shallow → sand → grass → rock → snow), fire, marble tint, a heat-map, or a toon colour ramp out. Each input pixel's perceptual luminance (0.299 R + 0.587 G + 0.114 B, clamped to [0,1]) is the ramp parameter; the ramp's colour becomes the output pixel (RGB from the ramp, and — for the stop/two-colour forms — the ramp's alpha too). Three forms: a two-colour `lo → hi` ramp, a multi-stop ramp (linear between sorted stops, clamped past the ends, exactly like a colour gradient), and a fully general callback form that accepts any `Color(float t)` callable (e.g. `anim::Gradient::sample`). Header-only, deterministic, headless.  Scope note (honest): luminance is computed from the raw 8-bit channel values with no gamma handling; a stop list is used as given (assumed sorted ascending by `t`); an empty stop list leaves the image unchanged in shape but paints it transparent black. A non-positive / empty input yields an empty image.

**Types:** `ColorStop`

**Functions:**

- `inline float luminance01(const Color& c)`
- `inline Image gradientMap(const Image& grey, Ramp&& ramp)`
- `inline Image gradientMap(const Image& grey, const Color& lo, const Color& hi)`
- `inline Image gradientMap(const Image& grey, const std::vector<ColorStop>& stops)`

### `ImageNormalMap`
<sub>`engine/include/maz/render/ImageNormalMap.hpp`</sub>

maz::render HEIGHT -> NORMAL MAP — turn a grey heightmap (bright = high, dark = low) into a tangent-space NORMAL MAP, the blue-purple texture that makes a flat surface look bumpy under lighting. Paint or generate a height image — bricks, cobbles, scales, wrinkles, carved detail, hammered metal — and this reads its slopes and writes the surface direction at every pixel, so a lighting shader can fake all that relief without extra geometry. It is the standard "bake a normal map from a height texture" step (Godot's Image bump-to-normal, Blender's bump node, Substance/Photoshop's "Normal from Height"). The height is read from the RED channel; `strength` exaggerates or softens the bumps. Output is an RGBA8 image encoding the unit normal as (x,y,z)*0.5+0.5 with alpha 1 — a flat region comes out the classic (128,128,255) light blue. Header-only, deterministic, headless — pure pixel math.  Scope note (honest): slopes are measured with central differences and the border pixels clamp to their neighbours (so the very edge is flat-ish). Green is +Y (OpenGL convention — the common one; flip G for a DirectX map). The input's red channel is the height; colour/other channels are ignored. An empty input yields an empty image.

**Functions:**

- `inline Image heightToNormalMap(const Image& heights, float strength = 1.0f)`

### `ImagePatterns`
<sub>`engine/include/maz/render/ImagePatterns.hpp`</sub>

maz::render PROCEDURAL IMAGE PATTERNS — generate common textures in code, no art files needed. A checkerboard for a placeholder / "missing texture" material, UV-check pattern, or floor tiles; a smooth top-to-bottom gradient for skies, backdrops, UI panels, and fades; a radial glow for spotlights, vignettes, soft particle sprites, and button highlights. Each returns a CPU `Image` (RGBA8) ready to hand to `Renderer::createTexture` or to save with the image codecs. Great for prototyping before real art exists, for runtime-generated UI, and for test cards. Header-only, deterministic, headless — pure pixel math.  Scope note (honest): these are basic building-block patterns (checker, linear gradient, radial gradient). Colours are plain RGBA with no gamma handling — the values are written straight to 8-bit channels. A non-positive width or height yields an empty image; a checker cell size below 1 is treated as 1.

**Types:** `Cellular`

**Functions:**

- `inline Image checkerboard(int width, int height, int cell, const Color& a, const Color& b)`
- `inline Image verticalGradient(int width, int height, const Color& top, const Color& bottom)`
- `inline Image linearGradient(int width, int height, float angleRadians, const Color& from, const Color& to)`
- `inline Image radialGradient(int width, int height, const Color& centre, const Color& edge)`
- `inline Image noiseTexture(int width, int height, float scale = 0.08f, std::uint64_t seed = 0, int octaves = 4)`
- `inline Image brickWall(int width, int height, int brickW, int brickH, int mortarPx, const Color& brick,`
- `inline Image cellularTexture(int width, int height, float scale = 0.06f, std::uint32_t seed = 0,`
- `inline Image voronoiTexture(int width, int height, float scale = 0.08f, std::uint32_t seed = 0)`
- `inline Image marbleTexture(int width, int height, float veinFrequency = 0.12f, float turbulence = 4.0f,`
- `inline Image woodTexture(int width, int height, float ringScale = 0.5f, float turbulence = 3.0f,`

### `Ktx2`
<sub>`engine/include/maz/render/Ktx2.hpp`</sub>

maz::render KTX2 container parsing — KTX2 (Khronos Texture 2) is the standard GPU-texture file: it stores an ALREADY-GPU-READY image (a specific VkFormat, including GPU-compressed formats like BC7 / ASTC / ETC2) plus its full mip chain, so the engine uploads the bytes straight to the GPU with no decode. That is the whole point of compressed textures — a BC7 4K texture is ~4x smaller in VRAM than RGBA8 and needs no CPU unpack. Godot ships textures as .ktx2/.basis; this is the container reader that tells the loader what's inside and where each mip level's bytes live.  This header parses the KTX2 *structure* (identifier, header, and the level index) — pure, std-only, bounds-checked, and unit-tested against a hand-built buffer. It does NOT transcode Basis-Universal supercompression or decode block formats (that needs the libktx/basisu transcoder and, ultimately, the GPU); it reports the format + level offsets so a GPU uploader can consume them. A follow-up wires this into VulkanTexture behind a real device.

**Types:** `Ktx2Level`, `Ktx2Info`

**Functions:**

- `inline uint32_t rdU32(const uint8_t* p)`
- `inline uint64_t rdU64(const uint8_t* p)`
- `inline bool hasKtx2Identifier(const uint8_t* data, size_t size)`
- `inline Ktx2Info parseKtx2(const uint8_t* data, size_t size)`
- `inline Ktx2Info parseKtx2(const std::vector<uint8_t>& bytes)`

### `Lightmap`
<sub>`engine/include/maz/render/Lightmap.hpp`</sub>

maz::render CPU lightmap baker — the offline "burn the lighting into a texture" step behind Godot's LightmapGI. A lightmap stores precomputed lighting per surface point so static geometry looks lit without paying for lights at runtime. This bakes the *direct* term: for each surfel (a world-space point + normal — in a full pipeline these are the unwrapped texel centers), it sums every light's contribution (N·L, with distance falloff for point lights) and traces a shadow ray against the occluder triangles so geometry casts hard shadows into the map. It reuses the header-only `math::segmentIntersectsTriangle` ray test, so it is pure CPU and fully unit-testable; only *sampling* the resulting map at draw time needs a GPU.  Scope note (honest): direct lighting + hard shadows only. It does not do indirect/bounce GI, area-light softness, or the UV-atlas unwrap (the caller supplies the surfels). Those are documented follow-ups.

**Types:** `Surfel`, `BakeLight`, `BakeTriangle`, `LightmapBakeOptions`

**Functions:**

- `inline bool segmentBlocked(const math::vec3& from, const math::vec3& to,`
- `inline std::vector<math::vec3> bakeLightmap(const std::vector<Surfel>& surfels,`

### `Line2D`
<sub>`engine/include/maz/render/Line2D.hpp`</sub>

2D polyline stroking — Godot's Line2D. Maz can FILL a convex polygon (drawConvexPolygon), but a polyline is a *stroke*: a path of points thickened to a ribbon of a given WIDTH, with the corners (JOINTS) and the two ends (CAPS) shaped so the ribbon reads as one continuous stroke — the primitive behind trails, drawn curves, graphs, outlines, and lightning. This turns a point list into a triangle soup (groups of three math::vec2) that any 2D fill path can draw; it is pure geometry (no renderer dependency, no allocation beyond the output), so it unit-tests headlessly and is deterministic.

**Types:** `JointMode`, `CapMode`, `PolylineStyle`

**Functions:**

- `inline math::vec2 perp(math::vec2 d)`
- `inline math::vec2 normalized(math::vec2 v)`
- `inline void emitTri(std::vector<math::vec2>& out, math::vec2 a, math::vec2 b, math::vec2 c)`
- `inline void emitQuad(std::vector<math::vec2>& out, math::vec2 a, math::vec2 b, math::vec2 c, math::vec2 d)`
- `inline void emitFan(std::vector<math::vec2>& out, math::vec2 center, math::vec2 a, math::vec2 b,`
- `inline bool lineIntersect(math::vec2 p0, math::vec2 d0, math::vec2 p1, math::vec2 d1, math::vec2& out)`
- `inline std::vector<math::vec2> buildPolyline(const std::vector<math::vec2>& pts, const PolylineStyle& style)`

### `LineAA`
<sub>`engine/include/maz/render/LineAA.hpp`</sub>

maz::render::drawLineAA — Xiaolin Wu's anti-aliased line. The engine's Bresenham drawLine (ImageDraw.hpp) snaps each step to one pixel, so any non-axis-aligned line comes out jagged ("staircased"). Wu's algorithm instead spreads each step across the TWO pixels it straddles, weighted by how much of the pixel the line actually covers, producing smooth edges — exactly what crisp graph plots, wireframe overlays, vector-style UI strokes, minimap routes, and debug gizmos want on a CPU raster. It also takes sub-pixel float endpoints, so lines can start and end between pixels. Coverage is alpha-composited over whatever is already in the image. Godot's Image has no anti-aliased line primitive. Header-only, std-only, deterministic.

**Functions:**

- `inline void blendCoverage(Image& img, int x, int y, const Color& c, float cov)`
- `inline float ipartf(float x)`
- `inline float fpartf(float x)`
- `inline float rfpartf(float x)`
- `inline int roundi(float x)`
- `inline void drawLineAA(Image& img, float x0, float y0, float x1, float y1, const Color& c)`

### `MedianCut`
<sub>`engine/include/maz/render/MedianCut.hpp`</sub>

maz::render median-cut colour quantization — shrink a full-colour image down to a small, representative PALETTE of at most K colours, the way a GIF, an indexed texture, or a deliberately retro/limited-palette look is produced. It recursively splits the cloud of pixel colours: at each step it takes the box of colours with the widest spread along red, green, or blue and cuts it at the MEDIAN of that channel, so dense regions of colour get more palette entries than sparse ones. Each final box contributes its average colour to the palette. This is the classic Heckbert median cut — better balanced than a naive "keep the most common colours" pass, which is what the GIF encoder currently falls back to. Pair it with `nearestColor` to remap the image to palette indices. Godot has no runtime colour quantizer. Header-only, std-only, deterministic.

**Functions:**

- `inline std::vector<Color> medianCutPalette(const std::vector<Color>& pixels, int maxColors)`
- `inline int nearestColor(const std::vector<Color>& palette, const Color& c)`

### `MedianFilter`
<sub>`engine/include/maz/render/MedianFilter.hpp`</sub>

maz::render median filter — remove "salt-and-pepper" speckle while keeping edges crisp.  Replacing each pixel with the MEDIAN of its neighbourhood (rather than the average) is the classic impulse-noise cleaner: a lone bright or dark speckle is an outlier in the window, so the median simply ignores it — the pixel takes a neighbour's real value. Crucially, unlike a Gaussian/box blur, the median does NOT smear edges: on either side of a sharp boundary the majority of the window still holds that side's value, so the edge stays sharp. Used to clean noisy masks, denoise generated/scanned textures, and pre-filter before thresholding or edge detection. Clamp-to-edge borders, radius r gives a (2r+1)x(2r+1) window. Pure CPU, header-only, deterministic — unit-tested that it kills a speckle, leaves flat regions and sharp edges untouched, and matches a hand-computed window median.

**Functions:**

- `inline std::vector<std::uint8_t> medianFilter(const std::uint8_t* gray, int w, int h, int radius = 1)`
- `inline std::vector<std::uint8_t> medianFilter(const std::vector<std::uint8_t>& gray, int w, int h,`

### `MeshAmbientOcclusion`
<sub>`engine/include/maz/render/MeshAmbientOcclusion.hpp`</sub>

maz::render PER-VERTEX AMBIENT OCCLUSION bake — the offline "bake AO into the mesh" step that darkens crevices, contact points, and interiors so a scene reads with depth even under flat ambient light, exactly what Godot's LightmapGI / the classic "vertex bake" does but stored per vertex. For each vertex it shoots a deterministic fan of rays over the hemisphere around the vertex normal and measures how many are blocked by the mesh's own triangles within a distance: fully open -> 0, deep in a cavity -> approaching 1. Ray/triangle tests use Möller–Trumbore. Fully deterministic (a golden-angle hemisphere set, no RNG) so it unit-tests by asserting an occluded vertex is darker than an exposed one. Pure CPU, header-only, headless.  Scope note (honest): brute-force O(verts · rays · tris) against the mesh's own geometry — fine for the offline bake of props and levels; a BVH acceleration (game::Bvh exists) and multi-bounce colour bleed are the documented follow-ups. Normals are computed area-weighted from the triangles, so the input needs no pre-baked normals.

**Functions:**

- `inline bool aoRayTri(const math::vec3& o, const math::vec3& d, const math::vec3& a, const math::vec3& b,`
- `inline std::vector<float> bakeVertexAO(const shapes::MeshData& mesh, int rayCount, float maxDistance)`

### `MeshArrow`
<sub>`engine/include/maz/render/MeshArrow.hpp`</sub>

maz::render ARROW MESH — a solid 3D arrow (a round shaft with a cone tip) pointing along +Y. Arrows are the universal "look here / this way" marker: draw a force or velocity vector, show which way a spawn/waypoint faces, build the move/rotate gizmo handles for an editor, point at an objective, make a compass needle or a wind-direction indicator. Point it wherever you like by rotating the mesh (aim +Y at your target). It is one closed watertight solid — shaft tube + bottom cap + the flat under-shoulder of the head + the cone — so it lights and casts shadows like any prop. Reuses the engine's area-weighted `computeNormals`. Header-only, deterministic, headless — pure CPU geometry.  Scope note (honest): the total `length` runs from the base at y=0 to the tip at y=length; `headLength` is how much of that is the cone (clamped into the open interval so there is always some shaft and some head), `shaftRadius` < `headRadius` gives the classic arrow shoulder. Normals are smoothed across the shaft/shoulder/ cone joins (one `computeNormals` pass over shared vertices), so the creases read a touch soft — split the vertices for razor-sharp edges if you need them. `segments` (>= 3) sets how round the shaft/cone are. Bad params (length <= 0, radii <= 0, segments < 3) return an empty mesh.

**Functions:**

- `inline shapes::MeshData buildArrow(float length = 1.0f, float shaftRadius = 0.03f, float headRadius = 0.08f,`

### `MeshBend`
<sub>`engine/include/maz/render/MeshBend.hpp`</sub>

maz::render BEND — curl a straight mesh around an arc: a bar wraps onto a circle of radius `radius`, so a plank becomes an archway, a straight pipe becomes an elbow, a flat strip becomes a curled ribbon or a barrel stave. This is Blender's "Simple Deform → Bend" and the third classic deformer alongside twist (M581) and taper (M582). You pick the `alongAxis` the bar extends down and the `upAxis` it bends toward; the third axis rides through unchanged. A vertex's coordinate along the bar becomes an ANGLE (theta = alongCoord / radius) swept around a bend centre sitting `radius` up the up-axis, and its up-coordinate becomes how far it sits from that centre — so the whole length curls smoothly. Positions are exact; the normal's in-plane components rotate with the arc. Header-only, pure CPU, deterministic.  Scope note (honest): the bend is exact for positions — a vertex's distance from the bend centre stays `radius − up` and its swept angle is `along / radius`, both unit-tested. Normals get their (along, up) components rotated by the local arc angle, which is right for the bend's rotation but ignores the slight scale the curl adds; re-run `computeNormals` for pixel-accurate shading on a tight bend. Smoothness of the arc is limited by how many segments the bar has along its length (a 2-segment bar bends into a single kink; subdivide first for a smooth curve). A smaller `radius` curls tighter (a full circle closes when the bar length reaches 2·pi·radius); `radius` may be negative to bend the other way. The bar should straddle alongCoord = 0 for a symmetric bend (that column is the hinge that stays put).

**Functions:**

- `inline float bendGet(const MeshVertex& v, int axis)`
- `inline void bendSetPos(MeshVertex& v, int axis, float val)`
- `inline float bendGetN(const MeshVertex& v, int axis)`
- `inline void bendSetN(MeshVertex& v, int axis, float val)`
- `inline shapes::MeshData bendMesh(const shapes::MeshData& mesh, int alongAxis, int upAxis, float radius)`

### `MeshBoundaryLoops`
<sub>`engine/include/maz/render/MeshBoundaryLoops.hpp`</sub>

maz::render BOUNDARY / HOLE edge-loop extraction — walk the open edges of a mesh into the ordered vertex LOOPS that ring each hole or the outer rim of an open surface. MeshTopology (M528) already counts boundary edges; this turns them into usable curves, the front end for hole FILLING (cap each loop with a fan/ triangulation), silhouette/outline rendering, cloth/rope attachment along an edge, and "select boundary" in an editor. Each boundary undirected edge is used by exactly one triangle, so its single directed half-edge (wound by that triangle) points consistently around the hole; chaining next[a]=b from those half-edges yields the loops. Pure CPU, header-only, headless.  Scope note (honest): manifold boundaries (each boundary vertex on exactly one loop). A non-manifold figure-eight boundary vertex is resolved arbitrarily (last edge wins) — the documented edge case.

**Functions:**

- `inline std::vector<std::vector<std::uint32_t>> extractBoundaryLoops(const shapes::MeshData& mesh)`

### `MeshBoundingCylinder`
<sub>`engine/include/maz/render/MeshBoundingCylinder.hpp`</sub>

maz::render BOUNDING-CYLINDER FIT — the tightest CAPSULE-LIKE cylinder wrapped around a mesh, aligned to the object's own long axis rather than a world axis. Where an axis-aligned box (Aabb3) or even an oriented box (FitObb) is the natural proxy for a boxy prop, a cylinder is the right hull for anything long-and-round: a character's torso or limb, a pillar, a barrel, a thrown log, a rocket. Games use it for capsule colliders, trigger volumes, and cheap broad-phase bounds on elongated bodies (Godot's CapsuleShape3D wants exactly a radius + height + axis). The method is PCA: centre the vertices, take the covariance matrix's dominant eigenvector (via the engine's symmetric Jacobi solver) as the cylinder's length axis, then measure how far the points spread ALONG that axis (the height) and AWAY from it (the radius = the farthest perpendicular distance). Reuses `math::detail::jacobiEigen3`. Header-only, std-only, deterministic.  Scope note (honest): this is the PCA-aligned fit, not a global minimum-volume optimiser — for a genuinely L-shaped or clustered cloud the principal axis may not be the visually obvious one, and the radius is the worst- case perpendicular distance (it fully contains every vertex, never clips). It reads only positions. A mesh with fewer than two vertices, or one with no spread, returns `valid=false`.

**Types:** `BoundingCylinder`

**Functions:**

- `inline BoundingCylinder fitBoundingCylinder(const shapes::MeshData& mesh)`

### `MeshCleanup`
<sub>`engine/include/maz/render/MeshCleanup.hpp`</sub>

maz::render MESH CLEANUP — the import/optimization hygiene pass that shrinks a mesh without changing what it draws: merge BIT-EXACT duplicate vertices (identical in EVERY attribute — position, normal, colour, UV) into one, drop DEGENERATE triangles (two corners the same index -> zero area), and remove UNUSED vertices (no surviving triangle references them), compacting the buffers. Importers and mesh generators routinely emit bloat — a glTF/OBJ authored face-by-face repeats every shared corner; CSG/boolean and marching-cubes output leave orphaned vertices; edits leave slivers — and that bloat costs VRAM, breaks the vertex cache, and stops smoothing/subdivision from treating a shared corner as one point. This is Godot's SurfaceTool.index() hygiene, but attribute-exact and attribute-preserving (unlike MeshWeld, which welds by SPATIAL proximity and keeps positions only): use MeshWeld to fuse near-coincident corners after generation, use cleanupMesh to strip exact redundancy while keeping normals/UVs intact. Deterministic (survivors keep first-seen order), header-only, std-only.

**Types:** `MeshCleanupStats`

**Functions:**

- `inline shapes::MeshData cleanupMesh(const shapes::MeshData& mesh, MeshCleanupStats* stats = nullptr)`

### `MeshClosestPoint`
<sub>`engine/include/maz/render/MeshClosestPoint.hpp`</sub>

maz::render CLOSEST POINT ON A MESH — for any point in space, find the nearest spot ON the model's surface and how far away it is. This is the "snap to surface" / "how deep am I" query games lean on constantly: stick a decal, bullet-hole, or footprint flat on the wall it hit; snap a placed object or the mouse cursor onto the terrain; find how far a character has sunk into geometry to push them back out; measure clearance to the nearest wall; pick the mesh vertex/face closest to a click. Unlike a raycast (which needs a direction and can miss), this always returns an answer — the single closest surface point, no matter where the query point sits. It walks every triangle with the engine's exact `closestPointOnTriangle` and keeps the nearest, also reporting which triangle won and that triangle's facing normal (handy for orienting a decal). Header-only, deterministic, headless.  Scope note (honest): this is a brute-force scan over all triangles — O(triangles) per query, ideal for one-off queries and small/medium meshes; for many queries against a big mesh, put a BVH (game::Bvh / TriMesh3D) in front. It returns the closest point on the SURFACE and an UNSIGNED distance — it does not say inside vs outside (use the M556 containment / M551 SDF tools for a signed result). `normal` is the hit triangle's geometric face normal (normalized; zero for a degenerate triangle). An empty mesh yields `valid == false`.

**Types:** `ClosestPointResult`

**Functions:**

- `inline ClosestPointResult closestPointOnMesh(const shapes::MeshData& mesh, const math::vec3& query)`

### `MeshComponentColor`
<sub>`engine/include/maz/render/MeshComponentColor.hpp`</sub>

maz::render COMPONENT TINT — paint every disconnected PIECE of a mesh a different colour, so you can SEE at a glance how many separate islands it is made of and which triangles belong together. A model that looks like one object is often secretly several (a character plus loose props, terrain chunks that never welded, stray shards from a bad boolean); tinting each connected component a distinct hue is the standard debug view for spotting that — "why is my one mesh actually 40 pieces?" — and for authoring per-part masks. Each vertex's colour is set from its component's index via evenly-spread hues (golden-ratio stepping so adjacent components never share a near-colour), at the given saturation/value. Reuses `connectedComponentLabels` (M517). Header-only, std-only, deterministic — the same mesh always gets the same colours.  Scope note (honest): "connected" means sharing a vertex INDEX (welded topology) — two pieces touching in space but with separate vertices read as separate components (weld first with `weldVertices` if you want them merged). This overwrites the RGB of every vertex; positions, normals and UVs are untouched. Colours are opaque debug hues, not a physically meaningful signal.

**Functions:**

- `inline void hsvToRgb(float h, float s, float v, float& r, float& g, float& b)`
- `inline shapes::MeshData tintComponents(const shapes::MeshData& mesh, float saturation = 0.7f, float value = 0.9f,`

### `MeshComponents`
<sub>`engine/include/maz/render/MeshComponents.hpp`</sub>

maz::render CONNECTED COMPONENTS / mesh island splitting — separate a triangle soup into the independent sub-meshes that are actually STITCHED together, the "Mesh > Separate / by loose parts" operation every DCC and Godot's own tooling offers. Two triangles belong to the same island when they share an EDGE (a vertex-only touch does NOT connect them, matching how importers and physics treat loose parts). Built on the shared MeshTopology (M528): flood-fill triangles across their edge-twins, then compact each island into its own MeshData with a remapped, minimal vertex list. Useful for per-part physics bodies, per-island culling/streaming, cleaning stray geometry, and splitting a merged export back into pieces. Pure CPU, header-only, headless.

**Functions:**

- `inline std::vector<std::uint32_t> connectedComponentLabels(const shapes::MeshData& mesh,`
- `inline std::vector<shapes::MeshData> splitConnectedComponents(const shapes::MeshData& mesh)`

### `MeshContainment`
<sub>`engine/include/maz/render/MeshContainment.hpp`</sub>

maz::render POINT-IN-MESH CONTAINMENT — is a point INSIDE a closed triangle mesh? For each query point, cast one ray to infinity and count how many triangles it crosses: an odd count means inside, even means outside (the Jordan-curve / ray-parity test), reusing the M533 Möller–Trumbore ray/triangle — the same inside test that signs MeshSdf (M534) and fills MeshVoxelize (M540). Unlike those, this answers arbitrary points DIRECTLY with no grid to bake, so it is the right tool for a handful of ad-hoc tests: is a spawn point inside the level geometry, is a particle/agent still within a volume, does a prop's centre sit inside a trigger solid, rejection-sampling points into a shape. Batch and single-point entry points; an oblique ray dodges the degenerate through-a-shared-edge case. Pure CPU, header-only, headless.  Scope note (honest): brute force O(points · triangles) with a ray-parity sign, so it assumes a watertight mesh and suits modest point counts / triangle counts; for many queries against a big mesh, bake a MeshSdf once (or index with game::Bvh) instead — the documented faster path. A generalized-winding-number test for open meshes is the follow-up shared with MeshSdf.

**Functions:**

- `inline bool pointInsideTris(const math::vec3& p, const std::vector<math::vec3>& pos,`
- `inline bool containsPoint(const shapes::MeshData& mesh, const math::vec3& p)`
- `inline std::vector<std::uint8_t> containsPoints(const shapes::MeshData& mesh,`

### `MeshCurvature`
<sub>`engine/include/maz/render/MeshCurvature.hpp`</sub>

maz::render PER-VERTEX DISCRETE CURVATURE — estimate how sharply a triangle mesh bends at every vertex, producing two scalar fields: GAUSSIAN curvature K (the angle deficit — positive on domes, negative on saddles, zero on anything developable like a plane or a cylinder) and MEAN curvature |H| (the average of the two principal curvatures, a bend magnitude). These are the classic Meyer / Desbrun / Schroeder / Barr "Discrete Differential-Geometry Operators for Triangulated 2-Manifolds" (2003) estimates: Gaussian K_i = (2*pi - sum of incident triangle angles at i) / A_mixed(i) Mean     H_i = |(1 / (2*A_mixed)) * sum_j (cot a_ij + cot b_ij)(x_i - x_j)| / 2   (cotangent Laplacian) where A_mixed is the Voronoi/barycentric mixed area (obtuse-triangle safe). On a sphere of radius r these converge to K = 1/r^2 and H = 1/r; on a flat sheet both are 0. Uses: curvature-adaptive tessellation and LOD, feature / crease / ridge-valley detection, curvature-guided remeshing and texturing, wear / cavity shading masks, and "curvature" vertex-paint like a DCC tool. Reuses MeshTopology (M528) only to flag boundary vertices, where the closed-surface estimate does not apply. Pure CPU, header-only, deterministic.  Scope note (honest): the mesh should be welded (a shared corner = one vertex) for the angle sum and the Laplacian to close up — feed it through weldVertices (MeshWeld) first if it was built face-by-face. Boundary vertices are flagged and left at zero (their estimate is unreliable), not silently reported.

**Types:** `MeshCurvature`

**Functions:**

- `inline MeshCurvature computeCurvature(const shapes::MeshData& mesh)`

### `MeshCurvatureColor`
<sub>`engine/include/maz/render/MeshCurvatureColor.hpp`</sub>

maz::render CURVATURE HEATMAP — paint a mesh so you can SEE where it bends: flat regions go cool blue, gently curved areas green, and sharp creases/tips hot red. This is the standard "curvature map" every DCC tool and inspection package shows — it's how modellers spot pinching, lumps, and over-sharp edges that will shade badly, and how a retopo/QA pass finds the high-detail zones. It runs the engine's `computeCurvature` (M535) and maps each vertex's curvature MAGNITUDE through a blue→green→red colour ramp, normalised so the mesh's own maximum (or a supplied `maxValue`) becomes full red. Pick mean curvature |H| (creases, the usual choice) or Gaussian curvature |K| (spherical-vs-saddle points). Header-only, pure CPU, deterministic.  Scope note (honest): this overwrites the RGB of every vertex with a debug colour (positions/normals/UVs untouched); it is a visualisation, not a physical signal. Curvature at open BOUNDARY vertices is unreliable (the estimate needs a full one-ring), so rim vertices may read low — trust the interior. Auto-normalisation means the colours are RELATIVE to this mesh's own range; pass an explicit `maxValue` to compare two meshes on the same scale.

**Functions:**

- `inline void curvatureColor(float t, float& r, float& g, float& b)`
- `inline shapes::MeshData curvatureHeatmap(const shapes::MeshData& mesh, CurvatureKind kind = CurvatureKind::Mean,`

### `MeshDegenerate`
<sub>`engine/include/maz/render/MeshDegenerate.hpp`</sub>

maz::render DEGENERATE / SLIVER-TRIANGLE CLASSIFIER — find the badly-shaped triangles in a mesh and label each by DEFECT TYPE, returning their indices as a cleanup report. Bad triangles come from booleans, decimation, planar-cut operations, and sloppy imports; they wreck normals, lighting, physics, and simplification, so meshing tools flag them for removal or repair. Unlike the M537 TriangleQuality score (a single 0..1 number per triangle), this NAMES the problem so a repair step knows what to do: ZERO-AREA (collapsed — vertices coincident or collinear, must be deleted), CAP (one angle near 180° — a flat "sliver" that pokes across its neighbours, usually split or collapsed), and NEEDLE (one angle near 0° — a thin spike from a very short edge, usually collapsed along that edge). It also returns the mesh's worst (smallest/largest) angles and smallest area. Header-only, pure CPU; complements MeshCleanup (M541), which removes exact-duplicate and zero-area triangles.  Scope note (honest): this REPORTS defects (kind + indices + extremes); it does not repair them — feed the indices to a collapse/split/delete pass (MeshCleanup already drops the zero-area ones). Classification order is zero-area, then cap, then needle, so a triangle that is both spiky and flat is reported as a cap. Thresholds (cap angle, needle angle) are tunable; the zero-area epsilon scales with the mesh's bounding box.

**Types:** `DegenerateReport`

**Functions:**

- `inline DegenerateReport analyzeDegenerate(const shapes::MeshData& mesh, float capAngleDegrees = 150.0f,`

### `MeshDisplace`
<sub>`engine/include/maz/render/MeshDisplace.hpp`</sub>

maz::render DISPLACE / ROUGHEN — push each vertex along its (smooth) surface normal by a procedural NOISE amount, so a too-perfect surface gains organic bumpiness: a flat plane becomes rough ground, a smooth sphere becomes a lumpy rock or asteroid, a cylinder becomes a gnarled tree trunk. This is Blender's "Displace" modifier driven by a noise texture — the cheapest way to make procedural or CAD-clean geometry look natural. The offset is coherent VALUE NOISE (nearby vertices move together, so the surface undulates instead of turning to static) scaled by `amplitude`, with `frequency` setting how fine the bumps are (low = broad swells, high = tight pebbling) and `seed` picking a different random field. Everything is deterministic: the same mesh + amplitude + frequency + seed always yields the exact same result, so it is safe for networked/replayed procedural content. Reuses the engine's area-weighted `computeNormals` for the push direction. Header-only, std-only.  Scope note (honest): vertices move only ALONG their normals (no sideways drift), by at most `amplitude` in magnitude (the noise is bounded to [-1,1]); positions change and the stored normals go stale, so re-run `computeNormals` afterwards if you want the lighting to follow the new bumps. The detail you can add is limited by the mesh's existing resolution — displacing a 2-triangle quad just tilts it; subdivide first (Subdivision) for fine roughness. Amplitude may be negative; frequency <= 0 is treated as a single broad lump.

**Types:** `DisplaceResult`

**Functions:**

- `inline std::uint32_t noiseHash3(int x, int y, int z, std::uint32_t seed)`
- `inline float noiseCorner(int x, int y, int z, std::uint32_t seed)`
- `inline float valueNoise3(math::vec3 p, std::uint32_t seed)`
- `inline DisplaceResult displaceMesh(const shapes::MeshData& mesh, float amplitude, float frequency = 1.0f,`

### `MeshDominantPlane`
<sub>`engine/include/maz/render/MeshDominantPlane.hpp`</sub>

maz::render DOMINANT-PLANE / FLATNESS DETECTOR — fit the best-matching flat plane to a mesh's vertices and measure how FLAT the shape actually is. Via principal component analysis (centre the points, form their 3x3 covariance, take its eigenvectors), the direction of LEAST spread is the plane's normal and the leftover spread along it is how far the shape departs from flat. This answers "is this a wall / floor / panel / decal, and which way does it face?" — used to auto-orient flat props to a surface, pick a planar-UV axis, snap a billboard, detect ground/wall pieces for gameplay, or decide a nearly-flat mesh can collapse to a quad. Reuses the M-era FitObb symmetric-eigen solver. Header-only, pure CPU.  Scope note (honest): this fits ONE global plane through the centroid — great for genuinely planar-ish meshes (walls, panels, terrain patches), but a folded or multi-part mesh returns the average best-fit plane, not a per-region one (segment first via M529 components / M545 planar regions). `planarity` is a shape descriptor in [0,1] (1 = perfectly flat, 0 = isotropic like a cube/ball), not a physical unit; `rmsDistance` and `thickness` are in mesh units. Every vertex is weighted equally (not area-weighted), so a dense cluster pulls the fit.

**Types:** `MeshPlane`

**Functions:**

- `inline MeshPlane fitDominantPlane(const shapes::MeshData& mesh)`

### `MeshExplode`
<sub>`engine/include/maz/render/MeshExplode.hpp`</sub>

maz::render EXPLODE — pull a mesh's triangles APART: give every triangle its own three vertices, then shove each triangle bodily OUTWARD so the surface blooms open like an exploded-view diagram. Two flavours: • `explodeFaces` pushes each triangle along its OWN face normal by `distance` — the surface puffs out along the direction each face already points (a cube's six sides slide straight out, a sphere's facets bristle outward). This is the "peel a model apart to see it / dissolve / shatter-bloom" effect games use for unlock reveals, deaths, and assembly animations, and animating `distance` 0→D is the whole effect. • `explodeFacesRadial` pushes each triangle away from a CENTRE point (the bbox centre by default) by `distance` — every piece flies outward from the middle regardless of which way it faces, the classic exploded-parts look. Both first UNWELD (each triangle gets private corners carrying that triangle's flat face normal, exactly like the M-facet split) so neighbouring triangles separate cleanly instead of dragging shared corners; that also makes the exploded pieces flat-shade correctly. Header-only, pure CPU.  Scope note (honest): this multiplies the vertex count by 3× the triangle count (no sharing — that is the point; re-weld/`reindexMesh` if you set distance back to 0 and want the compact mesh back). Positions and normals are rewritten; UVs and colours ride along per corner. Degenerate (zero-area) triangles have no defined normal, so they are copied in place with no offset. Distance is in world units and may be negative (implode inward).

**Functions:**

- `inline math::vec3 explodeFaceNormal(const shapes::MeshData& mesh, std::size_t t)`
- `inline void emitExplodedTri(const shapes::MeshData& mesh, std::size_t t, math::vec3 off, math::vec3 nrm,`
- `inline shapes::MeshData explodeFaces(const shapes::MeshData& mesh, float distance)`
- `inline shapes::MeshData explodeFacesRadial(const shapes::MeshData& mesh, float distance, math::vec3 centre)`
- `inline shapes::MeshData explodeFacesRadial(const shapes::MeshData& mesh, float distance)`

### `MeshExtrude`
<sub>`engine/include/maz/render/MeshExtrude.hpp`</sub>

maz::render EXTRUDE FACES — raise every triangle off the surface into a little standing prism: each face is pushed OUT along its own normal by `distance` and the gap it leaves is walled in on all three sides, so a flat panel sprouts a field of raised studs / buttons / greebles / brick-relief. This is Blender's "Extrude Individual Faces" and the workhorse for turning a plain surface into panelled sci-fi hull detail, chunky pixel-art relief, or the raised keys of a keypad. Each input triangle becomes a self-contained prism: its TOP (the triangle moved out by `distance`, still facing the same way) plus three SIDE walls (a quad per original edge, bridging the base edge to the raised edge) — 7 triangles from 1. Header-only, pure CPU.  Scope note (honest): this extrudes each triangle INDEPENDENTLY (individual-faces mode) and unwelds, so a flat region tiled by many triangles raises each triangle as its own separate stud with walls along every interior edge, not one merged block — run it on a low-poly / already-panelled mesh, or merge coplanar triangles first, for clean single studs. Normals are set to each prism's TOP face normal (the side walls therefore shade like the top; re-run `computeNormals` for correct wall shading). The base outline stays where it was; `distance` may be negative to press faces INward. Multiplies the triangle count by 7 and the vertex count by 6.

**Functions:**

- `inline shapes::MeshData extrudeFaces(const shapes::MeshData& mesh, float distance)`

### `MeshExtrudePolygon`
<sub>`engine/include/maz/render/MeshExtrudePolygon.hpp`</sub>

maz::render LINEAR EXTRUDE / PRISM FROM A POLYGON — take any flat 2D shape and give it thickness, turning the outline into a solid 3D block. Draw a star, a gear, a heart, a letter of the alphabet, a company logo, an arrow, an L-shaped room footprint, or a staircase side-profile as a list of 2D points, and this stamps it out into a prism `depth` units thick. It is Godot's CSGPolygon3D in Depth mode / Blender's "extrude region" / the classic CAD linear-extrude, and the fastest way to make chunky 3D text, coins and medals, extruded signage, cookie-cutter props, pipes with a fancy cross-section, or blocky level geometry from a hand-drawn footprint. The flat shape is laid in the XY plane and pushed along Z, centred so it runs from z = -depth/2 to z = +depth/2. Two end caps (triangulated with the engine's ear-clipping `triangulatePolygon`) plus one quad wall per outline edge make a closed, solid prism. Header-only, deterministic, headless — pure CPU geometry.  Scope note (honest): the outline must be a SIMPLE polygon — no self-crossings and no holes (a donut needs a separate hole-aware path). Winding is auto-normalised, so either clockwise or counter-clockwise input works. The mesh is UNWELDED with flat per-face normals, so the caps and side walls read as crisp hard edges (ideal for a faceted prism); it is a closed, consistently-wound solid (signed volume == area x depth). Fewer than 3 points, or a zero-area outline, yields an empty mesh.

**Functions:**

- `inline shapes::MeshData extrudePolygon(const std::vector<math::vec2>& poly, float depth)`
- `inline shapes::MeshData extrudePolygonScaled(const std::vector<math::vec2>& poly, float depth, float topScale)`
- `inline shapes::MeshData extrudeRing(const std::vector<math::vec2>& outer, const std::vector<math::vec2>& inner,`

### `MeshFacet`
<sub>`engine/include/maz/render/MeshFacet.hpp`</sub>

maz::render FLAT-SHADING FACET SPLIT — rebuild a mesh so every triangle owns its three OWN vertices, each carrying that triangle's FACE normal. Because no vertex is shared between faces, the lighting can't blend across edges, so the surface renders faceted — every triangle a crisp flat plane. This is Blender's "Shade Flat" / the low-poly look: a sphere becomes a geodesic gem, terrain becomes stylized facets, and it is also the honest way to export a mesh whose faces really are flat (a cube should NOT have its corners smoothed). The inverse of smooth (shared-vertex, averaged-normal) shading — pair with computeNormals for the smooth version. Positions/colours/UVs are copied per corner; only the normal is replaced with the face normal. Header-only, std-only, deterministic — output has exactly 3·triangleCount vertices.  Scope note (honest): this INFLATES the vertex count (no sharing), which is the point — it is a display/export transform, not an optimization. Degenerate (zero-area) triangles get a zero normal (no valid facing) but are still emitted so the triangle set is preserved; drop them first with MeshCleanup if unwanted.

**Functions:**

- `inline shapes::MeshData facetMesh(const shapes::MeshData& mesh)`

### `MeshFeatureLines`
<sub>`engine/include/maz/render/MeshFeatureLines.hpp`</sub>

maz::render FEATURE-LINE EXTRACTION (ridge / valley crest lines) — find the SHARP FOLDS of a mesh, label each as a convex RIDGE or a concave VALLEY, and CHAIN them into connected polylines. Where M548 sharp-edge detection just answers "which edges are creased," this goes two steps further: it tells you which way each crease bends (a roof ridge vs a gutter valley) and stitches the loose creased edges into actual CURVES. Those curves are what stylized/NPR renderers stroke as ink outlines and interior "hard" lines, what retopo tools follow to lay clean edge loops, what auto-UV uses as natural seam candidates, and what a "select hard edges" editor command returns. Reuses the M528 half-edge topology; classification uses the sign of the fold relative to the outward face normals. Header-only, pure CPU.  Scope note (honest): ridge/valley sign needs OUTWARD-consistent winding (an inside-out mesh flips the labels); only manifold interior edges (exactly two faces) are considered — boundary and non-manifold edges are skipped. Chaining produces maximal simple paths and breaks at junctions (a vertex where three-plus feature edges meet), so a branching crest network comes back as several polylines meeting at the junction, not one tangled path.

**Types:** `FeatureKind`, `FeatureEdge`, `FeatureLines`

**Functions:**

- `inline FeatureLines extractFeatureLines(const shapes::MeshData& mesh, float sharpAngleDegrees = 30.0f)`

### `MeshFlatten`
<sub>`engine/include/maz/render/MeshFlatten.hpp`</sub>

maz::render FLATTEN / PROJECT-TO-PLANE — squash a mesh toward a flat plane: each vertex slides along the plane's normal toward its perpendicular projection onto the plane, blended by `t`. At t=0 nothing moves; at t=1 every vertex lands exactly on the plane (a pancake); in between the shape squashes smoothly. Uses: a cheap drop-shadow / blob-shadow caster (flatten a copy of a model onto the ground plane and render it dark), a decal or sticker baked onto a surface, a "pressed flat" squash-and-stretch pose, or projecting a prop onto a wall. The plane is a point + a normal; the normal is normalised internally so any length works. Header-only, CPU.  Scope note (honest): this moves POSITIONS only, straight along the plane normal — normals are left as they were, so a fully-flattened mesh keeps its original (now wrong) shading; for a lit pancake set the normals to the plane normal or re-run `computeNormals` afterwards. At t=1 the mesh is coplanar and has zero thickness (its two sides overlap — expect z-fighting if both are drawn; it is meant as a shadow/decal source, not a solid). `t` may exceed 1 (overshoot past the plane) or go negative (push away from it).

**Functions:**

- `inline shapes::MeshData projectToPlane(const shapes::MeshData& mesh, math::vec3 planePoint, math::vec3 planeNormal,`

### `MeshFlip`
<sub>`engine/include/maz/render/MeshFlip.hpp`</sub>

maz::render FLIP / REVERSE — deliberately turn a mesh inside-out: reverse every triangle's winding AND negate every vertex normal, so the surface faces the OTHER way. This is a different job from M562 winding-consistency (which only makes a mesh AGREE with itself): here you WANT the flip. Uses: build an inward-facing shell — a skybox, a room seen from inside, a cave interior, a hollow that culls its outer faces so you see the far walls; correct a whole model that imported inside-out in one call; or make a two-sided effect by MERGING (M572) a mesh with its flipped copy so both faces render under single-sided culling. Reversing the winding (swap the 2nd and 3rd corner of each triangle) flips which side back-face culling drops; negating the normals flips which way the surface shades. Header-only, pure CPU.  Scope note (honest): this negates STORED vertex normals and reverses triangle order — it does not recompute normals from geometry (if a mesh has none, negating zero stays zero; run computeNormals first). Positions, UVs and colours are untouched. `flipWinding` alone reverses culling without touching shading; `flipNormals` alone re-shades without changing culling; `flipMesh` does both (the usual "make it face inward").

**Functions:**

- `inline shapes::MeshData flipWinding(const shapes::MeshData& mesh)`
- `inline shapes::MeshData flipNormals(const shapes::MeshData& mesh)`
- `inline shapes::MeshData flipMesh(const shapes::MeshData& mesh)`

### `MeshGeodesic`
<sub>`engine/include/maz/render/MeshGeodesic.hpp`</sub>

maz::render MESH GEODESIC DISTANCE — the shortest "walk along the surface" distance from one or more source vertices to every other vertex, measured along the mesh's EDGES (Dijkstra on the vertex graph, edge weight = the 3D length of that edge). Straight-line distance cuts through the solid; geodesic distance is how far it actually is over the skin, which is what you want for: heat-map / falloff vertex weights (damage spreading from a wound, snow accumulating from a peak), texture-blend and vertex-paint masks that follow the form, region growing / "flood N metres from here" selection, feature-distance fields, and cheap procedural effects. Multi-source seeds every source at 0 in one pass (a discrete distance-to-nearest-feature field). The predecessor array reconstructs the actual shortest edge-path back to a source. Header-only, std-only, deterministic.  Scope note (honest): this is the EDGE-graph geodesic — the standard cheap approximation. It can only step vertex-to-vertex, so on a coarse mesh it slightly OVERESTIMATES the true smooth surface geodesic (which may cross triangle faces); it converges as the mesh is refined. Exact polyhedral geodesics (MMP / heat method) are the documented follow-up. Weld the mesh first (a shared corner must be one vertex) or islands stay disconnected — compose with MeshCleanup / MeshWeld.

**Types:** `GeodesicResult`

**Functions:**

- `inline GeodesicResult geodesicDistance(const shapes::MeshData& mesh,`
- `inline GeodesicResult geodesicDistance(const shapes::MeshData& mesh, std::uint32_t source)`

### `MeshHardEdges`
<sub>`engine/include/maz/render/MeshHardEdges.hpp`</sub>

maz::render HARD-EDGE / SMOOTHING-GROUP split by crease angle — the importer step that decides where a surface should shade SMOOTH (normals averaged across an edge) versus FLAT (a crisp crease): every edge whose two faces meet at more than the crease angle is a hard edge, and the shared vertices along it are DUPLICATED so the smooth-normal averaging doesn't bleed across the fold. This is Godot's import "Normals > From Smoothing Groups" / the shade-smooth-by-angle operation, and the correct front end to computeNormals: a raw cube welded to 8 vertices would otherwise get rounded, mushy corners. Built on the M528 topology: around each vertex the incident triangles are grouped (union-find) so neighbours joined by a SUB-threshold edge stay together, and each group becomes one output vertex with its own averaged normal. Pure CPU, header-only, headless.

**Functions:**

- `inline shapes::MeshData splitHardEdges(const shapes::MeshData& mesh, float creaseAngleDegrees)`

### `MeshHeightfield`
<sub>`engine/include/maz/render/MeshHeightfield.hpp`</sub>

maz::render HEIGHTFIELD / TERRAIN MESH — turn a flat grid of height numbers into a rolling 3D terrain surface. You hand it a `cols` × `rows` grid of heights (row-major: one float per grid point, e.g. straight out of a Perlin/fbm noise function, a greyscale heightmap image, or hand-authored contours) and it lays down a vertex at every grid point, lifts each one to its height, and stitches the whole sheet together with two triangles per cell. This is the bread-and-butter of outdoor game worlds — hills, dunes, valleys, ocean floors, golf courses — and it's exactly Godot's HeightMapShape3D / a terrain node's mesh. The grid is centred on the origin and laid on the XZ plane with height along +Y, so it drops straight into a scene. Smooth per-vertex normals come from the engine's area-weighted `computeNormals`, so lighting follows the slopes for free. Header-only, deterministic, headless — pure CPU geometry, no GPU needed to build or verify it.  Scope note (honest): this builds an open single-sided sheet (no skirt/underside/walls) — it is the terrain SURFACE, not a closed solid; add a skirt or extrude down if you need thickness or watertightness. `cellSize` is the world spacing between neighbouring grid points and `heightScale` multiplies the raw height values. Needs at least a 2×2 grid, and `heights.size()` must equal `cols*rows`, or an empty mesh is returned.

**Functions:**

- `inline shapes::MeshData buildHeightfield(const std::vector<float>& heights, int cols, int rows,`

### `MeshHoleFill`
<sub>`engine/include/maz/render/MeshHoleFill.hpp`</sub>

maz::render HOLE FILL / CAP — seal the open holes a mesh has, turning a leaky surface into a watertight solid. Where M566 REPORTS holes, this PATCHES them: for each open boundary loop it adds a centre vertex at the hole's average position and fans triangles from that centre to the rim, closing the gap. That's the "fill holes / make watertight" repair 3D-print prep, scan cleanup, and boolean/CSG post-processing all run so the mesh passes solid checks (volume, mass, inside/outside, printing). The rim comes from the M536 boundary-loop walk, whose ordering follows the existing faces' winding, so each cap triangle is wound to MATCH its neighbours (no flipped patch). A `maxEdges` limit fills only small holes (pinholes and cut faces) while leaving big openings — a deliberately open cup mouth, say — untouched. Header-only, pure CPU.  Scope note (honest): this is a simple centre-fan cap — ideal for small, roughly-flat or convex holes (cut faces, pinholes); a large or highly non-planar hole gets a valid but crude flat-ish patch (feed it to M540/ M564 smoothing or a remesh for a nicer surface). The added centre vertex copies the rim's average colour; its normal is left zero for a downstream computeNormals pass. Non-manifold edges are not repaired (a distinct problem). After filling, the mesh is edge-watertight over the capped loops (verify with M566).

**Types:** `HoleFillResult`

**Functions:**

- `inline HoleFillResult fillHoles(const shapes::MeshData& mesh, std::size_t maxEdges = 0)`

### `MeshIcosphere`
<sub>`engine/include/maz/render/MeshIcosphere.hpp`</sub>

maz::render ICOSPHERE / GEODESIC SPHERE — a round ball built from an icosahedron (a 20-sided die) repeatedly split into smaller triangles, giving a sphere whose triangles are all nearly the SAME size and shape. This is the good kind of sphere for most jobs: the everyday "UV sphere" (M28 makeSphere) crowds its triangles into tight pinch-points at the north and south poles, which shows up as ugly stretching on planets, blotchy shading, and uneven tessellation — the icosphere has no poles and no pinching, so it lights evenly and subdivides cleanly. Reach for it for planets and moons, explosion / shockwave domes, force-field bubbles, evenly-spread point scatters, low-poly rock/asteroid bases, and anywhere you want a sphere that looks the same from every angle. `subdivisions` controls smoothness: 0 is the raw 20-face icosahedron (a faceted gem), 1 = 80 faces, 2 = 320, each level multiplying the face count by 4. Header-only, deterministic, headless — pure CPU geometry.  Scope note (honest): normals are the exact analytic sphere normals (the normalized position), so shading is perfectly smooth with no extra `computeNormals` pass. UVs use the standard latitude/longitude mapping, which has the usual single wrap SEAM down one side and a slight pinch at the very top/bottom texel (shared by every lat-long sphere) — fine for solid colours, procedural/triplanar texturing, or a seam-tolerant map; re-unwrap if you need a perfect atlas. `subdivisions` is clamped to >= 0; the mesh is a closed watertight shell.

**Functions:**

- `inline shapes::MeshData makeIcosphere(float radius, int subdivisions)`

### `MeshInset`
<sub>`engine/include/maz/render/MeshInset.hpp`</sub>

maz::render INSET FACES — shrink every triangle IN PLACE toward its own centre, opening a gap between neighbouring faces. Each triangle keeps its shape and orientation but scales down about its centroid by `amount` (0 = untouched, 1 = collapsed to a point), so a solid surface becomes a field of shrunken tiles with dark seams between them: panel gaps on a spaceship hull, grout lines between floor tiles, a "greeble" panelled look, or the base ring for a per-face extrude/bevel. This is Blender's "Inset Faces → Individual" (the shrink part). It first UNWELDS (each triangle gets its own three corners carrying that triangle's flat face normal, so the tiles separate cleanly and flat-shade), then moves each corner a fraction `amount` of the way toward the triangle's centroid. Header-only, pure CPU.  Scope note (honest): this insets each triangle INDEPENDENTLY (Blender's "individual faces" mode), so a flat region tiled by many triangles gets a seam along every interior edge, not just the region's outline — run it on a low-poly / quad-like mesh, or merge coplanar triangles first, if you want gaps only between real panels. Positions move toward the per-triangle centroid; the centroid itself is preserved (so the tile stays put, just smaller). `amount` may exceed 1 (overshoot through the centre) or go negative (grow the tile). Triples the vertex count (3× triangles, no sharing).

**Functions:**

- `inline shapes::MeshData insetFaces(const shapes::MeshData& mesh, float amount)`

### `MeshLod`
<sub>`engine/include/maz/render/MeshLod.hpp`</sub>

maz::render mesh LOD selection — the CPU half of Godot's automatic mesh level-of-detail: as an object shrinks on screen, swap in a cheaper mesh so distant geometry costs less. The decision is screen-coverage based (like Godot's mesh_lod_threshold): project the object's bounding radius to a pixel size for the current camera, then pick the finest LOD whose pixel threshold it still meets; below the coarsest threshold the object can be culled entirely. A lod_bias multiplier and optional switch hysteresis (to stop LODs flickering at a boundary) round it out. Pure math — no GPU, no mesh data — so it unit-tests headlessly; the renderer just draws whichever index this returns.

**Types:** `LodChain`

**Functions:**

- `inline float projectedRadiusPixels(float radius, float distance, float fovYRadians,`

### `MeshMassProperties`
<sub>`engine/include/maz/render/MeshMassProperties.hpp`</sub>

maz::render MESH MASS PROPERTIES — compute the VOLUME, CENTER OF MASS, and full INERTIA TENSOR of a solid bounded by a closed triangle mesh, assuming uniform density. This is what a physics engine needs to make a custom (non-primitive) collider spin correctly: Physics3D already derives inertia for boxes/spheres/ capsules analytically, but an arbitrary imported hull had no way to get its real mass distribution. Uses the signed-tetrahedron method (Mirtich / Blow-Binstock): every triangle forms a tetrahedron with the origin, and the signed contributions of those tets sum to the exact integrals over the enclosed solid, independent of where the origin sits. Consistent winding is required (the sign is auto-corrected if the whole mesh is wound inward). Pure CPU, header-only, headless — verifiable against the closed-form values of a unit cube.  Results use double precision and unit density: `mass == volume`. Scale the inertia by real density (or desiredMass/volume) to get physical values; shift with the parallel-axis theorem for a non-centroid pivot.

**Types:** `MassProperties`

**Functions:**

- `inline MassProperties computeMassProperties(const shapes::MeshData& mesh)`

### `MeshMerge`
<sub>`engine/include/maz/render/MeshMerge.hpp`</sub>

maz::render MESH MERGE / CONCATENATE — glue several meshes into ONE mesh (one vertex buffer, one index buffer). This is the inverse of the M529 split-into-components and the workhorse of DRAW-CALL BATCHING: a scene with a hundred static props drawn as one combined mesh renders in a single draw call instead of a hundred, which is usually the biggest CPU win a renderer gets. It also backs "join selected" in an editor, flattening a set of pieces (each already placed via M571 applyTransform) into a single exportable object, and assembling procedural kit-bashed geometry from parts. Each source mesh's indices are re-based by the running vertex count so the triangles keep pointing at the right (now-appended) vertices; all vertex attributes (position, normal, UV, colour) come along unchanged. Header-only, pure CPU.  Scope note (honest): this is a pure concatenation — it does NOT weld coincident vertices at the seams between parts (run M525 weldVertices / M559 autoWeld afterwards if you need a watertight join) and it does NOT re-pack or deduplicate, so merging N copies of a mesh yields N× the vertices. Winding and attributes are preserved exactly as given; if the parts disagree on winding, fix with M562 after merging. Empty inputs are skipped.

**Functions:**

- `inline shapes::MeshData mergeMeshes(const std::vector<shapes::MeshData>& meshes)`
- `inline shapes::MeshData mergeMeshes(const shapes::MeshData& a, const shapes::MeshData& b)`

### `MeshMirror`
<sub>`engine/include/maz/render/MeshMirror.hpp`</sub>

maz::render MESH MIRROR / SYMMETRIZE — reflect a mesh across an axis-aligned plane and JOIN the reflection to the original, producing a symmetric whole: the "mirror modifier" every DCC tool has. Model one wing / one half of a face / the left side of a spaceship, mirror it, and get a seamless symmetric result — the standard modelling shortcut, also used to symmetrize a slightly-off scan and to fold a full mesh's data onto one half. Reflecting flips handedness, so each mirrored triangle's winding is REVERSED (and its baked normal negated) to keep faces pointing outward. Vertices lying on the mirror plane (within `weldEps`) are shared, not duplicated, so the seam is watertight (welded to the matching original vertex). `axis` picks the plane normal (0=X,1=Y,2=Z), `planeCoord` its position along that axis. Header-only, std-only, deterministic.  Scope note (honest): welds only along the mirror plane (the seam) — it does not re-weld the two halves elsewhere or dedup interior duplicates; run MeshCleanup/MeshWeld afterwards if the source itself had coincident verts. Vertices on the far side of the plane are still mirrored (the modifier assumes the source sits on one side); clip first if you need a strict half.

**Functions:**

- `inline shapes::MeshData mirrorMesh(const shapes::MeshData& mesh, int axis, float planeCoord,`

### `MeshNormalize`
<sub>`engine/include/maz/render/MeshNormalize.hpp`</sub>

maz::render NORMALIZE TO A TARGET BOX — recentre AND uniformly scale a mesh so it fills a chosen box. Imported models arrive at wildly different scales — one in metres, one in centimetres, one a thousand units tall — and off-centre to boot. This is the import-normalization companion to the M569 pivot snap: it fits any mesh into a consistent size (default: centred in a unit cube, longest side = 1) so a whole asset library shares one scale and pivot, thumbnails frame identically, and downstream tools (voxelize, SDF, sampling) get predictable extents. The scale is UNIFORM (one factor on all axes) so the shape never distorts; `fitLongest` scales the longest side to the target (the mesh fits inside the box) while the alternative fills every axis. Returns the transformed copy plus the exact scale and translation applied, so an inverse or a parent transform can undo it. Only vertex POSITIONS change. Reuses the mesh bounding box; header-only, pure CPU.  Scope note (honest): UNIFORM scale preserves proportions — the mesh is centred in the target box and touches it on its longest axis (fitLongest=true) or exactly fills it only if it already matches the box's aspect; it does NOT stretch to fill a non-cubic box on every axis (that would distort). Normals/UVs/colours are untouched (uniform scale keeps normals valid). A zero-extent (single-point or empty) mesh is returned unchanged with unit scale. This translates+scales only — it never rotates (align first via M568 if needed).

**Types:** `NormalizeResult`

**Functions:**

- `inline NormalizeResult normalizeToBox(const shapes::MeshData& mesh, math::vec3 targetSize = math::vec3(1, 1, 1),`

### `MeshPlanarRegions`
<sub>`engine/include/maz/render/MeshPlanarRegions.hpp`</sub>

maz::render COPLANAR REGION segmentation — group a mesh's triangles into maximal CONNECTED, near-PLANAR patches: the flat faces of a shape. It is the workhorse behind collision-hull simplification (one convex face instead of a triangle fan), greedy meshing, decal/lightmap chart seeding, and "select coplanar" in an editor. Flood-fill across the shared MeshTopology (M528) edge-neighbours, but only cross an edge when the neighbour triangle's face normal stays within `angleTolerance` of the region's SEED normal — so every triangle in a region is guaranteed planar to the seed within that tolerance (not just to its neighbour, which would let a region slowly bend away). A cube yields exactly six regions, a curved surface yields many. Pure CPU, header-only, headless.

**Types:** `CoplanarRegions`

**Functions:**

- `inline math::vec3 faceNormal(const shapes::MeshData& m, std::uint32_t tri)`
- `inline CoplanarRegions segmentCoplanarRegions(const shapes::MeshData& mesh, float angleToleranceDegrees)`

### `MeshPoissonPrune`
<sub>`engine/include/maz/render/MeshPoissonPrune.hpp`</sub>

maz::render POISSON-DISK PRUNE — thin a dense cloud of surface points down to an EVENLY-SPACED (blue-noise) subset: keep a point only if it is at least `minDistance` away from every point already kept. This is the step that turns raw surface samples (from `sampleSurfacePoints`, which are random and therefore clumpy — some points almost on top of each other, some gaps) into the tidy, no-two-too-close scatter you want when placing grass blades, pebbles, trees, bullet-holes/decals, or crowd spawn points across a mesh. Godot calls this "poisson disk sampling"; the classic use is "scatter N items on this surface but never let two overlap."  The method is dart-elimination with a spatial hash: walk the input in order and accept each point unless a kept point already sits within `minDistance`, using a grid keyed by `minDistance`-sized cells so the "is anything too close?" test only ever looks at the 27 neighbouring cells (O(1) expected, O(n) total). Because it walks in order, the INPUT ORDER is the priority order — the first point in a cluster wins its spot and the rest are dropped. `sampleSurfacePoints` already returns points in random order, so feeding its output straight in gives an unbiased blue-noise result; if you built the list some other structured way, shuffle it first. Header-only, std-only, deterministic (same input + same radius -> same kept subset).

**Functions:**

- `inline std::vector<SurfacePoint> prunePointsPoisson(const std::vector<SurfacePoint>& points, float minDistance)`
- `inline std::vector<SurfacePoint> scatterBlueNoise(const shapes::MeshData& mesh, float minDistance,`

### `MeshPrincipalAxes`
<sub>`engine/include/maz/render/MeshPrincipalAxes.hpp`</sub>

maz::render PRINCIPAL INERTIA AXES — the three natural spin axes of a solid mesh and how hard it is to spin about each. Every rigid body has three perpendicular axes about which it rotates cleanly (no wobble); a physics engine that wants realistic tumbling (a thrown plank spins easily end-over-end but resists rolling about its length) needs exactly this: the centre of mass, the three principal axes, and the moment of inertia about each. This takes the raw inertia TENSOR from `computeMassProperties` (M549) and DIAGONALISES it with the engine's symmetric Jacobi solver: the eigenvectors are the principal axes, the eigenvalues the principal moments. Axes come sorted by moment ascending, so `axis[0]` is the easiest to spin (the object's long direction) and `axis[2]` the hardest. Reuses `math::detail::jacobiEigen3`. Header-only, std-only, deterministic.  Scope note (honest): correct only for a CLOSED, consistently-wound solid (the volume integral needs a watertight surface — an open mesh gives `valid=false`). Values are at unit density (mass == volume); scale the moments by real density for physical units. Reads positions only. A degenerate / zero-volume mesh is invalid.

**Types:** `PrincipalAxes`

**Functions:**

- `inline PrincipalAxes computePrincipalAxes(const shapes::MeshData& mesh)`

### `MeshProjectedArea`
<sub>`engine/include/maz/render/MeshProjectedArea.hpp`</sub>

maz::render PROJECTED / FRONTAL AREA — how big a shadow does this model cast when you look at it from a given direction? It measures the area of the model's outline as projected onto the screen — the "frontal area" an engineer means by cross-section. That number drives a lot of game and sim math: aerodynamic / water drag (drag scales with frontal area), wind load on a structure, how much sunlight a solar panel or leaf catches, the size of the shadow a light casts, or how tightly a camera needs to frame an object. It sums the projected area of every triangle that FACES the given direction (a triangle's contribution is its area times how square-on it is), which for a closed convex shape is exactly the silhouette area. Reuses only vec3/cross/dot. Header-only, pure CPU.  Scope note (honest): this is EXACT for a convex closed mesh (a box, a sphere, a convex hull) — there the front-facing triangles tile the silhouette with no overlap. For a CONCAVE mesh it is an UPPER BOUND: folds that hide behind nearer surface still count, so the true visible silhouette can be smaller (use a rasterised/coverage method if you need the exact projected area of a concave shape). `direction` need not be unit length. The value is symmetric in the view direction (a closed mesh has the same frontal area from the front and the back).

**Functions:**

- `inline float projectedArea(const shapes::MeshData& mesh, const math::vec3& direction)`

### `MeshQuantize`
<sub>`engine/include/maz/render/MeshQuantize.hpp`</sub>

maz::render MESH VERTEX QUANTIZATION — the lossy-but-bounded attribute compression a glTF/Draco-style exporter runs to shrink a mesh: instead of a full 32-bit float per position/UV channel, snap each channel to an N-bit integer grid spanning that attribute's bounding box, storing the compact ints plus the box (origin + extent) needed to reconstruct. Dequantizing maps the grid back to floats. Because the grid step is extent / (2^bits - 1), the reconstruction error on any channel is BOUNDED by half a grid step — a guarantee this unit-tests directly. Godot's importer exposes the same idea (import compression / the meshoptimizer + Draco path). This is the CPU quantize/dequantize core (the bit-packing to disk is a separate transport concern); it works on the geometry buffers headlessly, no GPU.  Scope note (honest): uniform per-attribute-bbox quantization of positions and UVs with a provable error bound — the workhorse that gives most of the size win. Normal/tangent octahedral encoding and entropy coding of the index stream are documented follow-ups.

**Types:** `QuantizedMesh`

**Functions:**

- `inline std::uint32_t quantChannel(float v, float mn, float extent, std::uint32_t maxLevel)`
- `inline float dequantChannel(std::uint32_t q, float mn, float extent, std::uint32_t maxLevel)`
- `inline QuantizedMesh quantizeMesh(const shapes::MeshData& mesh, int bits)`
- `inline shapes::MeshData dequantizeMesh(const QuantizedMesh& q)`

### `MeshRecenter`
<sub>`engine/include/maz/render/MeshRecenter.hpp`</sub>

maz::render PIVOT SNAP / RECENTER — move a mesh's PIVOT (the point that ends up at the world origin) to a sensible place. Imported models land wherever the exporter left them — floating off-axis, pivot in a random corner — which makes them awkward to place, rotate, and scale in a scene. This recentres the geometry so its pivot sits at the origin, choosing the pivot by intent: the BOUNDING-BOX CENTRE (spin-in-place props), the BASE (characters, trees, furniture that stand on the ground — bottom-centre), the CENTRE OF MASS (physics bodies that should rotate about their true balance point), or the VERTEX AVERAGE (a cheap centroid). It returns a recentred copy plus the applied offset and the world-space pivot, so callers can compensate a parent transform. Reuses the M532 mass-properties centroid for the centre-of-mass mode. Header-only, pure CPU.  Scope note (honest): this only TRANSLATES — it never rotates or scales (pair with M568 dominant-plane to also align, or a normalize-to-box pass to also scale). CENTRE OF MASS needs a closed, consistently-wound solid (M566/M562); on an open or degenerate mesh it falls back to the vertex average and sets `fellBack`. BASE uses the supplied `up` axis (default +Y): the pivot is the bbox centre in the two perpendicular axes and the minimum extent along `up`. Only vertex POSITIONS move; normals/UVs/colours are untouched.

**Types:** `RecenterResult`

**Functions:**

- `inline RecenterResult recenterMesh(const shapes::MeshData& mesh, PivotMode mode = PivotMode::BBoxCenter,`

### `MeshReindex`
<sub>`engine/include/maz/render/MeshReindex.hpp`</sub>

maz::render INDEX / DEDUPLICATE — turn a "triangle soup" (a mesh where every triangle carries its own three corners, so shared corners are stored two, three or six times over) into a compact INDEXED mesh: keep one copy of each truly-identical vertex and point every triangle at it through a fresh index buffer. This is the cleanup most of the engine's own shape builders, CSG output, marching-cubes/SurfaceNets output, and flat OBJ/STL imports need — they emit unshared corners, which bloats the vertex buffer and stops the GPU's post-transform vertex cache from ever hitting. Godot's SurfaceTool.index() is exactly this.  The key difference from M-weld (position welding): this merges vertices ONLY when EVERY attribute matches bit for bit — position AND normal AND colour AND UV. That is deliberately conservative: two corners that sit at the same point but carry different normals (a hard crease) or different UVs (a texture seam) are LEFT SEPARATE, because collapsing them would smooth the crease or tear the texture. Position welding (which ignores normals and UVs) is the tool when you WANT to fuse a seam; reindexing is the tool when you want a smaller buffer with the look completely unchanged. Degenerate triangles (two corners that were already the same vertex) are dropped. Header-only, std-only, deterministic — the first occurrence of each unique vertex keeps its slot, so output order is stable.

**Types:** `ReindexReport`, `VertexKey`, `VertexKeyHash`

**Functions:**

- `inline VertexKey keyOf(const MeshVertex& v)`
- `inline ReindexReport reindexMesh(const shapes::MeshData& mesh)`

### `MeshRevolve`
<sub>`engine/include/maz/render/MeshRevolve.hpp`</sub>

maz::render REVOLVE / LATHE — spin a 2D outline (a "profile") around the vertical axis to build a solid of revolution. Give it the silhouette of a vase, a bottle, a wine glass, a wheel, a chess pawn, a lamp base, a bowl — a list of (radius-from-the-axis, height) points tracing the side view — and it sweeps that outline all the way around, stitching a smooth surface. This is the single most productive way to model any round object: it's Blender's "Spin", a wood-lathe in software, Godot's CSGPolygon3D in Spin mode. The profile is a polyline in the RADIUS-HEIGHT plane; the sweep runs around the +Y axis in `segments` angular steps. A partial sweep (`sweepRadians` < 2π) makes an open fan / arc wedge; a full turn makes a closed round body. Reuses the engine's area-weighted `computeNormals` so the result shades smoothly out of the box. Header-only, pure CPU, deterministic — no GPU needed to build or verify the geometry.  Scope note (honest): this builds only the swept SIDE surface. It does NOT add end caps — a profile that stops short of the axis leaves the top/bottom open (a tube), exactly like a lathe with no facing cut; to close an end, run the profile down to radius 0 (a point on the axis, which forms a natural cone tip) or cap it later. Profile points that sit on the axis (radius 0) collapse to a single pole vertex per ring, so the degenerate zero-area triangles they would make are skipped, giving clean cone tips. Winding is outward (normals point away from the axis) for a positive-radius profile. Needs >= 2 profile points and >= 3 segments.

**Types:** `ProfilePoint`

**Functions:**

- `inline shapes::MeshData revolveProfile(const std::vector<ProfilePoint>& profile, int segments,`

### `MeshRipple`
<sub>`engine/include/maz/render/MeshRipple.hpp`</sub>

maz::render RIPPLE / WAVE — send concentric ripples across a surface: each vertex is pushed along one axis by a sine wave of its DISTANCE from a centre point, so a flat plane becomes a pond after a stone drops, a disc becomes a vinyl-record warp, a flag gets a rippling wobble. This is the "Ripple"/"Wave" deformer every DCC tool ships. The push along `axis` is `amplitude · sin(radial · frequency − phase)` where `radial` is the distance from `centre` measured in the plane perpendicular to `axis` (so the rings are truly circular). `frequency` sets how tightly packed the rings are, `amplitude` how tall the crests, and animating `phase` makes the rings travel outward (or inward) — the whole animated-water effect is just `phase += speed · dt` each frame. Deterministic and pure. Header-only, CPU.  Scope note (honest): this offsets POSITIONS only, along a single axis — normals are left as they were, so the stored shading no longer matches the rippled surface; re-run `computeNormals` afterwards for correct lighting on the crests and troughs. The wave's smoothness is limited by the mesh's tessellation along the radius: too few rings of vertices and the sine reads as jagged facets (subdivide first). `amplitude`/`frequency`/`phase` may be any value; frequency 0 pushes the whole surface by a constant `amplitude·sin(−phase)`.

**Functions:**

- `inline shapes::MeshData rippleMesh(const shapes::MeshData& mesh, int axis, float amplitude, float frequency,`

### `MeshSdf`
<sub>`engine/include/maz/render/MeshSdf.hpp`</sub>

maz::render SIGNED DISTANCE FIELD bake — sample the signed distance to a closed mesh's surface onto a 3D grid: NEGATIVE inside the solid, POSITIVE outside, ~0 on the surface. An SDF is the shared currency behind a lot of engine tech — soft/contact shadows, ambient occlusion, collision & penetration depth, smooth CSG/booleans, raymarched volumes, and flow-field navigation around obstacles — and Godot's SDFGI / 2D SDF collision use exactly this. The unsigned distance at each grid point is the minimum distance to any triangle (exact closest-point-on-triangle); the SIGN comes from a ray-parity inside/outside test (odd crossings = inside), reusing the M533 Möller–Trumbore ray/triangle. `sampleMeshSdf` trilinearly interpolates the grid at an arbitrary point. Pure CPU, header-only, headless — verifiable against a cube's closed-form distance.  Scope note (honest): brute-force O(cells · tris) with a ray-parity sign (assumes a watertight mesh) — fine for offline bakes of props/levels; a BVH (game::Bvh) to accelerate the nearest-triangle search and a generalized-winding-number sign for open meshes are the documented follow-ups.

**Types:** `MeshSdf`

**Functions:**

- `inline math::vec3 closestOnTriangle(const math::vec3& p, const math::vec3& a, const math::vec3& b,`
- `inline MeshSdf bakeMeshSdf(const shapes::MeshData& mesh, int resolution, float padding)`
- `inline float sampleMeshSdf(const MeshSdf& sdf, const math::vec3& p)`

### `MeshSharpEdges`
<sub>`engine/include/maz/render/MeshSharpEdges.hpp`</sub>

maz::render DIHEDRAL-ANGLE / SHARP-EDGE DETECTION — for every interior edge, the dihedral angle between the two triangles that share it (0° = the faces fold flat back on themselves, 180° = perfectly flat/coplanar), and the list of edges whose faces bend more sharply than a threshold. Sharp edges are the CREASES of a model — a cube's twelve rims, the fold of a roof, the lip of a cup — and detecting them drives: wireframe/crease overlay in an editor, automatic bevel/chamfer selection, UV-seam and hard-normal suggestions (creases usually want a seam and a split normal), and feature-preserving simplification/smoothing that must not round a crease off. The angle is derived from the two face normals across the shared edge; boundary and non-manifold edges (not exactly two faces) are skipped. Reuses MeshTopology (M528) for the twin-face lookup. Header-only, std-only.  Scope note (honest): "sharpness" is the deviation from flat — `sharpAngleDegrees` is the threshold on (180° − dihedral), so 0 flags nothing, 30 flags a moderate crease, 90 only right-angle-or-sharper folds. Uses geometric face normals (winding-consistent); a mesh with inconsistent winding may mis-sign an angle (reported unsigned). Weld first so a crease is one edge, not two boundary edges.

**Types:** `SharpEdge`, `SharpEdgeResult`

**Functions:**

- `inline SharpEdgeResult detectSharpEdges(const shapes::MeshData& mesh, float sharpAngleDegrees = 30.0f)`

### `MeshSilhouette`
<sub>`engine/include/maz/render/MeshSilhouette.hpp`</sub>

maz::render SILHOUETTE / OUTLINE EDGES — find the edges that form a model's OUTLINE as seen from a particular direction: the crisp boundary between the parts of the surface facing the camera and the parts facing away. On a sphere seen from the front that's the circle around its rim; on a cube seen corner-on it's a hexagon. Unlike the engine's sharp/hard/feature-edge tools (M548/M549/M567), which mark folds baked into the geometry no matter where you look, a silhouette is VIEW-DEPENDENT — it slides around the surface as the camera moves. It's the thing you need for cartoon / ink outlines (draw a fat line along the silhouette), hidden-line and blueprint looks, pencil shading, and building shadow-volume "caps" for stencil shadows. An edge is on the silhouette when one of the two triangles sharing it faces TOWARD the view and the other faces AWAY; an open boundary edge (only one triangle) is always on the outline. Whether the mesh's normals point in or out doesn't matter — flipping them all swaps which side is "front" but leaves the front-vs-back BOUNDARY exactly where it was. Header-only, pure CPU.  Scope note (honest): faces are tested by their flat geometric normal (from the winding), so the mesh should be consistently wound. A triangle seen exactly edge-on (its normal perpendicular to the view) counts as facing away — a deliberate tie-break, only reachable when a face is precisely side-on to the view. `silhouetteEdges` takes a single view DIRECTION (orthographic / distant camera); `silhouetteEdgesFromEye` uses a camera POSITION, testing each face against its own direction to the eye (correct for a near, perspective camera). Both return the silhouette edges as undirected vertex-index pairs; `boundaryCount` says how many of them are open-boundary edges.

**Types:** `SilhouetteResult`

**Functions:**

- `inline math::vec3 faceNormalOf(const shapes::MeshData& m, std::size_t t)`
- `inline math::vec3 faceCentroidOf(const shapes::MeshData& m, std::size_t t)`
- `inline SilhouetteResult silhouetteCore(const shapes::MeshData& mesh, FrontFn frontFacing, bool includeBoundary)`
- `inline SilhouetteResult silhouetteEdges(const shapes::MeshData& mesh, const math::vec3& viewDir,`
- `inline SilhouetteResult silhouetteEdgesFromEye(const shapes::MeshData& mesh, const math::vec3& eye,`

### `MeshSimplify`
<sub>`engine/include/maz/render/MeshSimplify.hpp`</sub>

maz::render mesh simplification by vertex clustering — the load-time decimation an engine runs to generate lower-poly LODs and cheap collision hulls from a dense source mesh, the way Godot's importer auto-generates LODs. It overlays a uniform grid of `cellSize` over the mesh, merges every vertex that falls in the same cell into one representative (the average of its members — position, normal, color, UV), remaps the triangles to those representatives, and drops any triangle whose corners collapsed together (now degenerate). Coarser cells → fewer vertices and triangles, monotonically. Unlike quadric-error edge collapse it is O(n), order-independent, and never produces holes or flipped normals — the robust choice for collision proxies and distant LODs. Pure CPU vertex math (no GPU), so it unit-tests headlessly: the output has fewer verts/tris, its bounding box stays within one cell of the original, every triangle is non-degenerate, and all indices are in range.  Scope note (honest): uniform-grid clustering (fast, robust, shape-preserving at the cell scale). Feature- preserving quadric-error edge collapse — which better keeps silhouettes at aggressive ratios — is the heavier follow-up; clustering is the dependable base every engine ships first.

**Types:** `CellKey`, `CellKeyHash`

**Functions:**

- `inline std::int64_t cellCoord(float v, float cellSize)`
- `inline shapes::MeshData simplifyClustering(const shapes::MeshData& in, float cellSize)`

### `MeshSimplifyQuadric`
<sub>`engine/include/maz/render/MeshSimplifyQuadric.hpp`</sub>

maz::render feature-preserving mesh simplification by QUADRIC-ERROR EDGE COLLAPSE (Garland–Heckbert QEM) — the higher-quality decimation an engine's importer runs to make LODs that keep their silhouette at aggressive triangle budgets, where the O(n) vertex-clustering path (MeshSimplify.hpp) would visibly round off edges. Each vertex carries a 4×4 error quadric (the summed squared distance to the planes of its incident triangles); collapsing an edge merges the two quadrics, and the cost of the collapse is the quadric evaluated at the optimal merged position (found by solving a 3×3 system, or falling back to the cheaper of the two endpoints / their midpoint when that system is singular). A min-heap always collapses the cheapest edge next, so flat regions decimate first and creases/boundaries — which have high quadric error — survive. Pure CPU vertex math (no GPU), so it unit-tests headlessly: fewer verts/tris toward the requested budget, every triangle non-degenerate + in range, the bounding box and a curved surface's peak preserved, and deterministic (same input → same output).  Scope note (honest): manifold-oriented QEM with area-weighted quadrics and optimal-position placement. It complements — does not replace — simplifyClustering: clustering is the fast, hole-proof choice for collision proxies and far LODs; QEM is the silhouette-preserving choice for visible mid LODs. Attribute (normal/color/UV) handling is position-driven: normals are recomputed on the result; color/UV are carried from the surviving representative. Non-manifold input still simplifies but without manifold guarantees.

**Types:** `Quadric`

**Functions:**

- `inline shapes::MeshData simplifyQuadric(const shapes::MeshData& in, std::size_t targetTriangles)`

### `MeshSkin`
<sub>`engine/include/maz/render/MeshSkin.hpp`</sub>

maz::render SKIN / LOFT ACROSS SECTIONS — stretch a smooth surface over a stack of cross-section "ribs", like pulling a skin over the frames of a boat hull or an aeroplane fuselage. You give it an ordered list of rings (each ring is a loop of 3D points — the outline of the shape at that station), and it bridges every rib to the next with a band of triangles, so the shape flows from one outline into the next. Unlike sweep (which drags ONE fixed profile along a path), each rib here can be a DIFFERENT size and shape, so the surface can taper, bulge, twist, or morph: a funnel (big ring → small ring), a boat hull (keel → beam → stern), a vase whose silhouette changes freely, a tube that fairs from a circle into a square. This is Blender's "Bridge Edge Loops" / classic CAD lofting. Reuses the engine's area-weighted `computeNormals` for smooth shading. Header-only, deterministic.  Scope note (honest): every rib must have the SAME number of points (point j of one rib connects to point j of the next — there is no resampling), and the ribs should be given in order along the body. `closedRings` (default true) picks whether each rib is a closed loop (a tube) or an open strip; `closedPath` (default false) also bridges the last rib back to the first for a closed torus-like body. This builds the swept SIDE skin only — the two end ribs are left OPEN (cap them separately for a closed solid). Needs >= 2 ribs, each with >= 2 points.

**Functions:**

- `inline shapes::MeshData skinSections(const std::vector<std::vector<math::vec3>>& sections,`

### `MeshSlice`
<sub>`engine/include/maz/render/MeshSlice.hpp`</sub>

maz::render MESH PLANE SLICE / CROSS-SECTION — intersect a triangle mesh with an infinite plane and return the CONTOUR: the line segments where the surface crosses the plane, chained into ordered (closed on a watertight solid) polyline loops. This is the cross-section a CAD tool draws, and the building block for cutaway / section views, a waterline or lava-line on a hull, terrain contour ("topographic") lines at a set of heights, silhouette/outline extraction, deriving a 2D collision outline from a 3D prop, and 3D-printing slicers. Each triangle straddling the plane contributes one segment between the crossing points on its two sign-changing edges. The crossing on each mesh edge is keyed by that undirected edge (the two triangles sharing it produce the SAME point), so on a manifold every contour point has exactly two incident segments and the chain closes into clean loops. Reuses only vec3/dot from maz::math. Header-only, deterministic, headless — a cube sliced through its middle yields a square loop.  Scope note (honest): the plane is {x : dot(normal, x) == offset}; `normal` sets orientation and need not be unit length. A vertex exactly on the plane counts to the non-positive side, and a triangle lying flat in the plane contributes no 1D contour (both are documented). Chaining assumes a manifold cut; a non-manifold edge (>2 triangles) may leave an open chain, which is reported (loopClosed==0), never silently closed.

**Types:** `SliceContour`

**Functions:**

- `inline SliceContour sliceMesh(const shapes::MeshData& mesh, const math::vec3& normal, float offset)`

### `MeshSliceLayers`
<sub>`engine/include/maz/render/MeshSliceLayers.hpp`</sub>

maz::render SLAB SLICING — cut a mesh into a STACK of evenly-spaced cross-sections along one axis and return each layer's contour. This is what a 3D-printer / laser-cutter slicer does before it prints: chop the model into N horizontal slabs and trace the outline of each so the machine knows where to lay material or cut. It is also the way to build a "topographic" contour set (a hill drawn as stacked height rings), a stack of collision cross-sections, or a layered cutaway preview. It simply calls the engine's `sliceMesh` (M532) at N plane heights spanning the mesh's extent along the chosen axis and collects the resulting polyline loops per layer. Header-only, deterministic.  Scope note (honest): each layer is exactly what `sliceMesh` returns (ordered loops, closed on a watertight solid). By default the N planes sample the layer CENTRES — heights (i+0.5)/N across the bounding box — which avoids landing a plane exactly on the flat top/bottom cap (where a coplanar face gives a degenerate contour); pass `sampleEdges=true` to place the planes at the layer boundaries instead. axis 0=X, 1=Y (default), 2=Z. A mesh with no extent along the axis, or count < 1, yields no layers.

**Types:** `MeshLayers`

**Functions:**

- `inline MeshLayers sliceLayers(const shapes::MeshData& mesh, int axis, int count, bool sampleEdges = false)`

### `MeshSmooth`
<sub>`engine/include/maz/render/MeshSmooth.hpp`</sub>

maz::render Laplacian / Taubin mesh smoothing — relax the vertices of an indexed triangle mesh toward the average of their neighbours, ironing out noise and faceting WITHOUT changing the topology (no vertices added or removed, only moved). It is the denoise pass for a mesh built from noisy data: a marching-cubes isosurface, a heightfield perturbed by fbm, a scanned/voxelised blob. Plain Laplacian smoothing shrinks the shape (every point drifts inward); the Taubin lambda|mu two-pass alternates a positive (smoothing) and a slightly larger negative (unshrinking) step so the surface relaxes while its volume is preserved — the standard low-pass mesh filter. Boundary vertices can be pinned so open edges keep their shape. Reuses the engine's positions+indices mesh form (MeshTools / subdivision / weld family). Godot exposes no runtime mesh smoothing to gameplay code, so this is a beyond-Godot geometry utility. Header-only, std-only, deterministic.

**Functions:**

- `inline void buildAdjacency(std::size_t vertexCount, const std::vector<std::uint32_t>& indices,`
- `inline void laplacianPass(std::vector<math::vec3>& pos,`
- `inline std::vector<math::vec3> smoothMeshLaplacian(const std::vector<math::vec3>& positions,`
- `inline std::vector<math::vec3> smoothMeshTaubin(const std::vector<math::vec3>& positions,`

### `MeshSnapGrid`
<sub>`engine/include/maz/render/MeshSnapGrid.hpp`</sub>

maz::render SNAP-TO-GRID — round every vertex position onto a regular WORLD grid: pick a grid step (say 0.25 units) and each vertex jumps to the nearest multiple of it. This is the "tidy up" pass for CAD-like or block/voxel-style meshes: it removes the tiny floating-point drift that creeps in from modelling, rotation, or import (so 1.0000001 and 0.9999998 both become a clean 1.0), and it makes vertices that were ALMOST at the same spot land EXACTLY on it — which then lets the bit-exact reindex (M575) actually fuse them. Godot modellers reach for "snap to grid" for exactly this kind of clean, aligned geometry.  This is a DIFFERENT job from mesh quantization (`MeshQuantize`): that packs positions into N-bit integers relative to the mesh's bounding box to shrink the FILE (a compression/transport concern, with a per-mesh grid that changes with the box); this snaps to a fixed ABSOLUTE world grid you choose, purely to clean up the geometry — the vertices stay full 32-bit floats, just rounded. The report tells you how many vertices actually moved and by how much, so you can see whether the step was gentle (drift cleanup) or aggressive (block-ifying).  Scope note (honest): this rounds POSITIONS only — normals, colours and UVs are untouched, so after an aggressive snap the stored normals may no longer match the flattened geometry (re-run `computeNormals` if the shading looks off). Snapping can pull two triangles' corners onto the same grid point and so create degenerate (zero-area) triangles; follow with `reindexMesh`/`weldVertices` to drop them. A grid step of 0 (or negative) on an axis leaves that axis untouched, so you can snap only X/Z (ground plane) and leave height free. Header-only, deterministic.

**Types:** `SnapResult`

**Functions:**

- `inline SnapResult snapVerticesToGrid(const shapes::MeshData& mesh, math::vec3 step,`
- `inline SnapResult snapVerticesToGrid(const shapes::MeshData& mesh, float step,`

### `MeshSolidify`
<sub>`engine/include/maz/render/MeshSolidify.hpp`</sub>

maz::render SOLIDIFY / SHELL — give a paper-thin surface real THICKNESS: take a one-sided sheet (a plane, a curved patch, a cloth, an open terrain skirt, a leaf) and turn it into a closed solid slab with a front face, a back face, and a rim sealing the two together along the open edges. This is Blender's "Solidify" modifier and the standard fix for surfaces that look like they vanish when seen edge-on or that leak light because they have no back: a wall built from a single quad, an imported single-sided mesh, a heightmap patch you want to render as a solid block. The back face is the front pushed inward along each vertex's (smooth) normal by `thickness` and wound the opposite way; the rim bridges every boundary (open) edge, so the result is watertight whenever the input was a clean manifold-with-boundary. Reuses the engine's area-weighted `computeNormals`. Header-only.  Scope note (honest): the shell offsets each vertex straight along its smooth normal — correct for gentle surfaces, but on a very sharp concave crease the inner offsets can cross and self-intersect (Blender's "complex" solidify avoids this; this is the fast "simple" mode). The rim reuses the existing front/back vertices rather than adding creased rim vertices, so rim shading is smooth rather than hard-edged (run `computeNormals`/facet after if you want crisp rim edges). `thickness` may be negative to push the shell the other way. A CLOSED input (no boundary edges) just gets a second inner shell and no rim (`hadBoundary=false`).

**Types:** `SolidifyResult`

**Functions:**

- `inline SolidifyResult solidifyMesh(const shapes::MeshData& mesh, float thickness)`

### `MeshSolidity`
<sub>`engine/include/maz/render/MeshSolidity.hpp`</sub>

maz::render MESH SOLIDITY (convexity) RATIO — how CONVEX is a shape? Wrap the mesh in its convex hull (the tightest shape with no dents — imagine shrink-wrapping it) and compare the mesh's own enclosed volume to the hull's. The ratio (mesh volume ÷ hull volume) is 1.0 for a perfectly convex solid (a cube, a ball, a die) and drops toward 0 the more the shape caves in — a bowl, a cog, a chair, a tree. This "solidity" is a one-number convexity score meshing and analysis tools report: it drives LOD/collision decisions (a near-convex prop can use its cheap hull as a collider), flags whether a boolean/CSG result stayed solid, and classifies shapes (blobby vs branchy) for procedural placement. Reuses the M532 mass-properties volume and the M292 convex-hull builder; the hull's volume comes from summing signed tetrahedra over its faces. Header-only, pure CPU.  Scope note (honest): the mesh volume needs a CLOSED, consistently-wound surface (an open shell reports invalid — weld and orient it first via M525/M562); winding sign is auto-corrected so an inside-out but closed mesh still measures. The hull is built from the mesh's vertices, so solidity captures concavity of the VERTEX cloud, not sub-vertex surface ripple. Numerically solidity can sit a hair above 1.0 on a convex mesh from floating- point; clamp if you need a strict [0,1].

**Types:** `SolidityReport`

**Functions:**

- `inline SolidityReport analyzeSolidity(const shapes::MeshData& mesh)`

### `MeshSpherify`
<sub>`engine/include/maz/render/MeshSpherify.hpp`</sub>

maz::render SPHERIFY / CAST-TO-SPHERE — inflate a mesh toward a perfect sphere: each vertex is pulled from where it is toward the point on a sphere of radius `radius` (about `centre`) that lies along its own direction from the centre, blended by `t`. At t=0 nothing moves; at t=1 every vertex sits exactly on the sphere; in between the shape smoothly rounds out. This is Blender's "Cast" modifier (sphere target) and the classic way to round a blocky low-poly shape — turn a subdivided cube into a ball, puff an angular rock smooth, or morph between a boxy and a rounded silhouette by animating `t`. Position AND normal blend toward the radial (outward-from- centre) direction, so the lighting rounds out with the shape. Header-only, pure CPU, deterministic.  Scope note (honest): the roundness you get is limited by the mesh's tessellation — spherifying an 8-vertex cube just moves its 8 corners onto the sphere (still an octahedron-ish shell); subdivide first (Subdivision) so there are enough vertices to actually read as round. A vertex sitting exactly at `centre` has no direction to cast along, so it is left in place. `t` outside [0,1] is allowed (t>1 overshoots past the sphere, t<0 pushes inward away from it); the usual range is 0..1.

**Functions:**

- `inline shapes::MeshData spherifyMesh(const shapes::MeshData& mesh, float radius, float t,`

### `MeshStats`
<sub>`engine/include/maz/render/MeshStats.hpp`</sub>

maz::render MESH STATISTICS — the at-a-glance size-and-scale report an editor's mesh-info panel or an import log shows: the axis-aligned BOUNDING BOX (min/max/size/centre), the area-weighted CENTROID (the surface's balance point), the total SURFACE AREA, and the edge-length distribution (shortest/longest/mean — a quick read on tessellation uniformity and whether the mesh is scaled sanely). It answers "how big is this thing, where is it centred, how dense is it" before you place, scale, texture (texel density needs area), or LOD it. Distinct from TriangleQuality (M537, per-triangle SHAPE) and MeshMassProperties (M532, the SOLID's volume/inertia): this is the SURFACE's extent and area. Reuses only vec3/cross from maz::math. Header-only, deterministic.  Scope note (honest): the centroid is AREA-weighted over triangles (the balance point of the shell), not the vertex average and not the solid's centre of mass (that is MeshMassProperties). Edge stats count each undirected edge once. Surface area sums triangle areas as-is (a double-sided or self-overlapping mesh counts the overlap twice — accurate to the triangles present).

**Types:** `MeshStats`

**Functions:**

- `inline MeshStats analyzeMesh(const shapes::MeshData& mesh)`

### `MeshStrip`
<sub>`engine/include/maz/render/MeshStrip.hpp`</sub>

maz::render TRIANGLE-STRIP GENERATION — repack an indexed triangle LIST into triangle STRIPS. A strip stores a run of triangles as one vertex sequence v0 v1 v2 v3 ..., where every new vertex forms a triangle with the previous two (GPU GL_TRIANGLE_STRIP / VK primitive-restart semantics), so N connected triangles cost N+2 indices instead of 3N. Strips shrink index bandwidth, feed fixed-function / mobile / retro GPU paths that prefer strips, and are the on-disk form some formats (MD2/MD3, PS2/GameCube era) want. This is a simple greedy stripifier: start a triangle, orient it so its trailing edge has an un-stripped neighbour, then walk neighbour-to-neighbour across the trailing edge until the run dead-ends; separate runs by a restart index. `expandTriangleStrips` is the exact inverse (strips -> triangle list), so the pair round-trips the triangle SET losslessly. Header-only, std-only, deterministic.  Scope note (honest): greedy, not length-optimal (NvTriStrip / tipsify find longer runs); it minimises index count opportunistically, not maximally. Winding within a strip alternates per GPU convention, so the expanded list preserves the triangle SET (same three vertices per face) though a face's winding may be normalised.

**Types:** `TriangleStrips`

**Functions:**

- `inline TriangleStrips buildTriangleStrips(const std::vector<std::uint32_t>& triList,`
- `inline TriangleStrips buildTriangleStrips(const shapes::MeshData& mesh, std::uint32_t restart = 0xFFFFFFFFu)`
- `inline std::vector<std::uint32_t> expandTriangleStrips(const TriangleStrips& strips)`

### `MeshSurfaceSample`
<sub>`engine/include/maz/render/MeshSurfaceSample.hpp`</sub>

maz::render AREA-WEIGHTED SURFACE POINT SAMPLING — scatter N points uniformly across a mesh's SURFACE, so every unit of area is equally likely to be picked (a triangle twice as big gets twice as many points). This is the seed for scattering grass, rocks, foliage, or debris over terrain and props; for generating a point cloud from a mesh; for placing decals or spawn points; and as the input to blue-noise / Poisson-disk relaxation. Each point carries its world position, the face normal there (to orient what you place), and the triangle it landed on. Picking is exact: a per-triangle area CDF chooses the face (binary search on a uniform draw), then a standard barycentric warp (u=1−√r1, …) places the point uniformly inside it. Deterministic — the same `seed` gives the same points every run. Reuses only vec3 from maz::math; self-contained RNG. Header-only.  Scope note (honest): uniform over AREA, not blue-noise — points can clump; feed these into a relaxation pass if you need even spacing. Degenerate (zero-area) triangles are never chosen. The normal is the geometric FACE normal (winding-derived), not the interpolated smooth normal.

**Types:** `SurfacePoint`

**Functions:**

- `inline std::vector<SurfacePoint> sampleSurfacePoints(const shapes::MeshData& mesh, std::size_t count,`

### `MeshSweep`
<sub>`engine/include/maz/render/MeshSweep.hpp`</sub>

maz::render SWEEP ALONG A PATH / LOFT — take a flat 2D cross-section (a "profile") and push it down a 3D path, leaving a solid tube of that shape behind it. Give it a circle and a curvy path and you get a pipe, cable, rope, wire, garden hose, or tentacle; give it a rectangle and you get a rail, a moulding, a road ribbon, a fence beam; give it a star or an L and you get an extruded girder or trim. This is Blender's "Curve → bevel/taper" sweep and Godot's CSGPolygon3D in Path mode — the standard way to build anything long and bendy that follows a line.  The tricky part of sweeping is keeping the cross-section from spinning wildly as the path curves. This uses a ROTATION-MINIMIZING FRAME (parallel transport): the profile's orientation is carried forward from one path point to the next by the smallest rotation that follows the bend, so a pipe doesn't twist along its length. Reuses the engine's area-weighted `computeNormals` for smooth shading. Header-only, deterministic, headless.  Scope note (honest): this builds the swept SIDE surface only — the two ends are left OPEN (like a cut pipe); cap them separately if you need a closed solid. `closedProfile` (default true) controls whether the cross-section is a closed ring (a tube) or an open strip (a ribbon). The path is a polyline of >= 2 points; a profile needs >= 2 points. Consecutive duplicate path points (zero-length segments) are tolerated (they reuse the previous frame).

**Functions:**

- `inline shapes::MeshData sweepProfile(const std::vector<math::vec2>& profile,`
- `inline shapes::MeshData buildTube(const std::vector<math::vec3>& path, float radius, int sides)`

### `MeshSymmetry`
<sub>`engine/include/maz/render/MeshSymmetry.hpp`</sub>

maz::render MESH SYMMETRY-PLANE DETECTION — decide whether a mesh is MIRROR-SYMMETRIC and across which plane. Most game props and characters are built symmetric (a face, a car, a sword), and knowing the symmetry plane unlocks a lot: symmetric modelling/sculpt tools that mirror edits, half-mesh authoring then reflect (see the M547 mirror tool), UV/texture mirroring, "is this the left or right variant?" checks, and pivot/alignment fixes for importers that landed a model off-axis. This tests the three axis-aligned candidate planes through the mesh's centroid (normal along X, Y, or Z), reflects every vertex across each, and scores the plane by the fraction of vertices that land on an existing vertex within tolerance — then reports the best plane and each axis' score. A spatial hash makes the correspondence lookup near-linear. Header-only, pure CPU.  Scope note (honest): only the three AXIS-ALIGNED planes through the centroid are tested — a model symmetric about a tilted or off-centre plane reads as non-symmetric here (fit an OBB first via math::FitObb and test in its frame). Scoring is by VERTEX correspondence, so an asymmetric tessellation of a symmetric SHAPE can score below 1 even though the surface is symmetric; raise `tolerance` or resample if that bites. The default tolerance scales with the bounding box (1e-3 of its diagonal).

**Types:** `SymmetryPlane`, `SymmetryReport`

**Functions:**

- `inline SymmetryReport detectSymmetryPlanes(const shapes::MeshData& mesh, float tolerance = -1.0f,`

### `MeshTaper`
<sub>`engine/include/maz/render/MeshTaper.hpp`</sub>

maz::render TAPER — squeeze or fan a mesh along an axis: the cross-section perpendicular to the axis is scaled by a factor that ramps LINEARLY from `startScale` (at the low end of the axis) to `endScale` (at the high end), so a straight bar cones into a pyramid or spike, a cylinder becomes a carrot or a trumpet, a leg thins toward the ankle. This is Blender's "Simple Deform → Taper" and the quickest way to give straight geometry a swelling or narrowing profile without remodelling. The axis coordinate of each vertex is untouched; only its distance from the axis (through `centre`) is scaled by the ramped factor. axis 0=X, 1=Y, 2=Z. Reuses the same axis mapping as the twist deformer. Header-only, pure CPU, deterministic.  Scope note (honest): positions are exact; normals are updated with the inverse scale on their perpendicular components and renormalized, which is correct for the cross-section squeeze but ignores the extra tilt the taper SLOPE introduces along the axis — for pixel-accurate shading on a strong taper, re-run `computeNormals` afterwards. Leaving `axisMin`/`axisMax` at their defaults (min ≥ max) auto-fits the ramp to the mesh's extent along the axis. A factor of 0 collapses that end onto the axis line (a true cone tip / degenerate ring — weld or reindex if you need the tip to be a single vertex). Factors may exceed 1 (fan outward) or be negative.

**Functions:**

- `inline shapes::MeshData taperMesh(const shapes::MeshData& mesh, int axis, float startScale, float endScale,`

### `MeshThickness`
<sub>`engine/include/maz/render/MeshThickness.hpp`</sub>

maz::render MESH WALL-THICKNESS PROBE — measure how THICK the material is at every point of a surface by shooting a ray straight INTO the surface (opposite its outward normal) and returning the distance to the wall on the far side. This is the "wall thickness" / "shell gauge" check 3D-printing slicers and CAD tools run to catch spots too thin to print or too thin to be structurally sound; it also drives subsurface-scattering thickness maps (skin, wax, leaves glowing at their thin edges) and "is this hollow shell uniform?" audits. For a solid model the probe reports the full span across the object; for a hollow shell it reports the gap between the outer and inner walls. Reuses the M533 ambient-occlusion ray/triangle test; header-only, pure CPU.  Scope note (honest): this is a single inward ray per vertex along the smooth normal — it measures thickness in exactly that direction, so a wall sampled at a glancing angle reads thicker than its true minimum; average several offset rays if you need a robust minimum. A vertex whose inward ray escapes without hitting anything within `maxDistance` (an open edge, a one-sided sheet) is reported as `maxDistance` and flagged "open". Normals are area-weighted smooth normals recomputed internally, so winding must be outward-consistent for the ray to point into the material.

**Types:** `ThicknessReport`

**Functions:**

- `inline std::vector<float> computeThickness(const shapes::MeshData& mesh, float maxDistance = 1e6f)`
- `inline ThicknessReport analyzeThickness(const shapes::MeshData& mesh, float maxDistance = 1e6f)`

### `MeshTools`
<sub>`engine/include/maz/render/MeshTools.hpp`</sub>

Mesh post-processing — the "fill in the vertex attributes an importer or generator left blank" step, Godot's SurfaceTool.generate_normals() / generate_tangents(). A raw mesh is often just positions + indices (+ maybe UVs): a heightfield you built procedurally, a decimated collision hull, a glTF that shipped without a NORMAL/TANGENT stream. Lighting needs a per-vertex NORMAL, and normal mapping needs a per-vertex TANGENT frame; deriving them from the geometry is a standard, well-defined computation. Maz could build primitive shapes (which bake their own normals) and load glTF (which may carry them), but had no way to (re)generate these for arbitrary geometry. Pure vector math, header-only, deterministic — it unit-tests exactly (a flat mesh yields the plane normal; a shared ridge yields the averaged normal) and drives a golden.  computeNormals uses AREA-WEIGHTED face accumulation: each triangle adds its un-normalized cross product (whose magnitude is twice the triangle area) to its three vertices, so larger faces pull a shared vertex more — the same default SurfaceTool uses, and it gives smooth results on curved meshes while degenerate (zero-area) triangles contribute nothing. computeTangents uses Lengyel's method (the one Godot/most engines use): accumulate per-triangle tangent/bitangent from the UV gradient, then Gram-Schmidt-orthonormalize against the normal and store handedness in .w so the shader can rebuild the bitangent as cross(normal, tangent.xyz) * tangent.w.  Scope note (honest): these operate on a single indexed triangle stream with fully shared vertices (smoothing groups are "every face that shares a vertex index"). They do not split vertices along hard edges / UV seams, weld a soft threshold, or triangulate polygons — a full SurfaceTool with index/dedup/seam handling remains a follow-up.

**Functions:**

- `inline float length3(const math::vec3& v)`
- `inline math::vec3 safeNormalize3(const math::vec3& v)`
- `inline std::vector<math::vec3> computeNormals(const std::vector<math::vec3>& positions,`
- `inline std::vector<math::vec4> computeTangents(const std::vector<math::vec3>& positions,`

### `MeshTopology`
<sub>`engine/include/maz/render/MeshTopology.hpp`</sub>

maz::render MESH TOPOLOGY / connectivity — the reusable half-edge-style adjacency an engine builds once and then everything that needs to know how a mesh is STITCHED TOGETHER queries: boundary/hole detection, watertightness (is this a closed solid?), per-triangle neighbours for flood-fill smoothing groups / UV islands / crease detection, and the Euler characteristic as a sanity check. Godot exposes the same thing via MeshDataTool (get_edge_faces, get_face_edges). Existing modules built ad-hoc edge maps inline (MeshSmooth's vertex-vertex adjacency, Subdivision's EdgeInfo); this is the shared, tested primitive.  The mesh is a triangle list. Each triangle owns three DIRECTED half-edges: for triangle t, he 3t+0 = v0→v1, 3t+1 = v1→v2, 3t+2 = v2→v0. Two half-edges are TWINS when they share the same undirected edge {a,b}; `opposite[he]` is that twin (or kNone on a boundary / non-manifold edge). Twin matching is by UNDIRECTED edge, so it is robust to inconsistent winding (a mis-wound but connected mesh still reports its neighbours, not spurious boundaries). Pure CPU, header-only, headless.

**Types:** `MeshTopology`

**Functions:**

- `inline MeshTopology buildTopology(std::uint32_t vertexCount, const std::vector<std::uint32_t>& indices)`
- `inline MeshTopology buildTopology(const shapes::MeshData& mesh)`

### `MeshTopologySummary`
<sub>`engine/include/maz/render/MeshTopologySummary.hpp`</sub>

maz::render TOPOLOGY SUMMARY — one struct answering "what SHAPE, topologically, is this mesh?": how many separate pieces (connected components), how many holes ring it (boundary loops), whether it is a closed solid (watertight) and manifold, its Euler characteristic V−E+F, and its GENUS — the number of "handles"/through- holes (a sphere/box is genus 0, a donut/torus or a coffee mug is genus 1, a pretzel higher). This is the mesh-health / "is this printable, is this a valid solid, how complex is it" report a DCC tool or a 3D-print slicer shows, and the sanity check before physics/booleans/simplification that assume a clean manifold. It composes the earlier connectivity work: MeshTopology (M528) for edges/manifoldness, MeshComponents (M529) for the piece count, MeshBoundaryLoops (M538) for the hole count — then genus falls out of the Euler–Poincaré formula χ = 2c − 2g − b (c = components, g = genus, b = boundary loops). Header-only, std-only.  Scope note (honest): genus is exact only for a WELDED, ORIENTABLE, manifold mesh (a shared corner must be one vertex — run MeshCleanup/MeshWeld first). The vertex count used is the number of REFERENCED vertices (unused vertices in the buffer are ignored, so they do not corrupt Euler), but duplicated/un-welded corners still would — reported via `manifold`. When the mesh is non-manifold, `genus` is left at -1 (undefined).

**Types:** `TopologySummary`

**Functions:**

- `inline TopologySummary summarizeTopology(const shapes::MeshData& mesh)`

### `MeshTransform`
<sub>`engine/include/maz/render/MeshTransform.hpp`</sub>

maz::render APPLY / BAKE TRANSFORM — permanently apply a 4×4 transform (translate + rotate + scale) to a mesh's geometry, moving both its POSITIONS and its NORMALS correctly. This "freeze transform / apply transform" step is everywhere in a content pipeline: flatten a node's transform into its mesh before export, merge several placed copies into one buffer, pre-bake an import fix-up (a rotate to swap Y-up/Z-up, a scale to convert units) so the runtime does no per-frame matrix work, or snapshot an instance. The subtlety it gets right: normals do NOT transform by the same matrix as positions under NON-UNIFORM scale — they use the INVERSE-TRANSPOSE of the 3×3 part, then renormalize, so a squashed surface keeps its normals perpendicular (naive transforms leave them skewed and lighting goes wrong). Ships with translate/scale/rotate matrix builders so callers needn't touch glm. Header-only, pure CPU.  Scope note (honest): this BAKES the transform into vertex data — it does not keep a separate transform (that's a scene-graph node's job). Tangents (if present) are not recomputed here (regenerate via computeTangents after a mirroring/negative-scale transform, which also flips winding — pair with M562 if the determinant is negative). Positions use the full 4×4 (translation included); normals use only the inverse-transpose 3×3 (translation-free) and are renormalized, so a zero/degenerate normal stays zero.

**Functions:**

- `inline math::mat4 translationMatrix(const math::vec3& t)`
- `inline math::mat4 scaleMatrix(const math::vec3& s)`
- `inline math::mat4 rotationMatrix(const math::vec3& axis, float radians)`
- `inline shapes::MeshData applyTransform(const shapes::MeshData& mesh, const math::mat4& m)`

### `MeshTwist`
<sub>`engine/include/maz/render/MeshTwist.hpp`</sub>

maz::render TWIST — spiral a mesh around an axis: the further a vertex sits along the axis, the more it is rotated about it, so a straight bar becomes a corkscrew, a blade gains a spiral flute, a tower gets a helical sweep. This is Blender's "Simple Deform → Twist" and the classic way to add a wound, spiralled, or barley-sugar look to otherwise straight geometry without hand-modelling every ring. The rotation angle at a vertex is `radiansPerUnit × (its distance along the axis from `centre`)`, applied in the plane perpendicular to the axis, about the axis line through `centre`. Both the position AND the normal's perpendicular components are rotated, so lighting follows the twist. axis: 0 = X, 1 = Y, 2 = Z. Header-only, pure CPU, deterministic.  Scope note (honest): this is a rigid rotation per cross-section — it preserves each vertex's height along the axis and its distance from the axis exactly (no stretching), so a cylinder stays the same radius as it winds. The visible smoothness of the spiral is limited by how many rings the mesh has along the axis: a bar with only two rings (top and bottom) just shears into a parallelogram twist; subdivide along the axis first for a smooth helix. `radiansPerUnit` may be negative to wind the other way.

**Functions:**

- `inline shapes::MeshData twistMesh(const shapes::MeshData& mesh, int axis, float radiansPerUnit,`

### `MeshUvProject`
<sub>`engine/include/maz/render/MeshUvProject.hpp`</sub>

maz::render PROJECTION UV UNWRAP — auto-generate texture coordinates without a hand-made unwrap, by PROJECTING world positions onto a plane. Two flavours: PLANAR projection drops every vertex straight down one axis (the top-down "decal"/terrain map — paint a whole landscape or floor with one texture), and BOX projection picks, per triangle, the axis its face points most toward and projects onto that plane (the "cube"/triplanar unwrap Blender and Maya offer as an instant UV for hard-surface props — each face gets sensible, low-distortion coordinates with no manual seam work). This is the quick UV a mesh needs before it can show a tiled material, a decal, or a checker map for inspection. Planar keeps the mesh topology (UVs written in place); box must give each triangle its own corners (a face's projection axis differs from its neighbour's), so it returns a facet-split mesh like MeshFacet. Reuses only vec3 from maz::math. Header-only, deterministic.  Scope note (honest): projection UVs stretch on surfaces steep to the projection axis (planar) or seam at the 45° between box axes — that is inherent to projection unwraps, not a bug; for low distortion on organic shapes an angle-based/LSCM unwrap is the heavier follow-up. Box projection maps opposite faces the same way (no back-face flip), the simple convention.

**Functions:**

- `inline void uvChannels(int axis, int& c0, int& c1)`
- `inline shapes::MeshData planarUv(const shapes::MeshData& mesh, int axis, float scale,`
- `inline shapes::MeshData boxUv(const shapes::MeshData& mesh, float scale,`

### `MeshUvRadial`
<sub>`engine/include/maz/render/MeshUvRadial.hpp`</sub>

maz::render SPHERICAL & CYLINDRICAL UV PROJECTION — the wraparound auto-unwraps for round objects, completing the projection-UV family alongside planar/box (M551). SPHERICAL maps each vertex by its direction from a centre to longitude (u, the angle around) and latitude (v, top-to-bottom) — the equirectangular / lat-long layout a planet, an eyeball, a ball, or a skydome wants (world maps are stored exactly this way). CYLINDRICAL maps the angle around an axis to u and the height along it to v — what a bottle label, a tree trunk, a pipe, or a tin can wants. Both are the one-click UVs a round mesh needs before it can wear a texture, with no manual seam work. UVs are written onto the existing vertices (topology unchanged). Reuses only vec3 from maz::math. Header-only, deterministic.  Scope note (honest): both have the inherent projection artefacts — a single wrap SEAM where u jumps from ~1 back to 0 (the mesh should be split there, or the texture set to wrap), spherical PINCHING at the two poles (all longitudes converge), and cylindrical stretch on caps that face along the axis. These are properties of the projection, not bugs; an LSCM/angle-based unwrap is the distortion-free follow-up.

**Functions:**

- `inline shapes::MeshData sphericalUv(const shapes::MeshData& mesh, const math::vec3& center = math::vec3(0, 0, 0),`
- `inline shapes::MeshData cylindricalUv(const shapes::MeshData& mesh, int axis = 1,`

### `MeshUvSeams`
<sub>`engine/include/maz/render/MeshUvSeams.hpp`</sub>

maz::render UV-SEAM EDGE DETECTION — find the edges of a mesh where the texture coordinates are DISCONTINUOUS: the two triangles that meet along a 3D edge disagree on the UV of the shared corners, so the texture is cut there. Those cuts are the seams of a UV unwrap — the boundaries of the flat "islands" a model is unfolded into (a cube unwrapped as a cross has a seam along most of its rim). Knowing them drives: lightmap / texture-atlas SEAM DILATION (bleed colour a few texels past a seam so bilinear filtering doesn't sample the gap), seam-hiding and seam-aware smoothing, and the "select seams" convenience in a UV editor. Detection welds vertices by POSITION so the same physical edge from two UV islands is recognised as one edge, then compares the UVs the two faces assign at each endpoint. Reuses nothing beyond std. Header-only, deterministic.  Scope note (honest): compares the mesh's stored per-vertex UVs, so a mesh that already welds UV-identical corners simply reports no seam there (correct). Position welding is grid-quantised by `posEps`; a boundary edge (one triangle) is an island rim, reported separately from interior UV-discontinuity seams. Non-manifold position-edges (3+ triangles) are counted but not seam-classified.

**Types:** `UvSeamResult`

**Functions:**

- `inline UvSeamResult detectUvSeams(const shapes::MeshData& mesh, float posEps = 1e-5f, float uvEps = 1e-6f)`

### `MeshValence`
<sub>`engine/include/maz/render/MeshValence.hpp`</sub>

maz::render VERTEX VALENCE / IRREGULAR-VERTEX REPORT — count how many edges meet at each vertex (its VALENCE) and flag the IRREGULAR ones. In a clean triangle mesh almost every interior vertex has valence 6 (six triangles fanning around it); vertices that don't — the 5s and 7s, called poles or singularities — are where edge flow pinches or splays. Retopology and subdivision tools work hard to MINIMISE them because irregular vertices cause shading artefacts, uneven subdivision, and awkward UV/animation deformation. This report gives the per-vertex valence, marks boundary vertices (open edges, which are naturally lower-valence and judged separately), and tallies how many interior vertices are regular (6) vs irregular — a one-number read on mesh quality that a modelling tool shows as a "show poles" overlay or a retopo score. Header-only, pure CPU; no topology build needed — it counts distinct edge-neighbours and single-face (boundary) edges directly.  Scope note (honest): "regular = 6" is the triangle-mesh convention; a quad mesh's ideal is valence 4, so read `irregularInterior` accordingly for quad-derived data. Valence counts DISTINCT connected neighbours, so a non-manifold or duplicated-vertex mesh can report surprising values — weld first (see M525/M559). Boundary vertices are reported and counted but never labelled irregular, since their low valence is expected.

**Types:** `ValenceReport`

**Functions:**

- `inline ValenceReport analyzeValence(const shapes::MeshData& mesh, std::uint32_t regularValence = 6)`

### `MeshVertexColorAo`
<sub>`engine/include/maz/render/MeshVertexColorAo.hpp`</sub>

maz::render VERTEX-COLOR AMBIENT-OCCLUSION BAKE — darken each vertex's stored RGB by how OCCLUDED it is, so the mesh carries its own soft contact shadows with no texture, no lightmap, and no runtime lighting. AO is the free ambient shadowing of nooks and crevices — under a ledge, inside a fold, where two walls meet — and baking it straight into vertex colours is the cheapest way to give flat-lit or mobile/retro content that grounded, hand-painted look (it is exactly what "bake AO to vertex colours" does in Blender, and what a lot of low-poly games ship). Reuses the M533 hemisphere raycaster (`bakeVertexAO`, 0 = open, 1 = fully occluded) and multiplies each channel by (1 − ao·strength): open surfaces keep their colour, creases go dark. Returns a copy; the source is untouched. Header-only, deterministic (golden-angle sampling), headless.  Scope note (honest): this MULTIPLIES existing vertex colours, so a white mesh becomes a pure AO map and a tinted mesh keeps its tint times the shadow — call it once (baking twice double-darkens). AO smoothness is limited by mesh tessellation (it is a per-VERTEX value); for crisp AO on low-poly meshes bake to a texture instead. `strength` in [0,1] scales the darkening; `rayCount`/`maxDistance` are the M533 knobs.

**Functions:**

- `inline shapes::MeshData bakeAoToVertexColor(const shapes::MeshData& mesh, float strength = 1.0f,`

### `MeshVertexColorCavity`
<sub>`engine/include/maz/render/MeshVertexColorCavity.hpp`</sub>

maz::render VERTEX-COLOR CAVITY (curvature) BAKE — write a mesh's own shape into its vertex colours so that CREVICES, GROOVES, and CONCAVE folds go DARK while RIDGES, EDGES, and CONVEX bulges go LIGHT, with no texture and no ray-tracing. This is the "cavity map" sculpting tools (ZBrush, Blender, Substance) overlay to make surface detail — panel-line grime, worn edges, carved seams — read at a glance; here it is baked straight into per-vertex RGB so any flat-lit or unlit renderer shows the form for free. It complements the M553 ambient- occlusion bake (which measures how BOXED-IN a point is by shooting rays) — cavity is purely LOCAL curvature (how the immediate neighbourhood folds), so it is far cheaper and sharpens fine creases AO misses.  The signal is a signed concavity: for each vertex we take the vector from the vertex to the CENTROID of its edge-neighbours and project it onto the (smooth) surface normal, scaled by local edge length so it is resolution- and size-independent. Neighbours sitting FURTHER OUT along the normal than the vertex mean the vertex is RECESSED → concave → positive; neighbours pulled INWARD mean the vertex JUTS OUT → convex → negative; a flat neighbourhood gives ~0. Normals are recomputed internally (area-weighted), so the input needn't carry them. Header-only, pure CPU.  Scope note (honest): this is a first-ring DISCRETE curvature estimate — a fast local proxy, not the exact mean-curvature of MeshCurvature (M539); it reads folds within one edge of a vertex, so detail finer than the tessellation is invisible. Values are unbounded in principle (a razor crease gives a large magnitude); the colour bake clamps them. Boundary vertices (open edges) see a lopsided neighbourhood and read less reliably.

**Functions:**

- `inline std::vector<float> cavitySignal(const shapes::MeshData& mesh)`
- `inline std::vector<float> computeCavity(const shapes::MeshData& mesh)`
- `inline shapes::MeshData bakeCavityToVertexColor(const shapes::MeshData& mesh, float strength = 1.0f,`

### `MeshVertexColorGradient`
<sub>`engine/include/maz/render/MeshVertexColorGradient.hpp`</sub>

maz::render VERTEX-COLOUR GRADIENT PAINT — tint a mesh's per-vertex RGB by WHERE each vertex sits, in one call. Two of the most common "give it a look without a texture" moves an artist makes: • an AXIS gradient — colour fading from one shade to another along X, Y or Z. Green grass at a hill's base fading to brown rock at its peak; a wall darker at the floor and lighter at the ceiling; a blade glowing hotter toward its tip. `t` runs 0→1 as the vertex moves from `axisMin` to `axisMax` along the chosen axis and the colour is a straight blend from `low` to `high`. • a RADIAL gradient — colour fading outward from a point. A glow or scorch mark around a hit, a spotlight pool on a floor, a target ring. `t` runs 0→1 as the vertex's distance from `centre` grows from `inner` to `outer`, blending `innerColor` to `outerColor`. Both write straight into the RGB the mesh already stores (alpha is ignored) and leave positions, normals and UVs untouched — so the result composes with the other vertex-colour tools (smooth it with M564, add M556 cavity, bake M553 AO). Header-only, pure CPU.  Scope note (honest): this is a hard REPLACE of each vertex's RGB, not a blend over the existing colour — call it first, then layer AO/cavity/smoothing on top. The blend is linear in the stored [0,1] RGB (no gamma correction and no easing curve); pre-smooth the mesh or run M564 afterwards if you want a softer ramp. For the axis gradient, leaving `axisMin`/`axisMax` at their defaults (min ≥ max) auto-fits the range to the mesh's bounding box along that axis, so the gradient always spans the whole model.

**Functions:**

- `inline Color lerpColor(const Color& a, const Color& b, float t)`
- `inline shapes::MeshData paintAxisGradient(const shapes::MeshData& mesh, int axis, const Color& low,`
- `inline shapes::MeshData paintRadialGradient(const shapes::MeshData& mesh, float cx, float cy, float cz,`

### `MeshVertexColorSmooth`
<sub>`engine/include/maz/render/MeshVertexColorSmooth.hpp`</sub>

maz::render VERTEX-COLOUR SMOOTHING — blur a mesh's per-vertex RGB across its edges without moving a single vertex. Baked vertex colours — ambient occlusion (M553), cavity/curvature (M556), hand-painted masks — often come out noisy or blocky: a low ray count leaves AO speckled, a coarse mesh makes cavity shading stair-step, and a paint stroke lands hard-edged. This relaxes each vertex's colour toward the average of its edge-neighbours (a Laplacian blur on the colour signal, exactly like M540 mesh smoothing but on COLOUR, not POSITION), so the shading reads soft and clean while the geometry stays bit-for-bit identical. `strength` (0..1) sets the blur per pass and `iterations` how many passes; boundary vertices can be pinned so open edges keep their colour. Reuses the M540 adjacency builder. Header-only, pure CPU.  Scope note (honest): this smooths ONLY the RGB channels; positions, normals, and UVs are untouched. It is an unweighted (umbrella) Laplacian — neighbour count, not edge length or angle, sets the weight — which is fast and stable but slightly blurs across sharp colour boundaries; drop `strength`/`iterations` or pin boundaries to preserve edges. Colours are read and written in the [0,1] range the vertices already store; nothing is clamped beyond staying within the neighbours' own range, so a valid input stays valid.

**Functions:**

- `inline shapes::MeshData smoothVertexColors(const shapes::MeshData& mesh, float strength = 0.5f,`

### `MeshVoxelize`
<sub>`engine/include/maz/render/MeshVoxelize.hpp`</sub>

maz::render SOLID VOXELIZATION — convert a closed triangle mesh into a boolean 3D occupancy grid: each cell is 1 if its centre lies INSIDE the solid, 0 if outside. This is the bridge from surface geometry to the volumetric representations games lean on: destructible/editable voxel terrain (Minecraft/Teardown-style), building nav volumes for 3D pathfinding, GPU-particle/fluid collision masks, fast approximate inside tests, procedural interior filling, and the input to a marching-cubes/SurfaceNets re-mesh. Unlike a SURFACE voxelization (which only marks cells the triangles pass through), this is a SOLID fill: the inside/outside decision at each cell centre is a ray-parity test (cast one oblique ray, an odd triangle-crossing count means inside), reusing the M533 Möller–Trumbore ray/triangle — the same sign test MeshSdf (M534) uses at its grid corners. Pure CPU, header-only, headless — verifiable because a filled solid's voxel count times the cell volume must converge to the mesh's true volume.  Scope note (honest): brute-force O(cells · tris) with a ray-parity sign, so it assumes a watertight mesh and is intended for offline bakes; a BVH (game::Bvh) to prune the ray test and a generalized-winding-number sign for open meshes are the documented follow-ups (shared with MeshSdf).

**Types:** `VoxelGrid`

**Functions:**

- `inline VoxelGrid voxelizeSolid(const shapes::MeshData& mesh, int resolution, float padding = 0.0f)`

### `MeshWatertight`
<sub>`engine/include/maz/render/MeshWatertight.hpp`</sub>

maz::render WATERTIGHTNESS / HOLE REPORT — is this mesh SEALED, or does it have gaps? A watertight (closed) surface has no open edges and no edge shared by three-plus faces; it's what 3D printing, boolean/CSG, solid physics, volume/mass, and inside/outside tests all require. This walks the mesh's edges to answer "is it closed?" and, when it isn't, finds every HOLE — each open boundary loop — reporting how many there are and, per hole, its rim as an ordered vertex loop with an edge count and perimeter length (so you can rank the big gaps worth patching from the pinholes). It also surfaces NON-MANIFOLD edges (three-plus faces meeting), the other way a mesh fails to be a clean solid. Reuses the M528 half-edge topology and the M536 boundary-loop extractor. Header-only, pure CPU — the analysis behind an editor's "mesh is not watertight: N holes" warning.  Scope note (honest): "watertight" here is EDGE-manifold closure (no boundary, no 3+-face edges) — the standard printability/solidity test; it does not separately verify consistent winding (use M562) or self-intersection (a distinct, costlier check). Holes are open boundary loops, so a mesh split into separate closed shells reads as watertight with zero holes even though it is several pieces (pair with M529 components if that matters).

**Types:** `MeshHole`, `WatertightReport`

**Functions:**

- `inline WatertightReport analyzeWatertight(const shapes::MeshData& mesh)`

### `MeshWeld`
<sub>`engine/include/maz/render/MeshWeld.hpp`</sub>

maz::render vertex welding — merge coincident (or near-coincident) vertices of an indexed triangle mesh into one, remapping the indices and dropping triangles that collapse to a line. It is the cleanup pass a mesh needs after being built face-by-face (each quad or triangle emitting its own corners), CSG or marching-cubes output, or an import that duplicated shared vertices along every seam: welding turns that soft soup into a compact shared-vertex mesh so smoothing groups, subdivision, and normal generation behave (a shared corner must be ONE vertex for its faces to average). This is Godot's SurfaceTool.index() with a distance threshold. A spatial hash keyed by the weld cell finds candidates in O(n) expected; the first occurrence of each cluster is kept as the representative, so surviving positions are unchanged. Godot's SurfaceTool.index welds only bit-exact duplicates; the distance threshold here is the extra step -> parity-or-better. Header-only, std-only, deterministic.

**Types:** `WeldedMesh`

**Functions:**

- `inline std::uint64_t cellHash(long cx, long cy, long cz)`
- `inline WeldedMesh weldVertices(const std::vector<math::vec3>& positions,`

### `MeshWeldAuto`
<sub>`engine/include/maz/render/MeshWeldAuto.hpp`</sub>

maz::render WELD-TOLERANCE AUTO-DETECT — look at the spacing of a mesh's vertices and SUGGEST a good weld distance, so you don't have to guess the epsilon that M525 weldVertices needs. Importers routinely duplicate the vertices along every UV seam or smoothing split — sometimes at the exact same spot, sometimes a hair apart — and welding them back together is what makes a mesh watertight for physics, simplification, and normal smoothing. But pick the epsilon too small and the seams stay split; too large and you collapse genuine detail. This measures every vertex's nearest neighbour, finds the natural GAP between the tight cluster of duplicate/seam pairs and the much larger spacing of real geometry, and returns an epsilon that sits safely in that gap — plus a small report so you can see why. Header-only, pure CPU.  Scope note (honest): nearest-neighbour distances are computed pairwise (O(n²)) — intended for import-time analysis of moderate meshes (up to a few thousand vertices); for very large meshes bucket or decimate first. The suggestion is a heuristic: when duplicates and real geometry are clearly separated it is reliable (`bimodal` = true); when they are not (a uniformly sampled surface with no duplicates), it returns a tiny conservative epsilon that welds nothing, and you should weld only if you know duplicates exist.

**Types:** `WeldSuggestion`

**Functions:**

- `inline WeldSuggestion suggestWeldTolerance(const shapes::MeshData& mesh)`
- `inline WeldedMesh autoWeld(const shapes::MeshData& mesh)`

### `MeshWinding`
<sub>`engine/include/maz/render/MeshWinding.hpp`</sub>

maz::render TRIANGLE WINDING / NORMAL-CONSISTENCY DETECTOR — find the triangles whose winding (vertex order, which decides which way the face points) DISAGREES with their neighbours, so the mesh can be made uniformly outward-facing. Flipped faces are one of the most common import defects: they turn black under lighting, punch holes in shadows, and break backface culling and solid-mesh tests. In a consistently wound surface every shared edge is traversed in OPPOSITE directions by its two triangles; a flipped triangle traverses its shared edges the SAME way as its neighbours. This walks each connected surface from a seed, propagates a consistent orientation across shared edges (M528 half-edge topology), and reports the MINORITY set per component — the triangles you'd flip to make it uniform — plus the raw count of inconsistently-wound edges. It's the analysis behind a "recalculate / make normals consistent" command. Header-only, pure CPU.  Scope note (honest): this reports which triangles disagree WITH EACH OTHER and the smaller set to flip per connected component; it does NOT decide which way is "out" (that needs a containment/volume test — pair with M546 containsPoint or a signed-volume check to orient outward). Non-orientable surfaces (a Möbius strip) have no consistent assignment — the walk still returns a best-effort labelling and the inconsistent-edge count stays non-zero. Boundary and non-manifold edges are simply not propagated across.

**Types:** `WindingReport`

**Functions:**

- `inline WindingReport analyzeWinding(const shapes::MeshData& mesh)`
- `inline shapes::MeshData makeWindingConsistent(const shapes::MeshData& mesh)`

### `MeshWireframe`
<sub>`engine/include/maz/render/MeshWireframe.hpp`</sub>

maz::render WIREFRAME / EDGE EXTRACTION — pull the UNIQUE edges out of a triangle mesh so you can draw it as a cage of lines. Every triangle shares its edges with its neighbours, so the raw triangle list mentions each interior edge twice; this collapses them to one edge each. Feed the result to a line renderer (drawLine) for a wireframe overlay, an editor "show edges" mode, a hologram / blueprint look, a selection highlight outline, or a debug view of how a mesh is built. `meshEdges` returns the edges as vertex-index PAIRS; `meshWireframe` returns them ready to draw as a flat LINE LIST — two positions per edge, so you can hand the whole vector straight to a line-segment draw call. Header-only, deterministic, headless.  Scope note (honest): this returns EVERY triangle edge (deduplicated) — including the diagonal that splits each quad face into two triangles, so a triangulated cube yields 18 edges (12 box edges + 6 face diagonals), not the 12 "clean" edges an artist sees. For only the visually meaningful creases, use the sharp / hard / feature-edge tools (M548/M549/M567) which filter by dihedral angle. Edges are undirected and de-duplicated; a degenerate index in a triangle is skipped.

**Functions:**

- `inline std::vector<std::pair<std::uint32_t, std::uint32_t>> meshEdges(const shapes::MeshData& mesh)`
- `inline std::vector<math::vec3> meshWireframe(const shapes::MeshData& mesh)`

### `Model`
<sub>`engine/include/maz/render/Model.hpp`</sub>

A glTF model loaded into CPU data: merged geometry plus its base-color texture (if any).

**Types:** `ModelData`, `SceneNode`, `SceneData`

### `MtlLoader`
<sub>`engine/include/maz/render/MtlLoader.hpp`</sub>

maz::render Wavefront MTL (.mtl) material-library parser — the companion to the OBJ mesh loader. An OBJ file references materials by name (`mtllib foo.mtl` + `usemtl name`); the actual colors, shininess, transparency, and texture-map paths live in the sibling `.mtl` file. Godot's OBJ importer reads it to build real materials; Maz's OBJ loader parsed only geometry and skipped `mtllib`/`usemtl`, so imported models came in untextured and flat-colored. `parseMtl` reads the text library into a list of `MtlMaterial` records (ambient/diffuse/specular colors, specular exponent, optical density, dissolve, illumination model, and the diffuse/ambient/specular/bump texture-map filenames). Pure text parsing — no GPU — so it unit-tests headlessly from an in-memory string; `loadMtl` wraps it for files, and `findMaterial` resolves a `usemtl` name against the parsed list.  Scope note (honest): the common, widely-emitted subset — Ka/Kd/Ks/Ns/Ni/d/Tr/illum and map_Ka/map_Kd/map_Ks/map_Bump(bump). A single grayscale value is accepted where a color is expected. Texture lines take the final whitespace-delimited token as the filename, so leading option flags (`-o`, `-s`, `-bm`, …) are skipped rather than interpreted. Spectral/xyz color spaces and PBR extension tags (Pr/Pm/Pc/…) are not decoded. A library with no `newmtl` yields an empty list.

**Types:** `MtlMaterial`

**Functions:**

- `inline void mtlReadColor(std::istringstream& ss, float out[3])`
- `inline std::string mtlMapPath(std::istringstream& ss)`
- `inline bool parseMtl(const std::string& text, std::vector<MtlMaterial>& out)`
- `inline const MtlMaterial* findMaterial(const std::vector<MtlMaterial>& lib, const std::string& name)`
- `inline bool loadMtl(const std::string& path, std::vector<MtlMaterial>& out)`

### `MultiMesh2D`
<sub>`engine/include/maz/render/MultiMesh2D.hpp`</sub>

2D multi-mesh instancing — Godot's MultiMesh / MultiMeshInstance2D. Drawing a thousand grass blades, stars, or bullets as a thousand separate polygons means a thousand transform setups; a MultiMesh stores ONE base shape once plus a compact per-instance buffer (position, rotation, scale, colour) and stamps the shape at every instance. This is the 2D data structure for that: a convex base polygon in local space plus a list of `Instance2D`s. `transformedPolygon(i)` returns one instance's world-space polygon (for a per-instance coloured draw), and `bakeTriangles()` flattens EVERY instance into one triangle-fan soup ready to hand to a single batched draw / vertex upload. Header-only, math-only (no GPU state), so the transform math unit-tests headlessly; the app draws the baked instances.

**Types:** `Instance2D`, `MultiMesh2D`

**Functions:**

- `inline math::vec2 transformInstance(const Instance2D& inst, const math::vec2& local)`

### `ObjLoader`
<sub>`engine/include/maz/render/ObjLoader.hpp`</sub>

maz::render Wavefront OBJ loader — a second model importer alongside the glTF path (Maz had only glTF). OBJ is the lingua franca hand-off format from almost every DCC tool and asset pack, so supporting it widens what artists can bring in. parseObj reads the text form (positions `v`, texcoords `vt`, normals `vn`, faces `f`) into a MeshData ready for Renderer::createMesh: it resolves OBJ's separate 1-based (and negative/relative) index pools, deduplicates each unique position/uv/normal combination into one vertex, and fan-triangulates n-gon faces. Pure text parsing (no GPU), so it unit-tests headlessly from an in-memory string; loadObj wraps it for files.

**Types:** `ObjLoadOptions`

**Functions:**

- `inline bool parseObj(const std::string& text, shapes::MeshData& out, const ObjLoadOptions& opts =`
- `inline bool loadObj(const char* path, shapes::MeshData& out, const ObjLoadOptions& opts =`

### `Occlusion`
<sub>`engine/include/maz/render/Occlusion.hpp`</sub>

maz::render software occlusion culling — a conservative low-resolution occlusion buffer, the CPU technique behind Godot's OccluderInstance3D culling. Big nearby occluders (walls, terrain) are rasterized into a coarse depth grid; each writes the depth of its FARTHEST face, so the buffer records, per cell, a depth beyond which geometry is CERTAINLY hidden. A candidate object is culled only if every cell it covers is solid AND the object's nearest point lies at/behind that solid depth — so the test never hides something visible (no false occlusion), it only ever culls what is provably blocked. projectAabb() maps a world AABB through a view-projection matrix to the screen rect + depth range this buffer consumes. Pure math (no GPU); the coarse buffer keeps it cheap and unit-testable.

**Types:** `ScreenRect`, `OcclusionBuffer`

**Functions:**

- `inline ScreenRect projectAabb(const math::vec3& mn, const math::vec3& mx, const math::mat4& viewProj,`

### `OtsuThreshold`
<sub>`engine/include/maz/render/OtsuThreshold.hpp`</sub>

maz::render Otsu automatic thresholding — pick the best black/white cutoff for a grayscale image.  Turning a grayscale image into a clean two-tone (foreground vs background) mask needs a threshold, and choosing it by hand is fragile across lighting. Otsu's method (1979) finds the threshold AUTOMATICALLY: it treats the pixel histogram as two classes split at level t and picks the t that maximises the BETWEEN-CLASS variance — i.e. separates the two groups as cleanly as possible (equivalently, minimises the spread within each group). This is the standard block behind converting a coverage/height/mask texture to 1-bit, isolating a sprite silhouette, blob/marker detection, and "sunk into a valley of the histogram" segmentation. Pure CPU on the 256-bin histogram, header-only, deterministic — unit-tested on bimodal images where the correct cutoff is obvious.

**Functions:**

- `inline int otsuThresholdHist(const std::uint32_t hist[256])`
- `inline int otsuThreshold(const std::uint8_t* gray, int n)`
- `inline int otsuThreshold(const std::vector<std::uint8_t>& gray)`
- `inline std::vector<std::uint8_t> binarize(const std::uint8_t* gray, int n, int threshold)`
- `inline std::vector<std::uint8_t> binarize(const std::vector<std::uint8_t>& gray, int threshold)`

### `OverdrawOptimize`
<sub>`engine/include/maz/render/OverdrawOptimize.hpp`</sub>

maz::render OVERDRAW optimization — the load-time triangle reorder that pairs with vertex-cache optimization (VertexCacheOptimize.hpp) to make opaque geometry cheaper to shade. Vertex-cache order cuts redundant VERTEX-shader work; overdraw order cuts redundant FRAGMENT-shader work: with early-Z depth testing, a fragment behind an already-drawn one is rejected before shading, so drawing triangles roughly FRONT-TO-BACK means each pixel is shaded close to once instead of once per overlapping layer. This is the meshopt `optimizeOverdraw` / Godot importer idea in its verifiable core: `optimizeOverdraw` returns the mesh with its triangles reordered nearest-first along a view direction (a pure index permutation — positions and the triangle SET are untouched), and `simulateOverdraw` software-rasterizes the mesh under an orthographic view and counts depth-test-PASSING fragment writes, so a unit test can prove the reorder lowers overdraw. Pure CPU (no GPU), headless.  Scope note (honest): a static front-to-back order for a given view direction (the standard early-Z win for a dominant camera — foliage cards, UI, top-down scenes). meshopt's view-independent cluster reordering that also preserves the vertex-cache ACMR within a threshold is the heavier follow-up; this is the direct, measurable core. Run it AFTER optimizeVertexCache when you want both wins for a known dominant view.

**Types:** `OverdrawStats`

**Functions:**

- `inline void overdrawBasis(const math::vec3& viewDir, math::vec3& right, math::vec3& up, math::vec3& forward)`
- `inline OverdrawStats simulateOverdraw(const shapes::MeshData& mesh, const math::vec3& viewDir, int resolution)`
- `inline shapes::MeshData optimizeOverdraw(const shapes::MeshData& mesh, const math::vec3& viewDir)`

### `PlyLoader`
<sub>`engine/include/maz/render/PlyLoader.hpp`</sub>

maz::render PLY (Stanford .ply) mesh importer — closes another import gap versus Godot's asset pipeline. PLY is the standard output of 3D scanners and tools like MeshLab/CloudCompare, storing a vertex list (position, optional normal / color / texcoord) and a face list. `parsePly` reads both the ASCII and binary (little- and big-endian) encodings into a `shapes::MeshData` ready for `Renderer::createMesh`, mapping the usual property names (x/y/z, nx/ny/nz, red/green/blue, s/t or u/v) and fan-triangulating faces with more than three vertices. It is pure CPU byte/string work — no GPU — so it unit-tests headlessly from an in-memory buffer; `loadPly` wraps it for files.  Scope note (honest): reads the common `vertex` + `face` elements with scalar vertex properties and a single face index list. It does not interpret custom elements, edge lists, or material blocks; unknown vertex properties are skipped by type. Malformed input returns false rather than throwing.

**Types:** `PlyLoadOptions`, `PlyReader`, `PlyProp`

**Functions:**

- `inline PlyType plyType(const std::string& s)`
- `inline int plySize(PlyType t)`
- `inline bool plyIsFloat(PlyType t)`
- `inline PlySem plySem(const std::string& n)`
- `inline bool parsePly(const std::string& bytes, shapes::MeshData& out, const PlyLoadOptions& opts =`
- `inline bool loadPly(const std::string& path, shapes::MeshData& out, const PlyLoadOptions& opts =`

### `PolyTriangulate`
<sub>`engine/include/maz/render/PolyTriangulate.hpp`</sub>

Ear-clipping triangulation of a SIMPLE polygon — the geometry behind Godot's Polygon2D fill.  Maz can already FILL a *convex* polygon (drawConvexPolygon triangulates it as a fan from vertex 0), but a fan only reads correctly when every interior angle is < 180 deg. Feed it a CONCAVE outline (a star, an arrow, an L / C / comb shape) and the fan spills triangles outside the shape. Godot's Polygon2D handles arbitrary simple polygons by triangulating them properly; this closes that gap.  triangulatePolygon() runs the classic O(n^2) ear-clipping algorithm: repeatedly find a "convex ear" (a vertex whose triangle with its two neighbours points outward and contains no other vertex) and snip it off, until a single triangle remains. It works on any simple polygon (no self-intersections, no holes) of EITHER winding — the winding is detected via signed area and normalised to CCW so the convexity test is consistent. The result is a flat list of vertex INDICES into `poly`, three per triangle, each triangle wound the same way as the (normalised CCW) input. Every triangle is convex, so it can be drawn by the existing convex-fill path. Pure geometry: no renderer dependency, no allocation beyond the output, deterministic and headlessly unit-testable.

**Functions:**

- `inline float triSignedArea2(const math::vec2& a, const math::vec2& b, const math::vec2& c)`
- `inline float polygonSignedArea2(const std::vector<math::vec2>& poly)`
- `inline float polygonArea(const std::vector<math::vec2>& poly)`
- `inline bool pointInTriangle(const math::vec2& p, const math::vec2& a, const math::vec2& b,`
- `inline std::vector<std::uint32_t> triangulatePolygon(const std::vector<math::vec2>& poly)`

### `PresentMode`
<sub>`engine/include/maz/render/PresentMode.hpp`</sub>

maz::render present-mode selection — how the swapchain decides between the display sync modes the GPU/surface actually supports. This is the "vsync toggle / present-mode selection" policy, kept as a PURE function over an engine-side enum (mirroring Vulkan's VK_PRESENT_MODE_*) so it is unit-tested without a GPU; the Vulkan swapchain maps its real modes onto this and back.  Fifo        — hard vsync: queue images, present on vblank. No tearing, always supported. The safe default and the only mode guaranteed present. FifoRelaxed — adaptive vsync: like Fifo, but if the app missed a vblank the next image tears instead of stalling a whole frame — smoother under load than hard vsync. Mailbox     — triple-buffered: newest image replaces the queued one; no tearing AND low latency (the good "vsync off" for most desktops). Preferred when vsync is off. Immediate   — no sync at all: lowest latency, but tears. Last resort when Mailbox is absent.

**Functions:**

- `inline bool supportsMode(const std::vector<PresentMode>& supported, PresentMode m)`
- `inline PresentMode choosePresentMode(const std::vector<PresentMode>& supported, bool vsync,`

### `Radiosity`
<sub>`engine/include/maz/render/Radiosity.hpp`</sub>

maz::render multi-bounce global illumination (progressive radiosity) — turns the SINGLE-bounce `gatherIrradiance` (M511) into the FULL bounced-light solution Godot's LightmapGI bakes. One bounce makes a red wall tint the floor beside it; but that tinted floor should then tint the wall back, and the ceiling, and so on — light keeps bouncing, each bounce dimmer than the last, until it settles. This iterates exactly that: every surface patch is BOTH an emitter and a receiver, and each pass re-gathers the incoming light at each patch from the current radiance of all the others, then sets the patch's new radiance to its own emission plus its albedo times what it received. Because every bounce loses energy (albedo < 1), the total grows but converges to a finite steady state — the hallmark of a correct radiosity solve, and exactly what the unit test pins down. Pure CPU (built on the tested hemisphere gather), so it verifies headlessly; the result is a per-patch radiance a lightmap/probe bake stores for the GPU to sample.  Scope note (honest): a Jacobi-iterated patch radiosity (each patch = one triangle), diffuse only, `bounces` passes. Adaptive subdivision, form-factor caching, and hemicube acceleration are the offline optimizations on top; this is the correct-but-unoptimized reference the GPU/offline path can accelerate.

**Types:** `RadiosityPatch`

**Functions:**

- `inline math::vec3 patchCentroid(const RadiosityPatch& p)`
- `inline math::vec3 patchNormal(const RadiosityPatch& p)`
- `inline void bakeRadiosity(std::vector<RadiosityPatch>& patches, const GiBakeOptions& opt =`

### `ReflectionProbe`
<sub>`engine/include/maz/render/ReflectionProbe.hpp`</sub>

maz::render reflection probe influence + box projection — the CPU math behind Godot's ReflectionProbe. A probe captures the surroundings into a cubemap inside an axis-aligned box; reflective surfaces in that box sample it. Two pieces are pure math and testable without a GPU: (1) `influenceWeight`, how strongly a probe affects a point — full inside the box, fading to zero across a blend margin near the faces, so overlapping probes cross-fade; and (2) `boxProjectDirection`, the parallax correction that makes a box-captured cubemap look right off flat walls — it intersects the reflection ray with the box and re-aims the sample from the probe center to that hit point (without this, reflections slide as the camera moves). Combined with the M506 cubemap mapping, `probeSample` gives the face/UV a surface should read.  Scope note (honest): axis-aligned box probes, linear face blend, box-projection parallax. It does not capture the cubemap (a GPU render pass), prefilter roughness mips, or blend probe *colors* (that is the shader compositing the weights); this provides the weights and directions it needs.

**Types:** `ReflectionProbe`

**Functions:**

- `inline float influenceWeight(const ReflectionProbe& probe, const math::vec3& point)`
- `inline bool probeContains(const ReflectionProbe& probe, const math::vec3& point)`
- `inline math::vec3 boxProjectDirection(const ReflectionProbe& probe, const math::vec3& worldPos,`
- `inline CubeSample probeSample(const ReflectionProbe& probe, const math::vec3& worldPos,`

### `Renderer`
<sub>`engine/include/maz/render/Renderer.hpp`</sub>

**Types:** `RendererConfig`, `Color`, `BlendMode`, `SpriteDesc`, `Point2`, `PolyVertex`, `Camera2D`, `MeshVertex`, `RenderStats`, `SceneLighting`, `Renderer`

### `ScreenSpaceIndirectLight`
<sub>`engine/include/maz/render/ScreenSpaceIndirectLight.hpp`</sub>

maz::render screen-space indirect light (SSIL) — the CPU reference for Godot's SSIL pass. Where SSAO darkens creases by counting nearby occluders, SSIL goes one step further and gathers the *colored* light bouncing off those nearby surfaces: a red wall tints the white floor beside it red, a lit surface throws a soft colored glow onto its neighbours. This is one-bounce screen-space global illumination.  The full effect runs in a fragment shader over the depth/normal/color G-buffer on the GPU; this header is the gather MATH, decoupled from any GPU, so it is unit-testable headlessly (exactly how the SSAO pass was validated). For each pixel it samples a screen-space neighbourhood, and for every neighbour that sits in the pixel's hemisphere (in front of its surface) and within a world-space radius, it accumulates that neighbour's color weighted by the cosine term and a distance falloff. The result is the indirect (bounced) light to add to the pixel. Plain float math, no dependencies.

**Types:** `SsilSample`, `SsilColor`, `SsilParams`

**Functions:**

- `inline std::vector<SsilColor> ssilGather(int w, int h, const std::vector<SsilSample>& img,`

### `ScreenSpaceReflection`
<sub>`engine/include/maz/render/ScreenSpaceReflection.hpp`</sub>

maz::render screen-space reflections (SSR) — the CPU ray-march that drives Godot's SDFGI/SSR-style mirror reflections on glossy floors, wet streets, and metal. SSR reflects what's already on screen: for a reflective fragment it bounces the view vector about the surface normal, then MARCHES that reflection ray forward, projecting each step back into the depth buffer and checking whether the ray has passed *behind* a visible surface. The first crossing (refined by a short binary search) is the reflected pixel; its UV is where the shader reads the color to mirror. The exact same march runs in a fragment/compute shader on the GPU — but because it is pure projection + depth comparison, it is fully unit-testable here against a hand-built depth buffer (a ray aimed at a wall must land on that wall's UV; a ray into empty screen or off the edge must miss), which is the part SSR most often gets subtly wrong.  Honest tag (see docs/GODOT_GAPS_ROADMAP.md): the TRACE + PROJECTION math below is CPU-verified here and is the arithmetic the SSR shader runs. Producing the actual reflected image (sampling the color buffer at the returned UV, roughness blur, temporal accumulation) is the GPU pass, verified on the owner's machine.

**Types:** `SsrCamera`, `DepthBuffer`, `SsrParams`, `SsrHit`

**Functions:**

- `inline bool ssrProject(const SsrCamera& cam, float x, float y, float z, float& u, float& v)`
- `inline SsrHit ssrTrace(const SsrCamera& cam, const DepthBuffer& depth,`
- `inline void reflect(float dx, float dy, float dz, float nx, float ny, float nz,`

### `SeamCarve`
<sub>`engine/include/maz/render/SeamCarve.hpp`</sub>

maz::render::SeamCarve — content-aware image resizing (Avidan & Shamir 2007). Ordinary scaling squashes everything uniformly; seam carving instead removes the least-important pixels, so a photo can be narrowed while the interesting subject keeps its shape and only the bland background is squeezed out. It works by scoring every pixel with an "energy" (how much its colour differs from its neighbours — edges are high, flat regions low), then repeatedly deleting a *seam*: a connected, one-pixel-wide path from top to bottom (or left to right) that threads through the lowest total energy. The optimal seam is found by dynamic programming, and removing it shifts the rest of the row over, shrinking the image by one column at a time. Godot has no content-aware resize (Image.resize only interpolates); this is the real thing, exact on the CPU and fully testable — the chosen seam provably minimizes total energy, verifiable against brute force. Header-only, std-only, deterministic (ties break to the leftmost column).

**Functions:**

- `inline float seamEnergyAt(const Image& img, int x, int y)`
- `inline std::vector<int> findVerticalSeam(const Image& img)`
- `inline Image removeVerticalSeam(const Image& img, const std::vector<int>& seam)`
- `inline Image seamTranspose(const Image& img)`
- `inline std::vector<int> findHorizontalSeam(const Image& img)`
- `inline Image carveWidth(const Image& img, int targetWidth)`
- `inline Image carveHeight(const Image& img, int targetHeight)`

### `SeamlessClone`
<sub>`engine/include/maz/render/SeamlessClone.hpp`</sub>

maz::render::seamlessClone — Poisson image editing (Pérez et al. 2003): paste a patch of one image into another so the seam DISAPPEARS. Naively copying pixels leaves a hard edge whenever the patch's lighting or colour differs from its new surroundings. Seamless cloning instead copies the patch's GRADIENTS (its internal detail) while forcing its border to match the destination exactly, then solves a Poisson equation to fill the interior — so the patch keeps its texture and shapes but its overall tone slides to blend perfectly with the background. This is how "healing brush" / seamless compositing works, and it is handy for decals, damage overlays, terrain-splat blending, and stitching texture tiles without visible joins. Solves one Poisson problem per colour channel with the destination as the boundary condition. Godot has no gradient-domain compositing. Header-only, deterministic; built on math::poissonSolve.

**Functions:**

- `inline Image seamlessClone(const Image& dest, const Image& source, const Image& mask, int offX, int offY,`

### `Shapes`
<sub>`engine/include/maz/render/Shapes.hpp`</sub>

CPU geometry ready to hand to Renderer::createMesh. Normals point outward; every vertex is tinted `color`.

**Types:** `MeshData`

### `Shapes2D`
<sub>`engine/include/maz/render/Shapes2D.hpp`</sub>

maz::render 2D SHAPE OUTLINES — ready-made point rings for the common flat shapes, so you don't hand-type coordinates. Each function returns a counter-clockwise list of 2D points tracing the outline of a shape, which you feed straight into `extrudePolygon` (M602) to make a 3D prism, `revolveProfile` (M594) to spin a solid, `triangulatePolygon` (M156) to fill it flat, or a 2D polygon collider. Between them they cover most of what UI, signage, and props need: a regular n-gon (hexagon nut, pentagon, octagon stop-sign), a star or sparkle, a rounded rectangle (button, card, badge, panel, rounded platform), a spur gear / cog (machinery, clocks, steampunk), and a pie slice / sector (gauges, pie charts, vision cones). Header-only, deterministic, headless — pure coordinate math.  Scope note (honest): all outlines are simple (non-self-intersecting) and wound COUNTER-CLOCKWISE, centred on the origin. A star with a big enough inner radius stays simple; a rounded rect clamps its corner radius to at most half the shorter side. These are OUTLINES (point rings), not filled meshes — pair them with the extrude/fill/ revolve tools above.

**Functions:**

- `inline std::vector<math::vec2> regularPolygon(int sides, float radius, float startAngle = 0.0f)`
- `inline std::vector<math::vec2> star(int points, float outerRadius, float innerRadius)`
- `inline std::vector<math::vec2> roundedRect(float width, float height, float radius, int cornerSegments = 4)`
- `inline std::vector<math::vec2> gear(int teeth, float outerRadius, float rootRadius, float toothWidthFrac = 0.5f)`
- `inline std::vector<math::vec2> pieSlice(float radius, float startAngle, float sweepAngle, int segments)`

### `Shapes3D`
<sub>`engine/include/maz/render/Shapes3D.hpp`</sub>

**Functions:**

- `inline MeshVertex vtx(float px, float py, float pz, float nx, float ny, float nz, const Color& c,`
- `inline MeshData makeCylinder(float radius, float height, int sectors, const Color& color)`
- `inline MeshData makeCone(float radius, float height, int sectors, const Color& color)`
- `inline MeshData makeTorus(float majorRadius, float minorRadius, int majorSegs, int minorSegs,`
- `inline MeshData makeCapsule(float radius, float cylHeight, int sectors, int rings, const Color& color)`

### `SobelEdge`
<sub>`engine/include/maz/render/SobelEdge.hpp`</sub>

maz::render Sobel edge detection — find the edges (sharp brightness changes) in a grayscale image.  The Sobel operator convolves the image with two small 3x3 kernels that estimate the horizontal (Gx) and vertical (Gy) brightness gradient at each pixel; the gradient MAGNITUDE sqrt(Gx^2+Gy^2) is large exactly where the image changes fast — i.e. on an edge — and the DIRECTION atan2(Gy,Gx) points across it. This is the classic building block behind toon/outline post-processing (run it on depth or normals to draw ink lines), sprite/UI outline generation, and general image analysis. Borders use clamp-to-edge so every pixel gets a value. Pure CPU, header-only, deterministic — unit-tested against hand-computed Sobel responses (a flat image gives zero; a vertical step edge gives Gx=+/-4, Gy=0, and vice versa).

**Types:** `EdgeField`

**Functions:**

- `inline EdgeField sobel(const float* gray, int w, int h)`
- `inline EdgeField sobel(const std::vector<float>& gray, int w, int h)`
- `inline std::vector<unsigned char> edgeMask(const EdgeField& e, float threshold)`

### `SpriteOrder`
<sub>`engine/include/maz/render/SpriteOrder.hpp`</sub>

maz::render::SpriteOrder — deterministic 2D draw ordering: canvas layers, per-item z-index, and Y-sort, the three knobs a 2D engine gives artists to control what draws in front of what. This is the sorting Godot's CanvasLayer.layer + Node2D.z_index + Y-Sort node provide, as a pure, dependency-light algorithm the sprite/polygon batcher can call each frame before flushing.  Ordering (earlier = drawn first = further BACK; later = drawn on top): 1. layer     — coarse canvas layer (a HUD layer above the world, a background layer below it) 2. zIndex    — fine ordering within a layer (Godot's -4096..4096 z_index) 3. y-sort    — when enabled on an item, larger world-Y draws LATER (in front), so a character lower on the screen correctly occludes one higher up (top-down / 2.5D depth) 4. insertion — stable tie-breaker, so equal keys keep their submission order (no flicker)  sortedIndices() returns the input indices in draw order without moving the caller's data; sort() reorders a vector of items in place. Y-sort is per-item (items with useYSort=false ignore their y, matching Godot where only YSort-parented nodes participate). Header-only, deterministic.

**Types:** `DrawItem`

**Functions:**

- `inline bool drawsBefore(const DrawItem& a, const DrawItem& b, size_t ia, size_t ib)`
- `inline std::vector<uint32_t> sortedIndices(const std::vector<DrawItem>& items)`
- `inline void sort(std::vector<DrawItem>& items)`

### `StlLoader`
<sub>`engine/include/maz/render/StlLoader.hpp`</sub>

maz::render STL (.stl) mesh importer — closes another import gap versus Godot's asset pipeline. STL is the universal interchange format for 3D printing and CAD (SolidWorks, Blender, Meshmixer, every slicer), storing a flat triangle soup: each triangle carries a face normal and its three corner positions, with no shared vertices, UVs, or materials. `parseStl` reads BOTH encodings — the ASCII form (`solid` / `facet normal` / `vertex` keywords) and the binary form (80-byte header + uint32 triangle count + 50-byte records) — auto-detecting which one a buffer is, and emits a `shapes::MeshData` (three vertices per triangle, sequential indices) ready for `Renderer::createMesh`. Pure CPU byte/string work — no GPU — so it unit-tests headlessly from an in-memory buffer; `loadStl` wraps it for files.  Scope note (honest): triangle geometry + per-face normals only. STL stores no texture coordinates and no standard color, so UVs are zero and every vertex takes the fallback tint; the rare per-face color found in some binary "attribute byte count" extensions is not decoded. Because STL normals are frequently absent or wrong, `recomputeNormals` (and the automatic fallback when a stored normal is zero-length) derives the geometric normal from the winding. Malformed input returns false rather than throwing.

**Types:** `StlLoadOptions`

**Functions:**

- `inline bool stlIsBinary(const std::uint8_t* data, std::size_t size)`
- `inline float stlF32le(const std::uint8_t* p)`
- `inline void stlEmitTriangle(shapes::MeshData& out, const float p0[3], const float p1[3], const float p2[3],`
- `inline bool parseStl(const std::uint8_t* data, std::size_t size, shapes::MeshData& out,`
- `inline bool parseStl(const std::vector<std::uint8_t>& bytes, shapes::MeshData& out,`
- `inline bool loadStl(const std::string& path, shapes::MeshData& out, const StlLoadOptions& opts =`

### `Subdivision`
<sub>`engine/include/maz/render/Subdivision.hpp`</sub>

maz::render mesh subdivision — refine an indexed triangle mesh into a denser one, either LINEARLY (each triangle split into four by its edge midpoints, geometry unchanged) or with LOOP SMOOTHING (the standard subdivision surface: new points are weighted averages that round the silhouette toward the limit surface). Linear subdivision adds detail for per-vertex effects (displacement, vertex lighting, wave deformation) without changing the shape; Loop turns a blocky low-poly cage into a smooth organic form — the runtime counterpart to a modelling package's subdivision modifier. Both share the same topology step (every triangle becomes four, one new vertex per unique edge) and differ only in where the new and original vertices land. Operates on the engine's positions+indices mesh form (as used by MeshTools). Godot exposes no runtime subdivision to gameplay code, so this is a beyond-Godot geometry utility. Header-only, std-only, deterministic.

**Types:** `SubdivMesh`

**Functions:**

- `inline std::uint64_t edgeKey(std::uint32_t a, std::uint32_t b)`
- `inline SubdivMesh subdivideOnce(const std::vector<math::vec3>& pos,`
- `inline SubdivMesh subdivideMesh(const std::vector<math::vec3>& positions,`
- `inline SubdivMesh subdivideMeshLoop(const std::vector<math::vec3>& positions,`

### `SurfaceNets`
<sub>`engine/include/maz/render/SurfaceNets.hpp`</sub>

maz::render isosurface mesher — turns a signed distance field (see game::Csg) into a real triangle mesh, the step that gives Godot's CSG nodes their actual polygons. This uses Naive Surface Nets: the field is sampled on a grid; every cell the surface passes through gets ONE vertex placed at the mean of the surface crossings on that cell's edges; and each grid edge that flips sign emits a quad tying the four cells around it together. The result is a watertight, evenly-tessellated mesh that follows the field — union/intersection/subtraction of primitives come out as one connected surface. Pure CPU + deterministic (no GPU), so it unit-tests headlessly by checking that every output vertex lies on the isosurface; the renderer just uploads the buffers.

**Types:** `SdfMesh`

**Functions:**

- `inline SdfMesh surfaceNets(const DistanceFn& field, const math::vec3& mn, const math::vec3& mx,`

### `Thinning`
<sub>`engine/include/maz/render/Thinning.hpp`</sub>

maz::render::thinZhangSuen — morphological thinning (Zhang-Suen 1984): reduce a filled binary shape to its one-pixel-wide SKELETON, the centerline that captures the shape's topology. Where dilate/erode grow or shrink a region, thinning peels a shape down to its bones without breaking it apart — turning a thick blob into a stick-figure medial axis. It's the standard tool for extracting road/river centerlines from a mask, stroke skeletons for handwriting or gesture analysis, path graphs from painted regions, and shape descriptors. The algorithm repeatedly deletes boundary pixels whose removal neither breaks connectivity nor shortens an endpoint, in two alternating sub-passes, until nothing more can be removed. Godot has erode/dilate but no thinning. Operates on a width*height grid of 0/1. Header-only, std-only, deterministic.

**Functions:**

- `inline void thinNeighbours(const std::vector<std::uint8_t>& g, int w, int h, int x, int y, int p[8])`
- `inline std::vector<std::uint8_t> thinZhangSuen(int width, int height, const std::vector<std::uint8_t>& image)`

### `Tonemap`
<sub>`engine/include/maz/render/Tonemap.hpp`</sub>

maz::render tonemapping operators — the HDR-to-displayable color curves every modern renderer applies as its final step. A physically-lit scene produces radiance well above 1.0 (a bright sky, a specular highlight, an explosion); a display can only show [0,1], so a tonemap curve compresses that unbounded range into the visible one while keeping shadows, midtones, and highlights looking natural. This is the CPU-side math (what the tonemap fragment shader evaluates per pixel): ACES filmic (Narkowicz's widely-used fit — the current default look, with its characteristic gentle shoulder and slight contrast), Reinhard (the classic x/(1+x)) and its white-point-extended form, and the Hejl-Burgess / Uncharted2 filmic curve. Pure float math, no allocation — exactly unit-testable (monotonic, maps 0 to 0, saturates to 1). The engine already has the HDR target + GPU tonemap pass; this is the reusable, testable curve itself (also handy for CPU-side color grading, thumbnails, and golden images).

**Types:** `Color3`

**Functions:**

- `inline float tonemapClamp01(float x)`
- `inline float acesFilmic(float x)`
- `inline Color3 acesFilmic(const Color3& col)`
- `inline float reinhard(float x)`
- `inline float reinhardExtended(float x, float whitePoint)`
- `inline float uncharted2Partial(float x)`
- `inline float uncharted2(float x, float exposureBias = 2.0f, float whitePoint = 11.2f)`
- `inline float applyExposure(float x, float exposure)`

### `Trail`
<sub>`engine/include/maz/render/Trail.hpp`</sub>

maz::render::Trail — the motion-ribbon behind sword swings, projectile streaks, dash after-images, and vehicle skid trails: keep a short history of where a point has been, and turn it into a flat ribbon of triangles that tapers to nothing at the old end and fades out over a lifetime. Every frame you push the current position and advance time; old points expire, and buildRibbon() emits a camera-facing quad strip (two vertices per history point, offset sideways perpendicular to both the trail's direction and the view direction) ready to draw. The width tapers by age so the head is full-width and the tail pinches to a point — the look everyone expects from a trail. This is a common Godot request (its built-in trails live only inside the particle system); here it is a small standalone builder over an arbitrary moving point. Header-only, std-only, deterministic — the ribbon geometry is exact CPU-side and testable without a GPU.

**Types:** `RibbonMesh`, `Trail`

### `TriangleQuality`
<sub>`engine/include/maz/render/TriangleQuality.hpp`</sub>

maz::render TRIANGLE-QUALITY / sliver analysis — the mesh-QA pass that flags badly-SHAPED triangles: long thin slivers and needles shade poorly, self-shadow, crawl under rasterization, and wreck physics and simplification. Every meshing tool reports a "triangle quality / minimum angle" histogram for exactly this. Per triangle it computes the normalized shape quality q = 4·sqrt(3)·area / (a²+b²+c²) — the standard mean- ratio metric that is 1 for a perfect equilateral triangle and falls toward 0 as a triangle degenerates into a sliver — plus its smallest interior angle (degrees, the number artists eyeball). It summarizes the worst triangle, the sliver count under a threshold, and the degenerate count. Pure CPU, header-only, headless.

**Types:** `TriangleQualityStats`

**Functions:**

- `inline TriangleQualityStats analyzeTriangleQuality(const shapes::MeshData& mesh, float sliverThreshold = 0.1f)`

### `UvDensity`
<sub>`engine/include/maz/render/UvDensity.hpp`</sub>

maz::render UV / TEXEL-DENSITY analysis — the asset-QA pass that checks a mesh's texture coordinates are SANE before it ships: is the texel density uniform (so the texture looks equally sharp everywhere, no stretched or blurry patches), and how much of the UV square does the layout actually use? Every DCC and Godot's lightmap importer offer a "texel density / UV stretch" checker for exactly this. Per triangle it computes the ratio of UV area to 3D (world) area — constant across the mesh means consistent density; an outlier means a stretched or over-sampled face. It also totals the world and UV areas (UV coverage of the [0,1] square) and counts degenerate faces. Pure CPU, header-only, headless.

**Types:** `UvDensityStats`

**Functions:**

- `inline UvDensityStats analyzeUvDensity(const shapes::MeshData& mesh)`

### `VertexCacheOptimize`
<sub>`engine/include/maz/render/VertexCacheOptimize.hpp`</sub>

maz::render vertex-cache optimization (Forsyth's linear-speed algorithm) — reorders a mesh's triangle INDICES so the GPU's post-transform vertex cache hits far more often, a standard load-time mesh optimization every serious engine (and Godot's importer) runs. The GPU caches the last handful of transformed vertices; if consecutive triangles reuse those vertices the vertex shader runs far fewer times. A mesh straight out of an exporter is often ordered badly for this (e.g. a grid emitted row-by-row thrashes the cache). This rewrites the index order — the SAME triangles, just sequenced so neighbours share vertices — measured by ACMR (Average Cache Miss Ratio: cache misses per triangle; lower is better, ~0.5 is the floor for large closed meshes). Vertex positions are untouched, so it is a pure, lossless index permutation. Pure integer work — no GPU — so it unit-tests headlessly: the triangle SET is preserved and the simulated-cache ACMR drops.  Reference: Tom Forsyth, "Linear-Speed Vertex Cache Optimisation" (2006).

**Functions:**

- `inline float simulateAcmr(const std::vector<std::uint32_t>& indices, int cacheSize = 32)`
- `inline float vertexScore(int cachePos, int remainingTris)`
- `inline std::vector<std::uint32_t> optimizeVertexCache(const std::vector<std::uint32_t>& indices,`

### `VolumetricFog`
<sub>`engine/include/maz/render/VolumetricFog.hpp`</sub>

maz::render analytic volumetric fog — the CPU evaluation behind Godot's height/volumetric fog. It answers "how much fog is between the camera and this point, and what color does it leave the pixel?" via the Beer–Lambert law: it integrates the fog density along a view segment (with optional exponential height falloff — thicker low, thinner high, like real mist) to an optical depth, converts that to a blend factor `1 - exp(-optical)`, and mixes the scene color toward the fog color. Godot's volumetric fog raymarches a froxel volume on the GPU; this is the closed-form analytic evaluation, exact and unit- testable on the CPU (a renderer can use it directly for simple exponential fog or as a reference).  Scope note (honest): homogeneous or exponential-height density with a single fog color. It does not do GPU froxel scattering, per-light in-scatter, noise/wind animation, or fog volume shapes; those are the renderer's job.

**Types:** `FogParams`

**Functions:**

- `inline float fogOpticalDepth(const FogParams& fog, const math::vec3& from, const math::vec3& to)`
- `inline float fogFactor(const FogParams& fog, const math::vec3& from, const math::vec3& to)`
- `inline math::vec3 applyFog(const math::vec3& sceneColor, const FogParams& fog, const math::vec3& from,`


<a name="scene"></a>
## Scene — node tree, transforms, serialization

### `CanvasLayer`
<sub>`engine/include/maz/scene/CanvasLayer.hpp`</sub>

maz::scene CanvasLayer — Godot's CanvasLayer node: an independent 2D drawing layer with its own transform and a draw-order index, used to pin HUDs/UI to the screen (unaffected by the world camera) or to build parallax-free overlays. Its children draw through the layer's transform (offset / rotation / scale). By default the layer is screen-fixed; with follow_viewport enabled it instead tracks the world/viewport canvas transform, optionally scaled. A CanvasLayerStack keeps several layers ordered by their `layer` index (low draws first, behind). Pure transform maths on math::Transform2D, header-only, deterministic — the renderer consumes finalTransform()/drawOrder() to place and order the layers.

**Types:** `CanvasLayer`, `CanvasLayerStack`

### `GroupRegistry`
<sub>`engine/include/maz/scene/GroupRegistry.hpp`</sub>

Node groups — Godot's SceneTree groups (add_to_group / get_nodes_in_group / call_group / is_in_group). A group is a named tag you attach to any node; the registry answers "give me every node tagged X" (find all enemies, all save-points, everything to pause) and "run this on every node in X" — without the caller keeping and maintaining its own lists. Ids are plain integers, so this layers over ecs::World entities, scene::TransformGraph nodes, or an app's own handles. Membership is unique and kept in INSERTION ORDER, so queries and broadcasts are deterministic under the fixed timestep. A reverse index (node -> its groups) makes "which groups is this in?" and whole-node removal cheap. Header-only, no engine dependencies.

**Types:** `GroupRegistry`

### `Prefab`
<sub>`engine/include/maz/scene/Prefab.hpp`</sub>

Prefabs / instancing — Godot's PackedScene, the single most defining thing about Godot's workflow. A Prefab is a reusable TEMPLATE: a tree of named nodes, each carrying a bag of exported properties (position, colour, hp, …). You author it once and then INSTANTIATE it many times, each instance applying per-node OVERRIDES so every copy differs (a different spawn position, tint, or stat) without duplicating the template. Instancing produces a fresh, independent node tree — mutating one instance never touches the template or its siblings. Pure data (no GPU); header-only; unit-tests headlessly.

**Types:** `PropValue`, `PrefabNode`, `Prefab`

**Functions:**

- `inline const PropValue* findProp(const PropBag& bag, const std::string& key)`
- `inline void setProp(PropBag& bag, const std::string& key, const PropValue& value)`
- `inline float getFloat(const PropBag& bag, const std::string& key, float def = 0.0f)`
- `inline int getInt(const PropBag& bag, const std::string& key, int def = 0)`
- `inline bool getBool(const PropBag& bag, const std::string& key, bool def = false)`
- `inline math::vec2 getVec2(const PropBag& bag, const std::string& key, math::vec2 def = math::vec2(0.0f))`
- `inline math::vec4 getColor(const PropBag& bag, const std::string& key,`
- `inline PrefabNode* findNode(PrefabNode& root, const std::string& path)`
- `inline PrefabNode instantiate(const Prefab& prefab, const OverrideMap& overrides =`
- `inline int nodeCount(const PrefabNode& node)`

### `RemoteTransform2D`
<sub>`engine/include/maz/scene/RemoteTransform2D.hpp`</sub>

maz::scene RemoteTransform2D — Godot's RemoteTransform2D node: a node that pushes its own transform onto some *other* node each frame (handy for driving a decoupled follower, a camera rig, or a hierarchy-crossing attachment). It mirrors Godot's toggles — copy position, rotation, and/or scale independently — plus the global-vs-local coordinate choice. This is the pure transform-merge maths: given the remote's transform and the target's current transform, it returns the target's new transform with only the enabled channels overwritten. When all three channels are on it copies the whole transform verbatim (so skew survives); otherwise it recomposes from the selected components. Header-only, deterministic, unit-tested; the SceneTree integrator supplies the transforms per frame.

**Types:** `RemoteTransform2D`

### `SceneSerialize`
<sub>`engine/include/maz/scene/SceneSerialize.hpp`</sub>

maz::scene text (de)serialization — Maz's answer to Godot's `.tscn` scene files. A whole node tree (structure + per-node transform, visibility, groups, and script class reference) round-trips to a small, human-readable, line-based text format. Scripts live in the script program (load them with SceneTree::loadScripts before loadTree); the scene only references classes by name.  std::string text = scene::saveTree(tree); SceneTree other; other.loadScripts(programSource); scene::loadTree(other, text);   // rebuilds the identical hierarchy, re-attaches scripts

**Functions:**

- `inline std::string num(double v)`
- `inline std::string pathOf(const SceneNode& node)`
- `inline std::string field(const std::string& line, const std::string& key)`
- `inline std::string writeNode(const SceneNode& node)`
- `inline std::string saveTree(SceneTree& tree)`
- `inline bool loadTree(SceneTree& tree, const std::string& text)`

### `SceneTree`
<sub>`engine/include/maz/scene/SceneTree.hpp`</sub>

maz::scene::SceneTree — the unified node hierarchy, Maz's answer to Godot's SceneTree + Node2D. It ties together the three things a real game needs in one model: 1. A tree of named nodes with parent/child relationships. 2. 2D transform hierarchy — a child's world transform composes with its parent's (position, rotation, and scale all propagate), so moving a parent moves its whole subtree. 3. Attached scripts — a node can carry a maz::script class whose _ready / _process(dt) / _physics_process(dt) hooks the tree drives in deterministic depth-first order, with the node's LOCAL transform exposed to the script as `self.node` (SC6 binding). Plus groups (broadcast to tagged nodes) and path lookup ("Player/Weapon"). This is the substrate a whole game — the ZOMBOID port included — is described and simulated on.

**Types:** `SceneTree`, `SceneNode`

### `TransformGraph`
<sub>`engine/include/maz/scene/TransformGraph.hpp`</sub>

A 2D transform hierarchy (scene graph): every node has a LOCAL transform (position, rotation, scale) relative to its parent, and update() propagates those into WORLD transforms parent-first. This is the structural backbone for composite objects — a turret on a tank, a moon around a planet, a hand on an arm, a health bar pinned to a unit: move or rotate the parent and the whole subtree follows. Composition is the standard decomposed TRS (world rotation = sum, world scale = product, world position = parent position + parent-rotated, parent-scaled local position), which is exact for uniform scale and the pragmatic norm for 2D engines. Header-only.

**Types:** `Transform2D`, `TransformGraph`


<a name="script"></a>
## Script — the maz::script language VM + engine binding

### `Debugger`
<sub>`engine/include/maz/script/Debugger.hpp`</sub>

maz::script source-level debugger — the CPU core of a script debugger (Godot's ScriptDebugger / remote debug protocol). It drives the tree-walking VM through its per-statement onStep hook and adds what a real debugger needs: line breakpoints, the four stepping modes (into / over / out / continue) resolved from call-stack depth, a call-stack snapshot at each stop, and variable inspection of the paused frame. Execution pauses *synchronously*: when a stop condition is hit the VM calls your onPause handler on the same stack, you inspect state and return the next step mode, and execution resumes — exactly how an embedded debug hook works. Pure CPU + deterministic, so it unit-tests headlessly. Wiring this to a remote IDE over a socket is the [DESK] transport layer on top; the decision logic and inspection here are complete and testable.

**Types:** `StepMode`, `PauseEvent`, `Debugger`

### `Script`
<sub>`engine/include/maz/script/Script.hpp`</sub>

maz::script — a small dynamically-typed scripting language with a tree-walking interpreter, the engine's answer to Godot's GDScript. Game logic can live in text scripts (hot-reloadable, no recompile) instead of compiled C++, and native C++ functions are exposed to scripts through a simple binding API. It is a pure, dependency-free, deterministic VM (no globals, no allocation surprises) — a deliberate edge over an embedded third-party runtime: a script can never reach outside the API the host hands it.  SC1: numbers / strings / bools / nil, the full operator set, variables, if/else, while, C-style for, functions + return, native host functions, line-numbered errors. SC2: arrays [..] and dictionaries {k: v} (reference semantics), indexing a[i] / d[k] / d.k with read + write, for-in over arrays / dict keys / ranges / string chars, break / continue, the `in` / membership operator, method calls (arr.append(x), dict.keys(), ...), len()/range(). SC3: string / math / conversion stdlib + a seedable deterministic RNG + assert. SC4: first-class functions — lambdas (func(x){...}), real closures capturing (and mutating) the scope they were defined in, and higher-order array methods (map/filter/reduce/any/all/ sort/sort_custom). Environments are heap-allocated (make_shared) so a returned closure keeps its captured scope alive after the enclosing call returns. SC5: classes — `class Foo { var fields; func methods }` with `self`, the `_init` constructor, `Foo.new(...)` / `Foo(...)` construction, single inheritance (`extends`) and `super` dispatch. Instances have reference semantics; methods read off an instance are bound callables. Classes are top-level, hoisted like functions. SC6: host-object binding — bindClass("T").property(get,set).method(fn) exposes a C++ type; makeNativeObject wraps a live host object behind a weak handle (touching a freed object is a catchable error, not a crash). instantiate()/objectHasMethod()/callOn() let the engine drive script instances' _ready / _process(dt) / _physics_process(dt) lifecycle hooks. SC7: signals — class-level `signal name;` fields + a standalone Signal() builtin; connect / disconnect / is_connected / emit / connection_count, one-shot connections, and sync-vs-deferred dispatch (emit_deferred queues; the host drains it via flushDeferred() for deterministic netcode ordering). Coroutine `await` is deferred to a VM-core pass. SC8: safety & diagnostics — stack traces on error (stackTrace()); an execution step budget (setStepBudget) and recursion limit (setRecursionLimit) that turn a runaway loop/recursion into a catchable error instead of a hang/crash; and a static warnings pass (warnings()) for variable shadowing and unreachable code. SC9: hot reload — reload(source) swaps in new code without a restart, updating global functions and class method bodies IN PLACE so live instances keep their field state while gaining the new behavior. A lex/parse failure leaves the previous version fully live. Old ASTs are retained so still-referenced closures stay valid. (Top-level statements are not re-run.) SC10: gradual typing — type hints (var x: int, func f(a: int) -> T, Array[int]) + inference (var x := ...); a static type-checker flags literal-level mismatches (typeErrors()); and setStrictTypes() promotes them to run() failures + enforces typed declarations at runtime. Untyped code stays fully dynamic. (Typed fast-path optimization is deferred.) SC11: modules & tooling — import "name" pulls a host-registered module's funcs/classes into scope (registerModule; transitive + cycle-safe, no filesystem access); introspection (has_method / call-by-name / get_property / set_property / has_property / class_name); and debugger hooks (onStep per statement + addBreakpoint/onBreakpoint). The SC1–SC11 roadmap is complete; a true coroutine `await` and a typed fast-path await a VM-core pass.  Everything is header-only to match the rest of maz::. The AST is owned by the Vm for the lifetime of a loaded program; runtime environments are reference-counted (shared_ptr) so closures capture.

**Types:** `FuncDef`, `Environment`, `ClassInfo`, `Instance`, `NativeClass`, `NativeObjectData`, `SignalData`, `Value`, `Tok`, `Token`, `ScriptError`, `Stmt`, `Expr`, `Parser`, `Vm`

**Functions:**

- `inline std::vector<Token> lex(const std::string& src, ScriptError& err)`
- `inline Value Value::newSignal(std::string name)`

### `ScriptSystem`
<sub>`engine/include/maz/script/ScriptSystem.hpp`</sub>

maz::script::ScriptSystem — the bridge that makes maz::script actually *drive the engine*. It attaches a script class (see SC5) to a game node and runs that instance's lifecycle hooks (_ready / _process(dt) / _physics_process(dt), see SC6) each frame, exposing the node's transform to the script through a bound "Node2D" host type (SC6 binding). This is the capstone that turns the scripting language from "a language" into "how you write Maz gameplay" — the same script-attached-to-node model as Godot, but sandboxed and deterministic.  ScriptSystem sys; sys.registerScript("Spinner", "func _process(dt) { self.node.rotation = self.node.rotation + dt; }"); auto node = sys.spawn("Spinner");   // returns a Node2D the host can read/move sys.process(0.016);                 // drives every attached _process(dt) float r = node->rotation;           // the script moved it

**Types:** `Node2D`, `ScriptInstance`, `ScriptSystem`

### `Tooling`
<sub>`engine/include/maz/script/Tooling.hpp`</sub>

maz::script::tooling — editor/IDE tooling for the engine's GDScript-style language: a document OUTLINE (the symbols a script declares — classes, functions, signals, variables) and CONTEXT-AWARE AUTOCOMPLETE (the candidate identifiers at a cursor position). This is the "deepen the script VM toward GDScript-grade tooling" side of the roadmap's scripting gap: the data an editor needs to draw a symbol tree, offer completions, and show a function's parameters in a tooltip.  It uses its own ERROR-TOLERANT scanner rather than the VM's lex() (which bails on the first bad character): editors must work on half-typed, not-yet-valid code, so the scanner skips what it can't classify and keeps going. The token grammar mirrors the VM lexer exactly — `#` and `//` line comments, "..." strings, numbers, identifiers, and single-character punctuation — so the symbols it reports match what the VM would actually parse. Pure text analysis, no evaluation, unit-testable.

**Types:** `DocSymbol`, `Completion`, `Tk`

**Functions:**

- `inline const std::vector<std::string>& keywords()`
- `inline bool isKeyword(const std::string& s)`
- `inline bool isIdentStart(char c)`
- `inline bool isIdentChar(char c)`
- `inline std::vector<Tk> scan(const std::string& src)`
- `inline std::vector<DocSymbol> collectSymbols(const std::vector<Tk>& t)`
- `inline std::vector<DocSymbol> documentSymbols(const std::string& src)`
- `inline const DocSymbol* findFunction(const std::vector<DocSymbol>& syms, const std::string& name)`
- `inline std::string prefixAt(const std::string& src, size_t offset)`
- `inline bool isMemberAccessAt(const std::string& src, size_t offset)`
- `inline std::vector<Completion> completionsAt(const std::string& src, size_t offset)`


<a name="ecs"></a>
## ECS — entity/component/system world

### `Components`
<sub>`engine/include/maz/ecs/Components.hpp`</sub>

maz::ecs core components — the handful of engine-standard components every scene needs, so games don't reinvent "where is this / what is it called / what is it a kind of / who is its parent":  Transform      — local TRS (position, rotation quaternion, scale) + matrix() WorldTransform — the cached world matrix propagated from the hierarchy Parent         — a link to a parent entity (Godot's node parenting, ECS-style) Name           — a human-readable identifier Tag            — a 64-bit category bitmask (fast "is this an Enemy/Pickup/…" filtering)  plus propagateTransforms(World&), the hierarchy system that turns local Transforms + Parent links into WorldTransforms parent-first (a topological pass, cycle-safe). These mirror Godot's Node3D / name / groups, but as plain data an ECS system operates on. Header-only, no GPU.

**Types:** `Transform`, `WorldTransform`, `Parent`, `Name`, `Tag`

**Functions:**

- `inline void propagateTransforms(World& world)`

### `Scheduler`
<sub>`engine/include/maz/ecs/Scheduler.hpp`</sub>

maz::ecs::Scheduler — an ordered system runner with optional intra-phase parallelism. Systems are registered into named PHASES (e.g. Input < Simulation < LateUpdate) and, within a phase, an integer order; run() executes them deterministically phase-by-phase, order-by-order. Systems the caller marks parallelSafe (they touch disjoint data) can run concurrently within their phase via a core::JobSystem, with a hard barrier between phases so ordering across phases is always exact.  This is the "ordered + parallel system execution" an ECS scheduler needs — Godot runs node _process callbacks single-threaded, so deterministic phase ordering WITH opt-in parallelism inside a phase is a capability beyond it. Header-only; the JobSystem is the caller's (reused across frames).

**Types:** `Scheduler`

### `World`
<sub>`engine/include/maz/ecs/World.hpp`</sub>

A lightweight entity-component system. Entities are ids; components live in per-type sparse sets. Iterate with each<T>() or view<A, B>(). Header-only so component types stay generic.  World w; Entity e = w.create(); w.add<Transform>(e, {0, 0}); w.add<Velocity>(e, {10, 0}); w.view<Transform, Velocity>([&](Entity, Transform& t, Velocity& v) { t.x += v.vx * dt; });

**Types:** `IPool`, `Pool`, `World`


<a name="game"></a>
## Game — physics, collision, AI, pathfinding, tilemaps

### `AStar2D`
<sub>`engine/include/maz/game/AStar2D.hpp`</sub>

General weighted-graph A* over arbitrary 2D points — Godot's AStar2D. Unlike NavGrid (a uniform walkable/blocked grid) and NavMesh (A* across convex mesh cells), this is a free-form graph: you place points at any position with any integer id, connect them however you like (roads, rails, waypoint webs, teleporters, skill/ability graphs), then query the least-cost route. Each point carries a `weightScale` that multiplies the cost of moving INTO it, so you can make some nodes expensive (mud, danger) without moving them. Edges may be one-way (bidirectional=false). Traversal cost between connected points is their Euclidean distance × the destination's weight scale; the A* heuristic is the straight-line distance to the goal (admissible when every weight ≥ 1). Points are held in an ordered map and each point's neighbours in an ordered set, so identical graphs produce identical paths — deterministic, header-only, GPU-free, so it unit-tests exactly.  Honest scope vs Godot's AStar2D: this covers the graph, weights, one-/two-way links, id/point paths, closest-point and closest-position-in-segment queries. It does NOT override _compute_cost / _estimate_cost via subclassing (the cost model is fixed to weighted Euclidean), nor does it expose AStar3D or AStarGrid2D — those remain separate.

**Types:** `AStar2D`

### `AStar3D`
<sub>`engine/include/maz/game/AStar3D.hpp`</sub>

General weighted-graph A* over arbitrary 3D points — Godot's AStar3D, the 3D twin of AStar2D (M169). Place points at any position with any integer id, connect them however you like (flight lanes, jump links, teleporters, 3D waypoint webs), then query the least-cost route. Each point's `weightScale` multiplies the cost of moving INTO it; edges may be one-way. Traversal cost between connected points is their Euclidean distance × the destination's weight scale; the heuristic is straight-line distance (admissible when every weight >= 1). Ordered map + ordered neighbour sets -> identical graphs give identical paths. Deterministic, header-only, GPU-free — unit-tests exactly.  Honest scope vs Godot's AStar3D: covers the graph, weights, one-/two-way links, id/point paths, and closest-point / closest-position-in-segment queries. It does NOT override _compute_cost / _estimate_cost via subclassing (the cost model is fixed to weighted Euclidean).

**Types:** `AStar3D`

### `AStarGrid2D`
<sub>`engine/include/maz/game/AStarGrid2D.hpp`</sub>

maz::game AStarGrid2D — Godot's AStarGrid2D: A* pathfinding specialized for a dense rectangular grid of walkable/solid cells. Unlike the general weighted-graph AStar2D (arbitrary points + links) or the simple orthogonal-only NavGrid, this adds the two features that make grid pathfinding feel right: selectable DIAGONAL movement rules (never / only through open corners / around single obstacles / always) and selectable HEURISTICS (Euclidean / Manhattan / Octile / Chebyshev). Header-only, pure, deterministic — unit-tested cell by cell.

**Types:** `DiagonalMode`, `AStarGrid2D`

### `Achievements`
<sub>`engine/include/maz/game/Achievements.hpp`</sub>

maz::game achievement system — the unlock tracker behind "Achievement Unlocked!" toasts and the completion percentage on a save file. Each achievement has a target count (a simple one-shot trophy uses target 1; a grind like "defeat 100 enemies" uses target 100); the game reports progress and the system fires an unlock the moment the target is reached, reporting it back exactly once so a popup shows a single time. Progress clamps at the target, unlocking is one-way (until an explicit reset), and the registry answers per-achievement progress and an overall completion fraction for a stats screen. Godot ships no achievement system (games talk to Steam/console SDKs or hand-roll one) -> beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `Achievements`

### `AggroTable`
<sub>`engine/include/maz/game/AggroTable.hpp`</sub>

**Types:** `AggroTable`

### `AllPairsShortestPath`
<sub>`engine/include/maz/game/AllPairsShortestPath.hpp`</sub>

maz::game all-pairs shortest paths (Floyd-Warshall) — the shortest distance between EVERY pair of nodes in a weighted graph, precomputed in one O(V^3) pass. Unlike the engine's single-source pathfinders (AStar2D, DijkstraMap), this fills a whole distance matrix at once, which is the right tool for a precomputed routing/influence table on a small graph: "distance from every room to every other room", "which of my N bases is nearest to each threat", static AI cost tables. It handles negative edge weights (as long as there is no negative cycle, which it detects) and reconstructs the actual path via a next-hop matrix. Godot's AStar is single-pair, so all-pairs is a beyond-Godot utility. Header-only, std-only, deterministic.

**Types:** `WEdge`, `AllPairsResult`

**Functions:**

- `inline AllPairsResult allPairsShortestPaths(int nodeCount, const std::vector<WEdge>& edges)`

### `Area2D`
<sub>`engine/include/maz/game/Area2D.hpp`</sub>

Area2D — a sensor / trigger region. Unlike a rigid body (Physics2D) it pushes nothing; it just reports which things OVERLAP it and fires ENTER / EXIT as they cross its boundary. This is Godot's Area2D, and it's how nearly every game does pickups, hurt/hit boxes, checkpoints, doorways, and water / wind / gravity zones. It's pure geometry (circle & axis-aligned box overlap) plus set-diffing (this frame's members vs last frame's), so it's deterministic and unit-tests headlessly.

**Types:** `Area2D`, `AreaMonitor`

**Functions:**

- `inline math::vec2 closestOnBox(math::vec2 p, math::vec2 center, math::vec2 half)`
- `inline bool overlaps(const Area2D& a, const Area2D& b)`

### `AutoTile`
<sub>`engine/include/maz/game/AutoTile.hpp`</sub>

Procedural cave generation + tilemap autotiling — the pieces behind Godot's TileMap terrain (autotiling) sets and the classic cellular-automata cave generator. A grid stores 1 = solid (wall), 0 = open (floor). CellularCave grows organic caverns from seeded noise; autotileMask4 turns the grid into per-cell edge bitmasks so a renderer can pick the right border tile. Pure logic (only core::Random) — deterministic under a seed, so it unit-tests headlessly and a given seed always yields the same cave.

**Types:** `CellularCave`

**Functions:**

- `inline uint8_t autotileMask4(const std::vector<uint8_t>& solid, int w, int h, int x, int y)`
- `inline int autotileIndex4(uint8_t mask4)`

### `Avoidance`
<sub>`engine/include/maz/game/Avoidance.hpp`</sub>

Local collision avoidance — Godot's NavigationAgent2D avoidance (RVO). Each agent has a PREFERRED velocity (usually "toward my goal"); rvoVelocity nudges it to a nearby velocity that won't run into moving neighbours, by sampling candidate velocities and scoring each on time-to-collision plus how far it strays from the preference. It is RECIPROCAL: every agent runs the same rule and each takes half the avoidance (the velocity-obstacle is centred on the average velocity, `2·c − vA − vB`), so a pair on a head-on course peels apart smoothly instead of oscillating. Pure 2D math — deterministic, no GPU — so it unit-tests headlessly and a scene replays identically.

**Types:** `AvoidNeighbor`

**Functions:**

- `inline float timeToCollision(math::vec2 relPos, math::vec2 relVel, float r)`
- `inline math::vec2 rvoVelocity(math::vec2 pos, math::vec2 vel, math::vec2 prefVel, float radius,`

### `Ballistics`
<sub>`engine/include/maz/game/Ballistics.hpp`</sub>

maz::game ballistics — closed-form projectile-motion helpers for aiming under gravity: the math an AI (or the player's aim assist) needs to lob a grenade, arc an arrow, or range an artillery shell onto a target. Godot ships no ballistic solver, so this is a beyond-Godot gameplay utility. All functions are exact/analytic (no iteration), deterministic, header-only. Gravity is given as a positive magnitude acting along -Y.

**Types:** `LaunchSolution`

**Functions:**

- `inline LaunchSolution solveLaunchAngle(float range, float height, float speed, float gravity = 9.81f)`
- `inline math::vec3 solveLaunchVelocity(const math::vec3& from, const math::vec3& to, float gravity,`
- `inline math::vec3 projectilePosition(const math::vec3& from, const math::vec3& vel, float gravity,`
- `inline float projectileApex(float launchSpeed, float launchAngle, float gravity)`
- `inline float maxRangeFlat(float speed, float gravity = 9.81f)`

### `BarnesHut`
<sub>`engine/include/maz/game/BarnesHut.hpp`</sub>

maz::game::barnesHutAccelerations — the Barnes-Hut quadtree method for n-body force fields. Computing the gravitational (or any inverse-square attraction) force on every body from every other is naively O(n^2) — hopeless past a few thousand bodies. Barnes-Hut buckets the bodies into a quadtree, records each node's total mass and centre of mass, and when a whole cluster is far enough away (its width / distance is below a threshold theta) treats it as one lumped mass instead of visiting each body — dropping the cost to O(n log n). This is what powers galaxy/gravity toys, large-scale particle attraction, dust/debris fields, and swarm forces at a scale a per-pair loop can't reach. Godot ships no n-body solver. The approximation is tunable: theta = 0 opens every node and reproduces the EXACT all-pairs force; larger theta trades accuracy for speed. Softening avoids the singularity when two bodies coincide. Header-only, std-only, deterministic.

**Types:** `GravBody`, `BhNode`

**Functions:**

- `inline int bhBuild(std::vector<BhNode>& nodes, const std::vector<GravBody>& b, std::vector<int> idx,`
- `inline std::vector<math::vec2> barnesHutAccelerations(const std::vector<GravBody>& bodies, float theta,`

### `BehaviorTree`
<sub>`engine/include/maz/game/BehaviorTree.hpp`</sub>

Behavior trees — a scalable, reactive alternative to the finite state machine for AI decisions. A tree is ticked every frame; each node returns Success, Failure, or Running. Composites route the tick: a Sequence runs children until one is not Success (AND), a Selector until one is not Failure (fallback / priority OR). These composites are REACTIVE (memoryless): every tick re- evaluates from the first child, so a higher-priority branch (e.g. "flee") can pre-empt a running lower-priority one (e.g. "patrol") the instant its condition flips — the behaviour you want for reactive agents. Leaves wrap gameplay via std::function. Header-only, no GPU, so it unit-tests headless.

**Types:** `Node`, `Action`, `Condition`, `Sequence`, `Selector`, `Inverter`, `Blackboard`, `Parallel`, `Repeater`, `AlwaysSucceed`, `AlwaysFail`, `Tap`, `BehaviorTree`

**Functions:**

- `inline NodePtr action(std::function<Status()> fn)`
- `inline NodePtr condition(std::function<bool()> fn)`
- `inline NodePtr inverter(NodePtr child)`
- `inline NodePtr repeater(int count, NodePtr child)`
- `inline NodePtr alwaysSucceed(NodePtr child)`
- `inline NodePtr alwaysFail(NodePtr child)`
- `inline NodePtr tap(int* out, NodePtr child)`

### `BspDungeon`
<sub>`engine/include/maz/game/BspDungeon.hpp`</sub>

maz::game BSP dungeon generation — the classic roguelike "rooms and corridors" layout via binary space partitioning. The map area is recursively split into sub-regions; each leaf gets a randomly sized room, and sibling regions are joined by L-shaped corridors, so the whole dungeon is one connected space. This is the staple generator behind Rogue/NetHack-style levels and a different flavour from the organic cave generator (CellularCave): sharp rectangular rooms linked by straight halls. Deterministic for a given seed (core::Pcg32), pure integer grid math, header-only. Godot leaves procedural level generation to the game, so this is a genuinely-useful utility with testable structural guarantees (rooms in bounds, rooms non-overlapping, all floor connected).

**Types:** `Dungeon`, `BspDungeonParams`

**Functions:**

- `inline void bspCarveFloor(Dungeon& d, int x, int y)`
- `inline void bspCarveCorridor(Dungeon& d, math::Vector2i a, math::Vector2i b)`
- `inline math::Vector2i roomCenter(const math::Rect2i& r)`
- `inline math::Vector2i bspSplit(Dungeon& d, int rx, int ry, int rw, int rh, int depth,`
- `inline Dungeon generateBspDungeon(int width, int height, std::uint64_t seed,`

### `Bvh`
<sub>`engine/include/maz/game/Bvh.hpp`</sub>

maz::game::Bvh — a bounding-volume hierarchy over 3D AABBs, the acceleration structure engines build for RAY CASTING and broadphase over (mostly static) geometry. Where a quad/octree partitions space, a BVH partitions the OBJECTS: each node bounds a group of items and splits them into two child groups, so a ray or box query descends only the branches its path actually touches. This is what makes "what does this ray hit?" — bullet/line-of-sight/mouse-pick against a whole level — sublinear instead of testing every triangle-box.  build() takes id+AABB items and recursively splits by the longest axis at the centroid median. queryBox() returns every item overlapping a box; raycast() returns every item the ray enters (within tMax); raycastNearest() returns the closest hit (id + entry distance). Ray/box tests use the standard slab method and prune whole subtrees whose bounds the query misses — exact, no false negatives. Header-only, no GPU; rebuild when geometry changes.

**Types:** `Bvh`

### `CameraController2D`
<sub>`engine/include/maz/game/CameraController2D.hpp`</sub>

A 2D follow camera: the controller every 2D game needs but the engine only had the pieces for (a raw Camera2D data struct + a separate Shake). It tracks a target with three standard behaviors layered together: * Deadzone — a box around the current focus the target can move within WITHOUT scrolling the camera; only when the target leaves the box does the camera move (to put it back on the edge). Kills jitter from tiny target motion. * Smoothing — the camera eases toward its desired focus with a frame-rate-independent exponential approach (higher = snappier), so scrolling feels weighty instead of locked. * World bounds — the visible rectangle is clamped inside the level so the camera never shows past the edges; if the world is smaller than the view on an axis, that axis is centered. A shake offset can be added on top without feeding back into the follow position. This is math-only (no renderer dependency): the app reads center()/zoom() to fill a render::Camera2D. Header-only.

**Types:** `CameraController2D`

### `ChunkStreamer`
<sub>`engine/include/maz/game/ChunkStreamer.hpp`</sub>

maz::game::ChunkStreamer — chunked streaming for large tilemaps / open worlds. A world too big to hold in memory (or to draw) is cut into fixed-size square CHUNKS; only the chunks near the camera stay resident, and as the focus point moves the streamer reports which chunks to LOAD (newly in range) and which to UNLOAD (left range). That load/unload delta is exactly what a game feeds to its asset loader and renderer to page a giant map in and out without a hitch.  This is the streaming layer Godot's TileMap doesn't provide (it loads a whole map at once); the streamer is generic — chunk coordinates are integer (cx, cy) cells of `chunkSize` world units, and a chunk is resident when it lies within `radius` chunks (Chebyshev / square window, or set `circular` for a disc) of the focus chunk. update() returns the delta AND updates the resident set; deterministic, allocation-light, header-only, no GPU.

**Types:** `ChunkCoord`, `ChunkCoordHash`, `ChunkStreamer`

### `Collision`
<sub>`engine/include/maz/game/Collision.hpp`</sub>

Axis-aligned bounding box.

**Types:** `Aabb`, `RayHit`

**Functions:**

- `inline RayHit raycastAabb(const math::vec3& origin, const math::vec3& dir, const Aabb& box,`
- `inline RayHit raycast(const math::vec3& origin, const math::vec3& dir,`
- `inline math::vec3 slideMove(math::vec3 pos, const math::vec3& delta, const math::vec3& halfExtents,`

### `CollisionLayers`
<sub>`engine/include/maz/game/CollisionLayers.hpp`</sub>

Collision layers & masks — Godot's collision_layer / collision_mask. Overlap geometry (Area2D, Physics2D) answers "do these two shapes touch?"; layers answer the OTHER half every game needs: "should these two even be considered?". Each object lives in some LAYERS ("what I am": player, enemy, pickup, wall) and scans some MASK ("what I react to"). A pickup magnet scans only the pickup layer; an enemy hurtbox scans only the player layer; the player's bullets scan only enemies. Without this, every zone reacts to everything. It's pure bitmask logic — no allocation on the hot path — so it's deterministic and unit-tests headlessly. Godot 2D exposes 32 layers; LayerMask is a 32-bit set.

**Types:** `CollisionObject2D`, `LayerRegistry`

**Functions:**

- `inline constexpr LayerMask layerBit(int index)`
- `inline LayerMask layerMask(std::initializer_list<int> indices)`
- `inline bool detects(LayerMask observerMask, LayerMask targetLayer)`
- `inline bool interact(LayerMask aLayer, LayerMask aMask, LayerMask bLayer, LayerMask bMask)`

### `CombineMode`
<sub>`engine/include/maz/game/CombineMode.hpp`</sub>

How two bodies' per-body friction / restitution scalars combine into the effective pair value (Godot PhysicsMaterial / Box2D). GeometricMean = sqrt(a*b) is the physically-standard friction combine and the engine's historical default; Max is the usual restitution choice. Shared by the 2D and 3D physics solvers so the same material semantics apply in both.

**Functions:**

- `inline float combineValue(CombineMode mode, float a, float b)`

### `ComboMeter`
<sub>`engine/include/maz/game/ComboMeter.hpp`</sub>

maz::game combo meter — the score-chain tracker behind arcade multipliers, fighting-game combo counters, and rhythm-game streaks. Each `hit` bumps the combo count and refreshes a countdown; if the countdown runs out (or the game calls `breakCombo` on a miss), the chain resets to zero. The current count maps through configurable tiers to a score MULTIPLIER, and `scoreFor` applies it to a base point value. The best combo reached is remembered for an end-of-run "max combo" stat. Purely time-driven, no rendering. Godot ships no combo/score-chain system — games hand-roll it every time — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `ComboTier`, `ComboMeter`

### `ConvexHull3D`
<sub>`engine/include/maz/game/ConvexHull3D.hpp`</sub>

maz::game 3D convex hull — the incremental hull of a point cloud, the geometry behind Godot's ConvexPolygonShape3D (create a tight convex collider from an arbitrary mesh's vertices) and useful for bounding volumes and simplified proxies. Given points, buildConvexHull returns the outward- facing triangular faces of their convex hull plus the subset of points that lie on it (interior points are dropped). The algorithm seeds a non-degenerate tetrahedron, then for each remaining point removes the faces it can "see" and stitches new faces from the horizon edges to it — every face is oriented outward against a fixed interior point, so the result is a watertight, convex, correctly-wound mesh. Pure math (no renderer/physics), deterministic, unit-tested; returns valid=false for fewer than 4 points or a degenerate (collinear/coplanar) set.

**Types:** `HullFace`, `ConvexHull3D`

**Functions:**

- `inline float hullDot(const math::vec3& a, const math::vec3& b)`
- `inline math::vec3 hullCross(const math::vec3& a, const math::vec3& b)`
- `inline float hullLen(const math::vec3& a)`
- `inline ConvexHull3D buildConvexHull(const std::vector<math::vec3>& pts, float eps = 1e-5f)`

### `ConvexShape2D`
<sub>`engine/include/maz/game/ConvexShape2D.hpp`</sub>

Arbitrary 2D CONVEX POLYGON collision via the Separating-Axis Theorem (SAT) — Godot's ConvexPolygonShape2D / CollisionPolygon2D. Physics2D already collides circles and (oriented) boxes, and it uses SAT internally for the box-box case, but there was no way to collide an ARBITRARY convex shape: a triangle, a pentagon, a hexagonal bumper, a hand-authored hull. This adds a standalone convex-polygon type plus the two queries every 2D game wants against it — "do these two shapes overlap, and if so which way and how far do I push to separate them (the minimum translation vector)?" and "is this point inside this shape?" — as pure geometry with no simulation or renderer dependency, so it unit-tests headlessly.  SAT: two convex shapes are disjoint iff there exists a separating axis — a line onto which their projections don't overlap. The candidate axes are the face normals of both polygons. If every axis shows overlap, the shapes intersect, and the axis of MINIMUM overlap gives the MTV (push direction + penetration depth). Only valid for CONVEX polygons (decompose concave shapes into convex pieces first).

**Types:** `ConvexPoly2D`, `SatHit2D`

**Functions:**

- `inline void projectPoly(const ConvexPoly2D& p, const math::vec2& axis, float& mn, float& mx)`
- `inline math::vec2 polyCenter(const ConvexPoly2D& p)`
- `inline bool testAxes(const ConvexPoly2D& src, const ConvexPoly2D& a, const ConvexPoly2D& b,`
- `inline SatHit2D satOverlap(const ConvexPoly2D& a, const ConvexPoly2D& b)`
- `inline bool polyContains(const ConvexPoly2D& p, const math::vec2& pt)`
- `inline ConvexPoly2D makeRegularPoly(const math::vec2& center, float radius, int sides, float rotation = 0.0f)`
- `inline ConvexPoly2D makeBoxPoly(const math::vec2& center, const math::vec2& half, float angle = 0.0f)`

### `Cooldown`
<sub>`engine/include/maz/game/Cooldown.hpp`</sub>

maz::game ability cooldown manager — the timer bank behind spell cooldowns, dashes, the global cooldown, and any "you can't do that yet" gate. Abilities are keyed by an integer id; `tryUse` fires an ability and puts it on cooldown in one call (returning false if it is still recharging), `tick` advances every active timer by the frame delta, and `isReady` / `remaining` / `fraction` drive the greyed-out button and the radial sweep. Cooldowns that reach zero are dropped, so an idle manager holds nothing. Godot ships Timer nodes but no ability-cooldown abstraction — games wire this up by hand every time — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `CooldownManager`

### `Crafting`
<sub>`engine/include/maz/game/Crafting.hpp`</sub>

maz::game crafting system — turns a bench of recipes plus an Inventory into "combine these items to make that item." A Recipe lists the input item ids and quantities it consumes and the output id and quantity it produces; `canCraft` checks an inventory has every ingredient and room for the result, and `craft` atomically consumes the inputs and adds the output (leaving the inventory untouched on failure). A RecipeBook stores many recipes and can report which ones a given inventory can currently make — the data behind a crafting menu that greys out what you can't yet build. Built directly on maz::game::Inventory (ItemStack ids and quantities), so crafting and carrying share one item model. Godot ships no crafting system — games hand-roll it every time — so this is a beyond-Godot gameplay utility extending the RPG suite. Header-only, std-only, deterministic.  Room policy: `canCraft` requires the output to fit in the inventory's CURRENT free space (partial output stacks + empty slots). It does not speculatively count slots that consuming the inputs would free, so a completely full inventory must have room for the result before crafting — a deliberate simplification that keeps `canCraft` and `craft` perfectly consistent.

**Types:** `Recipe`, `RecipeBook`

**Functions:**

- `inline bool canCraft(const Recipe& r, const Inventory& inv)`
- `inline bool craft(const Recipe& r, Inventory& inv)`

### `Csg`
<sub>`engine/include/maz/game/Csg.hpp`</sub>

maz::game constructive solid geometry — Godot's CSG nodes (CSGCombiner3D union / intersection / subtraction of CSGBox/Sphere/etc). This implements the boolean *math* with signed distance fields: each solid is a function giving the signed distance from a point to its surface (negative inside, positive outside, zero on it), and the booleans are exact closed-form combinations of those distances — union = min, intersection = max, subtraction = max(a, -b) — plus rounded ("smooth") variants for filleted joins. From an SDF you can test containment, estimate the surface normal, and (via a mesher such as marching cubes) extract a triangle mesh. Pure math, header-only, no GPU — exactly unit-testable by sampling distances. Triangle-mesh boolean output (Godot's actual CSG mesh) is a separate mesher on top; this is the boolean core it would sample.

**Functions:**

- `inline Sdf sdSphere(const math::vec3& c, float r)`
- `inline Sdf sdBox(const math::vec3& c, const math::vec3& he)`
- `inline Sdf sdRoundBox(const math::vec3& c, const math::vec3& he, float r)`
- `inline Sdf sdPlane(const math::vec3& n, float d)`
- `inline Sdf sdCylinder(const math::vec3& base, const math::vec3& axis, float r)`
- `inline Sdf opUnion(Sdf a, Sdf b)`
- `inline Sdf opIntersect(Sdf a, Sdf b)`
- `inline Sdf opSubtract(Sdf a, Sdf b)`
- `inline Sdf opSmoothUnion(Sdf a, Sdf b, float k)`
- `inline Sdf opSmoothIntersect(Sdf a, Sdf b, float k)`
- `inline Sdf opSmoothSubtract(Sdf a, Sdf b, float k)`
- `inline Sdf opTranslate(Sdf s, const math::vec3& t)`
- _…and 3 more_

### `Damage`
<sub>`engine/include/maz/game/Damage.hpp`</sub>

maz::game combat damage resolver — the "how much damage actually lands" math behind every attack. Given a raw hit amount and the defender's mitigation (scaling armor, a fractional resistance to the damage type, and optional flat reduction) plus an optional critical-hit multiplier, `resolveDamage` returns the final integer damage and whether the blow crit or was fully blocked. Armor uses the classic diminishing-returns curve `100 / (100 + armor)` (100 armor halves damage, 200 armor thirds it — never negative), resistance scales by `(1 - resist)`, and a `True` damage type bypasses all mitigation. Crit is a caller-supplied flag (roll it with the engine's Pcg32), so the resolver stays pure and deterministic. This is combat math Godot has no notion of — it ships no damage/combat system — so this is a beyond-Godot gameplay utility that pairs with the stat and status-effect systems. Header-only, std-only, deterministic.

**Types:** `DamageType`, `DamageInfo`, `DamageResult`

**Functions:**

- `inline double armorMultiplier(double armor)`
- `inline DamageResult resolveDamage(const DamageInfo& in)`

### `DayNightCycle`
<sub>`engine/include/maz/game/DayNightCycle.hpp`</sub>

maz::game day/night cycle — a looping time-of-day clock for open-world lighting, shop hours, and spawn schedules. One in-game day spans `dayLength` real seconds; `update(dt)` advances the clock, wraps it at midnight, and ticks a day counter. It reports the normalized time-of-day [0,1), the in-game hour [0,24), a DayPhase (Night / Dawn / Day / Dusk) from configurable thresholds, and a sun elevation in [-1,1] (-1 at midnight, 0 at sunrise/sunset, +1 at noon) the renderer can feed straight into a directional light. Godot ships no day/night system — games hand-roll it every time — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `DayNightCycle`

### `Dialogue`
<sub>`engine/include/maz/game/Dialogue.hpp`</sub>

maz::game branching dialogue system — the conversation graph behind NPC talk, cutscene lines, and "[1] Accept / [2] Decline" prompts. A DialogueTree is a set of nodes, each with a speaker, a line of text, and either a list of player CHOICES (each pointing to the next node) or a single auto-advance `next` link for a straight run of lines. A DialogueRunner walks the tree: it exposes the current line and its choices, `choose(i)` follows a branch, and `advance()` steps a choice-less line forward, ending the conversation when a link points to -1. The tree is pure data (one tree can drive many NPCs at once via separate runners). Godot ships no dialogue system — games hand-roll it every time or reach for a third-party addon — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `DialogueChoice`, `DialogueNode`, `DialogueTree`, `DialogueRunner`

### `DiamondSquare`
<sub>`engine/include/maz/game/DiamondSquare.hpp`</sub>

maz::game::DiamondSquare — the diamond-square (midpoint-displacement) fractal heightmap generator. It produces a (2^n + 1) square grid of heights by seeding the four corners and repeatedly subdividing: each "diamond" step sets a square's centre to the average of its four corners plus a random offset, each "square" step sets an edge midpoint to the average of its orthogonal neighbours plus a random offset, and the random amplitude shrinks each level (roughness). The result is self-similar fractal terrain with a characteristic ridged/plasma look — distinct from the engine's Perlin/fbm noise (core::Noise): where value/gradient noise is band-limited and smooth, diamond-square is a recursive random-midpoint fractal, the classic generator for island heightmaps, cloud/plasma textures, and lightning. Deterministic given a seed (embedded splitmix64 — no <random>, no clock), header-only, std-only. Godot ships no such generator.

**Types:** `DiamondSquare`

### `Dice`
<sub>`engine/include/maz/game/Dice.hpp`</sub>

maz::game DICE NOTATION — parse and roll the classic tabletop dice strings ("2d6+3", "d20", "4d8-1") that RPGs, board-game ports, and loot/damage tables are written in. `parseDice` turns the text into a `DiceSpec` (dice count, sides, flat modifier); `rollDice` rolls it with any engine RNG that offers `range(lo,hi)` (e.g. `core::Pcg32`), returning the total and the individual dice; and `minRoll`/`maxRoll`/`averageRoll` give the distribution bounds without rolling (for tooltips, balancing, and AI expected-value decisions). Godot ships no dice parser, so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic (the randomness lives in the caller's seeded RNG).

**Types:** `DiceSpec`, `RollResult`

**Functions:**

- `inline int keptCount(const DiceSpec& d)`
- `inline bool readDiceInt(const std::string& s, std::size_t& i, std::size_t end, long& out)`
- `inline DiceSpec parseDice(const std::string& str)`
- `inline int minRoll(const DiceSpec& d)`
- `inline int maxRoll(const DiceSpec& d)`
- `inline double averageRoll(const DiceSpec& d)`
- `inline RollResult rollDice(const DiceSpec& d, Rng& rng)`
- `inline RollResult rollDice(const std::string& expr, Rng& rng, DiceSpec* out = nullptr)`

### `DijkstraMap`
<sub>`engine/include/maz/game/DijkstraMap.hpp`</sub>

maz::game Dijkstra map — the Brogue-style scalar "desire map" that drives roguelike monster AI. Multi-source breadth-first search fills every passable cell with its step-distance to the NEAREST of one or more goals (walls impassable); a monster then just walks to the lowest-valued neighbour to approach (`descend`). Negating and re-scanning the map (`makeFleeMap`) turns "walk toward the hero" into "walk away from the hero", and several maps can be combined for richer behaviour (safety, desire, patrol). This complements the vector-field FlowField (M112) — that bakes a single-goal float integration field into per-cell direction vectors for crowds; this is a multi-source INTEGER distance field the game reads and recombines directly, the shape roguelikes actually use. Unit-cost BFS (4- or 8-connected), deterministic, header-only. Godot leaves this to the game.

**Types:** `DijkstraMap`

**Functions:**

- `inline DijkstraMap buildDijkstraMap(int width, int height, const std::vector<MapCell>& goals,`
- `inline DijkstraMap makeFleeMap(const DijkstraMap& approach,`

### `Elo`
<sub>`engine/include/maz/game/Elo.hpp`</sub>

maz::game Elo rating — the ranking + matchmaking math behind ranked ladders, leaderboards, and difficulty matching (Arpad Elo's system, as used by chess and virtually every competitive game).  Each competitor carries a single number (the rating). Before a match, the ratings predict the probability each side wins; after it, both ratings move by an amount that depends on how SURPRISING the result was — beating a much stronger opponent gains a lot, beating a much weaker one gains almost nothing, and the exchange is zero-sum (the winner gains exactly what the loser drops). The K-factor controls volatility: large K (new/provisional players) moves ratings fast, small K (established players) keeps them stable. Use it for ranked matchmaking, seeding brackets, or scaling AI difficulty to a player's measured skill. Pure value math, header-only, deterministic — unit-tested against the exact Elo formulas and the conservation (zero-sum) property.

**Types:** `EloPair`

**Functions:**

- `inline float eloExpectedScore(float ratingA, float ratingB)`
- `inline float eloUpdate(float rating, float opponentRating, float actualScore, float k = 32.0f)`
- `inline float eloDelta(float rating, float opponentRating, float actualScore, float k = 32.0f)`
- `inline EloPair eloPlay(float ratingA, float ratingB, float scoreA, float k = 32.0f)`
- `inline float eloKFactor(int gamesPlayed, float provisionalK = 40.0f, float establishedK = 20.0f,`

### `Erosion`
<sub>`engine/include/maz/game/Erosion.hpp`</sub>

maz::game thermal erosion — weather a procedural heightmap so it looks geologically aged instead of raw fractal noise. Terrain straight out of Perlin/fbm or diamond-square has implausibly steep, jagged slopes; real hillsides can't hold material past a "talus angle" — anything steeper crumbles and the debris slides downhill until the slope relaxes. Thermal erosion simulates exactly that: wherever an adjacent height difference exceeds the talus threshold, a fraction of the excess material is moved down to the lower neighbours, spread in proportion to how much lower each is. Run for a number of passes it softens ridges, fills gullies, and forms natural scree slopes at the talus angle. It is the cheap, stable half of terrain weathering (the other being water/hydraulic erosion) and a staple of procedural landscape tools. Crucially it only MOVES material between cells — never creates or destroys it — so total volume is conserved exactly. Uses a Jacobi (double-buffered) update so the result is order-independent and deterministic. Header-only, std-only. Godot has no terrain erosion.

**Functions:**

- `inline void thermalErosion(std::vector<float>& height, int width, int height_, float talus,`

### `FieldOfView`
<sub>`engine/include/maz/game/FieldOfView.hpp`</sub>

maz::game field of view — recursive shadowcasting, the standard roguelike "what can this tile see?" algorithm. Given an origin, a sight radius, and an opacity predicate (which tiles block vision), it returns every visible tile: the lit region of a torch, a guard's sight (occluded by walls), or the fog-of-war reveal as a unit moves. Unlike point-to-point line-of-sight (GridLine), this computes the WHOLE visible set in one sweep. It processes the eight octants around the origin, tracking the shadow each opaque tile casts as a slope range and recursing into the still-visible sub-ranges, so cost is proportional to the visible area, not the square of the radius. Opaque tiles are themselves visible (you see the wall you're up against), but tiles behind them are not. Pure integer grid math on a caller-supplied predicate, deterministic, header-only. Godot leaves FOV to the game, so this is a genuinely-useful utility with a testable output.

**Functions:**

- `inline void castLight(int cx, int cy, int radius, int row, float startSlope, float endSlope, int xx,`
- `inline std::vector<FovCell> computeFov(FovCell origin, int radius,`

### `FillDepressions`
<sub>`engine/include/maz/game/FillDepressions.hpp`</sub>

maz::game::fillDepressions — priority-flood depression filling for heightfields (Barnes, Lehman & Mulla 2014). Terrain from noise or erosion is riddled with pits and closed basins where water would pool and get stuck with nowhere to drain. Before you can trace rivers, compute drainage / flow accumulation, place lakes, or run a hydraulic-erosion pass, you first "fill" every depression up to the lowest lip over which water could spill — turning the surface into one where every point has a downhill (non-ascending) path to the map edge. This is the standard hydrology preprocessing step (ArcGIS "Fill", GRASS r.fill.dir); Godot has none. The algorithm floods inward from the boundary using a min-priority queue keyed by spill height, so each interior cell is raised to the maximum of its own height and the lowest level needed to reach the edge. Output >= input everywhere, the boundary is untouched, and no interior pit remains. Header-only, std-only, deterministic (the filled surface is unique regardless of tie order).

**Functions:**

- `inline std::vector<float> fillDepressions(int width, int height, const std::vector<float>& elev)`

### `FloodFill`
<sub>`engine/include/maz/game/FloodFill.hpp`</sub>

maz::game flood fill & connected regions — the grid "paint bucket" and its region-labelling companion. Flood fill collects every cell reachable from a seed through passable cells (BFS, 4- or 8-connected): the paint-bucket tool, "which tiles can the player actually reach from here", spill/water fill, and enclosed-area detection. Connected regions partitions ALL passable cells of a grid into separate components — counting rooms/islands, finding the biggest cavern of a procedural map, or pruning unreachable pockets. Pure integer grid math on a caller-supplied passability predicate, deterministic (scan/BFS order), header-only. Godot leaves this to the game, so it is a genuinely-useful utility with an exact, testable output.

**Functions:**

- `inline void floodBfs(int width, int height, FillCell start,`
- `inline std::vector<FillCell> floodFill(int width, int height, FillCell start,`
- `inline std::vector<std::vector<FillCell>> connectedRegions(`

### `FlowAccumulation`
<sub>`engine/include/maz/game/FlowAccumulation.hpp`</sub>

maz::game D8 flow accumulation — figure out where water DRAINS on a heightmap: for each cell, how many cells upstream ultimately flow through it. This is the standard hydrology primitive that turns a terrain into rivers: high accumulation traces out valleys, streams, and river mouths, which drives procedural river placement, moisture/biome maps (wetter downstream), where hydraulic erosion cuts channels, and where to spawn lakes or settlements. The "D8" model routes each cell's water to its single STEEPEST-DESCENT neighbour among the 8 around it (or nowhere, if it is a local pit / map-edge outlet); accumulation is then the count of cells whose drainage passes through each cell (itself included). Because every cell's unit of water flows monotonically downhill until it leaves the map or reaches a pit, the total water is conserved — the accumulations at all the outlet cells sum to the cell count. Computed in one height-sorted pass, no recursion. Header-only, std-only, deterministic. Godot has no hydrology tools.

**Types:** `FlowResult`

**Functions:**

- `inline FlowResult flowAccumulation(const std::vector<float>& height, int width, int height_)`

### `FlowField`
<sub>`engine/include/maz/game/FlowField.hpp`</sub>

Flow-field (vector-field) pathfinding — the crowd-movement technique the per-agent A* of Godot's NavigationServer (and Maz's own NavGrid, M57) doesn't provide. When MANY agents share ONE goal you don't path each of them: you run a single Dijkstra OUTWARD from the goal to get an INTEGRATION FIELD (least cost-to-goal for every cell), then bake a FLOW FIELD — each cell stores a unit direction pointing down that cost gradient toward the goal. Every agent then navigates for free: it just reads the direction under its feet and walks. So a thousand units route around walls to the goal for the price of one search, and the paths update in one pass when the goal moves. Pure grid math (8-connected Dijkstra, diagonal cost sqrt(2), no corner cutting), deterministic and GPU-free, so it unit-tests headlessly.

**Types:** `FlowField`

### `FlyCamera`
<sub>`engine/include/maz/game/FlyCamera.hpp`</sub>

A first-person "fly" camera: a position plus yaw/pitch, driven by move()/look(), producing a view matrix. Engine-agnostic (math only) — the app maps its input to the move/look axes.

**Types:** `FlyCamera`

### `FogOfWar`
<sub>`engine/include/maz/game/FogOfWar.hpp`</sub>

maz::game::FogOfWar — the persistent "what has this player seen?" memory for a tile map, the staple of RTS, strategy, and roguelike games. Every tile is in one of three states: Unseen (never revealed — drawn black), Explored (seen before but not currently in view — drawn dimmed, from memory), or Visible (in view right now — drawn fully lit, and where enemies actually show). This is DISTINCT from FieldOfView (M?, which computes the set of tiles a unit can see this instant); fog of war is the layer that REMEMBERS: it merges the current sight with the history so the map fills in permanently as you explore, while live vision comes and goes. The per-frame cycle is: beginFrame() demotes last frame's Visible tiles back to Explored, then you reveal() every tile currently in sight (or revealCircle() around each unit) — leaving unrevealed tiles as they were. Godot ships no fog-of-war primitive. Header-only, std-only, deterministic.

**Types:** `FogOfWar`

### `Formation`
<sub>`engine/include/maz/game/Formation.hpp`</sub>

maz::game squad formations — arrange a group of units into a recognisable shape around an anchor (the leader or a target point), oriented to a facing direction. This is the geometry every RTS squad, party of followers, tactical fireteam, or escort mission needs: give it a count and a shape and it hands back the slot each unit should move toward (feed those to Steering/arrive or a pathfinder). `formationSlots` returns LOCAL offsets with forward = +Y and right = +X (anchor at the origin); `formationPositions` rotates those by a facing vector and translates them to the anchor, so the whole formation turns as the leader turns. Shapes: Line (abreast), Column (single file behind), Wedge (arrowhead V), Box (a centred grid), Circle (a defensive ring). Godot ships no formation helper. Header-only, pure, deterministic.

**Functions:**

- `inline std::vector<math::vec2> formationSlots(FormationShape shape, int count, float spacing)`
- `inline std::vector<math::vec2> formationPositions(const math::vec2& anchor, const math::vec2& facing,`

### `Goap`
<sub>`engine/include/maz/game/Goap.hpp`</sub>

Goal-Oriented Action Planning — a step beyond the behaviour tree. Instead of an author hand-wiring what an agent does, the agent is given a GOAL (a desired world-state) and a LIBRARY of actions, each with preconditions and effects, and it PLANS backward-optimally the cheapest sequence of actions that carries the current world from where it is to the goal. This is the classic F.E.A.R. AI technique; it gives emergent, re-plannable behaviour (drop in a new action and every agent can use it, no tree edits) that Godot ships no built-in equivalent for. Pure integer/graph search — deterministic, headless- testable, and it replays identically every run.  The world is a set of boolean facts packed into a 64-bit word (bit i = fact i is true). A Condition is a PARTIAL state: `mask` marks which facts it constrains and `want` their required truth, so a goal or a precondition can care about three facts and ignore the other sixty-one. The planner is A* over world states: g = accumulated action cost, h = an admissible lower bound (you still need at least one more action, costing at least the cheapest action, whenever the goal is unmet), so the first plan it pops is guaranteed minimum-cost.

**Types:** `Condition`, `Action`, `Plan`

**Functions:**

- `inline constexpr State bit(int i)`
- `inline bool satisfied(State s, const Condition& c)`
- `inline State apply(State s, const Action& a)`
- `inline Plan plan(State start, const Condition& goal, const std::vector<Action>& library,`

### `GravityField2D`
<sub>`engine/include/maz/game/GravityField2D.hpp`</sub>

Area gravity fields — Godot's Area2D gravity override. A rectangular zone can change the gravity a body feels while inside it: a DIRECTIONAL field pushes a fixed way (wind, an updraft, a sideways conveyor of force), a POINT field pulls toward (or pushes from) a centre with inverse-square falloff (a planet, a black hole, a magnet). Zones carry a PRIORITY and a mode — REPLACE (this zone's gravity overrides what lower zones set, like Godot's "Replace") or ADD (accumulate on top, like Godot's "Combine"). `gravityAt` walks the zones low-priority-first and returns the final gravity vector at a point, starting from the world's default. Pure geometry + vector math, header-only, deterministic — it unit-tests exactly and a fixed-step sim drives a golden.

**Types:** `GravityArea2D`

**Functions:**

- `inline math::vec2 zoneGravity(const GravityArea2D& a, math::vec2 p)`
- `inline math::vec2 gravityAt(const std::vector<GravityArea2D>& areas, math::vec2 p, math::vec2 base)`

### `GridLine`
<sub>`engine/include/maz/game/GridLine.hpp`</sub>

maz::game grid-line utilities — the integer-grid line rasteriser (Bresenham) and the tile line-of-sight test built on it. These are the staples of tile/grid games: draw a straight line of tiles (lasers, roads, trajectory previews, tile-brush strokes), and answer "can A see B?" across a tilemap where some cells block vision (roguelike FOV checks, guard sight cones, cover tests). Bresenham steps one cell at a time choosing the axis with the larger delta, so the returned path is 8-connected and symmetric-ish; line-of-sight walks that same path and reports whether any cell strictly between the endpoints blocks it. Pure integer math, deterministic, header-only — Godot has no built-in grid line/LOS helper (TileMap leaves this to the game), so this is a genuinely-useful utility with an exact, testable output.

**Functions:**

- `inline std::vector<LineCell> bresenhamLine(const LineCell& a, const LineCell& b)`
- `inline bool lineOfSight(const LineCell& a, const LineCell& b,`

### `GridMap`
<sub>`engine/include/maz/game/GridMap.hpp`</sub>

maz::game GridMap — the data model behind Godot's GridMap node: a sparse 3D grid of cells, each holding a tile/mesh-library id plus one of the 24 orthogonal orientations. Only occupied cells are stored (an unordered_map keyed by packed integer coordinates), so a vast world costs only what is actually placed. This is the [CPU] half of GridMap — authoring, queries, bounds, and world<->cell mapping — everything a game or editor needs to build and reason about a voxel/tile level; the [GPU] half (instancing each cell's mesh from a MeshLibrary) layers on top and is deferred honestly until there's a display. Pure, header-only, deterministic, unit-tested.

**Types:** `GridCell`, `GridItem`, `GridMap`

### `GridRaycast`
<sub>`engine/include/maz/game/GridRaycast.hpp`</sub>

maz::game grid ray traversal — the Amanatides & Woo "fast voxel traversal" DDA in 2D. Unlike the integer Bresenham line (GridLine), this marches a CONTINUOUS float-coordinate ray through the unit grid, visiting every cell the ray actually passes through in the exact order it crosses them. That sub-cell precision is what tile raycasters (Wolfenstein-style walls), light/sound propagation, projectile sweeps, and precise "which tile does this shot hit first" queries need. The world is a unit grid: cell (i, j) covers [i, i+1) x [j, j+1); a point maps to cell (floor x, floor y). Pure float+integer math, deterministic, header-only. Godot leaves grid raycasting to the game.

**Functions:**

- `inline std::vector<RayCell> traverseGrid(const math::vec2& origin, const math::vec2& dir,`
- `inline std::optional<RayCell> raycastGrid(const math::vec2& origin, const math::vec2& dir,`

### `Health`
<sub>`engine/include/maz/game/Health.hpp`</sub>

maz::game health component — the hit-point pool behind health bars, death, and "you can't hurt me right now" invulnerability frames. It holds current / max HP, applies `takeDamage` and `heal` (both clamped, so HP never goes below 0 or above max), and reports the fraction for a health bar plus `isDead` / `isFull`. After a hit it can grant a window of invulnerability (the classic post-damage i-frames) during which further damage is ignored, and `update(dt)` ticks that window down and applies optional passive regeneration. Pairs directly with the M486 damage resolver (feed its result into `takeDamage`). Godot ships no health system — games hand-roll it every time — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `Health`

### `HeightField3D`
<sub>`engine/include/maz/game/HeightField3D.hpp`</sub>

maz::game height-field collider — Godot's HeightMapShape3D: terrain stored as a regular grid of height samples rather than a full mesh, so a large landscape costs one float per cell. The core queries are heightAt/normalAt (the surface under a world XZ position, bilinearly interpolated — what a character controller, object placement, or foot IK needs) and a general raycast that marches the ray across the grid (2D DDA) testing each cell's two triangles with Möller–Trumbore. Local space: the grid's corner is the origin, X spans (cols-1)*cellX, Z spans (rows-1)*cellZ, Y is the stored height. Pure math (no renderer/physics), deterministic, header-only, unit-tested.

**Types:** `HeightField3D`

### `HexGrid`
<sub>`engine/include/maz/game/HexGrid.hpp`</sub>

maz::game hex-grid math — axial-coordinate hexagon utilities (the redblobgames conventions), the backbone of hex strategy/board games and Godot's TileMap hexagon layout. A Hex is an axial (q, r) coordinate; helpers convert to/from pixel space for both pointy-top and flat-top orientations, measure hex distance, list the six neighbours, round a fractional hex to the nearest cell, and walk a straight line of hexes. Pure integer/float value math — header-only, deterministic, exactly unit-testable. Godot's TileMap only offers square/isometric/hex *rendering*; this is the coordinate algebra games actually compute with, so it is parity-plus.

**Types:** `Hex`

**Functions:**

- `inline int hexDistance(const Hex& a, const Hex& b)`
- `inline const Hex* hexDirections()`
- `inline std::vector<Hex> hexNeighbors(const Hex& h)`
- `inline Hex hexRound(float qf, float rf)`
- `inline math::vec2 hexToPixel(const Hex& h, float size, HexOrientation o = HexOrientation::PointyTop)`
- `inline Hex pixelToHex(const math::vec2& p, float size, HexOrientation o = HexOrientation::PointyTop)`
- `inline Hex hexScale(const Hex& h, int k)`
- `inline std::vector<Hex> hexRing(const Hex& center, int radius)`
- `inline std::vector<Hex> hexRange(const Hex& center, int radius)`
- `inline std::vector<Hex> hexSpiral(const Hex& center, int radius)`
- `inline std::vector<Hex> hexLine(const Hex& a, const Hex& b)`

### `HexPath`
<sub>`engine/include/maz/game/HexPath.hpp`</sub>

maz::game hex A* pathfinding — shortest path across a hexagonal grid, built on HexGrid's axial coordinates. You supply a `blocked(hex)` predicate (walls / impassable terrain); the search walks the six neighbours with a hex-distance heuristic (admissible, so the result is a genuine shortest path) and returns the cell list from start to goal inclusive, or empty if unreachable. Godot's AStar2D makes you register every node and edge by hand; this is the ready-made hex pathfinder on top of the M280 coordinate algebra. Header-only, deterministic, exactly unit-testable.

**Types:** `HexHash`

**Functions:**

- `inline std::vector<Hex> hexFindPath(const Hex& start, const Hex& goal,`

### `InfluenceMap`
<sub>`engine/include/maz/game/InfluenceMap.hpp`</sub>

maz::game::InfluenceMap — a tactical-AI grid where "influence" spreads outward from sources and decays, the standard tool for spatial reasoning in strategy and shooter AI. Drop POSITIVE influence at your own units and NEGATIVE at enemies, then propagate: each cell blends toward its neighbours' average and loses a fraction each step, so influence bleeds across the map and fades with distance. Reading the result answers questions no single query can: where is it SAFE vs DANGEROUS (sign and magnitude), where is the FRONT LINE (near-zero contour between opposing armies), and which way should a unit FLEE or ADVANCE (the gradient points toward higher influence). This is distinct from pathfinding (a route), flow fields (a movement vector field toward one goal), and noise (unstructured): an influence map is a decaying diffusion of gameplay meaning. Deterministic, header-only, std-only. Godot ships no influence map.

**Types:** `InfluenceMap`

### `InterceptAim`
<sub>`engine/include/maz/game/InterceptAim.hpp`</sub>

maz::game INTERCEPT AIM — first-order "lead the target" solver: where should a turret, archer, or homing AI aim so a projectile fired at a FIXED speed hits a target that is moving at constant velocity? This is the gravity-free companion to `Ballistics.hpp` (which arcs a shot under gravity) — the math a tower-defense tower, a spaceship gun, or a guard's thrown rock needs to hit a mover. Closed-form (one quadratic), exact, deterministic, header-only. Works in 3D; for a 2D game leave z = 0.

**Types:** `InterceptSolution`

**Functions:**

- `inline float smallestPositiveRoot(float a, float b, float c)`
- `inline InterceptSolution solveIntercept(const math::vec3& shooter, const math::vec3& target,`

### `Inventory`
<sub>`engine/include/maz/game/Inventory.hpp`</sub>

maz::game slot-based stackable inventory — the backbone of loot bags, chests, hotbars, and shop stock. An Inventory is a fixed array of slots; each slot is either empty or holds a stack of one item type up to a per-inventory stack limit (Minecraft's 64, an RPG's 99, etc.). Items are referred to by an integer id (index into the game's own item database). Adding items fills existing partial stacks of the same id first, then spills into empty slots, so a scattered inventory naturally consolidates; anything that does not fit is reported back as leftover. Removing pulls from every stack of that id until the requested amount is satisfied. Godot ships no inventory system — every game hand-rolls one — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `ItemStack`, `Inventory`

### `IsoGrid`
<sub>`engine/include/maz/game/IsoGrid.hpp`</sub>

maz::game isometric tile-grid math — the diamond ("2:1") coordinate conversions behind Godot's TileMap Isometric layout and countless iso RPG/strategy games. A cell (col, row) maps to the pixel centre of its diamond and back; screenToTile rounds a pixel position to the cell under it. Tile width/height are the full diamond footprint in pixels (Godot's tile_size). Pure value math over Vector2i / vec2 — header-only, deterministic, exactly unit-testable (round-trip verified).  Convention (matches Godot's default DIAMOND_DOWN): +col goes down-right on screen, +row down-left, with the map origin's diamond centred at the screen origin.

**Types:** `IsoGrid`

### `JumpAssist`
<sub>`engine/include/maz/game/JumpAssist.hpp`</sub>

maz::game jump-assist — the two small timers that make a platformer feel responsive instead of stiff: COYOTE TIME (you can still jump for a brief moment after walking off a ledge) and JUMP BUFFERING (a jump pressed just before landing fires the instant you touch the ground). Feed it the frame delta and whether the character is grounded via `update`, record button presses with `pressJump`, and each frame call `tryJump` — it returns true (consuming the buffered press) exactly when a jump should start: there is a live buffered press AND the character is grounded or still inside the coyote window. Both windows are configurable. This forgiving-input tuning is something Godot leaves entirely to the game -> beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `JumpAssist`

### `JumpPointSearch`
<sub>`engine/include/maz/game/JumpPointSearch.hpp`</sub>

maz::game JumpPointSearch — Jump Point Search (Harabor & Grastien, 2011): an optimisation of A* for UNIFORM-COST 8-connected grids that returns the exact same optimal path as plain grid A* while expanding dramatically fewer nodes. Ordinary A* on an open grid wastes almost all its work exploring the countless equivalent zig-zag routes between two points (path "symmetries"). JPS eliminates that by "jumping" in a straight line along each direction, skipping over every cell that couldn't possibly be a turning point, and only ever placing a handful of genuine decision cells (jump points) on the open list. On a wide-open map JPS is routinely 10-30x faster than the same A* — the difference between a smooth RTS with hundreds of pathing units and a stuttering one. Maz already ships AStarGrid2D (Dijkstra-grade grid A*); this is its high-performance sibling for the common case of a uniform grid where every step costs 1 (orthogonal) or sqrt(2) (diagonal). Godot has no JPS at all.  Movement model: 8-connected, straight cost 1, diagonal cost sqrt(2), corner-cutting ALLOWED (a diagonal step is legal whenever its target cell is free — matching AStarGrid2D's DiagonalMode::Always). That equivalence is exactly what makes the result checkable: JPS here yields the same path cost as AStarGrid2D(Always) on any grid. Header-only, pure, deterministic.

**Types:** `JumpPointSearch`

### `KinematicBody2D`
<sub>`engine/include/maz/game/KinematicBody2D.hpp`</sub>

2D kinematic character controller — Godot's CharacterBody2D.move_and_slide, the single most-used movement primitive for platformers and top-down games. Rigid bodies (Physics2D) are driven by forces; a *kinematic* character is driven directly by a velocity and must not tunnel through or stick into the static world. moveAndSlide sweeps an axis-aligned body along its motion, stops at the first contact, and SLIDES the leftover motion along the surface — repeating for a few iterations so a body can round a corner or run along a wall in one call. Each contact is classified against an `up` direction into floor / wall / ceiling (via a max floor angle), so gameplay can ask is_on_floor()/is_on_wall(). Pure geometry over a list of static AABBs — no allocation beyond the solids the caller owns — so it unit-tests exactly and drives a deterministic golden. (Discrete resolution against oriented/rotated shapes and moving platforms remain future work; this is AABB-vs-AABB swept collision.)

**Types:** `Aabb2`, `SweptHit`, `SlideResult`

**Functions:**

- `inline SweptHit sweptAabb(math::vec2 pos, math::vec2 half, math::vec2 motion, const Aabb2& solid)`
- `inline SlideResult moveAndSlide(math::vec2 pos, math::vec2 half, math::vec2 velocity, float dt,`

### `LSystem`
<sub>`engine/include/maz/game/LSystem.hpp`</sub>

maz::game L-system (Lindenmayer system) — grammar-based procedural generation. Starting from an axiom string and a set of rewrite rules (symbol -> replacement), it repeatedly expands the string; a turtle interpreter then walks the result to draw line segments. It is the classic compact way to generate plants, trees, roots, and space-filling fractals (Koch, dragon, Sierpinski) from a few characters, and to grow branching dungeon corridors or river networks. Godot ships no L-system, so this is a beyond-Godot procgen utility. The rewrite is deterministic (context-free, single-pass per iteration); the turtle uses the standard command alphabet. Header-only, std-only.

**Types:** `LSystem`, `TurtleSegment`, `TurtleConfig`

**Functions:**

- `inline std::vector<TurtleSegment> interpretTurtle(const std::string& s, const TurtleConfig& cfg)`

### `Leaderboard`
<sub>`engine/include/maz/game/Leaderboard.hpp`</sub>

maz::game leaderboard — a ranked score table with competition ranking and top-N / around-me windows.  Every game with high scores, ranked ladders, speedrun times, or weekly challenges needs the same thing: submit a player's score, and answer "what rank am I?", "show the top 10", and "show me and my neighbours". This does it with proper COMPETITION ranking (tied scores share a rank; the next distinct score skips ahead — the "1224" convention), keeps only each player's BEST score, and supports both higher-is-better (points) and lower-is-better (race/lap times) boards. Pure value logic, header-only, deterministic — unit-tested for ordering, tie ranks, best-score-kept updates, top-N, around-window, and ascending (time) boards.

**Types:** `LeaderboardEntry`, `Leaderboard`

### `Leveling`
<sub>`engine/include/maz/game/Leveling.hpp`</sub>

maz::game experience / leveling system — the character-progression backbone behind XP bars, "level up!" popups, and difficulty pacing. A LevelCurve defines how much experience each level costs; an ExperienceTrack is the live counter that accumulates XP and reports the current level, progress into it, and how much remains. Levels start at 1 and cost 0 XP; the curve's cost-to-next grows per level so the grind lengthens as characters advance. Costs and totals are integers (no float drift on level boundaries), and every cost is clamped to at least 1 so a curve can never grant infinite instant levels. Three curve shapes cover the common designs: linear (arithmetic growth), geometric (each level a fixed percentage more than the last — the classic RPG feel), and an explicit hand-authored table. Godot ships no leveling system — games hand-roll XP curves every time — so this is a beyond-Godot gameplay utility that completes the stat / inventory / loot RPG suite. Header-only, std-only, deterministic.

**Types:** `LevelCurve`, `ExperienceTrack`

### `LootTable`
<sub>`engine/include/maz/game/LootTable.hpp`</sub>

maz::game weighted loot table — the drop system behind chests, defeated enemies, and treasure rolls. A LootTable is a list of entries, each an item id with a relative weight and a quantity range; rolling picks ONE entry with probability proportional to its weight and yields a random count within that entry's [minCount, maxCount]. Weights are relative (a weight-3 entry is three times as likely as a weight-1 one), so designers never have to make them sum to 1. An entry with item id < 0 models a "nothing dropped" outcome (a blank on the wheel): it can win the roll, and the resulting drop reports empty(). Rolls are driven by the engine's deterministic core::Pcg32, so a given seed reproduces the exact same loot — essential for replays and shareable seeds. Godot ships no loot-table resource — games hand-roll weighted drops every time — so this is a beyond-Godot gameplay utility. Header-only, std-only.

**Types:** `LootEntry`, `LootDrop`, `LootTable`

### `MarkovName`
<sub>`engine/include/maz/game/MarkovName.hpp`</sub>

maz::game character-level Markov chain — procedural NAME / word generation from example lists.  Feed it a list of real names (elf names, town names, sci-fi surnames, potions...) and it learns the letter patterns — which letters tend to follow which short sequences — then invents NEW words that share that "flavour" without copying the inputs. This is the classic lightweight generator behind fantasy name makers and roguelike vocabularies. It is an order-`k` model: each next letter is chosen from the distribution that followed the previous `k` letters in the training data (higher order = closer to the source, lower = wilder). Start/end are handled with sentinel markers so generated words begin and end plausibly. Deterministic given a Pcg32 seed, so the same seed always yields the same name — unit-testable for reproducibility and for the invariant that every letter transition it emits was actually observed in training (it never invents unseen patterns). Header-only.

**Types:** `MarkovName`

### `MazeGen`
<sub>`engine/include/maz/game/MazeGen.hpp`</sub>

maz::game maze generation — a "perfect" maze via the recursive-backtracker (depth-first) algorithm. A perfect maze has exactly one path between any two cells: fully connected, no loops, no isolated pockets. The classic look for puzzle levels, hedge mazes, and pipe/wire layouts, and a different flavour again from BSP dungeons (rooms) and cellular caves (organic blobs). The result is rendered as a tile grid of (2*width+1) x (2*height+1) cells: odd coordinates are cell centres, even ones are the walls between them; a wall tile is carved to floor exactly when the DFS connects the two cells it separates. Deterministic for a given seed (core::Pcg32), pure integer grid math, header-only. Godot leaves procedural generation to the game.

**Types:** `Maze`

**Functions:**

- `inline Maze generateMaze(int width, int height, std::uint64_t seed)`

### `Minimax`
<sub>`engine/include/maz/game/Minimax.hpp`</sub>

maz::game minimax with alpha-beta pruning — the classic adversarial search for perfect-information, turn-based games (tic-tac-toe, connect-four, checkers, reversi, and simpler chess-likes). Given a way to list moves, apply a move, tell terminal states, and score a position from the maximizing player's point of view, it returns the optimal move and its value, looking `depth` plies ahead. Alpha-beta pruning skips branches that cannot affect the result, so it explores far fewer nodes than naive minimax while returning the exact same value. Godot ships no game-tree search, so this is a beyond-Godot AI utility. Generic over the caller's State/Move types via std::function callbacks. Header-only, std-only, deterministic (ties keep the first optimal move in move order).

**Types:** `GameRules`, `SearchResult`

### `NavGrid`
<sub>`engine/include/maz/game/NavGrid.hpp`</sub>

A uniform 2D navigation grid over the X/Z ground plane with A* pathfinding. Cells are either walkable or blocked; findPath returns a least-cost route between two cells using 8-directional movement (orthogonal cost 1, diagonal cost sqrt(2)) with an octile-distance heuristic and corner-cutting disallowed (a diagonal step is blocked if either orthogonally-adjacent cell it squeezes past is solid). World<->cell mapping matches the engine's ground-plane convention: X and Z map to grid columns/rows, Y is ignored. Header-only and dependency-free so it unit-tests without a GPU.

**Types:** `NavGrid`

### `NavMesh`
<sub>`engine/include/maz/game/NavMesh.hpp`</sub>

Navigation-mesh pathfinding: polygon-based navigation, the step up from a uniform grid (NavGrid) and the analogue of Godot's NavigationServer / NavigationPolygon. The walkable area is described by a set of CONVEX polygon cells that share edges; build() finds those shared edges (portals) to form a cell graph. findPath() then A*-searches the graph for the corridor of cells between two points and runs the "simple stupid funnel" algorithm over the corridor's portals to string-pull a short, smooth path that hugs corners — instead of the staircase a grid produces. Header-only, dependency-free (2D math only), so it unit-tests without a GPU.

**Types:** `NavPoly`, `NavMesh`

### `NavMesh3D`
<sub>`engine/include/maz/game/NavMesh3D.hpp`</sub>

maz::game 3D navigation mesh — the query side of Godot's NavigationServer3D / NavigationRegion3D. The walkable world is described by convex 3D polygons (floors, ramps, platforms) that share edges; `findPath` returns a smoothed list of 3D waypoints from a start to a goal across them. Because a walkable surface is effectively 2D, this projects every polygon to the ground (XZ) plane and reuses the tested 2D `NavMesh` (M87) for the corridor A* + funnel string-pulling — no duplicated pathfinding — then lifts each waypoint's height back onto the polygon it lands on, so a path correctly climbs ramps and steps. Deterministic and GPU-free, so it unit-tests headlessly; a full pipeline would bake these polygons from level geometry (Recast-style voxelization), which is a documented follow-up.  Scope note (honest): pathfinding + height reconstruction over supplied convex walkable polygons projected on XZ. It does not bake the navmesh from raw geometry, handle overlapping multi-level surfaces at the same XZ, or do dynamic obstacle avoidance; those are follow-ups.

**Types:** `NavMesh3D`

### `NormalLight2D`
<sub>`engine/include/maz/game/NormalLight2D.hpp`</sub>

Normal-mapped 2D lighting — Godot's Light2D with a normal map. A flat 2D sprite carries a NORMAL MAP (a per-texel surface normal, +z pointing out of the screen); a 2D light then shades each texel by how squarely its normal faces the light, so a painted-flat brick wall or ground catches light directionally and reads as three-dimensional — bumps lit on the side facing the light, shadowed on the far side, the highlight sliding across as the light moves. Earlier lights (M89/M101) were flat-coloured pools with occluder shadows but no per-texel normal response; this adds it. It is pure vector math (a 3D point-light Lambert term + smooth distance attenuation), deterministic and headless-testable; a renderer just fills each shaded texel/cell with the returned colour.

**Types:** `PointLight2D`

**Functions:**

- `inline math::vec3 shadePointLight(math::vec2 p, math::vec3 n, math::vec3 albedo, const PointLight2D& L)`
- `inline math::vec3 shadeSurface(math::vec2 p, math::vec3 n, math::vec3 albedo,`
- `inline math::vec3 decodeNormal(math::vec3 rgb)`

### `Octree`
<sub>`engine/include/maz/game/Octree.hpp`</sub>

maz::game::Octree — a bounded-region octree over 3D axis-aligned boxes: the 3D counterpart of game::Quadtree, for broadphase culling and spatial range queries in a 3D world. Dense regions subdivide into eight children while empty space stays a single node, so a query only descends into the octants that could overlap — the standard structure for frustum/box culling of a large 3D scene, physics broadphase candidate lists, and 3D neighbour/AoE queries.  Each item is an id + a 3D AABB (x, y, z, w, h, d). insert() pushes an item to the deepest node that still fully contains it (capped by maxDepth); items straddling a child boundary rest at the parent. query(region) prunes non-overlapping subtrees and returns an EXACT overlap set (no false negatives, and every returned item genuinely overlaps). Header-only, no GPU; rebuild per frame for dynamic scenes (clear() + re-insert) or keep for static geometry.

**Types:** `Octree`

**Functions:**

- `inline bool aabb3Overlap(float ax, float ay, float az, float aw, float ah, float ad, float bx,`
- `inline bool aabb3Contains(float bx, float by, float bz, float bw, float bh, float bd, float ax,`

### `OneWayPlatform`
<sub>`engine/include/maz/game/OneWayPlatform.hpp`</sub>

One-way platforms — Godot's `one_way_collision` on StaticBody2D / TileMap collision shapes. A one-way platform is solid only from ONE side: a character falling onto it lands, but a character jumping up from below passes straight through (and, standing on it, can drop through by tapping down). Maz's Physics2D collides solid boxes both ways; this adds the swept "solid-from-above" resolve a platformer needs. It is pure geometry over a horizontal surface: given a body's vertical span this frame (prevBottom -> curBottom) and its horizontal extent, `resolveOneWayPlatform` reports whether the body crossed the surface FROM ABOVE while descending (a landing) and the snapped resting height; a body moving up, or already below the surface, is never blocked. `resolveOneWayPlatforms` picks the topmost surface a falling body lands on this step. Header-only, no renderer/sim dependency, so it unit-tests headlessly; the app runs a fixed-step simulation and draws the settled result.

**Types:** `OneWayPlatform2D`, `OneWayResult`

**Functions:**

- `inline OneWayResult resolveOneWayPlatform(float prevBottom, float curBottom, float bx0, float bx1,`
- `inline OneWayResult resolveOneWayPlatforms(float prevBottom, float curBottom, float bx0, float bx1,`

### `Overlap3D`
<sub>`engine/include/maz/game/Overlap3D.hpp`</sub>

maz::game::Overlap3D — 3D sphere overlap/trigger queries and a swept-sphere cast against AABBs, the "what's touching this volume?" and "what does this moving ball hit first?" primitives a 3D game needs for triggers, pickups, character sweeps, and explosion/AoE checks. This complements the existing ray queries in game::Collision (raycast vs AABBs) with volume queries.  sphereVsAabb / sphereVsSphere — exact overlap tests (closest-point on the box for AABBs) overlapSphere(...)            — every AABB a sphere touches (Godot's intersect_shape for a SphereShape3D), the trigger/overlap query sphereCast(...)               — the nearest AABB a sphere of radius r sweeping along a ray hits, with the contact distance  The overlap tests are EXACT. sphereCast is CONSERVATIVE: it sweeps by ray-testing each AABB grown by the radius, which is exact when the sphere meets a face but treats box corners as square rather than rounded — so near a corner it can report contact slightly early (never late: it has no false negatives). That's the standard, safe first-pass sweep for character/projectile movement; a corner-exact refinement can layer on later. Header-only, deterministic, no GPU.

**Types:** `Sphere3`

**Functions:**

- `inline math::vec3 closestPointOnAabb(const math::vec3& p, const Aabb& box)`
- `inline bool sphereVsAabb(const math::vec3& center, float radius, const Aabb& box)`
- `inline bool sphereVsSphere(const math::vec3& ca, float ra, const math::vec3& cb, float rb)`
- `inline std::vector<uint32_t> overlapSphere(const math::vec3& center, float radius,`
- `inline RayHit sphereCast(const math::vec3& origin, const math::vec3& dir, float radius,`

### `Parallax`
<sub>`engine/include/maz/game/Parallax.hpp`</sub>

Parallax scrolling backgrounds — Godot's ParallaxBackground + ParallaxLayer. A staple of 2D games that Maz had no notion of: several background layers that scroll at DIFFERENT rates relative to the camera so the scene reads as having depth (distant mountains barely move, near foliage races past), each layer TILED/MIRRORED so a small motif covers an unbounded scroll. This is pure transform math — given the camera's scroll and a layer's motion scale, it produces the layer's on-screen offset and the tiling helpers to cover the viewport — so the app draws whatever art it likes at the returned positions. No renderer dependency; header-only; unit-tests headlessly.

**Types:** `ParallaxLayer`

**Functions:**

- `inline math::vec2 layerOffset(const ParallaxLayer& layer, math::vec2 cameraScroll)`
- `inline float pmod(float a, float period)`
- `inline float firstTile(float offset, float period)`
- `inline int tileCount(float extent, float period)`

### `PathFollow2D`
<sub>`engine/include/maz/game/PathFollow2D.hpp`</sub>

maz::game PathFollow2D — Godot's PathFollow2D node: something that travels ALONG a Curve2D at a distance offset and, optionally, orients itself to the path's direction. You set progress (arc length from the start) or progress_ratio (0..1), advance it each frame, and sample() gives back the world position and a heading. `loop` wraps progress at the ends (a patrol that circles a track); otherwise it clamps. `hOffset` slides the result sideways along the path normal (a lane offset / formation). `rotates` turns the heading to face along the curve. Built on the existing arc-length-baked Curve2D so motion is constant-speed regardless of how the curve bends. Pure, math-only, unit-tested; a moving platform, a homing rail, or a camera dolly rides one of these.

**Types:** `PathSample2D`, `PathFollow2D`

### `PathFollow3D`
<sub>`engine/include/maz/game/PathFollow3D.hpp`</sub>

maz::game Path3D / PathFollow3D — Godot's 3D path nodes, the twin of Path2D/PathFollow2D (M221). A Path3D holds a Curve3D; a PathFollow3D walks it at a *progress* measured in arc length, so something advances along it at constant speed. Exposes loop (wrap vs clamp) and reports the position + the forward tangent at that progress (the caller builds an orientation from it). Pure, header-only, deterministic. Honest scope: Godot's PathFollow3D rotation modes (Y/XY/XYZ/ORIENTED) and per-point tilt/up-vector banking are not modelled — this returns the forward tangent for the caller to use.

**Types:** `PathSample3D`, `Path3D`, `PathFollow3D`

### `Physics2D`
<sub>`engine/include/maz/game/Physics2D.hpp`</sub>

Impulse-based 2D rigid-body dynamics — the layer above collision *detection* (this module resolves collisions, not just reports them). Bodies are circles or axis-aligned boxes carrying velocity, an inverse mass (0 = immovable/infinite mass), restitution (bounciness), and friction; the world integrates gravity, then resolves overlaps with a normal impulse + Coulomb friction impulse + a positional correction (so stacks don't sink), and bounces bodies off a static box. Deterministic under a fixed timestep and free of GPU/RNG, so it unit-tests headlessly. Coordinates are whatever the caller uses (the demos use screen pixels with +y pointing down).

**Types:** `Body2D`, `Bounds2D`, `ContactEvent`, `Joint2D`, `Manifold`, `Contact2`, `ContactConstraint`, `PhysicsWorld2D`

**Functions:**

- `inline Body2D makeWorldBoundary(math::vec2 normal, math::vec2 pointOnPlane)`
- `inline Body2D makePolyline(const std::vector<math::vec2>& points, float thickness = 0.0f)`
- `inline bool contactCircleCircle(const Body2D& a, const Body2D& b, math::vec2& n, float& pen)`
- `inline bool contactBoxBox(const Body2D& a, const Body2D& b, math::vec2& n, float& pen)`
- `inline bool contactCircleBox(const Body2D& c, const Body2D& x, math::vec2& nCircleToBox, float& pen)`
- `inline bool contact(const Body2D& a, const Body2D& b, math::vec2& n, float& pen)`
- `inline void resolveContact(Body2D& a, Body2D& b, const math::vec2& n, float pen)`
- `inline bool collide(Body2D& a, Body2D& b)`
- `inline bool collideCircles(Body2D& a, Body2D& b)`
- `inline void collideBounds(Body2D& b, const Bounds2D& bounds)`
- `inline float cross2(math::vec2 a, math::vec2 b)`
- `inline math::vec2 crossSV(float s, math::vec2 v)`
- _…and 41 more_

### `Physics3D`
<sub>`engine/include/maz/game/Physics3D.hpp`</sub>

Impulse-based 3D rigid-body dynamics — the 3D sibling of Physics2D. This module resolves collisions (it doesn't just report them). Bodies carry a position, linear velocity, an inverse mass (0 = immovable/infinite mass), restitution (bounciness) and friction; the world integrates gravity with semi-implicit Euler, detects contacts, and resolves them with a rotation-aware normal impulse + Coulomb friction impulse + a positional correction (so resting bodies don't sink). Deterministic under a fixed timestep and free of GPU/RNG, so it unit-tests headlessly.  D1 shipped dynamic Spheres + a static ground Plane with linear impulses. D2 (this milestone) adds Box (OBB) shapes and full angular dynamics: an inverse-inertia tensor per body, quaternion orientation integration, and contact impulses applied at the contact point (lever arms), so a tilted box dropped on the ground tumbles and settles flat, and a sphere landing off-centre on a box imparts spin. Rotation is opt-in via enableRotation() — a body left with a zero inverse-inertia tensor behaves exactly like the D1 translation-only body, so every D1 result is unchanged. Box-vs-box contact (3D SAT) and warm-started stacking arrive in later milestones.

**Types:** `Body3D`, `Contact3`, `Constraint3`, `Joint3D`, `RayHit3`, `PhysicsWorld3D`, `MoveResult3`

**Functions:**

- `inline Body3D makeSphere(math::vec3 pos, float radius, float mass = 1.0f)`
- `inline Body3D makeBox(math::vec3 pos, math::vec3 half, float mass = 1.0f)`
- `inline Body3D makeCapsule(math::vec3 pos, float radius, float halfHeight, float mass = 1.0f)`
- `inline Body3D makeGroundPlane(math::vec3 normal, math::vec3 pointOnPlane)`
- `inline Contact3 sphereSphere(int ia, const Body3D& a, int ib, const Body3D& b)`
- `inline Contact3 spherePlane(int is, const Body3D& s, int ip, const Body3D& p)`
- `inline Contact3 sphereBox(int is, const Body3D& s, int ib, const Body3D& box)`
- `inline void boxPlane(int ibox, const Body3D& box, int ip, const Body3D& p,`
- `inline void closestSegSeg3(const math::vec3& p1, const math::vec3& q1, const math::vec3& p2,`
- `inline std::vector<math::vec3> clipPoly3(const std::vector<math::vec3>& poly, const math::vec3& planeN,`
- `inline void capsuleSegment3(const Body3D& c, math::vec3& p0, math::vec3& p1)`
- `inline math::vec3 closestOnSeg3(const math::vec3& p, const math::vec3& a, const math::vec3& b)`
- _…and 18 more_

### `PhysicsQuery2D`
<sub>`engine/include/maz/game/PhysicsQuery2D.hpp`</sub>

2D physics-space queries — the "ask the world a spatial question" side of a 2D physics engine (Godot's PhysicsDirectSpaceState2D): cast a RAY and get the first collider it hits, test whether a POINT lands inside any collider, and cast a bounded SEGMENT. These are the primitives behind line-of-sight checks, hitscan weapons, ground/wall probes, and mouse picking — Maz had rigid-body dynamics + contact generation (Physics2D) but no way to *query* the collider set without stepping the simulation. This is pure geometry against static shape descriptions, so it has no renderer or simulation dependency and unit-tests exhaustively.  Shapes are circles or ORIENTED boxes (a box with a rotation), matching Physics2D's Body2D shapes. Every query takes a 32-bit collision MASK; a shape is only considered when `shape.layer & mask` is non-zero (Godot collision_mask semantics), so callers can probe "only walls" or "only enemies".

**Types:** `QueryShape2D`, `RayHit2D`

**Functions:**

- `inline bool rayCircle(const math::vec2& O, const math::vec2& D, const math::vec2& C, float r, float tMax,`
- `inline bool rayBox(const math::vec2& O, const math::vec2& D, const math::vec2& C, const math::vec2& half,`
- `inline bool pointInShape(const math::vec2& p, const QueryShape2D& s)`
- `inline RayHit2D queryRay(const math::vec2& origin, const math::vec2& dir,`
- `inline RayHit2D querySegment(const math::vec2& a, const math::vec2& b,`
- `inline std::vector<int> queryPoint(const math::vec2& p, const std::vector<QueryShape2D>& shapes,`

### `Quadtree`
<sub>`engine/include/maz/game/Quadtree.hpp`</sub>

maz::game::Quadtree — a bounded-region quadtree over 2D axis-aligned boxes, for broadphase culling and spatial range queries. Where the uniform spatial grid (game::SpatialGrid) is ideal for evenly-spread objects, a quadtree adapts to CLUSTERED distributions: dense regions subdivide deep while empty space stays a single big node, so a query only visits the cells that could possibly overlap. This is the structure engines (Godot included) reach for to answer "what's near here?" — visible-set culling, mouse/AoE picking, neighbour finding — in better-than-linear time.  Each item is an id + an AABB (x, y, w, h). insert() pushes an item down to the deepest node that still fully contains it (capped by maxDepth); items straddling a child boundary rest at the parent. query(region) walks only nodes whose bounds overlap the region and returns every item whose AABB overlaps it — no false negatives (a superset-free exact overlap set), with non-overlapping subtrees pruned. Header-only, no GPU; rebuilt per frame for dynamic scenes (clear() + re-insert) or kept for static geometry.

**Types:** `Quadtree`

**Functions:**

- `inline bool aabbOverlap(float ax, float ay, float aw, float ah, float bx, float by, float bw,`
- `inline bool aabbContains(float bx, float by, float bw, float bh, float ax, float ay, float aw,`

### `Quest`
<sub>`engine/include/maz/game/Quest.hpp`</sub>

maz::game quest / objective tracker — the journal behind "kill 5 goblins", "collect 3 keys", and the "Quest Complete!" banner. A quest is a set of counted objectives; each objective needs a target count and tracks progress toward it, and a quest auto-completes when every objective is met. The canonical driver is `advance(objectiveId, amount)`: the game reports an event once ("a goblin died -> objective 5 +1") and every active quest with a matching objective moves forward, with quests that finish reported back so the UI can celebrate. Quests carry a state (Inactive -> Active -> Completed / Failed) and the log answers which are active or completed for a journal screen. Godot ships no quest system — games hand-roll it every time — so this is a beyond-Godot gameplay utility extending the RPG suite. Header-only, std-only, deterministic.

**Types:** `Objective`, `Quest`, `QuestLog`

### `Ragdoll`
<sub>`engine/include/maz/game/Ragdoll.hpp`</sub>

maz::game ragdoll builder — Godot's PhysicalBone3D ragdoll: turn a skeleton (a list of bones, each a segment with a parent) into a jointed set of capsule rigid bodies. Each bone becomes a capsule oriented along its length; each non-root bone is tied to its parent by a cone-twist joint at the shared joint point, so the assembly swings and collapses like a limp body while staying connected. This composes the existing 3D physics (capsule bodies + cone-twist joints, M235) into the one helper games actually want. Pure CPU; unit-tests exactly (bodies + joints created, connectivity preserved under simulation, falls under gravity without exploding).

**Types:** `RagdollBone`, `Ragdoll`

**Functions:**

- `inline math::quat shortestArc(const math::vec3& from, const math::vec3& to)`
- `inline Ragdoll buildRagdoll(PhysicsWorld3D& world, const std::vector<RagdollBone>& bones)`

### `ReactionDiffusion`
<sub>`engine/include/maz/game/ReactionDiffusion.hpp`</sub>

maz::game reaction-diffusion — a Gray-Scott simulation that grows organic Turing patterns (spots, stripes, mazes, coral, mitosis) from two diffusing/reacting chemicals U and V on a grid. It is the classic procedural source for animal-coat textures, rust/lichen growth, and alien-surface detail that plain noise can't produce, because the pattern emerges from a feedback rule rather than a static field. Godot has no built-in reaction-diffusion, so this is a beyond-Godot procedural utility. The grid is toroidal (wrap-around), the step is a standard explicit Euler update with a 5-point Laplacian, and everything is deterministic. Header-only, std-only.

**Types:** `GrayScottParams`, `ReactionDiffusion`

### `Recoil`
<sub>`engine/include/maz/game/Recoil.hpp`</sub>

maz::game::RecoilPattern — the climbing "spray" every first-person shooter needs: each shot kicks the aim by a DEFINED amount (so a weapon has a recognisable, learnable spray pattern like CS/Valorant), the kicks ACCUMULATE while firing, and the aim RECOVERS smoothly back toward centre when you stop. This is distinct from game::Spread (M632), which perturbs each shot by a RANDOM amount within a cone (bullet inaccuracy); recoil is the deterministic, per-shot, memorised climb that the player learns to counter by pulling down. You give it a pattern (a list of per-shot offset kicks — typically climbing up then drifting), call fire() per shot to advance and accumulate, update(dt) each frame to recover, and add offset() to the aim direction/crosshair. Firing past the pattern's end repeats the last kick (a sustained climb). Beyond Godot. Header-only, pure, deterministic.

**Types:** `RecoilPattern`

### `Reputation`
<sub>`engine/include/maz/game/Reputation.hpp`</sub>

maz::game faction reputation system — the standing meter behind "the guards now attack you on sight" and "the merchants give you a discount." Each faction carries a reputation value (clamped to a configurable range) that quests and deeds nudge up or down; that value maps through thresholds to a Standing tier — Hostile / Unfriendly / Neutral / Friendly / Allied — which the game reads to decide who fights, trades, or opens doors for the player. Thresholds are configurable so a game can tune how quickly goodwill turns to alliance. Godot ships no reputation/faction system — games hand-roll it every time — so this is a beyond-Godot gameplay utility that pairs with the quest and dialogue systems. Header-only, std-only, deterministic.

**Types:** `Reputation`

### `Shake`
<sub>`engine/include/maz/game/Shake.hpp`</sub>

Trauma-based camera shake (after Squirrel Eiserloh's "Juicing Your Cameras"). Impactful moments add "trauma" in [0,1]; the shake amount is trauma^2 so small hits barely wobble while big ones jolt, and trauma decays linearly so the shake settles on its own. The offset/rotation are pure functions of (trauma, time) built from layered sines, so no RNG state is needed and the motion is smooth and deterministic. Feed the offset into the camera position and the yaw/pitch into the look angles each frame.

**Types:** `Shake`

### `ShapeCast2D`
<sub>`engine/include/maz/game/ShapeCast2D.hpp`</sub>

Swept-shape casting — continuous collision detection, the "move a shape and find the first thing it hits" query behind Godot's ShapeCast2D and PhysicsDirectSpaceState2D.cast_motion. PhysicsQuery2D already answers ZERO-radius questions (a ray, a segment, a point); this answers the FINITE-radius one: sweep a circle of radius `r` from A to B through a set of static colliders and report the first contact — the fraction of the motion travelled, the world contact point, the surface normal, and the caster's centre at that instant. That is what stops a fast projectile or a character-controller step from TUNNELLING through a thin wall in a single frame (a discrete overlap test at A and at B misses a wall that sits entirely between them). It is also the primitive under a "how far can I move before I touch something" probe.  The obstacle set reuses PhysicsQuery2D's `QueryShape2D` (circles + oriented boxes) and its 32-bit collision MASK, so the same world description feeds rays, points, and now swept circles. The maths is the Minkowski sum: sweeping a circle of radius r against a shape is the same as sweeping a POINT (the circle's centre) against that shape GROWN by r — a circle of radius R+r, or a box rounded by r. So a circle-vs-circle sweep is a ray against an inflated circle, and a circle-vs-box sweep is a ray against the box's four r-offset faces plus four r-radius corner arcs. Pure geometry, header-only, deterministic — it unit-tests exactly (a hit leaves the caster exactly r from the surface) and drives a golden.  Scope note (honest): this casts a CIRCLE (the common character/projectile probe). Casting an arbitrary oriented BOX or convex polygon along a motion — Godot's full ShapeCast2D with any CollisionShape2D — is the follow-up; so is returning Godot's separate safe/unsafe fractions for a start-in-contact shape (here a caster that already overlaps reports contact at fraction 0 with a push-out normal).

**Types:** `ShapeCastHit2D`

**Functions:**

- `inline bool sweptCircleCircle(const math::vec2& P, const math::vec2& D, float r, const math::vec2& C,`
- `inline bool sweptCircleBox(const math::vec2& P, const math::vec2& D, float r, const math::vec2& C,`
- `inline ShapeCastHit2D shapeCastCircle(const math::vec2& from, const math::vec2& motion, float radius,`

### `Shop`
<sub>`engine/include/maz/game/Shop.hpp`</sub>

maz::game shop / merchant economy — the vendor counter behind "buy" and "sell." A Shop is a catalogue of items, each with a gold price and an optional stock count, that trades against a player's gold wallet and maz::game::Inventory. `buy` charges gold, hands over the item, and draws down stock; `sell` takes the item back and pays out a fraction of its price (the sell margin, the classic "shops pay less than they charge"). Every trade is atomic and fully guarded — it checks gold, stock, inventory room, and ownership up front and returns a typed TradeResult, leaving the wallet and bag untouched on any failure. The player's gold lives outside the shop (passed by reference), so one wallet can visit many shops. Built on the shared Inventory item model. Godot ships no shop/economy system — games hand-roll it every time — so this is a beyond-Godot gameplay utility extending the RPG suite. Header-only, std-only, deterministic.

**Types:** `ShopItem`, `TradeResult`, `Shop`

### `SkillTree`
<sub>`engine/include/maz/game/SkillTree.hpp`</sub>

**Types:** `SkillTree`

### `SoftBody`
<sub>`engine/include/maz/game/SoftBody.hpp`</sub>

maz::game soft bodies — Godot's SoftBody3D (cloth, rope, jelly). Rather than a rigid transform, a soft body is a cloud of point masses linked by distance constraints, simulated with Position-Based Dynamics: each step Verlet-integrates the particles, then repeatedly nudges each constrained pair back toward its rest length. PBD is unconditionally stable (it corrects positions, never adds unbounded force), so cloth hangs, ropes swing, and pinned points hold — all deterministically. Pure CPU math, header-only; unit-tests exactly (a stretched link relaxes to rest, a pinned chain hangs without stretching, energy stays bounded). Collision against the rigid world is a follow-up.

**Types:** `SoftParticle`, `DistanceConstraint`, `SoftBody`

### `SoftShadow2D`
<sub>`engine/include/maz/game/SoftShadow2D.hpp`</sub>

---- Soft (penumbra) 2D shadows via area-light sampling ---------------------------------------- A point light (game::Visibility2D) casts a razor-sharp shadow: every point is either lit or not. A real light has SIZE, so shadow edges are soft — an inner UMBRA that sees none of the light, an outer fully-lit region, and a PENUMBRA between them that sees only part of the light. Godot's Light2D approximates this with a shadow filter. Here we model the light as a small DISC and sample it: cast a hard shadow from each sample point and average. A point that can reach every sample is fully lit; one that reaches none is in umbra; the fraction it can reach IS the soft-shadow value. Pure 2D math (no GPU), so it unit-tests headless; the renderer approximates it by compositing one faint visibility fan per sample additively.

**Functions:**

- `inline bool segmentsIntersect(math::vec2 p1, math::vec2 p2, math::vec2 q1, math::vec2 q2)`
- `inline bool lineBlocked(math::vec2 a, math::vec2 b, const std::vector<Segment2>& occluders)`
- `inline std::vector<math::vec2> diskSamples(math::vec2 center, float radius, int count)`
- `inline float softVisibility(math::vec2 p, math::vec2 lightCenter, float lightRadius,`

### `SpanningTree`
<sub>`engine/include/maz/game/SpanningTree.hpp`</sub>

maz::game minimum spanning tree — Kruskal's algorithm over a weighted, undirected graph. Given a set of nodes and weighted edges, it returns the cheapest set of edges that keeps every node connected (with no cycles). This is the standard procedural-generation tool for connecting a scatter of dungeon rooms with the shortest total corridor length, laying out road/river/power networks, or clustering points — anywhere you need "link these up as cheaply as possible". If the graph is disconnected it returns the minimum spanning FOREST (one tree per component). Reuses core::DisjointSet (union-find) so each edge is accepted only when it joins two so-far-separate components. Godot ships no MST, so this is a beyond-Godot utility. Deterministic (ties broken by original edge order). Header-only, std-only.

**Types:** `MstEdge`, `MstResult`

**Functions:**

- `inline MstResult minimumSpanningTree(int nodeCount, const std::vector<MstEdge>& edges)`

### `SpatialGrid`
<sub>`engine/include/maz/game/SpatialGrid.hpp`</sub>

Uniform spatial hash over the X/Z plane for broadphase AABB queries. Static level geometry (walls, blocks) is bucketed once into square cells; a query then tests only the solids sharing the query box's cells instead of the whole world. Y is ignored in bucketing, which suits ground-plane games where colliders are tall relative to the play area; the narrow-phase overlap test is still full 3D.

**Types:** `SpatialGrid`

**Functions:**

- `inline math::vec3 slideMove(math::vec3 pos, const math::vec3& delta, const math::vec3& halfExtents,`

### `Sph2D`
<sub>`engine/include/maz/game/Sph2D.hpp`</sub>

maz::game — 2D Smoothed-Particle Hydrodynamics (SPH), the particle-based way to simulate fluids: water, goo, lava, blood, or a splash of coloured liquid, as a cloud of little blobs that push apart when squeezed and drag along their neighbours. Each particle carries a "smoothing kernel" — a soft bump of influence of radius h — and the fluid's density at a particle is the overlap of its neighbours' bumps. Where the fluid is denser than its rest density it develops pressure that shoves particles apart; a viscosity term makes neighbours share velocity so the flow stays coherent. This is the runtime core behind liquid effects, destructible mud/slime, and toy fluid sandboxes. It uses the standard Müller/Monaghan kernels (poly6 for density, spiky-gradient for pressure, viscosity-laplacian for drag) with Monaghan's symmetric pressure form, so the internal pressure forces conserve momentum exactly. Godot has no fluid solver. Header-only, std-only, deterministic; brute-force neighbours (fine for the few-thousand-particle scale games use).

**Types:** `SphParams`

**Functions:**

- `inline std::vector<float> sphDensities(const std::vector<math::vec2>& pos, const SphParams& p)`
- `inline std::vector<math::vec2> sphAccelerations(const std::vector<math::vec2>& pos,`

### `Spread`
<sub>`engine/include/maz/game/Spread.hpp`</sub>

maz::game WEAPON SPREAD — perturb an aim direction into a cone/fan for shotgun pellets, bullet inaccuracy, spray weapons, and particle emission. `spreadDirection2D`/`spreadDirection3D` jitter a direction randomly within a half-angle (using any RNG with `rangef(lo,hi)`, e.g. `core::Pcg32`); `spreadFan2D` returns a DETERMINISTIC, evenly-spaced fan of directions for a fixed multi-pellet pattern. 3D sampling is uniform over the cone's solid angle (no clustering at the axis). Godot has no spread helper, so this is a beyond-Godot gameplay utility. Header-only, deterministic (randomness lives in the caller's seeded RNG). Pass unit directions in.

**Functions:**

- `inline math::vec2 spreadDirection2D(const math::vec2& dir, float halfAngle, Rng& rng)`
- `inline std::vector<math::vec2> spreadFan2D(const math::vec2& dir, float halfAngle, int count)`
- `inline math::vec3 spreadDirection3D(const math::vec3& axis, float coneHalfAngle, Rng& rng)`

### `Stat`
<sub>`engine/include/maz/game/Stat.hpp`</sub>

maz::game stat / modifier system — the backbone of RPG character attributes, buffs/debuffs, and equipment bonuses. A Stat holds a base value plus a stack of modifiers and combines them into a final value in a fixed, well-defined order so results are deterministic and stack predictably: all FLAT bonuses add first (base + 5 + 10), then all ADDITIVE-PERCENT bonuses sum and apply once (+10% +20% = +30%, so xx1.3), then each MULTIPLICATIVE-PERCENT bonus applies in turn (a separate x1.5 on top). Each modifier carries a `source` id so a whole buff or an unequipped item can be removed in one call without tracking individual handles. This is the classic "flat/percent-additive/percent-multiplicative" order used across game frameworks; Godot ships no stat system, so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic (multiplicative order is commutative, so the final value is independent of insertion order).

**Types:** `ModifierType`, `StatModifier`, `Stat`

### `StateMachine`
<sub>`engine/include/maz/game/StateMachine.hpp`</sub>

A lightweight finite state machine keyed by an integer/enum state id — the decision layer that sits above steering/pathfinding (guard patrol -> chase -> return), and equally the backbone of game flow (menu/playing/paused) or animation states. Each state has optional onEnter/onUpdate/ onExit callbacks. Transitions are guarded predicates evaluated every update(); "any" transitions fire from whatever state is current. Evaluation is deterministic: on each update the any- transitions are checked first (in registration order), then the current state's transitions, and the first guard that returns true wins. Header-only, no GPU/allocation beyond the callback lists.

**Types:** `StateMachine`

### `StatusEffect`
<sub>`engine/include/maz/game/StatusEffect.hpp`</sub>

maz::game status-effect system — the timed buff / debuff layer behind poison, regeneration, haste, and burning. Each effect has a type id, a remaining duration, a stack count, and an optional periodic interval; `update(dt)` counts every effect down, fires a "tick" each time an effect's interval elapses (the moment a damage-over-time or heal-over-time effect should act), and drops effects whose duration runs out. The ticks are reported back with the effect's current stack count, so the game multiplies poison damage by stacks without tracking timers itself. Re-applying an effect follows a stack policy — refresh the timer, add a stack (up to a cap), or keep the existing one. This is DISTINCT from the stat modifier stack (untimed, in Stat.hpp) and the ability cooldown bank (one gating timer per ability, in Cooldown.hpp): status effects are the durational on-entity effects that expire and pulse. Godot ships no status-effect system — games hand-roll it every time — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `StackMode`, `StatusEffect`, `StatusTick`, `StatusEffectSystem`

### `Steering`
<sub>`engine/include/maz/game/Steering.hpp`</sub>

Reynolds-style steering behaviors for autonomous agents. Each behavior returns a steering *force* (an acceleration request, already clamped to the agent's maxForce); the caller sums the forces it wants, then calls integrate() to apply them. Forces compose linearly, so seek + separation + path-following just add together. Pure vector math — no GPU/allocation, so it unit-tests headless.  Convention: full 3D vectors, but games that move on the ground can simply keep y fixed (the demo zeroes the y component of every force). Speeds/forces are in world-units/second and /second^2.

**Types:** `Agent`

**Functions:**

- `inline math::vec3 limit(const math::vec3& v, float maxLen)`
- `inline math::vec3 seek(const Agent& a, const math::vec3& target)`
- `inline math::vec3 flee(const Agent& a, const math::vec3& target)`
- `inline math::vec3 arrive(const Agent& a, const math::vec3& target, float slowRadius)`
- `inline math::vec3 separation(const Agent& a, const std::vector<math::vec3>& neighborPositions,`
- `inline void integrate(Agent& a, const math::vec3& force, float dt)`
- `inline math::vec3 followPath(const Agent& a, const std::vector<math::vec3>& waypoints,`

### `SweepPrune2D`
<sub>`engine/include/maz/game/SweepPrune2D.hpp`</sub>

maz::game::SweepPrune2D — a sweep-and-prune (SAP) broadphase for 2D AABBs. Where a uniform grid (game::SpatialGrid) buckets objects by cell, SAP sorts objects along an axis and sweeps a moving window: two boxes can only overlap if their projections onto that axis overlap, so once the sweep passes a box's right edge it can stop comparing against it. This is the broadphase engines reach for when object sizes vary a lot or the world is unbounded (no fixed cell size to tune) — it finds the same candidate pairs a grid would, but adapts to the data instead of a chosen resolution.  build() takes id+AABB boxes; overlappingPairs() returns every pair whose AABBs actually overlap (the sweep prunes on the sort axis, then a cheap y-test confirms — so the result is exact, no false positives). The sort axis is chosen automatically as the one with greater spread (fewer sweep collisions). O(n log n) to sort + O(n + k) to report k pairs. Header-only, deterministic, no GPU — a drop-in broadphase for physics/trigger/query systems.

**Types:** `SweepPrune2D`

### `ThetaStar`
<sub>`engine/include/maz/game/ThetaStar.hpp`</sub>

maz::game Theta* any-angle pathfinding — a grid path planner that produces SHORT, STRAIGHT routes instead of the staircase zig-zag that ordinary grid A* (and jump-point search) is stuck with. Classic grid search can only step between cell centres along the 8 compass directions, so a diagonal crossing of an open room comes out as a jagged approximation that is both longer and visibly unnatural. Theta* adds a line-of-sight test: when relaxing a node it checks whether the node's *grandparent* can see the new cell directly, and if so it links straight to it — letting path segments cut across the grid at any angle. The result hugs walls tightly and, in open space, collapses to a single straight line of the true Euclidean length. This is exactly the "shorter, natural-looking" path Godot's grid navigation can't produce. Uses the standard Nash line-of-sight. Header-only, std-only, deterministic. Grid steps are 8-connected with no corner cutting; as in standard Theta*, an any-angle shortcut may graze a convex obstacle's outer corner but never passes through a wall's body.

**Types:** `ThetaPath`

**Functions:**

- `inline bool thetaLineOfSight(const std::vector<std::uint8_t>& blocked, int w, int h, int x0, int y0, int x1,`
- `inline ThetaPath thetaStar(const std::vector<std::uint8_t>& blocked, int w, int h, int sx, int sy, int gx,`

### `TileSet`
<sub>`engine/include/maz/game/TileSet.hpp`</sub>

TileSet — Godot's TileSet resource. A Tilemap holds a grid of tile IDs; a TileSet gives each ID MEANING: which atlas cell to DRAW it with, and what COLLISION it contributes. Godot's per-tile collision can be SUB-CELL (a half-height platform, a shelf), which the Tilemap's single "solid" bit can't express — so a level built from one grid can mix full walls with thin ledges. TileDef carries a None / Full / Box collision plus an atlas source cell; the free queries turn a (Tilemap, TileSet) pair into world-space collision boxes, a point-solidity test, and a drop-to-ground helper. Pure data + geometry — deterministic, unit-testable, no GPU.

**Types:** `TileDef`, `TileSet`, `TileBox`

**Functions:**

- `inline TileBox tileBox(const Tilemap& map, const TileDef& def, int cx, int cy)`
- `inline std::vector<TileBox> collectSolids(const Tilemap& map, const TileSet& set)`
- `inline bool solidAt(const Tilemap& map, const TileSet& set, math::vec2 p)`
- `inline float dropY(const Tilemap& map, const TileSet& set, float x, float fromY, float maxY)`

### `Tilemap`
<sub>`engine/include/maz/game/Tilemap.hpp`</sub>

**Types:** `Tilemap`

### `Timer`
<sub>`engine/include/maz/game/Timer.hpp`</sub>

maz::game Timer — Godot's Timer node: a countdown that fires a "timeout" when it elapses. Set a wait time, start it, and tick(delta) each frame; when the countdown crosses zero it fires (via return count and an optional callback). one_shot stops after the first fire; otherwise it repeats, carrying the leftover time so the cadence never drifts (a big delta can fire it several times in one tick). Pausable and restartable. Pure logic — no clock; the caller supplies delta — so it is deterministic and unit-testable. Matches Godot's defaults: one_shot is false (repeating).

**Types:** `Timer`

### `TriMesh3D`
<sub>`engine/include/maz/game/TriMesh3D.hpp`</sub>

maz::game concave trimesh collider — Godot's ConcavePolygonShape3D: an arbitrary triangle soup used as static level geometry (the whole world's collision mesh). Unlike a convex shape it can be any shape, so it's a static/immovable collider. Ray queries against a big mesh would be O(triangles) naively; TriMesh3D builds a BVH over the per-triangle AABBs and, on a raycast, only tests the few triangles whose bounds the ray actually enters (Möller-Trumbore), returning the nearest hit with its point, distance, triangle index, and geometric normal. Building on the existing game::Bvh keeps this small and exact. Pure math (no renderer/physics), deterministic, header-only, unit-tested.

**Types:** `TriMeshHit`, `TriMesh3D`

### `TurnOrder`
<sub>`engine/include/maz/game/TurnOrder.hpp`</sub>

maz::game turn-order scheduler — the initiative queue behind tactics and JRPG combat, where each combatant acts in order of a speed / initiative score, highest first, and play loops round after round. Combatants are added with an initiative value; `start` opens round 1 by sorting them (ties broken by id so the order is deterministic), `current` names whose turn it is, and `advance` steps to the next combatant, rolling into a fresh round (re-sorting to pick up any initiative changes) once everyone has acted. Combatants can be removed mid-battle (a defeated enemy is skipped immediately, passing the turn on) and added between rounds (a summon joins the next round). Godot ships no turn/initiative system — games hand-roll it every time — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `TurnOrder`

### `ViewCone`
<sub>`engine/include/maz/game/ViewCone.hpp`</sub>

maz::game VIEW CONE — the continuous "can this AI see that?" perception test: is a target within an observer's sight RANGE and inside its field-of-view CONE (half-angle around a facing direction)? This is the geometry every stealth guard, turret, sentry, or aggro check needs — distinct from the grid-based shadowcasting `FieldOfView` (which lights tiles). Pure angle+distance math; combine with a line-of-sight/raycast check for wall occlusion (kept separate so you choose the occluder source). Works in 2D and 3D. Header-only, deterministic. Godot has no built-in vision-cone helper, so this is a beyond-Godot gameplay utility.

**Types:** `ViewSample`

**Functions:**

- `inline ViewSample sampleViewCone2D(const math::vec2& observer, const math::vec2& facing, const math::vec2& target,`
- `inline bool inViewCone2D(const math::vec2& observer, const math::vec2& facing, const math::vec2& target,`
- `inline bool inViewCone3D(const math::vec3& observer, const math::vec3& facing, const math::vec3& target,`

### `Visibility2D`
<sub>`engine/include/maz/game/Visibility2D.hpp`</sub>

2D visibility / light occlusion: from a point light, compute the polygon of everything it can see given a set of blocking segments (occluders) inside a bounding rectangle — the geometry behind 2D lights + shadows (Godot's Light2D + LightOccluder2D). The result is a star-shaped polygon around the light: fill it (a triangle fan from the light) to render the lit region; the notches carved out behind occluders ARE the shadows. Uses the classic angle-sweep algorithm — cast a ray toward every occluder endpoint (and just past each side of it) and keep the nearest hit. Header-only, dependency- free (2D math only), so it unit-tests without a GPU.

**Types:** `Segment2`, `Visibility2D`

### `VisibleOnScreenNotifier2D`
<sub>`engine/include/maz/game/VisibleOnScreenNotifier2D.hpp`</sub>

maz::game VisibleOnScreenNotifier2D — Godot's node of the same name: watches whether an object's rectangle overlaps the camera's view and fires an edge event the moment it enters or leaves the screen. Games use it to wake/sleep behaviour cheaply — spawn an enemy only once its region scrolls into view, pause an off-screen animation, free a far-away emitter. It holds the object's world-space rect; update(view) compares it to the current camera rect and returns entered/exited transitions (not the steady state), tracking the on-screen flag between calls. Pure rect math, deterministic, header-only, unit-tested — no renderer or camera object required, just the two rectangles.

**Types:** `ScreenNotifierEvents`, `VisibleOnScreenNotifier2D`

### `VoxelRaycast`
<sub>`engine/include/maz/game/VoxelRaycast.hpp`</sub>

maz::game 3D voxel ray traversal — the Amanatides & Woo "A Fast Voxel Traversal Algorithm" (1987): walk a ray through a 3D grid of unit cells and visit EVERY voxel it passes through, in order, with no gaps and no duplicates. This is the 3D companion to GridRaycast (which is 2D) and the workhorse behind block-world interaction: which block is the player looking at / mining / placing against, 3D line-of-sight and light propagation through a voxel volume, and ray-marching a sparse voxel scene. A naive "step along the ray in small increments" either skips thin voxels (steps too big) or visits the same voxel many times (steps too small) and drifts off the true line; this advances exactly to the next cell boundary each iteration, so it is both exact and O(number of voxels crossed). Cells are unit-sized; the integer cell of a point p is floor(p) per axis. Header-only, std-only, deterministic. Godot ships no voxel traversal.

**Functions:**

- `inline int voxelFloor(float v)`
- `inline std::vector<math::Vector3i> traverseVoxels(const math::vec3& origin, const math::vec3& dir,`
- `inline std::optional<math::Vector3i> voxelRaycast(const math::vec3& origin, const math::vec3& dir,`

### `WangTiles`
<sub>`engine/include/maz/game/WangTiles.hpp`</sub>

maz::game — Wang tiling: lay out tiles so their edges always match, producing large NON-REPEATING textures, terrain, dungeons, or road/river networks from a small tile set. Each Wang tile has a colour on each of its four edges; two tiles may sit next to each other only if their touching edges share a colour. Because the constraint is purely local, you can fill an arbitrarily large grid one tile at a time and the result tiles seamlessly yet never falls into an obvious repeating pattern — the classic trick behind infinite ground textures, auto-generated mazes, and pipe/track puzzles. This does stochastic scanline placement: for each cell it picks (deterministically from a seed) among the tiles whose west edge matches the left neighbour's east edge and whose north edge matches the upper neighbour's south edge. With a COMPLETE tile set (at least one tile for every west/north colour pair) placement never gets stuck. Godot ships no Wang tiler. Header-only, std-only, deterministic.

**Types:** `WangTile`

**Functions:**

- `inline std::uint64_t wangHash(std::uint64_t x)`
- `inline std::vector<int> wangTiling(const std::vector<WangTile>& tiles, int width, int height,`

### `WaveField2D`
<sub>`engine/include/maz/game/WaveField2D.hpp`</sub>

maz::game 2D wave / ripple simulation — the "water surface" effect on a grid.  Drop a stone in a pond and rings spread out, bounce off the edges, cross each other, and fade. This simulates that on a height grid with the classic two-buffer wave step (Hugo Elias' water algorithm, a discretisation of the wave equation): each cell's next height is the average of its four neighbours' current heights minus its own PREVIOUS height, times a damping factor. That one line reproduces travelling ripples, interference between multiple drops, reflection at the boundary, and gradual decay. Feed the height (or its gradient) into a normal map / UV distortion to render water, force fields, shockwaves, or a trampoline surface. Border cells are held at rest (a fixed shore). Pure CPU, header-only, deterministic — unit-tested for a still surface staying still, radial symmetry of a centred drop, outward propagation, and amplitude decay under damping.

**Types:** `WaveField2D`

### `WaveFunctionCollapse`
<sub>`engine/include/maz/game/WaveFunctionCollapse.hpp`</sub>

Adjacency rules over up to 64 tile types. Directions: 0 = +x (right), 1 = -x (left), 2 = +y (down), 3 = -y (up). allowedMask[dir][a] is the bitset of tiles permitted to sit in direction `dir` from `a`.

**Types:** `WfcRules`, `WfcResult`

**Functions:**

- `inline std::uint64_t allowedNeighbours(const WfcRules& r, std::uint64_t mask, int dir)`
- `inline WfcResult wfcGenerate(const WfcRules& rules, int width, int height, std::uint64_t seed,`

### `WaveSpawner`
<sub>`engine/include/maz/game/WaveSpawner.hpp`</sub>

maz::game wave spawner — the enemy-wave director behind tower-defense, survival, and horde modes. Waves run in sequence: each wave waits a start delay, then releases its enemies one at a time on a fixed interval, and the NEXT wave begins only once the current wave is fully spawned AND every enemy is dead (the game reports kills). `update(dt)` returns the enemy-type ids to spawn this tick, `reportKilled` feeds the clear condition, and the spawner tracks the current wave, the alive count, and whether all waves are finished. Purely time- and event-driven, no rendering — the caller turns each returned id into an actual enemy. Godot ships no wave/spawner system — games hand-roll it every time — so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic.

**Types:** `Wave`, `WaveSpawner`


<a name="anim"></a>
## Anim — skeletons, clips, blending, tweening, curves

### `AdditiveBlend`
<sub>`engine/include/maz/anim/AdditiveBlend.hpp`</sub>

Additive / layered pose blending — Godot's AnimationNodeAdd2 (and the "additive" import flag). The existing blendPoses/blendPosesWeighted CROSS-FADE between whole poses (idle <-> walk): every joint is interpolated, so a walk pose fully replaces an idle pose at weight 1. Additive blending instead layers a *difference* on top of a base: an additive clip is stored relative to a REFERENCE pose, its per-joint DELTA (how far each joint moved from the reference) is computed, and that delta is applied on top of whatever base pose is playing — scaled by a weight. A joint that doesn't move in the additive clip has a zero delta and leaves the base untouched, so you can layer a "wave", "breathe", "aim", or "recoil" motion onto any locomotion without disturbing the unrelated joints. Pure math on JointPose (TRS with a quaternion), header-only, unit-testable without a skeleton or GPU.

**Functions:**

- `inline JointPose makeAdditiveDelta(const JointPose& additive, const JointPose& reference)`
- `inline JointPose applyAdditiveDelta(const JointPose& base, const JointPose& delta, float weight)`
- `inline JointPose additiveBlendJoint(const JointPose& base, const JointPose& additive,`
- `inline void makeAdditivePose(const std::vector<JointPose>& additive,`
- `inline void applyAdditivePose(const std::vector<JointPose>& base, const std::vector<JointPose>& delta,`
- `inline void additiveBlend(const std::vector<JointPose>& base, const std::vector<JointPose>& additive,`

### `AnimClip`
<sub>`engine/include/maz/anim/AnimClip.hpp`</sub>

Keyframed animation clips — the playback layer on top of Skeleton. A clip stores, per joint, three keyframe tracks (translation, rotation, scale); sampling at a time interpolates each track (vec3 lerp, quaternion slerp) into a per-joint local pose, which feeds Skeleton::computeSkinning. blendPoses cross-fades two sampled poses, the basis of animation state blending (idle<->walk). Pure math — no GPU — so it unit-tests headless and stays deterministic under the fixed timestep.

**Types:** `JointPose`, `Key`, `JointTrack`, `AnimClip`

**Functions:**

- `inline math::vec3 sampleVec3(const std::vector<Key<math::vec3>>& keys, float time,`
- `inline math::quat sampleQuat(const std::vector<Key<math::quat>>& keys, float time,`
- `inline void blendPoses(const std::vector<JointPose>& a, const std::vector<JointPose>& b, float weight,`
- `inline void blendPosesWeighted(const std::vector<const std::vector<JointPose>*>& poses,`
- `inline void posesToLocals(const std::vector<JointPose>& poses, std::vector<math::mat4>& out)`

### `AnimStateMachine`
<sub>`engine/include/maz/anim/AnimStateMachine.hpp`</sub>

---- Animation state machine ------------------------------------------------------------------- Godot's AnimationNodeStateMachine: a graph of named states with CROSS-FADING transitions. Each state here carries an integer payload (a clip index, or a blend-space id — so a state can itself be a blend space, giving "a state machine over blend spaces"). The machine tracks the current state, runs timed cross-fades on transitions, and reports the active state(s) with weights that sum to 1 — the exact same shape as a blend space's weights, so the output feeds straight into anim::blendPosesWeighted and the two compose (state weight x blend-space weight). Transitions fire on an explicit travel(name) or when a per-transition condition() returns true. Pure logic (no GPU/clips), so it unit-tests headless.

**Types:** `AnimStateMachine`

### `Animator`
<sub>`engine/include/maz/anim/Animator.hpp`</sub>

The animation controller — the stateful layer a game actually drives. It holds a library of named clips and plays one at a time; play() starts a timed CROSS-FADE from whatever is currently playing to a new clip, and both clips keep advancing during the fade so the blend is smooth (a run doesn't freeze while it eases into a jump). update() advances time + the fade; pose() returns the blended per-joint result to hand to Skeleton::computeSkinning. Pure logic on top of AnimClip — no GPU, so it unit-tests headless and stays deterministic under the fixed timestep.

**Types:** `Animator`

### `BlendSpace`
<sub>`engine/include/maz/anim/BlendSpace.hpp`</sub>

Animation blend spaces — Godot's AnimationTree BlendSpace1D / BlendSpace2D. A blend space places animations (referenced here by an integer `id`, e.g. a clip index) at positions in a 1-D or 2-D parameter plane; querying a point returns the small set of animations to mix and the weight of each (summing to 1). Feed those weights + the sampled poses into anim::blendPosesWeighted to get one blended pose. Pure geometry — no GPU, no clips — so it unit-tests headlessly and stays deterministic.

**Types:** `BlendSpace1D`, `BlendSpace2D`

### `BlendTree`
<sub>`engine/include/maz/anim/BlendTree.hpp`</sub>

Animation blend TREE — Godot's AnimationNodeBlendTree. Where a BlendSpace (BlendSpace.hpp) mixes a flat set of animations by one parameter and a state machine (AnimStateMachine.hpp) cross-fades whole states, a blend tree is the graph that NESTS them: a node reads named blend parameters and combines the poses of its child nodes, so you can build "a walk/run blend space, cross-faded into a jump by an air parameter, with an additive upper-body wave layered on top" as one evaluable tree.  A `Pose` is a vector of per-joint local transforms (anim::JointPose). Leaf `Input` nodes pull a pose out of an external table you pass to evaluate() (in a real rig those are sampled clips); the interior nodes blend them. Everything is pure pose math — no GPU, no clip sampling here — so the tree unit-tests headlessly and stays deterministic under the fixed timestep.

**Types:** `BlendTree`

### `CubicBezierEasing`
<sub>`engine/include/maz/anim/CubicBezierEasing.hpp`</sub>

maz::anim CUBIC-BEZIER EASING — arbitrary motion curves defined exactly like CSS `cubic-bezier(x1,y1,x2,y2)` and the browser `ease`/`ease-in`/`ease-out`/`ease-in-out` presets. The engine already has a fixed menu of named easings (Transition/Tween); this lets a designer dial in ANY curve by placing the two control handles, then evaluate it per-frame to drive a tween, a UI transition, or a camera move. Endpoints are fixed at (0,0)→(1,1); the input is progress `t` in [0,1] (the curve's x), and the output is the eased value (its y). Solved with the standard Newton–Raphson-then-bisection root find on x (WebKit's UnitBezier method). Header-only, deterministic.  Scope note (honest): x1/x2 are clamped to [0,1] so x stays monotonic (a well-defined function); y1/y2 are unclamped, so springy curves may intentionally overshoot below 0 or above 1 (as in CSS).

**Types:** `CubicBezierEasing`

**Functions:**

- `inline CubicBezierEasing easeCurve()`
- `inline CubicBezierEasing easeInCurve()`
- `inline CubicBezierEasing easeOutCurve()`
- `inline CubicBezierEasing easeInOutCurve()`

### `Curve`
<sub>`engine/include/maz/anim/Curve.hpp`</sub>

Curve — Godot's Curve resource: a keyframed 1-D function y = f(x), sampled over a domain (usually [0,1]), that drives value-over-time / value-over-parameter effects — particle size or alpha over lifetime, an audio fade, a custom easing shape, a difficulty ramp. This is NOT math::Curve2D (a Bézier *path* through 2D space); this maps one scalar to another. Points carry per-point left/right TANGENTS (slopes) so the Cubic mode is a smooth Hermite spline; Linear and Constant modes ignore tangents. Results are clamped to [minValue, maxValue]. Pure math, header-only, deterministic — it unit-tests exactly and drives a golden.

**Types:** `CurvePoint`, `Curve`

### `Gradient`
<sub>`engine/include/maz/anim/Gradient.hpp`</sub>

Gradient — Godot's Gradient resource: a colour ramp defined by sorted (offset, colour) stops and sampled over a parameter (usually [0,1]). It is the colour analogue of `anim::Curve` (which maps a scalar), and it drives colour-over-lifetime for particles, health/heat tints, sky ramps, minimap legends, and bakes into a GradientTexture. Three interpolation modes match Godot's Gradient.InterpolationMode: Constant (hard bands — the lower stop's colour holds until the next), Linear (straight per-channel blend), and Cubic (a Catmull-Rom spline through the neighbouring stops for smooth, slightly overshooting transitions). Below the first stop it returns the first colour, above the last the last colour (Godot's clamped domain). Pure data + math, header-only, deterministic — it unit-tests exactly and drives a golden.

**Types:** `GradientStop`, `Gradient`

### `IK`
<sub>`engine/include/maz/anim/IK.hpp`</sub>

2-bone inverse kinematics — Godot's SkeletonModification2DTwoBoneIK. Given a fixed root joint, two bone lengths (upper `len1`, lower `len2`), and a target, solve for the middle joint (elbow/knee) and the end effector so the chain reaches the target. Uses the law of cosines: the angle at the root between the root->target line and the upper bone is acos((len1²+d²-len2²)/(2·len1·d)). `bendSign` (+1 / -1) picks which side the elbow bends to. When the target is out of reach the chain points straight at it, fully extended. Pure 2D math — no skeleton, no GPU — so it unit-tests headlessly.

**Types:** `IKResult`

**Functions:**

- `inline IKResult solveTwoBoneIK(math::vec2 root, float len1, float len2, math::vec2 target,`
- `inline void solveFabrik(std::vector<math::vec2>& joints, math::vec2 target, int iterations = 10,`

### `RootMotion`
<sub>`engine/include/maz/anim/RootMotion.hpp`</sub>

Root motion — Godot's AnimationMixer root-motion track. A locomotion clip (walk, run, roll) that actually TRAVELS bakes the character's displacement into a "root" bone; without root motion you play the clip in place and move the character with a separate hand-tuned velocity, and the feet slide whenever the two disagree. Root motion instead READS the travel back out of the clip and hands it to the character each frame, so the body moves exactly as far as the animation says — no foot sliding.  This is the reusable core: a track of the root's cumulative planar POSITION and HEADING over the clip. `delta` returns how far the root moved between two clip times (with one-loop wrap-around), and `advance` applies that step to a world pose — rotating the clip-local displacement by the character's current facing (so "walk forward" goes wherever the character faces) and accumulating the turn. Pure planar math (the XZ-plane + Y-yaw reduction Godot uses for characters), so it unit-tests headless and stays deterministic under the fixed timestep. Heading is stored UNWRAPPED (a cumulative path integral), so a clip may turn any amount and deltas never need angle-wrap fixups.

**Types:** `RootMotionSample`, `RootMotionKey`, `RootMotionTrack`

### `Skeleton`
<sub>`engine/include/maz/anim/Skeleton.hpp`</sub>

Skeletal-animation core: a joint hierarchy plus the matrix math that turns an animated pose into the per-joint "skinning matrices" a mesh is deformed by. This is the engine-agnostic heart of character animation; a renderer can apply the result on the GPU (a joint-matrix UBO + skinned vertex shader) or, as the demo does, skin vertices on the CPU and stream them through the dynamic- mesh path — so it needs no new vertex format and can't regress the existing mesh pipeline.  Convention: joints are stored parents-before-children (topological order), each with a parent index (-1 for a root) and a rest-pose LOCAL transform. From those, global bind transforms and their inverses are precomputed. Header-only, pure math — unit-tests without a GPU.

**Types:** `Joint`, `Skeleton`

### `SpringBone`
<sub>`engine/include/maz/anim/SpringBone.hpp`</sub>

maz::anim::SpringBone — secondary motion for a chain of bones: tails, hair, antennae, ponytails, capes, dangling accessories, and anything that should JIGGLE and trail as the character moves rather than stay rigidly rigged. You drive the ROOT joint each frame (attach it to a real bone), and the rest of the chain follows with inertia and damping — lagging behind sudden motion, overshooting, then settling back into the rest pose. Each joint is a damped spring pulled toward where it would be if rigidly attached to its parent, and because a joint chases its parent's CURRENT (also-lagging) position, the motion propagates down the chain like a whip. Godot exposes this as its SpringBoneSimulator / jiggle modifiers; the engine had core::Spring (a single scalar/vector spring) and SoftBody (full physics) but no bone-chain jiggle. Stepped with sub-stepping so it stays stable at any frame rate. Header-only, pure, deterministic.

**Types:** `SpringBone`

### `SpriteAnim`
<sub>`engine/include/maz/anim/SpriteAnim.hpp`</sub>

Sprite-sheet (flipbook) animation: play a sequence of UV sub-rectangles on a single texture over time — walk cycles, explosions, idle bobs, UI spinners. Pairs with SpriteDesc's uvMin/uvMax so a frame is drawn by copying the current SpriteFrame into those fields. Pure logic (frame timing), so it advances deterministically under the fixed timestep and unit-tests without a GPU.

**Types:** `SpriteFrame`, `SpriteAnim`

**Functions:**

- `inline std::vector<SpriteFrame> gridFrames(int cols, int rows, int first, int count)`

### `Timeline`
<sub>`engine/include/maz/anim/Timeline.hpp`</sub>

---- Keyframe timeline / sequencer ------------------------------------------------------------- Godot's AnimationPlayer in miniature: an animation is a set of named TRACKS, each a list of KEYFRAMES (time -> value) that are interpolated between, and a PLAYHEAD that advances over the clip's length with a loop policy. Where a `Tween` (see Tween.hpp) animates ONE value from A to B, a `Timeline` animates MANY named properties through arbitrary keyed poses at once — the backbone of cutscenes, UI transitions, and property animation. Pure math (no GPU/allocation beyond the key vectors), so it unit-tests headless and samples identically every frame under the fixed timestep.

**Types:** `Keyframe`, `Track`, `Timeline`

### `Transition`
<sub>`engine/include/maz/anim/Transition.hpp`</sub>

maz::anim complete easing/transition set — Godot's Tween transition matrix. The existing anim::Tween (M59) carries a handy but partial easing enum; this adds the FULL Godot model: 12 transition types x 4 ease types, computed with the Robert Penner equations Godot uses, so a Maz tween can reproduce any Godot ease exactly. All curves are normalised to t in [0,1] -> value (0->0, 1->1); Back/Elastic/Spring deliberately overshoot for anticipation/springy motion. Pure, header-only, deterministic — unit-tests exactly against known values and endpoint/symmetry laws.

**Types:** `TransitionType`

**Functions:**

- `inline float bounceOutEq(float t)`
- `inline float springOutEq(float t)`
- `inline float easeIn(TransitionType type, float t)`
- `inline float applyTransition(TransitionType type, EaseType ease, float t)`
- `inline float tweenInterpolate(float from, float to, float elapsed, float duration,`

### `TriggerTrack`
<sub>`engine/include/maz/anim/TriggerTrack.hpp`</sub>

Call-method / trigger tracks — the other half of Godot's AnimationPlayer (M100's Timeline gave VALUE tracks that interpolate a property; this gives METHOD tracks that FIRE at a keyframe time). A trigger track is a list of timed markers; as a playhead sweeps across the clip each marker fires EXACTLY ONCE when the head passes it — the hook a clip uses to play a footstep sound on the plant frame, spawn a muzzle flash on the shoot frame, or open a gate at the end of a cutscene. Pure timing math (no GPU), deterministic under the fixed timestep, so it unit-tests headless.

**Types:** `Trigger`, `TriggerTrack`, `MethodTimeline`

### `Tween`
<sub>`engine/include/maz/anim/Tween.hpp`</sub>

Easing + tweening: the engine's general-purpose "animate a value from A to B over time" toolkit. Cross-cutting on purpose — the same curves drive UI transitions, moving platforms, doors, camera moves, color fades, and gameplay juice. Pure math (no GPU/allocation), so it unit-tests headless and stays deterministic under the fixed timestep.

**Types:** `Ease`, `Loop`, `Tween`

**Functions:**

- `inline float bounceOut(float t)`
- `inline float ease(Ease type, float t)`
- `inline T mix(const T& a, const T& b, float t)`
- `inline float mix(float a, float b, float t)`

### `TweenPlayer`
<sub>`engine/include/maz/anim/TweenPlayer.hpp`</sub>

Tween sequencer / property animator — Godot's SceneTreeTween (create_tween + tween_property/ tween_interval/tween_callback + parallel + set_loops). anim::Tween is a single time-cursor that interpolates ONE from→to over one duration; this composes many of those into a CHOREOGRAPHY: a list of steps that run one after another, any of which may run in PARALLEL with its neighbours, with delays and callbacks interleaved and the whole thing optionally looping. Each property step is bound to a value (a `void(float)` setter) that it writes every update, so one player animates a dot's x, then its y, while a second grows its radius — all advanced by a single update(dt). Pure logic (no GPU/allocation beyond the step list), so it unit-tests headlessly and runs deterministically under the fixed timestep.  Model (mirrors Godot): the player holds an ordered list of GROUPS; groups run sequentially, and the tweeners inside a group run in parallel. `append*` starts a new group; `parallel*` adds to the current (last) group. A group's duration is its longest tweener; a tweener shorter than its group holds at its end value for the remainder.

**Types:** `Tweener`, `TweenPlayer`


<a name="audio"></a>
## Audio — mixer, DSP effects, spatialization, synthesis

### `Audio`
<sub>`engine/include/maz/audio/Audio.hpp`</sub>

**Types:** `SoundDesc`, `Audio`

### `BusGraph`
<sub>`engine/include/maz/audio/BusGraph.hpp`</sub>

Audio bus graph — Godot's AudioServer bus layout. Every voice in a game plays into a named BUS ("Music", "SFX", "Voice", ...), and each bus carries a volume (in dB), mute / solo / bypass switches, an ordered chain of effects, and a SEND that routes its processed output into another bus. The buses form a forest rooted at "Master" (bus 0), whose output is what reaches the speakers — so you can drop a reverb on a "Reverb" bus and send several buses into it, duck all SFX with one fader, or solo the music while mixing. Maz already had a single linear effect chain (`Bus`); this is the multi-bus router on top of it. Pure per-sample math over the existing Effect chain — no device, no threads — so it unit-tests exactly (a −6 dB bus halves its signal; muting silences everything routed through it; solo keeps only the soloed bus's path to Master) and drives a golden mixer view.  Processing model: push each source sample into its bus's input (`pushInput`), then call `process()` once per output sample. Buses are processed deepest-first so a bus sees all of its child sends before its own effects run; each bus applies its effects (unless bypassed), its dB gain, and its audibility (mute / solo), then adds the result into its send target's input. `process()` returns the Master output and clears the per-sample input accumulators.  Scope note (honest): this is a MONO router — the routing, gain, mute/solo, bypass, and send semantics match Godot, but stereo bus processing (and stereo-aware effects) arrives with the stereo-effects milestone. A per-bus output level meter is exposed for metering/visualization.

**Types:** `BusGraph`

### `Dsp`
<sub>`engine/include/maz/audio/Dsp.hpp`</sub>

---- DSP effects + mix buses ------------------------------------------------------------------- Godot's AudioServer routes every voice through a BUS, and each bus carries an ordered chain of EFFECTS (filter, delay, reverb, ...). Maz's mixer was a flat sum with no per-bus processing. This adds the reusable, testable core: RBJ-cookbook biquad filters, a feedback delay (echo), and a `Bus` that chains effects in series with an output gain. Pure per-sample math — no device, no threads — so it unit-tests exactly and drives a deterministic offline waveform golden, and a real-time mixer can consume it unchanged (process one sample, or a whole buffer, through the chain).

**Types:** `Amplify`, `Biquad`, `Delay`, `Comb`, `Allpass`, `Reverb`, `Distortion`, `MultiDistortion`, `Compressor`, `Limiter`, `Lfo`, `Chorus`, `Flanger`, `Phaser`, `Equalizer`, `Effect`, `BiquadEffect`, `DelayEffect`, `ReverbEffect`, `DistortionEffect`, `DistortionModeEffect`, `CompressorEffect`, `LimiterEffect`, `ChorusEffect`, `FlangerEffect`, `PhaserEffect`, `AmplifyEffect`, `EqualizerEffect`, `Bus`

**Functions:**

- `inline float dbToLinear(float db)`
- `inline float linearToDb(float linear)`
- `inline float fracTap(const std::vector<float>& line, std::size_t head, float delaySamples)`

### `Envelope`
<sub>`engine/include/maz/audio/Envelope.hpp`</sub>

ADSR envelope — the amplitude contour every synth voice is shaped by. When a key goes down (note-on) the level ramps 0 → 1 over ATTACK, falls to the SUSTAIN level over DECAY, then holds there for as long as the key is held; when the key is released (note-off) it ramps from wherever it is down to 0 over RELEASE. Multiplying a raw oscillator by this level turns a flat buzz into a note with a shape — a plucky blip, a slow-swelling pad, a percussive stab. Times are in seconds, the sustain level in [0,1]. It's a tiny gated state machine advanced by process(dt); pure scalar math, deterministic, so it unit-tests headless and a synth callback just multiplies each sample by process(1/sampleRate).

**Types:** `ADSR`

### `EnvelopeFollower`
<sub>`engine/include/maz/audio/EnvelopeFollower.hpp`</sub>

maz::audio envelope follower + level metering — track the moment-to-moment loudness of a signal.  A huge amount of audio behaviour keys off "how loud is this right now?": compressors and gates decide when to clamp, sidechain ducking lowers the music under a voice, a VU/peak meter drives a UI, auto-wah and envelope-driven filters sweep with the amplitude, and onset/beat detection watches the envelope for jumps. The raw waveform swings +/- many times per cycle, so you cannot read level off it directly; an envelope follower smooths the rectified/squared signal with separate ATTACK (how fast it rises to a louder level) and RELEASE (how slowly it falls back) time constants — the classic one-pole detector. Peak mode follows |x|; RMS mode follows sqrt(mean of x^2), the perceptually-truer "energy" level. Plus block helpers rms()/peakLevel() for one-shot metering. Pure CPU, header-only, deterministic — unit-tested against the exact one-pole step response (1 - 1/e of the target after one time constant).

**Types:** `EnvelopeFollower`

**Functions:**

- `inline float rms(const float* x, int n)`
- `inline float rms(const std::vector<float>& x)`
- `inline float peakLevel(const float* x, int n)`
- `inline float peakLevel(const std::vector<float>& x)`

### `G711`
<sub>`engine/include/maz/audio/G711.hpp`</sub>

maz::audio G.711 (ITU-T) μ-law / A-law companding codec — the compression behind virtually all digital telephony and a great deal of voice/VoIP audio, and a common `.wav` payload format (WAVE format tags 7 and 6). G.711 "compands" a 16-bit PCM sample down to a single byte using a logarithmic curve, so quiet sounds keep their detail while loud ones lose the least-significant bits you can't hear anyway — a fixed 2:1 compression at telephone quality, decode-anywhere with no tables to ship. This is the exact ITU-T reference companding: μ-law (used in North America/Japan) and A-law (used in Europe/international). Byte-in / byte-out around the engine's `audio::WavData` (float [-1,1]) via int16, so it unit-tests headlessly by an encode→decode round-trip whose error matches the standard's quantization, plus the spec's exact anchor values (μ-law encodes silence to 0xFF).

**Functions:**

- `inline std::uint8_t linearToMuLaw(int sample)`
- `inline int muLawToLinear(std::uint8_t mu)`
- `inline std::uint8_t linearToALaw(int sample)`
- `inline int aLawToLinear(std::uint8_t a)`
- `inline int floatToS16(float s)`
- `inline std::uint8_t encodeMuLawSample(std::int16_t pcm)`
- `inline std::int16_t decodeMuLawSample(std::uint8_t mu)`
- `inline std::uint8_t encodeALawSample(std::int16_t pcm)`
- `inline std::int16_t decodeALawSample(std::uint8_t a)`
- `inline std::vector<std::uint8_t> encodeMuLaw(const WavData& in)`
- `inline WavData decodeMuLaw(const std::uint8_t* d, std::size_t n, std::uint16_t channels = 1,`
- `inline WavData decodeMuLaw(const std::vector<std::uint8_t>& b, std::uint16_t channels = 1,`
- _…and 3 more_

### `Goertzel`
<sub>`engine/include/maz/audio/Goertzel.hpp`</sub>

maz::audio Goertzel single-frequency detector — measure how much of ONE specific frequency is present in a block of samples, far cheaper than a full FFT when you only care about a handful of target tones. The Goertzel algorithm evaluates a single DFT bin with a tiny two-tap recurrence, so it is the classic tool for DTMF/touch-tone decoding, detecting a whistle or reference pitch, a cheap guitar-tuner bin, or watching a couple of alarm/marker frequencies in a stream without paying for a whole spectrum. Returns the complex bin (real/imag) and its magnitude; the magnitude of a pure sine of amplitude A landing on an integer bin is A*N/2 (the DFT convention). Godot's only frequency tool is the full AudioEffectSpectrumAnalyzer. Uses double internally for accuracy. Header-only, std-only, deterministic.

**Types:** `GoertzelBin`

**Functions:**

- `inline GoertzelBin goertzelBin(const std::vector<float>& x, float k)`
- `inline float goertzelMagnitude(const std::vector<float>& x, float k)`
- `inline float goertzelMagnitudeHz(const std::vector<float>& x, float targetHz, float sampleRate)`

### `ImaAdpcm`
<sub>`engine/include/maz/audio/ImaAdpcm.hpp`</sub>

maz::audio IMA ADPCM — the classic 4-bit Adaptive Differential PCM codec (Interactive Multimedia Association / DVI), the one behind WAV format tag 0x11 and the sound banks of countless games. It squeezes 16-bit PCM down to 4 bits per sample — a flat 4:1 compression — by storing, per sample, only a 4-bit code for the DIFFERENCE from a running prediction, with an adaptive step size that grows on loud passages and shrinks on quiet ones. Decode is a handful of adds and shifts per sample (no tables of multiplies, no floating point), so it is cheap enough to decode hundreds of voices on any CPU. This complements the engine's other audio codecs: QOA (higher quality, ~3.2 bits/sample), G.711 (telephony 8-bit companding), and raw WAV — ADPCM is the tiny-and-fast option for short SFX where 4:1 with graceful quality is exactly right. The stream stores the first sample verbatim (so it is reproduced EXACTLY) plus the initial step index, then two 4-bit codes per byte. Header-only, std-only, deterministic. Godot has no ADPCM codec.

**Types:** `AdpcmState`

**Functions:**

- `inline const int* imaStepTable()`
- `inline const int* imaIndexTable()`
- `inline int clampSample(int v)`
- `inline int clampIndex(int i)`
- `inline std::uint8_t adpcmEncodeSample(AdpcmState& st, std::int16_t sample)`
- `inline std::int16_t adpcmDecodeSample(AdpcmState& st, std::uint8_t code)`
- `inline std::vector<std::uint8_t> encodeImaAdpcm(const std::vector<std::int16_t>& pcm)`
- `inline std::vector<std::int16_t> decodeImaAdpcm(const std::vector<std::uint8_t>& data, std::size_t sampleCount)`

### `KarplusStrong`
<sub>`engine/include/maz/audio/KarplusStrong.hpp`</sub>

maz::audio — Karplus-Strong plucked-string synthesis. A startlingly simple recipe that produces convincing plucked/struck string tones (guitar, harp, koto, a twangy UI blip) with no samples: fill a short delay line with a burst of noise (the "pluck"), then repeatedly play it back while averaging each pair of adjacent samples. The averaging is a gentle low-pass that shaves the high frequencies a little more on each pass, so the bright noisy attack mellows into a decaying harmonic tone whose PITCH is set by the delay-line length (frequency = sampleRate / length). It is the classic physical-modelling synthesis method — cheap enough to run per-note at runtime for procedural instruments and impact sounds. Godot has oscillators/samples but no string model. Header-only, std-only, deterministic (seeded noise burst).

**Functions:**

- `inline std::vector<float> karplusStrongPluck(float frequency, int sampleRate, int sampleCount, float decay,`

### `Mp3`
<sub>`engine/include/maz/audio/Mp3.hpp`</sub>

maz::audio MPEG audio (MP3) frame parsing + seek index — the demux/metadata half of MP3 support: locate every MPEG audio frame in a buffer, read its header (version, layer, bitrate, sample rate, channels), compute frame lengths, skip a leading ID3v2 tag, and total up samples/duration for a seek table. This is the layer a player runs BEFORE decoding: you cannot decode or seek an MP3 without first framing it, and duration/seek metadata is what most apps need first.  Honest scope: this is the container/framing + metadata layer, not the Layer III audio codec. The heavy DSP that turns frame payloads into PCM samples (Huffman decode, IMDCT, the synthesis filterbank) is a separate, large, patent-adjacent step — the engine already ships dependency-free compressed audio via QOA and uncompressed via WAV, so playback is not blocked on it. Everything here is exact per the MPEG-1/2/2.5 Layer I/II/III specification and unit-tested against hand-constructed frame headers, so it is fully verifiable headlessly.

**Types:** `Mp3FrameHeader`, `Mp3Info`

**Functions:**

- `inline int bitrateKbps(int tableId, int index)`
- `inline int sampleRateHz(MpegVersion v, int index)`
- `inline Mp3FrameHeader parseMp3FrameHeader(const uint8_t* p, size_t avail)`
- `inline size_t id3v2Size(const uint8_t* p, size_t n)`
- `inline Mp3Info scanMp3(const uint8_t* data, size_t n)`
- `inline Mp3Info scanMp3(const std::vector<uint8_t>& bytes)`

### `MusicScales`
<sub>`engine/include/maz/audio/MusicScales.hpp`</sub>

maz::audio SCALES & CHORDS — build the note sets procedural music needs from a root note: a scale (major, the modes, pentatonics, blues, whole-tone, chromatic) or a chord (triads, sevenths, sus, extensions). Returns MIDI note numbers you feed to `midiToFrequency` (MusicTheory.hpp) → `audio::Oscillator` for an arpeggiator, a generative melody line, or chord stabs. The interval tables are the canonical semitone offsets from the root. Header-only, std-only, deterministic. Complements the note↔pitch conversions; together they're a small music-theory toolkit the audio module previously lacked.

**Types:** `Scale`, `Chord`

**Functions:**

- `inline std::vector<int> scaleIntervals(Scale s)`
- `inline std::vector<int> chordIntervals(Chord c)`
- `inline std::vector<int> scaleNotes(int rootMidi, Scale s, int octaves = 1)`
- `inline std::vector<int> chordNotes(int rootMidi, Chord c)`

### `MusicSequencer`
<sub>`engine/include/maz/audio/MusicSequencer.hpp`</sub>

Interactive / adaptive music — Godot's AudioStreamInteractive + AudioStreamPlaylist. Game music is not one long file: it is a set of SEGMENTS (intro, explore, combat, boss) that the game switches between as the action changes, and the switch has to happen MUSICALLY — on the next beat or the next bar, with an optional crossfade — or it sounds like a needle scratch. This sequencer is the scheduler that makes that clean: each segment carries a tempo (BPM) and a bar length, one plays, and `transitionTo` queues the next one with a mode (Immediate / AtNextBeat / AtNextBar / Crossfade). It runs on a sample clock and, at any instant, reports which segment(s) are audible and at what gain — so a mixer just multiplies the two candidate streams by those gains. Pure timing + gain math (no decoding), deterministic, so the beat/bar boundaries and equal-power crossfade unit-test to the exact sample and drive a golden timeline.  The scheduler owns only timing and gains; the caller supplies the actual audio for each segment index. A crossfade is equal-power (outGain = cos, inGain = sin over the fade), so the summed loudness stays roughly constant through the transition.

**Types:** `MusicSegment`, `MusicMix`, `MusicSequencer`

### `MusicTheory`
<sub>`engine/include/maz/audio/MusicTheory.hpp`</sub>

maz::audio MUSIC THEORY — the note ↔ pitch conversions procedural music and synth voices need but the audio module lacked. Convert a MIDI note number to a frequency in Hz (12-tone equal temperament, A4 = MIDI 69 = 440 Hz) and back, parse scientific-pitch note names ("A4", "C#5", "Bb3", "C-1") to MIDI numbers, and render a MIDI number back to a name. Feed the result straight into `audio::Oscillator` / a `Sound`'s `freq`, or drive an arpeggiator / `MusicSequencer`. Header-only, std-only, deterministic.  Scope note (honest): standard 12-TET at A4=440 Hz with C4 = middle C = MIDI 60 (scientific pitch notation); `noteNameToMidi` accepts one optional '#'/'b' accidental and validates the result to the MIDI range [0,127], returning -1 otherwise. Alternate tunings / microtonality are out of scope.

**Functions:**

- `inline float midiToFrequency(int midi)`
- `inline double frequencyToMidi(double hz)`
- `inline int noteNameToMidi(const std::string& name)`
- `inline std::string midiToNoteName(int midi)`
- `inline float noteNameToFrequency(const std::string& name)`

### `Noise`
<sub>`engine/include/maz/audio/Noise.hpp`</sub>

maz::audio noise-colour generators — the coloured noise sources procedural sound design leans on. WHITE noise (flat spectrum) is the raw "static" hiss; PINK noise (1/f, equal energy per octave) is the natural, balanced "shhh" of steady rain, a waterfall, ocean surf, or a ventilation hum, and the reference signal audio engineers test systems with; BROWN / red noise (1/f², even more low-end) is the deep rumble of distant thunder, heavy wind, or a rocket. The engine's Oscillator already had a white source baked in for its waveform enum; this exposes all three colours as small, reusable, DETERMINISTIC generators (each seeded from an xorshift PRNG) that a mixer, SFX synth, or wind/ambience layer can pull samples from. Output is in roughly [-1, 1]. Header-only, std-only, per-sample — unit-testable to the bit.

**Types:** `WhiteNoise`, `PinkNoise`, `BrownNoise`

### `Oscillator`
<sub>`engine/include/maz/audio/Oscillator.hpp`</sub>

Oscillator / procedural tone generator — Godot's AudioStreamGenerator source material. Where the rest of the audio module PROCESSES incoming sound, this GENERATES it: the raw waveforms a synth voice is built from — sine, sawtooth, square/pulse, triangle, and white noise — at a chosen frequency, plus a detune ratio, phase modulation input (for FM), and a two-operator FM voice. The catch with naive digital saw/square is ALIASING: their sharp edges contain harmonics above the Nyquist limit that fold back as inharmonic "grit". This uses PolyBLEP (polynomial band-limited step) to round those edges so the saw and square stay clean across the musical range — the same anti-aliasing real soft-synths use. Pure per-sample math, deterministic (the noise source is a seeded xorshift), so it unit-tests exactly (a sine hits 0,1,0,-1 at quarter phases; the band-limited saw's edge jump is softened below the naive 2.0 step) and drives a golden waveform gallery.  Scope note (honest): saw and square are PolyBLEP band-limited (the aliasing-prone shapes); sine is exact and triangle is the direct piecewise form (its harmonics roll off as 1/n^2, so its aliasing is minor). A full wavetable-with-mip synthesis path and higher-order BLAMP triangle correction are natural follow-ups.

**Types:** `Oscillator`, `FMVoice`

**Functions:**

- `inline float polyBlep(float t, float dt)`

### `PitchDetect`
<sub>`engine/include/maz/audio/PitchDetect.hpp`</sub>

maz::audio monophonic pitch detection — the YIN algorithm (de Cheveigné & Kawahara, 2002).  Given a block of mono audio samples, estimate the fundamental frequency (the perceived pitch). This is what a guitar/vocal TUNER, a rhythm game that scores sung or played notes, auto-harmony, and voice-driven mechanics all need. Naive autocorrelation famously "octave-errors" — it locks onto a harmonic instead of the fundamental. YIN fixes that with a cumulative-mean-normalised difference function plus an absolute threshold, then refines the estimate to sub-sample precision with parabolic interpolation. It is monophonic (one note at a time), CPU-only, and deterministic — so it unit-tests headlessly against synthesised tones of known frequency, including harmonic-rich ones that trip up autocorrelation. Header-only.

**Types:** `PitchResult`

**Functions:**

- `inline PitchResult detectPitchYin(const float* samples, int n, float sampleRate, float threshold = 0.15f,`
- `inline PitchResult detectPitchYin(const std::vector<float>& samples, float sampleRate,`

### `PitchShifter`
<sub>`engine/include/maz/audio/PitchShifter.hpp`</sub>

Pitch shifter — Godot's AudioEffectPitchShift. Raises or lowers the pitch of a signal WITHOUT changing its speed (a monster voice an octave down, a chipmunk an octave up, a pickup jingle nudged up a few semitones). This is the classic time-domain GRANULAR / overlap-add shifter: recent input is kept in a ring buffer and read back through TWO overlapping "grains" whose read pointer moves at the pitch ratio relative to the write pointer; the two grains are crossfaded with a Hann window so that as one grain runs off the end of the buffer it fades out while the other (half a window out of phase) fades in — keeping the output continuous and the buffer from over/under-running. Cheaper and more deterministic than a phase vocoder. Mono float->float, so it drops straight into a Bus/Effect chain (PitchShiftEffect). Deterministic -> it unit-tests (unity ratio passes through delayed; a shifted sine's dominant period scales by the ratio) and drives a golden.  Scope note (honest): a granular shifter trades some quality for simplicity — on very wide shifts or transient-heavy material it has mild warble/smearing (as Godot's does). A phase-vocoder or formant-preserving path is the higher-fidelity follow-up.

**Types:** `PitchShifter`, `PitchShiftEffect`

### `Qoa`
<sub>`engine/include/maz/audio/Qoa.hpp`</sub>

maz::audio QOA (Quite OK Audio) codec — a compact lossy audio format that closes the "only WAV" gap in the asset pipeline. WAV is uncompressed (huge on disk); QOA is a fixed, dependency-free format that compresses PCM roughly 3-4x at good quality using a tiny per-channel LMS predictor plus 4-bit-scaled 3-bit residuals, with none of the patent/complexity baggage of MP3/Vorbis. This is a full, spec-accurate encoder AND decoder (the reference algorithm by Dominic Szablewski, ported to header-only C++): `encodeQoa` turns an `audio::WavData` into a `.qoa` byte stream, `decodeQoa` turns those bytes back into `WavData`. Because it is byte-in/byte-out and lossy-but-deterministic, it unit-tests headlessly by an ENCODE→DECODE round-trip: the decoded signal matches the input within QOA's bounded per-sample error — no external reference file needed. Mono or interleaved multi-channel, any sample rate.  Scope note (honest): QOA v1 (magic "qoaf", static frames of up to 5120 samples/channel). Streaming playback hookup is the app's job; this provides the encode/decode.

**Types:** `QoaLms`

**Functions:**

- `inline int qoaLmsPredict(const QoaLms& lms)`
- `inline void qoaLmsUpdate(QoaLms& lms, int sample, int residual)`
- `inline int qoaClamp(int v, int lo, int hi)`
- `inline int qoaClampS16(int v)`
- `inline int qoaDiv(int v, int scalefactor)`
- `inline void putU16Be(std::vector<std::uint8_t>& out, unsigned v)`
- `inline std::vector<std::uint8_t> encodeQoa(const WavData& in)`
- `inline WavData decodeQoa(const std::uint8_t* d, std::size_t n)`
- `inline WavData decodeQoa(const std::vector<std::uint8_t>& bytes)`

### `Randomizer`
<sub>`engine/include/maz/audio/Randomizer.hpp`</sub>

Stream randomizer — Godot's AudioStreamRandomizer. Repetitive one-shots (footsteps, gunshots, impacts, UI blips) sound robotic when the exact same clip plays every time. A randomizer wraps a pool of interchangeable streams and, on each trigger, picks one and jitters its pitch and volume so the ear never hears a mechanical repeat. Three pick modes match Godot: Random (weighted uniform), RandomNoRepeat (weighted, but never the clip that just played — Godot's default), and Sequential (round-robin). Pitch is scaled by a log-symmetric factor in [1/randomPitch, randomPitch] and volume offset by ± a dB range. This is the SELECTION + variance logic only (which clip, what pitch/volume) — it returns a RandomPick the caller feeds to the mixer; it does not itself decode or play audio. Deterministic (seeded core::Random), std-only, so it unit-tests exactly and drives a golden histogram/scatter.

**Types:** `RandomPick`, `StreamRandomizer`

### `SampleMixer`
<sub>`engine/include/maz/audio/SampleMixer.hpp`</sub>

Sample-playback mixer — Godot's AudioStreamPlayer over an AudioStreamWAV. Until now Maz could *decode* a .wav into float samples (audio::Wav) and *synthesize* procedural tones (audio::Audio), but there was no way to take a decoded clip and actually PLAY it back through a mixer — start it as a voice, set its gain and stereo pan, loop it, or pitch-shift it, and have several such voices summed into one output buffer. That runtime is this header. It is a self-contained, offline (buffer-in / buffer-out) stereo mixer: it never touches the SDL audio device, so it unit-tests headlessly and byte-deterministically, yet it feeds exactly the interleaved-float format a real device callback wants. The app (or a future device backend) owns the callback and simply asks the mixer to fill each block.  A voice references a WavData clip by pointer (the caller owns the clip and must outlive the voice). Reads are linearly interpolated so pitch/speed and sample-rate conversion are smooth; a mono clip is panned into both output channels, a stereo clip maps its two channels straight through (with pan attenuating the opposite side). Non-looping voices deactivate automatically when they run past the end.

**Types:** `SampleVoice`, `SampleMixer`

**Functions:**

- `inline float sampleAt(const WavData& clip, double pos, std::uint16_t ch)`

### `Spatial2D`
<sub>`engine/include/maz/audio/Spatial2D.hpp`</sub>

2D positional audio — Godot's AudioStreamPlayer2D. Given a listener (position + a "right" axis) and a sound source position, compute the source's per-channel (left/right) gain: a distance ATTENUATION (the source fades with range) times a constant-power PAN (a source off to one side is louder in that ear). Pure math — no device, no mixer — so it unit-tests headlessly and can drive any backend that accepts a left/right gain per voice (the Maz mixer does, via SoundDesc::leftGain/rightGain).

**Types:** `Attenuation`, `Listener2D`, `StereoGain`

**Functions:**

- `inline float attenuation(float distance, float refDistance, float maxDistance, Attenuation mode)`
- `inline StereoGain spatialize(const Listener2D& listener, math::vec2 source, float baseVolume,`

### `Spatial3D`
<sub>`engine/include/maz/audio/Spatial3D.hpp`</sub>

3D spatial audio — Godot's AudioStreamPlayer3D. Where Spatial2D gives distance attenuation + a left/ right pan on a plane, a 3D source heard by a 3D LISTENER needs three things: (1) distance ATTENUATION with a choice of falloff curve, (2) a stereo PAN derived from where the source sits relative to the listener's ORIENTATION (its forward/up basis), and (3) DOPPLER — the pitch shift when source and listener move relative to each other. All pure math — no device, no mixer — so it unit-tests headlessly and drives any backend that takes a per-voice left/right gain + pitch.

**Types:** `Attenuation3D`, `Listener3D`, `Source3D`, `SpatialMix`, `SpatialConfig`

**Functions:**

- `inline float attenuation3D(float distance, float refDistance, float maxDistance, float rolloff,`
- `inline float panPosition(const Listener3D& l, math::vec3 sourcePos)`
- `inline void equalPowerPan(float pan, float gain, float& left, float& right)`
- `inline float dopplerPitch(const Listener3D& l, const Source3D& s, float speedOfSound = 343.0f)`
- `inline SpatialMix computeSpatialMix(const Listener3D& l, const Source3D& s, const SpatialConfig& cfg)`

### `Spectrum`
<sub>`engine/include/maz/audio/Spectrum.hpp`</sub>

SpectrumAnalyzer — Godot's AudioEffectSpectrumAnalyzer: turn a block of audio samples into a frequency spectrum so a game can react to sound (rhythm games, VU meters / equalizer visualizers, beat-reactive lights and particles, lip-sync). It runs an in-place radix-2 FFT over a windowed, zero-padded frame and exposes the per-bin magnitudes plus `magnitudeForRange(lowHz, highHz)` — the same query Godot's analyzer gives (`get_magnitude_for_frequency_range`) — for band energy (bass / mid / treble meters). Pure DSP maths, header-only, deterministic — it unit-tests exactly (a pure tone peaks on its bin) and drives a golden (a spectrum bar graph).

**Types:** `SpectrumAnalyzer`

**Functions:**

- `inline void fft(std::vector<Cplx>& a, bool inverse)`
- `inline std::size_t nextPow2(std::size_t n)`

### `Stereo`
<sub>`engine/include/maz/audio/Stereo.hpp`</sub>

Stereo processors — Godot's AudioEffectStereoEnhance and AudioEffectPanner. Maz's DSP so far is mono (one float in, one float out); this adds the small stereo-aware layer that games use to place and widen a sound across the two speakers. A StereoFrame is one interleaved L/R sample pair. Two processors operate on it: StereoEnhance controls the perceived WIDTH of the stereo image (from mono at the center out to a wide, enveloping field) via mid/side scaling plus an optional Haas time-offset; and Panner shifts the stereo BALANCE left or right with a constant-power law. Both are pure per-frame math — no device, no threads — so they unit-test exactly (width 0 collapses to mono; a centered balance is unchanged; a hard pan silences one side) and drive a golden goniometer (vectorscope) view.  Mid/side: mid = (L+R)/2 is the mono-compatible center, side = (L-R)/2 is the stereo difference. Scaling `side` by a width factor narrows (<1, toward mono) or widens (>1) the image; a truly mono input (L==R) has zero side, so width cannot invent width that isn't there — it stays mono, which is correct.  Scope note (honest): these are standalone stereo processors + a StereoFrame type. Rewiring the whole BusGraph/effect chain to carry stereo end-to-end (so every effect is stereo-aware) is the larger follow-up; the mono effect path is unchanged.

**Types:** `StereoFrame`, `StereoEnhance`, `Panner`

**Functions:**

- `inline float stereoMid(const StereoFrame& f)`
- `inline float stereoSide(const StereoFrame& f)`
- `inline StereoFrame fromMidSide(float m, float s)`

### `TempoEstimate`
<sub>`engine/include/maz/audio/TempoEstimate.hpp`</sub>

maz::audio tempo (BPM) estimation — find the beat rate of a piece of music from its samples.  Rhythm games, beat-synced visuals/lighting, auto-cut editors, and adaptive music all want to know the tempo. The approach here is the classic one: build an ONSET-STRENGTH signal (how much the short-time energy JUMPS up from one frame to the next — a proxy for "a beat just happened"), then AUTOCORRELATE it. A steady beat makes the onset signal periodic, so its autocorrelation peaks at the beat period; the lag of that peak, refined with parabolic interpolation, converts to BPM. Searches a musical range (default 60-200 BPM). Pure CPU, header-only, deterministic — unit-tested by feeding synthetic click tracks of known tempo and reading the BPM back.  Honest caveat — the OCTAVE ambiguity: a steady beat autocorrelates just as strongly at half and at double its true period, so every onset-autocorrelation tempo detector (this one included) can land on a tempo octave (e.g. 75 for a 150 BPM track). A perceptual log-tempo prior (Gaussian centred on `priorBpm`) biases the pick toward musically-typical tempos and resolves this near the centre; outside the comfortable band the answer is correct to within an octave. This matches how the field evaluates trackers ("Accuracy-2" credits octave-equivalent answers).

**Types:** `TempoResult`

**Functions:**

- `inline TempoResult estimateTempo(const float* samples, int n, float sampleRate, int hop = 512,`
- `inline TempoResult estimateTempo(const std::vector<float>& samples, float sampleRate, int hop = 512,`

### `Wav`
<sub>`engine/include/maz/audio/Wav.hpp`</sub>

WAV load/save — Godot's AudioStreamWAV / the "load a .wav" half of the asset pipeline. Every prior Maz sound was PROCEDURALLY synthesized (audio::SoundDesc); there was no way to read an actual audio file, or to export one. This is a self-contained RIFF/WAVE PCM codec: `decodeWav` parses the exact bytes of a .wav file into float samples, and `encodeWav` writes them back out. It handles the two ubiquitous PCM formats — 8-bit unsigned and 16-bit signed, mono or interleaved multi-channel — which covers the vast majority of game sound assets. Byte-in / byte-out (no file device), so it unit-tests headlessly and the app owns any real disk read/write.

**Types:** `WavData`

**Functions:**

- `inline std::uint16_t rd16(const std::uint8_t* p)`
- `inline std::uint32_t rd32(const std::uint8_t* p)`
- `inline void wr16(std::vector<std::uint8_t>& b, std::uint16_t v)`
- `inline void wr32(std::vector<std::uint8_t>& b, std::uint32_t v)`
- `inline void wrTag(std::vector<std::uint8_t>& b, const char* t)`
- `inline bool tagEq(const std::uint8_t* p, const char* t)`
- `inline bool decodeWav(const std::uint8_t* data, std::size_t size, WavData& out)`
- `inline bool decodeWav(const std::vector<std::uint8_t>& bytes, WavData& out)`
- `inline std::vector<std::uint8_t> encodeWav(const WavData& wav)`

### `Window`
<sub>`engine/include/maz/audio/Window.hpp`</sub>

maz::audio window functions — the tapering envelopes you multiply a block of samples by BEFORE an FFT (or a filter design) so the block's abrupt start/end don't smear energy across the whole spectrum ("spectral leakage"). Multiplying by a window that fades smoothly to zero at both ends turns a rough chunk of audio into a clean, analysable frame — the standard front-end for the engine's SpectrumAnalyzer / FFT (Spectrum.hpp only had an inline Hann), for building FIR filters, and for smooth grain/crossfade envelopes. Provides the classic family — rectangular (none), Hann, Hamming, Blackman, Blackman-Harris, and Bartlett (triangular) — each a symmetric taper peaking at the centre, plus helpers to apply one to a buffer and to report its coherent gain (mean value, the amplitude-scaling a window imposes). Godot exposes no window functions. Header-only, std-only, deterministic.

**Functions:**

- `inline float windowValue(WindowType type, std::size_t n, std::size_t N)`
- `inline void applyWindow(std::vector<float>& buf, WindowType type)`
- `inline float coherentGain(WindowType type, std::size_t N)`


<a name="ui"></a>
## UI — controls, layout, theming, text

### `ColorPicker`
<sub>`engine/include/maz/ui/ColorPicker.hpp`</sub>

maz::ui ColorPicker — the interactive model behind Godot's ColorPicker control (the colour math itself lives in render::ColorOps, added earlier). A ColorPicker is more than an RGBA value: it edits colour in HSV space and must keep the *hue* (and saturation) stable while the user drags a value/saturation slider to an extreme — otherwise dragging brightness to black and back would scramble the hue. So this stores H, S, V, A as the source of truth (matching Godot, whose picker keeps `h`/`s` when a colour becomes grey/black), and derives RGB/hex on demand. It also carries the picker's editing state: an alpha-editing toggle, user preset swatches, and a capped most-recent list. Pure, header-only, deterministic — unit-tests exactly; the widget layer draws the wheel/sliders and calls these setters.

**Types:** `ColorPicker`

### `Container`
<sub>`engine/include/maz/ui/Container.hpp`</sub>

Auto-layout containers — Godot's Container controls (BoxContainer / GridContainer / MarginContainer / CenterContainer). M86's LayoutNode already gives an anchor tree with a *simple* box mode where every `expand` child grabs an equal slice of the leftover space and the cross axis always fills. Godot's real container model is richer, and that richness is what you actually need to build a resizable UI:  * per-axis SIZE FLAGS — a child independently chooses, for its horizontal and its vertical axis, whether to Fill the cell, Expand (grab leftover main-axis space), or Shrink to its minimum and sit at the Begin / Center / End of the cell; * STRETCH RATIOS — two expanding children with ratios 1 and 3 split the leftover 1:3, not 50/50; * a real GRID — N columns, column widths driven by the widest cell in each column, expanding columns sharing the leftover, so a form of label/field pairs lines up; * BOTTOM-UP minimum size — a container reports the min size it needs from its children, so nested containers (a VBox of HBoxes) size correctly.  All pure rectangle math (no renderer, no Font) operating on ui::Rect, so it unit-tests headlessly and the results are deterministic. The app owns the Control structs; each layout call writes their `rect`.

**Types:** `SizeFlag`, `Control`, `Span`

**Functions:**

- `inline Span placeCross(float origin, float extent, float minSize, SizeFlag flag)`
- `inline std::vector<Span> distributeMain(const std::vector<float>& mins,`
- `inline void hbox(const Rect& area, const std::vector<Control*>& kids, float sep = 0.0f)`
- `inline void vbox(const Rect& area, const std::vector<Control*>& kids, float sep = 0.0f)`
- `inline void grid(const Rect& area, const std::vector<Control*>& kids, int columns, float hsep = 0.0f,`
- `inline void margin(const Rect& area, Control& child, float left, float top, float right, float bottom)`
- `inline void center(const Rect& area, Control& child)`
- `inline void hboxMinSize(const std::vector<Control*>& kids, float sep, float& outW, float& outH)`
- `inline void vboxMinSize(const std::vector<Control*>& kids, float sep, float& outW, float& outH)`
- `inline void gridMinSize(const std::vector<Control*>& kids, int columns, float hsep, float vsep,`

### `Controls`
<sub>`engine/include/maz/ui/Controls.hpp`</sub>

maz::ui small stateful controls — the selection/value logic behind Godot's SpinBox, OptionButton, and TabBar. These are pure state models (no rendering): a SpinBox is a Range with step buttons + text format/parse (prefix/suffix), an OptionButton is a drop-down list with a selected item (text + id + disabled), and a TabBar is an ordered tab strip with a current tab and disabled-skipping navigation. Header-only + deterministic, so the behaviour unit-tests exactly; a widget layer draws them.

**Types:** `SpinBox`, `OptionButton`, `TabBar`

### `DebugOverlay`
<sub>`engine/include/maz/ui/DebugOverlay.hpp`</sub>

A small profiling overlay: smoothed FPS + frame time and the renderer's per-frame draw counts, drawn with a Font. Off by default; toggle it (e.g. on F3). Engine-agnostic — the app owns the Font and decides where to place it.

**Types:** `DebugOverlay`

### `DragAndDrop`
<sub>`engine/include/maz/ui/DragAndDrop.hpp`</sub>

maz::ui drag-and-drop — the coordinator behind Godot's Control drag/drop (get_drag_data / can_drop_data / drop_data). A drag begins at a source control carrying a typed payload; while it is in flight, potential targets are asked whether they accept it (a predicate, like can_drop_data); releasing over an accepting target delivers the payload (drop_data) and ends the drag, otherwise the drag continues or is cancelled. This models that state machine + payload; the widget layer supplies the hit-testing and the drag preview. Pure, header-only, deterministic — unit-tests exactly.

**Types:** `DragPayload`, `DragAndDrop`

### `FileDialog`
<sub>`engine/include/maz/ui/FileDialog.hpp`</sub>

maz::ui FileDialog — the model behind Godot's FileDialog control: a file browser with a current directory, a filtered/sorted entry list, name filters ("*.png, *.jpg ; Images"), a filename field, and a file mode (open one/many, pick a directory, or save). To stay pure and deterministic (and to unit-test without touching disk), it takes a *directory lister* callback — the widget/platform layer supplies the real filesystem; here it is any function dir -> entries, so tests inject an in-memory tree. Path joining/normalisation, hidden-file handling, extension filtering, dirs-before-files sorting, single/multi selection, and mode-specific confirmation all live here. Header-only.

**Types:** `FileEntry`, `FileDialog`

### `Font`
<sub>`engine/include/maz/ui/Font.hpp`</sub>

Bitmap font baked from a TTF via stb_truetype into a single atlas texture. Text is drawn as tinted glyph sprites through the Renderer's 2D API, so it works on any renderer backend and respects the active camera (use a pixel-space Camera2D for a screen-fixed HUD).

**Types:** `Font`

### `FontFallback`
<sub>`engine/include/maz/ui/FontFallback.hpp`</sub>

maz::ui font-fallback chain — the "which font can draw this character?" resolver behind mixed-script text. Godot lets a Font carry an ordered list of fallback fonts and picks, per glyph, the first that has the character; a UI drawing Latin + Cyrillic + emoji in one string leans on exactly this. Maz's Font atlas covers ASCII, so anything beyond it needs a fallback chain. This is the reusable core: an ordered set of fonts (by int id), each with the Unicode ranges it covers, plus `fontFor(codepoint)` (first covering font in priority order, or the default) and `runs(text)` which splits a UTF-32 string into contiguous runs that resolve to the same font — the unit a shaper/renderer draws in one pass. Deterministic, std-only, GPU-free, so it unit-tests headlessly; wire real font coverage into it later.

**Types:** `FontFallback`

### `GlyphCache`
<sub>`engine/include/maz/ui/GlyphCache.hpp`</sub>

maz::ui dynamic-font glyph cache — the size-keyed glyph atlas Godot's FontFile maintains for dynamic (TrueType/OpenType) fonts. A font has no fixed bitmaps: each (glyph, pixel-size) is rasterized on first use and cached in a texture atlas, so the same character at the same size is rasterized once and thereafter is a cheap lookup. This models that cache independently of the rasterizer (which is injected), so it unit-tests headlessly: keying by font+codepoint+size, packing new glyphs into the atlas via the skyline packer, returning the atlas rect + metrics, and — when the atlas fills — evicting everything and starting over (glyphs re-rasterize lazily on next access). Wiring a real stb_truetype rasterizer + GPU atlas texture on top is the usual font path; this is the caching/layout brain.

**Types:** `GlyphKey`, `GlyphMetrics`, `GlyphBitmap`, `CachedGlyph`, `GlyphCache`

### `GraphEdit`
<sub>`engine/include/maz/ui/GraphEdit.hpp`</sub>

maz::ui node-graph model — Godot's GraphEdit / GraphNode: the data structure behind visual scripting, the shader graph, and animation/blend trees. Nodes carry named input/output ports and a canvas position; connections wire an output port of one node to an input port of another. This is the pure graph brain: add/remove nodes, connect/disconnect ports with full validation (both endpoints exist, no self-links, no duplicate wires, and — for the acyclic graphs shader/visual-script use — no cycles), plus queries and a Kahn topological ordering for evaluation. No rendering here (the editor draws the boxes and wires); header-only + deterministic, so it unit-tests exactly.

**Types:** `GraphPort`, `GraphNode`, `GraphConnection`, `GraphEdit`

### `ItemList`
<sub>`engine/include/maz/ui/ItemList.hpp`</sub>

ItemList — Godot's ItemList control: a scrollable column of selectable text rows. It backs Godot's FileDialog file list, the animation/audio-bus pickers, inventory and dialogue lists, level-select menus — anywhere a game shows a bounded box of choosable entries. Each row carries text, a caller id, and `selectable`/`disabled` flags. Selection is either Single (picking one clears the rest, like a radio group) or Multi (rows toggle independently). The list has a fixed row height + separation and a vertical `scroll` offset, so it exposes the geometry a renderer needs: itemRect(i) for a row's pixel box, itemAtPoint() to hit-test a click, ensureVisible() / visibleRange() for scrolling. It is pure logic — no GPU, no windowing — so it unit-tests deterministically and a view just draws the rows it reports.

**Types:** `ListItem`, `ItemList`

### `Layout`
<sub>`engine/include/maz/ui/Layout.hpp`</sub>

Retained UI layout — anchors + containers, modeled on Godot's Control system. Maz already had immediate-mode widgets, but every position was a hand-typed pixel coordinate that broke at a different resolution. This adds the missing piece: a layout tree that computes screen rects responsively.  Two ways a node is placed: * Anchors + offsets (Godot's model): anchorMin/anchorMax are fractions [0..1] of the PARENT rect for each edge; L/T/R/B offsets are pixel margins from those anchored points. So (0,0,1,1) with zero offsets fills the parent; (0,0,1,0)+offsets makes a top bar of fixed height that stretches to any width; (0.5,0.5,0.5,0.5) pins a fixed-size box to the center. * Container modes (HBox / VBox / Center) that arrange children automatically: fixed-size children keep their min size, `expand` children share the leftover space, `spacing` sits between them and `pad` insets the container. This is how you build responsive toolbars, lists, and dialogs.  layout() walks the tree from a root rect (usually the framebuffer) and fills every node's `rect`. Non-owning children pointers — the app owns the nodes. Header-only, math-only (no renderer dep).

**Types:** `LayoutNode`

### `PopupMenu`
<sub>`engine/include/maz/ui/PopupMenu.hpp`</sub>

PopupMenu — Godot's PopupMenu: the vertical list of items behind right-click context menus, OptionButton dropdowns, and menu bars. Each item is a label with a caller id and optional check state (a checkbox or a radio button), a disabled flag, an accelerator/shortcut hint, or a submenu arrow; a `separator` item draws a thin divider and is never selectable. The menu stacks items from its `position` at a fixed row height, so it exposes the geometry a view + input need: `rect()` / `itemRect(i)` for drawing, `itemAtPoint()` to hit-test the cursor (rejecting separators and outside points), `hoverNext`/`hoverPrev` for keyboard navigation (skipping separators + disabled rows), and `activate()` to fire the hovered item — toggling a checkbox, switching a radio group, and returning the item id. Pure logic, header-only, deterministic — it unit-tests exactly and drives a golden (an open menu).

**Types:** `MenuItem`, `PopupMenu`

### `Range`
<sub>`engine/include/maz/ui/Range.hpp`</sub>

Range — Godot's Range, the shared value model behind ProgressBar, HSlider/VSlider, ScrollBar, and SpinBox. It holds a scalar `value` clamped to [min, max], optionally snapped to a `step`, and exposes it as a normalized `ratio` in [0,1] — the single number a bar or slider draws from. `page` supports scrollbar-style ranges where a visible window of size `page` means the value can only reach `max-page` (so ratio still spans 0..1). `allowGreater`/`allowLesser` lift the clamp when a field may legitimately exceed its nominal bounds. Pure logic, header-only, deterministic — it unit-tests exactly.

**Types:** `Range`, `ProgressBar`

### `Rect`
<sub>`engine/include/maz/ui/Rect.hpp`</sub>

Screen-space rectangle (pixel coordinates, origin top-left). Shared by the immediate-mode UI (hit-testing) and the retained layout system (computed node rects).

**Types:** `Rect`

### `RichText`
<sub>`engine/include/maz/ui/RichText.hpp`</sub>

BBCode rich-text parser — Godot's RichTextLabel markup. Maz can draw a plain string (ui::Font) and wrap it (ui::layoutText, M134), but there was no way to mix styles WITHIN a string: bold a word, colour a phrase, enlarge a heading. Godot does this with BBCode — `[b]bold[/b]`, `[i]/[u]`, `[color=#ff0000]red[/color]`, `[size=32]big[/size]` — parsed into styled runs a label then lays out. This is that parser: `parseBBCode` turns a tagged string into a flat list of `RichSpan`s (each a substring + its resolved bold/italic/ underline/colour/size), and `stripBBCode` returns the tags-removed plain text. It is deliberately renderer-independent (no Font/Color dependency) so it unit-tests headlessly; the app maps each span's attributes onto its own font draw. Tag handling is lenient like Godot: nested tags stack, an unclosed tag runs to the end, a stray close tag is ignored, `[lb]`/`[rb]` emit literal brackets, and an unrecognized tag is passed through as literal text rather than dropped.

**Types:** `RichSpan`

**Functions:**

- `inline bool parseRichColor(const std::string& v, float& r, float& g, float& b, float& a)`
- `inline std::vector<RichSpan> parseBBCode(const std::string& src)`
- `inline std::string stripBBCode(const std::string& src)`

### `RichTextEffects`
<sub>`engine/include/maz/ui/RichTextEffects.hpp`</sub>

maz::ui RichTextLabel effects — the per-glyph animation maths behind Godot's RichTextLabel BBCode effects ([wave], [tornado], [shake], [fade], [rainbow], [pulse]). The BBCode parser (M141) tags which characters an effect covers; this computes, for a given character index and time, the offset to nudge that glyph by and/or the colour to modulate it with. Each effect is a pure, deterministic function of (index, time, params) so the widget layer just calls it per visible glyph per frame — and it unit-tests exactly. No engine state, header-only.

**Types:** `CharFx`

**Functions:**

- `inline float hash01(std::uint32_t x)`
- `inline float hash01(std::uint32_t a, std::uint32_t b)`
- `inline CharFx rtWave(int index, float t, float amp = 10.0f, float freq = 5.0f)`
- `inline CharFx rtTornado(int index, float t, float radius = 10.0f, float freq = 2.0f)`
- `inline CharFx rtShake(int index, float t, float rate = 20.0f, float level = 5.0f)`
- `inline render::Color rtRainbow(int index, float t, float freq = 1.0f, float sat = 1.0f,`
- `inline float rtFadeAlpha(int index, int start, int length)`
- `inline render::Color rtPulse(int index, float t, const render::Color& base, float freq = 1.0f,`

### `Sdf`
<sub>`engine/include/maz/ui/Sdf.hpp`</sub>

maz::ui SDF — turn a coverage bitmap (a rasterized glyph's alpha) into a Signed Distance Field: per texel, the signed distance to the shape's edge (positive inside, negative outside, ~0 on the contour). An SDF glyph atlas is what lets text stay crisp at ANY scale from one small texture — the fragment shader thresholds the interpolated distance instead of the blurry alpha, so a 32px SDF renders sharp at 8px or 200px (Godot's "MSDF"/SDF font mode; Valve's classic technique).  generateSdf() computes the field with dead reckoning — an O(n) two-pass sweep that tracks each texel's nearest boundary point and recomputes the true Euclidean distance to it, so the result is accurate to well under a texel (validated against a brute-force exact transform in the tests). packSdf() maps the signed field to bytes with 0.5 on the edge and a chosen spread (texels per 0.5 unit), the layout an SDF shader samples. Header-only, deterministic, no GPU — a bake step.

**Functions:**

- `inline std::vector<float> generateSdf(const uint8_t* coverage, int w, int h,`
- `inline std::vector<uint8_t> packSdf(const std::vector<float>& signedField, float spread)`

### `StyleBox`
<sub>`engine/include/maz/ui/StyleBox.hpp`</sub>

---- Nine-patch / StyleBox -------------------------------------------------------------------- Godot draws every themed Panel/Button through a StyleBox — most powerfully a NINE-PATCH: a source image sliced into a 3x3 grid by border insets. When the box is drawn at an arbitrary size the four CORNERS keep their exact size, the four EDGES stretch along one axis, and the CENTER stretches both ways — so a bordered/rounded panel scales to any rectangle without distorting its corner art. This module is the pure mapping (source region -> destination region) behind that; it's dependency-free geometry (no GPU), so it unit-tests headless and a renderer just blits the 9 quads it returns.

**Types:** `Border`, `Patch9`, `Patch`

**Functions:**

- `inline std::array<Patch, 9> ninePatch(const Rect& dst, const Border& border, const Rect& src)`

### `TabContainer`
<sub>`engine/include/maz/ui/TabContainer.hpp`</sub>

maz::ui TabContainer — Godot's TabContainer control: a container that holds several content panels but shows only one at a time, with an integrated strip of tabs to switch between them. Where TabBar (in Controls.hpp) is just the strip, a TabContainer also owns the *content* — each tab carries an opaque content id (the widget layer maps it to the panel/Control to display) and the container guarantees exactly one selectable tab is "current" (or none, when empty/all-unselectable). It adds hidden tabs (not shown in the strip, distinct from disabled tabs which are shown but greyed) and keeps `current` on a selectable tab across disable/hide/remove. Pure, header-only, deterministic.

**Types:** `TabContainer`

### `TextInput`
<sub>`engine/include/maz/ui/TextInput.hpp`</sub>

Single-line editable text — the model behind Godot's LineEdit. Pure logic: no rendering, no input polling, so it unit-tests headlessly. A UI widget draws text() and a caret at position caret(); typed characters come in via insert() and the editing keys drive the caret/erase ops. ASCII/byte caret (one byte == one column), which is what the bundled font renders.

**Types:** `TextField`, `FocusChain`, `TextEditInput`

### `TextLayout`
<sub>`engine/include/maz/ui/TextLayout.hpp`</sub>

Text layout — word-wrapping + alignment for multi-line paragraphs (Godot's Label autowrap + align). The Font renderer can draw a single line and measure its width, but it has no notion of FITTING text into a box: breaking a paragraph across lines at word boundaries so it doesn't overflow, and aligning each line left / center / right within the box. That's what every dialog box, tooltip, description pane, and subtitle needs. This is a pure algorithm — it takes a MEASURE callback (so it has no renderer/Font dependency and unit-tests headlessly) and returns positioned lines the caller then draws with one Font::drawText per line.

**Types:** `TextLine`, `TextLayout`

**Functions:**

- `inline void wrapParagraph(std::string_view para, float maxWidth,`
- `inline TextLayout layoutText(std::string_view text, float maxWidth,`

### `TextServer`
<sub>`engine/include/maz/ui/TextServer.hpp`</sub>

maz::ui text server (subset) — the CPU text-analysis Godot's TextServer performs before shaping: UTF-8 decoding, base-direction detection, bidirectional run segmentation, and line-break opportunities. Real complex-script *shaping* (glyph substitution/positioning, HarfBuzz-class) and the full Unicode Bidirectional Algorithm (UAX #9 with explicit embeddings/isolates and weak-type resolution) are a much larger effort and remain; this implements the widely-needed subset: which direction a paragraph runs, splitting mixed LTR/RTL text into runs, and where a line may wrap (UAX #14 subset — after spaces/hyphens, mandatory at newlines, between CJK ideographs). Pure, header- only, deterministic — unit-tests exactly.

**Types:** `BidiRun`

**Functions:**

- `inline std::u32string decodeUtf8(const std::string& s)`
- `inline bool isRtl(char32_t c)`
- `inline bool isStrongLtr(char32_t c)`
- `inline bool isIdeograph(char32_t c)`
- `inline Direction baseDirection(const std::u32string& text)`
- `inline std::vector<BidiRun> bidiRuns(const std::u32string& text)`
- `inline std::vector<std::size_t> lineBreakOpportunities(const std::u32string& text,`

### `TextShaping`
<sub>`engine/include/maz/ui/TextShaping.hpp`</sub>

maz::ui text shaping — the OpenType GSUB/GPOS step that turns a run of glyph ids into POSITIONED glyphs, the piece complex scripts (and good Latin typography) need beyond the existing word-wrap (ui::layoutText) and bidi/line-break analysis (TextServer.hpp). Three data-driven features model the OpenType tables real fonts carry: * Ligature substitution (GSUB LookupType 4): a contiguous run of glyphs (f, i) collapses to one ligature glyph (fi). Longest match wins, and source-character clusters are merged so hit-testing and caret placement still map back to the original text. * Pair kerning (GPOS LookupType 2): an adjustment to the advance between two specific glyphs (the classic "AV" tuck-in). * Mark-to-base attachment (GPOS LookupType 4): a zero-advance combining mark (an accent, an Arabic/Indic vowel sign) is offset so ITS anchor point coincides with the base glyph's anchor point — the heart of "mark positioning". This is the shaping DATA + engine (what HarfBuzz evaluates), decoupled from any specific font blob: you feed it a ShapingTable (which a font's cmap/GSUB/GPOS would populate) and a glyph run, and it returns advances and x/y offsets. Pure CPU + integer/float math, so it unit-tests headlessly. This is Godot's TextServer shaping layer.

**Types:** `ShapedGlyph`, `ShapeAnchor`, `LigatureRule`, `ShapingTable`

**Functions:**

- `inline std::vector<ShapedGlyph> shapeGlyphs(const std::vector<uint32_t>& glyphs,`
- `inline std::vector<ShapedGlyph> shapeCodepoints(const std::u32string& text, const ShapingTable& table)`
- `inline float shapedWidth(const std::vector<ShapedGlyph>& run)`

### `Theme`
<sub>`engine/include/maz/ui/Theme.hpp`</sub>

StyleBoxFlat + Theme — the other half of Godot's theming (M103 gave the nine-patch StyleBoxTexture). Godot draws almost every default control through a StyleBoxFlat: a solid, ROUNDED-corner rectangle with an optional border and a soft drop shadow, generated procedurally — no texture asset. A Theme then names those styles per control class + state ("Button/normal", "Button/hover", …) so a whole UI restyles from one place. This module is that: dependency-light rounded-rect geometry (unit-testable), a StyleBoxFlat value type + a draw helper that layers shadow → border → fill through the 2D renderer, and a small Theme registry with a default fallback.

**Types:** `Corners`, `StyleBoxFlat`, `Theme`, `ThemeOverrides`

**Functions:**

- `inline std::vector<render::Point2> roundedRectPolygon(const Rect& box, Corners c, int seg = 6)`
- `inline void drawStyleBoxFlat(render::Renderer& r, const Rect& box, const StyleBoxFlat& s, int seg = 6)`
- `inline render::Color resolveColor(const ThemeOverrides& ov, const Theme& theme, const std::string& type,`
- `inline StyleBoxFlat resolveStyleBox(const ThemeOverrides& ov, const Theme& theme, const std::string& type,`
- `inline float resolveConstant(const ThemeOverrides& ov, const Theme& theme, const std::string& type,`

### `Tree`
<sub>`engine/include/maz/ui/Tree.hpp`</sub>

Tree / TreeItem — Godot's hierarchical list control, the backbone of its scene dock, inspector, and FileSystem dock. A TreeItem carries text, an optional id + colour, a `collapsed` flag, and child items; the Tree FLATTENS the currently-expanded items into an ordered list of visible ROWS, each tagged with its depth (for indentation) and whether it has children (so the view draws a fold arrow). Folding a branch hides its whole subtree in one flag. It's a pure data structure + depth-first traversal — deterministic and GPU-free — so it unit-tests headless and a renderer just draws the rows it returns.

**Types:** `TreeItem`, `TreeRow`, `Tree`

### `UI`
<sub>`engine/include/maz/ui/UI.hpp`</sub>

Map a horizontal pointer position within `track` to a value in [minV, maxV], clamped to the ends.

**Types:** `Context`

**Functions:**

- `inline float sliderValueFromX(const Rect& track, float pointerX, float minV, float maxV)`


<a name="fx"></a>
## FX — particles and force fields

### `ForceField2D`
<sub>`engine/include/maz/fx/ForceField2D.hpp`</sub>

Composable 2D force field for particles & gameplay — Godot's GPUParticlesAttractor2D family plus a wind zone and drag, generalized into one reusable resource. The existing fx::ParticleSystem carries a single hard-wired attractor; this is a *set* of attractors/repulsors (each with a position, a signed strength, an influence radius, a falloff curve, and an optional tangential SWIRL for vortices), on top of a uniform directional WIND and a global linear DRAG. It exposes the pure force query `accelAt(pos, vel)` and a deterministic semi-implicit-Euler `step()` integrator over a particle array, so it drives gravity wells, black holes, wind tunnels, and orbiting swarms — and, being pure math with no GPU/RNG, it unit-tests exactly and renders a golden-stable swirl.  Honest scope: this is a CPU point/vector force model. It does NOT implement Godot's texture-baked vector-field attractors, 3D attractors, or the full turbulence-noise process; those remain follow-ups.

**Types:** `Attractor2D`, `FieldParticle`, `ForceField2D`

### `GpuParticles`
<sub>`engine/include/maz/fx/GpuParticles.hpp`</sub>

maz::fx GPU-driven 3D particle system. The particle state lives in flat, struct-of-arrays float buffers laid out exactly as they are uploaded to a GPU storage buffer (SSBO), and the CPU `update()` step mirrors — arithmetic for arithmetic — the compute shader that advances those particles on the GPU: integrate forces (gravity + drag), collide every particle against a set of world planes with restitution + friction, age each particle, and recycle dead ones from the emitter. Because that same math runs here on the CPU, the simulation is headlessly unit-testable (a particle dropped onto a floor must bounce with the right energy and must never sink through the surface) — while on real hardware the identical arithmetic runs in a compute dispatch and the surviving particles are drawn with a single INSTANCED draw call (one quad, N instances), which is how a modern engine reaches hundreds of thousands of particles without per-particle CPU cost.  Honest tag (see docs/GODOT_GAPS_ROADMAP.md): the SIMULATION + PLANE COLLISION below are CPU-verified here and are the exact arithmetic the compute shader performs. The GPU compute dispatch and the instanced draw are wired into the Vulkan renderer and are verified on the owner's machine — this headless box has no GPU, so it proves the math, not the pixels.

**Types:** `ParticlePlane`, `GpuParticleConfig`, `ParticleInstance`, `GpuParticleSystem`

### `ParticleEmitter`
<sub>`engine/include/maz/fx/ParticleEmitter.hpp`</sub>

Particle emitter RESOURCE — Godot's CPUParticles2D. The existing fx::ParticleSystem is a runtime pool that emits point bursts with a linear start->end colour/size; a real emitter is a *resource* you author once and reuse, with (1) an EMISSION SHAPE (point / disk / ring / rectangle), (2) per-lifetime CURVES for scale and alpha (not just two endpoints), and (3) a multi-stop colour GRADIENT over lifetime. This header is that resource plus a DETERMINISTIC simulator: given a seed and a query time it returns every live particle's drawable state, so it unit-tests headlessly and renders a golden-stable snapshot. Pure math — no GPU, no global state — so it composes with any renderer.

**Types:** `Curve`, `Gradient`, `EmitShape`, `Emitter`, `ParticleState`

**Functions:**

- `inline render::Color lerpColor(const render::Color& a, const render::Color& b, float t)`
- `inline math::vec2 sampleOffset(const EmitShape& s, float u, float v)`
- `inline float hash01(uint32_t seed, int index, int channel)`
- `inline std::vector<ParticleState> simulate(const Emitter& e, uint32_t seed, float t)`

### `Particles`
<sub>`engine/include/maz/fx/Particles.hpp`</sub>

Parameters for one burst of particles emitted from a point.

**Types:** `BurstDesc`, `ParticleSystem`


<a name="io"></a>
## IO — JSON, config, serialization, resource packs

### `Base64`
<sub>`engine/include/maz/io/Base64.hpp`</sub>

Base64 — Godot's Marshalls raw_to_base64 / base64_to_raw. The standard way to carry BINARY data through TEXT channels: embed a texture, a save blob, or any byte buffer inside a JSON string, a .tres/.tscn resource, a URL, or a config value. Encoding maps every 3 bytes to 4 ASCII characters (A–Z a–z 0–9 + /) with '=' padding; decoding reverses it, tolerating embedded whitespace/newlines (so wrapped blobs decode) and rejecting stray non-alphabet characters. Standard RFC 4648 alphabet. Header-only, deterministic — it unit-tests exactly against the canonical vectors and drives a golden text readout.

**Functions:**

- `inline std::string base64Encode(const std::uint8_t* data, std::size_t n)`
- `inline std::string base64Encode(const std::vector<std::uint8_t>& data)`
- `inline std::string base64Encode(const std::string& text)`
- `inline int base64Value(char c)`
- `inline bool base64Decode(const std::string& text, std::vector<std::uint8_t>& out)`
- `inline std::vector<std::uint8_t> base64Decode(const std::string& text)`

### `BinaryDiff`
<sub>`engine/include/maz/io/BinaryDiff.hpp`</sub>

maz::io binary diff/patch — build a compact PATCH that turns one byte buffer (the "source") into another (the "target"), and apply it. When two blobs are mostly the same — a save file after a few minutes of play, an asset re-exported with a small tweak, last tick's serialized world versus this tick's — shipping or storing the whole new blob is wasteful. binaryDiff finds the runs the target shares with the source and emits only COPY(from source) + ADD(new bytes) instructions, so a tiny change produces a tiny patch; binaryPatch replays those instructions to reconstruct the target EXACTLY. This is the classic rsync/bsdiff idea (block-hash the source, greedily match the target). It complements the engine's other deltas — net::writeSnapshotDelta does FIELD-level deltas of a known schema; this works on ARBITRARY bytes with no schema at all: patched saves, incremental asset updates, diffing opaque serialized state. The patch is self-contained and bounds-checked on apply (a malformed patch is rejected, never a buffer overrun). Header-only, std-only, deterministic. Godot has no binary diff.

**Functions:**

- `inline std::uint64_t diffHash(const std::uint8_t* p, std::size_t n)`
- `inline std::vector<std::uint8_t> binaryDiff(const std::uint8_t* src, std::size_t srcLen,`
- `inline std::vector<std::uint8_t> binaryDiff(const std::vector<std::uint8_t>& src,`
- `inline bool binaryPatch(const std::uint8_t* src, std::size_t srcLen,`
- `inline bool binaryPatch(const std::vector<std::uint8_t>& src, const std::vector<std::uint8_t>& patch,`

### `BundlePlan`
<sub>`engine/include/maz/io/BundlePlan.hpp`</sub>

maz::io desktop export bundle planner — the per-OS "brain" behind tools/package.sh and Godot's "Export Project". Given a built app (its executable, the runtime libraries it links, its shaders and assets) and a target OS, it computes the COMPLETE bundle layout deterministically: the platform executable name (game.exe on Windows, game on Linux/macOS), where each file lands inside the bundle, a launcher script that makes the game find its own libraries and run from anywhere, a MANIFEST, and the total size. This is the decision-making package.sh currently does inline in shell — lifting it into a tested C++ core is what turns the packager into a real, verifiable per-OS bundler. Pure string/size logic, no filesystem calls, so the whole plan unit-tests headlessly; the shell (or an in-editor "Export" button) just executes the plan the planner returns.

**Types:** `PlatformSpec`, `SourceFile`, `BundleFile`, `BundlePlan`

**Functions:**

- `inline std::string osName(TargetOs os)`
- `inline PlatformSpec platformSpec(TargetOs os)`
- `inline std::string baseName(const std::string& p)`
- `inline std::string bundleDirName(const std::string& app, const std::string& version, TargetOs os,`
- `inline std::string makeLauncher(TargetOs os, const std::string& exeName)`
- `inline BundlePlan planBundle(const std::string& appName, const std::string& version, TargetOs os,`
- `inline const BundleFile* findDest(const BundlePlan& plan, const std::string& dest)`

### `Bwt`
<sub>`engine/include/maz/io/Bwt.hpp`</sub>

maz::io Burrows-Wheeler Transform — the reversible byte-reordering at the heart of bzip2-style compression. The BWT rearranges the bytes of a block so that runs of the same symbol CLUSTER together (identical contexts end up adjacent), which a following move-to-front + entropy coder then squeezes far better than the raw data — yet it loses NOTHING: the exact original is recovered from the transformed block plus one index. It's the standard front-end for compressing repetitive game assets (text, level data, tilemaps, serialized scenes) before Huffman/range coding. This is the classic rotation-sort forward transform and the O(n) LF-mapping inverse. The engine has Huffman, LZW and a range coder but no BWT stage; this adds it. Header-only, std-only, deterministic.

**Types:** `BwtResult`

**Functions:**

- `inline BwtResult bwtEncode(const std::vector<std::uint8_t>& s)`
- `inline std::vector<std::uint8_t> bwtDecode(const std::vector<std::uint8_t>& last, std::size_t primaryIndex)`

### `Compression`
<sub>`engine/include/maz/io/Compression.hpp`</sub>

maz::io byte compression — a small, self-contained LZSS (LZ77 + sliding window) codec for shrinking save files, level data, and network payloads. It scans for the longest run of bytes already seen in a 4 KB back-window and replaces it with a compact (offset, length) reference; runs too short to pay for a reference are emitted as literals. Output is a stream of groups, each a control byte of eight flag bits (LSB first: 1 = the next literal byte, 0 = a 2-byte match token of a 12-bit offset and a 4-bit length). This is Godot's PackedByteArray.compress/decompress territory (a lossless general codec) — round-trip-exact for any input, not a specific on-disk format. Header-only, dependency-free.

**Functions:**

- `inline constexpr std::size_t kLzWindow = 4096;   // 12-bit offsets (1..4096)`
- `inline std::vector<std::uint8_t> lzCompress(const std::uint8_t* data, std::size_t n)`
- `inline std::vector<std::uint8_t> lzCompress(const std::vector<std::uint8_t>& data)`
- `inline std::vector<std::uint8_t> lzDecompress(const std::uint8_t* data, std::size_t n)`
- `inline std::vector<std::uint8_t> lzDecompress(const std::vector<std::uint8_t>& data)`
- `inline std::vector<std::uint8_t> lzCompress(const std::string& s)`
- `inline std::string lzDecompressToString(const std::vector<std::uint8_t>& data)`

### `Config`
<sub>`engine/include/maz/io/Config.hpp`</sub>

The bridge between the config registry (core::CVarRegistry, dependency-free) and JSON. This lives in the io layer so core stays zero-dependency while apps still get "config.json drives the engine": loadConfig applies a parsed JSON object's fields onto matching cvars (coercing JSON types into each cvar's declared type, so a bool cvar ignores a stray number), and configToJson serializes the whole registry back out for a settings file. Unknown keys are ignored, so a config file may target a subset (or a superset) of the registered cvars without error.

**Functions:**

- `inline int loadConfig(core::CVarRegistry& reg, const JsonValue& obj)`
- `inline JsonValue configToJson(const core::CVarRegistry& reg)`
- `inline int loadConfigFile(core::CVarRegistry& reg, const std::string& path)`
- `inline bool saveConfigFile(const core::CVarRegistry& reg, const std::string& path)`

### `ConfigFile`
<sub>`engine/include/maz/io/ConfigFile.hpp`</sub>

ConfigFile — Godot's ConfigFile: an INI-style `[section]` + `key=value` store, the format behind project settings, input maps, and hand-editable save/options files. Values are held as raw strings with typed accessors (getBool/getInt/getFloat coerce; setBool/setInt/setFloat format), which keeps it dependency-free while covering the overwhelmingly common settings-file use. Sections and keys preserve INSERTION ORDER so `encode()` produces stable, diff-friendly text that round-trips through `parse()`. Keys written before any `[section]` header live in the unnamed global section (Godot allows this). Parsing is lenient: blank lines and `;` / `#` comments are skipped, whitespace around keys/values is trimmed, and a value wrapped in matching quotes has them stripped. Header-only, deterministic — it unit-tests exactly and drives a golden (a rendered settings table + its encoded text).

**Types:** `ConfigFile`

### `ExportConfig`
<sub>`engine/include/maz/io/ExportConfig.hpp`</sub>

maz::io export presets — the data model behind Godot's export presets and the packaging pipeline (tools/package.sh): each preset names a target platform, an output path, a set of feature tags the build defines, and include/exclude resource filters deciding which files ship. The reusable core is a small glob matcher (* = any run, ? = one char) used by the filters. includes(path) applies the rules the way Godot does: an exclude match always drops a file; otherwise, an empty include list ships everything, and a non-empty one ships only matching files. Pure string logic, no filesystem or platform calls, so it unit-tests headlessly; the actual copy/zip step reads these decisions.

**Types:** `ExportPreset`, `ExportConfig`

**Functions:**

- `inline bool globMatch(const std::string& pattern, const std::string& text)`

### `GettextPo`
<sub>`engine/include/maz/io/GettextPo.hpp`</sub>

maz::io gettext PO catalog — the industry-standard translation format, alongside Maz's existing CSV tables. A .po file pairs each source string (msgid) with its translation (msgstr), optionally under a disambiguating context (msgctxt) and with plural variants (msgid_plural + msgstr[n]). Crucially, PO carries a per-language PLURAL RULE — a little C expression over `n` that selects which plural form to use (English has 2; Polish/Arabic/Russian have 3-6 with non-trivial rules). This module parses PO text and answers gettext/ngettext/pgettext/npgettext lookups, evaluating the language's own plural rule so counts render correctly everywhere. Pure text + a small integer expression evaluator (the C subset gettext uses: n, literals, % * / + -, comparisons, && || !, and ?:) — no I/O in the core, so it unit-tests headlessly. This is Godot's Translation/PO support.

**Types:** `PluralRule`, `PoCatalog`, `PotBuilder`

**Functions:**

- `inline std::string poEscape(const std::string& s)`

### `Gzip`
<sub>`engine/include/maz/io/Gzip.hpp`</sub>

maz::io gzip (.gz, RFC 1952) decompressor — the container Godot reads through FileAccess's gzip compression mode and that wraps countless downloaded/shipped assets (`.gz`). Maz already had raw-DEFLATE and zlib inflate (M499) but not the gzip framing on top, so a `.gz` blob could not be opened. `gunzip` parses the 10-byte gzip header (magic 1f 8b, method 08) plus the optional FEXTRA / FNAME / FCOMMENT / FHCRC fields, runs the embedded DEFLATE stream through the existing `inflateRaw`, and then VERIFIES the result against the trailing CRC-32 (via the existing `core::crc32`) and ISIZE footer — so a corrupted or truncated stream is reported rather than silently returning garbage. Pure CPU, header-only, unit-tested against blobs produced by Python's reference `gzip` module.  Scope note (honest): single-member gzip streams (the overwhelmingly common case); concatenated multi-member streams decode only their first member. Decompress only — no gzip *compression* (Maz's inflate side has no deflate encoder yet). Optional header fields are skipped correctly but not returned to the caller.

**Functions:**

- `inline bool gunzip(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out)`
- `inline bool gunzip(const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& out)`

### `Hdr`
<sub>`engine/include/maz/io/Hdr.hpp`</sub>

maz::io Radiance HDR (.hdr / RGBE) decoder — the high-dynamic-range image format used for skyboxes and image-based lighting (Godot loads .hdr for its Sky/environment). Each pixel is stored as RGBE: three 8-bit mantissas sharing one 8-bit exponent, so a whole float-range image fits in 4 bytes/px; decoding expands it back to linear float RGB. This parses the text header (magic, FORMAT, the "-Y H +X W" resolution line), the modern per-channel run-length scanline encoding (and a raw fallback), and converts RGBE -> float via the standard ldexp reconstruction. Pure bytes in, floats out — no GPU, no file I/O in the core — so it unit-tests headlessly; the renderer uploads the floats.

**Types:** `HdrImage`

**Functions:**

- `inline void rgbeToFloat(uint8_t r, uint8_t g, uint8_t b, uint8_t e, float* out)`
- `inline HdrImage decodeHdr(const uint8_t* data, std::size_t size)`
- `inline HdrImage decodeHdr(const std::vector<uint8_t>& bytes)`

### `Huffman`
<sub>`engine/include/maz/io/Huffman.hpp`</sub>

maz::io Huffman entropy coder — a self-contained, round-trip-exact byte compressor that assigns short bit codes to frequent bytes and long codes to rare ones (optimal prefix coding). It is the ENTROPY- coding companion to the engine's LZSS dictionary codec (Compression.hpp): LZSS removes repeated runs, Huffman squeezes skewed byte distributions, and the two together are exactly what "deflate" combines. Reach for Huffman on data with a lopsided byte histogram but few long repeats — packed tables, tile indices, quantised audio, text. The stream stores CANONICAL code lengths (one byte per present symbol) in its header, so the decoder rebuilds the identical code table deterministically with no separate model to ship. Godot bundles deflate/zstd but the engine only had a dictionary coder, so this adds the missing entropy path. Header-only, dependency-free, lossless.

**Types:** `HuffNode`

**Functions:**

- `inline std::array<std::uint32_t, 256> canonicalCodes(const std::array<std::uint8_t, 256>& lengths)`
- `inline void put32(std::vector<std::uint8_t>& out, std::uint32_t v)`
- `inline std::uint32_t get32(const std::uint8_t* p)`
- `inline std::vector<std::uint8_t> huffmanCompress(const std::uint8_t* data, std::size_t n)`
- `inline std::vector<std::uint8_t> huffmanCompress(const std::vector<std::uint8_t>& data)`
- `inline std::vector<std::uint8_t> huffmanCompress(const std::string& s)`
- `inline std::vector<std::uint8_t> huffmanDecompress(const std::uint8_t* data, std::size_t n)`
- `inline std::vector<std::uint8_t> huffmanDecompress(const std::vector<std::uint8_t>& data)`
- `inline std::string huffmanDecompressString(const std::vector<std::uint8_t>& data)`

### `ImportFile`
<sub>`engine/include/maz/io/ImportFile.hpp`</sub>

maz::io asset-import sidecar — Godot's `.import` pipeline. Every imported source asset (a .png, a .gltf, a .wav) gets a sibling `<source>.import` file describing HOW it was imported: which importer ran, the resource type/UID it produced, the source it came from, and the cooked file(s) written into the `.godot/imported/` cache. The engine loads the cooked resource, not the raw source, and reimports only when the source changed. This models that sidecar as data: parse/encode the INI-with-arrays format, the cooked-path convention, and an ImportDatabase that answers "does this need reimporting?" from a content hash. Pure string/data logic (no filesystem here) so it unit-tests headlessly; the editor/tool layer does the actual file reads + cooking on top.

**Types:** `ImportFile`, `ImportDatabase`

**Functions:**

- `inline std::string contentHashHex(const std::string& bytes)`

### `Inflate`
<sub>`engine/include/maz/io/Inflate.hpp`</sub>

maz::io DEFLATE / zlib inflate (RFC 1951 + RFC 1950) — a header-only, dependency-free decompressor. This is the missing building block under PNG import, gzip/zlib assets, and KTX2 ZLIB supercompression: Godot leans on zlib for all of these, and Maz previously had no inflate at all. `inflateRaw` expands a raw DEFLATE stream; `zlibInflate` validates and strips the 2-byte zlib header (and optional preset dictionary) before inflating. The decoder is the canonical bit-at-a-time Huffman walk (the well-known "puff" approach), handling stored, fixed-Huffman, and dynamic-Huffman blocks with LZ77 back-references. Pure CPU byte work — unit-tested headlessly against golden streams produced by the reference zlib.  Scope note (honest): this decompresses; it does not compress, and it does not verify the trailing Adler-32 checksum (it stops cleanly at the final block). Malformed input returns false rather than throwing.

**Types:** `InflateState`, `Huffman`

**Functions:**

- `inline int construct(Huffman& h, const int* length, int n)`
- `inline int decode(InflateState& s, const Huffman& h)`
- `inline bool inflateBlock(InflateState& s, const Huffman& lit, const Huffman& dist)`
- `inline bool fixedBlock(InflateState& s)`
- `inline bool dynamicBlock(InflateState& s)`
- `inline bool storedBlock(InflateState& s)`
- `inline bool inflateRaw(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out)`
- `inline bool inflateRaw(const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& out)`
- `inline bool zlibInflate(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out)`
- `inline bool zlibInflate(const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& out)`

### `Json`
<sub>`engine/include/maz/io/Json.hpp`</sub>

JSON value + parser + serializer: the engine's human-readable data format, alongside the binary Serialize backbone. Where ByteWriter/Reader is for fast, compact save games, this is for the files a person (or a tool) edits: configs, tuning tables, and data-driven scenes/levels. A JsonValue is a tagged union over the six JSON types (null / bool / number / string / array / object); objects keep insertion order so a round-trip is stable and diff-friendly. parse() is a hand-written recursive descent scanner that is bounds-checked and never throws — malformed input yields a null value plus a human-readable error (line/column), so loading untrusted data fails cleanly. dump() re-serializes, optionally pretty-printed. Header-only, zero dependencies beyond the standard library.

**Types:** `JsonValue`, `JsonParseResult`, `JsonParser`

**Functions:**

- `inline JsonValue* JsonValue::Object::find(const std::string& key)`
- `inline const JsonValue* JsonValue::Object::find(const std::string& key) const`
- `inline void JsonValue::writeEscaped(std::string& out, const std::string& s)`
- `inline void JsonValue::writeTo(std::string& out, int indent, int depth) const`
- `inline JsonParseResult parseJson(const std::string& text)`
- `inline bool readTextFile(const std::string& path, std::string& out)`
- `inline bool writeTextFile(const std::string& path, const std::string& text)`
- `inline JsonParseResult parseJsonFile(const std::string& path)`
- `inline bool writeJsonFile(const std::string& path, const JsonValue& value, int indent = 2)`

### `Localization`
<sub>`engine/include/maz/io/Localization.hpp`</sub>

CSV parsing + localization — Godot's Translation / CSV import. A shippable game needs its on-screen text in more than one language, and the standard authoring format (Godot's included) is a CSV whose first column is a message KEY and whose remaining columns are one LOCALE each. Maz could read JSON and its own prefab/binary formats but had no CSV reader and no translation lookup at all. This adds a robust RFC-4180-style CSV parser (quoted fields, embedded delimiters/newlines, "" escapes, CRLF or LF) plus a `TranslationTable` that loads such a CSV and answers tr(key) in the active locale with sensible fallback. Pure std, header-only, no engine deps.

**Types:** `TranslationTable`

**Functions:**

- `inline std::vector<std::vector<std::string>> parseCsv(std::string_view text, char delim = ',')`

### `Lzw`
<sub>`engine/include/maz/io/Lzw.hpp`</sub>

maz::io — LZW (Lempel-Ziv-Welch) lossless compression. Games squeeze save files, chunked tilemaps, procedural data blobs, and network payloads; LZW is the classic dictionary compressor (the one behind GIF, TIFF, and old Unix `compress`) that finds repeated byte sequences and replaces each with a single code. It builds its dictionary on the fly from the data itself, so the decompressor reconstructs the exact same dictionary as it goes — no table needs to be stored. This implementation uses fixed 16-bit codes with the dictionary frozen once full (65536 entries), which keeps encode/decode trivially in lockstep (no variable-width code-size sync to get wrong) and is exactly invertible for ANY input. Great on repetitive/structured data (long runs and repeats collapse to one code); incompressible data can grow (each new literal costs two bytes) — that's the honest trade of this simple variant, not a match for DEFLATE. Godot exposes zlib/gzip; this is a dependency-free, header-only, deterministic engine-side compressor. Round-trip exact: decompress(compress(x)) == x.

**Functions:**

- `inline std::vector<std::uint8_t> lzwCompress(const std::vector<std::uint8_t>& input)`
- `inline std::vector<std::uint8_t> lzwDecompress(const std::vector<std::uint8_t>& input)`

### `MessagePack`
<sub>`engine/include/maz/io/MessagePack.hpp`</sub>

maz::io::MessagePack — the MessagePack binary serialization format (https://msgpack.org): a compact, self-describing interchange encoding that is like JSON but in bytes. The engine already has JSON (great for human-edited config, but verbose and slow) and its own tag-free binary Serialize (tiny, but the two ends must agree on the exact layout in advance). MessagePack fills the gap between them: it is small and fast like binary, yet SELF-DESCRIBING like JSON — a decoder recovers the full structure (nil/bool/int/ float/string/bytes/array/map) with no schema. That makes it the natural wire format for network messages, replays, and save files that must stay readable across versions, and it interоperates with the MessagePack libraries shipped for essentially every language (handy for tools and servers). This encoder is CANONICAL — every value takes its smallest legal representation — so the output byte-for-byte matches the published spec vectors, which is exactly what the tests pin down. Header-only, std-only, deterministic. Godot only offers JSON + its own var_to_bytes; MessagePack is the portable standard.

**Types:** `MsgValue`, `Reader`

**Functions:**

- `inline void putBE(std::vector<std::uint8_t>& out, std::uint64_t v, int bytes)`
- `inline void encodeUnsigned(std::vector<std::uint8_t>& out, std::uint64_t u)`
- `inline void encodeSigned(std::vector<std::uint8_t>& out, std::int64_t v)`
- `inline void encodeValue(std::vector<std::uint8_t>& out, const MsgValue& v)`
- `inline MsgValue decodeValue(Reader& r, int depth)`
- `inline MsgValue decodeArray(Reader& r, std::size_t len, int depth)`
- `inline MsgValue decodeMap(Reader& r, std::size_t len, int depth)`
- `inline MsgValue decodeValue(Reader& r, int depth)`
- `inline MsgValue decodeArray(Reader& r, std::size_t len, int depth)`
- `inline MsgValue decodeMap(Reader& r, std::size_t len, int depth)`
- `inline std::vector<std::uint8_t> msgpackEncode(const MsgValue& v)`
- `inline bool msgpackDecode(const std::uint8_t* data, std::size_t size, MsgValue& out)`
- _…and 1 more_

### `MoveToFront`
<sub>`engine/include/maz/io/MoveToFront.hpp`</sub>

maz::io Move-To-Front (MTF) coding — the stage that sits between a Burrows-Wheeler Transform and the entropy coder in bzip2-style compression. It keeps a list of the 256 byte values and, for each input byte, emits its CURRENT POSITION in that list and then moves it to the front. When the data has locally repeated or clustered symbols (exactly what the BWT produces), recently-seen bytes sit near the front, so their codes are small — long runs collapse to streams of zeros, which a following run-length + Huffman/range coder squeezes hard. It is perfectly reversible. Pair it with the engine's BWT and range/Huffman coders to complete a real compression pipeline. Header-only, std-only, deterministic.

**Functions:**

- `inline std::vector<std::uint8_t> mtfEncode(const std::vector<std::uint8_t>& in)`
- `inline std::vector<std::uint8_t> mtfDecode(const std::vector<std::uint8_t>& in)`

### `PrefabText`
<sub>`engine/include/maz/io/PrefabText.hpp`</sub>

Text resource save/load for prefabs — Godot's .tscn / .tres text format. M125 gave prefabs an in-memory template + instancing; this makes them a DISK RESOURCE you can read, diff, and version-control as plain text (the whole reason Godot's scene files are text). A prefab tree serializes to a sequence of `[node name="…" parent="…"]` sections, each followed by typed `key = TYPE values` property lines, and parses straight back into an identical tree. Deterministic + round-trip-stable; pure string work, no I/O device, so it unit-tests headlessly (the app owns any actual file read/write).

**Functions:**

- `inline std::string fmtF(float v)`
- `inline std::string propToText(const scene::PropValue& p)`
- `inline std::vector<std::string> tokens(const std::string& s)`
- `inline bool textToProp(const std::string& s, scene::PropValue& out)`
- `inline void writeNode(std::string& out, const scene::PrefabNode& node, const std::string& parentPath)`
- `inline std::string attr(const std::string& line, const std::string& key)`
- `inline std::string savePrefabText(const scene::Prefab& prefab)`
- `inline bool loadPrefabText(const std::string& text, scene::Prefab& out)`

### `RangeCoder`
<sub>`engine/include/maz/io/RangeCoder.hpp`</sub>

maz::io — arithmetic (range) coding: entropy compression that squeezes a stream of symbols down toward its true information content. Where LZW replaces repeats with dictionary codes, a range coder assigns each symbol a slice of a numeric interval proportional to its probability, so common symbols cost a fraction of a bit and rare ones cost more — beating fixed-width and Huffman on skewed data (quantized audio/mesh residuals, save-game deltas, tile histograms). This is Dmitry Subbotin's carryless 32-bit range coder with a caller-supplied static frequency model (one integer weight per symbol, total <= 65536). Godot exposes zlib/gzip but no arithmetic coder. Header-only, std-only, deterministic; encode/decode round-trip exactly.

**Functions:**

- `inline std::vector<std::uint32_t> cumulate(const std::vector<std::uint32_t>& freq)`
- `inline std::vector<std::uint8_t> rangeEncode(const std::vector<std::uint32_t>& symbols,`
- `inline std::vector<std::uint32_t> rangeDecode(const std::vector<std::uint8_t>& data,`

### `ResourcePack`
<sub>`engine/include/maz/io/ResourcePack.hpp`</sub>

Resource pack archive — Godot's PackedData / the .pck file its shipping games load every asset from. Maz could already serialize a single blob (io::Serialize) and read/write one file at a time, but there was no way to bundle MANY named resources — textures, level JSON, sound clips, prefab text — into ONE archive and pull them back out by path. That is what a game ships: one .pck instead of a loose tree of files. This is a self-contained byte-in / byte-out container built on the existing ByteWriter/ByteReader: `packResources` writes a magic+version header, a directory of (path, offset, size) records, then the concatenated blob data; `ResourcePack::load` parses that back and hands out each blob by path with full bounds checking, so a truncated or foreign archive fails cleanly instead of reading out of range. The app owns any real disk read/write (via io::writeFile / io::readFile) — this just does the packing.

**Types:** `PackEntry`, `ResourcePack`

**Functions:**

- `inline std::vector<std::uint8_t> packResources(const std::vector<PackEntry>& entries)`

### `SceneSerializer`
<sub>`engine/include/maz/io/SceneSerializer.hpp`</sub>

Reflection-lite ECS scene serialization: save and load a live ecs::World as JSON. The ECS stores arbitrary component types in type-erased pools, so — without a full reflection system — the app tells the serializer, once, how each component maps to/from JSON:  io::SceneSerializer s; s.component<Transform>("Transform", [](const Transform& t){ io::JsonValue j; j.set("x", t.x); j.set("y", t.y); return j; }, [](const io::JsonValue& j){ return Transform{ j["x"].asFloat(), j["y"].asFloat() }; });  saveWorld then emits { "entities": [ { "id": N, "components": { "Transform": {..}, .. } }, .. ] } with entities in ascending-id order (deterministic), and loadWorld rebuilds the world from that. This is the standard content-pipeline backbone for save games, prefabs, and an editor. It lives in the io layer so ecs stays dependency-free. Header-only.

**Types:** `SceneSerializer`

### `Serialize`
<sub>`engine/include/maz/io/Serialize.hpp`</sub>

Binary serialization: the engine's backbone for save games, level files, and any structured data that must round-trip to disk (or across a wire later). ByteWriter appends; ByteReader consumes with bounds checking so truncated/corrupt input fails cleanly (ok() == false) instead of reading out of range. Layout is the host byte order — fine for the LE desktop targets the engine builds for; a byte-swap layer can slot in later if a BE target ever matters. Header-only.

**Types:** `ByteWriter`, `ByteReader`

**Functions:**

- `inline bool writeFile(const std::string& path, const std::vector<uint8_t>& bytes)`
- `inline bool readFile(const std::string& path, std::vector<uint8_t>& out)`

### `StreamPeer`
<sub>`engine/include/maz/io/StreamPeer.hpp`</sub>

maz::io::StreamPeerBuffer — Godot's StreamPeerBuffer: a growable byte buffer with a read cursor and explicit, fixed-WIDTH, endian-aware put/get for u8/16/32/64 (signed + unsigned), float, double, and length-prefixed strings. Unlike io::Serialize's ByteWriter/ByteReader (host byte order, template write<T>), this pins the on-the-wire layout, which is exactly what network protocols and portable file formats need — a big-endian sender and a little-endian receiver agree byte-for-byte. Reads past the end are safe: they return 0 / empty and leave the cursor put. Header-only, pure, C++20.

**Types:** `StreamPeerBuffer`

### `Varint`
<sub>`engine/include/maz/io/Varint.hpp`</sub>

maz::io variable-length integer encoding (LEB128 + zigzag) — the compact way to serialize integers that are usually small. A fixed 4- or 8-byte field wastes space when the value is 3; a varint spends one byte for values under 128, two under 16384, and so on, only paying for 64 bits when the number is genuinely huge. This is what Protocol Buffers, WebAssembly, DWARF, and most netcode use to shrink save files, replay streams, delta-compressed snapshots, and network packets. Unsigned values use plain LEB128; signed values are first folded through zigzag so small-magnitude negatives (-1, -2, …) also encode in one byte instead of ten. Decoding is bounds-checked and overflow-checked: a truncated or over-long stream returns false rather than reading past the buffer or wrapping. Complements the engine's fixed-width StreamPeer and base64 (M154). Header-only, std-only, endian-independent (the byte order is defined by the format itself).

**Functions:**

- `inline std::size_t varintSize(std::uint64_t value)`
- `inline void appendVarint(std::vector<std::uint8_t>& out, std::uint64_t value)`
- `inline bool readVarint(const std::uint8_t* data, std::size_t size, std::size_t& offset, std::uint64_t& out)`
- `inline bool readVarint(const std::vector<std::uint8_t>& data, std::size_t& offset, std::uint64_t& out)`
- `inline std::uint64_t zigzagEncode(std::int64_t v)`
- `inline std::int64_t zigzagDecode(std::uint64_t u)`
- `inline void appendVarintSigned(std::vector<std::uint8_t>& out, std::int64_t value)`
- `inline bool readVarintSigned(const std::uint8_t* data, std::size_t size, std::size_t& offset,`
- `inline bool readVarintSigned(const std::vector<std::uint8_t>& data, std::size_t& offset, std::int64_t& out)`

### `VirtualFileSystem`
<sub>`engine/include/maz/io/VirtualFileSystem.hpp`</sub>

maz::io — a virtual filesystem with scheme paths, Maz's answer to Godot's res:// / user:// path model. A game shouldn't hard-code absolute OS paths: it refers to assets as "res://textures/hero.png" and to saves as "user://save1.dat", and the VFS maps each scheme to a real directory chosen at startup (the install dir, the per-user save dir, a mounted mod folder...). That indirection is what lets the same game run from a dev tree, an installed bundle, or a packed archive without changing a single path in the game code.  The valuable, testable core is pure path logic: normalizePath collapses '.', '..', and duplicate slashes; resolve() maps a scheme path to a real path AND refuses any '..' that would escape the mount root (so "res://../../etc/passwd" is rejected, not silently followed) — the traversal guard a naive string-concat filesystem forgets. The actual byte reading/writing stays in platform code; this header is the routing + safety layer, header-only and unit-testable with no disk.

**Types:** `VirtualFileSystem`

**Functions:**

- `inline std::string normalizePath(const std::string& p)`
- `inline std::string joinPath(const std::string& a, const std::string& b)`
- `inline std::string fileName(const std::string& p)`
- `inline std::string extension(const std::string& p)`
- `inline std::string fileStem(const std::string& p)`
- `inline std::string parentPath(const std::string& p)`

### `Xml`
<sub>`engine/include/maz/io/Xml.hpp`</sub>

Pull-style XML reader — Godot's XMLParser: a forward, streaming tokenizer that hands back one node at a time (`read()` advances; the accessors describe the node just read) instead of building a DOM tree in memory. That is the shape you want for reading big or foreign documents — a Tiled `.tmx` tilemap, an SVG path set, a COLLADA model, an RSS feed, an app config in XML — where you walk the stream and pull out the handful of elements you care about. Maz had text formats (JSON, CSV, .tres/PrefabText) and binary (Serialize, ResourcePack) but no XML at all, so any XML-shaped asset was unreadable.  The parser recognises elements (`<a x="1">`), self-closing elements (`<br/>`), end tags (`</a>`), text runs, comments (`<!-- … -->`), CDATA (`<![CDATA[ … ]]>`), and processing / declaration nodes (`<?xml … ?>`). Attribute values and text runs are entity-decoded (`&lt; &gt; &amp; &quot; &apos;` and numeric `&#NN;` / `&#xHH;`, UTF-8 encoded). `depth()` reports the count of currently-open ancestor elements so a caller can indent or scope without tracking a stack by hand. Header-only, no allocation beyond the node's own strings, deterministic — it unit-tests exactly and drives a golden.  Scope note (honest): this is a well-formed-input pull parser, not a validator. It does not check tag nesting/matching, resolve namespaces, expand DTD/custom entities, or enforce a schema; malformed input (an unterminated tag or quote) sets the error flag and stops rather than recovering. Those remain follow-ups; a full validating/DOM parser is out of scope for a header-only module.

**Types:** `XmlParser`


<a name="input"></a>
## Input — action maps, analog helpers

### `ActionMap`
<sub>`engine/include/maz/input/ActionMap.hpp`</sub>

Action mapping: the layer that turns raw device state into named gameplay intents. Gameplay code asks "is Jump pressed?" or "what's MoveX?" instead of "is Space down / is pad A down / is the left stick past the deadzone?" — so one action can bind several physical sources (keyboard OR gamepad), bindings can be rebound at runtime, and the game logic never mentions a scancode.  Two action kinds: * Button actions are down if ANY bound source is down; each frame yields held / pressed (edge down this frame) / released (edge up this frame). * Axis actions combine negative/positive button pairs (each contributing -1 / +1) with any analog pad axes (scaled), clamped to [-1, 1] — so WASD and a thumbstick drive the same MoveX.  The map is deliberately SDL-free: update() takes sampler callbacks (down(device, code) and analog(axis)), so it is unit-testable with synthetic input and works over any backend. An app wires the samplers to platform::Input. Header-only, std only.

**Types:** `ActionMap`

### `Analog`
<sub>`engine/include/maz/input/Analog.hpp`</sub>

Analog-stick conditioning — Godot's `Input.get_vector` / `get_axis` deadzone maths.  A raw thumbstick reports a 2D vector whose length can drift up to ~√2 at the diagonals and jitters around zero at rest. Feeding that straight into movement gives two classic bugs: the character creeps while the stick is "centred" (drift inside the deadzone), and moves ~40% faster on the diagonals than the cardinals (the square input region is bigger than the unit circle). Godot fixes both in `get_vector`: a **radial** deadzone (the whole vector's magnitude, not each axis) zeroes rest jitter, the remaining magnitude is **rescaled** so the deadzone edge maps to 0 and 1 maps to 1 (no sudden jump as you leave the deadzone), and the magnitude is **clamped to the unit circle** so diagonals aren't faster. These are pure, stateless functions over raw values — no backend, no allocation — so they unit-test exactly and pair with any input source (the `platform::Input` gamepad axes, `ActionMap` axis actions, or a synthetic test vector).

**Functions:**

- `inline float sanitizeDeadzone(float deadzone)`
- `inline float applyDeadzone(float value, float deadzone)`
- `inline math::vec2 analogVector(math::vec2 raw, float deadzone = 0.2f)`


<a name="editor"></a>
## Editor — scene model, gizmos, inspector

### `Scene`
<sub>`engine/include/maz/editor/Scene.hpp`</sub>

A minimal editable scene model for the in-engine editor — the data an inspector edits and a scene tree lists, kept renderer-agnostic so it is pure logic and unit-testable without a GPU. Each Node owns a transform (position / Euler degrees / scale), a local-space AABB (for click picking), a mesh id (an index the app maps to a real MeshHandle), and material parameters. The editor app turns these into draw calls; the engine keeps only the model.

**Types:** `Node`, `Scene`, `History`

**Functions:**

- `inline int pickNode(const Scene& scene, const math::vec3& origin, const math::vec3& dir)`
- `inline float snap1(float v, float step)`
- `inline math::vec3 snapToGrid(const math::vec3& v, float step)`
- `inline bool rayPlaneY(const math::vec3& origin, const math::vec3& dir, float planeY,`
- `inline void screenRay(const math::mat4& invViewProj, float px, float py, float w, float h,`
- `inline bool worldToScreen(const math::mat4& viewProj, const math::vec3& world, float w, float h,`
- `inline io::JsonValue toJson(const Scene& s)`
- `inline bool fromJson(const io::JsonValue& root, Scene& out)`


<a name="root"></a>
## (root)

### `Engine`
<sub>`engine/include/maz/Engine.hpp`</sub>


<a name="docs"></a>
## docs

### `SiteGen`
<sub>`engine/include/maz/docs/SiteGen.hpp`</sub>

maz::docs static documentation-site generator — turns the engine's Markdown docs into a linked, browsable static HTML site (index + one page per doc + a shared nav sidebar), the "read the docs" surface every mature engine ships. It renders a practical Markdown subset — ATX headings (#..######), paragraphs, unordered lists, fenced ``` code blocks, and inline **bold** / `code` / [links](url) — HTML-escaping all text so a doc that contains `<`, `>` or `&` renders literally rather than injecting markup, and rewriting intra-doc `.md` links to `.html` so navigation works in the generated site. Pure std-only string work: no GPU, no I/O in the core (the CLI wrapper does the file reads/writes), so it unit-tests headlessly from in-memory strings. `buildSite` returns a filename -> full-HTML-document map ready to write to disk.  Scope note (honest): a focused Markdown subset (the constructs the repo's docs actually use), not a full CommonMark implementation — nested lists, tables, blockquotes, and images are documented follow-ups.

**Types:** `Page`, `SiteOptions`

**Functions:**

- `inline std::string escapeHtml(const std::string& s)`
- `inline std::string rewriteLink(const std::string& url)`
- `inline std::string renderInline(const std::string& raw)`
- `inline std::vector<std::string> splitLines(const std::string& s)`
- `inline bool startsWith(const std::string& s, const char* p)`
- `inline std::string renderMarkdown(const std::string& md)`
- `inline std::string navHtml(const std::vector<Page>& pages)`
- `inline const char* siteCss()`
- `inline std::string wrapDocument(const std::string& pageTitle, const std::string& siteTitle,`
- `inline std::map<std::string, std::string> buildSite(const std::vector<Page>& pages,`


<a name="ext"></a>
## ext

### `DynamicLibrary`
<sub>`engine/include/maz/ext/DynamicLibrary.hpp`</sub>

maz::ext dynamic extension loader — the real shared-library ("[DESK]") half of the GDExtension-style ABI in Extension.hpp. That header models the registry + version negotiation an in-process plugin uses; THIS one actually opens a compiled `.so`/`.dll`/`.dylib` at runtime, resolves the plugin's exported entry symbols, negotiates the ABI version, and runs the plugin against a host registry — exactly how Godot loads a GDExtension without recompiling the engine. A Maz extension library exports two C symbols: extern "C" void maz_extension_abi_version(int* major, int* minor);   // the ABI it was built against extern "C" bool maz_extension_entry(int major, int minor, maz::ext::ExtensionRegistry& reg); The loader reads the version, rejects an incompatible plugin up front (reusing `abiCompatible`), and only then calls the entry point so the plugin can register its classes/methods.  This is genuinely end-to-end verifiable on this box: the test compiles a real sample plugin into a shared library, loads it through here, and instantiates + calls a class the plugin registered — no mock. POSIX uses dlopen/dlsym; Windows uses LoadLibrary/GetProcAddress (compiled-blind here, exercised on Windows).

**Types:** `DynamicLibrary`

**Functions:**

- `inline DynamicLibrary loadExtensionLibrary(const std::string& path, ExtensionRegistry& reg,`

### `Extension`
<sub>`engine/include/maz/ext/Extension.hpp`</sub>

maz::ext GDExtension-style C ABI — Godot's GDExtension: a stable, function-pointer interface that lets third parties add engine classes and methods from a separate shared library WITHOUT recompiling the engine. The ABI surface uses only C-compatible types (a tagged variant + plain function pointers), so a plugin compiled against a matching ABI version can register classes, be instantiated, and have its methods dispatched by name. This models the registry + dispatcher + version negotiation that make that work; the actual dlopen()/LoadLibrary of a .so/.dll is the [DESK] loader on top. Header-only and deterministic (a mock in-process "extension" stands in for a real library), so it unit-tests exactly.

**Types:** `ExtVariant`, `ExtensionRegistry`

**Functions:**

- `inline bool abiCompatible(int pluginMajor, int pluginMinor)`
- `inline bool loadExtension(ExtEntryFn entry, int pluginMajor, int pluginMinor,`


<a name="net"></a>
## net

### `BitStream`
<sub>`engine/include/maz/net/BitStream.hpp`</sub>

maz::net bit stream — compact bit-level packet (de)serialization, the foundation of the networking layer. Games send state constantly, so every bit counts: a flag is 1 bit (not a byte), a value known to be 0..1000 is 10 bits (not 32), a normalized float can be quantized to 16. BitWriter packs values LSB-first into a byte buffer; BitReader unpacks them in the same order and reports underflow instead of reading past the end. This is the wire format snapshot/delta replication and RPC ride on — and a bandwidth win over Godot's byte-granular Variant encoding. Pure, std-only, deterministic, unit-tested. No sockets here (that's the transport layer).

**Types:** `BitWriter`, `BitReader`

### `ClockSync`
<sub>`engine/include/maz/net/ClockSync.hpp`</sub>

maz::net::ClockSync — estimate the CLOCK OFFSET between this machine and a remote peer, and the round-trip time, from timestamped ping/pong exchanges (the NTP algorithm). Everything else in net/ that "renders in the past" (Interpolation) or predicts on server time (Prediction) needs a shared notion of time, but a client's clock and the server's clock drift and start at different values — so the client must learn how far ahead/behind the server is and how long a round trip takes. Each exchange yields four timestamps: t0 (client sends), t1 (server receives), t2 (server sends reply), t3 (client receives). NTP then gives offset = ((t1 - t0) + (t2 - t3)) / 2      (server clock minus client clock) delay  = (t3 - t0) - (t2 - t1)            (round-trip time, excluding server processing) Queuing jitter corrupts individual samples, so — like NTP's clock filter — the best estimate comes from the sample with the SMALLEST delay (least affected by congestion); a smoothed offset is also exposed. Times are plain doubles in one consistent unit (seconds or ms); no clock or socket here — the caller supplies the stamps. Deterministic, header-only, std-only. Godot's high-level multiplayer hides this.

**Types:** `ClockSync`

### `Connection`
<sub>`engine/include/maz/net/Connection.hpp`</sub>

maz::net connection / packet framing — the glue that turns the reliability layer (M213) into actual framed packets, the piece that sits directly beneath a real UDP socket. Every packet carries a header: a protocol id (a magic number that rejects foreign or corrupt datagrams) plus the reliability triplet — this packet's sequence, the latest sequence we've received from the peer, and the 32-bit bitfield of the 32 before it. A Connection assigns outgoing sequences, learns from each incoming header which of its in-flight packets the peer has acknowledged (so it can stop resending them and sample RTT), and records the peer's sequences to build its own ack header. There are NO sockets here — you hand it a payload to frame (pack) and hand it received bytes to parse (unpack); binding this to a real UDP/ENet socket is the one remaining [DESK] step. Pure, deterministic, unit-tested.

**Types:** `PacketHeader`, `Connection`

**Functions:**

- `inline void writePacketHeader(BitWriter& w, const PacketHeader& h)`
- `inline bool readPacketHeader(BitReader& r, PacketHeader& h)`

### `FloatQuant`
<sub>`engine/include/maz/net/FloatQuant.hpp`</sub>

maz::net bounded float quantization — shrink a float to N bits for network snapshots.  Sending full 32-bit floats for every position, angle, and health value burns bandwidth. Almost all of them live in a KNOWN range (a level is ±1000 units; health is 0..100; an angle is one turn), so you can map that range onto a small integer of `bits` bits, send the integer, and reconstruct the float on the other side to within one quantization step. This is the core trick behind compact netcode snapshots and delta encoding (pairs with the engine's BitStream/Snapshot). `quantizeFloat`/`dequantizeFloat` handle an arbitrary [min,max] range; `quantizeAngle`/`dequantizeAngle` treat an angle as PERIODIC so -pi and +pi map to the same code and there is no seam. Unlike PackNorm (fixed [0,1]/[-1,1] at 8/16 bits for GPU vertex attributes), this is any range at any bit width. Pure math, header-only, deterministic — unit-tested for exact endpoints, round-trip within half a step, clamping, and finer error at more bits.

**Functions:**

- `inline float fqClamp01(float t)`
- `inline std::uint32_t fqMaxInt(int bits)`
- `inline std::uint32_t quantizeFloat(float value, float min, float max, int bits)`
- `inline float dequantizeFloat(std::uint32_t q, float min, float max, int bits)`
- `inline std::uint32_t quantizeAngle(float radians, int bits)`
- `inline float dequantizeAngle(std::uint32_t q, int bits)`

### `Interpolation`
<sub>`engine/include/maz/net/Interpolation.hpp`</sub>

maz::net interpolation buffer — the client-side smoothing that makes networked motion look fluid despite arriving in discrete ticks. The server sends state ~20-60 times a second; a client that snapped to each packet would jitter. Instead the client renders slightly IN THE PAST (by an "interpolation delay" of a tick or two) and, at each frame, LERPs between the two buffered snapshots that bracket the render time — so motion is continuous even though data is discrete. This is Valve-style entity interpolation, the same idea behind Godot's MultiplayerSynchronizer interpolation. Optional extrapolation extends the last known velocity a bounded amount when a packet is late. Pure, header-only, unit-tested — templated on any value with +, -, and *float (a scalar, or a math::vec2/vec3). No sockets or clocks here; the caller supplies timestamps.

**Types:** `TimedSample`, `InterpolationBuffer`

**Functions:**

- `inline T lerpSample(const T& a, const T& b, float t)`

### `NetSim`
<sub>`engine/include/maz/net/NetSim.hpp`</sub>

maz::net network-condition simulator — the deterministic "bad network" harness that lets the ack, interpolation, and prediction layers be validated without a real (flaky) network. You submit packets via send(now); the sim holds each until its delivery time (now + latency ± jitter) and drops or duplicates it per the configured probabilities; receive(now) returns everything due by then, in delivery-time order. Randomness comes from a seeded xorshift PRNG so a test replays identically, and time is supplied by the caller (no clock here). This is the same tool Godot and Gaffer-style netcode use to prove the layers above cope with loss, reordering, and jitter. Pure, std-only, unit-tested. All times share whatever unit the caller uses for `now` (e.g. seconds).

**Types:** `NetConditions`, `NetSim`

### `Prediction`
<sub>`engine/include/maz/net/Prediction.hpp`</sub>

maz::net client-side prediction + server reconciliation — what makes a networked game feel instant despite round-trip latency. Instead of waiting for the server to confirm each move, the client PREDICTS: it applies its own input locally the moment it's pressed and keeps a history of every still-unacknowledged input. When an authoritative snapshot arrives (the true state AFTER some input sequence the server processed), the client RECONCILES: it snaps to that authoritative state and re-simulates every input the server hasn't seen yet — so a correct prediction is invisible and a misprediction is smoothly caught up. This is the Valve/Gaffer client-prediction model, the same idea Godot's high-level multiplayer leaves to the game; here it's a reusable, deterministic, unit-tested primitive. Templated on your State, Input, and a pure step function `State step(const State&, const Input&)`. No sockets or clocks — the caller drives the tick.

**Types:** `PredictionBuffer`

### `QuatCompress`
<sub>`engine/include/maz/net/QuatCompress.hpp`</sub>

maz::net — "smallest three" quaternion compression: pack a full 3D rotation into 32 bits for cheap network replication. A unit quaternion has four components but only three degrees of freedom (x²+y²+z²+w²=1), and one component always has magnitude ≥ 1/2. The trick: DON'T send the largest component. Send a 2-bit index saying which of the four it was, then the OTHER three components — each guaranteed to lie in [-1/√2, +1/√2] — quantized to `bits` bits apiece. The receiver reconstructs the dropped one from the unit-length constraint. Because q and -q are the same rotation, we first flip the sign so the largest component is positive, and so its sign never needs a bit either. At the default 9 bits per component that is 2 + 3·9 = 29 bits (fits in a 32-bit word with room to spare) for a rotation accurate to a small fraction of a degree — versus 128 bits for four raw floats, or 96 for three Euler angles that also suffer gimbal issues. This is the standard way shipping engines replicate orientation (character facing, projectile spin, ragdoll bones). Header-only, deterministic. Godot's multiplayer has no equivalent built in.

**Functions:**

- `inline std::uint32_t compressQuat(const math::quat& q, int bits = 9)`
- `inline math::quat decompressQuat(std::uint32_t code, int bits = 9)`
- `inline int compressedQuatBits(int bits = 9)`

### `Reliability`
<sub>`engine/include/maz/net/Reliability.hpp`</sub>

maz::net reliability layer — the ack system that turns raw unreliable packets (UDP) into a channel that KNOWS what arrived, without forcing TCP's head-of-line blocking. Every packet carries a 16-bit sequence number plus an ACK of the most recent sequence the peer received and a 32-bit bitfield of the 32 before it — so one returning packet acknowledges up to 33 at once, and lost acks self-heal on the next packet. From the acks a sender learns which of its in-flight packets landed (resend the rest) and can measure RTT. This is Glenn Fiedler's reliable-UDP model (what ENet/Godot's multiplayer do under the hood), kept pure and unit-tested — no sockets here.

**Types:** `AckReceiver`, `AckSender`

**Functions:**

- `inline bool seqGreaterThan(uint16_t a, uint16_t b)`

### `ReliableChannel`
<sub>`engine/include/maz/net/ReliableChannel.hpp`</sub>

**Types:** `ReliableChannel`

### `Replication`
<sub>`engine/include/maz/net/Replication.hpp`</sub>

maz::net scene replication — the high-level "just keep these object fields in sync" layer, Godot's MultiplayerSynchronizer + SceneReplicationConfig. You declare which properties of an object travel together (each a get/set pair plus a wire width); a ReplicatedObject can then capture() them into a value vector and apply() one back. A Synchronizer holds the per-peer BASELINE (the last values that peer has) and turns capture/apply into wire packets: writeFull for the first packet or a keyframe, writeDelta for the common case (only changed fields, via the M214 snapshot delta). As long as a peer starts from a full and applies deltas in order, sender and receiver baselines stay coherent. Pure, built on net::Snapshot/BitStream, unit-tested. Values are uint32 (quantize floats first — same convention as Snapshot).

**Types:** `ReplicatedProperty`, `ReplicatedObject`, `Synchronizer`

### `Rpc`
<sub>`engine/include/maz/net/Rpc.hpp`</sub>

maz::net RPC layer — remote procedure calls, the high-level way gameplay code talks across the wire without hand-rolling packet formats. A peer registers named handlers; to "call" a method on another peer it writes a compact header (a 16-bit method id + a 2-bit transfer mode) followed by the argument bytes, and the receiver dispatches to the matching handler with the reader parked at the args. Method NAMES hash to ids (FNV-1a, deterministic across peers/platforms) so strings never ride the wire. This is Godot's @rpc / rpc()/rpc_id() model — reliable/unreliable/ordered transfer modes included — kept pure and unit-tested; the transport just carries the bytes.

**Types:** `RpcMode`, `RpcDispatcher`

**Functions:**

- `inline RpcMethodId rpcHash(const char* name)`

### `Snapshot`
<sub>`engine/include/maz/net/Snapshot.hpp`</sub>

maz::net snapshot / delta replication — how game state crosses the wire efficiently. A snapshot is the current value of a fixed SCHEMA of fields (each with a known bit width — a health that's 0..100 is 7 bits, a tile id 0..1023 is 10). Sending a full snapshot every tick is wasteful, so DELTA replication sends, against a baseline the peer already has, only a changed-field bitmask plus the values that actually changed. Most fields don't change most ticks, so a delta is a few bits instead of a full struct — the core bandwidth win of networked games (Godot's MultiplayerSynchronizer does the same idea; the per-field bit widths make Maz's tighter). Pure, built on net::BitStream, unit-tested. Values are carried as uint32 (quantize floats before, e.g. via a fixed-point scale) so the transport stays integer-exact.

**Types:** `FieldSpec`

**Functions:**

- `inline void writeSnapshotFull(BitWriter& w, const std::vector<FieldSpec>& schema,`
- `inline std::vector<uint32_t> readSnapshotFull(BitReader& r, const std::vector<FieldSpec>& schema)`
- `inline size_t countChanged(const std::vector<uint32_t>& base, const std::vector<uint32_t>& cur)`
- `inline void writeSnapshotDelta(BitWriter& w, const std::vector<FieldSpec>& schema,`
- `inline std::vector<uint32_t> readSnapshotDelta(BitReader& r, const std::vector<FieldSpec>& schema,`

### `Spawner`
<sub>`engine/include/maz/net/Spawner.hpp`</sub>

maz::net multiplayer spawner — Godot's MultiplayerSpawner: the authority creates and destroys networked nodes, and those spawn/despawn events are replicated so every peer instantiates and frees matching nodes. This models that as data: the authority assigns a network id to each spawn (with a scene-type tag + spawn args), queues spawn/despawn events, and serializes them to a BitStream; remotes apply the stream via onSpawn/onDespawn callbacks. It also keeps the live set so a late joiner can be sent a full snapshot (writeFull) and rebuild the world in one shot. Pure, built on net::BitStream, unit-tested — the transport (who to send bytes to) is the socket layer on top. Pairs with net::Synchronizer (M218), which keeps each spawned node's properties in sync thereafter.

**Types:** `SpawnRecord`, `MultiplayerSpawner`

### `UdpSocket`
<sub>`engine/include/maz/net/UdpSocket.hpp`</sub>

**Types:** `Endpoint`, `UdpSocket`

**Functions:**

- `inline bool platformNetInit()`

### `WebSocket`
<sub>`engine/include/maz/net/WebSocket.hpp`</sub>

maz::net WebSocket (RFC 6455) handshake + frame codec — the transport a browser game MUST use for networking. A WASM build (see WebLoop.hpp / WEB_BUILD.md) cannot open the raw UDP sockets in `net::UdpSocket`; browsers only expose WebSocket (and WebRTC). WebSocket rides over a normal TCP/HTTP connection: the client sends an HTTP Upgrade with a random `Sec-WebSocket-Key`, the server replies with a `Sec-WebSocket-Accept` derived from it, and thereafter both sides exchange length-prefixed, optionally XOR-masked binary/text FRAMES. This header implements the two pure-logic pieces — the accept-key derivation and the frame encode/decode — so a Maz server can speak WebSocket to browser clients (over the real TCP socket the app owns). Both pieces are exact-spec and unit-tested against RFC 6455's own worked examples, with no live socket needed.  Scope note (honest): framing + handshake key (the parts that are pure bytes). The TCP accept loop and the HTTP header exchange are the app's socket code; `serverHandshakeResponse` builds the response string for it, and `parseClientKey` pulls the key out of the client's request.

**Types:** `WsOpcode`, `WsFrame`

**Functions:**

- `inline std::string wsAcceptKey(const std::string& clientKey)`
- `inline std::string serverHandshakeResponse(const std::string& clientKey)`
- `inline std::optional<std::string> parseClientKey(const std::string& request)`
- `inline std::vector<std::uint8_t> wsEncodeFrame(WsOpcode opcode, const std::uint8_t* payload,`
- `inline std::vector<std::uint8_t> wsEncodeFrame(WsOpcode opcode, const std::string& text,`
- `inline std::optional<WsFrame> wsDecodeFrame(const std::uint8_t* data, std::size_t n, std::size_t& consumed)`
- `inline std::optional<WsFrame> wsDecodeFrame(const std::vector<std::uint8_t>& data, std::size_t& consumed)`


<a name="video"></a>
## video

### `Ivf`
<sub>`engine/include/maz/video/Ivf.hpp`</sub>

maz::video IVF container demuxer — the container/framing half of Godot's video playback. IVF is the simplest, fully-specified video container (a 32-byte file header + a 12-byte header before each compressed frame), used to wrap VP8/VP9/AV1 bitstreams. Demuxing means: read the file header (codec FourCC, dimensions, frame rate, frame count), then walk the file pulling out each compressed frame's bytes and presentation timestamp. That is the step a player runs BEFORE handing frame payloads to the video codec — you cannot decode or seek video without first demuxing it, and dimensions/fps/duration are what a UI needs first.  Honest scope: this is the container demuxer + metadata, not the VP8/VP9/AV1 codec. Turning a frame's compressed bytes into pixels is a separate, very large codec (and its display is GPU-side). Everything here is exact per the IVF spec and unit-tested against a hand-constructed bitstream, so it is fully verifiable headlessly.

**Types:** `IvfHeader`, `IvfFrame`, `IvfVideo`

**Functions:**

- `inline uint16_t readU16le(const uint8_t* p)`
- `inline uint32_t readU32le(const uint8_t* p)`
- `inline uint64_t readU64le(const uint8_t* p)`
- `inline IvfHeader parseIvfHeader(const uint8_t* data, size_t n)`
- `inline IvfVideo demuxIvf(const uint8_t* data, size_t n)`
- `inline IvfVideo demuxIvf(const std::vector<uint8_t>& bytes)`


