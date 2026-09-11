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
  - `push_branch` drives the injected `git` runner over the network same as
    Crew or the poster below. It runs after checks are green and before the
    branch is offered to anyone as a PR: a real GitHub head ref has to exist
    before `open_draft_pr` can name it, so a push that fails or raises is
    treated as this run's outcome (`push_failed`) — the branch is abandoned
    exactly as any other failed attempt is, and `open_draft_pr` is never
    reached on that path.
  - `open_draft_pr` calls the injected `poster`, an I/O boundary same as
    Crew or git — a real implementation hits the network. That call is
    wrapped too, and a raise there degrades to exactly the same "pr_failed"
    outcome as a poster that returns something falsy (`github.api()`'s
    documented shape for any failure, a missing `GITHUB_TOKEN` included) —
    never "pr_opened", which would lie about a PR existing.
  - `_abandon` drives the injected `git` runner to clean up a failed
    attempt; if that runner blows up, the failed attempt is still worth
    recording, branch cleaned up or not, so the git calls are wrapped
    inside `_abandon` itself.
  - `_return_to_base` runs on the one path `_abandon` does not cover: a
    successful night. Every run — failed or not — must leave the tree on
    `config.base_branch` so tomorrow's DO cuts its branch from the same
    known point tonight started from, rather than from tonight's own branch
    (see the module docstring's "branch stacking" incident). Unlike `_abandon`
    it must not delete the branch — a successful branch carries an open PR
    — so it is its own function, not a flag on `_abandon`. It is guarded
    the same way: a checkout failure here is folded into the entry's notes,
    never raised, so a tree left somewhere unexpected is visible to the
    operator without costing the run its ledger line.
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
from .exchange import is_loadable
from .gitops import checkout, delete_branch, push_branch
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
                         recent_zones=ledger_mod.recent_zones(root, config),
                         exchange_ok=is_loadable(root))
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
        cleanup_note = _abandon(started_branch, root, git, config.base_branch)
        # `failure_kind` is do()'s explicit, typed signal for "this failure
        # needs its own ledger outcome" — read that field rather than
        # pattern-matching `outcome.error`'s free text, which is written for
        # a human and must stay free to reword without silently breaking
        # this dispatch. Every ordinary DO failure leaves it None and lands
        # on "crew_failed", same as before this field existed.
        do_outcome = outcome.failure_kind or "crew_failed"
        entry = ledger_mod.new_entry(
            run_id, outcome=do_outcome, cost_usd=outcome.cost_usd,
            duration_min=outcome.duration_min, files_touched=len(outcome.files),
            notes=_with_cleanup_note(outcome.error, cleanup_note), **base,
        )
        _maybe_quarantine(entry, strikes, root, config)
        return _record(entry, root, config)

    # --- VERIFY --------------------------------------------------------
    # `open_draft_pr` below must only ever be reachable once `result.ok` is
    # true: a red check run returns here, before any poster call is made, so
    # "red checks => no PR" holds by construction rather than by convention.
    result = run_checks(chosen["zone"], root, runner=checks)
    if not result.ok:
        cleanup_note = _abandon(started_branch, root, git, config.base_branch)
        entry = ledger_mod.new_entry(
            run_id, outcome="verify_failed", checks="red",
            cost_usd=outcome.cost_usd, duration_min=outcome.duration_min,
            files_touched=len(outcome.files),
            notes=_with_cleanup_note(f"checks failed: {result.output[-300:]}", cleanup_note),
            **base,
        )
        _maybe_quarantine(entry, strikes, root, config)
        return _record(entry, root, config)

    # The branch only ever existed locally up to this point. It has to reach
    # the remote before `open_draft_pr` can name it as a PR's `head` — a real
    # GitHub API call against a ref that isn't there fails, and `github.api`
    # swallows that failure into `{}`, so without this step every green
    # night would silently record `pr: None` with no PR ever opened. A push
    # that fails or raises is this run's failure, recorded as such, with the
    # branch abandoned exactly like any other failed attempt — `open_draft_pr`
    # (and therefore the injected `poster`) must never be reached on this path.
    try:
        pushed = push_branch(started_branch, root, runner=git)
    except Exception as exc:  # noqa: BLE001 — a raise here must not lose the ledger line
        pushed, push_error = False, str(exc)
    else:
        push_error = ""
    if not pushed:
        cleanup_note = _abandon(started_branch, root, git, config.base_branch)
        entry = ledger_mod.new_entry(
            run_id, outcome="push_failed", checks="green",
            cost_usd=outcome.cost_usd, duration_min=outcome.duration_min,
            files_touched=len(outcome.files),
            notes=_with_cleanup_note(
                f"could not push {outcome.branch}" + (f": {push_error}" if push_error else ""),
                cleanup_note),
            **base,
        )
        _maybe_quarantine(entry, strikes, root, config)
        return _record(entry, root, config)

    try:
        pr = open_draft_pr(outcome, chosen, root, slug=slug, poster=poster,
                           base_branch=config.base_branch)
        pr_error = ""
    except Exception as exc:  # noqa: BLE001 — a bad poster must not lose the ledger line
        pr, pr_error = None, str(exc)

    # `github.api()` returns `{}` on ANY failure — a missing GITHUB_TOKEN
    # included — so a falsy `pr` here is not a rare shape, and recording it
    # as "pr_opened" (the old behaviour) actively lied: `docs/FORGE.md`
    # defines "pr_opened" as "a draft PR was opened", `followup.pending()`
    # requires a truthy `pr` before revisiting a run, and `strikes()` treats
    # the literal outcome "pr_opened" as a reset — all three would silently
    # accept a pushed-but-orphaned branch as a success. "pr_failed" is the
    # honest outcome for "checks were green and the branch reached the
    # remote, but no PR exists" — see ledger.OUTCOMES for why it counts as
    # ACTING (real work landed) but not a FAILURE (an environment fault,
    # not the candidate's).
    outcome_name = "pr_opened" if pr else "pr_failed"

    # The branch just did real, checks-passed work — whether or not the PR
    # call itself succeeded — so it must never be deleted like a failed
    # attempt's branch is by `_abandon`. The tree still has to come back to
    # config.base_branch so tomorrow's DO cuts its own branch from the same
    # known point tonight started from, not from tonight's branch (see the
    # module docstring's "branch stacking" incident). A failure to restore
    # is folded into this entry's notes rather than raised, exactly like
    # every other git call on this path.
    restore_note = _return_to_base(started_branch, root, git, config.base_branch)

    entry = ledger_mod.new_entry(
        run_id, outcome=outcome_name, checks="green", pr=pr,
        cost_usd=outcome.cost_usd, duration_min=outcome.duration_min,
        files_touched=len(outcome.files),
        notes=_with_cleanup_note(
            f"draft PR opened on {outcome.branch}" if pr else
            f"branch {outcome.branch} is green and pushed but no PR could be opened"
            + (f": {pr_error}" if pr_error else ""),
            restore_note),
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


def _abandon(branch: str | None, root: Path, git, base_branch: str) -> str:
    """Leave the working tree where the night started.

    `base_branch` comes from the caller's loaded `ForgeConfig`
    (`config.base_branch`) rather than a module-wide constant: this
    function has no config of its own to fall back on, on purpose — a
    silent fallback here is exactly the bug that shipped this repo's PRs
    against the wrong project line (see verify.py's module docstring for
    the incident).

    Best-effort: if the injected git runner blows up while cleaning up after
    a failed attempt, the failed attempt is still worth recording — branch
    cleaned up or not — so a crash here must not stop the caller from
    reaching `_record`.

    The delete is only attempted once the checkout back to `base_branch`
    actually succeeds. `delete_branch` runs `git branch -D <branch>`, which
    git refuses on the branch that's currently checked out — so calling it
    after a checkout that merely *returned* failure (the base branch
    missing locally, any ordinary git error; not the exception case above)
    would either no-op or, worse, silently leave the tree sitting on the
    Forge branch. Returns a human-readable note when that happens, so the
    caller can fold it into the ledger entry rather than let the tree's
    real state go unrecorded; returns "" when cleanup is not needed or
    succeeds.
    """
    if not branch:
        return ""
    try:
        if not checkout(base_branch, root, runner=git):
            return (f"could not clean up: checkout to {base_branch} failed; "
                    f"working tree left on {branch}")
        delete_branch(branch, root, runner=git)
    except Exception:  # noqa: BLE001 — cleanup failing is not this run's story
        pass
    return ""


def _return_to_base(branch: str, root: Path, git, base_branch: str) -> str:
    """Leave the working tree on `base_branch` after a *successful* run.

    `base_branch` is the caller's `config.base_branch`, for the same reason
    given in `_abandon` above: this function must never quietly substitute
    its own idea of "the base branch" when a real config was available to
    ask.

    The counterpart to `_abandon` above, for the one outcome it does not
    cover: `pr_opened`. The branch just earned an open PR, so — unlike
    `_abandon` — this never deletes it; only the checkout back to
    `base_branch` happens here. Skipping this step entirely is the bug this
    function exists to fix: without it, the tree stays on tonight's branch,
    and tomorrow's `create_branch` cuts tomorrow's branch from tonight's
    work (plus anything a human commits in between) instead of from a known
    base — so a PR whose declared `base` is `base_branch` ends up carrying
    commits that were never tonight's to offer.

    Best-effort, exactly like `_abandon`: a checkout that fails or raises
    must not cost this run its ledger line — `_record` still has to run —
    so a human-readable note is returned instead, for the caller to fold
    into the entry. The operator needs to know the tree was left on the
    Forge branch rather than assume a clean handoff to `base_branch`.
    """
    try:
        if not checkout(base_branch, root, runner=git):
            return (f"could not return to {base_branch} after opening the PR; "
                    f"working tree left on {branch}")
    except Exception as exc:  # noqa: BLE001 — a crash here must not lose the ledger line
        return (f"could not return to {base_branch} after opening the PR: {exc}; "
                f"working tree left on {branch}")
    return ""


def _with_cleanup_note(note: str, cleanup_note: str) -> str:
    """Append `_abandon`'s cleanup warning to an outcome's note, if any."""
    return f"{note}; {cleanup_note}" if cleanup_note else note


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
