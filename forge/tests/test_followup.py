"""Backfilling merged / human_edits once a human has looked at the PR."""

import os

import pytest

from forge.config import ForgeConfig
from forge.followup import backfill, pending, resolve
from forge.ledger import append, new_entry, read_all


def _seed(root, cfg, **kw):
    append(new_entry(kw.pop("run_id", "2026-09-10"), **kw), root, cfg)


def test_pending_finds_open_questions():
    entries = [
        new_entry("a", outcome="pr_opened", pr=1),
        new_entry("b", outcome="pr_opened", pr=2, merged=True),
        new_entry("c", outcome="no_task"),
    ]
    assert [e["run_id"] for e in pending(entries)] == ["a"]


def test_resolve_records_a_merge_and_the_edit_count():
    entry = new_entry("a", outcome="pr_opened", pr=1, files_touched=2)

    def fetch(path):
        if path.endswith("/pulls/1"):
            return {"merged": True, "state": "closed", "merge_commit_sha": "deadbee"}
        if path.endswith("/pulls/1/commits"):
            return {"items": [
                {"sha": "c1", "commit": {"author": {"name": "the Forge"}}},
                {"sha": "c2", "commit": {"author": {"name": "Cody"}},
                 "stats": {"total": 14}},
            ]}
        return {}

    out = resolve(entry, "a/b", fetch)
    assert out["merged"] is True
    assert out["human_edits"] == 14


def test_resolve_records_zero_edits_when_only_the_forge_committed():
    entry = new_entry("a", outcome="pr_opened", pr=1)

    def fetch(path):
        if path.endswith("/pulls/1"):
            return {"merged": True, "state": "closed"}
        return {"items": [{"sha": "c1", "commit": {"author": {"name": "the Forge"}}}]}

    assert resolve(entry, "a/b", fetch)["human_edits"] == 0


def test_a_closed_unmerged_pr_records_merged_false():
    entry = new_entry("a", outcome="pr_opened", pr=1)
    out = resolve(entry, "a/b", lambda p: {"merged": False, "state": "closed"})
    assert out["merged"] is False
    assert out["human_edits"] == 0


def test_an_open_pr_is_left_alone():
    entry = new_entry("a", outcome="pr_opened", pr=1)
    assert resolve(entry, "a/b", lambda p: {"merged": False, "state": "open"}) is None


def test_an_unreachable_pr_is_left_alone():
    entry = new_entry("a", outcome="pr_opened", pr=1)
    assert resolve(entry, "a/b", lambda p: {}) is None


def test_backfill_rewrites_the_month_file(tmp_path):
    cfg = ForgeConfig()
    _seed(tmp_path, cfg, outcome="pr_opened", pr=1)
    _seed(tmp_path, cfg, outcome="no_task", run_id="2026-09-11")

    def fetch(path):
        if path.endswith("/pulls/1"):
            return {"merged": True, "state": "closed"}
        return {"items": []}

    assert backfill(tmp_path, cfg, slug="a/b", fetch=fetch) == 1
    entries = read_all(tmp_path, cfg)
    assert len(entries) == 2  # nothing lost
    assert entries[0]["merged"] is True
    assert entries[1]["merged"] is None


def test_backfill_without_a_slug_changes_nothing(tmp_path):
    # A ``== 0`` return is also what a totally broken backfill would give,
    # so the real assertion here is that the ledger file itself is
    # byte-for-byte untouched — not merely that the count came back zero.
    cfg = ForgeConfig()
    _seed(tmp_path, cfg, outcome="pr_opened", pr=1)
    month_file = cfg.ledger_dir(tmp_path) / "2026-09.jsonl"
    before = month_file.read_bytes()

    def fetch_that_would_prove_the_bug(_path):
        raise AssertionError("fetch must never be called without a slug")

    assert backfill(tmp_path, cfg, slug=None, fetch=fetch_that_would_prove_the_bug) == 0
    assert month_file.read_bytes() == before


# --- backfill rewrites atomically: a failed write must never truncate the
# --- ledger, which is the one artifact in this system that cannot be
# --- regenerated. ---

def _fetch_merges_pr_1(path):
    if path.endswith("/pulls/1"):
        return {"merged": True, "state": "closed"}
    return {"items": []}


def test_backfill_failure_leaves_the_month_file_byte_for_byte_intact(tmp_path, monkeypatch):
    cfg = ForgeConfig()
    _seed(tmp_path, cfg, outcome="pr_opened", pr=1)
    _seed(tmp_path, cfg, outcome="no_task", run_id="2026-09-11")

    month_file = cfg.ledger_dir(tmp_path) / "2026-09.jsonl"
    before = month_file.read_bytes()

    def failing_write(fh, content):
        # A crash partway through: a few bytes land in the temp file, then
        # the process dies before flush/fsync/replace ever happen.
        fh.write(content[:5])
        raise OSError("simulated disk failure")

    monkeypatch.setattr("forge.ledger._write_and_sync", failing_write)

    with pytest.raises(OSError):
        backfill(tmp_path, cfg, slug="a/b", fetch=_fetch_merges_pr_1)

    assert month_file.read_bytes() == before
    # The ledger must still read back every original entry, unresolved.
    entries = read_all(tmp_path, cfg)
    assert len(entries) == 2
    assert entries[0]["merged"] is None
    assert entries[1]["merged"] is None


def test_backfill_failure_leaves_no_temp_file_in_the_ledger_dir(tmp_path, monkeypatch):
    cfg = ForgeConfig()
    _seed(tmp_path, cfg, outcome="pr_opened", pr=1)
    ledger_dir = cfg.ledger_dir(tmp_path)

    def failing_write(fh, content):
        fh.write(content[:5])
        raise OSError("simulated disk failure")

    monkeypatch.setattr("forge.ledger._write_and_sync", failing_write)

    with pytest.raises(OSError):
        backfill(tmp_path, cfg, slug="a/b", fetch=_fetch_merges_pr_1)

    assert os.listdir(ledger_dir) == ["2026-09.jsonl"]


def test_backfill_temp_file_is_written_beside_the_month_file(tmp_path, monkeypatch):
    # os.replace is only atomic within one filesystem. If a future edit
    # moved the temp file to /tmp, this is the test that would catch it.
    cfg = ForgeConfig()
    _seed(tmp_path, cfg, outcome="pr_opened", pr=1)
    ledger_dir = cfg.ledger_dir(tmp_path)

    import tempfile as tempfile_module

    real_mkstemp = tempfile_module.mkstemp
    seen_dirs = []

    def spying_mkstemp(*args, **kwargs):
        seen_dirs.append(kwargs.get("dir"))
        return real_mkstemp(*args, **kwargs)

    monkeypatch.setattr("forge.ledger.tempfile.mkstemp", spying_mkstemp)

    assert backfill(tmp_path, cfg, slug="a/b", fetch=_fetch_merges_pr_1) == 1
    assert seen_dirs == [ledger_dir]


def test_backfill_success_preserves_order_and_keeps_corrupt_lines_verbatim(tmp_path):
    cfg = ForgeConfig()
    _seed(tmp_path, cfg, outcome="pr_opened", pr=1)
    _seed(tmp_path, cfg, outcome="no_task", run_id="2026-09-11")
    _seed(tmp_path, cfg, outcome="pr_opened", pr=2, run_id="2026-09-12")

    month_file = cfg.ledger_dir(tmp_path) / "2026-09.jsonl"
    lines = month_file.read_text(encoding="utf-8").splitlines()
    # Splice a corrupt (non-JSON) line in between real entries.
    lines.insert(2, "{ this is not valid json")
    month_file.write_text("\n".join(lines) + "\n", encoding="utf-8")

    def fetch(path):
        if path.endswith("/pulls/1"):
            return {"merged": True, "state": "closed"}
        if path.endswith("/pulls/2"):
            return {"merged": False, "state": "closed"}
        return {"items": []}

    assert backfill(tmp_path, cfg, slug="a/b", fetch=fetch) == 2

    out_lines = month_file.read_text(encoding="utf-8").splitlines()
    # Corrupt line kept verbatim, in its original position.
    assert out_lines[2] == "{ this is not valid json"

    entries = read_all(tmp_path, cfg)
    assert [e["run_id"] for e in entries] == ["2026-09-10", "2026-09-11", "2026-09-12"]
    assert entries[0]["merged"] is True
    assert entries[1]["merged"] is None
    assert entries[2]["merged"] is False
