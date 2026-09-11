"""The full live cycle, wired end to end with every side effect injected."""

import json

from forge.config import ForgeConfig
from forge.ledger import read_all
from forge.models import Candidate
from forge.orchestrate import _no_task_note, live_run


def _exchange(root):
    """Give a tmp_path repo the declaration run_checks now requires before
    it will run any commands at all, for any zone.
    """
    shared = root / "shared"
    shared.mkdir(parents=True, exist_ok=True)
    (shared / "exchange.json").write_text(
        json.dumps({"publishes": {}, "consumes": []}), encoding="utf-8"
    )
    return root


def _collectors():
    def todo(root):
        return [Candidate(task="Write the loot table docs", source="todo:docs/a.md:1",
                          kind="todo", paths=("docs/a.md",), detail="recent")]
    return {"todo": todo}


class FakeGit:
    def __init__(self, changed=("docs/a.md",)):
        self.calls = []
        self.changed = changed

    def __call__(self, args):
        self.calls.append(args)
        if args[:2] == ["rev-parse", "--abbrev-ref"]:
            return (0, "main\n", "")
        # do.head_sha() calls exactly this ("rev-parse HEAD", not
        # "--abbrev-ref") right after the branch is cut, and treats a blank
        # result as "could not read the branch's starting commit" — a
        # falsy-but-not-caught failure that would make every DO step in
        # every test below fail before Crew is ever invoked. It must be
        # distinguished from the --abbrev-ref case above and answered with a
        # non-empty SHA.
        if args == ["rev-parse", "HEAD"]:
            return (0, "deadbeef\n", "")
        if args[0] == "diff":
            return (0, "\n".join(self.changed) + "\n", "")
        return (0, "", "")


class FailingPushGit(FakeGit):
    """A git double whose push fails (a rejected ref, bad auth) without
    raising — the ordinary "git said no" shape push_branch itself reports as
    a plain ``False``, not an exception.
    """

    def __call__(self, args):
        if args[:1] == ["push"]:
            self.calls.append(args)
            return (1, "", "remote: permission denied")
        return super().__call__(args)


class RaisingPushGit(FakeGit):
    """A git double whose push blows up outright (the network dying mid-call)."""

    def __call__(self, args):
        if args[:1] == ["push"]:
            self.calls.append(args)
            raise RuntimeError("network exploded during push")
        return super().__call__(args)


class FailingCheckoutGit(FakeGit):
    """A git double whose checkout back to main *returns* nonzero rather than
    raising — main missing locally, say. Distinct from ExplodingCleanupGit,
    which raises; this one exercises the ordinary "git said no" path _abandon
    must also handle, by never attempting the delete afterwards.
    """

    def __call__(self, args):
        if args == ["checkout", "main"]:
            self.calls.append(args)
            return (1, "", "error: pathspec 'main' did not match any file(s) known to git")
        return super().__call__(args)


class ExplodingCleanupGit(FakeGit):
    """A git double whose cleanup calls (checkout / branch -D) blow up.

    Everything DO needs (rev-parse, diff) still behaves normally — only the
    calls _abandon makes after a failure are broken, to prove that a crash
    during cleanup can't cost the run its ledger line.
    """

    def __call__(self, args):
        if args[:1] == ["checkout"] or args[:2] == ["branch", "-D"]:
            self.calls.append(args)
            raise RuntimeError("git binary vanished mid-cleanup")
        return super().__call__(args)


def test_happy_path_opens_a_pr_and_records_it(tmp_path):
    _exchange(tmp_path)
    entry = live_run(
        tmp_path,
        collectors=_collectors(),
        git=FakeGit(),
        crew=lambda t, r, s: (0, "done", 0.75),
        checks=lambda cmd, root: (0, "ok"),
        poster=lambda path, body: {"number": 99},
        slug="a/b",
    )
    assert entry["outcome"] == "pr_opened"
    assert entry["pr"] == 99
    assert entry["zone"] == "docs/"
    assert entry["files_touched"] == 1
    assert read_all(tmp_path, ForgeConfig())[-1]["pr"] == 99


def test_nothing_to_do_records_no_task_and_never_branches(tmp_path):
    git = FakeGit()
    entry = live_run(tmp_path, collectors={}, git=git,
                     crew=lambda t, r, s: (0, "", 0.0),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "no_task"
    assert not any(a[:1] == ["checkout"] for a in git.calls)


def test_crew_failure_records_crew_failed_and_deletes_the_branch(tmp_path):
    _exchange(tmp_path)
    git = FakeGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (1, "boom", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "crew_failed"
    assert entry["pr"] is None
    assert any(a[:2] == ["branch", "-D"] for a in git.calls)


def test_over_budget_crew_records_budget_exceeded_not_crew_failed(tmp_path):
    """A run that comes in over budget_usd is a distinct outcome from an
    ordinary Crew failure: `budget_exceeded` is in OUTCOMES, in
    FAILURE_OUTCOMES and in docs/FORGE.md as something the Forge writes, but
    every producer used to return a falsy CrewOutcome indistinguishable from
    any other failure, so orchestrate always recorded `crew_failed` instead.
    """
    _exchange(tmp_path)
    git = FakeGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (0, "done", 999.0),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b",
                     )
    assert entry["outcome"] == "budget_exceeded"
    assert entry["pr"] is None
    assert any(a[:2] == ["branch", "-D"] for a in git.calls)


def test_failing_checks_record_verify_failed_and_open_no_pr(tmp_path):
    _exchange(tmp_path)
    posted = []
    entry = live_run(tmp_path, collectors={"todo": lambda root: [
        Candidate(task="Fix the scraper retry", source="todo:scraper/a.py:1",
                  kind="todo", paths=("scraper/a.py",), detail="recent")]},
        git=FakeGit(changed=("scraper/a.py",)),
        crew=lambda t, r, s: (0, "done", 0.2),
        checks=lambda cmd, root: (1, "1 failed"),
        poster=lambda p, b: posted.append(b) or {"number": 1}, slug="a/b")
    assert entry["outcome"] == "verify_failed"
    assert entry["pr"] is None
    assert posted == []


def test_third_strike_quarantines_the_candidate(tmp_path):
    _exchange(tmp_path)
    cfg = ForgeConfig()
    for _ in range(3):
        live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                 crew=lambda t, r, s: (1, "boom", 0.1),
                 checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    stuck = (tmp_path / "forge" / "stuck.md")
    assert stuck.exists()
    assert "loot table docs" in stuck.read_text()


def test_a_quarantined_candidate_is_not_picked_again(tmp_path):
    """Names its own wiring: three failures must actually produce
    forge/stuck.md naming the struck-out candidate — not merely make the
    candidate stop scoring, which decide.py's strike count would do on its
    own even if quarantine() were wired to nothing at all (see the mutation
    check on this test: monkeypatch quarantine to a no-op and it must fail).
    """
    _exchange(tmp_path)
    for _ in range(3):
        live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                 crew=lambda t, r, s: (1, "boom", 0.1),
                 checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")

    stuck = tmp_path / "forge" / "stuck.md"
    assert stuck.exists()
    text = stuck.read_text()
    assert "loot table docs" in text
    assert "todo:docs/a.md:1" in text

    entry = live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {"number": 5}, slug="a/b")
    assert entry["outcome"] == "no_task"


def test_two_failures_do_not_quarantine_yet(tmp_path):
    """The other side of the strike boundary: two failures must leave
    stuck.md unwritten and the candidate still selectable. Pinned separately
    from the third-strike test so a regression that quarantines early (on
    the second failure) fails loudly instead of shipping silently.
    """
    _exchange(tmp_path)
    for _ in range(2):
        live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                 crew=lambda t, r, s: (1, "boom", 0.1),
                 checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")

    stuck = tmp_path / "forge" / "stuck.md"
    assert not stuck.exists()

    entry = live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {"number": 7}, slug="a/b")
    assert entry["outcome"] == "pr_opened"
    assert entry["chose"] == "Write the loot table docs"


def test_every_run_writes_exactly_one_ledger_line(tmp_path):
    for _ in range(2):
        live_run(tmp_path, collectors={}, git=FakeGit(), crew=lambda t, r, s: (0, "", 0.0),
                 checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert len(read_all(tmp_path, ForgeConfig())) == 2


def test_a_raising_poster_still_records_the_ledger_line(tmp_path):
    """A crashing poster (a network error opening the PR, say) must not cost
    the run its ledger line — it must degrade to "pr_failed": checks were
    green and the branch pushed, but no PR exists. Recording this as
    "pr_opened" with pr=None would satisfy followup.pending()'s truthy-pr
    check and the branch would be silently orphaned on the remote forever.
    """
    _exchange(tmp_path)

    def boom(path, body):
        raise RuntimeError("network exploded")

    entry = live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"), poster=boom, slug="a/b")
    assert entry["outcome"] == "pr_failed"
    assert entry["pr"] is None
    assert read_all(tmp_path, ForgeConfig())[-1]["pr"] is None


def test_a_poster_returning_empty_dict_records_pr_failed_not_pr_opened(tmp_path):
    """github.api() returns {} on ANY failure, including a missing
    GITHUB_TOKEN — exactly the poster shape a live run hits most often. This
    must not be recorded as "pr_opened", the false-positive this fix exists
    to remove.
    """
    _exchange(tmp_path)
    entry = live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "pr_failed"
    assert entry["pr"] is None
    assert entry["checks"] == "green"


def test_a_broken_scratch_write_still_records_the_ledger_line(tmp_path, monkeypatch):
    """pulse.json is a diagnostic aid, not the ledger. A disk error writing it
    must not cost the run the one artifact that matters.
    """
    import forge.orchestrate as orchestrate

    def boom(*a, **k):
        raise OSError("disk full")

    monkeypatch.setattr(orchestrate, "write_pulse", boom)
    entry = live_run(tmp_path, collectors={}, git=FakeGit(),
                     crew=lambda t, r, s: (0, "", 0.0),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "no_task"
    assert len(read_all(tmp_path, ForgeConfig())) == 1


def test_a_crashing_cleanup_still_records_crew_failed(tmp_path):
    """A git runner that blows up while abandoning a failed branch must not
    cost the run its ledger line either — the failed attempt is still worth
    recording, branch cleaned up or not.
    """
    _exchange(tmp_path)
    git = ExplodingCleanupGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (1, "boom", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "crew_failed"
    assert any(a[:1] == ["checkout"] for a in git.calls)
    assert len(read_all(tmp_path, ForgeConfig())) == 1


def test_the_branch_is_pushed_before_the_pr_is_opened(tmp_path):
    """Pins call *ordering*, not just that both happen: a push recorded after
    the poster call would mean the PR was opened against a head ref that did
    not exist on the remote yet.
    """
    _exchange(tmp_path)
    events: list[tuple[str, object]] = []

    class OrderTrackingGit(FakeGit):
        def __call__(self, args):
            if args[:1] == ["push"]:
                events.append(("push", tuple(args)))
            return super().__call__(args)

    def poster(path, body):
        events.append(("poster", path))
        return {"number": 42}

    entry = live_run(tmp_path, collectors=_collectors(), git=OrderTrackingGit(),
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"), poster=poster, slug="a/b")
    assert entry["outcome"] == "pr_opened"
    assert entry["pr"] == 42
    kinds = [k for k, _ in events]
    assert kinds == ["push", "poster"], kinds


def test_a_failed_push_records_push_failed_opens_no_pr_and_abandons(tmp_path):
    _exchange(tmp_path)
    posted = []
    git = FailingPushGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"),
                     poster=lambda p, b: posted.append(b) or {"number": 1}, slug="a/b")
    assert entry["outcome"] == "push_failed"
    assert entry["pr"] is None
    assert posted == []
    assert any(a[:2] == ["branch", "-D"] for a in git.calls)


def test_a_raising_push_still_records_the_ledger_line(tmp_path):
    """A push that throws (the network dying mid-call) must degrade to
    "push_failed", exactly like one that returns False — not lose the night.
    """
    _exchange(tmp_path)
    git = RaisingPushGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"),
                     poster=lambda p, b: {"number": 1}, slug="a/b")
    assert entry["outcome"] == "push_failed"
    assert entry["pr"] is None
    assert len(read_all(tmp_path, ForgeConfig())) == 1


def test_push_branch_is_never_called_with_main(tmp_path):
    """The only call site for push_branch passes the Forge branch that DO
    just cut (always ``forge/YYYY-MM-DD-...``, see do.branch_name) — never
    the base branch this cycle must not touch.
    """
    _exchange(tmp_path)
    git = FakeGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"),
                     poster=lambda p, b: {"number": 1}, slug="a/b")
    assert entry["outcome"] == "pr_opened"
    push_calls = [a for a in git.calls if a[:1] == ["push"]]
    assert push_calls, "push_branch was never invoked"
    for args in push_calls:
        assert args[-1] != "main"
        assert args[-1].startswith("forge/")


_CONFIGURED_BASE = "claude/zomboid-sega-neon-anchorage-i5emkk"


def _write_base_branch_config(root):
    (root / "forge.json").write_text(json.dumps({"base_branch": _CONFIGURED_BASE}))


def test_abandon_checks_out_the_configured_base_branch_not_main(tmp_path):
    """A failed run's cleanup must check out `config.base_branch`, not the
    hard-coded "main" — a lingering constant here would move the working
    tree to a wholly different project on a repo like this one.
    """
    _write_base_branch_config(tmp_path)
    _exchange(tmp_path)
    git = FakeGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (1, "boom", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "crew_failed"
    # `["checkout", "-b", <branch>]` (cutting tonight's branch) also starts
    # with "checkout" — excluded here so this only pins the *return*
    # checkout `_abandon` makes, not the unrelated call DO makes earlier.
    checkout_calls = [a for a in git.calls if a[:1] == ["checkout"] and a[:2] != ["checkout", "-b"]]
    assert checkout_calls, "expected a checkout during cleanup"
    assert all(a == ["checkout", _CONFIGURED_BASE] for a in checkout_calls), checkout_calls
    assert not any(a == ["checkout", "main"] for a in git.calls)


def test_return_to_base_checks_out_the_configured_base_branch_not_main(tmp_path):
    """The tree left behind after a *successful* run must land on
    `config.base_branch` — this is the consequence that is live on every
    successful night: a hard-coded "main" here moves the working tree to a
    different project before tomorrow's branch is even cut.
    """
    _write_base_branch_config(tmp_path)
    _exchange(tmp_path)
    git = FakeGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"),
                     poster=lambda p, b: {"number": 3}, slug="a/b")
    assert entry["outcome"] == "pr_opened"
    checkout_calls = [a for a in git.calls if a[:1] == ["checkout"] and a[:2] != ["checkout", "-b"]]
    assert checkout_calls, "expected a checkout back to base after success"
    assert all(a == ["checkout", _CONFIGURED_BASE] for a in checkout_calls), checkout_calls
    assert not any(a == ["checkout", "main"] for a in git.calls)


def test_live_run_opens_the_pr_against_the_configured_base_branch(tmp_path):
    _write_base_branch_config(tmp_path)
    _exchange(tmp_path)
    posted = {}
    git = FakeGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"),
                     poster=lambda p, b: posted.update(b) or {"number": 9}, slug="a/b")
    assert entry["outcome"] == "pr_opened"
    assert posted["base"] == _CONFIGURED_BASE


# --- Important 3: VERIFY must derive checks from what Crew actually -------
# --- changed, not from the zone DECIDE chose before Crew ran. -------------


def test_verify_runs_the_actually_changed_zones_checks_not_just_the_chosen_zone(tmp_path):
    """The finding-3 reproduction: DECIDE scopes the candidate to docs/ (the
    broadest zone spanning the eventual change, and the one recorded as
    `zone` in the ledger), but Crew's actual edit also reaches music/. The
    music edit must still be checked — recording `checks: green` without
    running a single command against it is the defect this branch exists to
    remove.
    """
    _exchange(tmp_path)
    checks_run = []
    entry = live_run(
        tmp_path,
        collectors={"todo": lambda root: [
            Candidate(task="Note the architecture doc", source="todo:docs/ARCHITECTURE.md:1",
                      kind="todo", paths=("docs/ARCHITECTURE.md",), detail="recent")]},
        git=FakeGit(changed=("docs/ARCHITECTURE.md", "music/js/composer.js")),
        crew=lambda t, r, s: (0, "done", 0.1),
        checks=lambda cmd, root: (checks_run.append(cmd), (0, "ok"))[1],
        poster=lambda p, b: {"number": 1}, slug="a/b",
    )
    assert entry["outcome"] == "pr_opened"
    assert entry["zone"] == "docs/"  # DECIDE's chosen zone is still recorded as-is
    assert ("node", "music/tests/music-logic.test.js") in checks_run, (
        f"music/ was touched but never checked; ran: {checks_run}"
    )


def test_verify_fails_closed_when_the_actual_change_cannot_be_checked(tmp_path):
    """A red music check on a change DECIDE scoped to docs/ must still fail
    the run — the fix must not accidentally make VERIFY more lenient than
    before, only more honest about what it covers.
    """
    _exchange(tmp_path)
    entry = live_run(
        tmp_path,
        collectors={"todo": lambda root: [
            Candidate(task="Note the architecture doc", source="todo:docs/ARCHITECTURE.md:1",
                      kind="todo", paths=("docs/ARCHITECTURE.md",), detail="recent")]},
        git=FakeGit(changed=("docs/ARCHITECTURE.md", "music/js/composer.js")),
        crew=lambda t, r, s: (0, "done", 0.1),
        checks=lambda cmd, root: (1, "music test failed"),
        poster=lambda p, b: {"number": 1}, slug="a/b",
    )
    assert entry["outcome"] == "verify_failed"
    assert entry["pr"] is None


# --- Important 4: a no_task night must say which skip reason dominated ----


def test_no_task_note_names_the_dominant_skip_reason():
    why = {"considered": 97, "skipped": {
        "outside_zone": 0, "struck_out": 0, "variety": 0,
        "below_floor": 0, "config_error": 97, "unscoreable": 0,
    }}
    note = _no_task_note(why)
    assert "config_error" in note or "exchange.json" in note
    assert "below the score floor" not in note.lower()
    assert "97" in note


def test_no_task_note_still_reports_below_floor_when_that_is_what_happened():
    why = {"considered": 3, "skipped": {
        "outside_zone": 0, "struck_out": 0, "variety": 0,
        "below_floor": 3, "config_error": 0, "unscoreable": 0,
    }}
    note = _no_task_note(why)
    assert "below the score floor" in note.lower()


def test_no_task_note_handles_nothing_considered_at_all():
    why = {"considered": 0, "skipped": {}}
    note = _no_task_note(why)
    assert note


def test_a_broken_exchange_records_a_no_task_note_naming_config_error(tmp_path):
    """Real end-to-end reproduction of finding 4: a malformed
    shared/exchange.json must not leave the ledger reading as though 97
    candidates were scored and found wanting.
    """
    shared = tmp_path / "shared"
    shared.mkdir(parents=True)
    (shared / "exchange.json").write_text("{ not json", encoding="utf-8")
    entry = live_run(
        tmp_path,
        collectors=_collectors(),
        git=FakeGit(),
        crew=lambda t, r, s: (0, "done", 0.1),
        checks=lambda cmd, root: (0, "ok"),
        poster=lambda p, b: {"number": 1}, slug="a/b",
    )
    assert entry["outcome"] == "no_task"
    assert entry["why"]["skipped"]["config_error"] == entry["why"]["considered"]
    assert entry["why"]["skipped"]["below_floor"] == 0
    assert "nothing scored above the floor" not in entry["notes"]


def test_abandon_with_a_failing_checkout_does_not_delete_and_still_records(tmp_path):
    """When checkout back to main *returns* failure (rather than raising),
    the delete must not be attempted — git refuses to delete the branch
    that's still checked out, and the run must still record.
    """
    _exchange(tmp_path)
    git = FailingCheckoutGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (1, "boom", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "crew_failed"
    assert not any(a[:2] == ["branch", "-D"] for a in git.calls)
    assert len(read_all(tmp_path, ForgeConfig())) == 1
