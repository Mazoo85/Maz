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
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path

from .config import ForgeConfig

# The closed set of ways a run can end.
#
# "push_failed" sits between a green check run and "pr_opened": the branch
# passed VERIFY but could not be pushed to the remote (auth, a rejected
# non-fast-forward, the network) before `open_draft_pr` was ever reached.
# Without it, that night had nowhere honest to land: "verify_failed" would
# lie about *why* no PR exists (the checks were green), and "pr_opened" with
# a null `pr` is already the documented shape for "the PR call itself
# failed", not "there was no branch on the remote to open one against".
OUTCOMES = (
    "pr_opened",
    "push_failed",
    "verify_failed",
    "crew_failed",
    "budget_exceeded",
    "no_task",
    "dry_run",
)

# Outcomes that count as a failed attempt at a specific candidate.
FAILURE_OUTCOMES = ("verify_failed", "crew_failed", "budget_exceeded", "push_failed")

# Outcomes where the Forge actually worked in a zone.
ACTING_OUTCOMES = ("pr_opened", "verify_failed", "push_failed")


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
    """The month file. ``when`` is only a fallback for a missing/unparseable
    ``at`` — the entry's own ``at`` is what decides the file, so a run
    stamped near midnight is filed by when it happened, not by when the
    process got around to writing it. Do not go back to deriving this from
    ``datetime.now`` at call time: that was the wall-clock bug this
    docstring exists to prevent from being reintroduced.
    """
    when = when or datetime.now(timezone.utc)
    return config.ledger_dir(root) / f"{when:%Y-%m}.jsonl"


def _entry_month(entry: dict) -> datetime:
    """The month an entry belongs in, taken from its own ``at`` field,
    converted to UTC.

    Honours any ISO-8601 ``at``: a trailing ``Z`` (normalised to
    ``+00:00`` first — Python 3.10's ``fromisoformat`` rejects a bare
    ``Z``; only 3.11+ accepts it, and this project must behave
    identically on both), a numeric UTC offset, fractional seconds, or a
    naive timestamp with no zone at all, which is treated as already-UTC
    since that is what ``new_entry`` emits. An offset ``at`` is converted
    to UTC *before* the month is taken — a run at
    ``2026-10-01T00:30:00+05:00`` is ``2026-09-30T19:30Z`` and belongs in
    September, not October — because the month a run belongs in is a UTC
    question, not a "does the local calendar page happen to match"
    question.

    Falls back to ``datetime.now(timezone.utc)`` only when ``at`` is
    missing, not a string, or a string ``fromisoformat`` cannot parse at
    all — never when it merely lacks a zone or has fractional seconds.
    ``append`` must never fail because of a malformed timestamp, since a
    raise here means a run goes unrecorded — the one thing this module
    exists to prevent. Do not narrow this back to a single literal
    ``strptime`` format: a fallback that fires on any ISO-8601 shape
    ``strptime`` doesn't happen to match is indistinguishable from the
    wall-clock bug this function exists to remove — it just fails
    silently instead of loudly, for whichever inputs a future producer
    happens to use.
    """
    at = entry.get("at")
    if isinstance(at, str):
        iso = at[:-1] + "+00:00" if at.endswith("Z") else at
        try:
            parsed = datetime.fromisoformat(iso)
        except (ValueError, TypeError):
            pass
        else:
            if parsed.tzinfo is None:
                parsed = parsed.replace(tzinfo=timezone.utc)
            return parsed.astimezone(timezone.utc)
    return datetime.now(timezone.utc)


def append(entry: dict, root: Path, config: ForgeConfig) -> Path:
    """Append one entry. Creates the month file and directory as needed."""
    path = _month_path(root, config, when=_entry_month(entry))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(entry, sort_keys=True) + "\n")
    return path


def _write_and_sync(fh, content: str) -> None:
    """The write-then-flush-then-fsync step, pulled out on its own so tests
    can make it fail partway through without needing a real full disk or a
    real power cut."""
    fh.write(content)
    fh.flush()
    os.fsync(fh.fileno())


def write_atomic(path: Path, content: str) -> None:
    """Replace ``path`` with ``content`` without ever risking a truncated file.

    This is the only supported way to rewrite a ledger month file in place
    (see ``followup.backfill``, the sole caller). ``Path.write_text`` is not
    safe for that: it truncates the file at open time, before a single byte
    of new content is written, so a crash partway through — disk full,
    process killed, power loss — leaves a truncated fragment where the
    ledger used to be. The ledger is the one artifact in this system that
    cannot be regenerated, so that failure mode is not acceptable here even
    though it would be fine for a disposable file.

    Instead: write the full new content to a temp file, fsync it so it is
    actually on disk, then ``os.replace`` it over the target. ``os.replace``
    is atomic on POSIX *within a single filesystem* — which is exactly why
    the temp file is created with ``dir=path.parent``, making it a sibling
    of the target. It must stay a sibling. Moving it to ``/tmp`` (a tempting
    "tidy it up" edit) would put it on a different filesystem in the general
    case, turning the final step into a copy-then-delete that is no longer
    atomic and reintroduces the exact truncation risk this function exists
    to remove.

    On any failure before the replace, ``path`` is left byte-for-byte as it
    was and the temp file is deleted in the ``finally`` — nothing is ever
    left as a mixture of old and new content, and no stray temp file
    accumulates in the ledger directory.
    """
    fd, tmp_name = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.", suffix=".tmp")
    tmp_path = Path(tmp_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as fh:
            _write_and_sync(fh, content)
        os.replace(tmp_path, path)
    finally:
        tmp_path.unlink(missing_ok=True)


def read_all(root: Path, config: ForgeConfig) -> list[dict]:
    """Every entry across every month file, oldest first. Corrupt lines skipped.

    ``UnicodeDecodeError`` is caught alongside ``OSError``: it is a
    ``ValueError`` subclass, not an ``OSError``, so one bad byte in any
    historical month file would otherwise propagate uncaught out of
    ``read_all`` — and therefore out of ``strikes()`` and ``recent_zones()``,
    for the entire ledger rather than just the offending file.
    """
    d = config.ledger_dir(root)
    if not d.exists():
        return []
    out: list[dict] = []
    for path in sorted(d.glob("*.jsonl")):
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
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
    """Consecutive recent failures per candidate key. A success resets to zero.

    ``dry_run`` and ``no_task`` (anything outside ``FAILURE_OUTCOMES`` and
    not ``pr_opened``) are inert: they neither increment nor reset a key's
    count. That is deliberate — week one of this system is all dry runs,
    and they must not accrue false strikes — not an oversight.
    """
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
