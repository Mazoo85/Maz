# Iron Front Stage 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A playable WC4-style turn-based WWII hex strategy skirmish (Europe 1939, 5 countries, 5 unit types, AI) as the Maz app `ironfront`.

**Architecture:** All rules live in header-only, GPU-free headers under `apps/ironfront/ironfront/` (namespace `ironfront`), each with a standalone test under `tests/ironfront/`. `apps/ironfront/main.cpp` is thin glue: input → `ironfront::Game` actions → flat-shape rendering + HUD text.

**Tech Stack:** C++20, Maz (`maz::game::Hex`/`hexNeighbors`/`hexDistance`/`hexToPixel`/`pixelToHex`, `maz::game::FogOfWar`, `maz::game::UtilityAction`/`selectBestAction`, `maz::core::Pcg32`, `maz::core::KeyValueStore`, `ui::Font`, `render::Renderer::drawConvexPolygon`), SDL3, Vulkan (CI only).

Spec: `docs/superpowers/specs/2026-10-21-ironfront-design.md`.

## Global Constraints

- Compiles clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` (cast narrowings explicitly).
- Header-only logic, no GPU/window dependency, namespace `ironfront`.
- Original name/art/data only — no World Conqueror names, art or maps.
- Every logic header has a test in `tests/ironfront/`, registered in `tests/CMakeLists.txt` with
  `target_include_directories(... ${CMAKE_SOURCE_DIR}/apps/ironfront)`.
- Combat formula, unit table and terrain table exactly as in the spec.
- Local verification (no Vulkan here):
  `g++ -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror -Iengine/include -I$GLM -Iapps/ironfront tests/ironfront/<t>.cpp -o $SCRATCH/<t> && $SCRATCH/<t>`
  with `$GLM` = scratchpad clone of GLM 1.0.1.
- Test style follows `tests/zomboid/sim.cpp`: a `CHECK` macro, `main` returns the failure count.

## Deviation from spec, decided here

- Turn order uses a plain `current` index over the fixed country order rather than
  `maz::game::TurnOrder`: `TurnOrder`'s cursor cannot be restored from a save, and the order is
  fixed anyway. Behaviour is identical to the spec.
- Land hexes carry an owner (−1 = neutral). A land unit entering a hex claims it; neutral cities
  (e.g. Madrid, Warsaw) are capturable like enemy ones. Elimination transfers the loser's hexes
  as well as its cities.

---

### Task 1: Scaffold the app, unit table, test wiring

**Files:** Create via `tools/new-game.sh ironfront "Iron Front"`; create `apps/ironfront/ironfront/Units.hpp`, `tests/ironfront/units.cpp`; modify `tests/CMakeLists.txt`.

**Produces:**
```cpp
namespace ironfront {
enum class Domain { Land, Air, Sea };
enum class UnitType : int { Infantry, Tank, Artillery, Fighter, Destroyer, Count };
struct UnitStats { const char* name; int maxHp, attack, defense, move, range, vision; Domain domain; };
const UnitStats& stats(UnitType t);
}
```
- [ ] Write test: each row matches the spec table (e.g. `stats(Tank).maxHp==12`, `stats(Artillery).range==2`, `stats(Destroyer).domain==Sea`).
- [ ] Run → fails (no header). Implement table. Run → passes.
- [ ] Register `maz_ironfront_tests` (one executable per test file: `ironfront_units`, …). Commit.

### Task 2: Map and the Europe 1939 scenario

**Files:** `apps/ironfront/ironfront/Map.hpp`, `apps/ironfront/ironfront/MapEurope.hpp`, `tests/ironfront/map.cpp`.

**Produces:**
```cpp
enum class Terrain { Plains, Forest, Mountain, Sea };
enum Country : int { Germany, Britain, France, Soviet, Italy, CountryCount }; // -1 = neutral
struct City { std::string name; maz::game::Hex hex; int owner; bool capital; };
class Map {
  int width() const; int height() const;
  bool inBounds(Hex) const; Terrain terrain(Hex) const;
  int owner(Hex) const; void setOwner(Hex, int);
  const City* cityAt(Hex) const; City* cityAt(Hex);
  std::vector<City>& cities(); const std::vector<City>& cities() const;
  static Map fromAscii(const std::vector<std::string>& terrainRows,
                       const std::vector<std::string>& ownerRows);
};
Hex offsetToHex(int col, int row);   // odd-r offset -> axial
void hexToOffset(Hex, int& col, int& row);
struct UnitSpawn { UnitType type; int owner; int col, row; };
struct Scenario { Map map; std::vector<UnitSpawn> units; };
Scenario europe1939();
const char* countryName(int c);
```
- [ ] Tests: offset↔axial round-trips; `fromAscii` reads `.`/`f`/`m`/`~` and owner letters `G U F S I -`; each country in `europe1939()` has exactly one capital and 4–8 cities and 6–10 units, all units on land of their own country (destroyers on sea); every city on land.
- [ ] Implement, pass, commit.

### Task 3: Movement

**Files:** `apps/ironfront/ironfront/Movement.hpp`, `tests/ironfront/movement.cpp`. Also introduces `Unit` and `World` (map + units) used by everything after.

**Produces:**
```cpp
struct Unit { int id; UnitType type; int owner; Hex hex; int hp; bool moved=false, attacked=false; };
struct World { Map map; std::vector<Unit> units;
  Unit* unitAt(Hex); const Unit* unitAt(Hex) const; Unit* unitById(int); };
int moveCost(const World&, const Unit&, Hex to);         // -1 = impassable
std::vector<Hex> reachable(const World&, const Unit&);   // excludes the start hex
```
Dijkstra over move points; occupied hexes are impassable (no stacking); ZOC: entering a hex adjacent to an enemy unit ends movement (not for fighters, which also ignore terrain); a unit with full move points may always enter one adjacent passable hex.
- [ ] Tests: infantry on plains reaches 6 neighbours; tank reaches distance 2 on plains but forest costs 2; infantry can still step into mountain; land can't enter sea, destroyer only sea; ZOC stops tank beside an enemy; fighter flies over sea/mountain 4 hexes and ignores ZOC; occupied hex excluded.
- [ ] Implement, pass, commit.

### Task 4: Combat

**Files:** `apps/ironfront/ironfront/Combat.hpp`, `tests/ironfront/combat.cpp`.

**Produces:**
```cpp
bool canTarget(const World&, const Unit& attacker, const Unit& defender);
float terrainDefense(const World&, const Unit& defender);   // T from the spec table
int computeDamage(const World&, const Unit& a, const Unit& d, float roll, bool counter);
struct CombatResult { int damageDealt, damageTaken; bool defenderDestroyed, attackerDestroyed; };
CombatResult resolveAttack(World&, Unit& a, Unit& d, maz::core::Pcg32& rng); // applies HP, removes dead
```
- [ ] Tests (roll fixed at 1.0 via `computeDamage`): infantry vs infantry on plains = round(4×1×1×10/14)=3; flanking with 1/3/5 friends adds ×1.15/×1.45/×1.45 (capped at 3); half-HP attacker deals half; infantry in city uses T=1.75; tank attacking into mountain ×0.75; artillery at range 2 gets no counter, at range 1 does; min damage 1; land non-artillery can't target a destroyer; destroyed defender is removed.
- [ ] Implement, pass, commit.

### Task 5: Game state and turn flow

**Files:** `apps/ironfront/ironfront/Game.hpp`, `tests/ironfront/game.cpp`.

**Produces:**
```cpp
class Game {
public:
  static Game newGame(int playerCountry, uint64_t seed);
  World& world(); const World& world() const;
  int player() const; int current() const; int turn() const;
  bool alive(int country) const;
  bool isOver() const; int winner() const;      // -1 while running
  bool playerWon() const; bool playerLost() const;
  std::vector<Hex> moveTargets(int unitId) const;
  std::vector<Hex> attackTargets(int unitId) const;
  bool move(int unitId, Hex to);                // validates owner/current/moved; captures
  bool attack(int unitId, Hex target, CombatResult* out = nullptr);
  void endTurn();                               // advance to next alive country, reset flags
  maz::core::Pcg32& rng();
  // save/restore support
  int& currentRef(); int& turnRef(); std::array<bool,CountryCount>& aliveRef(); int& nextIdRef();
};
```
- [ ] Tests: only the current country's units act; move then attack allowed, attack then move denied; capturing an unoccupied enemy city flips it and the hex; capturing a capital eliminates that country (units gone, cities + hexes transferred); `endTurn` skips eliminated countries and bumps `turn()` after the last; player capital captured → `playerLost()`; last country standing → `playerWon()`.
- [ ] Implement, pass, commit.

### Task 6: Fog of war

**Files:** `apps/ironfront/ironfront/Fog.hpp`, `tests/ironfront/fog.cpp`.

**Produces:** `void updateFog(const World&, int country, maz::game::FogOfWar& fog);` (offset x,y indexing) and `bool unitVisible(const maz::game::FogOfWar&, const Unit&)`.
- [ ] Tests: hex within an infantry's vision 2 is visible, distance 3 is not; radius 1 around an owned city visible; after a unit moves away an earlier hex becomes explored-not-visible.
- [ ] Implement, pass, commit.

### Task 7: AI

**Files:** `apps/ironfront/ironfront/Ai.hpp`, `tests/ironfront/ai.cpp`.

**Produces:**
```cpp
struct AiAction { enum Kind { Move, Attack } kind; int unitId; Hex target; };
std::optional<AiAction> nextAiAction(const Game&);   // one step; nullopt = turn done
void runAiTurn(Game&);                               // applies steps until nullopt
```
Per spec: candidates are attack / capture / advance / retreat, scored with `UtilityAction` and `selectBestAction`; one action per call so `main.cpp` can animate.
- [ ] Tests: AI captures an undefended adjacent capital; prefers a killing attack; damaged threatened unit retreats toward own city; full AI-only game from `europe1939()` runs ≤ 400 turns without exceptions and every call terminates (each step marks a unit moved/attacked).
- [ ] Implement, pass, commit.

### Task 8: Save/load

**Files:** `apps/ironfront/ironfront/Save.hpp`, `tests/ironfront/save.cpp`.

**Produces:** `std::string serialize(const Game&)`, `std::optional<Game> deserialize(const std::string&)` — a versioned line-based text format (header `ironfront 1`), holding player, current, turn, alive flags, next id, RNG state, hex owners, city owners, units. The app stores the string with `core::KeyValueStore` under `user://` key `game`.
- [ ] Tests: round-trip after several AI turns is identical (re-serialize equals); garbage / wrong version → `nullopt`.
- [ ] Implement, pass, commit.

### Task 9: The app (screens, input, rendering)

**Files:** `apps/ironfront/main.cpp`; modify `tests/CMakeLists.txt` (headless smoke `ironfront --headless --frames 30` with dummy SDL drivers, same as `zomboid_headless_smoke`).

- Start screen (New Game → 5 country buttons, Continue if a save exists), map screen (drag pan, wheel/pinch zoom, tap select/move/attack, End Turn button sized with `platform::recommendedTouchTargetPx`), AI turns stepped every 0.12 s (tap to skip), end screen.
- Hexes drawn with `drawConvexPolygon` (6 points from `hexToPixel`), owner tint, unit counters as rects with HP bar and a letter (I/T/A/F/D), star marker for capitals, blue/red highlights, fog dimming.
- Autosave after each player End Turn and on suspend. `--headless` runs a short AI-only game so the smoke test exercises the logic.
- [ ] Self-review for `-Wconversion` issues (can't compile here). Commit.

### Task 10: Finish

- [ ] App `README.md` (how to play, controls). Add `ironfront` to the release workflow's default packaged apps.
- [ ] Refresh `PROJECTS.md` via consolidate; run `node scripts/check-links.mjs && node scripts/check-exchange.mjs`.
- [ ] Re-run every ironfront test locally; push the branch; confirm CI.
