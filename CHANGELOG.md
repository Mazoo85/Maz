# Changelog

All notable changes to the Maz Engine are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/); the engine version is defined in
`engine/include/maz/core/Version.hpp` (`maz::engineVersion()`) and mirrored in the top-level
`CMakeLists.txt`.

## [Unreleased]

### Flagship game
- **ZOMBOID — a chilled body can no longer be healed by a Healer (the anti-heal rule is now truly
  unified).** The game's rule is that a body which is **burning, bleeding, or chilled can't heal at all** —
  and the Regenerator mutator and the boss's enrage-heal already honored all three. But the Healer's mend
  only checked burning and bleeding, so a **frozen** zombie in a Healer's radius was still getting patched
  up — even though the Healer's own code called this a "unified anti-heal role." Now cold blocks the mend
  exactly like fire and bleed do, so a Cryo Nova / Frost Field shuts a Healer's mend off just as it shuts
  off a Regenerator's self-heal. Headless-tested: a chilled wounded zombie in range is skipped by the mend
  while a clean-wounded neighbour is patched, alongside the existing burn/bleed cases.
- **ZOMBOID — new wave mutator: the Savage Horde.** An eighth random wave modifier joins Feral, Hulking,
  Frenzied, Bulwark, Volatile, Regenerator, and Relentless. A **Savage** wave isn't faster or tougher — it
  just **bites 60% harder** (every zombie's contact damage x1.6, boss included), so a single body that
  slips past your fire punishes you far more than usual and the run rewards keeping the horde at range
  rather than trading blows. Shown on the HUD as **SAVAGE HORDE** like the other mutators; the director's
  wave-3+ roll now spans all eight. Headless-tested: a walker spawned under the Savage mutator deals
  exactly 1.6x the unmutated bite while its speed and health are untouched, and every director roll across
  waves 3–40 lands inside the valid 1–8 mutator range.
- **ZOMBOID — the Spitter now keeps its firing distance.** The Spitter is the horde's one ranged
  attacker, described as an enemy that "keeps its distance" and lobs acid — but it only ever advanced to
  spitting range and then stood still, so you could stroll right up and melee it down for free. Now, like
  the summoner and the rest of the back line, it **backpedals to reopen a gap** when you push well inside
  its range, holding a proper firing line instead of a passive stop. It stays fully counterable — it has no
  melee bite, and a chill or a stagger still silences it outright — so cornering or freezing it is the
  play; you just have to work past its kiting now. Completes the "ranged and support enemies actually hold
  their distance" pattern across the whole roster. Headless-tested: a Spitter rushed to point-blank
  backpedals away to restore its firing gap.
- **ZOMBOID — Screamers and Healers now actually hold the back line.** Both are described as fragile
  "back-line support" — the Screamer whips the pack into a frenzy, the Healer mends its wounds — and the
  Summoner already backs away from you to stay protected. But Screamers and Healers had no such behaviour:
  they shambled straight into melee like ordinary walkers, so a "back-line" support unit walked right up to
  you and died instantly, undercutting its whole design. They now keep their distance like the Summoner —
  retreating when you close in and drifting back only when you're far — so you have to push through the
  horde or pick them off at range to silence them, making them the genuine priority targets they're meant
  to be. Their shriek/mend still works on the surrounding pack (not on you), so hanging back costs them
  nothing. Headless-tested: a Screamer and a Healer both back away from an approaching survivor instead of
  closing in.
- **ZOMBOID — the wave-clear vacuum now sweeps up ammo boxes too.** Clearing a wave auto-collects any
  medkits and power-ups still lying on the field so a hard-won drop is never stranded during the
  between-wave lull — but ammo boxes (the same kind of small, expiring zombie drop) were being skipped, so
  a box that fell late in a wave just quietly expired unclaimed. The vacuum now also gathers active ammo
  boxes, topping up your reserves on the clear — the same courtesy already extended to medkits and
  power-ups. (Supply crates are intentionally left out — they're a big care package you walk to, not a
  small drop.) Headless-tested: clearing a wave with a medkit, a power-up, and an ammo box all out of reach
  sweeps up all three and refills the active weapon's reserve.
- **ZOMBOID — explosive barrels replenish between waves.** Barrels were a finite, non-renewable resource:
  the arena started with a handful and, once each was popped, they were gone for the rest of the run. So
  the whole "lure the horde onto a barrel and detonate it" playstyle quietly died out a few waves in,
  leaving the endless mode with a bare arena. The Director now restores a couple of spent barrels at fresh
  positions around the survivor at the start of each wave (from wave 2 on), so there's always a live barrel
  or two to set up an environmental kill — the same "keep the supply flowing" idea as the ration fix and
  the crate/gadget refills. Headless-tested: after detonating every barrel, starting a new wave brings two
  back.
- **ZOMBOID — supply crates now restock rations, so starvation is escapable.** The survivor's hunger clock
  is answered by eating rations, but food had only one source — three one-time ration pickups scattered on
  the map. Once those were gone there was *no* way to get more (crates, zombie drops, and the shop all gave
  everything except food), so any run long enough to burn through six rations slid into unavoidable
  starvation damage no matter how well you played. Periodic supply-crate care packages now include **two
  rations**, making the crate the renewable food source the hunger system needs: grab crates and hunger
  stays a manageable pressure; ignore them and it bites. Headless-tested: collecting a crate raises the
  ration count by two (alongside its existing ammo/grenade/heal refill).
- **ZOMBOID — the enraged boss now heals — and damage-over-time shuts it off.** The code justified the
  boss's immunity to healer mends and the Regenerator mutator by saying it "already enrage-heals," but no
  such mechanic existed — the boss never regained health. That's now real: once the boss enters its
  sub-35%-health enrage phase, it slowly knits its wounds (2% of its health bar per second), so the climax
  rewards sustained pressure instead of a leisurely plink. Crucially it obeys the very rule the last two
  updates established — a boss that's **burning, bleeding, or chilled can't heal** — so fire, laceration,
  or cold shut the enrage-heal off completely, making the boss the ultimate showcase of "damage-over-time
  beats healing." Keep the burn on it and it can't recover; let up and it claws HP back. Headless-tested:
  an enraged boss regenerates over a clean second, but a burning one never gains health.
- **ZOMBOID — a Healer can't mend a burning or bleeding zombie.** Building on the rule that a body taking
  damage-over-time can't knit its own wounds (the Regenerator fix), a Healer's mend pulse now also skips
  any zombie that's actively on fire or bleeding — a wound held open by DoT can't be patched by an outside
  medic either. This gives fire and laceration a unified anti-heal role: torching a pack (molotov or the
  flamethrower cone) or raking it with kinetic fire shuts the Healer down, not just the enemy's own regen,
  so DoT is now the clean, consistent answer to *every* heal source in the game. Healers still mend the
  clean-wounded rest of the pack, so they're not defanged — you just have to keep the burn on them.
  Headless-tested: a mend pulse patches a clean-wounded neighbour but leaves a burning one and a bleeding
  one untouched.
- **ZOMBOID — fire and bleed now reliably counter the Regenerator Horde.** A Regenerator wave heals every
  body back up over time (6% of max health per second), and the game bills fire, bleed, and cold as the
  answers. But only *cold* actually halted the regen — fire and bleed merely had to *out-damage* it, and
  because the heal is a percentage of max health, a tanky late-wave regenerator healed ~6+ HP/s, faster
  than a light burn or a single bleed stack could chip. So "burn it to stop it healing" quietly stopped
  working the deeper you got. Now a body that's **burning or bleeding can't knit its wounds at all** —
  regen halts entirely while any damage-over-time is ticking (just like a chill already did), so fire and
  laceration are dependable hard counters at every wave. Headless-tested: a tanky wave-10 regenerator
  loses health under a light burn and under a single bleed stack (cases where the old out-damage-only rule
  would have let it heal through).
- **ZOMBOID — the Frost Field now actually freezes, not just slows (bug fix).** The Frost Field power-up
  is billed as a sustained cryo aura and "a hard answer to support-heavy waves" — freezing casters so a
  Summoner can't call, a Healer can't mend, a Warper can't blink, etc. In practice it only **halved
  movement**: it never applied the *chill status* the rest of the game keys off, so under a Frost Field
  bodies weren't brittle, didn't frost-shatter on death, and — worst of all — **back-line casters kept
  casting** (they don't need to move, so a movement-only slow did nothing to them). The field now refreshes
  the real chill status on every zombie each frame it's up, so it confers the full package a Cryo Nova
  does: +50% shatter damage, frost-shatter chains, and a hard shutdown of Summoner / Screamer / Healer /
  Warper / leaper / Spitter abilities — finally matching what it advertises. Headless-tested: a zombie
  gains the chill status while a Frost Field is active and stays un-chilled without one.
- **ZOMBOID — a sentry's farewell blast now sets off nearby barrels and acid.** When an auto-turret
  sentry powers down (lifetime up or magazine dry) it self-destructs in a blast that damages and staggers
  the surrounding horde. That blast is a real explosion, yet it was the one in the game that ignored
  explosive barrels and caustic puddles — barrels, mines, grenades, and exploder deaths all cook off a
  barrel and flash over acid in range. The sentry blast now does too, so planting a turret beside a
  barrel turns its power-down into a chained explosion (the sentry blast stays friendly to you; the
  barrel it lights is the barrel's own double-edged blast). Completes the "every hard blast sets off
  volatile hazards" rule for the last explosion that skipped it. Headless-tested: a self-destructing
  sentry pops a barrel and flashes over a puddle in range while a barrel well clear survives.
- **ZOMBOID — medkits reach out to a dying survivor.** Dropped health kits already drift toward you once
  you're within a short magnet radius. Now, while you're **critically wounded** (the last-stand
  *adrenaline* state, under 25% health), a kit reaches out from **twice as far** and drifts in **faster**
  — the one lifeline you're desperate for finds you in the scramble instead of sitting just out of reach
  behind the horde. It's a fifth desperation perk on top of adrenaline's existing faster fire, +30% shot
  damage, quicker dodge-recharge, and damage reduction, turning a sub-25% panic into a real comeback
  window. The extra reach is medkit-only — ammo and power-ups keep their normal pull — so it stays a
  survival lifeline, not a blanket loot magnet, and it reverts the instant you heal back above the line.
  Headless-tested: a wounded survivor pulls in a kit sitting 9 units away (past the normal 6-unit magnet)
  while a healthy one leaves the same kit untouched.
- **ZOMBOID — lighting a caustic puddle now sets off a barrel underneath it.** A spitter's acid pool
  flashes over into a violent fireball when a naked flame or blast touches it — that flash already burned
  the horde, but it ignored any explosive barrel sitting in it. Now the flash-over cooks off a barrel it
  engulfs, exactly the way fire and blasts do, so a puddle that lands on a barrel becomes a two-stage bomb:
  torch the acid, the flash pops the barrel. This finishes the rule that every violent combustion in the
  game (fire, blast, and now the acid flash-over) can detonate a barrel. Headless-tested: an acid pool
  combusting pops a barrel sitting on it while one well outside the flash is left standing.
- **ZOMBOID — the railgun now pops explosive barrels.** Every other gun detonates a barrel by shooting it,
  but the railgun's hitscan beam passed straight through one (its visual tracer deals no damage), so the
  most powerful weapon was the only one that couldn't set off a barrel. The beam now detonates any barrel
  it straddles, using the same ray test it already uses to shear through a line of zombies — so lining a
  railgun shot up through a zombie pack *and* a barrel chains the whole thing. Headless-tested: a barrel on
  the beam detonates while one off the beam is untouched.
- **ZOMBOID — a screamer can't frenzy the boss.** A screamer's shriek whips nearby zombies into a speed
  frenzy — and it used to catch the boss too, stacking on top of the boss's own enrage into a nearly
  uncatchable wave leader. The boss is now immune to frenzy (it's a self-contained fight tuned by its
  enrage phase), matching the way it's already exempt from stagger, gib, overkill, the Volatile mutator,
  and healer mends. Screamers still frenzy the rest of the pack. Headless-tested: a shriek leaves the boss
  un-frenzied while a nearby walker is whipped up.
- **ZOMBOID — new wave mutator: the Relentless Horde.** A seventh random wave modifier joins Feral,
  Hulking, Frenzied, Bulwark, Volatile, and Regenerator. A **Relentless** wave plants its feet: the whole
  horde **ignores knockback** — your melee shove, dash-strike, mine blasts, and shotgun push all stop
  moving them. Positioning-by-knockback is off the table that wave, so you lean on raw damage, chills,
  staggers, and kiting instead — a fresh tactical wrinkle that rewards a different toolset. Shows up on the
  HUD as **RELENTLESS HORDE** like the other mutators. Headless-tested: a shove that moves a normal walker
  leaves a Relentless one planted, and clearing the mutator restores normal knockback.
- **ZOMBOID — the dodge recharges faster in a last stand.** The critical-health surge (**adrenaline**,
  under 25% health) already lends faster fire, +30% shot damage, and damage reduction — now it also
  recharges the **dodge-roll 60% faster**, so your escape roll and its i-frames come back sooner exactly
  when you're desperate. It turns the sub-25% scramble into a real comeback window (roll → reposition →
  roll again) rather than a slow death, and reverts the moment you heal back above the threshold.
  Headless-tested: over the same elapsed time a critically wounded survivor's dodge cooldown drops further
  than a healthy one's.
- **ZOMBOID — summoner reinforcements scale with the run.** A summoner used to call plain **walkers** no
  matter how deep you were, so a late-game summoner just trickled in fodder you easily outran — no longer a
  real reason to prioritise it. From the mid-game on (wave 5+) it now calls faster **runners** instead, so
  a summoner left alive stays a genuine threat and hunting it down (or interrupting its cast) matters at
  every stage. Early summoners still call walkers, keeping the opening waves gentle. Headless-tested: a
  wave-7 summoner's reinforcement is a runner, while a wave-1 summoner's is a walker.
- **ZOMBOID — a healer can no longer top up the boss.** The healer zombie's mend pulse restored 25% of a
  target's max health, and it applied to *any* wounded zombie in range — including the boss. On the
  bullet-sponge boss that meant a single pulse could refund a huge slice of the health bar you'd been
  grinding down, undercutting the whole boss duel. The boss is now exempt from the mend (it still heals the
  surrounding pack), matching the way the boss is already exempt from stagger, gib, overkill, and the
  Volatile mutator. Headless-tested: a wounded boss parked next to a healer takes no heal while a nearby
  walker is still mended.
- **ZOMBOID — an exploding zombie now sets off the environment too.** The exploder's incendiary death blast
  already chained through other exploders and ignited nearby zombies, but — unlike the player's mines,
  grenades, and barrels — it didn't touch the surroundings. Now it **cooks off explosive barrels** and
  **flashes over caustic acid puddles** in its radius, completing the "any hard blast sets off volatile
  hazards" rule for the last blast type that lacked it. Popping an exploder on a spitter's pool, or beside
  a barrel, now sets up the same big environmental chain reaction your own explosives do (and, as ever, the
  blast cuts both ways). Headless-tested: a dying exploder detonates a barrel and combusts a puddle in
  range while ones outside it are untouched.
- **ZOMBOID — a chilled or staggered spitter can't lob acid.** The spitter — the horde's one ranged
  attacker — could keep sniping you even while frozen solid or staggered mid-flinch, unlike the back-line
  casters (which a chill silences and a stagger interrupts). Its acid throw is now gated the same way: a
  **Cryo Nova / Frost Field** (or the ultimate's cryo-lock) silences a spitter just like a summoner, and a
  **melee shove / dash-strike** that reaches it roots the throw. Control effects now shut down *every*
  enemy attack consistently — movement, telegraphed abilities, and the spitter's ranged glob alike.
  Headless-tested: a chilled spitter lobs nothing until it thaws, and a staggered one can't throw either.
- **ZOMBOID — you no longer starve to death with rations in your pack.** At max hunger the survivor used to
  simply lose health, even while carrying food. Now, when hunger hits the top, you **auto-eat a ration**
  instead of taking starvation damage — the drain only bites once your food is genuinely gone. Eating drops
  hunger by a chunk, so it won't re-trigger until hunger climbs back to max: a self-paced auto-feed that
  stretches your rations, in keeping with how the shop and pickups already refuse to squander a resource.
  Headless-tested: a starving survivor with rations spends one and takes no damage; a survivor with no food
  still takes the starve drain (existing test).
- **ZOMBOID — the shop won't sell a wasted ammo refill on the pistol.** The pistol's reserve is bottomless
  (it never runs dry), so buying an ammo refill while it's equipped spent 50 salvage on a pool that's never
  drawn down. The shop now declines that buy for free — the same "never throw away hard-won salvage" guard
  that already blocks a heal at full health or a fresh armor plate over an untouched one — and the refill
  still lands normally on any power weapon (which has a finite reserve worth topping up). Headless-tested:
  an ammo buy on the pistol is refused with no cash spent, while the same buy on the SMG spends the cash
  and grows its reserve.
- **ZOMBOID — a burning bloater erupts into fire instead of a toxic cloud.** A bloater normally ruptures on
  death into a lingering caustic pool (poison ground you must route around). Now *how* it dies matters: a
  bloater killed while **on fire** has its volatile gas ignite, so it bursts into a **fire patch** instead
  of a poison cloud — converting the enemy hazard into one that cooks the horde. This gives the flamethrower
  and molotov a defined job against bloaters (torch them to deny the poison and leave a blaze), mirroring
  how fire flashes acid over and how burning already denies a splitter its runners. Headless-tested: a
  burning bloater's death leaves a fire patch and no acid, while a normal kill still leaves a toxic cloud.
- **ZOMBOID — a stagger now interrupts caster wind-ups (summoner, screamer, healer).** The support
  enemies' telegraphed abilities could previously only be fizzled mid-cast by a **chill** — a **stagger**
  (a melee shove, a dash-strike, a heavy staggering hit) rooted them but let the cast finish. Now a stagger
  landed during the wind-up cancels the ability outright, exactly as it already breaks a leaper's coil and
  a warper's blink. This makes the melee shove and dash-strike a consistent, universal interrupt against
  *every* telegraphed enemy ability, so you can shove a summoner out of its reinforcement call, a screamer
  out of its frenzy shriek, or a healer out of its mend pulse. Headless-tested: a summoner staggered
  mid-tell spends no reinforcement, while an undisturbed one completes the call.
- **ZOMBOID — proximity mines now wait for a worthwhile catch.** A mine's blast radius (6) is twice its
  trigger ring (3), so popping for the first lone straggler to clip the edge threw away most of the blast.
  The trigger is now cluster-aware: a mine detonates the instant **two or more** zombies are inside the
  trigger ring (a cluster its blast can engulf), or the moment a **single** zombie steps point-blank onto
  it (half the trigger radius) — so a lone walker in the lane still sets it off rather than strolling over
  a dud, but the mine holds for the group when it can. Headless-tested: one zombie merely clipping the ring
  leaves the mine armed; a second zombie joining the ring detonates it and catches both.
- **ZOMBOID — deployable sentries now focus-fire the biggest threat.** The auto-turret used to shoot
  whatever zombie was simply *nearest*, so it would happily empty its scarce magazine into slow walkers
  while a boss strolled past. It now ranks targets in range by kind-threat (boss > summoner > healer >
  brute > screamer > armored > spitter > … > runner > walker) and fires on the most dangerous one, using
  nearest only to break ties between equal threats. A sentry planted in a mixed pack now spends its bolts
  where they count. Headless-tested: with a walker nearer than a boss, the sentry's bolt lands on the boss
  and the walker is left untouched.
- **ZOMBOID — grenades and barrels now flash over acid puddles.** A frag grenade or an exploding barrel
  whose blast overlaps a spitter's caustic pool now combusts that acid on the spot (the same volatile
  flash-over that a naked flame or a mine already triggered), completing the "any hard blast sets off
  volatile acid" rule. Lob a grenade onto — or pop a barrel next to — a spitter's puddle and it chains
  into a big combined fireball that also ignites the surrounding pack. Headless-tested: a puddle inside
  the blast radius combusts while one well outside it stays a puddle.
- **ZOMBOID — new between-wave upgrade: shorter dodge-roll cooldown.** The permanent upgrade cycle grows
  from seven picks to eight, adding a **-dodge-cooldown** upgrade that trims the dodge-roll's recharge
  (down to a 0.6s floor) so your escape roll — and its i-frames — comes back sooner the deeper you get. A
  defensive/mobility pick distinct from the existing +move-speed one, giving survivability builds another
  lever. Headless-tested: the eighth upgrade lowers dodge cooldown and the cycle now wraps at eight.
- **ZOMBOID — the Exploder is now a true suicide bomber (detonates on contact).** Previously an exploder
  only blew up when killed, so you could safely walk up and melee or tank it point-blank. Now the instant
  it reaches the survivor it **detonates on contact** — killing itself and catching the adjacent player in
  the blast — so exploders are a genuine "keep your distance and pop it from range" threat instead of a
  free melee target. (Its death/contact blast still chain-detonates other exploders.) Headless-tested: an
  exploder that reaches the survivor detonates (dies) and damages the adjacent player, rather than a
  harmless bite.
- **ZOMBOID — burning a Splitter now prevents its split.** A splitter normally bursts into two fast
  runners when killed. Now, if it dies **while on fire**, it's incinerated before it can rupture and
  spawns nothing — giving fire (a molotov, the flamethrower, or a spreading blaze) a specific job:
  **burn splitters to stop them multiplying** instead of shooting them and doubling the threat. A clean,
  discoverable counterplay that ties into the fire systems. Headless-tested: a burning splitter killed
  leaves zero new zombies, while an unlit one bursts into two.
- **ZOMBOID — starving now actually bites: no healing on an empty stomach.** The passive out-of-combat
  regen (+4/s) previously ran even while starving, quietly out-healing the hunger drain (−3/s) — so
  hunger applied *no* real pressure if you stood still. Regen is now **suppressed while starving** (hunger
  maxed), so an empty stomach genuinely drains you and a ration is the only way to stop the bleed and
  start recovering. Eating (which lowers hunger) re-enables regen as before. Headless-tested: a starving,
  out-of-combat survivor loses health, while a fed one in the same window regenerates.
- **ZOMBOID — body armor now shatters into a shove when it breaks.** A plate used to just soak damage and
  quietly deplete. Now the hit that **breaks** the plate throws off a concussive burst that knocks back
  and staggers every zombie within radius 5 — buying the survivor a breath of space at the exact moment
  their armor gives out. It fires only on the break (not on hits the plate merely soaks), so wearing a
  plate into a crush pays off twice: it eats a hit, then clears room as it fails. Headless-tested: a hit
  that spends the plate flings a nearby zombie back and staggers it, while a hit the plate merely soaks
  leaves the zombie undisturbed.
- **ZOMBOID — the shop no longer lets you waste salvage on a no-op buy.** Buying a heal at full health,
  or an armor plate when the current one is still full, previously charged the cash and did nothing. Both
  are now **declined without charging** (the buy returns false and the wallet is untouched), so a
  mistimed purchase in the heat of a fight never throws hard-won salvage away. Genuine buys (a heal that
  actually restores health, a plate on scratched armor) still go through. Headless-tested: full-HP heal
  and full-armor plate are both refused with no charge, while a real heal still costs its cash.
- **ZOMBOID — a Warper's blink can now be interrupted mid-tell.** Like the Leaper's pounce, the Warper's
  teleport telegraphs with a rooted shimmer — but previously a stagger only *paused* it (it resumed and
  blinked the instant the flinch wore off). Now a **stagger** (shove / dash-strike / grenade concussion)
  or a **chill** landed during the shimmer **cancels the blink outright** and puts it on recovery, so
  punishing the tell denies the teleport instead of merely delaying it. This makes the interrupt
  counterplay consistent across the Leaper and Warper. Headless-tested: a warper staggered mid-shimmer
  never blinks (its tell resets and it stays put), while an undisturbed one still teleports in.
- **ZOMBOID — you can now shoot a spitter's acid glob out of the air.** A bullet that catches an
  in-flight glob destroys it clean — no puddle left behind, exactly like a well-timed melee swat. This
  adds **ranged** counterplay to the spitter's one ranged threat: snipe the glob down from across the
  arena instead of only dodging it or swatting it at melee range. A piercing round shears through and
  keeps going; a normal round is spent knocking it down. Headless-tested: a bullet fired into a hovering
  glob deactivates it, the round is consumed, and no acid puddle is created.
- **ZOMBOID — the pistol is now an infinite-reserve sidearm (never left disarmed).** Previously the
  starting pistol could run bone-dry — empty magazine *and* empty reserve — leaving you unable to fire or
  reload until you scrounged ammo or cash, a genuine dead-end (worst in the early game). The pistol now
  has a **bottomless reserve**: it still has to reload when the mag empties, but it can never be
  exhausted, so it's the reliable fallback when the power weapons (shotgun / SMG / railgun / flamethrower)
  burn through their ammo. Those weapons keep their scarcity unchanged. The HUD shows the pistol as
  `AMMO n / --`. Headless-tested: firing the pistol's last round with an empty reserve auto-reloads and
  keeps firing, while a non-pistol with an empty reserve still goes dry after its last shot.
- **ZOMBOID — fire now spreads through a burning horde.** The offensive mirror of frost shatter: a zombie
  that **dies while on fire** passes the flames on, igniting every nearby zombie (within radius 4.5, at
  the intensity it was burning). So torching one body in a tight crowd can **cascade into the whole pack
  catching fire** — light the front of a horde and let the blaze chain back through it, the way killing a
  frozen body already spreads a freeze. Headless-tested: a burning zombie killed sets an in-range
  neighbour alight while a far one stays cold, and a zombie killed unlit spreads nothing.
- **ZOMBOID — a Leaper's pounce can now be interrupted mid-coil.** The Leaper telegraphs its lunge by
  crouching and coiling for a beat — previously you could only juke sideways from it. Now a **stagger**
  (a melee shove, a dash-strike, or a grenade's concussion) or a **chill** landed during that wind-up
  **breaks the pounce outright**: it uncoils harmlessly and has to recover before it can coil again. So
  punishing the tell denies the leap entirely — the same readable counterplay the back-line casters
  (Screamer / Healer / Summoner) already have. Headless-tested: a leaper staggered mid-coil never leaves
  the ground, while an undisturbed one still pounces.
- **ZOMBOID — a mine's blast now flashes over caustic acid puddles.** Acid is volatile: a naked flame
  already combusts it (molotov / flamethrower / barrel-fire), and now a **hard explosion does too** —
  a proximity mine's detonation ignites any spitter puddle (or Volatile-horde pool) in its blast radius,
  chaining the trap into a fiery flash-over that also sets the surrounding pack alight. It joins the
  mine's existing barrel-cook and exploder-daisy-chain, so rigging a mine on the hazards already
  downrange sets up a bigger environmental chain reaction. Headless-tested: a puddle inside the blast
  radius combusts while one outside is untouched.
- **ZOMBOID — caustic acid puddles now corrode the horde, not just you.** A spitter's acid puddle (and
  a Volatile-horde pool) still eats your health and bogs you down — but now **any zombie standing in it
  is slowed too**, refreshed for as long as they wade through it. It deals the horde no bonus damage
  (already-dead flesh doesn't bleed — to actually hurt the pack you still **burn** the pool, which
  flashes it over), so the puddle becomes a double-edged battlefield: it punishes you for holding a spot,
  but you can **kite the swarm through it** to slow the whole pack. Spitters are now a threat that cuts
  both ways. Headless-tested: a zombie inside the pool radius is slowed after one caustic tick while one
  just outside is untouched.
- **ZOMBOID — the Summoner now telegraphs its call (back-line trio complete).** Completing the readable
  back line, the Summoner no longer calls reinforcements the instant its cooldown is up — it **winds up
  with a tell first**, so you get a window to burst the fragile caster (or **chill it**) before the
  reinforcement arrives. Killing it during the wind-up cancels the call; a chill mid-tell fizzles it.
  Now all three back-liners (Summoner, Screamer, Healer) telegraph their abilities, so target-priority is
  a reaction you can see. Headless-tested: no reinforcement on the tell tick but one arrives once the
  call lands, and chilling the summoner during the wind-up prevents it.
- **ZOMBOID — the flamethrower now lays a lingering ground-fire trail.** Sweeping the flamethrower now
  paints **burning ground** mid-cone (on a short throttle), so the flames keep denying a lane for a few
  seconds after you stop firing — and, being fire, the trail **flashes over any acid it touches** (pairs
  with fire-ignites-acid). It gives the flamethrower real area-denial on top of its point-blank cone,
  without starving the shared fire pool (throttled to one patch at a time). Headless-tested: the first
  flamethrower shot lights a ground-fire patch and arms the throttle; an immediate second shot lays none.
- **ZOMBOID — the Healer now telegraphs its mend (readable counterplay).** Like the Screamer, the Healer
  no longer mends the wounded pack the instant its cooldown is up — it **winds up with a tell first**,
  giving you a window to kill the fragile back-liner (or **chill it**) before it undoes your chip damage.
  Killing it during the wind-up cancels the mend; a chill mid-tell makes it fizzle. Headless-tested: a
  wounded neighbour isn't healed on the tell tick but is once the mend lands, and chilling the healer
  during the wind-up prevents the heal entirely.
- **ZOMBOID — kills now shave the dodge-roll cooldown (aggression sustains mobility).** Every kill trims
  **0.3s off the dodge cooldown**, so staying on the offensive keeps your escape ready — chaining kills
  in a tight spot can refresh a dodge right when you need it, tying aggression to defense. The refund
  clamps at zero (never negative). Headless-tested: a kill at 2.0s cooldown drops it to 1.7s, and a kill
  with the dodge nearly ready leaves it at 0 rather than going negative.
- **ZOMBOID — melee shove now bats spitter acid globs out of the air.** A well-timed **melee (F)** now
  swats down any incoming spitter acid glob within reach — a defensive read that destroys the glob
  **clean, leaving no caustic puddle** (unlike letting it splat on the ground). So the shove isn't just
  a create-space/execute button: it's also an active *deflect* against ranged acid, rewarding good timing
  with total denial. A glob out of reach sails on untouched. Headless-tested: a glob in melee range is
  destroyed with no puddle left behind, while one well out of range is unaffected.
- **ZOMBOID — the Screamer now telegraphs its shriek (readable counterplay).** The Screamer no longer
  frenzies the horde the instant its cooldown is up — it now **winds up with a tell first** (a brief
  flash before the shriek lands), giving you a window to burst the fragile back-liner down or **chill it**
  to cut the shriek off. Killing it during the wind-up cancels the shriek entirely, and a Cryo Nova /
  Frost Field applied mid-tell makes it fizzle. Turns "silence it first" from a race you couldn't see
  into a readable reaction. Headless-tested: a neighbour isn't frenzied on the tell tick but is once the
  shriek lands, and chilling the screamer during the wind-up prevents the frenzy entirely.
- **ZOMBOID — active reload (skill-timed instant reload + damage surge).** Tapping **R** again during the
  tail end of a reload now triggers an **active reload**: the reload snaps shut instantly *and* grants a
  brief **+30% damage surge**. Miss the window (tap too early) and nothing happens — no penalty, the
  normal reload just carries on — so it's pure upside for skilled timing, adding depth to the moment-to-
  moment gunplay. The surge glows gold on the survivor. Headless-tested: a tap inside the window finishes
  the reload and arms the surge (which lifts shot damage ~30%), while a too-early tap leaves the reload
  running with no surge.
- **ZOMBOID — the auto-sentry now self-destructs in a blast when it powers down.** When a deployed sentry
  runs out of bolts or its lifetime expires, it no longer just winks out — it **detonates in a final
  blast** (50 damage + a brief stagger to every zombie within ~6 units), rewarding planting it deep in
  the horde. The blast is friendly to the survivor (zombie-only, unlike an explosive barrel). Headless-
  tested: an expiring sentry damages a zombie beside it while one well clear is untouched.
- **ZOMBOID — proximity mines now cook off explosive barrels.** A mine's blast detonates any explosive
  barrel in range, so rigging a mine beside a barrel sets up a huge combined blast (and the barrel's own
  blast chains on to more barrels). Mines already daisy-chain through exploder packs — their 120-damage
  blast kills exploders, triggering each one's detonation — so a well-placed mine can set off a whole
  environmental chain reaction. Headless-tested: a mine detonation pops a barrel within its radius while
  a barrel well clear is left standing.
- **ZOMBOID — exploders now chain-detonate each other.** An exploder's death blast used to spare other
  exploders; now it **chain-detonates** them, daisy-chaining a whole cluster into one string of blasts
  (the chain even hops through a middle exploder to reach one outside the first's radius). It's a real
  reward for luring exploders together and popping one — but double-edged, since the survivor eats every
  blast they're caught in. The chain is safely bounded (a detonated exploder is already dead and can't
  re-trigger). Headless-tested: popping one of three lined-up exploders kills all three via the chain,
  while an exploder well clear survives at full health and a plain zombie in the blast is caught but
  isn't a chain link.
- **ZOMBOID — Brutes now hurl you back when they hit.** A Brute's heavy blow no longer just chips your
  health — it **physically throws the survivor clear** (about 4 units), wrecking your position and your
  aim and potentially flinging you into the rest of the horde. It gives the Brute a distinct identity —
  a genuine spacing threat you have to respect, not just a slow damage sponge — and rewards dodging: a
  survivor mid-dodge-roll (i-frames up) rides the blow out untouched. Headless-tested: a brute's hit
  shoves the survivor >3 units, a walker's bite moves them 0, and an i-framed survivor isn't budged.
- **ZOMBOID — combo streaks now charge the Overcharge ultimate faster.** Kills landed on a hot combo
  streak bank **more ultimate meter** each (1× at ×1–2, 2× at ×3–4, 3× at ×5) instead of a flat one per
  kill. Keeping a chain alive now earns the screen-clearing panic button far more often, so the combo
  system feeds the ultimate — aggression and clean play compound. Headless-tested: a single kill banks
  1 charge at ×1, 2 at ×3, and 3 at ×5.
- **ZOMBOID — the shotgun now bodily knocks zombies back point-blank.** A shotgun pellet at close range
  lands a **heavy shove** (not just heavy damage), and that shove **fades with travel** on the same ramp
  as its damage falloff — so a blast to the face flings a zombie back and buys you space, while pellets
  fired across the arena barely nudge it. Gives the shotgun a real crowd-control identity beyond raw
  point-blank damage. Plain rounds keep their light nudge. Headless-tested: a fresh pellet's knockback is
  3.5 vs a plain round's 0.6, and end-to-end a point-blank pellet shoves a zombie back over 3× as far as
  a pistol round.
- **ZOMBOID — new between-wave upgrade: move speed.** The permanent upgrade cycle grows from six picks
  to seven, adding a **+move-speed** boost (+8% walk speed per pick). Mobility is king in a twin-stick
  survival game, so a fleeter survivor kites the horde, reaches loot, and slips out of hazards more
  easily as the run deepens — it slots in alongside +damage, +fire-rate, +max-health, +ammo,
  +crit-chance and +crit-damage. Headless-tested: one full seven-upgrade cycle raises the walk-speed
  multiplier once (1.0 → 1.08) while the older picks still apply, and a second cycle stacks it to 1.16.
- **ZOMBOID — exploded barrels leave a lingering fire patch.** Popping an explosive barrel now spills
  burning fuel: the blast leaves a **fire patch** where the barrel stood, so a detonated barrel keeps
  denying that ground (and cooking anything that walks through) for a few seconds after the bang — and,
  being fire, it **flashes over any acid puddle it overlaps** (pairs with the new fire-ignites-acid).
  Turns a one-shot trap into brief lasting area control. Headless-tested: popping a barrel leaves an
  active fire patch, while no fire burns at rest or from merely placing a barrel. (Refactored the
  molotov's fire-lighting into a shared `light_fire` helper.)
- **ZOMBOID — fire ignites acid pools (caustic flash-over).** Caustic ground is now flammable: when a
  molotov's fire patch — or the flamethrower's cone — touches a spitter's acid puddle (or a Volatile-horde
  pool), it **flashes over in one violent combustion**, dealing a burst of damage and setting alight every
  zombie caught in it, and the puddle is spent in the blast. It turns a hazard you normally have to route
  *around* into an offensive tool: torch the acid a spitter leaves under a pack to convert it into a
  fireball. Headless-tested: a fire patch overlapping a puddle consumes it and wounds a zombie standing in
  it, while a fire placed far away leaves the puddle and the zombie untouched.
- **ZOMBOID — new Regenerator Horde wave mutator.** A sixth wave modifier joins the roll (from wave 3):
  under **Regenerator Horde** every body slowly **knits its wounds back shut** (6% of its max health
  per second), so chip damage bleeds away and you must commit real burst to a kill rather than poking
  at the pack. Two hard counters keep it fair: a **burning or bleeding** body loses health faster than
  it heals, and a **chilled** body's regen is silenced — a Cryo Nova or Frost Field freezes the healing
  off. Bosses are exempt (they already have their enrage self-heal). Headless-tested: a wounded walker
  recovers health over a second under the mutator, stays flat with the mutator off, and stays flat
  while frozen.
- **ZOMBOID — new Berserk power-up (a combined offensive surge).** A ninth power-up joins the drop
  pool: **Berserk** lifts **both** your fire-rate *and* your damage at once (each ×1.7), where Rapid
  Fire boosts only rate and Double Damage only damage — so it's the "go loud" button, the biggest
  all-round burst of offense in a single pickup. It drops off the field like the others and is
  guaranteed to be a candidate in a boss's care-package roll. Headless-tested: granting Berserk sets
  both buff multipliers to 1.7 (both above 1), tags the buff, starts the timer, and the survivor's
  derived fire rate genuinely outpaces its un-buffed baseline.
- **ZOMBOID — medkits are never wasted (overflow banks as armor).** Picking up a medkit heals as
  before, but any healing past full health is now **converted to bonus body armor** (up to the plate
  cap) instead of being thrown away — so grabbing a kit while already topped up still pays off. Applies
  to both walking over a kit and the wave-clear vacuum sweep. Headless-tested: a full-health survivor
  who grabs a 40-heal kit at 5-below-max ends at full health with +35 armor.
- **ZOMBOID — the Overcharge ultimate now cryo-locks survivors.** The screen-wide ultimate blast hits
  every zombie for 500 and grants brief invulnerability; now anything **too tough to be one-shot** (a
  boss, a Bulwark shield, a beefy elite) is **left deep-frozen** by it — and since a chilled caster is
  silenced, the ultimate also shuts their abilities down while you regroup. Extends the panic button
  from "clear the trash" to "clear the trash *and* neutralise what's left." Headless-tested: a 2000-hp
  body survives the blast but is left with an active chill timer.
- **ZOMBOID — grenades now concuss (stun) the pack.** A grenade blast already chilled and damaged
  everything in range; now anything that **survives** it is also briefly **staggered — rooted in
  place** — giving the frag a genuine crowd-control identity. Because a staggered body takes the
  weak-point bonus, a grenade lobbed into a tanky pack sets up your follow-up fire to hit 40% harder.
  Headless-tested: a 500-hp zombie tanks the blast but is left with an active stagger timer.
- **ZOMBOID — the Warper now telegraphs its blink.** The teleporting Warper used to phase in with zero
  warning; now it **shimmers violet and roots itself for a brief wind-up** before the blink actually
  fires (matching the boss slam and leaper pounce tells), so the teleport is a readable, reactable
  threat instead of an unfair pop-in — shoot it during the tell or reposition. Headless-tested: a
  ready Warper spends a frame charging (rooted, no teleport) and only phases in after the wind-up.
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

### Fixed
- **Crash on exit when audio was active (segfault at teardown).** `platform::Window::shutdown()` called
  the global `SDL_Quit()`, which tears down *every* SDL subsystem — including `SDL_INIT_AUDIO`, which
  `audio::Audio` initializes and owns. Because `Audio` is usually destroyed *after* an explicit
  `window.shutdown()`, `SDL_Quit()` freed the audio device out from under the still-live `Audio`, whose
  destructor then dereferenced freed SDL state and crashed (observed as a hard `SIGSEGV` in
  `SDL_DestroyAudioQueue` under the headless "dummy" audio driver — every ZOMBOID CI smoke run — and a
  latent race on real audio backends). `Window::shutdown()` now quits *only* the subsystems it started
  (`SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)`), leaving audio for `Audio` to tear down —
  respecting SDL's per-subsystem refcounting so the two lifetimes are independent. `Audio::shutdown()`
  additionally pauses the device and syncs with the audio callback before destroying the stream.
  Verified: `zomboid_headless_smoke` now passes; the game exits 0 across five `dummy`-driver runs and a
  clean AddressSanitizer run; the `orbs`/`swarm`/`world` apps are unaffected.

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
