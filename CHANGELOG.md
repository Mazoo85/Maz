# Changelog

All notable changes to the Maz Engine are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); the engine version is defined in
`engine/include/maz/core/Version.hpp` (`maz::engineVersion()`) and mirrored in the top-level
`CMakeLists.txt`.

## [Unreleased]

### Flagship game
- **ZOMBOID — new shop item: the field kit ($70, key 5).** The salvage shop now sells a **field kit**
  that restocks the tactical gadgets in one buy — a **mine, a sentry, and a molotov** — giving the
  placement tools a reliable cash source between supply crates instead of relying on crate luck. Joins
  the existing ammo/grenade/heal/armor slots on the shop hotkey row. Headless-tested: buying it spends
  70 cash and adds exactly one mine, one sentry, and one molotov.
- **ZOMBOID — cryo silences the whole back line.** Extending the Warper cryo-counter to the other
  ability users: a **chilled Summoner, Screamer, or Healer can't use its ability** either — no calling
  reinforcements, no frenzy shriek, no mending — until the chill wears off. Cryo (a Cryo Nova or Frost
  Field) is now a consistent, tactical answer to a support-heavy pack, not just crowd control.
  Headless-tested: a chilled healer parked next to a wounded zombie lands no mend across ten frames.
- **ZOMBOID — cryo hard-counters the Warper.** The new teleporting Warper can't phase while chilled:
  a **Cryo Nova or Frost Field now pins it in place** until the chill wears off (a frozen body can't
  blink), giving those cold power-ups a clear tactical use against the game's slipperiest enemy.
  Headless-tested: a chilled Warper with its blink off cooldown stays put instead of teleporting in.
- **ZOMBOID — 14th enemy type: the Warper (kind 13).** A fragile teleporter that shambles slowly
  between blinks, then **teleports half the way to the survivor in a single instant** (on a ~2.5s
  cooldown, only while there's real ground to cover) — erasing a gap a walker never could and
  appearing right on top of you. It joins the horde from wave 8 and drops the usual salvage; kill it
  fast before it ports in. Rendered as a violet body. Headless-tested: a warper 40 units out with its
  cooldown ready teleports to ~20 in a single tick, while one still on cooldown only shambles a hair.
- **ZOMBOID — dodge roll cleanses the acid slow.** A spitter's caustic puddle bogs the survivor to
  half speed while they stand in it; now a **dodge roll clears that slow instantly** — the burst of
  speed shakes it off — so a well-timed roll is a real escape from a puddle instead of a slow slog.
  Headless-tested: a survivor bogged in acid has the slow reset to zero the moment a dash fires.
- **ZOMBOID — escalating flawless-wave streak.** Clearing a wave without taking a hit already paid a
  doubled score bonus, cash, and a heal; now the **cash reward escalates with an unbroken no-hit
  streak** — 25 for the first flawless wave, then 40, 55, … up to a 100 cap — and taking a single hit
  resets the streak to zero. Stringing perfect waves together is now a genuine high-skill payout.
  Headless-tested: three flawless waves in a row pay 25/40/55, a hit resets to 0, and the next
  flawless wave restarts at the 25 base.
- **ZOMBOID — boss bounty (guaranteed care package on a boss kill).** Felling a boss — the wave leader
  that heads every 5th wave — now **always drops a medkit AND a power-up** where it falls, on top of
  the big score and cash, instead of leaving the same rare dice-roll drops as a common zombie. Grinding
  down the hardest target on the field is now a reliable, satisfying payoff. Headless-tested: killing a
  boss leaves at least one active medkit and one active power-up on the ground; nothing drops while it
  still lives.
- **ZOMBOID — sixth permanent upgrade: +crit damage.** The between-wave upgrade cycle gained a sixth
  step that raises the **critical-hit multiplier** (+0.25 each), alongside the existing +damage,
  +fire-rate, +max-health, +ammo and +crit-chance. The two crit upgrades now compound — a crit build
  lands criticals both more often *and* harder over a long run. The upgrade HUD readout now also shows
  the current crit multiplier. Headless-tested: the sixth upgrade raises crit_mult from 2.0 to 2.25
  and the cycle wraps back to +damage on the seventh.
- **ZOMBOID — executioner's bloodthirst (melee execute heals).** A melee shove that **executes** a
  badly-wounded (<30% health) non-boss already refunds most of its cooldown; now each finisher also
  **siphons 5 health back** to the survivor. Wading into a wounded pack to shove-execute stragglers
  becomes a genuine *sustain* button, not just a create-space one, and chaining executes both clears
  and heals. Healthy targets (a normal swing, no execute) grant no heal. Headless-tested: a survivor
  at 50 health rises to 55 after executing one zombie, and stays at 50 when the swing doesn't execute.
- **ZOMBOID — overkill gibs now scale with force.** The chain shockwave from an overkill (a hit that
  dwarfs a zombie's health) used to be a flat 25-damage, radius-4 burst regardless of how hard the
  killing blow landed. Now it **scales with the overkill magnitude** — a monster hit (point-blank
  shotgun, railgun line, double-damage crit) throws a burst up to 70 damage and a wider radius, while
  a body that only just tipped over the threshold still pops the base burst. Splitters are now exempt
  from the gib alongside exploders and bosses, so a huge hit no longer vaporises the runners a splitter
  spawns the same frame. Headless-tested: a threshold kill splashes a neighbour for 25, a monster kill
  for 70.
- **ZOMBOID — last-stand grit (adrenaline damage resistance).** The desperation surge that kicks in
  below 25% health already made the survivor fire faster and hit 30% harder; now it also **cuts all
  incoming damage by 25%** while it's up. The low-health comeback window becomes a genuine fighting
  chance instead of a death spiral — you're most dangerous *and* toughest exactly when cornered.
  Headless-tested: a 20-damage hit removes the full 20 health normally but only 15 while the surge
  is active.
- **ZOMBOID — Volatile Horde wave mutator (5th mutator).** A new random wave modifier (from wave 3
  on): under it, **every non-boss body ruptures into a caustic acid pool where it falls**. As the
  fight drags on the arena steadily fills with hazard, so camping a single kill-zone poisons the
  ground under your own feet and forces you to keep repositioning. Shown as **VOLATILE HORDE** on the
  HUD. Headless-tested: a plain walker — which normally leaves nothing on death — drops a puddle when
  killed under the mutator, and none with it off.
- **ZOMBOID — Frost Field power-up (8th power-up).** A new drop that projects a sustained cold aura:
  while it lasts, **every zombie on the field crawls at half speed** — unlike the one-shot Cryo Nova,
  this is a lingering slow that buys you sustained breathing room to reposition or thin a swarm. The
  survivor glows icy blue while it's active. Headless-tested: a walker covers well under half its
  normal ground toward the survivor while the Frost Field is up.
- **ZOMBOID — night pays better.** The day/night cycle already made the horde faster and deadlier
  after dusk; now kills landed at night also bank **50% more salvage cash**, so holding out through
  the dark hours is a real risk/reward play instead of pure danger. Headless-tested: the same walker
  pays 7 cash killed by day and 10 killed at night.
- **ZOMBOID — grenades set off barrels.** A thrown grenade's frag blast now detonates any explosive
  barrel inside its radius, so lobbing a grenade at a barrel chains into a far bigger explosion —
  completing the "anything explosive/fiery sets off a barrel" rule alongside bullets, other barrels,
  the railgun, molotovs, and the flamethrower. Headless-tested: a barrel inside the grenade's blast
  detonates while one outside it is spared.
- **ZOMBOID — the flamethrower cooks off barrels too.** For consistency with the molotov, the
  flamethrower's flame cone now chips any explosive barrel it sweeps over until the barrel detonates —
  so you can torch a barrel to pop it, not just shoot it. Headless-tested: a barrel held in the flame
  cone cooks off, while one behind the survivor (out of the cone) stays intact.
- **ZOMBOID — elite kills release a shockwave.** Felling an elite champion now bursts a nova that
  knocks back and wounds the surrounding crowd, clearing breathing room around the corpse (and the
  medkit it always drops) — so killing an elite while it's waded into a pack thins the pack too.
  Headless-tested: an elite's death deals 30 damage and knocks back a zombie inside the radius, while
  one standing well clear takes nothing.
- **ZOMBOID — molotov fire cooks off barrels.** A burning molotov patch now chips any explosive barrel
  caught inside it until it detonates, so you can throw a molotov onto (or beside) a barrel to chain the
  flames into a blast — flames for area denial *and* a delayed explosion for burst. Headless-tested: a
  barrel sitting in a fire patch cooks off within a few seconds, while one well clear of the flames
  stays intact.
- **ZOMBOID — leaper pounce is now telegraphed.** The leaper no longer springs the instant its
  cooldown is up: it first **crouches and coils for a beat** (flashing lime), rooted in place, before
  it lunges — and the pounce commits toward wherever you are when the wind-up ends. Read the coil and
  juke sideways to make it whiff. Headless-tested: a ready leaper at mid-range spends its first tick
  coiling (rooted, not airborne), then springs toward the survivor once the wind-up elapses.
- **ZOMBOID — spitter acid now bogs you down.** A spitter's caustic puddle used to only eat at your
  health; now it also **slows you to half speed** while you're standing in it, making it genuine area
  denial — you can't just tank the damage and hold your ground, you have to slog out of it. Headless-
  tested: a survivor standing in a fresh puddle picks up the acid-slow state, while one standing clear
  does not.
- **ZOMBOID — the boss slam is now telegraphed.** The boss no longer slams instantly: it rears back
  for a half-second wind-up (flashing a bright warning white) before the shockwave lands, giving a
  sharp survivor a window to dash or run clear of the radius and avoid the hit entirely. Turns the
  slam from unavoidable chip damage into a readable skill check. Headless-tested: the first tick only
  starts the wind-up (no damage), and the slam lands ~0.5s later, hitting an in-range survivor and
  sparing one who is clear.
- **ZOMBOID — a big combo buys grace.** The kill-streak multiplier no longer decays on a flat timer:
  the higher your multiplier, the longer the gap you can go between kills before it resets (base 2.5s,
  up to 4.5s at ×5). A hard-won streak is now more resilient and worth pushing for, instead of
  evaporating on the same short fuse as a fresh one. Headless-tested: a ×5 streak survives a 3-second
  idle that resets a fresh streak, and still resets once past its own extended window.
- **ZOMBOID — explosive barrels are now double-edged.** A barrel blast used to hurt only zombies;
  now a survivor caught in it takes **half the blast damage** and is **flung clear** (still respecting
  dodge i-frames, shield, and armor). So a barrel is a trap to lure the horde onto, not something to
  detonate in your own face — hug one at your peril. Headless-tested: a survivor 2 units from a
  detonating barrel loses 45 health and is knocked back, while one standing well clear is untouched.
- **ZOMBOID — boss slam now knocks you back.** The boss's ground slam already hammered a nearby
  survivor for damage; now the shockwave also physically **hurls you away from the boss**, so a slam
  clears space instead of just chipping health — you can't simply stand in its face and trade hits.
  Headless-tested: a survivor 5 units from a slamming boss is thrown out past 10 units, straight along
  the boss→survivor axis.
- **ZOMBOID — wave-clear pickup vacuum.** Clearing a wave now sweeps up every medkit and power-up
  still lying on the field and delivers it straight to you, so a drop you couldn't reach mid-fight is
  never stranded and wasted during the between-wave lull. Headless-tested: a medkit and a power-up
  dropped far out of reach are both collected the instant the wave clears (the medkit heals, the buff
  activates).
- **ZOMBOID — last-stand adrenaline now hits harder.** The desperation surge that kicks in when
  you're critically wounded (below 25% health) already made you fire faster; now it also lends **+30%
  damage to every shot**, turning a near-death moment into a genuine comeback window instead of just a
  faster losing battle. Headless-tested: with the flag forced on, a shot deals 1.3× base; dropping to
  25% health flips it on automatically through the survivor's own update.
- **ZOMBOID — railgun is now an armor-piercer.** The railgun's beam **shears any shield clean off**
  before biting into health, making it the definitive answer to armored zombies and the new Bulwark
  waves — where lesser guns stall against the plating, one slug punches straight through. Gives the
  slow, limited railgun a sharp identity as your shield-breaker. Headless-tested: a shielded zombie on
  the beam loses its shield and takes health damage in one shot, while an off-beam one keeps its plating.
- **ZOMBOID — Bulwark wave mutator (4th modifier).** Joining Feral, Hulking, and Frenzied, a wave can
  now roll **Bulwark**: every zombie in it — even a plain walker — spawns behind a damage-absorbing
  shield that must be broken before its health can be touched, punishing weak, spread-out fire and
  rewarding heavy hitters (railgun, shotgun point-blank). Shown as "BULWARK HORDE" on the HUD.
  Headless-tested: under the mutator a wave-5 walker spawns with a 25-point shield; with it off, none.
- **ZOMBOID — weak-point window on staggered zombies.** A zombie that's reeling from a stagger now
  takes **40% more damage** from your shots, so the play is to flinch a tough target with a melee shove
  (F) or a dash-strike (Space) and then pour fire into it while it's defenceless. Stacks with the
  chilled-body shatter bonus for an extremely brittle target. The existing stagger flash already
  telegraphs the window. Headless-tested: an identical 20-damage hit removes 20 health from a calm
  zombie and 28 from a staggered one.
- **ZOMBOID — combo-scaled salvage.** The cash you bank per kill now grows with your streak
  multiplier: nothing extra at ×1, scaling up to +200% at ×5. Killing fast and unbroken pays out far
  more salvage, tightening the loop between the combo system and the shop economy. Headless-tested: an
  identical zombie pays 7 cash killed cold (×1) and 14 killed on a ×3 streak; the killstreak test's
  totals were re-derived to match.
- **ZOMBOID — Healer zombie (12th enemy type).** A back-line medic joins the horde from wave 9: on a
  cooldown it knits the wounds of every nearby zombie inside a radius, restoring a quarter of their max
  health (capped at full, never over), which can steadily undo your chip damage on a tough pack. It's
  fragile and never heals itself, so it's a priority kill — silence it before it patches the swarm back
  up. Renders as a green medic and pulses when it mends. Headless-tested: a wounded zombie inside the
  radius is topped up (a near-full one clamps exactly to max, not past it), while one outside the radius
  is left to bleed.
- **ZOMBOID — Overflow power-up (infinite ammo).** A new seventh power-up: for its duration you fire
  freely — no ammo spent, no reloads — so you can hose down a wave without pausing to reload. Grabbing
  it also tops your current magazine and cancels any reload in progress; the survivor glows warm gold
  while it lasts. Joins the drop pool alongside the other six. Headless-tested: with the buff active a
  long held burst never dips the magazine below full, while the same burst without it drains rounds.
- **ZOMBOID — ultimate grants panic-button i-frames.** Unleashing the charged overcharge (Q) now
  also gives a brief invulnerability window as the screen-wide blast goes off, so it's a true
  get-out-of-jail button you can fire mid-swarm without eating a hit in the same instant. A not-ready
  ultimate still does nothing (no free i-frames). Headless-tested: a charged detonation wipes the
  field, spends the charge, and leaves the survivor briefly untouchable; an uncharged one grants nothing.
- **ZOMBOID — dash strike now staggers too.** The offensive dodge-roll (Space) already shoulder-checks
  zombies it bulldozes through; now it also briefly flinches each one (reusing the same `stagger()` the
  melee shove uses), so dashing *into* a pinch reliably scatters and roots the pack you punch through,
  not just knocks it back. (Also audited every script method used as an expression for the missing-return
  class of bug found last increment — `split_off` was the only case, and it's already fixed.) Headless-
  tested: a zombie the dash passes through is staggered in addition to taking the hit and knockback.
- **ZOMBOID — enraged boss calls in reinforcements.** The boss's second phase (below 35% health) now
  periodically bellows and summons a pair of runners, up to a fixed reinforcement budget, so the climax
  becomes a real scramble instead of a straight damage race — then stops (a bounded flood, no endless
  spawns). Fixes a latent bug the test caught: `Zombie.split_off` never returned its spawn count, so
  callers gating on it (now the boss) misread it as zero. Headless-tested: an enraged boss spawns its
  first add-wave, and after a long run its reinforcement budget is exactly spent with the add count capped.
- **ZOMBOID — melee shove reliably staggers.** The melee shove (F) now briefly flinches whatever
  non-boss it hits, even a heavily-armored brute it can't meaningfully damage in one swing — turning
  melee into a dependable "create space" button when you're about to be surrounded, not just a weak
  hit. The boss stays immune, and the shove's own cooldown bounds it. Headless-tested: a high-wave
  brute (taking far less than the damage-stagger threshold) is flinched by the shove yet survives,
  while the boss shrugs it off.
- **ZOMBOID — molotov fire is now crowd control.** Zombies standing in a molotov's fire patch not only
  burn but **stumble** (a brief slow refreshed every frame they're in it), so the flames actually hold
  a lane instead of just chipping health — real area denial to buy space or funnel the horde. Headless-
  tested: a zombie in the patch is both slowed and alight after one tick, while one clear of it is
  untouched.
- **ZOMBOID — shotgun point-blank damage ramp.** Shotgun pellets now hit hardest fresh out of the
  barrel and fade with travel, down to a 40% floor at the end of their flight — giving the shotgun a
  true close-range identity (devastating in your face, weak across the arena) instead of flat damage
  at any distance. Only the shotgun's pellets fall off; every other weapon is unchanged. Headless-tested:
  a pellet deals full damage fresh, half at half-life, clamps to 40% when nearly spent, a plain round
  ignores travel entirely, and firing the shotgun tags its pellets with the falloff flag.
- **ZOMBOID — sentry ammo.** The deployable auto-turret now carries a limited magazine (25 bolts) and
  shuts down the moment it runs dry, on top of its existing lifetime clock. Against a dense pack it
  burns out fast, so *where* and *when* you place it is a real decision rather than free area denial.
  The sentry dims visibly as its magazine runs low. Headless-tested: with a target always in range and
  time still on the clock, the sentry fires exactly a full magazine, then deactivates out of bolts.
- **ZOMBOID — killstreak milestone rewards.** The combo system now pays out: every 10th unbroken kill
  banks a +15 cash bounty, and every 20th also patches the survivor up — turning a long, unbroken
  streak into a tangible reward on top of the existing score multiplier, and giving the decaying combo
  timer real stakes. Headless-tested: nine kills pay nothing extra, the tenth trips the bounty exactly
  (cash and a run-total milestone counter both advance).
- **ZOMBOID — Summoner now kites.** The Summoner (kind 7) no longer shambles into melee: it holds a
  comfortable range, backing away when the survivor closes in and drifting in only when far off, all
  while it keeps calling reinforcements. That turns it into a proper "chase it down" priority target —
  ignore it and it flees to safety and floods the field. Headless-tested: parked close it retreats
  (distance grows), parked far it drifts inward.
- **ZOMBOID — flawless-wave bonus.** Clearing a whole wave without taking a single hit now **doubles**
  the wave-clear score bonus, pays a cash reward, and patches the survivor up a little — a reward for
  aggressive, clean play that also gives skilled runs a real scoring ceiling. Any hit that lands
  (even one soaked entirely by armor) forfeits the bonus for that wave. Headless-tested: an untouched
  clear pays the double bonus + cash + heal and is flagged flawless, while taking one hit drops it
  back to the base bonus with no cash and no flawless flag.
- **ZOMBOID — Screamer support zombie (12th enemy type).** A fragile back-line zombie (kind 11) that
  periodically shrieks, whipping every nearby zombie into a temporary speed **frenzy** — turning a
  slow shamble into a sudden surge. It makes the horde a priority-target puzzle: silence the screamer
  early or watch the whole pack accelerate. Joins the director's spawn table from wave 7; frenzied
  zombies flush hot orange, the screamer itself is amber. Headless-tested: a shriek frenzies a nearby
  zombie but not a far one, and a frenzied zombie covers measurably more ground than a calm one.
- **ZOMBOID — Vampiric power-up (lifesteal).** A new sixth power-up: for its duration, every kinetic
  hit you land (bullets and the railgun beam) siphons a little health back to you — a comeback tool
  that turns a target-rich swarm into a heal instead of a threat. Joins the drop table alongside
  rapid-fire, double-damage, shield, piercing, and cryo, and tints the survivor crimson while active.
  Headless-tested: a buffed, wounded survivor heals when a bullet connects; an unbuffed one does not;
  and the leech stops the instant the buff expires.
- **ZOMBOID — heavy-hit stagger / flinch.** A single big blow (at least 40% of a zombie's full
  health) that doesn't kill now briefly roots it where it stands — rewarding shotgun point-blanks,
  railgun shots, grenades, and crits with a moment of breathing room. A short per-zombie cooldown
  stops rapid fire from stun-locking anything, and the boss is immune. Flinching zombies flash pale.
  Headless-tested: a heavy hit roots a walker (no advance while flinching), a light hit doesn't (it
  keeps closing), and a second heavy hit during the cooldown does not re-lock it.
- **ZOMBOID — bleed / laceration DoT.** Kinetic rounds (bullets and the railgun beam) now open a
  bleeding wound on the zombies they hit: a stacking damage-over-time that ticks for a few seconds
  after the shot, rewards staying on-target (stacks build up, capped at 5), and finishes fleeing or
  weakened bodies without another bullet. It's distinct from fire (fire ignites; bullets lacerate),
  so the two DoTs layer. Hemorrhaging zombies show a dark-crimson wash that deepens with the stack
  count. Headless-tested: bleed drains health over time, stacks cap at 5, an un-hit zombie takes no
  bleed damage, and a real bullet impact opens a wound on the zombie it strikes.
- **ZOMBOID** (`apps/zomboid`) — a top-down twin-stick zombie **shooter** whose entire simulation is
  written in `maz::script` and driven on a `scene::SceneTree`. Mouse-aim + hold-to-fire pulls rounds
  from an object-pooled bullet system (a script can't spawn nodes, so bullets and zombies are fixed
  pools the scripts activate/recycle); zombies have health and die; a wave Director spawns endless,
  escalating hordes on a ring; loot, hunger→health survival pressure, and the day/night rage ramp
  remain. Score / wave / kills HUD, a hurt-tint on wounded zombies, an aim tracer and crosshair.
  100% of the rules are verified headless in CI; the render app steps a deterministic fixed timestep
  under `--headless`/`--frames` (a 30 s autopilot run reaches wave 4 / 36 kills with no GPU).
- **Weapons** — four switchable guns (1/2/3/4): pistol (accurate), shotgun (6-pellet spread, slow),
  SMG (fast, weaker), and a **railgun** — a slow, high-damage **piercing hitscan beam** that damages
  an entire line of zombies in a single shot (with its own magazine/reserve/reload), spawning a
  fly-through tracer for the visual. Each gun has its own fire rate, damage, spread and bullet speed;
  the shotgun fires a real pellet burst in one shot. Verified headless (`zomboid_sim`): a railgun shot
  pierces three zombies lined up along the aim at once while sparing a body off the beam. Verified headless (`tests/zomboid/sim.cpp`, ctest
  `zomboid_sim`): pools build, wave 1 auto-spawns, firing respects each weapon's cadence, the shotgun
  emits a 6-bullet burst, the SMG out-shoots the pistol over a second, a bullet kills a zombie and
  scores, cleared waves escalate, and survival/loot still hold. Kept as its own fast-compiling target
  so game iterations verify in a second instead of rebuilding the full unit suite.
- **Elite champions** — from wave 2 the Director occasionally crowns a non-boss zombie as an elite:
  2.5× health, a speed bump, 3× score, and a guaranteed medkit on death (plus an extra blood
  flourish). They read at a glance — the app draws them larger and gold-tinted — turning a routine
  zombie into a high-value, high-risk target worth chasing. Verified headless (`zomboid_sim`):
  `make_elite` more than doubles a zombie's health and score, and killing an elite always drops a
  medkit (where an ordinary kill only sometimes does).
- **Boss ground slam** — the boss now has a special attack on top of its bite: every ~4 seconds it
  slams the ground, emitting a radial shockwave (particle burst + heavy screen-shake) that deals a
  flat 25 damage to the survivor if they are within ~10 units. Because the boss is slow, the slam is
  what punishes standing next to it — you have to keep circling. Verified headless (`zomboid_sim`): a
  boss slamming with a survivor 6 units away (inside the shockwave but outside bite range) drains
  their health, while an identical boss 30 units away leaves them untouched.
- **Enemy variety** — four zombie kinds with distinct stats, sprites and sizes: walkers (baseline),
  runners (fast, fragile), brutes (slow, tanky, big, hard-hitting) and a boss that leads every 5th
  wave. The Director mixes kinds into each wave (runners from wave 2, brutes from wave 3); tougher
  kinds award more score, and bullet hit tests use each body's radius so big enemies are easier to
  hit. Verified headless: a forced wave 5 spawns a boss (HP > 300) plus a genuine runner/brute/walker
  mix with runners out-pacing and brutes out-tanking walkers, and a brute survives a single pistol
  shot. Autopilot 45 s run reaches wave 5 (a boss wave).
- **Bullet knockback** — a bullet shoves the zombie it hits back along the shot's direction (a new
  `hit_knockback` on the zombie), giving fire tactile weight and a crowd-control lever — the shotgun's
  burst can stall a rush. Heavy bodies (brutes and bosses, by radius) shrug most of it off. Verified
  headless (`zomboid_sim`): a knockback impulse pushes a walker along the travel direction while a
  brute barely moves.
- **Impact juice** — a script-side particle pool (sparks on every hit, a blood burst on a kill) and a
  decaying screen-shake value that the camera reads (bigger kicks for brutes and bosses), plus an
  app-side muzzle flash. The simulation core is verified headless (`zomboid_sim`): the particle pool
  builds, a kill emits particles and raises `g_shake`, and both ease back to rest within a couple of
  seconds. The muzzle flash and camera shake render on the app side (owner-visible on real hardware).
- **Ammo + reload** — each weapon has a magazine, a reserve, a capacity and a reload time (pistol
  12/48, shotgun 6/24, SMG 30/90). A shot spends one round; emptying the magazine auto-reloads (or
  press R), and switching weapons cancels a reload. Loot doubles as an ammo crate, topping up every
  weapon's reserve. The HUD shows `AMMO mag / reserve` and a RELOADING indicator. Verified headless
  (`zomboid_sim`): firing drains the magazine and an auto-reload refills it from a diminished reserve;
  a weapon with an empty magazine and empty reserve fires exactly its last round then goes dry and
  cannot reload; and collecting loot raises the reserve.
- **End-of-run summary** — the death screen now reports the run: time survived (m:ss), shot accuracy
  (hits ÷ shots, capped at 100%), and total kills alongside the wave/score line. The survivor tracks
  `time_survived` (advances only while alive) and a `hits` counter incremented whenever a bullet or the
  railgun beam connects. Verified headless (`zomboid_sim`): the survive timer accrues ~1 s over 60
  ticks and firing into a parked target raises the hit counter.
- **Kill-milestone rewards** — every 25th kill hands the survivor a bonus grenade and a small heal,
  a steady sustained-play reward that runs alongside the between-wave upgrades. A single `on_kill` hook
  now drives kill tracking, the ultimate charge, and the milestone check. Verified headless
  (`zomboid_sim`): the 25th kill (and not the 24th) grants a grenade and heals, and the next milestone
  advances to 50.
- **Critical hits** — every shot rolls against a crit chance (15% base) for bonus damage (×2), across
  all four weapons (the railgun rolls once for the whole beam). Crit chance is a permanent stat the
  survivor can grow, and the HUD shows the live percentage. Verified headless (`zomboid_sim`): with the
  chance forced to 1.0 a shot deals exactly base×crit-mult, and forced to 0.0 it deals base.
- **Between-wave upgrades** — surviving a wave grants a permanent upgrade, cycling +20% damage, +15%
  fire rate, +25 max health (with a full heal), an ammo top-up, and +5% crit chance. Damage and fire rate are applied
  as multipliers over each weapon's base stats, so the whole arsenal scales together as the run goes
  on. The HUD shows the upgrade count and current DMG/RATE multipliers. Verified headless
  (`zomboid_sim`): each `apply_upgrade` raises the matching stat in turn, and clearing wave 1 hands out
  the first upgrade exactly when wave 2 opens (none before).
- **Chill / slow status + shatter** — grenade blasts now also chill every zombie caught in a slightly
  wider radius, leaving survivors crawling at 40% speed for ~2.5 s (a new `slow_timer` + `apply_slow`
  on the zombie, folded into its movement), and a **chilled body takes 50% extra damage** from any hit
  — a freeze-then-shred combo that turns grenades into a damage multiplier as well as crowd control.
  Chilled zombies read as icy blue in the app. Verified headless (`zomboid_sim`): a chilled walker
  advances less than 60% as far per tick as an unimpaired one, and takes ~1.5× the damage from an
  identical hit.
- **Grenades** — a throwable, object-pooled explosive (start with 3, press G): it flies along the aim,
  slows, and after a short fuse detonates, damaging every zombie inside the blast radius plus a burst
  of blood and screen-shake. Loot tops one up. The HUD shows the grenade count. Verified headless
  (`zomboid_sim`): the pool builds, throwing spends exactly one and arms a grenade, a detonation in the
  middle of a three-zombie cluster kills all three at once, throwing with none left does nothing, and
  loot refills a grenade.
- **Combo multiplier** — fast, unbroken kills build a score multiplier (+1× every 5 kills in the
  streak, capped at 5×) that decays after ~2.5 s without a kill, rewarding aggressive play. Each kill
  scores `score_value × multiplier`. The HUD flashes the live combo. Verified headless (`zomboid_sim`)
  against a hand-computed total: five back-to-back walker kills reach a 5-streak / 2× and score exactly
  60 (four at 1× + one at 2×), and the streak resets to 1× after the window elapses with no kills.
- **Persistent high score + restart** — the best run (best wave + best score) is saved to the platform
  pref dir via `core::KeyValueStore` and shown on the HUD; on death the game saves a new best (flagging
  "NEW BEST!") and offers "PRESS ENTER TO RESTART", which rebuilds a fresh SceneTree for a clean run.
  Verified headless (`zomboid_sim`): `beatsBest` ranks runs correctly (score primary, wave tiebreak),
  and a best written through `KeyValueStore.save()` reloads intact.
- **ZOMBOID player guide** (`docs/ZOMBOID.md`) — a written reference for the flagship game: the full
  control scheme (verified against `apps/zomboid/main.cpp`), every weapon and survivor ability, all
  eleven enemy types and five power-ups, and the combat/siege systems (combos, overkill, frost shatter,
  wave mutators, day/night, the salvage economy). Documentation only — no code change.
- **Frost shatter (chain freeze)** — a chilled zombie killed while it's still frozen now bursts into an
  icy cloud that chills every zombie nearby, so freezing a pack (cryo-nova power-up, chill grenade, or a
  mine's chill) and popping one can cascade the freeze across the whole clump. Composes with the
  existing shatter-damage bonus (chilled bodies already take 1.5×). Verified headless (`zomboid_sim`): a
  frozen zombie's death chills an in-range neighbour but not a far one, while an *unfrozen* zombie's
  death chills nothing.
- **Melee execute finisher** — a melee swing now *finishes off* any badly-wounded non-boss zombie
  (under 30% health) outright, and landing an execute refunds most of the melee cooldown — so cleaning
  up a row of stragglers chains into a fast flurry of swings instead of waiting on the timer. Bosses are
  exempt (no cheap execute). Verified headless (`zomboid_sim`): a melee against a brute at 20% health
  kills it and shortens the cooldown, while a full-health brute takes only the normal swing with no
  refund.
- **Offensive dodge (dash strike)** — the dodge-roll now *shoulder-checks* every zombie it passes
  through: one shove each, knocking them back and dealing dash damage. The dodge is no longer purely
  escape — dashing straight into a clump both grants i-frames and bulldozes a path clear, so a
  well-timed roll through a pinch is genuinely aggressive. Verified headless (`zomboid_sim`): a dash
  into a parked zombie strikes it exactly once (not per-frame) and knocks it further along the dash.
- **Body armor (buyable damage buffer)** — a depletable armor plate the survivor buys from the shop
  (key **9**, $80): incoming hits chip the plate first, and only damage that overflows a *spent* plate
  reaches health — a persistent survivability buffer distinct from the shield power-up's brief total
  immunity. The survivor takes on a steel sheen while plated. Verified headless (`zomboid_sim`): a small
  hit is fully soaked by the plate (health untouched), a bigger hit breaks the plate and passes only the
  overflow to health, and buying armor deducts the cost and fits a full plate.
- **Salvage economy + in-fight shop** — every kill now banks **cash** (shown on the HUD), which the
  survivor can spend mid-fight without pausing: **6** buys an ammo refill ($50), **7** buys a grenade
  ($40), **8** buys a heal ($60). It's the first real player-agency/economy layer — do you hoard for a
  panic heal, or keep topped up on grenades? Purchases are refused (no cash spent) when you can't afford
  them. Verified headless (`zomboid_sim`): a kill pays out salvage, an affordable buy deducts the cost
  and grants the item, and an unaffordable buy is rejected with cash and state untouched.
- **Bloater zombie (new enemy)** — a fat, slow, tanky zombie (kind 10) that *ruptures into a lingering
  toxic cloud when it dies* (reusing the acid-puddle hazard). Popping one at point-blank range leaves you
  standing in poison, so it's best dropped from a distance — a natural fit for the railgun or a well-timed
  grenade. Joins the wave roster from wave 6. Verified headless (`zomboid_sim`): it spawns as a tanky,
  high-value body with no cloud while alive, and a lethal hit both kills it and leaves an active toxic
  cloud behind.
- **Acid puddles (spitter ground hazard)** — a spitter's acid glob now leaves a bubbling green caustic
  puddle where it lands, and the puddle lingers for a few seconds eating away at the survivor's health
  while they stand in it (safe just outside the radius). Where molotov fire is *your* tool against the
  horde, this is the horde's tool against you — it adds real spatial pressure, turning spitters into
  zone-denial enemies that punish holding a spot. Implemented as a new pooled hazard (like fire patches
  and mines). Verified headless (`zomboid_sim`): a puddle on the survivor drains health over time while
  one placed away leaves them unharmed, and a spitter's glob spawns a puddle when it lands.
- **Cryo-nova power-up (new pickup)** — an icy-cyan power-up (kind 4) that works as a panic button:
  grabbing it instantly *chills every zombie on the field*, so a swarm crawls at reduced speed while you
  reload, reposition, or break for a medkit. Joins the random drop table alongside rapid-fire, damage,
  shield, and piercing rounds; the survivor glows pale-cyan while it's fresh. Verified headless
  (`zomboid_sim`): picking it up sets a slow timer on every live zombie regardless of distance, while a
  dormant (unspawned) pool slot is left untouched.
- **Boss enrage phase** — a boss (every 5th wave) that drops below 35% health flies into a permanent
  rage for a climactic second phase: it moves markedly faster and slams the ground twice as often,
  pulsing angry red so you can read the shift at a glance. The transition triggers exactly once, so
  wearing it down turns a lumbering sponge into a genuine sprint to the finish. Verified headless
  (`zomboid_sim`): a full-health boss stays calm, wounding it below the threshold flips the enrage flag
  and boosts its speed, and a subsequent frame doesn't compound the boost.
- **Overkill gibs (chain kills)** — when a single hit lands far more damage than a zombie's full
  health (a railgun bolt, a big crit, a point-blank shotgun blast on an already-weakened body), the
  zombie *bursts* in a small shockwave that chips every nearby zombie — which can pop weakened
  neighbours in turn, chaining through a tight pack. Bosses and exploders keep their own death
  behaviour and are exempt. Verified headless (`zomboid_sim`): a hit dwarfing the target's health gibs
  it and damages an in-range neighbour while sparing one out of range, whereas a merely-lethal hit kills
  cleanly with no shockwave.
- **Wave mutators (run-to-run variety)** — from wave 3 on, each wave rolls a random modifier that
  reshapes the whole horde for that round: **Feral** (everything moves faster), **Hulking** (everything
  is tougher), or **Frenzied** (noticeably more of them). The active modifier is named on the HUD in
  pink (`FERAL HORDE` / `HULKING HORDE` / `FRENZIED HORDE`), so no two runs feel quite the same. Verified
  headless (`zomboid_sim`): against an unmutated baseline, the feral roll spawns a faster walker with
  untouched health, the hulking roll a tougher walker (with matching max-health) at unchanged speed, and
  the director rolls no modifier before wave 3 but a valid one after.
- **Flamethrower (new weapon)** — a fifth weapon (press 5): no projectiles, just a short cone of fire
  in front of the survivor. Everything caught in the cone takes a little direct damage and is *set
  alight*, so the lingering burn does the real work — devastating against a tight pack at close range,
  useless at distance. Carries a big fuel tank (topped up by ammo boxes, crates, and the ammo upgrade),
  and shows as `FLAME` on the HUD. Verified headless (`zomboid_sim`): a single burst scorches and
  ignites a zombie in the cone while sparing bodies behind the survivor, out of range, or off the cone
  axis.
- **Leaper zombie (new enemy)** — a lean, lime-green zombie (kind 9) that doesn't just shamble: at
  mid-range, on a short cooldown, it winds up a *pounce* — a fast burst that closes the gap far quicker
  than a walk before dropping back to a stalk. Light on health but hard to keep at a comfortable
  distance, it punishes standing still and forces you to keep moving. Joins the wave roster from wave 5.
  Verified headless (`zomboid_sim`): with the survivor parked and only the leaper stepped, a pounce
  fires, each walking frame stays within the base walk step, and each pounce frame covers markedly more
  ground than a walk.
- **Piercing-rounds power-up (new pickup)** — a violet power-up (kind 3) that, while active, makes the
  survivor's bullets punch *through* zombies instead of stopping at the first: each round now carries up
  to two extra pierces, hitting a whole line of enemies before it's spent. A mid-tier version of the
  railgun's line-clearing, it turns a packed corridor into a shooting gallery. Bullets track which
  bodies they've already struck so a piercing round can't double-hit the same zombie. Drops (rarely)
  from slain zombies alongside rapid-fire, damage, and shield; the survivor and the pickup both glow
  violet while it's live. Verified headless (`zomboid_sim`): with the buff, one bullet fired down a line
  of three tanky zombies chips all three then expires; without it, the same bullet stops at the first
  and leaves the other two untouched.
- **Armored zombie (new enemy)** — a steel-sheened zombie (kind 8) that carries a heavy damage-
  absorbing shield in front of modest health. Shots (and any damage) chip the shield first; only the
  overflow past a *broken* shield bleeds into its health, so it must be worn down before it can be
  killed — rewarding sustained fire and heavy weapons. Joins the wave roster from wave 8. Verified
  headless (`zomboid_sim`): a hit under the shield is fully absorbed (health untouched), a blow past
  the remaining shield breaks it and passes exactly the overflow to health, and once broken further
  hits damage health directly.
- **Explosive barrels** — six rusty barrels are scattered around the arena from the start. Shoot one
  (it has a small hull that chips down, popping at zero) and it detonates a hefty blast that damages,
  knocks back and ignites every zombie in radius — and chain-reacts to neighbouring barrels. A one-shot
  environmental trap you lure the horde onto, drawn on the ground and flashing hotter as its hull is
  chipped low. Verified headless (`zomboid_sim`): all six start live, a chipping hit leaves a barrel
  standing while a lethal one pops it, and a detonation blasts and ignites a nearby zombie.
- **Smooth day/night danger ramp** — the horde's aggression (which scales both movement speed and bite
  damage) now ramps *smoothly* from 1.0 at dawn/midday up to 1.7 at midnight and back down, via a cosine
  over the day cycle, instead of snapping on at a hard night boundary. Tension builds through dusk and
  eases at first light. The HUD shows the live "THREAT x1.4" multiplier next to the DAY/NIGHT readout.
  Verified headless (`zomboid_sim`): danger is ~1.0 at dawn, ~1.7 at midnight, strictly greater at dusk
  than dawn and less than midnight, and eases back symmetrically in the pre-dawn hours. (Also isolated
  the medkit-expiry test to drive the kit's own timer, since the faster daytime horde could otherwise
  drop fresh kits mid-assertion.)
- **Wave-clear bonus** — clearing a wave now awards a score bonus that scales with the wave number
  (wave × 50), paid once per wave the moment the field is empty, with a pulsing "WAVE N CLEARED  +bonus"
  banner during the breather. It rewards finishing waves cleanly and gives the between-wave lull a
  payoff beat. Verified headless (`zomboid_sim`): after wave 1 starts and the field is wiped, an
  empty-field step pays exactly +50 (tracked on the Director), and a second empty step does not pay
  again (once-per-wave).
- **Ammo drops** — killed zombies now occasionally (~10%) drop a brass ammo box. Walking over it (or
  letting it drift in via magnetism) tops up the active weapon's reserve by two magazines, plus a
  little for the other three, so reserves stay healthy between supply crates and you're rewarded for
  staying on the offensive. Pooled (`Ammo`, 8) and despawns after ~12 s like other drops. Verified
  headless (`zomboid_sim`): the pool builds, a dropped box activates, collecting it tops up the active
  weapon's reserve (and a little for the others), and a box a few units away drifts toward the survivor.
- **End-of-run performance rank** — the death screen now grades each run with a single S/A/B/C/D
  letter, computed from a composite of how far you got (wave), how much you cleared (kills), and how
  cleanly you shot (accuracy). The grade is a pure function (`runRank`/`runRankLetter`) and is colour-
  coded (gold S down to grey D) so a run's quality reads at a glance beyond the raw score. Verified
  headless (`zomboid_sim`): a quick death grades D and a deep, accurate run grades S; the grade is
  monotonic in each input (a deeper/cleaner run never scores lower), accuracy is clamped to 0–100, and
  the five tiers map to distinct letters.
- **Summoner zombie (new enemy)** — a slow, tanky support zombie (kind 7, violet) that periodically
  calls reinforcement walkers from the pool while it lives, up to a finite budget (6). It makes killing
  a target a priority — leave it alive and the horde keeps replenishing; take it out and the tide stops.
  Joins the wave roster from wave 7. Verified headless (`zomboid_sim`): a spawned summoner reports kind
  7 with a full budget, calls at least one walker after its first timer, consumes budget as it summons,
  and stops entirely once the budget is spent (bounded — no infinite spawns).
- **Molotov firebomb** — press X (or, on autopilot, every ~10 s) to hurl a molotov from a small stock
  (starts at 2). It lands ~9 units ahead in your aim direction and leaves a burning fire patch (pooled
  `FirePool`, ~5 s, ~5-unit radius) that re-ignites any zombie standing in it — a proactive way to
  wield the fire system for area denial and choke-point control, not just via exploders. Supply crates
  replenish molotovs (+1). Verified headless (`zomboid_sim`): throwing consumes stock + activates one
  patch, a zombie at the landing point is ignited and burned, the patch burns out after its lifetime,
  and a supply crate replenishes it.
- **Incendiary blast + burning status** — zombies can now catch fire and take damage over time. The
  exploder's death blast is now incendiary: survivors of the initial hit are set alight (3 s at 10
  dps), so shooting an exploder inside a pack lights the whole pack up. Burn damage lands in periodic
  quarter-second ticks (bounded, no per-frame spam), stacks by taking the strongest ignition, and
  burning zombies flicker an ember glow. Verified headless (`zomboid_sim`): an ignited zombie loses
  health over ~1 s and stops taking damage once the fire burns out, and a dying exploder sets a
  neighbouring zombie alight.
- **Auto-turret sentries** — press Y (or, on autopilot, every ~11 s) to deploy a pooled sentry from a
  small stock (starts at 1). It auto-fires a hitscan bolt at the nearest zombie in range (~16 units) a
  few times a second for ~12 seconds, then powers down — a stationary ally that thins one lane while
  you handle another. Supply crates replenish sentries (+1) alongside mines, grenades, and ammo.
  Verified headless (`zomboid_sim`): the pool builds, deploying consumes stock + activates a sentry, an
  in-range zombie takes fire while a far one is ignored, the sentry powers down after its lifetime, and
  a supply crate replenishes it.
- **Splitter zombie (new enemy)** — a bloated mid-tier zombie (kind 6, magenta) that bursts into two
  fast runners the instant it dies, so killing it trades one slow target for two quick ones. It joins
  the wave roster from wave 6 onward. Punishes careless AoE and rewards positioning before the pop.
  Verified headless (`zomboid_sim`): a spawned splitter reports kind 6, and a lethal blow leaves
  exactly two live kind-1 runners spawned within a few units of where it fell (and nothing else alive).
- **Second wind (auto-revive)** — the survivor carries a revive charge (starts with 1). A blow that
  would kill you is cancelled: you burst back with half health, ~2 s of emergency invulnerability, and
  a nova that damages and knocks back the surrounding crowd to buy breathing room. A fresh charge is
  earned every 50 kills (capped at 3), and a HUD readout shows how many you're holding. Turns one
  fatal mistake per charge into a comeback instead of a game over. Verified headless (`zomboid_sim`):
  a 9999-damage killing blow is survived (charge spent, health at half, i-frames set, a parked zombie
  caught by the nova), a follow-up blow after clearing the i-frames with no charge left is fatal, and
  the 50-kill milestone grants a charge back.
- **Proximity mines** — press T (or, on autopilot, every ~9 s) to deploy a pooled proximity mine at
  your feet from a small stock (starts at 2). It arms after a ~0.6 s safety fuse, then detonates the
  instant a zombie steps within trigger range — a heavy blast that damages, knocks back, and chills
  everything in radius. Supply crates now top up mines (+1) alongside grenades and ammo. Adds a
  trap-laying, area-denial layer to the survival toolkit. Verified headless (`zomboid_sim`): the pool
  builds, deploying consumes one from the stock and activates a mine, an unarmed mine ignores a zombie
  sitting on it (safety fuse), it detonates and damages the zombie once the fuse elapses, and a supply
  crate replenishes a mine.
- **Melee shove** — press F (or, on autopilot, when a zombie is point-blank) for a free close-range
  swing that damages and knocks back every zombie in a short radius, then goes on a ~1-second cooldown.
  It costs no ammo, so it's the last-resort answer when a walker is right on top of you or you're mid-
  reload. Verified headless (`zomboid_sim`): a swing strikes only the point-blank zombie (damaging and
  knocking it back), leaves a distant zombie untouched, sets the cooldown, and a second swing during
  cooldown is refused and deals no further damage.
- **Dodge-roll** — press SPACE (or, on autopilot, when a zombie crowds you) for an evasive burst in
  your movement/aim direction, with a brief window of invulnerability (i-frames) that ignores all
  damage mid-roll, then a ~4-second cooldown. The survivor ghosts translucent-blue while invulnerable.
  It rewards timing dodges through spitter volleys, exploder rushes, and boss slams. Verified headless
  (`zomboid_sim`): the dodge fires and sets i-frames + cooldown, a second dodge is refused until it
  cools down, damage taken mid-roll is fully ignored, the burst carries the survivor, and damage lands
  normally again once the i-frames lapse.
- **Supply crates** — every ~30 seconds a care package drops on the ring around the survivor (an
  object-pooled `Crate`); reach it before it expires for a big refill: heal 50, +2 grenades, and a
  generous ammo top-up for all four weapons. It gives a reason to reposition between fights and a
  periodic goal. Verified headless (`zomboid_sim`): the pool builds, a dropped crate becomes active,
  and collecting it grants the grenades, ammo, and heal.
- **Pickup magnetism** — medkits and power-ups within a short radius (~6 units) drift toward the
  survivor instead of sitting inert, so you scoop them up by getting close rather than standing exactly
  on them. Verified headless (`zomboid_sim`): a medkit 5 units away slides toward the survivor over a
  tick while one 20 units away stays put.
- **Health medkits** — a slain zombie has a ~12% chance to drop a medkit; walk over it to heal 40
  (capped at max health). Kits are an object pool, blink as they near expiry, and vanish if ignored,
  giving a reason to push into danger for a top-up. Verified headless (`zomboid_sim`): the pool builds,
  `drop_medkit` activates a kit, walking onto it heals and consumes it, the heal is capped at max, and
  an ignored kit expires.
- **Exploder zombies** — a fifth zombie kind (from wave 4): a fast, fragile suicide bomber that
  detonates when killed, dealing area-of-effect damage to the survivor if they are within ~5 units,
  so it must be shot from a distance rather than let close. Its blast now also catches **nearby
  zombies** (40 damage in the same radius, other exploders excluded so the chain stays bounded), so an
  exploder popped inside a pack can take the pack with it — a double-edged crowd tool. The blast throws
  extra particles and a bigger screen-shake, and the Director folds exploders into the mix on later
  waves. Verified headless (`zomboid_sim`): a detonating exploder hurts a nearby walker while a distant
  one is untouched. Verified headless
  (`zomboid_sim`): a spawned exploder is faster and more fragile than a walker, dying next to the
  survivor drains their health while an identical exploder dying far away does not, and a forced wave 4
  actually contains an exploder.
- **Spitter zombies** — a sixth zombie kind (from wave 5): the horde's one ranged threat. Instead of
  charging, a spitter halts at a distance and lobs object-pooled acid globs at where the survivor was
  standing, on a cooldown; a glob travels and splashes on arrival, hurting the survivor only if they
  are still near the impact — so it can be side-stepped, but a spitter left alone will chip you down
  and must be prioritized. The Director folds spitters into later waves. Verified headless
  (`zomboid_sim`): a spawned spitter holds its ground rather than closing to melee, a tick puts an acid
  glob in the air, and the glob travels and lands on a stationary survivor to drain their health.
- **Overcharge ultimate** — every kill charges a meter (25 kills to full); once ready, press Q to
  detonate a screen-wide blast that hammers every live zombie on the field for 500 damage — wiping
  ordinary hordes outright — then the meter resets. The HUD shows the charge and flashes "OVERCHARGE
  READY"; autopilot fires it the moment it fills. Verified headless (`zomboid_sim`): charging the meter
  to full sets the ready flag, a detonate clears a cluster of live zombies to zero, and the charge is
  consumed back to empty.
- **Out-of-combat regeneration** — take no damage for ~5 seconds and the survivor slowly heals (4 HP/s)
  back toward full, rewarding disengaging and repositioning between fights; any hit resets the delay, so
  standing in the horde never heals you. Verified headless (`zomboid_sim`): health holds steady inside
  the post-hit delay window, climbs once the delay elapses, and never overshoots max health.
- **Adrenaline (last stand)** — dropping below 25% health triggers a passive fire-rate surge (×1.5),
  turning a near-death moment into a fighting chance instead of a slow bleed-out; the boost layers on
  top of the upgrade and power-up multipliers and drops away the instant you heal back above the
  threshold. The survivor pulses red-hot while it's active. Verified headless (`zomboid_sim`): crossing
  below 25% health flips on the adrenaline flag and lifts the fire rate, and healing back above it
  reverts both to baseline.
- **Power-up pickups** — a slain zombie rarely drops an object-pooled power-up; walk over it for a
  short timed buff: rapid fire (fire rate ×2.2), double damage (×2.2), or a shield (all incoming
  damage negated). Buffs layer over the permanent upgrade multipliers and cleanly revert when the
  ~8 s timer lapses; a fresh pickup refreshes it. The survivor is tinted by the active buff (blue for
  shield). Verified headless (`zomboid_sim`): the pool builds, collecting a rapid-fire kit multiplies
  the fire rate and it falls back to base once the window elapses, and a shield fully soaks a 50-damage
  hit.
- **Sound effects** — procedural SFX (`audio::Audio`) for gunfire (per-weapon pitch), zombie deaths,
  reloads, grenade/boss booms, wave starts, taking a bite, and dying. The app fires one-shots by
  watching simulation state change frame-to-frame — no script hooks needed — and resyncs on restart so
  a fresh run stays silent until it acts. Audio degrades gracefully with no device (headless smoke
  runs clean, exit 0); the sound is owner-audible on real hardware.

### Scripting & scene
- `maz::script` — a from-scratch, header-only scripting language (the GDScript competitor): values,
  collections, stdlib+RNG, closures, classes + inheritance + `super`, host binding + lifecycle,
  signals + deferred dispatch, safety budgets + stack traces, hot reload, gradual typing, modules +
  introspection + debugger hooks.
- `scene::SceneTree` / `SceneNode` — unified node hierarchy with 2D transform composition, attached
  scripts, groups, path lookup, and `get_node`/`has_node` script natives; plus `.tscn`-style
  text serialization (`scene::saveTree`/`loadTree`).

### Asset pipeline
- `core::AssetServer<T>` — asynchronous threaded loading with status/progress, dedup + ref-counting,
  and stamp-based reimport / hot reload (Godot's `ResourceLoader` threaded API).

### Infrastructure
- `platform::CrashHandler` — fatal-signal backtrace dump to stderr + a crash-log file, with symbol
  demangling (Godot's `CrashHandler`).
- `tools/package.sh` — one-command self-contained game export with a launch-verify step (Godot's
  "Export Project").
- API docs: `tools/gen_api_docs.py` → `docs/API.md` (144 headers) + a `Doxyfile`.
- `core::Telemetry` — opt-in, privacy-first analytics buffer (off by default, no built-in network
  transport, JSONL, no PII).

### Core systems
- `core::Replay<T>` — deterministic input record/replay with RLE serialization.
- `core::Checkpoints` — named save slots + a rolling rewind ring (rewind mechanics / rollback).
- `core::LinearArena` + `core::PoolAllocator` — frame arena + fixed-size pool allocators.
- `core::SmallVector<T,N>` + `core::SparseSet<T>` — inline-buffer vector + O(1) sparse set.
- `core::TypeDesc<T>` — minimal, type-safe reflection (member-pointer props + serialize).
- `core::DateTime` + `core::GameClock` — deterministic epoch↔calendar + in-game clock.
- `core::Version` — semantic version type (parse/compare) + build macros.

### Spatial & IO
- `game::Quadtree` + `game::Octree` — 2D/3D bounded-region spatial partitioning for broadphase
  culling and range queries.
- `io::VirtualFileSystem` — `res://` / `user://` scheme paths with normalization and a
  traversal-escape guard.

### Build & CI
- Cross-platform GitHub Actions CI (Linux build+test; macOS/Windows unit tests).
- Builds pass `-rdynamic` so crash backtraces resolve symbols.

---

_This project is under active development toward parity-or-better with the Godot game engine. Where a
capability can't be matched honestly (e.g. platform-specific packaging, GPU features requiring a real
device), it is documented as pending rather than claimed._
