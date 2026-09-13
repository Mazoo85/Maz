import pytest

from consolidate.models import SourceRepo
from consolidate.plan import RESERVED, assign_dirs, build_plan, classify, slugify


@pytest.mark.parametrize(
    "raw,expected",
    [
        ("Maz", "maz"),
        ("codebase-memory-mcp", "codebase-memory-mcp"),
        ("My Cool Repo!", "my-cool-repo"),
        ("  spaced  ", "spaced"),
        ("...", "project"),
        ("Dots.In.Name", "dots-in-name"),
    ],
)
def test_slugify_makes_one_naming_rule(raw, expected):
    assert slugify(raw) == expected


def test_same_name_from_two_owners_does_not_collide():
    repos = [SourceRepo("alice", "app", "u"), SourceRepo("bob", "app", "u")]
    dirs = assign_dirs(repos)
    assert dirs["alice/app"] == "app"
    assert dirs["bob/app"] == "bob-app"
    assert len(set(dirs.values())) == 2


def test_a_repo_never_takes_a_reserved_directory_name():
    dirs = assign_dirs([SourceRepo("o", "docs", "u")])
    assert dirs["o/docs"] not in RESERVED


def test_naming_is_stable_when_the_list_grows():
    first = [SourceRepo("alice", "app", "u")]
    grown = first + [SourceRepo("bob", "app", "u")]
    assert assign_dirs(first)["alice/app"] == assign_dirs(grown)["alice/app"]


def test_forks_are_skipped_by_default_with_a_reason_that_explains_why():
    disposition, reason = classify(
        SourceRepo("o", "n", "u", is_fork=True), include_forks=False, include_archived=True
    )
    assert disposition == "skip"
    assert "upstream" in reason


def test_forks_can_be_opted_back_in():
    disposition, reason = classify(
        SourceRepo("o", "n", "u", is_fork=True), include_forks=True, include_archived=True
    )
    assert disposition == "include"
    assert "fork" in reason


def test_an_empty_repo_is_always_skipped():
    disposition, _ = classify(
        SourceRepo("o", "n", "u", empty=True), include_forks=True, include_archived=True
    )
    assert disposition == "skip"


def test_archived_repos_fold_in_by_default_but_are_noted():
    disposition, reason = classify(
        SourceRepo("o", "n", "u", archived=True), include_forks=False, include_archived=True
    )
    assert disposition == "include"
    assert "archived" in reason


def test_build_plan_places_every_repo_under_the_prefix():
    plan = build_plan([SourceRepo("o", "One", "u"), SourceRepo("o", "Two", "u")], dest_name="mine")
    assert [p.dest for p in plan.placements] == ["projects/one", "projects/two"]


def test_an_empty_prefix_puts_projects_at_the_top_level():
    plan = build_plan([SourceRepo("o", "One", "u")], dest_name="mine", prefix="")
    assert plan.placements[0].dest == "one"
