"""LEARN — write the night back into the repo.

Three side effects, each independently optional and none of them fatal:

  quarantine()    a candidate that has failed its last chance goes to stuck.md
  tick_roadmap()  a merged roadmap item stops being a candidate tomorrow
  memory_note()   the codebase-memory graph learns what happened

The ledger append itself lives in ledger.py and is NOT optional — it happens on
every run, including the ones where all three of these do nothing. Every
function below is therefore guarded to never raise: orchestrate.live_run calls
all three after the ledger line's entry is built but (for tick_roadmap) before
it is written, and a raise here must degrade to "this side effect didn't
happen" rather than cost the run its ledger line.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from .config import ForgeConfig
from .signals.roadmap import _clean

STUCK_FILENAME = "stuck.md"
ROADMAP_PATH = "docs/ROADMAP.md"
MEMORY_PATH = ".claude/codebase-memory.json"

STUCK_HEADER = (
    "# Stuck\n\n"
    "Tasks the Forge tried and could not finish. It will not try these again "
    "until a human deletes the line.\n\n"
)


def quarantine(candidate_key: str, task: str, source: str, root: Path,
               config: ForgeConfig) -> Path:
    """Record a struck-out candidate. Idempotent: one line per key, ever.

    Never raises. A full disk or an unwritable `forge/` (a stray file where
    the directory belongs, permissions) must degrade to "the candidate keeps
    getting retried tomorrow", not a crashed night — this is the same
    contract `tick_roadmap` and `memory_note` below make, for the same
    reason: orchestrate.live_run depends on it to still reach the ledger
    write on a night this fails.
    """
    path = root / "forge" / STUCK_FILENAME
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        existing = path.read_text(encoding="utf-8") if path.exists() else STUCK_HEADER
        if candidate_key in existing:
            return path
        stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d")
        path.write_text(f"{existing}- `{candidate_key}` ({stamp}) {task} — from `{source}`\n",
                        encoding="utf-8")
    except OSError:
        return path
    return path


def tick_roadmap(task: str, root: Path) -> bool:
    """Flip `- [ ] task` to `- [x] task` for an exact task match. Never raises.

    The roadmap parser (`signals.roadmap.parse`) strips markdown emphasis and
    code ticks out of each item's text with its own `_clean` before ever
    handing a task to DECIDE — so `task` here is already in that cleaned
    form, not the literal file text. Matching therefore re-derives the same
    cleaned form for each candidate line using that *same* function, rather
    than a second, independent stripper: `_clean`'s regex only collapses
    markdown that appears as a matched `**...**` (or backtick) pair on the
    single line the item regex sees, and can leave a lone, unmatched marker
    in place when its closing half sits on an indented continuation line the
    single-line item regex never reads — a real shape in docs/ROADMAP.md. A
    cruder stripper like `.replace("**", "")` erases that marker
    unconditionally, which can collapse two genuinely different tasks down
    to the same normalised text and tick the wrong one. Reusing `_clean`
    means there is exactly one definition of "cleaned" in this codebase, so
    the two can never drift apart.
    """
    path = root / ROADMAP_PATH
    try:
        text = path.read_text(encoding="utf-8")
    except OSError:
        return False

    out, hit = [], False
    for line in text.splitlines(keepends=True):
        stripped = line.rstrip("\n")
        if not hit and stripped.startswith("- [ ] ") and _clean(stripped[6:]) == task:
            out.append(line.replace("- [ ] ", "- [x] ", 1))
            hit = True
            continue
        out.append(line)
    if hit:
        try:
            path.write_text("".join(out), encoding="utf-8")
        except OSError:
            return False
    return hit


def memory_note(entry: dict, root: Path) -> bool:
    """Append one observation to the codebase-memory graph. Never raises."""
    path = root / MEMORY_PATH
    if not path.exists():
        return False
    pr = entry.get("pr")
    observation = (
        f"Forge run {entry.get('run_id')}: {entry.get('outcome')}"
        + (f" (PR #{pr})" if pr else "")
        + (f" — {entry.get('chose')}" if entry.get("chose") else "")
    )
    record = {
        "type": "entity",
        "name": "The Forge",
        "entityType": "component",
        "observations": [observation],
    }
    try:
        with path.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(record) + "\n")
    except OSError:
        return False
    return True
