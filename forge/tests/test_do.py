"""DO: make a branch, hand one task to Crew, report what happened."""

import pytest

from forge import do as do_module
from forge.config import ForgeConfig
from forge.do import CrewOutcome, branch_name, do
from forge.gitops import current_branch

# The SHA FakeGit answers `rev-parse HEAD` with — the base the leash re-check
# must diff against, recorded before Crew ever runs.
BASE_SHA = "base-sha-0000"


class FakeGit:
    """Records git calls and answers them from a script."""

    def __init__(self, changed=("docs/a.md",)):
        self.calls = []
        self.changed = changed

    def __call__(self, args):
        self.calls.append(args)
        if args[:2] == ["rev-parse", "--abbrev-ref"]:
            return (0, "main\n", "")
        if args == ["rev-parse", "HEAD"]:
            return (0, f"{BASE_SHA}\n", "")
        if args[0] == "diff":
            return (0, "\n".join(self.changed) + "\n", "")
        return (0, "", "")


CHOSEN = {
    "candidate": {"task": "Write the loot table docs", "source": "todo:docs/a.md:1",
                  "kind": "todo", "paths": ["docs/a.md"], "current_milestone": False,
                  "tests_nearby": False, "detail": "recent", "key": "abc123"},
    "value": 5.0, "confidence": 7.0, "risk": 1.0, "score": 17.5, "zone": "docs/",
}


def test_branch_name_is_dated_and_slugged():
    import datetime
    name = branch_name("Fix the failing Music CI workflow!", when=datetime.date(2026, 9, 11))
    assert name.startswith("forge/2026-09-11-")
    assert " " not in name and "!" not in name
    assert len(name) < 70


def test_do_creates_a_branch_then_runs_crew(tmp_path):
    git = FakeGit()
    seen = {}

    def crew(task, root, timeout_s):
        seen["task"] = task
        seen["timeout_s"] = timeout_s
        return (0, "crew finished", 1.25)

    out = do(CHOSEN, tmp_path, ForgeConfig(), git=git, crew=crew)
    assert out.ok is True
    assert out.branch.startswith("forge/")
    assert out.files == ("docs/a.md",)
    assert out.cost_usd == 1.25
    assert seen["task"] == "Write the loot table docs"
    assert seen["timeout_s"] == 45 * 60
    assert ["checkout", "-b", out.branch] in git.calls


def test_crew_failure_is_reported_not_raised(tmp_path):
    def crew(task, root, timeout_s):
        return (1, "crew exploded", 0.4)

    out = do(CHOSEN, tmp_path, ForgeConfig(), git=FakeGit(), crew=crew)
    assert out.ok is False
    assert "exploded" in out.error


def test_crew_timeout_is_reported(tmp_path):
    import subprocess

    def crew(task, root, timeout_s):
        raise subprocess.TimeoutExpired(cmd="crew", timeout=timeout_s)

    out = do(CHOSEN, tmp_path, ForgeConfig(crew_timeout_min=7), git=FakeGit(), crew=crew)
    assert out.ok is False
    # The timeout branch names the configured leash, in minutes — that is
    # what only it can produce. The generic crash handler's message also
    # happens to contain the words "timed out" (from TimeoutExpired's own
    # __str__), so asserting on that phrase alone can't tell the branches
    # apart. A distinctive, non-default minute count can.
    assert "7 minutes" in out.error


def test_a_non_timeout_crash_is_reported_as_a_crash_not_a_timeout(tmp_path):
    def crew(task, root, timeout_s):
        raise RuntimeError("boom")

    out = do(CHOSEN, tmp_path, ForgeConfig(crew_timeout_min=7), git=FakeGit(), crew=crew)
    assert out.ok is False
    assert "crashed" in out.error.lower()
    assert "boom" in out.error
    assert "timed out" not in out.error.lower()
    assert "7 minutes" not in out.error


def test_too_many_files_touched_fails_the_run(tmp_path):
    many = tuple(f"docs/f{i}.md" for i in range(30))
    out = do(CHOSEN, tmp_path, ForgeConfig(max_files_touched=12),
             git=FakeGit(changed=many), crew=lambda t, r, s: (0, "ok", 0.1))
    assert out.ok is False
    assert "files" in out.error.lower()


def test_a_file_outside_the_safe_zone_fails_the_run(tmp_path):
    out = do(CHOSEN, tmp_path, ForgeConfig(),
             git=FakeGit(changed=("docs/a.md", "engine/src/core/app.cpp")),
             crew=lambda t, r, s: (0, "ok", 0.1))
    assert out.ok is False
    assert "outside" in out.error.lower()


def test_a_no_touch_file_fails_the_run_even_inside_a_safe_zone(tmp_path):
    out = do(CHOSEN, tmp_path, ForgeConfig(),
             git=FakeGit(changed=("docs/a.md", "forge/forge/decide.py")),
             crew=lambda t, r, s: (0, "ok", 0.1))
    assert out.ok is False
    assert "no-touch" in out.error.lower()


def test_budget_overrun_fails_the_run(tmp_path):
    out = do(CHOSEN, tmp_path, ForgeConfig(budget_usd=1.0),
             git=FakeGit(), crew=lambda t, r, s: (0, "ok", 9.99))
    assert out.ok is False
    assert "budget" in out.error.lower()


def test_branch_creation_failure_stops_before_crew_runs(tmp_path):
    class FailingCheckoutGit(FakeGit):
        """Answers everything like FakeGit, except `checkout -b` fails."""

        def __call__(self, args):
            self.calls.append(args)
            if args[:2] == ["checkout", "-b"]:
                return (1, "", "fatal: a branch named forge/... already exists")
            if args[:2] == ["rev-parse", "--abbrev-ref"]:
                return (0, "main\n", "")
            if args[0] == "diff":
                return (0, "\n".join(self.changed) + "\n", "")
            return (0, "", "")

    crew_called = {"value": False}

    def crew(task, root, timeout_s):
        crew_called["value"] = True
        return (0, "crew finished", 0.0)

    out = do(CHOSEN, tmp_path, ForgeConfig(), git=FailingCheckoutGit(), crew=crew)
    assert out.ok is False
    assert "branch" in out.error.lower()
    assert crew_called["value"] is False, "crew must never run if the branch could not be created"


def test_current_branch_uses_the_runner():
    assert current_branch(None, runner=lambda a: (0, "main\n", "")) == "main"


# --- Fix 1: the leash re-check must diff against the recorded base SHA, ---
# --- not HEAD~1, which silently trusts Crew to have made exactly one    ---
# --- commit. ---------------------------------------------------------------


class MultiCommitGit(FakeGit):
    """Three commits since the branch was cut: the first touches a
    no-touch-shaped path, the last two touch docs/.

    ``git diff --name-only HEAD~1`` (the old, wrong base) sees only the
    *last* commit's file — a clean-looking subset that would pass every
    leash check. Diffing from the recorded base SHA sees the full set,
    including the no-touch violation buried in the first commit. That
    contrast is the point of this fixture.
    """

    def __call__(self, args):
        self.calls.append(args)
        if args[:2] == ["rev-parse", "--abbrev-ref"]:
            return (0, "main\n", "")
        if args == ["rev-parse", "HEAD"]:
            return (0, f"{BASE_SHA}\n", "")
        if args == ["diff", "--name-only", BASE_SHA]:
            return (0, "forge/forge/decide.py\ndocs/b.md\ndocs/c.md\n", "")
        if args == ["diff", "--name-only", "HEAD~1"]:
            # What the old, wrong implementation would have seen: only the
            # last commit's file, no-touch violation invisible.
            return (0, "docs/c.md\n", "")
        return (0, "", "")


def test_multi_commit_no_touch_violation_is_not_invisible(tmp_path):
    out = do(CHOSEN, tmp_path, ForgeConfig(), git=MultiCommitGit(),
             crew=lambda t, r, s: (0, "ok", 0.1))
    assert out.ok is False
    assert "no-touch" in out.error.lower()
    assert "forge/forge/decide.py" in out.files


class ZeroCommitGit(FakeGit):
    """Crew made no commits at all. Diffing from the recorded base SHA (which
    equals current HEAD) must report no files — not the files of whatever
    commit predates the branch, which is what ``HEAD~1`` would fabricate.
    """

    def __call__(self, args):
        self.calls.append(args)
        if args[:2] == ["rev-parse", "--abbrev-ref"]:
            return (0, "main\n", "")
        if args == ["rev-parse", "HEAD"]:
            return (0, f"{BASE_SHA}\n", "")
        if args == ["diff", "--name-only", BASE_SHA]:
            return (0, "", "")
        if args == ["diff", "--name-only", "HEAD~1"]:
            return (0, "docs/pre-existing.md\n", "")
        return (0, "", "")


def test_zero_commit_run_reports_no_files_not_a_previous_commits(tmp_path):
    out = do(CHOSEN, tmp_path, ForgeConfig(), git=ZeroCommitGit(),
             crew=lambda t, r, s: (0, "ok", 0.1))
    assert out.files == ()
    assert "docs/pre-existing.md" not in out.files


def test_base_sha_is_read_before_crew_is_invoked(tmp_path):
    order = []

    class OrderTrackingGit(FakeGit):
        def __call__(self, args):
            if args == ["rev-parse", "HEAD"]:
                order.append("head_sha")
            return super().__call__(args)

    def crew(task, root, timeout_s):
        order.append("crew")
        return (0, "ok", 0.1)

    do(CHOSEN, tmp_path, ForgeConfig(), git=OrderTrackingGit(), crew=crew)
    assert order == ["head_sha", "crew"], "base SHA must be recorded before Crew runs"


def test_unreadable_base_sha_fails_without_invoking_crew(tmp_path):
    class BadShaGit(FakeGit):
        def __call__(self, args):
            self.calls.append(args)
            if args == ["rev-parse", "HEAD"]:
                return (1, "", "fatal: ambiguous argument 'HEAD'")
            if args[:2] == ["rev-parse", "--abbrev-ref"]:
                return (0, "main\n", "")
            return (0, "", "")

    crew_called = {"value": False}

    def crew(task, root, timeout_s):
        crew_called["value"] = True
        return (0, "ok", 0.1)

    out = do(CHOSEN, tmp_path, ForgeConfig(), git=BadShaGit(), crew=crew)
    assert out.ok is False
    assert crew_called["value"] is False, "crew must never run without a recorded base SHA"


# --- Fix 2: an unmeasured cost must read as unknown, never as free. --------


def test_default_crew_reports_cost_as_unknown_not_zero(tmp_path, monkeypatch):
    """`_default_crew` (the bundled, non-injected runner) has no way to learn
    Crew's spend, so it must say so (`None`) rather than claim $0.00 — a
    real number a ledger reader or a human auditor would take at face value.
    """

    class FakeCompletedProcess:
        returncode = 0
        stdout = "crew finished"
        stderr = ""

    monkeypatch.setattr(do_module.subprocess, "run",
                        lambda *a, **k: FakeCompletedProcess())

    out = do(CHOSEN, tmp_path, ForgeConfig(), git=FakeGit())
    assert out.cost_usd is None


def test_unknown_cost_does_not_trip_the_budget_check(tmp_path):
    out = do(CHOSEN, tmp_path, ForgeConfig(budget_usd=0.01), git=FakeGit(),
             crew=lambda t, r, s: (0, "ok", None))
    assert out.cost_usd is None
    assert out.ok is True, "an unmeasured cost must not silently fail the budget check"


def test_a_known_cost_over_budget_still_fails(tmp_path):
    out = do(CHOSEN, tmp_path, ForgeConfig(budget_usd=1.0), git=FakeGit(),
             crew=lambda t, r, s: (0, "ok", 9.99))
    assert out.ok is False
    assert out.cost_usd == 9.99
    assert "budget" in out.error.lower()


# --- Fix 3: create_branch must never raise out of do(). --------------------


def test_create_branch_crash_is_reported_not_raised(tmp_path):
    def raising_git(args):
        raise FileNotFoundError("git: command not found")

    crew_called = {"value": False}

    def crew(task, root, timeout_s):
        crew_called["value"] = True
        return (0, "ok", 0.1)

    out = do(CHOSEN, tmp_path, ForgeConfig(), git=raising_git, crew=crew)
    assert out.ok is False
    assert "branch" in out.error.lower()
    assert crew_called["value"] is False
