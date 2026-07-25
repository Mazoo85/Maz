# ZOMBOID — Player's Guide

A top-down twin-stick zombie survival shooter, and the flagship game built on the Maz engine.
Its entire ruleset is written in `maz::script` and driven on a `scene::SceneTree`; the app in
`apps/zomboid/` renders it and reads your input. You survive an endless, escalating siege — how many
waves can you last, and how high a score can you bank?

Run it: `./bin/zomboid` (or `./bin/zomboid --demo` to watch the autopilot show off every weapon).

---

## Controls

| Action | Key(s) |
|---|---|
| Move | **W A S D** (or arrow keys) |
| Aim | **Mouse** |
| Fire (hold) | **Left Mouse Button** |
| Switch weapon | **1** pistol · **2** shotgun · **3** SMG · **4** railgun · **5** flamethrower |
| Reload | **R** |
| Dodge roll (dash) | **Space** (+ a movement key for direction) |
| Melee shove | **F** |
| Throw grenade | **G** |
| Throw molotov | **X** |
| Lay proximity mine | **T** |
| Deploy auto-sentry | **Y** |
| Overcharge ultimate | **Q** (when the meter is full) |
| Eat a ration | **E** |
| **Shop** — buy field kit ($70: mine + sentry + molotov) | **5** |
| **Shop** — buy ammo ($50) | **6** |
| **Shop** — buy grenade ($40) | **7** |
| **Shop** — buy heal ($60) | **8** |
| **Shop** — buy armor plate ($80) | **9** |
| Restart (after death) | **Enter** |
| Quit | **Esc** |

---

## Weapons

- **Pistol** — accurate, reliable, unlimited-ish reserve. Your default.
- **Shotgun** — a spread of pellets that hit hardest **point-blank** and lose damage with distance;
  devastating in a zombie's face, weak across the arena.
- **SMG** — high rate of fire, low per-shot damage, a slight spread.
- **Railgun** — slow, high-damage hitscan beam that **pierces a whole line** of zombies in one shot,
  and **shears straight through shields** — your go-to answer to armored zombies and Bulwark waves.
- **Flamethrower** — no bullets; a short **cone of fire** that sets everything in it alight. Melts
  close packs, useless at distance.

Each weapon has its own magazine and reserve ammo; firing spends the magazine, **R** reloads it.
Between waves you earn permanent upgrades (+damage, +fire-rate, +max-health, +ammo, +crit-chance,
+crit-damage), cycling automatically — the two crit upgrades stack, so a crit build lands *both* more
often *and* harder as the run goes on.

## The survivor's kit

- **Dodge roll (Space)** — a fast dash with brief **invincibility**. It's also *offensive*: you
  shoulder-check zombies you roll through, knocking them back and hurting them — dash *into* a pinch to
  bulldoze free. The burst of speed also **shakes off a spitter's acid slow**, so a well-timed roll is
  your escape from a caustic puddle instead of slogging out at half speed.
- **Melee (F)** — a free heavy shove on a short cooldown. It knocks back and **briefly staggers** any
  non-boss it hits — even a brute — so it's a reliable *create-space* button when you're pinned. It also
  **executes** any badly-wounded (<30% health) non-boss outright, and an execute refunds most of the
  cooldown **and siphons a little health back** (executioner's bloodthirst) — so wading into a wounded
  pack to shove-execute stragglers is a real *sustain* button, and chaining finishers both cleans up
  and patches you up.
- **Grenade (G)** / **Molotov (X)** — a thrown frag / a lingering fire patch that both burns *and*
  **slows** anything standing in it, so it holds a lane as area denial, not just chip damage. The
  grenade also **concusses** — anything that survives the blast is briefly **stunned in place**, and a
  staggered body takes the weak-point bonus, so a frag lobbed into a pack sets up your follow-up fire.
- **Mine (T)** / **Sentry (Y)** — a proximity mine, and a stationary auto-turret that thins a lane. The
  sentry has a limited magazine and a lifetime, so place it where it'll earn its bolts.
- **Overcharge (Q)** — a screen-wide ultimate blast that also grants a brief **invulnerability window**,
  so it's a true panic button. Kills charge the meter; unleash it when full. Anything too tough to be
  one-shot — a boss, a Bulwark shield, a beefy elite — is left **deep-frozen** by the blast (and a
  chilled caster is silenced), so the ultimate also cryo-locks the survivors while you regroup.
- **Last-stand adrenaline** — drop below 25% health and a desperation surge kicks in: you **fire
  faster, hit 30% harder, and shrug off a quarter of all incoming damage** until you recover. Being
  cornered is dangerous, but the surge turns it into your biggest damage window *and* buys you extra
  survivability — a real chance to claw a fight back rather than a death spiral.
- **Second Wind** — a stored revive: lethal damage is cancelled once, bursting you back to half health
  with a crowd-clearing nova. Earned again every 50 kills.
- **Body armor** — a bought plate that soaks damage before your health; buy a fresh one with **9**.
- **Hunger** — you slowly get hungry; eat a ration (**E**) before it starts costing you health.

## The salvage economy

Every kill banks **cash**. Spend it mid-fight without pausing at the shop hotkeys (**5–9**): a field
kit (a mine + sentry + molotov in one buy), ammo, grenades, a heal, or an armor plate. Hoard for a panic heal, or stay stocked on offense — your call.
Salvage **scales with your combo multiplier**, too: kills landed on a hot streak pay out far more
(up to +200% at ×5), so keeping the chain alive fills your wallet as well as your score. And when you
**clear a wave**, any medkits or power-ups still lying on the field are swept straight to you — so a
drop you couldn't reach in the chaos is never wasted.

---

## The enemies

Fourteen zombie types join the horde as the waves climb:

| Kind | Enemy | Behaviour |
|---|---|---|
| 0 | **Walker** | The baseline shambler. |
| 1 | **Runner** | Fast, fragile; swarms you. |
| 2 | **Brute** | Slow, tanky, hits hard. |
| 3 | **Boss** | Huge bullet-sponge that leads every 5th wave; **ground-slams** for heavy damage *and* hurls you back — but it **flashes a warning as it winds up**, so dash clear of the ring to dodge it. **Enrages** below 35% health — faster, slams twice as often, and calls in waves of runners. Felling one always drops a **full care package — a guaranteed medkit *and* a guaranteed power-up** — so grinding the wave leader down pays off big. |
| 4 | **Exploder** | Detonates on death — shoot it from a distance. |
| 5 | **Spitter** | Hangs back and lobs acid that leaves a caustic **puddle** on the ground — it eats your health *and* **slows you to half speed** while you stand in it, so slog clear rather than tanking it. |
| 6 | **Splitter** | Bursts into two runners when killed. |
| 7 | **Summoner** | A back-line necromancer that **keeps its distance** — retreats when you close in while it calls reinforcements. Chase it down. |
| 8 | **Armored** | Modest health behind a heavy damage-absorbing **shield** — break it down first. |
| 9 | **Leaper** | Light and quick; closes the gap in sudden **pounces** — but it **crouches and flashes** as it coils, so read the tell and juke sideways to dodge the lunge. |
| 10 | **Bloater** | Fat, slow, tanky; ruptures into a **toxic cloud** on death — kill it at range. |
| 11 | **Screamer** | Fragile back-line support; periodically **shrieks**, whipping nearby zombies into a speed frenzy. Silence it first. |
| 12 | **Healer** | A back-line medic that periodically **mends** nearby wounded zombies, undoing your chip damage. Fragile and never heals itself — cull it before it patches the pack back up. |
| 13 | **Warper** | A fragile teleporter that shambles slowly, then **blinks a big chunk of the way to you in a single instant** — but it **shimmers violet and roots itself for a beat as it charges the blink**, so read the tell and shoot it or reposition before it phases in. Drop it fast — or **chill it** (Cryo Nova / Frost Field): a frozen Warper can't phase, so cryo pins it in place. |

Grabbing a **medkit** heals you, and any surplus past full health is **banked as bonus armor** (up to
the plate cap) rather than wasted — so scooping up a kit while already topped up is never a waste.

Some zombies spawn as **elites** — bigger, tankier, worth far more, and they always drop a medkit.
Killing one releases a **shockwave** that knocks back and wounds the surrounding crowd, so felling an
elite in the middle of a pack thins the pack and clears space around the medkit it leaves.

## Power-ups

Rarely dropped by the slain, grabbed off the ground for a short buff:

- **Rapid Fire** — doubles fire rate.
- **Double Damage** — doubles damage.
- **Shield** — brief total immunity.
- **Piercing Rounds** — your bullets punch through several zombies.
- **Cryo Nova** — instantly chills *every* zombie on the field — a panic button when swarmed.
- **Overflow** — infinite ammo and no reloads for a while — hose down a wave without pausing.
- **Frost Field** — a lingering cold aura that keeps *every* zombie crawling at half speed for its
  duration (unlike the one-shot Cryo Nova), so you can reposition or thin a swarm at your leisure.

## Combat systems worth knowing

- **Combo multiplier** — fast, unbroken kills build a score multiplier (up to ×5) that decays if you
  stop killing. A bigger streak buys **grace**: the higher your multiplier, the longer you can go
  between kills before it drops (2.5s at ×1, up to 4.5s at ×5), so a hard-won combo is harder to lose.
- **Overkill gibs** — a hit far bigger than a zombie's health bursts it in a shockwave that can chain
  through a weakened pack. The **harder** the killing blow overkills, the **bigger and wider** the
  burst — so a point-blank shotgun, a railgun line, or a double-damage crit landed deep in a crowd
  throws a far deadlier chain than a hit that only just tips a body over. (Exploders, bosses, and
  splitters have their own death behaviour and don't gib.)
- **Chilled = brittle (and silenced)** — frozen zombies take extra damage, and killing one while it's
  frozen **shatters** it into a spreading freeze. Cold also **silences the back line**: a chilled
  Summoner, Screamer, Healer, or Warper can't work its ability (call, shriek, mend, or blink) until
  the chill wears off — so a Cryo Nova or Frost Field is a hard answer to a support-heavy pack.
- **Stagger = weak point** — a zombie flinching from a stagger (a melee shove or dash-strike) takes
  **40% more damage** while it reels, so the combo is: knock it off balance, then pour fire in. It
  stacks with the chill bonus.
- **Explosive barrels** — rusty barrels are scattered around the arena; shoot one to pop a big blast,
  and lure the horde onto them. The blast is **double-edged** — caught in it yourself you take half
  the damage and get flung clear, so detonate them at a distance, never in your own face. **Fire cooks
  them off** too — a molotov thrown onto a barrel, or the flamethrower's cone sweeping over it, pops it
  — and a **grenade** or another barrel's blast will chain-detonate one as well.

## The siege

- **Endless waves** — a Director spawns each wave larger and tougher, on a ring around you. Clear a
  wave for a score bonus and a permanent upgrade.
- **Flawless streak** — clear a wave without taking a single hit and you bank a **doubled score bonus,
  a health patch-up, and a cash reward** — and the cash escalates the longer your no-hit streak runs
  (25, then 40, 55, … up to 100), so stringing perfect waves together is a real high-skill payout.
  One hit resets the streak.
- **Day / night** — a smooth threat ramp: the horde hunts faster and bites harder toward midnight,
  easing at dawn. Watch the **THREAT** readout. Night is also more lucrative — kills after dusk bank
  **50% more salvage cash**, so braving the dark hours pays off.
- **Wave mutators** — from wave 3, each wave rolls a random modifier shown on the HUD: **Feral**
  (faster), **Hulking** (tougher), **Frenzied** (more of them), **Bulwark** (the whole horde spawns
  behind damage-absorbing shields — break them down with heavy hits), or **Volatile** (every body
  ruptures into a caustic acid pool where it falls, so the arena fills with hazard and camping a
  kill-zone poisons the ground under your feet — keep moving). No two runs feel the same.

At the end of a run you're graded (**D** through **S**) on wave reached, kills, and accuracy, and your
best wave + score persist between sessions.

---

*This guide describes the game as shipped in `apps/zomboid`. Controls are read in `apps/zomboid/main.cpp`;
all gameplay rules live in the `maz::script` program inside `apps/zomboid/game.hpp`.*
