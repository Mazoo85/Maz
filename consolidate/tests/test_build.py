"""End-to-end tests against real git repositories.

These are the tests that matter most: the whole promise of the tool is that
consolidating loses nothing, and the only way to know that is to build a real
consolidated repo and go looking for the commits afterwards.
"""

import pytest

from conftest import PADDING, commit, git, make_repo, needs_git
from consolidate import build as build_mod
from consolidate import gitops, layout, overlap
from consolidate.models import SourceRepo
from consolidate.plan import build_plan

pytestmark = needs_git


@pytest.fixture
def sources(tmp_path):
    """Three repos: two sharing a file, one on master, plus an empty one."""
    src = tmp_path / "src"
    shas = {}
    shas["alpha"] = make_repo(src / "alpha", {"lib/util.js": PADDING, "lib/only-alpha.js": PADDING + "a"})
    shas["beta"] = make_repo(src / "beta", {"lib/util.js": PADDING, "lib/only-beta.js": PADDING + "b"})
    shas["alpha2"] = commit(src / "alpha", {"lib/more.js": PADDING + "more"}, "alpha: second")
    shas["gamma"] = make_repo(src / "gamma", {"main.py": PADDING + "g"}, branch="master")
    gitops.init_repo(src / "empty")
    return src, shas


def plan_for(src, names, **kwargs):
    repos = [SourceRepo("t", name, str(src / name), description=f"the {name} program") for name in names]
    return build_mod.probe(build_plan(repos, dest_name="mine", **kwargs))


def test_probe_finds_the_real_branch_instead_of_assuming_main(sources):
    src, _ = sources
    plan = plan_for(src, ["gamma"])
    assert plan.included[0].repo.default_branch == "master"


def test_probe_drops_a_repo_that_has_no_commits(sources):
    src, _ = sources
    plan = plan_for(src, ["empty"])
    assert plan.included == ()
    assert "empty repository" in plan.skipped[0].reason


def test_a_dry_run_creates_absolutely_nothing(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    result = build_mod.build(out, plan_for(src, ["alpha"]), dry_run=True)
    assert not out.exists()
    assert result.steps and all(step.ok is None for step in result.steps)


def test_a_dry_run_lists_the_same_steps_the_real_build_runs(tmp_path, sources):
    src, _ = sources
    plan = plan_for(src, ["alpha", "gamma"])
    dry = build_mod.build(tmp_path / "dry", plan, dry_run=True)
    real = build_mod.build(tmp_path / "real", plan)
    assert [s.action for s in dry.steps] == [s.action for s in real.steps]


def test_every_commit_of_every_source_survives_the_merge(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    plan = plan_for(src, ["alpha", "beta", "gamma"])
    result = build_mod.build(out, plan)
    assert result.ok, result.failures

    for name in ("alpha", "beta", "gamma"):
        for sha in git(src / name, "rev-list", "--all").split():
            assert gitops.contains_commit(out, sha), f"{name} commit {sha} was lost"


def test_the_files_arrive_byte_for_byte_under_their_new_home(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    build_mod.build(out, plan_for(src, ["alpha"]))
    assert (out / "projects/alpha/lib/util.js").read_text() == PADDING
    assert (out / "projects/alpha/lib/more.js").exists()


def test_a_build_verifies_itself(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    result = build_mod.build(out, plan_for(src, ["alpha", "beta"]))
    assert build_mod.verify(out, result) == []


def test_the_recorded_commit_is_the_source_repos_own_tip(tmp_path, sources):
    src, shas = sources
    out = tmp_path / "out"
    result = build_mod.build(out, plan_for(src, ["alpha"]))
    assert result.merged["projects/alpha"] == shas["alpha2"]


def test_the_generated_index_names_every_project(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    build_mod.build(out, plan_for(src, ["alpha", "beta", "gamma"]))
    readme = (out / "README.md").read_text()
    for name in ("alpha", "beta", "gamma"):
        assert f"projects/{name}" in readme


def test_the_build_leaves_nothing_uncommitted(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    build_mod.build(out, plan_for(src, ["alpha"]))
    assert gitops.is_clean(out)


def test_the_source_repositories_are_left_exactly_as_they_were(tmp_path, sources):
    src, _ = sources
    before = {name: git(src / name, "rev-parse", "HEAD") for name in ("alpha", "beta")}
    build_mod.build(tmp_path / "out", plan_for(src, ["alpha", "beta"]))
    after = {name: git(src / name, "rev-parse", "HEAD") for name in ("alpha", "beta")}
    assert before == after


def test_overlap_is_found_in_the_real_merged_trees(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    plan = plan_for(src, ["alpha", "beta"])
    build_mod.build(out, plan)
    report = overlap.analyse(build_mod.collect_trees(out, plan))
    duplicated = {group.locations for group in report.duplicates}
    assert (("alpha", "lib/util.js"), ("beta", "lib/util.js")) in duplicated


def test_survey_compares_projects_without_a_full_clone(tmp_path, sources):
    src, _ = sources
    trees = build_mod.survey(tmp_path / "survey", plan_for(src, ["alpha", "beta"]))
    assert set(trees) == {"alpha", "beta"}
    assert "lib/util.js" in trees["alpha"]


# --------------------------------------------------------------------------
# updating an existing consolidated repo
# --------------------------------------------------------------------------

def test_later_work_in_a_source_repo_can_be_pulled_in(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    plan = plan_for(src, ["alpha"])
    build_mod.build(out, plan)

    new_sha = commit(src / "alpha", {"lib/later.js": PADDING + "later"}, "alpha: later work")
    result = build_mod.build(out, plan_for(src, ["alpha"]), update=True)

    assert result.ok, result.failures
    assert gitops.contains_commit(out, new_sha)
    assert (out / "projects/alpha/lib/later.js").exists()


def test_rebuilding_without_update_leaves_what_is_already_there_alone(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    build_mod.build(out, plan_for(src, ["alpha"]))
    before = git(out, "rev-parse", "HEAD")
    result = build_mod.build(out, plan_for(src, ["alpha"]))
    assert any(step.action == "skip" for step in result.steps)
    assert git(out, "rev-parse", "HEAD") == before


def test_a_new_repo_can_be_folded_into_an_existing_consolidated_one(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    build_mod.build(out, plan_for(src, ["alpha"]))
    build_mod.build(out, plan_for(src, ["alpha", "beta"]), update=True)
    assert (out / "projects/beta").exists()
    assert "projects/beta" in (out / "README.md").read_text()


def test_updating_one_project_does_not_erase_the_others_from_the_index(tmp_path, sources):
    """Regression: a scoped update used to rewrite the index from the subset,
    dropping every project it was not asked about."""
    src, _ = sources
    out = tmp_path / "out"
    full = plan_for(src, ["alpha", "beta", "gamma"])
    build_mod.build(out, full)

    commit(src / "alpha", {"lib/later.js": PADDING + "later"}, "alpha: later work")
    just_alpha = full.with_placements([p for p in full.placements if p.dest.endswith("alpha")])
    build_mod.build(out, just_alpha, update=True, index_plan=full)

    readme = (out / "README.md").read_text()
    for name in ("alpha", "beta", "gamma"):
        assert f"projects/{name}" in readme, f"{name} vanished from the index"

    recorded = layout.read_manifest_commits(out)
    assert set(recorded) == {"projects/alpha", "projects/beta", "projects/gamma"}
    assert layout.read_manifest(out).included == full.included


def test_a_scoped_update_keeps_the_other_projects_files_on_disk(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    full = plan_for(src, ["alpha", "beta"])
    build_mod.build(out, full)
    commit(src / "alpha", {"lib/later.js": PADDING + "later"}, "alpha: later")
    just_alpha = full.with_placements([p for p in full.placements if p.dest.endswith("alpha")])
    build_mod.build(out, just_alpha, update=True, index_plan=full)
    assert (out / "projects/beta/lib/only-beta.js").exists()


# --------------------------------------------------------------------------
# refusing to do the wrong thing
# --------------------------------------------------------------------------

def test_preflight_refuses_a_folder_that_already_has_something_in_it(tmp_path):
    out = tmp_path / "out"
    out.mkdir()
    (out / "my-work.txt").write_text("do not clobber me")
    problems = build_mod.preflight(out, update=False)
    assert problems and "not empty" in problems[0]


def test_preflight_refuses_to_rebuild_over_a_repo_without_update(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    build_mod.build(out, plan_for(src, ["alpha"]))
    assert any("--update" in problem for problem in build_mod.preflight(out, update=False))
    assert build_mod.preflight(out, update=True) == []


def test_preflight_refuses_to_update_a_repo_with_uncommitted_changes(tmp_path, sources):
    src, _ = sources
    out = tmp_path / "out"
    build_mod.build(out, plan_for(src, ["alpha"]))
    (out / "scratch.txt").write_text("half-finished work")
    assert any("uncommitted" in problem for problem in build_mod.preflight(out, update=True))


def test_an_empty_folder_is_fine(tmp_path):
    assert build_mod.preflight(tmp_path / "nothing-here", update=False) == []


# --------------------------------------------------------------------------
# checking one repository that already holds several projects
# --------------------------------------------------------------------------

def test_local_trees_treats_each_top_level_directory_as_a_project(tmp_path):
    repo = tmp_path / "repo"
    make_repo(repo, {"one/a.js": PADDING, "two/b.js": PADDING + "b", "top-level.txt": PADDING})
    trees = build_mod.local_trees(repo)
    assert set(trees) == {"one", "two"}
    assert "a.js" in trees["one"]


def test_local_trees_can_be_narrowed_to_named_directories(tmp_path):
    repo = tmp_path / "repo"
    make_repo(repo, {"one/a.js": PADDING, "two/b.js": PADDING, "three/c.js": PADDING})
    assert set(build_mod.local_trees(repo, dirs=["one", "two"])) == {"one", "two"}


def test_local_trees_finds_duplication_inside_a_single_repo(tmp_path):
    repo = tmp_path / "repo"
    make_repo(repo, {"one/shared.js": PADDING, "two/shared.js": PADDING})
    report = overlap.analyse(build_mod.local_trees(repo))
    assert len(report.duplicates) == 1
    assert report.duplicates[0].projects == ("one", "two")


def test_local_trees_of_a_repo_with_no_subdirectories_is_empty(tmp_path):
    repo = tmp_path / "repo"
    make_repo(repo, {"only.txt": PADDING})
    assert build_mod.local_trees(repo) == {}
