"""Backfilling merged / human_edits once a human has looked at the PR."""

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
