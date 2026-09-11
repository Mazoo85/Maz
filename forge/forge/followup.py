"""The follow-up pass — the only part of the ledger a human writes.

Green checks prove nothing broke. Whether a PR was MERGED, and how much had to
be EDITED first, is the only evidence that the work was good — and it can only
be known days after the run that produced it. So the ledger is append-only for
outcomes but these two fields are backfilled in place, later, from GitHub.

This is the sole exception to "never rewrite the ledger", and it is why the
month file is rewritten whole rather than appended to.

Everything this module reads off the wire — a PR object, a commit list, a
commit's nested "commit"/"author"/"stats" blocks — comes from ``fetch``
(ordinarily ``github.api``, which itself never raises, but is deliberately
injectable). None of it is trusted to have the shape the happy path expects:
a field can be missing, ``None``, or the wrong type, and this module must
degrade an entry to "still unknown" rather than raise.
"""

from __future__ import annotations

import json
from pathlib import Path

from .config import ForgeConfig
from .github import api, repo_slug
from .ledger import read_all, write_atomic

# Commit authors that are the Forge itself, not a human editing its work.
FORGE_AUTHORS = ("the forge", "forge", "claude", "maz crew")


def pending(entries: list[dict]) -> list[dict]:
    """Entries that opened a PR whose fate is still unrecorded."""
    return [e for e in entries if e.get("pr") and e.get("merged") is None]


def _human_edit_lines(commits: list) -> int:
    """Total changed lines from commits not authored by the Forge itself.

    ``commits`` is whatever ``fetch`` handed back for a commits listing —
    real GitHub commit objects nest "commit" -> "author" -> "name" and
    "stats" -> "total", but each of those levels can be absent or the wrong
    type (a test double, a future API shape, a hand-built fixture), so every
    level is isinstance-checked before it is indexed rather than assumed.
    """
    total = 0
    for commit in commits or []:
        if not isinstance(commit, dict):
            continue
        commit_block = commit.get("commit")
        author = commit_block.get("author") if isinstance(commit_block, dict) else None
        name = author.get("name") if isinstance(author, dict) else None
        if isinstance(name, str) and name.strip().lower() in FORGE_AUTHORS:
            continue
        stats = commit.get("stats")
        lines = stats.get("total") if isinstance(stats, dict) else None
        total += lines if isinstance(lines, (int, float)) else 0
    return total


def resolve(entry: dict, slug: str, fetch) -> dict | None:
    """Fill in merged / human_edits for one entry, or None if still unknown."""
    number = entry.get("pr")
    if not number:
        return None
    pr = fetch(f"/repos/{slug}/pulls/{number}")
    if not isinstance(pr, dict) or "state" not in pr:
        return None  # unreachable (rate-limited, deleted, no token) — ask again later
    if pr.get("state") == "open":
        return None  # still in flight; ask again another night

    merged = bool(pr.get("merged"))
    edits = 0
    if merged:
        commits = fetch(f"/repos/{slug}/pulls/{number}/commits")
        items = commits.get("items") if isinstance(commits, dict) else None
        edits = _human_edit_lines(items if isinstance(items, list) else [])

    updated = dict(entry)
    updated["merged"] = merged
    updated["human_edits"] = edits
    return updated


def _key(record: dict) -> tuple:
    """The identity of a ledger entry: no field alone is a stable id (``at``
    is only second-precision and most entries share ``pr=None``), but GitHub
    never reuses a PR number within a repo, so a real ``pr`` plus the run
    that produced it is unambiguous. Entries without a PR never appear in
    ``updates`` (see ``pending``), so collisions among the many ``pr=None``
    entries never matter here.
    """
    return (record.get("run_id"), record.get("at"), record.get("pr"))


def backfill(root: Path, config: ForgeConfig, slug: str | None = None, fetch=None) -> int:
    """Resolve every pending entry and rewrite the month files. Returns the count.

    Each month file is read whole, patched in memory, and written back whole
    — the only rewrite this system ever performs on its own permanent
    record. A line that is valid JSON but not an object, or not JSON at all,
    is carried through byte-for-byte: this pass must never be the thing that
    turns a merely-corrupt line into a silently-deleted one.
    """
    slug = slug if slug is not None else repo_slug(root)
    if not slug:
        return 0
    getter = fetch or (lambda path: api(path))

    entries = read_all(root, config)
    updates: dict[tuple, dict] = {}
    for entry in pending(entries):
        try:
            resolved = resolve(entry, slug, getter)
        except Exception:  # noqa: BLE001 — one bad fetch must not sink the whole pass
            resolved = None
        if resolved is not None:
            updates[_key(entry)] = resolved
    if not updates:
        return 0

    ledger_dir = config.ledger_dir(root)
    applied = 0
    for path in sorted(ledger_dir.glob("*.jsonl")):
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            # Mirrors read_all's own guard: a file we cannot faithfully read
            # is a file we must not blindly rewrite. Its entries were never
            # in `entries` above either, so nothing in `updates` targets it.
            continue

        lines_out: list[str] = []
        touched = 0
        for line in text.splitlines():
            stripped = line.strip()
            if not stripped:
                continue
            try:
                record = json.loads(stripped)
            except (json.JSONDecodeError, ValueError):
                lines_out.append(line)  # not JSON at all — keep verbatim
                continue
            if not isinstance(record, dict):
                lines_out.append(line)  # valid JSON, but not an entry — keep verbatim
                continue
            key = _key(record)
            if key in updates:
                record = updates[key]
                touched += 1
            lines_out.append(json.dumps(record, sort_keys=True))

        if touched:
            # write_atomic, not Path.write_text: see its docstring in
            # ledger.py for why an in-place ledger rewrite must never
            # truncate the file before the new content is safely down.
            write_atomic(path, "\n".join(lines_out) + "\n")
            applied += touched
    return applied
