"""Turning recent GitHub Actions runs into candidates."""

import http.client
import json
from unittest.mock import patch

from forge.github import api, repo_slug
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


# --- Fix 1: api() must not raise on a mid-stream read failure -------------


def test_api_returns_empty_dict_on_incomplete_read():
    # http.client.IncompleteRead subclasses HTTPException -> Exception, not
    # OSError/URLError/ValueError/TypeError, so it escapes the old except
    # tuple entirely. token passed positionally (4th arg) to isolate this
    # from the Fix 4 keyword-rename test below.
    class FakeResp:
        def __enter__(self):
            return self

        def __exit__(self, *exc):
            return False

        def read(self):
            raise http.client.IncompleteRead(b"")

    with patch("forge.github.urllib.request.urlopen", return_value=FakeResp()):
        assert api("/repos/a/b/actions/runs", "GET", None, "x") == {}


# --- Fix 2: collect() must not raise on a non-dict payload ----------------


def test_collect_returns_empty_for_none_payload():
    assert collect(".", fetch=lambda path: None, slug="a/b") == []


def test_collect_returns_empty_for_list_payload():
    assert collect(".", fetch=lambda path: [1, 2, 3], slug="a/b") == []


# --- Fix 3: repo_slug must not go dark on trailing slashes ----------------


def test_repo_slug_tolerates_trailing_slash_https():
    assert repo_slug(None, runner=lambda a: "https://github.com/Mazoo85/Maz/\n") == "Mazoo85/Maz"


def test_repo_slug_tolerates_trailing_slash_dot_git():
    assert repo_slug(None, runner=lambda a: "https://github.com/Mazoo85/Maz.git/\n") == "Mazoo85/Maz"


def test_repo_slug_handles_dotted_repo_name_without_greedy_git_strip():
    url = "git@github.com:someone/my.git.tools.git\n"
    assert repo_slug(None, runner=lambda a: url) == "someone/my.git.tools"


# --- Fix 4: api()'s documented `token=` keyword must actually exist -------


def test_api_accepts_token_keyword():
    with patch("forge.github.urllib.request.urlopen") as mock_urlopen:
        mock_urlopen.return_value.__enter__.return_value.read.return_value = json.dumps(
            {"ok": True}
        ).encode("utf-8")
        assert api("/repos/a/b", token="x") == {"ok": True}
        assert mock_urlopen.called


# --- Fix 5: a malformed created_at must never mask a real failure --------


def test_from_runs_surfaces_failure_despite_malformed_sibling_timestamp():
    runs = [
        {"name": "Music CI", "conclusion": "failure", "head_branch": "main",
         "path": ".github/workflows/music-ci.yml", "created_at": "2026-09-09T02:00:00Z",
         "html_url": "https://github.com/Mazoo85/Maz/actions/runs/3"},
        {"name": "Music CI", "conclusion": "success", "head_branch": "main",
         "path": ".github/workflows/music-ci.yml", "created_at": None,
         "html_url": "https://github.com/Mazoo85/Maz/actions/runs/4"},
    ]
    cands = from_runs(runs)
    assert len(cands) == 1
    assert "actions/runs/3" in cands[0].detail
