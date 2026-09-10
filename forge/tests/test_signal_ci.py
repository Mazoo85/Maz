"""Turning recent GitHub Actions runs into candidates."""

from forge.github import repo_slug
from forge.signals.ci import collect, from_runs

RUNS = [
    {"name": "Music CI", "conclusion": "failure", "head_branch": "main",
     "path": ".github/workflows/music-ci.yml", "created_at": "2026-09-09T02:00:00Z",
     "html_url": "https://github.com/Mazoo85/Maz/actions/runs/3"},
    {"name": "Music CI", "conclusion": "success", "head_branch": "main",
     "path": ".github/workflows/music-ci.yml", "created_at": "2026-09-08T02:00:00Z",
     "html_url": "https://github.com/Mazoo85/Maz/actions/runs/2"},
    {"name": "Crew CI", "conclusion": "success", "head_branch": "main",
     "path": ".github/workflows/crew-ci.yml", "created_at": "2026-09-09T02:00:00Z",
     "html_url": "https://github.com/Mazoo85/Maz/actions/runs/1"},
]


def test_only_the_latest_run_per_workflow_counts():
    cands = from_runs(RUNS)
    assert len(cands) == 1
    assert cands[0].source == "ci:music-ci"


def test_green_workflows_produce_nothing():
    green = [r for r in RUNS if r["conclusion"] == "success"]
    assert from_runs(green) == []


def test_candidate_names_the_workflow_and_links_the_run():
    c = from_runs(RUNS)[0]
    assert "Music CI" in c.task
    assert c.kind == "ci"
    assert "actions/runs/3" in c.detail


def test_paths_point_at_the_workflow_subject_not_the_workflow_file():
    # music-ci.yml tests music/, so the work is in music/ — never in the
    # workflow file itself, which is a no-touch path.
    c = from_runs(RUNS)[0]
    assert c.paths == ("music/",)
    assert ".github" not in " ".join(c.paths)


def test_unknown_workflow_gets_no_paths():
    runs = [{"name": "Mystery CI", "conclusion": "failure", "head_branch": "main",
             "path": ".github/workflows/mystery.yml", "created_at": "2026-09-09T02:00:00Z",
             "html_url": "u"}]
    assert from_runs(runs)[0].paths == ()


def test_non_main_branches_are_ignored():
    runs = [dict(RUNS[0], head_branch="some-feature")]
    assert from_runs(runs) == []


def test_collect_returns_empty_without_a_fetcher(tmp_path):
    assert collect(tmp_path, fetch=lambda path: {}, slug="a/b") == []


def test_collect_wires_a_real_fetch_into_from_runs(tmp_path):
    # The two tests above only ever hand collect() an empty payload, which
    # would pass even if collect() ignored its fetch function entirely and
    # always returned []. This exercises the actual wiring: a fetch that
    # returns real workflow_runs must flow through from_runs() unchanged,
    # and the requested path must carry the slug collect() was given.
    seen_paths = []

    def fetch(path):
        seen_paths.append(path)
        return {"workflow_runs": RUNS}

    result = collect(tmp_path, fetch=fetch, slug="Mazoo85/Maz")
    assert result == from_runs(RUNS)
    assert len(seen_paths) == 1
    assert "Mazoo85/Maz" in seen_paths[0]


def test_repo_slug_parses_https_and_ssh():
    assert repo_slug(None, runner=lambda a: "https://github.com/Mazoo85/Maz.git\n") == "Mazoo85/Maz"
    assert repo_slug(None, runner=lambda a: "git@github.com:Mazoo85/Maz.git\n") == "Mazoo85/Maz"
    assert repo_slug(None, runner=lambda a: "") is None
