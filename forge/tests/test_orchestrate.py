"""The full live cycle, wired end to end with every side effect injected."""

from forge.config import ForgeConfig
from forge.ledger import read_all
from forge.models import Candidate
from forge.orchestrate import live_run


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
    git = FakeGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (1, "boom", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "crew_failed"
    assert entry["pr"] is None
    assert any(a[:2] == ["branch", "-D"] for a in git.calls)


def test_failing_checks_record_verify_failed_and_open_no_pr(tmp_path):
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
    cfg = ForgeConfig()
    for _ in range(3):
        live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                 crew=lambda t, r, s: (1, "boom", 0.1),
                 checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    stuck = (tmp_path / "forge" / "stuck.md")
    assert stuck.exists()
    assert "loot table docs" in stuck.read_text()


def test_a_quarantined_candidate_is_not_picked_again(tmp_path):
    for _ in range(3):
        live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                 crew=lambda t, r, s: (1, "boom", 0.1),
                 checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    entry = live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {"number": 5}, slug="a/b")
    assert entry["outcome"] == "no_task"


def test_every_run_writes_exactly_one_ledger_line(tmp_path):
    for _ in range(2):
        live_run(tmp_path, collectors={}, git=FakeGit(), crew=lambda t, r, s: (0, "", 0.0),
                 checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert len(read_all(tmp_path, ForgeConfig())) == 2


def test_a_raising_poster_still_records_the_ledger_line(tmp_path):
    """A crashing poster (a network error opening the PR, say) must not cost
    the run its ledger line — it must degrade to "no PR", the same as a
    poster that returns something falsy.
    """
    def boom(path, body):
        raise RuntimeError("network exploded")

    entry = live_run(tmp_path, collectors=_collectors(), git=FakeGit(),
                     crew=lambda t, r, s: (0, "done", 0.1),
                     checks=lambda cmd, root: (0, "ok"), poster=boom, slug="a/b")
    assert entry["outcome"] == "pr_opened"
    assert entry["pr"] is None
    assert read_all(tmp_path, ForgeConfig())[-1]["pr"] is None


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
    git = ExplodingCleanupGit()
    entry = live_run(tmp_path, collectors=_collectors(), git=git,
                     crew=lambda t, r, s: (1, "boom", 0.1),
                     checks=lambda cmd, root: (0, ""), poster=lambda p, b: {}, slug="a/b")
    assert entry["outcome"] == "crew_failed"
    assert any(a[:1] == ["checkout"] for a in git.calls)
    assert len(read_all(tmp_path, ForgeConfig())) == 1
