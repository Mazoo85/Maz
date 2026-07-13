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
