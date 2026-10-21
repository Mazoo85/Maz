# Iron Front — Stage 1 design (core skirmish)

*Working title. A turn-based WWII hex strategy game in the style of World Conqueror 4, built as a
Maz app. Original name, art and data — no EasyTech assets, names, portraits or maps are copied;
only the genre's mechanics are used.*

## Roadmap (each stage gets its own spec → plan → build)

| Stage | Adds |
|---|---|
| **1 (this spec)** | Europe 1939 hex map, 5 unit types, movement, WC4-style combat, fog of war, AI, win/lose, auto-save |
| 2 | Economy: money + industry per turn, buying units, building cities/factories |
| 3 | Generals: attach to armies, bonuses, experience and levels |
| 4 | Historical campaigns/scenarios; long-term, the whole-world conquest map |

Platforms: phone and computer. Stage 1 ships playable on desktop; it is phone-ready (touch +
mouse unified, safe-area UI) but packaging for iOS/Android needs a Mac / Android SDK
(`docs/MOBILE_BUILD.md`), outside this cloud box.

## Gameplay rules

### Setup
- Map: a hand-authored hex map of Europe, about 40×30 pointy-top hexes (axial coordinates,
  `maz::game::Hex`).
- Countries: Germany, United Kingdom, France, Soviet Union, Italy. The player picks one on the
  start screen; the AI plays the other four. Every country is hostile to every other (no
  alliances in Stage 1).
- Each country starts with one **capital**, 4–8 **cities**, and 6–10 units.

### Terrain

| Terrain | Move cost (land) | Defense multiplier `T` | Notes |
|---|---|---|---|
| Plains | 1 | 1.0 | |
| Forest | 2 | 1.25 | Infantry defending here: `T` = 1.5 |
| Mountain | 3 | 1.5 | Tanks attack from or into mountains at ×0.75 |
| City / Capital | 1 | 1.5 | Infantry defending here: `T` = 1.75 |
| Sea | — (naval only) | 1.0 | |

Land units cannot enter sea in Stage 1 (no transports). Naval units move only on sea.
Fighters ignore terrain cost (every hex costs 1) and terrain defense (`T` = 1.0).

### Units

| Unit | HP | Attack | Defense | Move | Attack range | Vision | Domain |
|---|---|---|---|---|---|---|---|
| Infantry | 10 | 4 | 4 | 1* | 1 | 2 | Land |
| Tank | 12 | 7 | 4 | 2* | 1 | 2 | Land |
| Artillery | 8 | 7 | 2 | 1* | 2 | 2 | Land |
| Fighter | 8 | 5 | 3 | 4 | 1 | 4 | Air |
| Destroyer | 10 | 5 | 4 | 3 | 1 | 3 | Sea |

\*Move is in move points, spent by terrain cost; a unit may always move at least one hex if it
has full move points (so infantry can enter forest/mountain, ending its move).

All stats live in one table (`UnitStats`) so they are tunable in one place.

### A turn
1. The current country's units each get one **move** and one **attack**, in either order
   (attacking first ends movement for that unit).
2. **Zone of control:** a land or naval unit that enters a hex adjacent to an enemy unit must
   stop. Fighters are not affected.
3. Moving a land unit onto an unoccupied enemy city or capital **captures** it.
4. The player taps **End Turn**; the AI countries then play in a fixed order (Germany, UK,
   France, USSR, Italy, skipping the player and eliminated countries).

### Targeting
- A unit attacks a hex within its attack range holding an enemy unit.
- Land units (other than artillery) cannot attack ships on sea hexes; artillery and destroyers
  can attack across the coast; fighters can attack anything and be attacked by anything adjacent.

### Combat
For attacker `a` against defender `d`:

```
flank  = 1 + 0.15 × min(3, friendlies of the attacker adjacent to d, excluding a)
roll   = uniform in [0.9, 1.1]   (seeded RNG, so tests are deterministic)
damage = max(1, round( a.attack × (a.hp / a.maxHp) × flank × roll × 10 / (10 + d.defense × T) ))
```
- `a.hp / a.maxHp` makes weakened armies hit less hard (WC4 behaviour).
- Tank mountain penalty (×0.75) multiplies `damage` before rounding.
- If the defender survives and the attacker is in its range (i.e. not an artillery strike from
  2 hexes), it **counter-attacks** with the same formula, `flank` = 1, using its post-damage HP.
- A unit at 0 HP is destroyed. Units do not heal in Stage 1.

### Fog of war
- The player sees hexes within each own unit's vision radius and radius 1 around own cities.
  Explored-but-not-visible hexes show terrain and ownership but not units.
- The AI uses full information (it is not fogged) in Stage 1.

### Elimination, winning, losing
- A country whose capital is captured is **eliminated**: its remaining units are removed, and
  its remaining cities pass to the country that captured the capital.
- **Win:** the player is the only country left. **Lose:** the player's capital is captured.

### AI
Each AI unit, in order, scores its candidate actions with `maz::game::UtilityAI` and takes the
best:
- *Attack* a reachable target — favours expected damage dealt minus counter-damage taken,
  kills, and targets in or next to cities.
- *Capture* an undefended city or capital in reach — high priority, highest for capitals.
- *Advance* toward the nearest enemy city/capital along `hexFindPath`.
- *Hold/retreat* — when below 35% HP and threatened, move toward the nearest own city.

Its moves are animated quickly on screen, with a tap/click to skip to the end of the AI turns.

### Saving
Auto-save after every End Turn and on app suspend (`apps/_template` pattern), to a single save
slot under `user://`. A Continue button on the start screen loads it.

## Screens and controls
- **Start screen:** New Game (pick country) / Continue.
- **Map screen:** drag to pan, pinch or mouse wheel to zoom. Tap a unit to select it: reachable
  hexes highlight blue, attackable enemies highlight red. Tap a highlight to act, tap elsewhere
  to deselect.
- **HUD** (inside the safe area): current country and turn number, a selected-unit panel (type,
  HP, moved/attacked), and an End Turn button sized to `platform::TouchTarget`.
- **End screen:** Victory or Defeat, with New Game.
- Art in Stage 1 is clean flat shapes: terrain-coloured hexes, country-coloured unit counters
  with a simple icon and HP bar, star for capitals.

## Code structure

```
apps/ironfront/
  main.cpp            screen, input, rendering, AI animation (GPU glue — verified in CI)
  ironfront/          pure game logic, no GPU/window, all unit-tested
    Units.hpp         UnitType, UnitStats table
    Map.hpp           terrain, cities, ownership; loads the ASCII map
    MapEurope.hpp     the hand-authored Europe 1939 map + starting armies (ASCII data)
    Movement.hpp      reachable hexes (move points, terrain, ZOC) via HexGrid/HexPath
    Combat.hpp        targeting rules + damage/counter formula
    Game.hpp          GameState, turn flow (TurnOrder), capture, elimination, win/lose
    Ai.hpp            UtilityAI-based country AI
    Fog.hpp           player visibility (FogOfWar)
    Save.hpp          serialise / deserialise GameState
tests/ironfront/      one test file per logic header, registered in tests/CMakeLists.txt
                      (include dir apps/ironfront, like the zomboid sim tests)
```

The app is created with `tools/new-game.sh ironfront "Iron Front"`. Code follows repo
conventions: C++20, header-only, namespace `ironfront`, clean under `-Werror` with
`-Wconversion` etc.

## Testing
- Unit tests for every logic header, e.g.: terrain costs and ZOC limit reach; artillery strikes
  at range 2 without counter; flanking adds 15% per neighbour up to 3; weakened attackers deal
  less; capturing a capital eliminates the country and transfers its cities; last country
  standing wins; the AI captures an undefended adjacent city; save → load round-trips exactly.
- Logic is verified locally by compiling the tests with `g++` (this box has no Vulkan SDK); CI
  does the full build and runs the complete suite. `main.cpp` must also run headless
  (`--headless --frames N`) in CI without crashing.

## Out of scope for Stage 1
Economy and buying units, buildings, generals, unit healing, alliances/diplomacy, transports
and amphibious landings, bombers/submarines/other unit types, campaigns, the world map,
multiplayer, sound, final art.
