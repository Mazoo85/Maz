# Production pipeline: idea → script → storyboard

The MadLibs Story Forge app generates story **ideas**. This document describes
how an idea becomes a **30-minute episode script** and then a **storyboard**.

The writing is done by **Claude** (interactively, in a Claude Code session) — no
API key, backend, or hosting is involved. The app is the hub that picks ideas,
hands off a brief, and tracks each project's stage.

## The flow

1. **Forge an idea** in the app and click **🎬 Script this idea**. This adds the
   idea to the **Productions** panel and copies a *production brief* to your
   clipboard.
2. **Paste the brief to Claude** and ask: *"Write the 30-minute script for this
   madlib."* Claude writes **3 distinct takes**.
3. **Read the options** (Claude publishes them as a page you can open) and **pick
   one**.
4. Claude turns the chosen script into a **storyboard** (a visual shot-list) and
   publishes it as a page.
5. Paste the **script** and **storyboard** links back into that project's row in
   the Productions panel so everything lives in one place, and advance its
   **stage** (Idea → Scripting → Script chosen → Storyboarded).

## Where the work is stored

```
madlibs/productions/<slug>/     # slug = story id + seed, e.g. museum-ghost-42
  brief.md            the idea + production spec (what you paste to Claude)
  script-option-a.md  } three distinct 30-minute teleplay takes
  script-option-b.md  }
  script-option-c.md  }
  chosen.md           a copy of the take you picked
  storyboard.md       the shot-list (source of the storyboard page)
```

Reader-friendly HTML pages (`*.artifact.html`) are generated from these Markdown
files for publishing and are **git-ignored** (they're previews, rebuilt from the
Markdown source above).

## Script format (30-minute teleplay)

- **Structure:** `COLD OPEN → ACT ONE → ACT TWO → ACT THREE (short) → TAG`,
  adapted to genre. Target ~25–30 pages of formatted screenplay.
- The idea's six beats map onto the acts:
  | Beat | Lands in |
  | --- | --- |
  | Logline | the premise |
  | Setup | Cold Open / top of Act One |
  | Inciting Incident | Act One turn |
  | Conflict | Act Two |
  | Climax | Act Three |
  | Resolution | Tag |
- **Standard elements:** scene headings (`INT./EXT. LOCATION — DAY/NIGHT`), action
  lines, centered `CHARACTER` cues, dialogue, `(parentheticals)`, and transitions.
- The character names, places, and key object from the idea stay **consistent**
  (the app's templates already keep these coherent across beats).
- The three takes should differ **meaningfully** — tone, POV, or structure — not
  just reword the same scenes.

## Storyboard format (visual shot-list)

Panels, in scene order, each with:

- **SHOT #** and the scene slugline it belongs to
- **Framing** — `WIDE`, `MEDIUM`, `CLOSE-UP`, `OTS`, `INSERT`, `POV`, etc.
- **Camera** — `STATIC`, `PAN`, `TILT`, `DOLLY`, `HANDHELD`, `CRANE`, etc.
- **Action** — what happens in the frame
- **Dialogue** — the line(s) spoken over the shot (if any)
- **Duration** — a rough seconds estimate (the totals sanity-check toward 30 min)
- A framed 16:9 box as a sketch placeholder (no drawn art in this environment)

## Notes

- Because Claude does the writing in-session, this pipeline runs when you're
  working with Claude. Everything it produces is saved to the repo and published
  as pages you can reopen any time.
- If you later want the app to write scripts on its own (no Claude session), that
  needs an Anthropic API key wired into the app or a small backend — see the
  "Out of scope" note in the project plan.
