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
    # returns real workflow_runs must flow through from_runs() unchanged.
    #
    # This pins the branch-scoped design (Fix 7), not the old single-call
    # one: collect() queries once per name in MAIN_BRANCHES, so two paths
    # are recorded, each carrying the slug, and together they must cover
    # both "main" and "master". The old assertion of exactly one call
    # encoded the unscoped-query bug (every branch sharing one N-run
    # window, which is what let main's runs fall out of it) — that was
    # incidental to what this test verifies, not the behavior it exists to
    # protect, so pinning the new count strengthens rather than weakens it.
    seen_paths = []

    def fetch(path):
        seen_paths.append(path)
        return {"workflow_runs": RUNS}

    result = collect(tmp_path, fetch=fetch, slug="Mazoo85/Maz")
    assert result == from_runs(RUNS)
    assert len(seen_paths) == 2
    assert all("Mazoo85/Maz" in p for p in seen_paths)
    assert any("branch=main" in p for p in seen_paths)
    assert any("branch=master" in p for p in seen_paths)


# --- base_branch: the configured base branch's CI must count too ---------


def test_from_runs_ignores_a_non_default_branch_with_no_base_branch_given():
    runs = [dict(RUNS[0], head_branch="claude/zomboid-sega-neon-anchorage-i5emkk")]
    assert from_runs(runs) == []


def test_from_runs_watches_the_configured_base_branch():
    runs = [dict(RUNS[0], head_branch="claude/zomboid-sega-neon-anchorage-i5emkk")]
    cands = from_runs(runs, base_branch="claude/zomboid-sega-neon-anchorage-i5emkk")
    assert len(cands) == 1
    assert cands[0].source == "ci:music-ci"


def test_from_runs_still_watches_main_and_master_when_base_branch_is_set():
    # base_branch extends the watch list, it does not replace it.
    cands = from_runs(RUNS, base_branch="claude/zomboid-sega-neon-anchorage-i5emkk")
    assert len(cands) == 1
    assert cands[0].source == "ci:music-ci"


def test_collect_queries_the_configured_base_branch_too(tmp_path):
    seen_paths = []

    def fetch(path):
        seen_paths.append(path)
        return {"workflow_runs": []}

    collect(tmp_path, fetch=fetch, slug="a/b",
            base_branch="claude/zomboid-sega-neon-anchorage-i5emkk")
    assert len(seen_paths) == 3
    assert any("branch=main" in p for p in seen_paths)
    assert any("branch=master" in p for p in seen_paths)
    assert any("branch=claude/zomboid-sega-neon-anchorage-i5emkk" in p for p in seen_paths)


def test_collect_finds_a_failure_on_the_configured_base_branch(tmp_path):
    runs = [dict(RUNS[0], head_branch="claude/zomboid-sega-neon-anchorage-i5emkk")]

    def fetch(path):
        if "branch=claude/zomboid-sega-neon-anchorage-i5emkk" in path:
            return {"workflow_runs": runs}
        return {"workflow_runs": []}

    result = collect(tmp_path, fetch=fetch, slug="a/b",
                     base_branch="claude/zomboid-sega-neon-anchorage-i5emkk")
    assert len(result) == 1
    assert result[0].source == "ci:music-ci"


def test_collect_does_not_duplicate_the_query_when_base_branch_is_main(tmp_path):
    seen_paths = []

    def fetch(path):
        seen_paths.append(path)
        return {"workflow_runs": []}

    collect(tmp_path, fetch=fetch, slug="a/b", base_branch="main")
    assert len(seen_paths) == 2


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


# --- Fix 6: from_runs() must not raise on non-dict elements in runs list ----


def test_from_runs_skips_none_elements():
    """A None element in the runs list should be skipped, not raise AttributeError."""
    assert from_runs([None]) == []


def test_collect_skips_none_elements_in_workflow_runs():
    """A None in workflow_runs should be skipped, not raise AttributeError."""
    assert collect(".", fetch=lambda path: {"workflow_runs": [None]}, slug="a/b") == []


# --- Fix 7: collect() must scope the runs query to each MAIN_BRANCHES name -


def test_collect_requests_a_branch_scoped_path_per_main_branch():
    # The unscoped `/actions/runs?per_page=N` endpoint returns the N most
    # recent runs across *every* branch. On a repo with heavy feature-branch
    # traffic, main's runs fall out of that window entirely and collect()
    # silently reports "no failures" instead of "I couldn't see". collect()
    # must instead ask GitHub to filter server-side, once per name in
    # MAIN_BRANCHES, so the window is N runs *of that branch*.
    seen_paths = []

    def fetch(path):
        seen_paths.append(path)
        return {"workflow_runs": []}

    collect(".", fetch=fetch, slug="a/b")
    assert len(seen_paths) == 2
    assert any("branch=main" in p for p in seen_paths)
    assert any("branch=master" in p for p in seen_paths)
    assert all("a/b" in p for p in seen_paths)


def test_collect_finds_failure_invisible_to_the_old_unscoped_query():
    # Regression test for the real bug: a getter that only returns runs when
    # asked for a specific branch (exactly what the GitHub API does when the
    # `branch=` query param is used) and an empty payload otherwise. Under
    # the old `?per_page=N` query (no branch param) this getter would return
    # `{"workflow_runs": []}` and collect() would silently report zero
    # candidates even though main has a real, currently-red workflow.
    def fetch(path):
        if "branch=main" in path:
            return {"workflow_runs": RUNS}
        return {"workflow_runs": []}

    result = collect(".", fetch=fetch, slug="a/b")
    assert result == from_runs(RUNS)
    assert len(result) == 1
    assert result[0].source == "ci:music-ci"


def test_collect_merges_results_from_both_branch_queries():
    # main and master each have their own independently-failing workflow;
    # both must survive the merge, not just whichever query runs last.
    main_runs = [
        {"name": "Music CI", "conclusion": "failure", "head_branch": "main",
         "path": ".github/workflows/music-ci.yml", "created_at": "2026-09-09T02:00:00Z",
         "html_url": "https://github.com/a/b/actions/runs/10"},
    ]
    master_runs = [
        {"name": "Scraper CI", "conclusion": "failure", "head_branch": "master",
         "path": ".github/workflows/scraper-ci.yml", "created_at": "2026-09-09T02:00:00Z",
         "html_url": "https://github.com/a/b/actions/runs/11"},
    ]

    def fetch(path):
        if "branch=main" in path:
            return {"workflow_runs": main_runs}
        if "branch=master" in path:
            return {"workflow_runs": master_runs}
        return {"workflow_runs": []}

    result = collect(".", fetch=fetch, slug="a/b")
    sources = {c.source for c in result}
    assert sources == {"ci:music-ci", "ci:scraper-ci"}


def test_collect_survives_junk_payload_from_one_branch_query():
    # If one branch's query returns a junk payload (None, a list, or a
    # workflow_runs list containing a non-dict element), the other branch's
    # valid results must still come through, and nothing may raise.
    for junk in (None, [1, 2, 3], {"workflow_runs": [None]}):

        def fetch(path, junk=junk):
            if "branch=main" in path:
                return {"workflow_runs": RUNS}
            return junk

        result = collect(".", fetch=fetch, slug="a/b")
        assert result == from_runs(RUNS)


def test_from_runs_preserves_valid_siblings_of_malformed_element():
    """A malformed element does not discard its valid siblings."""
    runs = [
        None,  # malformed element
        {"name": "Music CI", "conclusion": "failure", "head_branch": "main",
         "path": ".github/workflows/music-ci.yml", "created_at": "2026-09-09T02:00:00Z",
         "html_url": "https://github.com/Mazoo85/Maz/actions/runs/3"},
    ]
    cands = from_runs(runs)
    assert len(cands) == 1
    assert cands[0].source == "ci:music-ci"
