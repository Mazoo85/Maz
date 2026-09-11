"""The whole cycle, in the order the spec draws it.

    SENSE -> DECIDE -> DO -> VERIFY -> LEARN

One ledger line is written no matter which branch of this function runs — a
crash, a quiet night and a merged PR all leave the same kind of record. That
invariant is what makes the ledger trustworthy six months from now, and
`_record` (at the bottom) is the only thing that writes it: every return path
above it funnels through the same call.

Preserving that means every call this function makes on the way to `_record`
has to be accounted for:

  - `sense`, `decide` and `do` are documented "never raises" by their own
    modules (every failure mode they have becomes a return value instead),
    so they are trusted as given.
  - `write_pulse` and `write_tonight` write scratch state — diagnostic aids
    for a human debugging a run after the fact, not the ledger itself. They
    carry no such guarantee (a disk-full or permission error is a plain
    `OSError` out of `Path.write_text`), so their calls are wrapped here:
    losing pulse.json must not cost the run its ledger line.
  - `open_draft_pr` calls the injected `poster`, an I/O boundary same as
    Crew or git — a real implementation hits the network. That call is
    wrapped too, and a raise there degrades to exactly the same "no PR"
    outcome as a poster that returns something falsy.
  - `_abandon` drives the injected `git` runner to clean up a failed
    attempt; if that runner blows up, the failed attempt is still worth
    recording, branch cleaned up or not, so the git calls are wrapped
    inside `_abandon` itself.
  - `quarantine`, `tick_roadmap` and `memory_note` are this module's own
    LEARN helpers and are individually documented never to raise (see
    learn.py) — no wrapping needed at the call site.

Every side effect is injectable (`git`, `crew`, `checks`, `poster`) so the
whole cycle is exercised offline by the test suite.
"""

from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path

from . import ledger as ledger_mod
from .config import load_config
from .decide import decide as decide_step
from .decide import write_tonight
from .do import do as do_step
from .gitops import checkout, delete_branch
from .learn import memory_note, quarantine, tick_roadmap
from .sense import sense as sense_step
from .sense import write_pulse
from .verify import open_draft_pr, run_checks


def _run_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%d")


def live_run(root: Path, collectors=None, git=None, crew=None, checks=None,
             poster=None, slug=None) -> dict:
    """Run one full cycle. Always returns the ledger entry it wrote."""
    root = Path(root)
    config = load_config(root)
    run_id = _run_id()

    # --- SENSE -------------------------------------------------------------
    pulse = sense_step(root, config, collectors=collectors)
    _best_effort(write_pulse, pulse, root, config)

    # --- DECIDE ------------------------------------------------------------
    strikes = ledger_mod.strikes(root, config)
    record = decide_step(pulse, config, strikes=strikes,
                         recent_zones=ledger_mod.recent_zones(root, config))
    _best_effort(write_tonight, record, root, config)

    why = {
        "considered": record.get("considered", 0),
        "skipped": record.get("skipped", {}),
        "runners_up": record.get("runners_up", []),
    }
    chosen = record.get("chosen")
    if not chosen:
        return _record(ledger_mod.new_entry(
            run_id, outcome="no_task", why=why,
            notes="nothing scored above the floor",
        ), root, config)

    cand = chosen["candidate"]
    why.update({"value": chosen["value"], "confidence": chosen["confidence"],
                "risk": chosen["risk"], "score": chosen["score"]})
    base = dict(
        chose=cand["task"], source=cand["source"], kind=cand["kind"],
        candidate_key=cand["key"], zone=chosen["zone"], why=why,
    )

    # --- DO ----------------------------------------------------------------
    outcome = do_step(chosen, root, config, git=git, crew=crew)
    started_branch = outcome.branch
    if not outcome.ok:
        _abandon(started_branch, root, git)
        entry = ledger_mod.new_entry(
            run_id, outcome="crew_failed", cost_usd=outcome.cost_usd,
            duration_min=outcome.duration_min, files_touched=len(outcome.files),
            notes=outcome.error, **base,
        )
        _maybe_quarantine(entry, strikes, root, config)
        return _record(entry, root, config)

    # --- VERIFY --------------------------------------------------------
    # `open_draft_pr` below must only ever be reachable once `result.ok` is
    # true: a red check run returns here, before any poster call is made, so
    # "red checks => no PR" holds by construction rather than by convention.
    result = run_checks(chosen["zone"], root, runner=checks)
    if not result.ok:
        _abandon(started_branch, root, git)
        entry = ledger_mod.new_entry(
            run_id, outcome="verify_failed", checks="red",
            cost_usd=outcome.cost_usd, duration_min=outcome.duration_min,
            files_touched=len(outcome.files),
            notes=f"checks failed: {result.output[-300:]}", **base,
        )
        _maybe_quarantine(entry, strikes, root, config)
        return _record(entry, root, config)

    try:
        pr = open_draft_pr(outcome, chosen, root, slug=slug, poster=poster)
        pr_error = ""
    except Exception as exc:  # noqa: BLE001 — a bad poster must not lose the ledger line
        pr, pr_error = None, str(exc)

    entry = ledger_mod.new_entry(
        run_id, outcome="pr_opened", checks="green", pr=pr,
        cost_usd=outcome.cost_usd, duration_min=outcome.duration_min,
        files_touched=len(outcome.files),
        notes=(f"draft PR opened on {outcome.branch}" if pr else
              f"branch {outcome.branch} is green but no PR could be opened"
              + (f": {pr_error}" if pr_error else "")),
        **base,
    )

    # --- LEARN -------------------------------------------------------------
    if cand["kind"] == "roadmap":
        tick_roadmap(cand["task"], root)
    return _record(entry, root, config)


def _best_effort(fn, *args) -> None:
    """Run a scratch-state write (pulse.json, tonight.json), swallowing any
    failure. These files are diagnostic aids for a human reading a run after
    the fact — not the ledger — so a disk-full or permission error writing
    either one must not cost the run the one artifact that actually matters:
    the ledger line `_record` writes below.
    """
    try:
        fn(*args)
    except Exception:  # noqa: BLE001 — best-effort by design, see docstring
        pass


def _abandon(branch: str | None, root: Path, git) -> None:
    """Leave the working tree where the night started.

    Best-effort: if the injected git runner blows up while cleaning up after
    a failed attempt, the failed attempt is still worth recording — branch
    cleaned up or not — so a crash here must not stop the caller from
    reaching `_record`.
    """
    if not branch:
        return
    try:
        checkout("main", root, runner=git)
        delete_branch(branch, root, runner=git)
    except Exception:  # noqa: BLE001 — cleanup failing is not this run's story
        pass


def _maybe_quarantine(entry: dict, strikes: dict, root: Path, config) -> None:
    """Third strike (this failure included) sends the candidate to stuck.md."""
    key = entry.get("candidate_key") or ""
    if not key:
        return
    if strikes.get(key, 0) + 1 >= config.strike_limit:
        quarantine(key, entry.get("chose", ""), entry.get("source", ""), root, config)


def _record(entry: dict, root: Path, config) -> dict:
    """The one thing that happens on every path: write the ledger line.

    `ledger.append` is deliberately NOT guarded here (see ledger.py) — a
    failure to persist the one artifact this system exists to produce must
    surface, not vanish. `memory_note` runs after it and is guarded on its
    own side (see learn.py): the ledger line above is already durable by the
    time it runs, so a memory-graph failure here costs the run nothing more.
    """
    ledger_mod.append(entry, root, config)
    memory_note(entry, root)
    return entry
