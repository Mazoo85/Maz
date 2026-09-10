"""The permanent record — the single most important artifact the Forge produces.

One JSON line per run, appended to a month file, committed to git, never
deleted or rewritten. Everything else here is replaceable; six months of honest
records about what was tried and how it turned out is not.

Two fields are filled in later, by the follow-up pass, once a human has looked
at the PR: `merged` and `human_edits`. Green checks only prove nothing broke.
Merged with zero edits is the only evidence that the work was actually good,
and it is the signal a future scoring model (P3) will learn from.

Unlike ``sense.py``, which never raises because a broken collector should cost
the night a few candidates rather than the whole run, ``append`` here is not
guarded: a failure to persist the one artifact this system exists to produce
(a bad path, a full disk, an entry someone built with a non-JSON-serialisable
field) must surface to the caller, not vanish. Nothing is written until
``json.dumps`` succeeds, so a raise here never leaves a half-written line
behind for ``read_all`` to trip over.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from .config import ForgeConfig

# The closed set of ways a run can end.
OUTCOMES = (
    "pr_opened",
    "verify_failed",
    "crew_failed",
    "budget_exceeded",
    "no_task",
    "dry_run",
)

# Outcomes that count as a failed attempt at a specific candidate.
FAILURE_OUTCOMES = ("verify_failed", "crew_failed", "budget_exceeded")

# Outcomes where the Forge actually worked in a zone.
ACTING_OUTCOMES = ("pr_opened", "verify_failed")


def new_entry(run_id: str, **fields) -> dict:
    """An entry with every field present, so no reader has to guess."""
    entry = {
        "run_id": run_id,
        "at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "chose": "",
        "source": "",
        "kind": "",
        "candidate_key": "",
        "why": {},
        "zone": "",
        "outcome": "no_task",
        "pr": None,
        "checks": "",
        "files_touched": 0,
        "cost_usd": 0.0,
        "duration_min": 0.0,
        "notes": "",
        # Filled in later by the follow-up pass.
        "merged": None,
        "human_edits": None,
    }
    entry.update(fields)
    return entry


def _month_path(root: Path, config: ForgeConfig, when: datetime | None = None) -> Path:
    when = when or datetime.now(timezone.utc)
    return config.ledger_dir(root) / f"{when:%Y-%m}.jsonl"


def append(entry: dict, root: Path, config: ForgeConfig) -> Path:
    """Append one entry. Creates the month file and directory as needed."""
    path = _month_path(root, config)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(entry, sort_keys=True) + "\n")
    return path


def read_all(root: Path, config: ForgeConfig) -> list[dict]:
    """Every entry across every month file, oldest first. Corrupt lines skipped."""
    d = config.ledger_dir(root)
    if not d.exists():
        return []
    out: list[dict] = []
    for path in sorted(d.glob("*.jsonl")):
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue
        for line in text.splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except (json.JSONDecodeError, ValueError):
                continue
            if isinstance(record, dict):
                out.append(record)
    return out


def strikes(root: Path, config: ForgeConfig) -> dict[str, int]:
    """Consecutive recent failures per candidate key. A success resets to zero."""
    counts: dict[str, int] = {}
    for entry in read_all(root, config):
        key = entry.get("candidate_key") or ""
        if not key:
            continue
        outcome = entry.get("outcome")
        if outcome in FAILURE_OUTCOMES:
            counts[key] = counts.get(key, 0) + 1
        elif outcome == "pr_opened":
            counts[key] = 0
    return counts


def recent_zones(root: Path, config: ForgeConfig, n: int = 3) -> list[str]:
    """Zones from the last ``n`` runs that actually did work, newest first."""
    zones = [
        entry.get("zone") or ""
        for entry in read_all(root, config)
        if entry.get("outcome") in ACTING_OUTCOMES and entry.get("zone")
    ]
    return list(reversed(zones))[:n]
