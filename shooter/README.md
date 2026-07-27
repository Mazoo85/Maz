# DEAD SECTOR — Top-Down Zombie Shooter

A fast, arcade **twin-stick zombie shooter** built for **phone browsers**. One
self-contained HTML file, zero dependencies, zero build step.

## Play

Open **[`shooter/index.html`](index.html)** in any mobile (or desktop) browser
and tap **PLAY**. To play on your phone, host the repo (e.g. GitHub Pages) and
visit `/shooter/`, or open the file directly on the device.

## Controls

Dynamic dual virtual joysticks — a stick spawns wherever your thumb lands:

- **Left half of the screen** — move.
- **Right half of the screen** — aim, and the gun **auto-fires** while held.

Desktop fallback for testing: **WASD / arrows** to move, **mouse** to aim, and
**hold left-click** to shoot.

## Gameplay

- Endless escalating **waves** — clear the horde to advance, with a **boss every
  5th wave** (big health bar, bullet-ring attack, drops loot on death).
- Six enemy types: **walkers** (basic), **runners** (fast, fragile), **brutes**
  (tanky), **spitters** (ranged, keep their distance), **exploders** (rush and
  blow up), and **bosses**.
- **Four weapons** — pistol, SMG, shotgun, rifle — each with its own damage,
  fire rate, magazine, reload, spread, and pierce. Auto-reload when empty; tap
  the weapon button (or `Q`) to switch between unlocked guns.
- **Between-wave upgrades:** pick 1 of 3 cards — more damage, fire rate, reload
  speed, magazine, move speed, max health, crit chance, pierce, multishot,
  lifesteal, or **unlock a new weapon**.
- **Combo scoring:** chained kills build a multiplier for bigger score.
- **Minimap** (enemies, pickups, boss), **ammo/reload** readout, and a **pause**
  button. Health drops and ammo drops from kills; a **persistent best score**
  (`localStorage`).
- **Synthesized audio** (Web Audio API — no files): weapon SFX, hits, deaths,
  boss stinger, pickups, plus a tension music loop. Mute button included.
- Juice: blood particles/decals, muzzle sparks, hit flashes, floating damage
  numbers, explosions, screen shake, and a hurt vignette.

## Tech

- Single `<canvas>`, `requestAnimationFrame` loop with a fixed delta clamp.
- `devicePixelRatio`-aware scaling for crisp rendering on retina phones.
- Multi-touch handling with per-side touch identifiers so both sticks work at
  once. `touch-action: none` and locked viewport prevent scroll/zoom while
  playing.
