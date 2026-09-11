"""DECIDE: score every candidate, apply the three sanity rules, pick one."""

from forge.config import ForgeConfig
from forge.decide import decide, read_tonight, score_one, write_tonight
from forge.models import Candidate
from forge.sense import candidate_to_dict


def _pulse(*candidates):
    return {"generated_at": "2026-09-10T00:00:00Z", "sources": {},
            "candidates": [candidate_to_dict(c) for c in candidates]}


DOC = Candidate(task="Document the loot tables", source="todo:docs/x.md:3",
                kind="todo", paths=("docs/x.md",), detail="recent")
MUSIC = Candidate(task="Fix the failing Music CI workflow on main",
                  source="ci:music-ci", kind="ci", paths=("music/",))
ENGINE = Candidate(task="Add VMA", source="roadmap:phase-3", kind="roadmap",
                   paths=("engine/src/render/vma.cpp",))


def test_red_ci_outranks_a_recent_todo():
    record = decide(_pulse(DOC, MUSIC), ForgeConfig())
    assert record["chosen"]["candidate"]["source"] == "ci:music-ci"


def test_candidate_outside_safe_zones_is_skipped():
    record = decide(_pulse(ENGINE), ForgeConfig())
    assert record["chosen"] is None
    assert record["skipped"]["outside_zone"] == 1


def test_candidate_with_no_paths_is_skipped():
    roadmap = Candidate(task="Mixer, buses, ducking", source="roadmap:phase-7", kind="roadmap")
    record = decide(_pulse(roadmap), ForgeConfig())
    assert record["chosen"] is None
    assert record["skipped"]["outside_zone"] == 1


def test_struck_out_candidate_is_skipped():
    record = decide(_pulse(DOC), ForgeConfig(), strikes={DOC.key(): 3})
    assert record["chosen"] is None
    assert record["skipped"]["struck_out"] == 1


def test_two_strikes_still_runs():
    record = decide(_pulse(DOC), ForgeConfig(), strikes={DOC.key(): 2})
    assert record["chosen"] is not None


def test_variety_rule_blocks_a_third_run_in_the_same_zone():
    record = decide(_pulse(DOC), ForgeConfig(), recent_zones=["docs/", "docs/"])
    assert record["chosen"] is None
    assert record["skipped"]["variety"] == 1


def test_variety_rule_allows_a_second_run_in_the_same_zone():
    record = decide(_pulse(DOC), ForgeConfig(), recent_zones=["docs/", "music/"])
    assert record["chosen"] is not None


def test_score_floor_blocks_everything_below_it():
    cfg = ForgeConfig(score_floor=9999.0)
    record = decide(_pulse(DOC, MUSIC), cfg)
    assert record["chosen"] is None
    assert record["skipped"]["below_floor"] == 2


def test_empty_pulse_is_a_clean_no_task():
    record = decide(_pulse(), ForgeConfig())
    assert record["chosen"] is None
    assert record["considered"] == 0


def test_record_carries_runners_up_with_scores():
    record = decide(_pulse(DOC, MUSIC), ForgeConfig())
    assert len(record["runners_up"]) == 1
    assert record["runners_up"][0]["task"] == DOC.task
    assert record["runners_up"][0]["score"] > 0


def test_risky_paths_lower_the_score():
    safe = score_one(Candidate(task="t", source="s", kind="todo",
                               paths=("docs/a.md",)), "docs/", ForgeConfig())
    wide = score_one(Candidate(task="t", source="s", kind="todo",
                               paths=tuple(f"docs/f{i}.md" for i in range(8))),
                     "docs/", ForgeConfig())
    assert wide.risk > safe.risk
    assert wide.score < safe.score


def test_tests_nearby_raises_confidence():
    plain = score_one(Candidate(task="t", source="s", kind="todo", paths=("docs/a.md",)),
                      "docs/", ForgeConfig())
    tested = score_one(Candidate(task="t", source="s", kind="todo", paths=("docs/a.md",),
                                 tests_nearby=True), "docs/", ForgeConfig())
    assert tested.confidence > plain.confidence


def test_write_and_read_tonight_round_trip(tmp_path):
    cfg = ForgeConfig()
    record = decide(_pulse(DOC), cfg)
    write_tonight(record, tmp_path, cfg)
    assert read_tonight(tmp_path, cfg)["chosen"]["candidate"]["task"] == DOC.task


def test_read_tonight_returns_empty_when_missing(tmp_path):
    assert read_tonight(tmp_path, ForgeConfig()) == {}


# --- Robustness beyond the brief's own examples --------------------------
#
# Tasks 1-8 each shipped at least one bug from the plan's own example code:
# a guard on a container that doesn't cover its own elements. `decide()`
# reads `pulse.get("candidates")` and hands each element to
# `candidate_from_dict`, which does key access — so a malformed pulse (not a
# dict at all, or a `candidates` value that isn't a list) must degrade to
# "nothing found", never raise.

def test_pulse_that_is_not_a_dict_is_a_clean_no_task():
    record = decide("not a pulse", ForgeConfig())
    assert record["chosen"] is None
    assert record["considered"] == 0


def test_candidates_value_that_is_not_a_list_is_a_clean_no_task():
    """A scalar (not falsy, not iterable) `candidates` value must not crash
    the `for` loop itself — `pulse.get("candidates") or []` only rescues a
    falsy value, not a truthy non-iterable one like an int."""
    record = decide({"candidates": 5}, ForgeConfig())
    assert record["chosen"] is None
    assert record["considered"] == 0


def test_non_dict_candidate_element_is_skipped_not_fatal():
    record = decide({"candidates": [DOC.task, candidate_to_dict(DOC)]}, ForgeConfig())
    assert record["considered"] == 1
    assert record["chosen"]["candidate"]["task"] == DOC.task


def test_read_tonight_ignores_non_dict_json(tmp_path):
    """`tonight.json` corrupted into non-dict JSON (a list, a number) must
    read back as {} rather than a value that breaks every caller's `.get()`
    — the same contract `sense.read_pulse` already gives its own file."""
    cfg = ForgeConfig()
    path = cfg.state_dir(tmp_path)
    path.mkdir(parents=True)
    (path / "tonight.json").write_text("[1, 2, 3]", encoding="utf-8")
    assert read_tonight(tmp_path, cfg) == {}


def test_candidate_with_non_string_path_element_is_skipped_not_fatal():
    """A hand-edited or partially-corrupted pulse.json can smuggle a `null`
    into a candidate's `paths` list. `candidate_from_dict` has no reason to
    reject it (it just builds a tuple), so it reaches `zone_for` as a
    non-string element. That must not crash `decide()` and end the whole
    night — it must skip the one bad candidate and still pick a healthy one
    from the same pulse."""
    bad_pulse = {
        "generated_at": "2026-09-10T00:00:00Z",
        "sources": {},
        "candidates": [
            {"task": "corrupted", "source": "todo:docs/x.md:3", "kind": "todo",
             "paths": ["docs/x.md", None], "detail": "recent"},
            candidate_to_dict(MUSIC),
        ],
    }
    record = decide(bad_pulse, ForgeConfig())
    assert record["chosen"]["candidate"]["source"] == "ci:music-ci"
    assert record["skipped"]["outside_zone"] == 1


def test_missing_weight_is_skipped_not_fatal():
    """A user-edited forge.json can leave `config.weights` missing a key that
    `score_one` looks up unconditionally. `decide()` must not let one
    candidate's KeyError take down the whole night — it should record the
    candidate as unscoreable and keep going."""
    cfg = ForgeConfig(weights={k: v for k, v in ForgeConfig().weights.items()
                                if k != "value_todo_stale"})
    stale = Candidate(task="stale one", source="todo:docs/y.md:1", kind="todo",
                      paths=("docs/y.md",), detail="")
    record = decide(_pulse(stale), cfg)
    assert record["chosen"] is None
    assert record["skipped"]["config_error"] == 1
