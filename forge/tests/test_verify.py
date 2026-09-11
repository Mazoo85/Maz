"""VERIFY: run the zone's checks, then open a draft PR if they pass."""

import json

from forge.checks import commands_for
from forge.do import CrewOutcome
from forge.verify import open_draft_pr, run_checks


def _exchange(root):
    """Give a tmp_path repo the declaration run_checks now requires."""
    shared = root / "shared"
    shared.mkdir(parents=True, exist_ok=True)
    (shared / "exchange.json").write_text(
        json.dumps({"publishes": {}, "consumes": []}), encoding="utf-8"
    )
    return root


OUTCOME = CrewOutcome(ok=True, branch="forge/2026-09-11-write-docs",
                      files=("docs/a.md",), cost_usd=0.5, duration_min=3.0)
CHOSEN = {
    "candidate": {"task": "Write the loot table docs", "source": "todo:docs/a.md:1",
                  "kind": "todo", "paths": ["docs/a.md"], "key": "abc"},
    "score": 17.5, "value": 5.0, "confidence": 7.0, "risk": 1.0, "zone": "docs/",
}


def test_music_zone_runs_the_music_tests():
    cmds = commands_for("music/")
    assert any("music" in " ".join(c) for c in cmds)


def test_docs_zone_has_no_commands_and_passes_trivially(tmp_path):
    assert commands_for("docs/") == ()
    result = run_checks("docs/", _exchange(tmp_path), runner=lambda cmd, root: (1, "should not run"))
    assert result.ok is True
    assert result.ran == ()


def test_unknown_zone_has_no_commands(tmp_path):
    assert commands_for("nowhere/") == ()


def test_tests_zone_has_no_wrong_command_mapped():
    """This repo's `tests/` directory is C++ (CMake/ctest, needs the Vulkan
    SDK the sandbox lacks), not the Python suite `forge/tests`. There is no
    command this module can honestly run for it, so it must not claim one —
    `commands_for` must not map `tests/` to the Forge's own pytest suite,
    which verifies nothing about C++ changes.
    """
    assert commands_for("tests/") == ()


def test_all_commands_must_pass(tmp_path):
    calls = []

    def runner(cmd, root):
        calls.append(cmd)
        return (0, "ok")

    result = run_checks("scraper/", _exchange(tmp_path), runner=runner)
    assert result.ok is True
    assert len(calls) == len(commands_for("scraper/")) == 2


def test_a_failing_command_stops_the_run(tmp_path):
    calls = []

    def runner(cmd, root):
        calls.append(cmd)
        return (1, "2 failed")

    result = run_checks("scraper/", _exchange(tmp_path), runner=runner)
    assert result.ok is False
    assert "2 failed" in result.output
    assert len(calls) == 1  # stopped at the first failure, did not run the second


def test_open_draft_pr_posts_a_draft(tmp_path):
    posted = {}

    def poster(path, body):
        posted["path"] = path
        posted["body"] = body
        return {"number": 42}

    number = open_draft_pr(OUTCOME, CHOSEN, tmp_path, slug="Mazoo85/Maz", poster=poster)
    assert number == 42
    assert posted["path"] == "/repos/Mazoo85/Maz/pulls"
    assert posted["body"]["draft"] is True
    assert posted["body"]["head"] == OUTCOME.branch
    assert posted["body"]["base"] == "main"


def test_pr_body_explains_the_pick():
    posted = {}

    def poster(path, body):
        posted["body"] = body
        return {"number": 1}

    open_draft_pr(OUTCOME, CHOSEN, None, slug="a/b", poster=poster)
    text = posted["body"]["body"]
    assert "todo:docs/a.md:1" in text
    assert "17.5" in text
    assert "docs/a.md" in text


def test_open_draft_pr_returns_none_when_github_refuses(tmp_path):
    assert open_draft_pr(OUTCOME, CHOSEN, tmp_path, slug="a/b", poster=lambda p, b: {}) is None


def test_open_draft_pr_returns_none_without_a_slug(tmp_path):
    assert open_draft_pr(OUTCOME, CHOSEN, tmp_path, slug=None,
                         poster=lambda p, b: {"number": 1}) is None


def test_pr_body_renders_sensibly_with_no_cost_reported():
    # cost_usd=None ("not reported", per do.CrewOutcome) must never render as
    # "$None" or crash the f-string formatting.
    outcome = CrewOutcome(ok=True, branch="forge/x", files=("a.py",),
                          cost_usd=None, duration_min=2.0)
    posted = {}

    def poster(path, body):
        posted["body"] = body
        return {"number": 1}

    open_draft_pr(outcome, CHOSEN, None, slug="a/b", poster=poster)
    text = posted["body"]["body"]
    assert "$None" not in text
    assert "None" not in text


def test_open_draft_pr_uses_the_configured_base_branch(tmp_path):
    """The PR's `base` must be whatever base_branch the caller passes, not
    the module's own "main" default — a lingering hard-coded fallback here
    is exactly the bug this fix removes.
    """
    posted = {}

    def poster(path, body):
        posted["body"] = body
        return {"number": 7}

    open_draft_pr(OUTCOME, CHOSEN, tmp_path, slug="a/b", poster=poster,
                  base_branch="claude/zomboid-sega-neon-anchorage-i5emkk")
    assert posted["body"]["base"] == "claude/zomboid-sega-neon-anchorage-i5emkk"


def test_open_draft_pr_defaults_base_branch_to_main_when_not_given(tmp_path):
    posted = {}

    def poster(path, body):
        posted["body"] = body
        return {"number": 8}

    open_draft_pr(OUTCOME, CHOSEN, tmp_path, slug="a/b", poster=poster)
    assert posted["body"]["base"] == "main"


def test_pr_body_renders_sensibly_with_no_files():
    outcome = CrewOutcome(ok=True, branch="forge/x", files=(),
                          cost_usd=1.23, duration_min=2.0)
    posted = {}

    def poster(path, body):
        posted["body"] = body
        return {"number": 1}

    open_draft_pr(outcome, CHOSEN, None, slug="a/b", poster=poster)
    text = posted["body"]["body"]
    assert "(none recorded)" in text
