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

- **Pistol** — accurate, reliable, and its reserve is **infinite** (the HUD shows `AMMO n / --`). It
  still has to reload when the mag runs out, but it can never run dry — so it's the fallback that
  guarantees you're **never left disarmed** when the power weapons burn through their ammo. Your default.
- **Shotgun** — a spread of pellets that hit hardest **point-blank** and lose damage with distance;
  devastating in a zombie's face, weak across the arena. Up close it also **bodily knocks zombies back**
  — a face-full flings the target away and buys you breathing room — and that shove fades with range just
  like the damage, so it's your create-space button when something's on top of you.
- **SMG** — high rate of fire, low per-shot damage, a slight spread.
- **Railgun** — slow, high-damage hitscan beam that **pierces a whole line** of zombies in one shot,
  and **shears straight through shields** — your go-to answer to armored zombies and Bulwark waves.
- **Flamethrower** — no bullets; a short **cone of fire** that sets everything in it alight. Melts
  close packs, useless at distance. Sweeping it also **paints a lingering ground-fire trail** that keeps
  burning the lane for a few seconds after you stop — and, being fire, it flashes over any acid puddle it
  crosses — so it doubles as area denial, not just a burst cone.

Each weapon has its own magazine and reserve ammo; firing spends the magazine, **R** reloads it. Time it:
tap **R** *again* during the tail end of a reload for an **active reload** — the reload snaps shut
instantly and you get a brief **+30% damage surge** (you glow gold). Miss the window and nothing happens
— no penalty — so it's free power for good timing.
Between waves you earn permanent upgrades (+damage, +fire-rate, +max-health, +ammo, +crit-chance,
+crit-damage, +move-speed, and **shorter dodge-roll cooldown**), cycling automatically — the two crit
upgrades stack, so a crit build lands *both* more often *and* harder as the run goes on; the move-speed
picks make you steadily fleeter; and the dodge-cooldown picks bring your escape roll (and its i-frames)
back sooner and sooner, so the deeper you get, the more often you can bail out of a bad spot.

## The survivor's kit

- **Dodge roll (Space)** — a fast dash with brief **invincibility**. It's also *offensive*: you
  shoulder-check zombies you roll through, knocking them back and hurting them — dash *into* a pinch to
  bulldoze free. The burst of speed also **shakes off a spitter's acid slow**, so a well-timed roll is
  your escape from a caustic puddle instead of slogging out at half speed. **Each kill also shaves a
  little off the dodge's cooldown**, so staying aggressive keeps your roll ready — chaining kills in a
  pinch can refresh a dodge right when you need it.
- **Melee (F)** — a free heavy shove on a short cooldown. It knocks back and **briefly staggers** any
  non-boss it hits — even a brute — so it's a reliable *create-space* button when you're pinned. It also
  **executes** any badly-wounded (<30% health) non-boss outright, and an execute refunds most of the
  cooldown **and siphons a little health back** (executioner's bloodthirst) — so wading into a wounded
  pack to shove-execute stragglers is a real *sustain* button, and chaining finishers both cleans up
  and patches you up. A shove also **bats an incoming spitter acid glob out of the air** if one's in
  reach — a well-timed swing destroys the glob clean, leaving no puddle, so melee doubles as an active
  *deflect* against ranged acid. And you can **shoot globs out of the air too**: a bullet that catches an
  in-flight glob destroys it clean (no puddle) — ranged counterplay so you can snipe a spitter's shot
  down from across the arena, not only dodge or swat it up close.
- **Grenade (G)** / **Molotov (X)** — a thrown frag / a lingering fire patch that both burns *and*
  **slows** anything standing in it, so it holds a lane as area denial, not just chip damage. The
  grenade also **concusses** — anything that survives the blast is briefly **stunned in place**, and a
  staggered body takes the weak-point bonus, so a frag lobbed into a pack sets up your follow-up fire.
- **Mine (T)** / **Sentry (Y)** — a proximity mine, and a stationary auto-turret that thins a lane. The
  sentry has a limited magazine and a lifetime, so place it where it'll earn its bolts. It's a **smart
  turret**: instead of plinking whatever body is merely nearest, it **focus-fires the biggest threat in
  range** — a boss, a summoner (which calls reinforcements), or a healer (which undoes your damage) all
  outrank a slow walker — so its scarce bolts land where they count (nearest only breaks ties between
  equal threats). When it finally powers down (empty or expired) it **self-destructs in a blast** that
  damages and staggers the zombies around it, so planting it deep in the horde earns a farewell explosion
  (safe for you — it only hits zombies). The proximity mine is **cluster-aware**: its blast radius is
  twice its trigger ring, so rather than popping for the first lone straggler to clip the edge, it **holds
  for a worthwhile catch** — it goes off the instant **two or more** zombies are in the ring, or the
  moment a **single** zombie steps right on top of it (so a lone walker in the lane still sets it off, it
  just isn't wasted on one that merely grazes the edge). A mine's blast
  **cooks off explosive barrels**, **daisy-chains through exploder packs**, and **flashes over caustic
  acid puddles** in range (acid is volatile — a hard blast sets it off just like fire), so rigging one
  beside a barrel, a nest of exploders, or a spitter's puddle sets up a massive environmental chain
  reaction.
- **Overcharge (Q)** — a screen-wide ultimate blast that also grants a brief **invulnerability window**,
  so it's a true panic button. Kills charge the meter; unleash it when full. Kills landed **on a hot combo
  streak charge it faster** — up to 3× the meter per kill at ×5 — so keeping a chain alive earns the
  ultimate far more often, and the combo system feeds straight into your panic button. Anything too tough to be
  one-shot — a boss, a Bulwark shield, a beefy elite — is left **deep-frozen** by the blast (and a
  chilled caster is silenced), so the ultimate also cryo-locks the survivors while you regroup.
- **Last-stand adrenaline** — drop below 25% health and a desperation surge kicks in: you **fire
  faster, hit 30% harder, shrug off a quarter of all incoming damage, and your dodge-roll recharges 60%
  faster** until you recover. Being cornered is dangerous, but the surge turns it into your biggest damage
  window, buys you extra survivability, *and* keeps your escape roll ready — a real chance to claw a fight
  back (roll, reposition, roll again) rather than a death spiral.
- **Second Wind** — a stored revive: lethal damage is cancelled once, bursting you back to half health
  with a crowd-clearing nova. Earned again every 50 kills.
- **Body armor** — a bought plate that soaks damage before your health; buy a fresh one with **9**. When
  a hit finally **breaks** the plate it **shatters**, throwing off a concussive burst that shoves and
  staggers the zombies around you — so a plate isn't just a buffer, it hands you a moment of space at the
  exact instant it fails. Wearing one into the crush pays off twice.
- **Hunger** — you slowly get hungry; eat a ration (**E**) to keep it down. If hunger maxes out while you
  still have a ration, you **auto-eat one** rather than take damage — so starvation only hurts once your
  food is genuinely gone. You still **can't heal on an empty stomach**: while you're starving (out of
  food), the passive out-of-combat regen is switched off, so you can't just stand still and shrug the
  hunger off — grab more rations from loot to stop the bleed and start recovering again.

## The salvage economy

Every kill banks **cash**. Spend it mid-fight without pausing at the shop hotkeys (**5–9**): a field
kit (a mine + sentry + molotov in one buy), ammo, grenades, a heal, or an armor plate. Hoard for a panic heal, or stay stocked on offense — your call. The shop **won't let you waste salvage**: a heal at full health, a fresh plate when your armor is untouched, or an ammo refill while you're holding the pistol (its reserve is bottomless, so a refill does nothing) is declined and costs nothing, so a mistimed buy never throws money away. Switch to a power weapon before buying ammo.
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
| 2 | **Brute** | Slow, tanky, and its heavy blow doesn't just hurt — it **hurls you back**, wrecking your position and aim (and can fling you into the rest of the horde), so a brute that reaches you is a real spacing threat. Time a **dodge roll** through its swing and the i-frames let you ride it out untouched. |
| 3 | **Boss** | Huge bullet-sponge that leads every 5th wave; **ground-slams** for heavy damage *and* hurls you back — but it **flashes a warning as it winds up**, so dash clear of the ring to dodge it. **Enrages** below 35% health — faster, slams twice as often, and calls in waves of runners. Felling one always drops a **full care package — a guaranteed medkit *and* a guaranteed power-up** — so grinding the wave leader down pays off big. |
| 4 | **Exploder** | A suicide bomber: it detonates on death *and* **on contact** — the instant it reaches you it blows up in your face, so you can't tank or melee it. **Keep your distance and pop it from range.** Its blast also **chain-detonates other exploders** nearby, so a cluster daisy-chains into a string of blasts: lure them together and pop one to wipe the group — just don't be standing in it. Being a hard, incendiary blast, it **cooks off explosive barrels** and **flashes over acid puddles** in range too, so popping one on a spitter's pool or beside a barrel sets off a big environmental chain — a double-edged one, since it can just as easily catch *you*. |
| 5 | **Spitter** | Hangs back and lobs acid that leaves a caustic **puddle** on the ground — it eats your health *and* **slows you to half speed** while you stand in it, so slog clear rather than tanking it. **Chill it or stagger it and it can't lob** — a Cryo Nova / Frost Field silences it like the casters, and a melee shove or dash-strike roots the throw. But the sludge **corrodes the horde too**: any zombie wading through it gets **bogged down** (slowed), so a spitter's own puddle is a double-edged zone — **kite the swarm through it** to slow the pack (torch it to actually hurt them). |
| 6 | **Splitter** | Bursts into two runners when killed — *unless* it dies **on fire**: a burning splitter is incinerated before it can rupture, so it spawns nothing. **Burn splitters** (molotov, flamethrower, or a spreading blaze) to stop them multiplying instead of shooting them and doubling the problem. |
| 7 | **Summoner** | A back-line necromancer that **keeps its distance** — retreats when you close in while it calls reinforcements. Early on it calls fodder **walkers**, but from the mid-game (wave 5+) it calls faster **runners**, so a summoner left alive stays a real threat deep into a run. Each call **winds up with a tell first**, so you get a beat to burst it, **chill it**, or **stagger it** (a melee shove or dash-strike) to cancel the summon mid-cast. Chase it down. |
| 8 | **Armored** | Modest health behind a heavy damage-absorbing **shield** — break it down first. |
| 9 | **Leaper** | Light and quick; closes the gap in sudden **pounces** — but it **crouches and flashes** as it coils, so read the tell and juke sideways to dodge the lunge. Better yet, **interrupt the coil**: a stagger (a melee shove, a dash-strike, a grenade's concussion) or a chill landed mid-tell **breaks the pounce outright** and leaves it recovering, so punishing the wind-up denies the leap entirely. |
| 10 | **Bloater** | Fat, slow, tanky; ruptures into a **toxic cloud** on death — kill it at range. But **burn it** (flamethrower, molotov, a spreading blaze) and its gas ignites: it erupts into a **fire patch** instead of a poison cloud, denying the toxin and leaving a blaze that cooks the horde. |
| 11 | **Screamer** | Fragile back-line support; periodically **shrieks**, whipping nearby zombies into a speed frenzy (the **boss is immune** — it can't be sped up beyond its own enrage). It **winds up with a tell first**, so you get a beat to react: burst it down, **chill it**, or **stagger it** (a melee shove or dash-strike) during the wind-up and the shriek is cut off entirely. Silence it first. |
| 12 | **Healer** | A back-line medic that periodically **mends** nearby wounded zombies, undoing your chip damage. It **winds up with a tell first**, so you get a beat to kill it, **chill it**, or **stagger it** (a melee shove or dash-strike) during the wind-up to cancel the mend. Fragile and never heals itself — and it **can't mend the boss** either, so it won't refund a wave leader's health bar. Cull it before it patches the rest of the pack back up. |
| 13 | **Warper** | A fragile teleporter that shambles slowly, then **blinks a big chunk of the way to you in a single instant** — but it **shimmers violet and roots itself for a beat as it charges the blink**, so read the tell and shoot it or reposition before it phases in. Better still, **interrupt the shimmer**: a stagger (a shove, dash-strike, or grenade concussion) or a chill landed mid-tell **cancels the blink outright** and puts it on recovery — so punishing the tell denies the teleport, not just delays it. Drop it fast — or **chill it** (Cryo Nova / Frost Field): a frozen Warper can't phase, so cryo pins it in place. |

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
- **Berserk** — a combined offensive surge: fire rate **and** damage both jump at once (where Rapid
  Fire boosts only rate and Double Damage only damage), so it's the "go loud" button — your biggest
  all-round burst of firepower from a single pickup.

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
  Summoner, Screamer, Healer, Warper, or Spitter can't work its ability (call, shriek, mend, blink, or
  acid lob) until the chill wears off — so a Cryo Nova or Frost Field is a hard answer to a support-heavy
  pack. A stagger (melee shove / dash-strike) roots those same abilities mid-action too.
- **Fire spreads** — the offensive mirror of frost shatter: a zombie that **dies while burning** passes
  the flames on, setting every nearby zombie alight. So torching one body in a tight crowd can **cascade
  into the whole pack catching fire** — light the front of a horde and let the blaze chain back through it.
- **Stagger = weak point** — a zombie flinching from a stagger (a melee shove or dash-strike) takes
  **40% more damage** while it reels, so the combo is: knock it off balance, then pour fire in. It
  stacks with the chill bonus.
- **Fire ignites acid** — a spitter's caustic puddle (or a Volatile-horde acid pool) is **flammable**:
  sweep it with the flamethrower's cone or drop a molotov on it and it **flashes over** in one violent
  combustion — a burst of damage that sets alight every zombie caught in it, and the puddle is spent in
  the blast. So the ground a spitter poisons under a pack isn't just something to route around — torch it
  and it becomes a fireball on the horde. Acid is **volatile**, so fire isn't the only trigger: any hard
  blast sets it off — a **mine**, a **grenade**, or an **exploding barrel** whose radius overlaps a puddle
  flashes it over the same way. Lob a grenade onto (or pop a barrel next to) a spitter's pool and it chains
  into a big combined fireball that also lights up the surrounding pack.
- **Explosive barrels** — rusty barrels are scattered around the arena; shoot one to pop a big blast,
  and lure the horde onto them. The blast is **double-edged** — caught in it yourself you take half
  the damage and get flung clear, so detonate them at a distance, never in your own face. **Fire cooks
  them off** too — a molotov thrown onto a barrel, or the flamethrower's cone sweeping over it, pops it
  — and a **grenade** or another barrel's blast will chain-detonate one as well. A popped barrel also
  **leaves a lingering fire patch** where it stood, so the blast keeps denying that ground (and, being
  fire, flashes over any acid puddle it overlaps) for a few seconds after the bang.

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
  behind damage-absorbing shields — break them down with heavy hits), **Volatile** (every body
  ruptures into a caustic acid pool where it falls, so the arena fills with hazard and camping a
  kill-zone poisons the ground under your feet — keep moving), or **Regenerator** (every body slowly
  knits its wounds back shut, so chip damage bleeds away — you must commit real burst to a kill, and
  fire or cold are your answers: a burning body loses health faster than it heals, and a chilled body's
  regen is frozen off), or **Relentless** (the whole horde ignores knockback — your shove, dash-strike,
  mine blasts, and shotgun push stop moving them, so positioning-by-knockback is off the table and you
  lean on damage, chills, staggers, and kiting instead). No two runs feel the same.

At the end of a run you're graded (**D** through **S**) on wave reached, kills, and accuracy, and your
best wave + score persist between sessions.

---

*This guide describes the game as shipped in `apps/zomboid`. Controls are read in `apps/zomboid/main.cpp`;
all gameplay rules live in the `maz::script` program inside `apps/zomboid/game.hpp`.*
