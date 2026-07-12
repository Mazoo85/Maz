"""Central configuration for Maz Crew.

Model IDs and workflow tunables live here so they are easy to change in one place.
The Claude Agent SDK accepts short model aliases ("opus", "sonnet", "haiku") which
map to the current default model in each tier — that is what we use by default so
the tool keeps working as new model versions ship. Set the CREW_* env vars to pin
exact model IDs (e.g. "claude-opus-4-8") if you want reproducibility.
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path

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


def load_config() -> CrewConfig:
    """Build a config, honouring env overrides for the loop bounds."""
    kwargs: dict = {}
    if v := os.environ.get("CREW_MAX_FIX_ROUNDS"):
        kwargs["max_fix_rounds"] = int(v)
    if v := os.environ.get("CREW_MAX_TURNS"):
        kwargs["max_turns"] = int(v)
    return CrewConfig(**kwargs)
