"""SENSE — read the repo's vital signs into one file.

Every collector is called inside its own try/except and its outcome recorded in
the pulse. A broken parser degrades the night to fewer candidates; it never ends
it. The pulse is scratch state (gitignored) but it is written to disk anyway, so
that a run which goes wrong can be diagnosed after the fact.

``except Exception`` is deliberately used, not a bare ``except:`` — it lets
``KeyboardInterrupt`` and ``SystemExit`` (both ``BaseException``, not
``Exception``) pass through uncaught, so an operator stopping a run or the
process being asked to exit is not mistaken for "one collector misbehaved" and
silently swallowed. Everything a misbehaving collector can actually do —
raise, or hand back something that isn't a list of ``Candidate`` — is an
``Exception`` and is covered.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from .config import ForgeConfig
from .models import Candidate
from .signals import ci as ci_signal
from .signals import memory as memory_signal
from .signals import roadmap as roadmap_signal
from .signals import todos as todo_signal

PULSE_FILENAME = "pulse.json"

DEFAULT_COLLECTORS = {
    "ci": ci_signal.collect,
    "roadmap": roadmap_signal.collect,
    "todo": todo_signal.collect,
    "memory": memory_signal.collect,
}


def candidate_to_dict(c: Candidate) -> dict:
    return {
        "task": c.task,
        "source": c.source,
        "kind": c.kind,
        "paths": list(c.paths),
        "current_milestone": c.current_milestone,
        "tests_nearby": c.tests_nearby,
        "detail": c.detail,
        "key": c.key(),
    }


def candidate_from_dict(d: dict) -> Candidate:
    return Candidate(
        task=d["task"],
        source=d["source"],
        kind=d["kind"],
        paths=tuple(d.get("paths") or ()),
        current_milestone=bool(d.get("current_milestone")),
        tests_nearby=bool(d.get("tests_nearby")),
        detail=d.get("detail", ""),
    )


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def sense(root: Path, config: ForgeConfig, collectors: dict | None = None) -> dict:
    """Run every collector and assemble the pulse. Never raises.

    ``list(collect(root))`` already turns a ``None`` or other non-iterable
    return value into a ``TypeError`` that the ``except`` below records — but
    it would happily accept an iterable of the *wrong* shape (a dict yields
    its keys, a list of plain strings or dicts sails straight through) and
    hand that on to ``candidate_to_dict`` further down, which does attribute
    access and would blow up outside any per-collector guard, taking every
    other source's candidates down with it. So every item is checked against
    ``Candidate`` here, inside the same try — a collector that returns the
    wrong shape is treated exactly like one that raised: it fails alone.
    """
    collectors = DEFAULT_COLLECTORS if collectors is None else collectors
    sources: dict[str, dict] = {}
    candidates: list[Candidate] = []

    for name, collect in collectors.items():
        try:
            found = list(collect(root))
            for item in found:
                if not isinstance(item, Candidate):
                    raise TypeError(
                        f"collector {name!r} returned {type(item).__name__!r}, "
                        "expected Candidate"
                    )
        except Exception as exc:  # noqa: BLE001 — record and carry on
            sources[name] = {"ok": False, "count": 0, "error": str(exc)}
            continue
        sources[name] = {"ok": True, "count": len(found), "error": ""}
        candidates.extend(found)

    return {
        "generated_at": _now(),
        "sources": sources,
        "candidates": [candidate_to_dict(c) for c in candidates],
    }


def write_pulse(pulse: dict, root: Path, config: ForgeConfig) -> Path:
    d = config.state_dir(root)
    d.mkdir(parents=True, exist_ok=True)
    path = d / PULSE_FILENAME
    path.write_text(json.dumps(pulse, indent=2) + "\n", encoding="utf-8")
    return path


def read_pulse(root: Path, config: ForgeConfig) -> dict:
    """Read back the last pulse. Missing, unreadable, or malformed is {}, not an error.

    A pulse file is only ever written by ``write_pulse`` above, which always
    writes a dict — but this reads scratch state that could have been hand
    edited or left over from a future format, so the same "ignore, don't
    crash" guard `config.py` uses for its own JSON file is applied here too:
    valid JSON that isn't a dict (a list, a number, ``null``) is treated the
    same as no pulse at all, rather than handed back to a caller expecting
    ``pulse["sources"]`` to work.
    """
    path = config.state_dir(root) / PULSE_FILENAME
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, ValueError, OSError):
        return {}
    return data if isinstance(data, dict) else {}
