"""The vocabulary of a consolidation: a source repo, its placement, the plan.

Everything here is a frozen dataclass with no behaviour beyond derived values,
so a plan can be built, serialised, diffed and asserted on in a unit test
without touching git or the network.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, replace
from typing import Iterable, Literal

#: What we decided to do with a source repository.
#: ``include`` folds it in; ``skip`` leaves it alone and says why.
Disposition = Literal["include", "skip"]


@dataclass(frozen=True)
class SourceRepo:
    """One repository we might fold into the consolidated repo."""

    owner: str
    name: str
    url: str
    description: str = ""
    default_branch: str = "main"
    is_fork: bool = False
    archived: bool = False
    empty: bool = False
    visibility: str = "public"
    pushed_at: str = ""

    @property
    def slug(self) -> str:
        return f"{self.owner}/{self.name}"

    def to_dict(self) -> dict:
        return {
            "owner": self.owner,
            "name": self.name,
            "url": self.url,
            "description": self.description,
            "default_branch": self.default_branch,
            "is_fork": self.is_fork,
            "archived": self.archived,
            "empty": self.empty,
            "visibility": self.visibility,
            "pushed_at": self.pushed_at,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "SourceRepo":
        known = {f: d[f] for f in cls.__dataclass_fields__ if f in d}
        return cls(**known)


@dataclass(frozen=True)
class Placement:
    """Where one source repo lands in the consolidated repo — or why it doesn't."""

    repo: SourceRepo
    dest: str
    disposition: Disposition
    reason: str = ""

    @property
    def included(self) -> bool:
        return self.disposition == "include"

    def to_dict(self) -> dict:
        return {
            "repo": self.repo.to_dict(),
            "dest": self.dest,
            "disposition": self.disposition,
            "reason": self.reason,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "Placement":
        return cls(
            repo=SourceRepo.from_dict(d["repo"]),
            dest=d["dest"],
            disposition=d["disposition"],
            reason=d.get("reason", ""),
        )


@dataclass(frozen=True)
class Plan:
    """The whole decision, ready to be printed, saved, reviewed and then run."""

    dest_name: str
    prefix: str
    placements: tuple[Placement, ...] = ()

    @property
    def included(self) -> tuple[Placement, ...]:
        return tuple(p for p in self.placements if p.included)

    @property
    def skipped(self) -> tuple[Placement, ...]:
        return tuple(p for p in self.placements if not p.included)

    def with_placements(self, placements: Iterable[Placement]) -> "Plan":
        return replace(self, placements=tuple(placements))

    def to_dict(self) -> dict:
        return {
            "dest_name": self.dest_name,
            "prefix": self.prefix,
            "placements": [p.to_dict() for p in self.placements],
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), indent=2) + "\n"

    @classmethod
    def from_dict(cls, d: dict) -> "Plan":
        return cls(
            dest_name=d["dest_name"],
            prefix=d.get("prefix", "projects"),
            placements=tuple(Placement.from_dict(p) for p in d.get("placements", ())),
        )

    @classmethod
    def from_json(cls, text: str) -> "Plan":
        return cls.from_dict(json.loads(text))


@dataclass(frozen=True)
class Step:
    """One thing the builder did, or would do in a dry run.

    ``ok`` is None for a step that was only planned and never executed.
    """

    action: str
    detail: str
    ok: bool | None = None
    output: str = ""

    def to_dict(self) -> dict:
        return {"action": self.action, "detail": self.detail, "ok": self.ok, "output": self.output}
