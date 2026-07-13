"""Central configuration for Maz Crew.

Model IDs and workflow tunables live here so they are easy to change in one place.
The Claude Agent SDK accepts short model aliases ("opus", "sonnet", "haiku") which
map to the current default model in each tier — that is what we use by default so
the tool keeps working as new model versions ship. Set the CREW_* env vars to pin
exact model IDs (e.g. "claude-opus-4-8") if you want reproducibility.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field, fields
from pathlib import Path

# A committable project config file (JSON so it works on every supported Python
# with no extra dependency). Lives at the project root next to where you run
# `crew`, unlike the gitignored `.crew/` session state.
CONFIG_FILENAME = "crew.json"

# Fields a project config file is allowed to set.
_FILE_FIELDS = (
    "planner_model",
    "coder_model",
    "reviewer_model",
    "tester_model",
    "max_fix_rounds",
    "max_turns",
)

# --- Model tiers -----------------------------------------------------------
# Aliases are resolved by the SDK to the current model in each tier. Override
# with exact IDs via env vars when you need pinned, reproducible behaviour.
OPUS = os.environ.get("CREW_MODEL_OPUS", "opus")
SONNET = os.environ.get("CREW_MODEL_SONNET", "sonnet")
HAIKU = os.environ.get("CREW_MODEL_HAIKU", "haiku")


@dataclass(frozen=True)
class CrewConfig:
    """Runtime configuration for a crew run."""

    # Which model each role uses. Reviewer runs on the strongest tier because a
    # missed bug there is the most expensive kind. Tester just runs commands, so
    # it uses the cheapest fast tier.
    planner_model: str = SONNET
    coder_model: str = SONNET
    reviewer_model: str = OPUS
    tester_model: str = HAIKU

    # Bound on the coder<->tester repair loop so a stubborn failure can't spin
    # forever (and burn tokens).
    max_fix_rounds: int = 3

    # Total agent turns allowed per phase before the SDK stops on its own.
    max_turns: int = 40

    # Directory where per-project session state is written (relative to cwd).
    state_dirname: str = ".crew"

    # Extra tunables can be threaded through here later.
    extra: dict = field(default_factory=dict)

    def state_dir(self, root: Path | None = None) -> Path:
        return (root or Path.cwd()) / self.state_dirname


def _read_config_file(root: Path | None = None) -> dict:
    """Read allowed fields from ``crew.json`` at ``root`` (cwd by default)."""
    p = (root or Path.cwd()) / CONFIG_FILENAME
    if not p.exists():
        return {}
    try:
        data = json.loads(p.read_text())
    except (json.JSONDecodeError, ValueError, OSError):
        # A broken config file should never crash the tool — ignore it.
        return {}
    if not isinstance(data, dict):
        return {}
    return {k: v for k, v in data.items() if k in _FILE_FIELDS and v is not None}


def load_config(root: Path | None = None) -> CrewConfig:
    """Build a config from defaults, then ``crew.json``, then env overrides.

    Precedence (lowest to highest): dataclass defaults < ``crew.json`` < the
    ``CREW_MAX_*`` env vars (so a committed file sets project defaults while env
    vars stay handy for one-off runs).
    """
    kwargs: dict = _read_config_file(root)

    if v := os.environ.get("CREW_MAX_FIX_ROUNDS"):
        kwargs["max_fix_rounds"] = v
    if v := os.environ.get("CREW_MAX_TURNS"):
        kwargs["max_turns"] = v

    # Coerce the numeric fields (JSON may already give ints; env gives strings).
    for key in ("max_fix_rounds", "max_turns"):
        if key in kwargs:
            kwargs[key] = int(kwargs[key])

    return CrewConfig(**kwargs)


def effective_values(config: CrewConfig) -> dict:
    """The user-facing config fields, for display by `crew config`."""
    return {f.name: getattr(config, f.name) for f in fields(config) if f.name in _FILE_FIELDS}


def default_config_dict() -> dict:
    """The settable fields at their defaults — the body of a starter crew.json."""
    return effective_values(CrewConfig())


def write_starter_config(root: Path | None = None, force: bool = False) -> tuple[bool, Path]:
    """Write a starter ``crew.json`` at ``root``. Returns (written, path).

    Won't clobber an existing file unless ``force`` is set.
    """
    p = (root or Path.cwd()) / CONFIG_FILENAME
    if p.exists() and not force:
        return (False, p)
    p.write_text(json.dumps(default_config_dict(), indent=2) + "\n")
    return (True, p)
