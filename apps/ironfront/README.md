# Iron Front

A turn-based WWII hex strategy game in the style of *World Conqueror 4*, built on Maz.
Europe, 1939: pick one of five powers and conquer the continent.

**Stage 1 (this version):** the core skirmish. Coming next: economy (Stage 2), generals (Stage 3),
campaigns and a world map (Stage 4). Design: [`docs/superpowers/specs/2026-10-21-ironfront-design.md`](../../docs/superpowers/specs/2026-10-21-ironfront-design.md).

## How to play

- **New Game** → choose Germany, the United Kingdom, France, the Soviet Union or Italy. The computer
  plays the other four, and everyone is at war with everyone.
- **Your turn:** tap (or click) one of your units. Blue hexes are where it can move, red hexes are
  enemies it can attack. Each unit may **move once and attack once** per turn; attacking ends its
  movement. Press **End Turn** (or Enter) when you're done. Tap during the computer's turn to skip ahead.
- **Capture** cities by moving onto them. Taking a country's **capital** (gold square) knocks that
  country out and hands you all its cities and land. Be the last power standing to win; lose your
  capital and you lose.
- **Combat:** damage depends on attack vs defense, how healthy the attacker is, terrain, and a small
  random swing. Every extra friendly unit next to the target adds **+15%** (up to 3). The defender
  hits back if it can reach you; artillery firing from 2 hexes away takes no return fire.
- **Terrain:** forest and mountains slow land units and protect defenders; cities protect even more
  (especially infantry). Tanks fight poorly in mountains.
- **Crossing water:** moving a land unit onto the sea turns it into a transport (shown as `~I`,
  `~T`, …). Transports sail 2 hexes a turn, can't attack, and sink easily — escort them.
- **Zone of control:** moving next to an enemy unit stops you there. Fighters ignore this.
- **Fog of war:** you only see enemy units near your own units and cities.
- The game **auto-saves** at the start of each of your turns and when the app is put in the
  background; **Continue** on the title screen picks it back up.

| Unit | HP | Attack | Defense | Move | Range | Notes |
|---|---|---|---|---|---|---|
| Infantry (I) | 10 | 4 | 4 | 1 | 1 | Best defender in forests and cities |
| Tank (T) | 12 | 7 | 4 | 2 | 1 | Weak in mountains |
| Artillery (A) | 8 | 7 | 2 | 1 | 2 | Strikes from 2 hexes with no return fire |
| Fighter (F) | 8 | 5 | 3 | 4 | 1 | Flies over anything |
| Destroyer (D) | 10 | 5 | 4 | 3 | 1 | Sea only; can shell the coast |

**Controls:** drag to pan, pinch or mouse wheel to zoom, Esc to deselect.

## Building and running

```sh
cmake -S . -B build -G Ninja
cmake --build build --target ironfront
./build/bin/ironfront                     # --headless --frames N plays an all-AI game with no window
```

The rules live in `ironfront/*.hpp` (no graphics needed) and are tested by `tests/ironfront/`.
Unit numbers are all in one table in `ironfront/Units.hpp`, and the map is plain text in
`ironfront/MapEurope.hpp`, so balance and geography are easy to tweak.
