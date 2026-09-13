import pytest

from conftest import PADDING, make_repo, needs_git
from consolidate import gitops

pytestmark = needs_git


def test_a_fresh_repo_is_usable_immediately(tmp_path):
    ok, err = gitops.init_repo(tmp_path / "r")
    assert ok, err
    assert gitops.is_repo(tmp_path / "r")
    assert not gitops.has_commits(tmp_path / "r")


def test_a_fresh_repo_can_commit_without_a_configured_user(tmp_path):
    """Without an identity the first commit fails with git's 'who are you' error."""
    root = tmp_path / "r"
    gitops.init_repo(root)
    (root / "a.txt").write_text("hello")
    ok, err = gitops.commit_all(root, "first")
    assert ok, err
    assert gitops.has_commits(root)


def test_committing_nothing_is_not_a_failure(tmp_path):
    root = tmp_path / "r"
    gitops.init_repo(root)
    (root / "a.txt").write_text("hello")
    gitops.commit_all(root, "first")
    ok, note = gitops.commit_all(root, "again")
    assert ok and note == "nothing to commit"


def test_branches_are_read_from_the_remote_not_guessed(tmp_path):
    source = tmp_path / "src"
    make_repo(source, {"a.txt": PADDING}, branch="master")
    assert gitops.remote_branches(str(source)) == ["master"]
    assert gitops.default_branch(str(source)) == "master"


def test_an_empty_repo_reports_no_branches(tmp_path):
    root = tmp_path / "empty"
    gitops.init_repo(root)
    assert gitops.remote_branches(str(root)) == []


def test_ls_tree_lists_every_file_with_its_blob_and_size(tmp_path):
    source = tmp_path / "src"
    make_repo(source, {"a.txt": "hello", "sub/b.txt": "world"})
    tree = gitops.ls_tree(source, "HEAD")
    assert set(tree) == {"a.txt", "sub/b.txt"}
    assert tree["a.txt"][1] == 5


def test_identical_files_share_a_blob_which_is_what_overlap_relies_on(tmp_path):
    source = tmp_path / "src"
    make_repo(source, {"a.txt": "same", "b.txt": "same"})
    tree = gitops.ls_tree(source, "HEAD")
    assert tree["a.txt"][0] == tree["b.txt"][0]


def test_ls_tree_of_a_ref_that_is_not_there_is_empty_not_an_explosion(tmp_path):
    root = tmp_path / "r"
    gitops.init_repo(root)
    assert gitops.ls_tree(root, "nope") == {}


def test_a_missing_git_command_is_reported_rather_than_raised():
    code, _, err = gitops.run_git(["definitely-not-a-command"])
    assert code != 0 and err


def test_contains_commit_is_false_for_an_unrelated_sha(tmp_path):
    root = tmp_path / "r"
    make_repo(root, {"a.txt": "hi"})
    assert not gitops.contains_commit(root, "0" * 40)
