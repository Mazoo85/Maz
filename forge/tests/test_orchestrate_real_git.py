"""SLOW — real-git integration tests for the two lifecycle bugs the fast,
double-based tests in test_orchestrate.py cannot see.

Every git double in test_orchestrate.py answers `status --porcelain` with
silence no matter what the Forge just did, so a dirty tree left behind by
the Forge's own bookkeeping (Critical 1: the loop self-blocks after one
run) and a branch never returned to `main` after a successful run
(Critical 2: night 2's PR carries night 1's commits) are both invisible to
that file, however many of its tests pass.

These tests shell out to a real `git` binary against a throwaway repo built
fresh under a pytest `tmp_path`, with a real local *bare* remote to push
to — a filesystem path, so `git push` never touches the network. The `crew`
stub here makes a real commit via real git, the same shape a real Crew
subprocess run leaves behind. They are the only tests in this suite that
exercise `git status --porcelain` for real, and that realism is the whole
point: kept in their own file, clearly named and documented as slower (a
handful of subprocess calls per test — still comfortably under a second
each) so a reader does not mistake them for the fast doubles above, or
wonder why they don't just use FakeGit.

Never point any of this at a real working repository — every root here is
a `tmp_path` this test created and will discard.
"""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

from forge.config import ForgeConfig
from forge.gitops import current_branch, run_git
from forge.ledger import read_all
from forge.models import Candidate
from forge.orchestrate import live_run


def _git(args: list[str], cwd: Path) -> str:
    r = subprocess.run(["git", *args], cwd=str(cwd), capture_output=True, text=True)
    assert r.returncode == 0, f"git {args} failed: {r.stdout}{r.stderr}"
    return r.stdout


def _init_repo(root: Path, remote: Path) -> None:
    """A real repo with an initial commit and a real local bare remote."""
    root.mkdir(parents=True, exist_ok=True)
    remote.mkdir(parents=True, exist_ok=True)
    _git(["init", "-q", "-b", "main"], root)
    _git(["init", "-q", "--bare"], remote)
    _git(["config", "user.email", "forge-test@example.com"], root)
    _git(["config", "user.name", "Forge Test"], root)
    _git(["remote", "add", "origin", str(remote)], root)

    # Mirrors the real Maz repo's .gitignore for forge/state/ — the scratch
    # pulse.json/tonight.json writes must not be part of what these tests
    # are demonstrating; only the Forge's *tracked* bookkeeping (the ledger,
    # the memory note) is the point of Critical 1.
    (root / ".gitignore").write_text("forge/state/\n")
    (root / "docs").mkdir()
    (root / "docs" / "a.md").write_text("hello\n")
    (root / ".claude").mkdir()
    # `forge/` itself is already a tracked directory in the real Maz repo
    # (the Forge's own source lives there) — pre-creating it here matters:
    # `git status --porcelain` collapses a brand-new, wholly-untracked
    # directory into one line for the directory itself (`?? forge/`)
    # instead of listing what's inside it, which would make every path
    # under it look unmatched by any per-file ignore prefix. Tracking a
    # placeholder here first is what makes `forge/ledger/...` and
    # `forge/stuck.md` show up as their own porcelain lines later, matching
    # the real repo's shape.
    (root / "forge").mkdir()
    (root / "forge" / ".keep").write_text("")
    # One file per safe zone this fixture rotates through across nights
    # (see _ZONE_FILES) — decide.py deliberately refuses the same zone
    # three acting runs running (its "variety" rule, unrelated to either
    # lifecycle bug this file is about), so three consecutive real cycles
    # need three different zones to land on `pr_opened` each time. Not
    # `tests/`: that directory is this repo's real, unrelated C++ suite and
    # (deliberately, see config.ForgeConfig.safe_zones) is not a default
    # safe zone the Forge may work in — `shooter/` is used instead, purely
    # as a third zone with no checks of its own, same as `madlibs/`.
    (root / "shooter").mkdir()
    (root / "shooter" / "a.py").write_text("# placeholder\n")
    (root / "madlibs").mkdir()
    (root / "madlibs" / "a.txt").write_text("placeholder\n")
    # Present and tracked from the start, exactly like a real repo, so
    # learn.memory_note (which only writes when this file already exists)
    # actually appends to it during these runs — that append is one of the
    # two ways the Forge dirties its own tree (see FORGE_OWN_PATHS).
    (root / ".claude" / "codebase-memory.json").write_text("")
    # run_checks (via checks.all_commands) now reads shared/exchange.json
    # unconditionally, for every zone — an empty-but-valid declaration keeps
    # these tests exercising DO/VERIFY lifecycle behaviour rather than a
    # missing-file error unrelated to what each test is about.
    (root / "shared").mkdir()
    (root / "shared" / "exchange.json").write_text(
        '{"publishes": {}, "consumes": []}\n'
    )
    _git(["add", "-A"], root)
    _git(["commit", "-q", "-m", "initial"], root)
    _git(["push", "-q", "-u", "origin", "main"], root)


# One safe zone per night, so three consecutive nights don't trip decide.py's
# unrelated "variety" rule (never the same zone three acting runs running).
_ZONE_FILES = {1: "docs/a.md", 2: "shooter/a.py", 3: "madlibs/a.txt"}


def _collectors(n: int):
    path = _ZONE_FILES[n]

    def todo(root):
        return [Candidate(task=f"Improve the notes file {n}", source=f"todo:{path}:{n}",
                          kind="todo", paths=(path,), detail="recent")]
    return {"todo": todo}


def _crew_commits_for(n: int):
    """A crew stub that makes a real commit, the same shape a real `crew do
    --commit` subprocess run leaves behind — not a fake outcome object.

    Stages only the one file for its own zone (see _ZONE_FILES), not
    `git add -A`: a real Crew run is scoped to the one task it was handed,
    and never reaches into `forge/` (welded into every zone's no_touch
    list). Staging everything would sweep the Forge's own leftover,
    uncommitted bookkeeping — exempt from the *dirty-tree guard* by design,
    but never something Crew authors — into Crew's own commit, which the
    no-touch check downstream would then (rightly) reject as "touched a
    no-touch path". That is a fixture realism concern, not the bug under
    test here.
    """
    path = _ZONE_FILES[n]

    def crew(task: str, root, timeout_s: int):
        (Path(root) / path).write_text(f"{task}\n")
        _git(["add", path], root)
        _git(["commit", "-q", "-m", f"forge: {task[:60]}"], root)
        return (0, "done", None)

    return crew


def _run_cycle(root: Path, n: int, git=None) -> dict:
    return live_run(
        root,
        collectors=_collectors(n),
        git=git,
        crew=_crew_commits_for(n),
        checks=lambda cmd, r: (0, "ok"),
        poster=lambda path, body: {"number": 100 + n},
        slug="a/b",
    )


def _branch_from_notes(entry: dict) -> str:
    m = re.search(r"draft PR opened on (\S+)", entry["notes"])
    assert m, f"expected a 'draft PR opened on <branch>' note, got: {entry['notes']!r}"
    return m.group(1)


def test_three_consecutive_real_cycles_all_do_work(tmp_path):
    """The self-block, demonstrated: against a real repo, the Forge's own
    bookkeeping (the ledger line, the memory-graph note) must not leave the
    tree dirty in a way that blocks the *next* run's dirty-tree guard. All
    three nights must actually do work — none blocked as 'crew_failed'
    with a dirty working tree.
    """
    root = tmp_path / "repo"
    remote = tmp_path / "remote.git"
    _init_repo(root, remote)

    entries = [_run_cycle(root, n) for n in (1, 2, 3)]
    outcomes = [e["outcome"] for e in entries]
    notes = [e["notes"] for e in entries]
    assert outcomes == ["pr_opened", "pr_opened", "pr_opened"], list(zip(outcomes, notes))


def test_working_tree_is_back_on_main_after_a_successful_run(tmp_path):
    """Critical 2: a successful (`pr_opened`) run must leave the tree back
    on `main`, not sitting on the Forge branch it just opened a PR from.
    """
    root = tmp_path / "repo"
    remote = tmp_path / "remote.git"
    _init_repo(root, remote)

    entry = _run_cycle(root, 1)
    assert entry["outcome"] == "pr_opened"
    assert current_branch(root) == "main"


def test_night_two_branch_is_cut_from_main_not_from_night_ones_branch(tmp_path):
    """Critical 2's real failure mode: if night 1 leaves the tree on its own
    branch, night 2's `create_branch` cuts from *that* branch, so night 2's
    PR (whose base is `main`) ends up carrying night 1's unmerged commit
    too. Night 2's branch must contain only night 2's own commit against
    `main`, and night 1's branch must still exist (it has an open PR).
    """
    root = tmp_path / "repo"
    remote = tmp_path / "remote.git"
    _init_repo(root, remote)

    entry1 = _run_cycle(root, 1)
    entry2 = _run_cycle(root, 2)
    assert entry1["outcome"] == "pr_opened"
    assert entry2["outcome"] == "pr_opened"

    branch1 = _branch_from_notes(entry1)
    branch2 = _branch_from_notes(entry2)
    assert branch1 != branch2

    # Night 1's branch must not have been deleted — it has an open PR.
    existing = _git(["branch", "--list", branch1], root)
    assert branch1.rsplit("/", 1)[-1] in existing or branch1 in existing

    log = _git(["log", "--format=%s", f"main..{branch2}"], root)
    commits = [line for line in log.splitlines() if line.strip()]
    assert commits == [f"forge: Improve the notes file 2"], commits


def test_a_humans_uncommitted_edit_still_blocks_the_run(tmp_path):
    """The guard must not have become useless: a genuine uncommitted edit to
    a tracked file that is *not* one of the Forge's own bookkeeping paths
    must still refuse the run.
    """
    root = tmp_path / "repo"
    remote = tmp_path / "remote.git"
    _init_repo(root, remote)
    (root / "docs" / "a.md").write_text("a human's half-finished edit\n")

    entry = _run_cycle(root, 1)
    assert entry["outcome"] == "crew_failed"
    assert "dirty" in entry["notes"]
    assert current_branch(root) == "main"


def test_ledger_line_still_written_when_restore_to_main_fails(tmp_path):
    """A failure to check out back to `main` after a successful run must not
    cost the run its ledger line, and must say so in the notes so the
    operator knows the tree was left somewhere unexpected.
    """
    root = tmp_path / "repo"
    remote = tmp_path / "remote.git"
    _init_repo(root, remote)

    def flaky_restore(args):
        if args == ["checkout", "main"]:
            return (1, "", "simulated: could not check out main")
        return run_git(args, root)

    entry = _run_cycle(root, 1, git=flaky_restore)
    assert entry["outcome"] == "pr_opened"
    assert "main" in entry["notes"]
    assert len(read_all(root, ForgeConfig())) == 1
    # The simulated failure means the real checkout to main never ran.
    assert current_branch(root) != "main"
