"""The permanent record: append-only, one JSON line per run."""

import json

from forge.config import ForgeConfig
from forge.ledger import OUTCOMES, append, new_entry, read_all, recent_zones, strikes


def _entry(root, cfg, **kw):
    append(new_entry(kw.pop("run_id", "2026-09-10"), **kw), root, cfg)


def test_append_creates_a_month_file(tmp_path):
    cfg = ForgeConfig()
    path = append(new_entry("2026-09-10", outcome="no_task"), tmp_path, cfg)
    assert path.name == "2026-09.jsonl"
    assert json.loads(path.read_text().strip())["outcome"] == "no_task"


def test_append_is_additive(tmp_path):
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, outcome="no_task")
    _entry(tmp_path, cfg, outcome="pr_opened", run_id="2026-09-11")
    assert len(read_all(tmp_path, cfg)) == 2


def test_new_entry_has_every_field(tmp_path):
    e = new_entry("2026-09-10")
    for field in ("run_id", "chose", "source", "kind", "why", "zone", "outcome",
                  "pr", "checks", "files_touched", "cost_usd", "duration_min",
                  "notes", "candidate_key", "merged", "human_edits"):
        assert field in e


def test_outcome_set_is_closed():
    # Assert exact membership to catch typos in the closed vocabulary every
    # downstream reader depends on.
    expected = {"pr_opened", "verify_failed", "crew_failed", "budget_exceeded", "no_task", "dry_run"}
    assert set(OUTCOMES) == expected, f"OUTCOMES {set(OUTCOMES)} != expected {expected}"

    # Assert that both subsets are valid. A member of either that isn't in
    # OUTCOMES is a silent bug that never matches anything.
    from forge.ledger import FAILURE_OUTCOMES, ACTING_OUTCOMES
    assert set(FAILURE_OUTCOMES).issubset(set(OUTCOMES)), \
        f"FAILURE_OUTCOMES {set(FAILURE_OUTCOMES)} not a subset of OUTCOMES"
    assert set(ACTING_OUTCOMES).issubset(set(OUTCOMES)), \
        f"ACTING_OUTCOMES {set(ACTING_OUTCOMES)} not a subset of OUTCOMES"


def test_strikes_counts_failures_per_candidate(tmp_path):
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, candidate_key="abc", outcome="verify_failed")
    _entry(tmp_path, cfg, candidate_key="abc", outcome="crew_failed")
    _entry(tmp_path, cfg, candidate_key="xyz", outcome="verify_failed")
    s = strikes(tmp_path, cfg)
    assert s["abc"] == 2
    assert s["xyz"] == 1


def test_a_success_resets_the_strike_count(tmp_path):
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, candidate_key="abc", outcome="verify_failed")
    _entry(tmp_path, cfg, candidate_key="abc", outcome="pr_opened")
    assert strikes(tmp_path, cfg).get("abc", 0) == 0


def test_dry_runs_do_not_count_as_strikes(tmp_path):
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, candidate_key="abc", outcome="dry_run")
    assert strikes(tmp_path, cfg).get("abc", 0) == 0


def test_recent_zones_newest_first_and_skips_idle_runs(tmp_path):
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, zone="docs/", outcome="pr_opened")
    _entry(tmp_path, cfg, zone="", outcome="no_task")
    _entry(tmp_path, cfg, zone="music/", outcome="pr_opened")
    assert recent_zones(tmp_path, cfg, n=3) == ["music/", "docs/"]


def test_read_all_ignores_corrupt_lines(tmp_path):
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, outcome="no_task")
    path = cfg.ledger_dir(tmp_path) / "2026-09.jsonl"
    path.write_text(path.read_text() + "{ broken\n")
    assert len(read_all(tmp_path, cfg)) == 1


def test_read_all_on_empty_ledger(tmp_path):
    assert read_all(tmp_path, ForgeConfig()) == []


def test_read_all_skips_non_dict_json_lines(tmp_path):
    """Well-formed JSON of the wrong type (list, number, string) is skipped.

    The isinstance(record, dict) guard protects strikes() and recent_zones()
    from calling .get() on non-dict JSON, so the test exercises both consumers.
    """
    cfg = ForgeConfig()
    # Add three valid entries: one that will be counted by strikes(), one by
    # recent_zones(), one that is just valid.
    _entry(tmp_path, cfg, candidate_key="abc", outcome="verify_failed")
    _entry(tmp_path, cfg, zone="music/", outcome="pr_opened")
    _entry(tmp_path, cfg, outcome="no_task")

    # Intersperse well-formed JSON of the wrong type.
    path = cfg.ledger_dir(tmp_path) / "2026-09.jsonl"
    lines = path.read_text().splitlines()
    # Insert a JSON list after the first entry
    lines.insert(1, json.dumps([1, 2, 3]))
    # Insert a bare number after the second entry (will be at index 3 after insert)
    lines.insert(3, json.dumps(42))
    # Append a JSON string
    lines.append(json.dumps("a string"))
    path.write_text("\n".join(lines) + "\n")

    # read_all should return only the three valid entries, no raise.
    entries = read_all(tmp_path, cfg)
    assert len(entries) == 3, f"Expected 3 valid entries, got {len(entries)}: {entries}"
    assert all(isinstance(e, dict) for e in entries)

    # Both consumers should work over the same file without raising.
    s = strikes(tmp_path, cfg)
    assert s.get("abc", 0) == 1, f"strikes() failed: {s}"

    zones = recent_zones(tmp_path, cfg, n=3)
    assert len(zones) == 1 and zones[0] == "music/", f"recent_zones() failed: {zones}"
