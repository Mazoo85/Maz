# Better stories — more places, real structures, and MADLIBS premises

**Date:** 2026-09-11
**Status:** approved, ready to plan
**Sub-project:** 3 of 3 in the combined movie maker

## The goal

SCRIPT FORGE films now look good. They still feel like each other. A viewer
watching two films back to back sees two locations, the same beat order, and the
same sentence with different nouns.

This fixes the three measured causes of that.

## Where this sits

1. ~~**Score to picture**~~ — done (PR #22). SONG FORGE scores every film.
2. ~~**Better picture**~~ — done (PR #24). Characters act, sets have depth, the
   camera means something.
3. **Better stories** — this spec.
4. **Maz Engine renders the reel** — the later track. The reel stays plain data
   with no browser in it precisely so the engine can consume it.

Explicitly **out of scope**: renderer changes of any kind, the score, editing
controls, and any Maz Engine work.

## The correction that shaped this spec

The earlier plan said *"MADLIBS becomes the story brain so films stop sharing one
seven-beat spine."* That is wrong in an important way, and measuring it first
changed the design.

MADLIBS has **45 story templates across 34 genres** — but **all 45 share one
six-beat spine**: Logline → Setup → Inciting Incident → Conflict → Climax →
Resolution. Adopting MADLIBS wholesale would give 45 different *premises* on one
*shape*. That is a real gain, but it is not the gain that was assumed, and on its
own it would leave the sameness largely intact.

So MADLIBS solves one of the three causes below, and the other two need their own
work.

## The three causes, measured

### 1. Every film has exactly two locations

Not "usually" — **200 of 200** generated films offered exactly two places and used
exactly two. It is locked twice over:

- `film/js/parse.js` fills the place list to exactly two:
  ```js
  while (placeKeys.length < 2 && guardPlaces++ < 40) { … }
  var places = placeKeys.slice(0, 3)…   // allows 3; the loop never makes a 3rd
  ```
- `film/js/screenplay.js` then indexes a **hardcoded two-place table**:
  ```js
  var BEAT_PLACE = { open: 0, spark: 0, push: 1, turn: 1, crisis: 1, choice: 0, after: 0 };
  ```
  Even given five places, this only ever names 0 and 1.

A seven-scene film cuts between two rooms.

### 2. The default film has no crisis at all

| Length | Beats |
|---|---|
| micro (3) | open · spark · choice |
| **short (5, the default)** | open · spark · push · turn · choice |
| festival (7) | open · spark · push · turn · crisis · choice · after |

`short` has **no crisis beat**. The crisis is what triggers the handheld camera,
the Dutch tilt, the recoil / slump / head-in-hands poses, and the score's
full-band section. **None of that appears in a default film.** Sub-project 2's
most dramatic work is invisible unless the viewer happens to pick festival.

### 3. Every premise is one sentence

"Someone finds a thing and has one night to do something." Different nouns,
identical shape, every time.

## Design

### Part 1 — more places, and a reason to be in each

**`parse.js` produces three to five places** instead of two: those named in the
typed idea first, then the hero's role-default place, then connectors that
plausibly adjoin anywhere (a car, a street, a hallway). The existing CONNECTORS
list already exists for this and simply stops too early.

**`BEAT_PLACE` becomes a function**, `placeForBeat(beatId, placeCount, seed)`,
keeping the one rule worth keeping and adding the ones that create variety:

- **open and after share a place.** A film that ends where it began feels like it
  ended; this is the existing comment's insight and it stays.
- **the crisis is somewhere else.** Wherever possible it gets a location the film
  has not used yet — being somewhere unfamiliar is part of what a crisis is.
- **the push moves.** It is the beat about momentum.
- everything else distributes across the remaining places, seeded.

### Part 2 — structures, plural

One spine per length becomes **a set of spines per length**, chosen by seed and
genre, so two films of the same length can be shaped differently.

> **Deviation (whole-branch review, 2026-09-11):** the shipped `spineFor(lengthKey,
> seed)` picks by seed only, not genre. Task 2 implemented it that way and every
> later task built on it without objection; the whole-branch review is what caught
> the gap against this line. Making it genre-aware would mean inventing an
> association between (say) horror and a particular beat order with no reason
> behind the pairing beyond "it had to be something" — exactly what this spec's
> own "illustrative rather than final" framing was trying to avoid inventing
> without cause. Recording the deviation here rather than guessing at one.

**Every spine at `short` and `festival` must contain a crisis**, which fixes cause
2 outright. `micro` at three beats is too short to always carry one, but at least
one of its spines must.

Illustrative rather than final — the plan fixes the exact sets:

| Length | Spines |
|---|---|
| micro | `open · spark · choice` · `open · crisis · after` |
| short | `open · spark · crisis · choice · after` · `open · push · turn · crisis · choice` · `open · spark · turn · crisis · after` |
| festival | the current seven · a version that opens on the crisis and goes back · one that ends on the turn |

Beat ids stay exactly as they are (`open spark push turn crisis choice after`) so
the reel, the renderer and the score need no changes whatsoever. Only the
*orders* become plural.

### Part 3 — MADLIBS premises

A new module adapts a MADLIBS story into a SCRIPT FORGE premise. MADLIBS is
loaded as a library from the film app — the two projects live in one repository
and this is exactly the combination the whole movie maker was for.

- **When it fires:** when the typed idea is empty, when it is too thin to yield a
  premise, and from an explicit **"Surprise me"** control on the film page.
- **What it gives:** 45 premises across 34 genres, with MADLIBS's tagged word
  reuse keeping a character, a place and a key object consistent across beats.
- **Genre mapping:** every MADLIBS genre must resolve to one SCRIPT FORGE genre.
  This is a bigger job than it first looked: an early reading of this spec said
  MADLIBS had nine genres, which came from sampling the first nineteen templates
  rather than all forty-five. It has **34**, against SCRIPT FORGE's ten, so most
  of them collapse — `noir` onto mystery, `cyberpunk` and `time-travel` onto
  scifi, `spy` and `survival` onto thriller, `coming-of-age` and `war` onto
  drama. `adventure` has no equivalent at all and needs a deliberate choice.
- **Beat mapping:** MADLIBS's six labels map onto SCRIPT FORGE beat ids.

MADLIBS itself is **not modified**. It keeps working standalone and keeps its own
tests.

## Testing

**Logic (Node, no browser):**

- a festival film uses **at least three distinct places**; open and after share
  one; the crisis differs from the open
- every length offers **more than one spine**, and spine choice is deterministic
  for a seed and varies across seeds
- **every `short` and `festival` spine contains a crisis** — the test that would
  have caught cause 2
- every MADLIBS genre maps to a genre that exists in SCRIPT FORGE's own table,
  asserted against that table rather than a copy
- every MADLIBS beat label maps to a real SCRIPT FORGE beat id
- all 45 templates convert to a valid premise with every field the writer needs
- the same idea and seed still produce exactly the same film

**Browser:**

- the film page still writes, plays, records and downloads (the existing suite
  must keep passing untouched)
- "Surprise me" produces a playable film
- the link checker and site smoke test stay green with the film app loading
  MADLIBS's modules

**Proof the sameness is actually gone**, measured across many films rather than
asserted: distinct-place counts, spine variety, and crisis presence at the
default length, before and after.

## Risks

- **Cross-app dependency.** The film app gaining a dependency on MADLIBS's modules
  is new coupling in a repo where each app has stood alone. Mitigation: the
  dependency runs one way only, MADLIBS is untouched, and the link and smoke
  checks already verify every app still boots.
- **More places could read as incoherent** — a lighthouse story cutting to a
  hospital. Mitigation: the CONNECTORS idea already guards this and stays; new
  places must plausibly adjoin.
- **Changing spines changes every existing film.** The same typed idea will now
  produce a different film than it did yesterday. That is the point, but it means
  the determinism tests pin *stability going forward*, not agreement with the
  past.
- **Scope creep into the renderer.** None of this touches drawing. If a change
  seems to need renderer work, that is a signal the design is wrong.

## Build order

1. More places in `parse.js`, and `placeForBeat` replacing the hardcoded table.
2. Plural spines per length, with the crisis guaranteed at short and festival.
3. The MADLIBS adapter, with the genre and beat mappings tested against the real
   tables.
4. The "Surprise me" control, and the thin-idea fallback.
5. Measure the sameness before and after; update the film README and the hub
   blurb.
