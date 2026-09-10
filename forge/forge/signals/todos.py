"""Leftovers: TODO / FIXME / HACK markers in tracked source.

Two git calls, not one per file: `git ls-files` enumerates what is tracked, and
a single `git log --name-only` pass names everything changed recently. A marker
in a file you touched last week is worth more than one that has sat for a year,
so recency rides along on the candidate.
"""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

from ..config import HARD_NO_TOUCH
from ..models import Candidate

MARKER_RE = re.compile(r"\b(TODO|FIXME|HACK)\b\s*[:\-]\s*(\S.*)")

# Extensions worth scanning. Binary and vendored files are skipped outright.
SCANNABLE = {
    ".py", ".js", ".mjs", ".ts", ".c", ".h", ".cc", ".cpp", ".hpp",
    ".css", ".html", ".md", ".yml", ".yaml", ".toml", ".json", ".sh", ".cmake",
}

# Directories whose markers can never represent real outstanding work, so
# scanning them is pure noise (or, worse, an actionable-looking false
# positive). `HARD_NO_TOUCH` (forge/, .github/workflows/) is imported rather
# than re-listed so the scanner and the leash cannot drift apart: the thing
# that decides what to scan and the thing that decides what may be changed
# must agree on what "the Forge's own machinery" means.
#
#   forge/                the Forge's own source, tests, and this scanner's
#                          own regression fixtures — inert, `zone_for` already
#                          rejects it, but scanning it is still noise.
#   .github/workflows/    covered by HARD_NO_TOUCH; CI can never be actioned.
#   .github/              broadened by judgement beyond just workflows/: issue
#                          templates, CODEOWNERS, and similar repo-admin files
#                          under .github/ are tooling metadata, not project
#                          source, so the same "not real work" reasoning
#                          applies to the whole directory.
#   docs/superpowers/     plans and specs — documents *about* work, not work
#                          itself. Their worked examples contain literal
#                          TODO:/FIXME:/HACK: strings as illustrative fixtures,
#                          and (unlike the rest of docs/) this subtree is a
#                          safe zone, so a fixture here was picked as real
#                          work by a live run — the one false positive that is
#                          actually dangerous rather than merely inert.
#   .claude/               skills, settings, and the memory graph: tooling
#                          configuration, not project source.
SKIP_DIRS = (
    "engine/third_party/",
    "node_modules/",
    "build/",
    *HARD_NO_TOUCH,
    ".github/",
    "docs/superpowers/",
    ".claude/",
)

# Cap the work handed to a single night.
MAX_TASK_CHARS = 160


def _under_skip_dir(path: str, skip_dirs: tuple[str, ...] = SKIP_DIRS) -> bool:
    """True if `path` sits inside any of `skip_dirs`, at any depth.

    A skip dir is one or more path segments (e.g. "build/" or
    "engine/third_party/"). Matching must be segment-aware, not
    `str.startswith`: `startswith` only catches a skip dir at the repo root
    (a nested "sub/build/x.js" would slip through) and, worse, matches a
    prefix of a *name* rather than a whole segment ("builds/" and
    "node_modules_old/" must NOT be caught by "build/" and "node_modules/").
    Comparing whole path segments, at every starting position, avoids both
    failure modes at once. Only directory segments are checked — the final
    segment (the filename) is excluded, since skip dirs are directories.
    """
    segments = path.split("/")[:-1]
    for skip in skip_dirs:
        needle = [s for s in skip.split("/") if s]
        n = len(needle)
        if n == 0:
            continue
        for i in range(len(segments) - n + 1):
            if segments[i : i + n] == needle:
                return True
    return False


def _git(args: list[str], root: Path) -> str:
    r = subprocess.run(["git", *args], cwd=str(root), capture_output=True, text=True)
    if r.returncode != 0:
        return ""
    return r.stdout


def scan_text(path: str, text: str, recent: bool) -> list[Candidate]:
    """Find markers in one file's text. Never raises."""
    out: list[Candidate] = []
    for lineno, line in enumerate(text.splitlines(), start=1):
        m = MARKER_RE.search(line)
        if not m:
            continue
        marker, body = m.group(1), m.group(2).strip().rstrip("*/").strip()
        if not body:
            continue
        out.append(
            Candidate(
                task=f"Resolve the {marker} in {path}: {body[:MAX_TASK_CHARS]}",
                source=f"todo:{path}:{lineno}",
                kind="todo",
                paths=(path,),
                detail="recent" if recent else "",
            )
        )
    return out


def collect(root: Path, runner=None, recent_days: int = 45) -> list[Candidate]:
    """Scan every tracked, scannable file for markers. Never raises."""
    run = runner or (lambda args: _git(args, root))
    try:
        listing = run(["ls-files", "-z"])
        changed = run(["log", f"--since={recent_days}.days", "--name-only", "--format="])
    except Exception:  # noqa: BLE001 — a broken signal must not stop the night
        return []

    recent_paths = {p.strip() for p in (changed or "").splitlines() if p.strip()}

    out: list[Candidate] = []
    for path in (p for p in (listing or "").split("\x00") if p):
        if _under_skip_dir(path):
            continue
        if Path(path).suffix.lower() not in SCANNABLE:
            continue
        try:
            text = (root / path).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        out.extend(scan_text(path, text, recent=path in recent_paths))
    return out
