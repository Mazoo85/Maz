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
# CommonMark fenced code blocks, close enough for a backlog file. A naive
# "does the line start with ``` — toggle a boolean" is NOT enough, and the
# difference is a safety bug rather than a formatting nit: any line this
# parser calls a fence but CommonMark does not (or the reverse) inverts the
# state for the rest of the document, and an illustration inside a fenced
# block becomes a real candidate handed to Crew. Three rules do the work:
#
#   * an opener is 3+ of the same character (` or ~), indented at most 3
#     spaces — 4 spaces is an indented code block, not a fence;
#   * a backtick opener's info string may not contain a backtick, so a
#     line-initial inline span (```forge sense``` prose) is a paragraph;
#   * a closer must use the opener's character, run at least as long, and
#     carry no info string — which is what lets a ````-block quote a
#     ```-block, the construct documenting this very rule requires.
_FENCE_RE = re.compile(r"^ {0,3}(`{3,}|~{3,})(.*)$")


def _fence_opens(line: str) -> tuple[str, int] | None:
    """The (character, length) of the fence this line opens, or None."""
    m = _FENCE_RE.match(line)
    if not m:
        return None
    run, info = m.group(1), m.group(2)
    if run[0] == "`" and "`" in info:
        return None
    return run[0], len(run)


def _fence_closes(line: str, char: str, length: int) -> bool:
    """True if `line` closes a fence opened with `length` of `char`."""
    m = _FENCE_RE.match(line)
    if not m:
        return False
    run, info = m.group(1), m.group(2)
    return run[0] == char and len(run) >= length and not info.strip()


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
    if any(ch.isspace() for ch in token):
        return False
    if "\\" in token:
        return False
    # Every segment, not just the container: "", ".", ".." each rule out the
    # token. The empty-segment rule is what refuses absolute paths too —
    # "/etc/passwd".split("/") leads with "" — so there is deliberately no
    # separate startswith("/") clause to drift out of step with this one.
    return not any(seg in ("", ".", "..") for seg in token.split("/"))


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
        except Exception:  # noqa: BLE001 — parse() promises it never raises
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
    fence: tuple[str, int] | None = None
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
        if fence is not None:
            if _fence_closes(line, *fence):
                fence = None
            continue
        opened = _fence_opens(line)
        if opened is not None:
            fence = opened
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
    try:
        root_real = root.resolve()
    except OSError:
        return []

    def exists(rel: str) -> bool:
        # `is_file()` and not `exists()`: a directory is not an edit target,
        # and handing one to the leash would misdescribe what the work touches.
        #
        # `looks_like_path` has already refused traversal and absolute forms,
        # so the join is lexically inside `root` — but lexically is not the
        # same as actually, because a symlink inside a safe zone can point
        # anywhere. `zones.py` deliberately refuses to resolve paths, so
        # extraction is the only layer that can catch it, and the check has
        # to be on the resolved target rather than the written name.
        try:
            target = (root / rel).resolve()
            return target.is_file() and target.is_relative_to(root_real)
        except OSError:
            return False

    try:
        return parse(p.read_text(encoding="utf-8"), exists=exists)
    except (OSError, UnicodeDecodeError):
        return []
