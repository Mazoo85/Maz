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

from ..models import Candidate

MARKER_RE = re.compile(r"\b(TODO|FIXME|HACK)\b\s*[:\-]\s*(\S.*)")

# Extensions worth scanning. Binary and vendored files are skipped outright.
SCANNABLE = {
    ".py", ".js", ".mjs", ".ts", ".c", ".h", ".cc", ".cpp", ".hpp",
    ".css", ".html", ".md", ".yml", ".yaml", ".toml", ".json", ".sh", ".cmake",
}

SKIP_DIRS = ("engine/third_party/", "node_modules/", "build/")

# Cap the work handed to a single night.
MAX_TASK_CHARS = 160


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
        if any(path.startswith(d) for d in SKIP_DIRS):
            continue
        if Path(path).suffix.lower() not in SCANNABLE:
            continue
        try:
            text = (root / path).read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        out.extend(scan_text(path, text, recent=path in recent_paths))
    return out
