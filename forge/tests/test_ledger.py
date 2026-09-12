"""The permanent record: append-only, one JSON line per run."""

import json
import os
from datetime import datetime, timezone

import pytest

from forge.config import ForgeConfig
from forge.ledger import OUTCOMES, _entry_month, append, new_entry, read_all, recent_zones, strikes, write_atomic


def _entry(root, cfg, **kw):
    append(new_entry(kw.pop("run_id", "2026-09-10"), **kw), root, cfg)


def test_append_creates_a_month_file(tmp_path):
    cfg = ForgeConfig()
    # Pass an explicit at so the test doesn't rot. Without it, new_entry stamps at
    # with datetime.now(), and this hardcoded assertion only passes when today
    # happens to be in September — it fails every run from 1 October onward.
    path = append(new_entry("2026-09-10", at="2026-09-10T12:00:00Z", outcome="no_task"), tmp_path, cfg)
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
    expected = {"pr_opened", "pr_failed", "push_failed", "verify_failed", "crew_failed",
                "budget_exceeded", "no_task", "dry_run"}
    assert set(OUTCOMES) == expected, f"OUTCOMES {set(OUTCOMES)} != expected {expected}"

    # Assert that both subsets are valid. A member of either that isn't in
    # OUTCOMES is a silent bug that never matches anything.
    from forge.ledger import FAILURE_OUTCOMES, ACTING_OUTCOMES
    assert set(FAILURE_OUTCOMES).issubset(set(OUTCOMES)), \
        f"FAILURE_OUTCOMES {set(FAILURE_OUTCOMES)} not a subset of OUTCOMES"
    assert set(ACTING_OUTCOMES).issubset(set(OUTCOMES)), \
        f"ACTING_OUTCOMES {set(ACTING_OUTCOMES)} not a subset of OUTCOMES"


def test_pr_failed_is_acting_but_not_failure():
    """pr_failed means checks were green and the branch pushed — real,
    validated work happened in that zone, so the variety rule (ACTING_OUTCOMES)
    must count it. But the PR call failing is an environment fault (no
    GITHUB_TOKEN, GitHub down), not the candidate's fault, so it must never
    accrue a strike (FAILURE_OUTCOMES) — that would quarantine an innocent
    candidate after three bad-token nights.
    """
    from forge.ledger import FAILURE_OUTCOMES, ACTING_OUTCOMES
    assert "pr_failed" in ACTING_OUTCOMES
    assert "pr_failed" not in FAILURE_OUTCOMES


def test_pr_failed_neither_increments_nor_resets_strikes(tmp_path):
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, candidate_key="abc", outcome="verify_failed")
    _entry(tmp_path, cfg, candidate_key="abc", outcome="pr_failed")
    # Still exactly 1: pr_failed does not add a strike...
    assert strikes(tmp_path, cfg).get("abc", 0) == 1
    _entry(tmp_path, cfg, candidate_key="xyz", outcome="pr_failed")
    # ...nor does it reset one for a candidate with no prior failures.
    assert strikes(tmp_path, cfg).get("xyz", 0) == 0


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


def test_read_all_survives_invalid_utf8_in_a_month_file(tmp_path):
    """UnicodeDecodeError is a ValueError, not an OSError — a bare `except
    OSError` around path.read_text() lets one bad byte in any historical
    month file propagate uncaught out of read_all, and therefore out of
    strikes() and recent_zones() for the *entire* ledger. Pin that the
    guard covers both exception types, and that both downstream consumers
    still work over the rest of the ledger.
    """
    cfg = ForgeConfig()
    _entry(tmp_path, cfg, candidate_key="abc", outcome="verify_failed")
    _entry(tmp_path, cfg, zone="music/", outcome="pr_opened")

    bad_path = cfg.ledger_dir(tmp_path) / "2026-08.jsonl"
    bad_path.parent.mkdir(parents=True, exist_ok=True)
    bad_path.write_bytes(b"\xff\xfe")

    entries = read_all(tmp_path, cfg)
    assert len(entries) == 2, f"Expected 2 valid entries, got {len(entries)}: {entries}"

    s = strikes(tmp_path, cfg)
    assert s.get("abc", 0) == 1, f"strikes() failed: {s}"

    zones = recent_zones(tmp_path, cfg, n=3)
    assert len(zones) == 1 and zones[0] == "music/", f"recent_zones() failed: {zones}"


def test_append_files_by_the_entrys_own_at_not_wall_clock(tmp_path):
    """An entry stamped in a past month must be filed under that month, not
    under whatever month the process happens to be running in when it calls
    append(). Deterministic without freezing the clock: the `at` is a fixed
    date well in the past.
    """
    cfg = ForgeConfig()
    entry = new_entry("run-past", at="2025-03-04T12:00:00Z", outcome="no_task")
    path = append(entry, tmp_path, cfg)
    assert path.name == "2025-03.jsonl"


def test_append_falls_back_to_now_when_at_is_missing(tmp_path):
    cfg = ForgeConfig()
    entry = new_entry("run-missing-at", outcome="no_task")
    del entry["at"]
    path = append(entry, tmp_path, cfg)
    now_name = f"{datetime.now(timezone.utc):%Y-%m}.jsonl"
    assert path.name == now_name


def test_append_falls_back_to_now_when_at_is_malformed(tmp_path):
    cfg = ForgeConfig()
    entry = new_entry("run-bad-at", at="not-a-date", outcome="no_task")
    path = append(entry, tmp_path, cfg)
    now_name = f"{datetime.now(timezone.utc):%Y-%m}.jsonl"
    assert path.name == now_name


# --- write_atomic: the only supported way to rewrite a ledger file in place ---
#
# Path.write_text() truncates at open time, before writing a single byte —
# a crash partway through a rewrite turns the file into a fragment. These
# tests pin that write_atomic never allows that: the target is either the
# old content or the complete new content, never a partial mixture, and a
# failed write leaves no stray temp file behind.

def test_write_atomic_replaces_content_successfully(tmp_path):
    path = tmp_path / "2026-09.jsonl"
    path.write_text("old content\n", encoding="utf-8")
    write_atomic(path, "new content\n")
    assert path.read_text(encoding="utf-8") == "new content\n"


def test_write_atomic_leaves_original_intact_when_the_write_fails(tmp_path, monkeypatch):
    path = tmp_path / "2026-09.jsonl"
    original = '{"run_id": "a"}\n{"run_id": "b"}\n'
    path.write_text(original, encoding="utf-8")

    def failing_write(fh, content):
        # Simulate a crash partway through: some bytes reach the temp file,
        # then the process dies before flush/fsync/replace ever happen.
        fh.write(content[:5])
        raise OSError("simulated disk failure")

    monkeypatch.setattr("forge.ledger._write_and_sync", failing_write)

    with pytest.raises(OSError):
        write_atomic(path, '{"run_id": "a", "merged": true}\n{"run_id": "b"}\n')

    assert path.read_bytes() == original.encode("utf-8")


def test_write_atomic_leaves_no_temp_file_after_a_failed_write(tmp_path, monkeypatch):
    path = tmp_path / "2026-09.jsonl"
    path.write_text("original\n", encoding="utf-8")

    def failing_write(fh, content):
        fh.write(content[:3])
        raise OSError("simulated disk failure")

    monkeypatch.setattr("forge.ledger._write_and_sync", failing_write)

    with pytest.raises(OSError):
        write_atomic(path, "replacement\n")

    assert os.listdir(tmp_path) == ["2026-09.jsonl"]


def test_write_atomic_uses_a_temp_file_in_the_same_directory(tmp_path, monkeypatch):
    # os.replace is only atomic within one filesystem — the temp file must
    # be a sibling of the target, never dropped in /tmp, or a future rename
    # across filesystems silently breaks the atomicity guarantee.
    path = tmp_path / "2026-09.jsonl"
    path.write_text("original\n", encoding="utf-8")

    import tempfile as tempfile_module

    real_mkstemp = tempfile_module.mkstemp
    seen_dirs = []

    def spying_mkstemp(*args, **kwargs):
        seen_dirs.append(kwargs.get("dir"))
        return real_mkstemp(*args, **kwargs)

    monkeypatch.setattr("forge.ledger.tempfile.mkstemp", spying_mkstemp)

    write_atomic(path, "replacement\n")

    assert seen_dirs == [path.parent]
    assert path.read_text(encoding="utf-8") == "replacement\n"


def test_read_all_finds_entries_across_multiple_month_files(tmp_path):
    """The entry-derived month path must not break the multi-file glob that
    read_all relies on to see the whole ledger.

    Asserts on the actual filenames written to disk, not just a count: a
    revert to unconditional ``now()`` would collapse both entries into a
    single file and still pass a bare ``len(entries) == 2`` check, since
    both would still round-trip through read_all. That is exactly what
    happened in the earlier version of this test — confirmed empirically
    by reverting ``_entry_month`` locally and watching it still pass.
    """
    cfg = ForgeConfig()
    path_a = append(new_entry("run-a", at="2025-03-04T12:00:00Z", outcome="no_task"), tmp_path, cfg)
    path_b = append(new_entry("run-b", at="2026-09-10T00:00:00Z", outcome="pr_opened"), tmp_path, cfg)

    assert path_a.name == "2025-03.jsonl"
    assert path_b.name == "2026-09.jsonl"
    assert path_a != path_b

    ledger_dir = cfg.ledger_dir(tmp_path)
    assert sorted(p.name for p in ledger_dir.glob("*.jsonl")) == ["2025-03.jsonl", "2026-09.jsonl"]

    entries = read_all(tmp_path, cfg)
    assert [e["run_id"] for e in entries] == ["run-a", "run-b"]  # oldest first


def test_append_converts_numeric_offset_to_utc_before_taking_month(tmp_path):
    """2026-10-01T00:30:00+05:00 is 2026-09-30T19:30Z: it belongs in
    September, not October. This is the assertion that proves UTC
    conversion happens, rather than merely that the string parses.

    Deliberately not run on the actual current month (2026-09 as this test
    was written): a bug that falls back to ``now()`` would file this in
    the current month too and the test would pass for the wrong reason.
    Using a fixed offset date whose *fallback* month (now) differs from
    its *correct* UTC month (2026-12) is what makes this catch a
    regression to the old strptime-only behaviour.
    """
    cfg = ForgeConfig()
    entry = new_entry("run-offset", at="2027-01-01T00:30:00+05:00", outcome="no_task")
    path = append(entry, tmp_path, cfg)
    assert path.name == "2026-12.jsonl"


def test_append_converts_negative_offset_forward_across_month_boundary(tmp_path):
    """2026-09-30T20:00:00-05:00 is 2026-10-01T01:00:00Z: the local calendar
    reads 30 September, but UTC is 1 October, so it belongs in October.
    This tests the opposite direction from the positive-offset test above.

    The discriminator is October, not the current month (2026-09 as this
    test was written): a regression that falls back to ``now()`` would file
    this under the current month (September) and the test would still pass,
    since today happens to be in September. Expecting October rules out the
    fallback bug while the local date still reads September.
    """
    cfg = ForgeConfig()
    entry = new_entry("run-neg-offset", at="2026-09-30T20:00:00-05:00", outcome="no_task")
    path = append(entry, tmp_path, cfg)
    assert path.name == "2026-10.jsonl"


def test_append_honours_fractional_seconds(tmp_path):
    """Fixed in a month distinct from 'now' so a fallback-to-now() bug
    would file this wrong and the test would actually catch it."""
    cfg = ForgeConfig()
    entry = new_entry("run-frac", at="2026-01-15T23:59:59.999999Z", outcome="no_task")
    path = append(entry, tmp_path, cfg)
    assert path.name == "2026-01.jsonl"


def test_append_honours_naive_at_as_utc(tmp_path):
    """No zone at all: treated as already-UTC, since that is what this
    system emits. Fixed in a month distinct from 'now' for the same
    reason as the fractional-seconds test above."""
    cfg = ForgeConfig()
    entry = new_entry("run-naive", at="2026-03-15T12:00:00", outcome="no_task")
    path = append(entry, tmp_path, cfg)
    assert path.name == "2026-03.jsonl"


def test_append_never_raises_on_any_junk_at(tmp_path):
    """The never-raise property must survive the switch to fromisoformat:
    absent, None, an int, a dict, empty string, and a garbage string must
    all file under the current month without raising.

    A raw ``datetime`` object is exercised separately against
    ``_entry_month`` directly (see
    ``test_entry_month_never_raises_on_a_datetime_object``): routing it
    through ``append`` would hit ``json.dumps`` failing to serialise the
    ``at`` field itself, which is ``append``'s documented, deliberate
    behaviour for non-JSON-serialisable fields (see the module docstring)
    and has nothing to do with month derivation.
    """
    cfg = ForgeConfig()
    now_name = f"{datetime.now(timezone.utc):%Y-%m}.jsonl"

    entry_missing = new_entry("run-missing", outcome="no_task")
    del entry_missing["at"]
    assert append(entry_missing, tmp_path, cfg).name == now_name

    for bad_at, label in [
        (None, "none"),
        (12345, "int"),
        ({"not": "a date"}, "dict"),
        ("", "empty-string"),
        ("not-a-date", "garbage-string"),
    ]:
        entry = new_entry(f"run-{label}", at=bad_at, outcome="no_task")
        path = append(entry, tmp_path, cfg)
        assert path.name == now_name, f"{label} at={bad_at!r} filed under {path.name}"


def test_entry_month_never_raises_on_a_datetime_object(tmp_path):
    """A bare ``datetime`` fails the ``isinstance(at, str)`` guard and must
    fall back to the current month without raising, same as any other
    non-string junk."""
    now = datetime.now(timezone.utc)
    result = _entry_month({"at": datetime(2026, 9, 10, tzinfo=timezone.utc)})
    assert (result.year, result.month) == (now.year, now.month)
