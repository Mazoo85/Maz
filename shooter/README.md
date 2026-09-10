<!-- Part of MAZ ARCADE — see the repo root README for every project. -->

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

- Endless escalating **waves** — clear the horde to advance.
- Three zombie types: **walkers** (basic), **runners** (fast, fragile), and
  **brutes** (slow, tanky, big hits).
- Health ring around the player; **health packs** drop from kills.
- Score, kill count, and a **persistent best score** (saved in `localStorage`).
- Juice: blood particles, muzzle flash, hit flashes, blood decals, screen shake,
  and a hurt vignette.

## Tech

- Single `<canvas>`, `requestAnimationFrame` loop with a fixed delta clamp.
- `devicePixelRatio`-aware scaling for crisp rendering on retina phones.
- Multi-touch handling with per-side touch identifiers so both sticks work at
  once. `touch-action: none` and locked viewport prevent scroll/zoom while
  playing.

---

← Back to the [**MAZ ARCADE hub**](../index.html) · [repository README](../README.md) · [play/open this one](../shooter/)
