"""The leash.

Every limit the Forge operates under lives in one committable JSON file at the
repo root. JSON (not TOML) because Python 3.10 has no stdlib TOML reader and the
Forge must stay dependency-free — `crew.json` set the same precedent.

Two zone lists do the real safety work:

  safe_zones  paths the Forge is allowed to change. Anything outside is skipped.
  no_touch    paths it may never change, even if a safe zone would allow it.

`forge/` and `.github/workflows/` are welded into `no_touch` on every load: the
thing that decides does not get to rewrite its own rules, and a config edit must
not be able to unweld that.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field, fields
from pathlib import Path

CONFIG_FILENAME = "forge.json"

# Paths the Forge may never author a change to, no matter what the file says.
HARD_NO_TOUCH = ("forge/", ".github/workflows/")

_FILE_FIELDS = (
    "budget_usd",
    "max_files_touched",
    "safe_zones",
    "no_touch",
    "score_floor",
    "weights",
    "crew_timeout_min",
    "strike_limit",
    "base_branch",
)

# Default scoring weights. Tuning these must never require a code change.
DEFAULT_WEIGHTS = {
    "value_ci_red": 10.0,
    "value_roadmap_current": 7.0,
    "value_todo_recent": 5.0,
    "value_todo_stale": 2.0,
    "value_roadmap_later": 3.0,
    "value_memory": 4.0,
    # The inventory's own priorities: P1 is something broken or unprotected, P2
    # a real coverage gap, P3 polish. Pitched around the other signals — a
    # broken gate matters more than a roadmap item, polish less than a stale
    # TODO — so one signal cannot crowd the others out of every night.
    "value_inventory_p1": 8.0,
    "value_inventory_p2": 5.0,
    "value_inventory_p3": 1.5,
    "confidence_base": 5.0,
    "confidence_tests_nearby": 2.0,
    "confidence_small_scope": 2.0,
    "risk_base": 1.0,
    "risk_build_system": 6.0,
    "risk_engine_core": 5.0,
    "risk_cross_cutting": 3.0,
}


@dataclass(frozen=True)
class ForgeConfig:
    """Everything the Forge is and isn't allowed to do."""

    # Cap for a single run's cost, checked once Crew reports a number. This
    # binds only when the runner actually reports a cost: the bundled Crew
    # runner (`do._default_crew`) has no machine-readable cost to read and
    # reports `None` ("not measured"), so this cap is currently enforced
    # only for an injected runner under test, not for a real run. It is not
    # dead code — a future Crew that reports spend, or any other injected
    # runner, makes it bind for real — but do not read this field as a live
    # guarantee about production spend today.
    budget_usd: float = 5.0
    max_files_touched: int = 12

    # Where the Forge may work. Week 1-2 values; widen only on ledger evidence.
    #
    # `tests/` is deliberately absent: in this repo that directory is C++
    # (CMakeLists.txt, unit_*.cpp), verified only by a `cmake`/`ctest` build
    # against the Vulkan SDK (see .github/workflows/ci.yml) — a build this
    # module cannot run and no command checks.PROJECT_CHECKS may honestly
    # invent. A zone the Forge cannot verify is not a safe zone to work in
    # by default, whatever a green run there would otherwise look like.
    safe_zones: tuple[str, ...] = (
        "docs/",
        "madlibs/",
        "music/",
        "shooter/",
        "scraper/",
        "crew/tests/",
    )
    no_touch: tuple[str, ...] = HARD_NO_TOUCH

    # Below this score, the run records "nothing worth doing" and exits clean.
    score_floor: float = 4.0

    weights: dict = field(default_factory=lambda: dict(DEFAULT_WEIGHTS))

    # Wall-clock cap on the Crew subprocess.
    crew_timeout_min: int = 45

    # Failures on the same task before it is quarantined to forge/stuck.md.
    strike_limit: int = 3

    # What every PR's `base` is opened against, and what the tree is
    # returned to after `_abandon`/`_return_to_base` run in orchestrate.py.
    # "main" is the right default for most repositories, and since the two
    # trunks were merged it is right for this one too — forge.json now sets it
    # explicitly rather than overriding it to something else. A repo whose real
    # mainline is some other branch must still override it, or the Forge opens
    # PRs against, and returns the tree to, an unrelated line of the project.
    base_branch: str = "main"

    state_dirname: str = "forge/state"
    ledger_dirname: str = "forge/ledger"

    def state_dir(self, root: Path | None = None) -> Path:
        return (root or Path.cwd()) / self.state_dirname

    def ledger_dir(self, root: Path | None = None) -> Path:
        return (root or Path.cwd()) / self.ledger_dirname


def _read_config_file(root: Path | None = None) -> dict:
    """Read allowed fields from ``forge.json``. A broken file is ignored, never fatal."""
    p = (root or Path.cwd()) / CONFIG_FILENAME
    if not p.exists():
        return {}
    try:
        data = json.loads(p.read_text())
    except (json.JSONDecodeError, ValueError, OSError):
        return {}
    if not isinstance(data, dict):
        return {}
    return {k: v for k, v in data.items() if k in _FILE_FIELDS and v is not None}


def load_config(root: Path | None = None) -> ForgeConfig:
    """Build a config from dataclass defaults, then ``forge.json`` on top.

    Every field is coerced on its own and a bad value for one field is simply
    dropped, falling back to that field's default — a typo in ``budget_usd``
    must not stop ``safe_zones`` (or anything else) from taking effect, and it
    must never crash ``load_config`` itself. A crash here would happen before
    the run can write a ledger line, and every run — including a broken one —
    has to leave a trace.
    """
    kwargs = _read_config_file(root)

    # A bare string is iterable, so a typo like {"safe_zones": "docs/"} would
    # silently explode into one safe zone per character ('d', 'o', 'c', 's',
    # '/') and, because zone matching is startswith-based, that widens the
    # allowlist to nearly everything instead of narrowing it. Only a real
    # list/tuple of strings is accepted; anything else falls back to default.
    #
    # The same startswith-based matching means an empty (or whitespace-only)
    # *element* inside an otherwise well-formed list is just as dangerous,
    # by the identical mechanism: `"".startswith("")` is true for every
    # path, so `{"safe_zones": ["docs/", ""]}` — a trailing comma or a blank
    # line in hand-edited JSON, easy to introduce and easy to miss — widens
    # the allowlist to the entire repo instead of narrowing it, exactly like
    # the bare-string typo above, just one level deeper. `no_touch` is not
    # exempt either: a blank element there is simply a no-op prefix, never a
    # widening one, since `no_touch` only ever narrows what `safe_zones`
    # allows — but it is stripped for the same reason and by the same code
    # path, so the two lists cannot silently diverge in how blanks are
    # handled. Empty/blank elements are therefore dropped from both lists
    # before use, never merely tolerated.
    #
    # Dropping every element of `safe_zones` this way can leave it empty —
    # a config file of `{"safe_zones": ["", "  "]}` is exactly that case.
    # An empty tuple there does not read as "the operator wants a locked-down
    # Forge"; it reads as a config that, after the one legitimate cleanup
    # this loop performs, no longer says anything at all — indistinguishable
    # from having declared no `safe_zones` in the first place. So it falls
    # back to the default zone list, matching every other malformed-field
    # case in this function, rather than to "nothing is safe": the latter
    # would silently turn a sloppy trailing comma into total lockout instead
    # of the harmless no-op it should be. `no_touch` needs no matching
    # special case: an empty *declared* `no_touch` is already exactly what
    # the dataclass default (`HARD_NO_TOUCH` alone) welds down to below, so
    # there is nothing to fall back to that isn't already the outcome.
    for key in ("safe_zones", "no_touch"):
        if key in kwargs:
            value = kwargs[key]
            if isinstance(value, (list, tuple)) and not isinstance(value, str):
                cleaned = tuple(str(v) for v in value if str(v).strip())
                if key == "safe_zones" and not cleaned:
                    del kwargs[key]
                else:
                    kwargs[key] = cleaned
            else:
                del kwargs[key]

    # A non-string or blank base_branch (a stray `{"base_branch": 3}` or
    # `""`) is exactly the same class of typo `safe_zones`/`no_touch` are
    # guarded against above: silently falling back to the default here is
    # correct precisely because the default ("main") is a real, usable
    # branch name — unlike those fields, there is no widened-allowlist
    # danger to worry about, just a bad value that must not crash the load.
    if "base_branch" in kwargs:
        value = kwargs["base_branch"]
        if isinstance(value, str) and value.strip():
            kwargs["base_branch"] = value.strip()
        else:
            del kwargs["base_branch"]

    for key in ("budget_usd", "score_floor"):
        if key in kwargs:
            try:
                kwargs[key] = float(kwargs[key])
            except (TypeError, ValueError):
                del kwargs[key]

    for key in ("max_files_touched", "crew_timeout_min", "strike_limit"):
        if key in kwargs:
            try:
                kwargs[key] = int(kwargs[key])
            except (TypeError, ValueError):
                del kwargs[key]

    if "weights" in kwargs:
        raw = kwargs["weights"]
        if isinstance(raw, dict):
            # Only known weight keys are merged in, matching the same
            # unknown-key-is-ignored philosophy applied to top-level fields.
            merged = dict(DEFAULT_WEIGHTS)
            for k, v in raw.items():
                if k not in DEFAULT_WEIGHTS:
                    continue
                try:
                    merged[k] = float(v)
                except (TypeError, ValueError):
                    continue
            kwargs["weights"] = merged
        else:
            del kwargs["weights"]

    # Weld the hard no-touch paths on, whatever the file said.
    declared = tuple(kwargs.get("no_touch", ForgeConfig.no_touch))
    kwargs["no_touch"] = tuple(dict.fromkeys(HARD_NO_TOUCH + declared))

    return ForgeConfig(**kwargs)


def default_config_dict() -> dict:
    """The settable fields at their defaults — the body of a starter forge.json."""
    cfg = ForgeConfig()
    out = {}
    for f in fields(cfg):
        if f.name not in _FILE_FIELDS:
            continue
        value = getattr(cfg, f.name)
        out[f.name] = list(value) if isinstance(value, tuple) else value
    return out


def write_starter_config(root: Path | None = None, force: bool = False) -> tuple[bool, Path]:
    """Write a starter ``forge.json``. Returns (written, path). Won't clobber."""
    p = (root or Path.cwd()) / CONFIG_FILENAME
    if p.exists() and not force:
        return (False, p)
    p.write_text(json.dumps(default_config_dict(), indent=2) + "\n")
    return (True, p)
