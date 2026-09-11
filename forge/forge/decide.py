"""DECIDE — weigh every candidate and pick exactly one.

    score = (value x confidence) / (1 + risk)

Deliberately readable arithmetic, not a model. Anyone should be able to look at
a night's pick and follow why it won; that explanation is written into the
ledger, and it is the training data a learned scorer (P3) will eventually use.

Three sanity rules sit on top of the arithmetic:

  three strikes  a candidate that has failed `strike_limit` times is quarantined
  variety        never the same zone three acting runs in a row
  score floor    below it, the night records "nothing worth doing" and exits

The floor matters more than it looks. A loop that MUST produce output will
produce garbage; silence has to be a valid night.
"""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from .config import ForgeConfig
from .models import Candidate, Scored
from .sense import candidate_from_dict, candidate_to_dict
from .zones import risk_keys_for, zone_for

TONIGHT_FILENAME = "tonight.json"

# At or below this many files, a change counts as small in scope.
SMALL_SCOPE_FILES = 2

# Runners-up recorded alongside the winner.
RUNNERS_UP = 3


def _value(candidate: Candidate, weights: dict) -> float:
    if candidate.kind == "ci":
        return weights["value_ci_red"]
    if candidate.kind == "roadmap":
        key = "value_roadmap_current" if candidate.current_milestone else "value_roadmap_later"
        return weights[key]
    if candidate.kind == "todo":
        key = "value_todo_recent" if candidate.detail == "recent" else "value_todo_stale"
        return weights[key]
    return weights["value_memory"]


def _confidence(candidate: Candidate, weights: dict) -> float:
    score = weights["confidence_base"]
    if candidate.tests_nearby:
        score += weights["confidence_tests_nearby"]
    if 0 < len(candidate.paths) <= SMALL_SCOPE_FILES:
        score += weights["confidence_small_scope"]
    return score


def _risk(candidate: Candidate, weights: dict) -> float:
    score = weights["risk_base"]
    for key in risk_keys_for(candidate.paths):
        score += weights[key]
    return score


def score_one(candidate: Candidate, zone: str, config: ForgeConfig) -> Scored:
    """Weigh a single candidate that has already cleared the zone check.

    Raises ``KeyError`` if ``config.weights`` is missing a key this candidate's
    kind needs — that can happen for a hand-built ``ForgeConfig`` (tests,
    or a caller that skips `load_config`'s merge-with-defaults step) even
    though `load_config` itself always backfills every key from
    `DEFAULT_WEIGHTS`. Callers that must never raise (`decide`, below) are
    responsible for catching it; `score_one` stays a small, honest function
    that does exactly the arithmetic and nothing else.
    """
    w = config.weights
    value = _value(candidate, w)
    confidence = _confidence(candidate, w)
    risk = _risk(candidate, w)
    return Scored(
        candidate=candidate,
        value=value,
        confidence=confidence,
        risk=risk,
        score=round((value * confidence) / (1.0 + risk), 3),
        zone=zone,
    )


def scored_to_dict(s: Scored) -> dict:
    return {
        "candidate": candidate_to_dict(s.candidate),
        "value": s.value,
        "confidence": s.confidence,
        "risk": s.risk,
        "score": s.score,
        "zone": s.zone,
    }


def _blocked_by_variety(zone: str, recent_zones: list[str]) -> bool:
    """True when the two most recent acting runs were both in this zone."""
    return len(recent_zones) >= 2 and recent_zones[0] == zone and recent_zones[1] == zone


def decide(
    pulse: dict,
    config: ForgeConfig,
    strikes: dict | None = None,
    recent_zones: list | None = None,
    exchange_ok: bool = True,
) -> dict:
    """Score everything in the pulse, apply the rules, pick one. Never raises.

    `pulse` is read back from disk (`sense.read_pulse`) or handed in directly
    by a caller/test, so nothing about its shape is trusted: it may not be a
    dict at all, its `candidates` value may not be a list, and any element of
    that list may not be a dict `candidate_from_dict` can key into. Every one
    of those must degrade to "this candidate/night contributes nothing", not
    an uncaught exception — a malformed pulse is exactly the kind of night
    that must still produce a clean "nothing worth doing" record rather than
    crash the loop before the ledger gets written.

    `exchange_ok` is the caller's answer to "can shared/exchange.json be
    read?". False skips every candidate as `config_error`: without that file
    the Forge cannot tell which projects a change could break, and choosing
    anyway would mean verifying less than the ledger claims. This is an
    early exit, not the safety guarantee — `verify.run_checks` fails closed
    on the same condition with no default to weaken it, which is why the
    default here can safely be True.
    """
    strikes = strikes or {}
    recent_zones = list(recent_zones or [])

    raw_candidates = pulse.get("candidates") if isinstance(pulse, dict) else None
    if not isinstance(raw_candidates, list):
        raw_candidates = []

    skipped = {
        "outside_zone": 0,
        "struck_out": 0,
        "variety": 0,
        "below_floor": 0,
        "config_error": 0,
    }
    survivors: list[Scored] = []
    considered = 0

    for raw in raw_candidates:
        try:
            candidate = candidate_from_dict(raw)
        except (KeyError, TypeError):
            continue
        considered += 1

        if not exchange_ok:
            skipped["config_error"] += 1
            continue

        zone = zone_for(candidate.paths, config)
        if zone is None:
            skipped["outside_zone"] += 1
            continue
        if strikes.get(candidate.key(), 0) >= config.strike_limit:
            skipped["struck_out"] += 1
            continue
        if _blocked_by_variety(zone, recent_zones):
            skipped["variety"] += 1
            continue

        try:
            scored = score_one(candidate, zone, config)
        except KeyError:
            # A weight this candidate's kind needs is missing from
            # `config.weights` (a hand-edited forge.json, or a config built
            # without going through `load_config`'s defaults merge). One
            # unscoreable candidate must cost the night that candidate, not
            # the whole run.
            skipped["config_error"] += 1
            continue
        if scored.score < config.score_floor:
            skipped["below_floor"] += 1
            continue
        survivors.append(scored)

    # Highest score wins; ties break on task text so a rerun picks the same one.
    survivors.sort(key=lambda s: (-s.score, s.candidate.task))

    chosen = survivors[0] if survivors else None
    return {
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "considered": considered,
        "skipped": skipped,
        "chosen": scored_to_dict(chosen) if chosen else None,
        "runners_up": [
            {"task": s.candidate.task, "source": s.candidate.source, "score": s.score}
            for s in survivors[1 : 1 + RUNNERS_UP]
        ],
    }


def write_tonight(record: dict, root: Path, config: ForgeConfig) -> Path:
    d = config.state_dir(root)
    d.mkdir(parents=True, exist_ok=True)
    path = d / TONIGHT_FILENAME
    path.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return path


def read_tonight(root: Path, config: ForgeConfig) -> dict:
    """Read back the last decision. Missing, unreadable, or malformed is {}.

    Valid JSON that isn't a dict (a list, a number, ``null`` — e.g. a hand
    edited or truncated ``tonight.json``) is treated the same as no record at
    all, matching the guard `sense.read_pulse` already applies to its own
    file: every caller of this function expects to call `.get(...)` on the
    result.
    """
    path = config.state_dir(root) / TONIGHT_FILENAME
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError, ValueError):
        return {}
    return data if isinstance(data, dict) else {}
