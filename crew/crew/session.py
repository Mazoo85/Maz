"""Per-project session persistence.

We store the SDK ``session_id`` (plus a little metadata) under ``.crew/session.json``
in the working directory so a task can be resumed after the terminal is closed. The
SDK itself keeps the full transcript under ``~/.claude/projects/<cwd>/<id>.jsonl``;
we only need to remember which id belongs to this project.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path

from .config import CrewConfig

SESSION_FILE = "session.json"


@dataclass
class SessionState:
    session_id: str | None = None
    task: str | None = None
    phase: str | None = None  # last completed phase: plan|code|review|test|done


def _path(config: CrewConfig, root: Path | None = None) -> Path:
    return config.state_dir(root) / SESSION_FILE


def load(config: CrewConfig, root: Path | None = None) -> SessionState:
    p = _path(config, root)
    if not p.exists():
        return SessionState()
    try:
        data = json.loads(p.read_text())
        return SessionState(**{k: data.get(k) for k in SessionState().__dict__})
    except (json.JSONDecodeError, TypeError, ValueError):
        # Corrupt state should never crash the tool — start fresh.
        return SessionState()


def save(state: SessionState, config: CrewConfig, root: Path | None = None) -> Path:
    d = config.state_dir(root)
    d.mkdir(parents=True, exist_ok=True)
    p = d / SESSION_FILE
    p.write_text(json.dumps(asdict(state), indent=2))
    return p


def clear(config: CrewConfig, root: Path | None = None) -> None:
    p = _path(config, root)
    if p.exists():
        p.unlink()
