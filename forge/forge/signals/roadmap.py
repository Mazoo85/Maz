"""The appetite: unchecked boxes in docs/ROADMAP.md.

The roadmap is already a machine-readable backlog — 14 phases of `- [ ]` lines
with no nesting. Items marked `[~]` (in progress) count as candidates too, and
the phase containing them is taken to be the milestone currently in flight.

A roadmap item carries `paths` only when it names real files in backticks:

    - [ ] Add a volume slider to `music/player.js`

That is the one deliberate way a human points the Forge at work. An item that
names nothing stays pathless, and DECIDE refuses to run it — the roadmap
describes work, not locations, unless you say otherwise in the line itself.
Naming a file is not permission to touch it: the safe zones still decide that.
"""

from __future__ import annotations

import re
from pathlib import Path

from ..models import Candidate

ROADMAP_PATH = "docs/ROADMAP.md"

_PHASE_RE = re.compile(r"^##\s+Phase\s+(\d+)\b")
_ITEM_RE = re.compile(r"^-\s+\[([ x~])\]\s+(.+?)\s*$")
_FENCE_RE = re.compile(r"^\s*(```|~~~)")


# Spans between backticks, read off the raw item text before `_clean` removes
# the ticks. Non-greedy and single-line: a stray tick cannot swallow the rest
# of the document.
_TICKED_RE = re.compile(r"`([^`\n]+)`")

# Longer than any real path in this repo by a wide margin; a guard against a
# pathological line, not a meaningful limit.
MAX_PATH_CHARS = 200


def looks_like_path(token: str) -> bool:
    """True if `token` is shaped like a repo-relative path we could check.

    Shape only — this says nothing about whether the file exists. It rejects
    outright the forms that must never reach a filesystem lookup: `..`
    segments, absolute paths, backslashes, and anything with whitespace in it.
    Doing that here rather than relying on `zones.is_no_touch` downstream is
    deliberate belt-and-braces: extraction is where a hostile-looking string
    should die, so that no later step is ever handed one to reason about.
    """
    if not token or len(token) > MAX_PATH_CHARS:
        return False
    if token != token.strip() or any(ch.isspace() for ch in token):
        return False
    if token.startswith("/") or "\\" in token:
        return False
    segments = token.split("/")
    if any(seg in ("", ".", "..") for seg in segments):
        return False
    return True


def paths_in(item_text: str, exists) -> tuple[str, ...]:
    """Backticked real files named by one roadmap line, in the order written.

    `exists` answers "is this a real file in the repo?". Everything else —
    prose in backticks like `FetchContent`, a path that was renamed away, a
    directory — comes back empty, so the item stays pathless and DECIDE skips
    it. Requiring the file to exist is what keeps this from turning every
    backticked word into a claim about the tree.
    """
    out: list[str] = []
    for token in _TICKED_RE.findall(item_text):
        if token in out or not looks_like_path(token):
            continue
        try:
            if exists(token):
                out.append(token)
        except OSError:
            continue
    return tuple(out)


def _clean(text: str) -> str:
    """Strip markdown emphasis and code ticks so the task reads as plain English."""
    text = re.sub(r"\*\*(.+?)\*\*", r"\1", text)
    text = re.sub(r"`(.+?)`", r"\1", text)
    return text.strip()


def parse(text: str, exists=None) -> list[Candidate]:
    """Turn roadmap markdown into candidates. Never raises.

    `exists` answers "is this a real file in the repo?" and is what lets an
    item name the files it touches. Left out — as every caller that has only
    a string can do — nothing is ever claimed as a path, which is the same
    conservative result as an item that names nothing.
    """
    phase: str | None = None
    in_fence = False
    # phase -> list of (state, task, paths)
    per_phase: dict[str, list[tuple[str, str, tuple[str, ...]]]] = {}
    order: list[str] = []

    for line in text.splitlines():
        # A fenced block is illustration, not backlog. Without this, a roadmap
        # that documents its own conventions hands its own worked example to
        # Crew as real work — the failure mode that already bit the TODO
        # scanner on docs/superpowers fixtures. An unclosed fence deliberately
        # swallows the remainder: losing candidates is recoverable, inventing
        # them is not.
        if _FENCE_RE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        m = _PHASE_RE.match(line)
        if m:
            phase = m.group(1)
            if phase not in per_phase:
                per_phase[phase] = []
                order.append(phase)
            continue
        if phase is None:
            continue
        m = _ITEM_RE.match(line)
        if not m:
            continue
        raw = m.group(2)
        state, task = m.group(1), _clean(raw)
        if not task:
            continue
        paths = paths_in(raw, exists) if exists is not None else ()
        per_phase[phase].append((state, task, paths))

    # A phase carrying an in-progress marker is the milestone in flight.
    current = {p for p, items in per_phase.items() if any(s == "~" for s, _, _ in items)}

    out: list[Candidate] = []
    for phase in order:
        for state, task, paths in per_phase[phase]:
            if state == "x":
                continue
            out.append(
                Candidate(
                    task=task,
                    source=f"roadmap:phase-{phase}",
                    kind="roadmap",
                    paths=paths,
                    current_milestone=phase in current,
                    detail="in progress" if state == "~" else "",
                )
            )
    return out


def collect(root: Path) -> list[Candidate]:
    """Read docs/ROADMAP.md under ``root``. Missing or unreadable is not an error.

    ``UnicodeDecodeError`` is caught alongside ``OSError``: it is a
    ``ValueError`` subclass, not an ``OSError``, so a roadmap file containing
    invalid UTF-8 would otherwise slip past an ``except OSError`` and break
    the "never raises" contract every collector in this package promises.
    """
    p = root / ROADMAP_PATH

    def exists(rel: str) -> bool:
        # `looks_like_path` has already refused traversal and absolute forms,
        # so this join stays inside `root`. `is_file()` and not `exists()`:
        # a directory is not an edit target, and handing one to the leash
        # would misdescribe what the work touches.
        try:
            return (root / rel).is_file()
        except OSError:
            return False

    try:
        return parse(p.read_text(encoding="utf-8"), exists=exists)
    except (OSError, UnicodeDecodeError):
        return []
