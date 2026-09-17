"""Tests for ledger.graft — the nightly job's way of putting a live run's
ledger line back on the branch that actually holds the record.

The property that matters is append-only: whatever else graft does, it must
never reorder, edit or drop a line the ledger already had. Every test here
checks that the *existing* lines survive untouched, not just that the new
one arrived.
"""

import json
from pathlib import Path

import pytest

from forge.config import ForgeConfig
from forge.ledger import graft


def _write(p: Path, entries: list[dict]) -> None:
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text("".join(json.dumps(e, sort_keys=True) + "\n" for e in entries))


def _read(p: Path) -> list[dict]:
    return [json.loads(l) for l in p.read_text().splitlines() if l.strip()]


def _entry(at: str) -> dict:
    return {"at": at, "outcome": "dry_run", "run_id": at[:10]}


def test_adds_only_the_lines_the_ledger_lacks(tmp_path):
    cfg = ForgeConfig()
    root = tmp_path / "repo"
    src = tmp_path / "captured"

    # The real ledger: three nights.
    real = cfg.ledger_dir(root) / "2026-09.jsonl"
    _write(real, [_entry("2026-09-01T00:00:00Z"),
                  _entry("2026-09-02T00:00:00Z"),
                  _entry("2026-09-03T00:00:00Z")])

    # What a live run leaves behind on the trunk: a STALE copy (night one
    # only) plus tonight's line. This is the real shape, not a contrivance.
    _write(src / "2026-09.jsonl", [_entry("2026-09-01T00:00:00Z"),
                                   _entry("2026-09-04T00:00:00Z")])

    added = graft(src, root, cfg)
    assert added == {"2026-09.jsonl": 1}

    got = _read(real)
    assert [e["at"] for e in got] == ["2026-09-01T00:00:00Z", "2026-09-02T00:00:00Z",
                                      "2026-09-03T00:00:00Z", "2026-09-04T00:00:00Z"]


def test_running_twice_changes_nothing_the_second_time(tmp_path):
    """Idempotent: a re-run must not duplicate a night."""
    cfg = ForgeConfig()
    root = tmp_path / "repo"
    src = tmp_path / "captured"
    real = cfg.ledger_dir(root) / "2026-09.jsonl"
    _write(real, [_entry("2026-09-01T00:00:00Z")])
    _write(src / "2026-09.jsonl", [_entry("2026-09-02T00:00:00Z")])

    assert graft(src, root, cfg) == {"2026-09.jsonl": 1}
    before = real.read_text()
    assert graft(src, root, cfg) == {"2026-09.jsonl": 0}
    assert real.read_text() == before


def test_a_month_the_ledger_has_never_seen_is_created(tmp_path):
    cfg = ForgeConfig()
    root = tmp_path / "repo"
    src = tmp_path / "captured"
    _write(src / "2026-10.jsonl", [_entry("2026-10-01T00:00:00Z")])

    assert graft(src, root, cfg) == {"2026-10.jsonl": 1}
    assert _read(cfg.ledger_dir(root) / "2026-10.jsonl")[0]["at"] == "2026-10-01T00:00:00Z"


def test_a_malformed_line_raises_rather_than_being_skipped(tmp_path):
    """Skipping would drop a night's only evidence and still report success."""
    cfg = ForgeConfig()
    root = tmp_path / "repo"
    src = tmp_path / "captured"
    (src).mkdir(parents=True)
    (src / "2026-09.jsonl").write_text('{"at": "ok"}\nnot json at all\n')

    with pytest.raises(ValueError, match="not valid JSON"):
        graft(src, root, cfg)


def test_nothing_is_written_when_a_later_line_is_malformed(tmp_path):
    """The whole file is validated before a byte is appended, so a bad line
    cannot leave the ledger half-grafted."""
    cfg = ForgeConfig()
    root = tmp_path / "repo"
    src = tmp_path / "captured"
    real = cfg.ledger_dir(root) / "2026-09.jsonl"
    _write(real, [_entry("2026-09-01T00:00:00Z")])
    before = real.read_text()

    (src).mkdir(parents=True)
    (src / "2026-09.jsonl").write_text(
        json.dumps(_entry("2026-09-02T00:00:00Z"), sort_keys=True) + "\nbroken\n")

    with pytest.raises(ValueError):
        graft(src, root, cfg)
    assert real.read_text() == before


def test_a_missing_source_directory_raises(tmp_path):
    with pytest.raises(FileNotFoundError):
        graft(tmp_path / "nope", tmp_path / "repo", ForgeConfig())
