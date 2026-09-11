"""The follow-up pass — the only part of the ledger a human writes.

Green checks prove nothing broke. Whether a PR was MERGED, and how much had to
be EDITED first, is the only evidence that the work was good — and it can only
be known days after the run that produced it. So the ledger is append-only for
outcomes but these two fields are backfilled in place, later, from GitHub.

This is the sole exception to "never rewrite the ledger", and it is why the
month file is rewritten whole rather than appended to.

Everything this module reads off the wire — a PR object, a commit list, and
(one call per non-Forge commit) that commit's own detail, with its nested
"commit"/"author"/"message" and "stats"/"total" blocks — comes from
``fetch`` (ordinarily ``github.api``, which itself never raises, but is
deliberately injectable). None of it is trusted to have the shape the happy
path expects: a field can be missing, ``None``, or the wrong type, and this
module must degrade an entry to "still unknown" rather than raise. Note in
particular that the commit-*list* endpoint (``.../pulls/{n}/commits``)
never carries "stats" at all — only the single-commit endpoint
(``.../commits/{sha}``) does — which is why a per-commit fetch exists.
"""

from __future__ import annotations

import json
from pathlib import Path

from .config import ForgeConfig
from .github import api, repo_slug
from .ledger import read_all, write_atomic

# Commit authors that are the Forge itself, not a human editing its work.
# Kept only as a fallback: nothing in this codebase actually sets a commit
# author for the Forge's own commits (`crew do --commit` runs under
# whatever identity git is locally configured with, which on a
# contributor's machine is probably them). This list is therefore a
# coincidence-based secondary signal, not a control the Forge exercises —
# see FORGE_MESSAGE_PREFIX below for the one it does.
FORGE_AUTHORS = ("the forge", "forge", "claude", "maz crew")

# Every commit the Forge makes goes through `do._default_crew`, which always
# calls `crew do ... --commit -m "forge: {task[:60]}"`. This prefix is the
# one signal this codebase actually controls end to end, so it is the
# decisive check. Do not "simplify" this down to the author-name check
# above: that list matches on values nobody in this pipeline sets, so it
# would silently misclassify the Forge's own commits as human edits (the
# bug this module exists to fix) while also risking the opposite mistake —
# flagging a real human merely because they happen to share a name with the
# list. The author check is retained only as a fallback for the case the
# message itself can't be read at all (missing, not a string).
FORGE_MESSAGE_PREFIX = "forge: "


def pending(entries: list[dict]) -> list[dict]:
    """Entries that opened a PR whose fate is still unrecorded."""
    return [e for e in entries if e.get("pr") and e.get("merged") is None]


def _is_forge_commit(commit_block: object) -> bool:
    """Is this commit the Forge's own work, not a human editing it?

    The message prefix is checked first and, when the message is present
    and readable, is decisive either way — see the FORGE_MESSAGE_PREFIX
    docstring for why it is trusted over the author name even when they
    disagree (a real "forge: ..." commit under an unexpected author name is
    still the Forge's; an ordinary commit that merely has an author named
    "claude" is still a human's). The author-name fallback only applies when
    there is no readable message to decide from at all — a defensive path
    for malformed input, not a second vote.
    """
    block = commit_block if isinstance(commit_block, dict) else None
    message = block.get("message") if block else None
    if isinstance(message, str):
        return message.startswith(FORGE_MESSAGE_PREFIX)
    author = block.get("author") if block else None
    name = author.get("name") if isinstance(author, dict) else None
    return isinstance(name, str) and name.strip().lower() in FORGE_AUTHORS


def _human_edit_lines(commits: list, slug: str, fetch) -> int | None:
    """Total changed lines from commits not authored by the Forge itself.

    ``commits`` is whatever ``fetch`` handed back for a commits *listing*
    (``GET /repos/{slug}/pulls/{n}/commits``) — but that endpoint never
    returns a "stats" block on its list items; "stats" only exists on the
    single-commit endpoint, ``GET /repos/{slug}/commits/{sha}``. So for
    every commit that isn't the Forge's own, this fetches that commit
    individually and reads ``stats.total`` from *that* response. Forge PRs
    typically carry zero or one non-Forge commit, so this is a small,
    bounded number of extra calls, not a fan-out — and when every commit is
    the Forge's own, no per-commit fetch happens at all.

    Every level of both shapes — the list item's "commit"/"author"/
    "message", and the per-commit response's "stats"/"total" — can be
    absent or the wrong type (a test double, a future API shape, a
    transient fetch failure returning ``github.api``'s own ``{}``), so each
    is isinstance-checked before use. Critically, an unreadable stat is
    never counted as zero: a wrong small number is worse than an honest
    unknown for a signal a later system will train on, so the first
    non-Forge commit whose stats can't be read makes the whole result
    ``None`` rather than under-counting.
    """
    shas: list[object] = []
    for commit in commits or []:
        if not isinstance(commit, dict):
            continue
        if _is_forge_commit(commit.get("commit")):
            continue
        shas.append(commit.get("sha"))

    if not shas:
        return 0

    total = 0
    for sha in shas:
        if not isinstance(sha, str) or not sha:
            return None  # no sha to fetch by — this commit's stats are unknowable
        detail = fetch(f"/repos/{slug}/commits/{sha}")
        stats = detail.get("stats") if isinstance(detail, dict) else None
        lines = stats.get("total") if isinstance(stats, dict) else None
        if isinstance(lines, bool) or not isinstance(lines, (int, float)):
            return None
        total += lines
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
    edits: int | None = 0
    if merged:
        commits = fetch(f"/repos/{slug}/pulls/{number}/commits")
        items = commits.get("items") if isinstance(commits, dict) else None
        edits = _human_edit_lines(items if isinstance(items, list) else [], slug, fetch)

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
