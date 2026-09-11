"""Write a Markdown transcript of a crew run for later review.

Runs stream a lot of output to the terminal; once it scrolls, it's gone. Saving a
transcript under ``.crew/runs/`` means you can go back and read exactly what each
agent planned, changed, and reported — especially handy for unattended runs.
"""

from __future__ import annotations

from pathlib import Path


def format_transcript(task: str, records: list[tuple[str, str]]) -> str:
    """Render (phase title, phase text) pairs into a Markdown document."""
    lines = ["# Crew run", "", f"**Task:** {task}", ""]
    for title, text in records:
        lines.append(f"## {title.strip()}")
        lines.append("")
        lines.append(text.strip() or "_(no output)_")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def _safe_name(name: str) -> str:
    cleaned = "".join(c if (c.isalnum() or c in "-_.") else "_" for c in name)
    return cleaned or "run"


def write_transcript(runs_dir: Path, name: str, content: str) -> Path:
    """Write ``content`` to ``runs_dir/<safe name>.md``, creating dirs as needed."""
    runs_dir.mkdir(parents=True, exist_ok=True)
    path = runs_dir / f"{_safe_name(name)}.md"
    path.write_text(content)
    return path


_TASK_PREFIX = "**Task:**"


def parse_task(path: Path) -> str:
    """Recover the task line from a transcript, or a placeholder if unreadable."""
    try:
        for line in path.read_text().splitlines():
            if line.startswith(_TASK_PREFIX):
                return line[len(_TASK_PREFIX):].strip()
    except OSError:
        pass
    return "(unknown)"


def list_runs(runs_dir: Path) -> list[tuple[Path, str]]:
    """List saved transcripts as (path, task), most recently modified first."""
    if not runs_dir.exists():
        return []
    files = [p for p in runs_dir.glob("*.md") if p.is_file()]
    files.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return [(p, parse_task(p)) for p in files]


def resolve_run(entries: list[tuple[Path, str]], target: str | None) -> Path | None:
    """Pick a run from ``list_runs`` output by 1-based index or name/session id.

    ``None`` target selects the most recent run. Returns ``None`` if nothing matches.
    """
    if not entries:
        return None
    if target is None:
        return entries[0][0]
    if target.isdigit():
        idx = int(target)
        return entries[idx - 1][0] if 1 <= idx <= len(entries) else None
    for path, _ in entries:
        if path.stem == target:
            return path
    return None
