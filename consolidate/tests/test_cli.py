import json

from typer.testing import CliRunner

from conftest import PADDING, commit, make_repo, needs_git, repos_file
from consolidate import gitops
from consolidate.cli import app

runner = CliRunner()
pytestmark = needs_git


def flat(output):
    """Rich wraps output to the terminal width, so collapse whitespace before
    looking for a phrase in it."""
    return " ".join(output.split())


def local_repos(tmp_path):
    src = tmp_path / "src"
    make_repo(src / "alpha", {"lib/util.js": PADDING})
    make_repo(src / "beta", {"lib/util.js": PADDING, "lib/b.js": PADDING + "b"})
    gitops.init_repo(src / "empty")
    return src, repos_file(
        tmp_path,
        [
            {"owner": "t", "name": "alpha", "url": str(src / "alpha"), "description": "first"},
            {"owner": "t", "name": "beta", "url": str(src / "beta"), "description": "second"},
            {"owner": "t", "name": "empty", "url": str(src / "empty")},
        ],
    )


def test_plan_shows_what_would_happen(tmp_path):
    _, repos = local_repos(tmp_path)
    result = runner.invoke(app, ["plan", "--from-json", str(repos), "--name", "mine"])
    assert result.exit_code == 0, result.output
    assert "alpha" in result.output and "beta" in result.output
    assert "left out" in result.output


def test_a_plan_can_be_saved_and_replayed_offline(tmp_path):
    _, repos = local_repos(tmp_path)
    saved = tmp_path / "plan.json"
    runner.invoke(app, ["plan", "--from-json", str(repos), "--out", str(saved)])
    assert json.loads(saved.read_text())["placements"]

    result = runner.invoke(app, ["show-plan", str(saved)])
    assert result.exit_code == 0
    assert "alpha" in result.output


def test_build_is_a_dry_run_unless_you_ask_for_it(tmp_path):
    _, repos = local_repos(tmp_path)
    out = tmp_path / "out"
    result = runner.invoke(app, ["build", str(out), "--from-json", str(repos)])
    assert result.exit_code == 0, result.output
    assert "dry run" in flat(result.output).lower()
    assert not out.exists()


def test_build_with_yes_makes_the_repository(tmp_path):
    src, repos = local_repos(tmp_path)
    out = tmp_path / "out"
    result = runner.invoke(app, ["build", str(out), "--from-json", str(repos), "--yes"])
    assert result.exit_code == 0, result.output
    assert (out / "projects/alpha/lib/util.js").exists()
    assert (out / "projects/beta/lib/b.js").exists()
    assert (out / "CONSOLIDATION.md").exists()


def test_build_reports_the_duplication_it_found(tmp_path):
    _, repos = local_repos(tmp_path)
    result = runner.invoke(app, ["build", str(tmp_path / "out"), "--from-json", str(repos), "--yes"])
    assert "overlap" in flat(result.output).lower() or "util.js" in flat(result.output)


def test_build_never_pushes_and_says_so(tmp_path):
    _, repos = local_repos(tmp_path)
    result = runner.invoke(app, ["build", str(tmp_path / "out"), "--from-json", str(repos), "--yes"])
    assert "Nothing was pushed" in flat(result.output)


def test_building_over_an_existing_repo_is_refused_with_advice(tmp_path):
    _, repos = local_repos(tmp_path)
    out = tmp_path / "out"
    runner.invoke(app, ["build", str(out), "--from-json", str(repos), "--yes"])
    result = runner.invoke(app, ["build", str(out), "--from-json", str(repos), "--yes"])
    assert result.exit_code == 2
    assert "--update" in result.output


def test_update_pulls_later_work_in(tmp_path):
    src, repos = local_repos(tmp_path)
    out = tmp_path / "out"
    runner.invoke(app, ["build", str(out), "--from-json", str(repos), "--yes"])
    commit(src / "alpha", {"lib/later.js": PADDING + "later"}, "alpha: later work")

    result = runner.invoke(app, ["update", str(out), "alpha", "--yes"])
    assert result.exit_code == 0, result.output
    assert (out / "projects/alpha/lib/later.js").exists()


def test_a_scoped_update_keeps_every_project_in_the_index(tmp_path):
    src, repos = local_repos(tmp_path)
    out = tmp_path / "out"
    runner.invoke(app, ["build", str(out), "--from-json", str(repos), "--yes"])
    commit(src / "alpha", {"lib/later.js": PADDING + "later"}, "alpha: later work")
    runner.invoke(app, ["update", str(out), "alpha", "--yes"])
    readme = (out / "README.md").read_text()
    assert "projects/alpha" in readme and "projects/beta" in readme


def test_update_is_also_a_dry_run_by_default(tmp_path):
    src, repos = local_repos(tmp_path)
    out = tmp_path / "out"
    runner.invoke(app, ["build", str(out), "--from-json", str(repos), "--yes"])
    commit(src / "alpha", {"lib/later.js": PADDING}, "alpha: later")
    result = runner.invoke(app, ["update", str(out), "alpha"])
    assert "Dry run" in flat(result.output)
    assert not (out / "projects/alpha/lib/later.js").exists()


def test_update_on_a_folder_we_did_not_build_says_so(tmp_path):
    plain = tmp_path / "plain"
    plain.mkdir()
    result = runner.invoke(app, ["update", str(plain)])
    assert result.exit_code == 2
    assert "consolidate.json" in result.output


def test_update_of_an_unknown_project_names_the_problem(tmp_path):
    _, repos = local_repos(tmp_path)
    out = tmp_path / "out"
    runner.invoke(app, ["build", str(out), "--from-json", str(repos), "--yes"])
    result = runner.invoke(app, ["update", str(out), "nonexistent"])
    assert result.exit_code == 2
    assert "nonexistent" in result.output


def test_asking_for_nothing_explains_what_to_pass(tmp_path):
    result = runner.invoke(app, ["plan"], env={"GITHUB_TOKEN": "", "GH_TOKEN": ""})
    assert result.exit_code != 0
    assert "--user" in result.output or "--repo" in result.output


def test_report_finds_the_shared_file_between_two_projects(tmp_path):
    _, repos = local_repos(tmp_path)
    result = runner.invoke(app, ["report", "--from-json", str(repos)])
    assert result.exit_code == 0, result.output
    assert "util.js" in result.output


def test_check_finds_duplication_inside_one_repository(tmp_path):
    repo = tmp_path / "repo"
    make_repo(repo, {"one/shared.js": PADDING, "two/shared.js": PADDING})
    result = runner.invoke(app, ["check", str(repo)])
    assert result.exit_code == 0, result.output
    assert "shared.js" in result.output


def test_check_says_so_when_a_repository_is_clean(tmp_path):
    repo = tmp_path / "repo"
    make_repo(repo, {"one/a.js": PADDING + "a", "two/b.js": PADDING + "b"})
    result = runner.invoke(app, ["check", str(repo)])
    assert result.exit_code == 0
    assert "No duplicated files" in flat(result.output)


def test_check_can_be_narrowed_to_named_directories(tmp_path):
    repo = tmp_path / "repo"
    make_repo(repo, {"one/s.js": PADDING, "two/s.js": PADDING, "three/s.js": PADDING})
    result = runner.invoke(app, ["check", str(repo), "-d", "one", "-d", "three"])
    assert result.exit_code == 0
    assert "two/" not in result.output


def test_check_on_something_that_is_not_a_repository_says_so(tmp_path):
    plain = tmp_path / "plain"
    plain.mkdir()
    result = runner.invoke(app, ["check", str(plain)])
    assert result.exit_code == 2
    assert "not a git repository" in flat(result.output)
