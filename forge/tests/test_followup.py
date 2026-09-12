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
    # Reshaped: the real commits-list endpoint never returns "stats" on a
    # list item (that key only exists on the single-commit endpoint), so
    # the human commit's stats must come from a per-commit fetch, not from
    # the list item itself.
    entry = new_entry("a", outcome="pr_opened", pr=1, files_touched=2)

    def fetch(path):
        if path.endswith("/pulls/1"):
            return {"merged": True, "state": "closed", "merge_commit_sha": "deadbee"}
        if path.endswith("/pulls/1/commits"):
            return {"items": [
                {"sha": "c1", "commit": {"author": {"name": "the Forge"},
                                          "message": "forge: do the task"}},
                {"sha": "c2", "commit": {"author": {"name": "Cody"},
                                          "message": "fix nit"}},
            ]}
        if path.endswith("/commits/c2"):
            return {"sha": "c2", "stats": {"total": 14}}
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


# --- the central defect: the commits-list endpoint carries no "stats", so
# --- human_edits must come from a per-commit fetch. These tests use the
# --- real API shape (commit.author.{name,email,date}, commit.message, no
# --- "stats" on the list item) so the pre-fix code — which reads
# --- commit["stats"] straight off the list item — fails them for the
# --- stated reason: it silently returns 0. ---

def test_resolve_fetches_stats_per_commit_for_realistic_commit_list_shape():
    """A commit-list item shaped exactly like GET .../pulls/{n}/commits really
    returns: no top-level "stats". Only a per-commit fetch to
    GET .../commits/{sha} supplies it. The old code read commit["stats"]
    off the list item directly and got None -> 0 every time; this must
    come back non-zero.
    """
    entry = new_entry("a", outcome="pr_opened", pr=9)
    calls = []

    def fetch(path):
        calls.append(path)
        if path.endswith("/pulls/9"):
            return {"merged": True, "state": "closed"}
        if path.endswith("/pulls/9/commits"):
            return {"items": [
                {
                    "sha": "abc123",
                    "commit": {
                        "author": {"name": "Cody Collins",
                                   "email": "cody@example.com",
                                   "date": "2026-09-10T12:00:00Z"},
                        "message": "address review comments",
                    },
                },
            ]}
        if path.endswith("/commits/abc123"):
            return {"sha": "abc123", "stats": {"total": 37}}
        return {}

    out = resolve(entry, "a/b", fetch)
    assert out["human_edits"] == 37
    assert "/repos/a/b/commits/abc123" in calls


def test_forge_commit_identified_by_message_prefix_despite_human_author_name():
    """`do._default_crew` always commits with "-m forge: {task}", so the
    message prefix is the signal this codebase actually controls. Author
    name is not: nothing sets it, so it defaults to whatever git is
    configured with locally — an ordinary human name here. The prefix must
    still mark this as the Forge's own commit, and since it's the only
    commit, no per-commit fetch should even happen.
    """
    entry = new_entry("a", outcome="pr_opened", pr=10)
    calls = []

    def fetch(path):
        calls.append(path)
        if path.endswith("/pulls/10"):
            return {"merged": True, "state": "closed"}
        if path.endswith("/pulls/10/commits"):
            return {"items": [
                {
                    "sha": "f00",
                    "commit": {
                        "author": {"name": "Cody Collins", "email": "cody@example.com"},
                        "message": "forge: fix the flaky retry test",
                    },
                },
            ]}
        raise AssertionError(f"unexpected fetch: {path}")

    out = resolve(entry, "a/b", fetch)
    assert out["human_edits"] == 0
    assert not any(c.endswith("/commits/f00") for c in calls)


def test_message_prefix_wins_over_a_coincidentally_forge_named_author():
    """The inverse case, and the one that decides which signal wins when
    they disagree: a commit whose author name happens to be in
    FORGE_AUTHORS (nothing stops a real human from being named "Claude"),
    but whose message does NOT start with "forge: ". The message is the
    reliable, codebase-controlled signal, so this must count as a human
    edit, not be excluded as the Forge's own work — author name is only a
    fallback for when the message itself is unreadable, not a veto over a
    present, informative message.
    """
    entry = new_entry("a", outcome="pr_opened", pr=11)

    def fetch(path):
        if path.endswith("/pulls/11"):
            return {"merged": True, "state": "closed"}
        if path.endswith("/pulls/11/commits"):
            return {"items": [
                {
                    "sha": "h1",
                    "commit": {
                        "author": {"name": "claude", "email": "human@example.com"},
                        "message": "Fix bug reported by QA",
                    },
                },
            ]}
        if path.endswith("/commits/h1"):
            return {"sha": "h1", "stats": {"total": 5}}
        return {}

    out = resolve(entry, "a/b", fetch)
    assert out["human_edits"] == 5


def test_unreadable_stats_for_a_non_forge_commit_yields_unknown_not_zero():
    """A wrong small number is worse than an honest unknown: if the
    per-commit fetch fails (github.api's documented failure shape is {}),
    human_edits must come back None, never silently 0.
    """
    entry = new_entry("a", outcome="pr_opened", pr=12)

    def fetch(path):
        if path.endswith("/pulls/12"):
            return {"merged": True, "state": "closed"}
        if path.endswith("/pulls/12/commits"):
            return {"items": [
                {
                    "sha": "bad1",
                    "commit": {
                        "author": {"name": "Cody"},
                        "message": "tweak the threshold",
                    },
                },
            ]}
        if path.endswith("/commits/bad1"):
            return {}  # github.api's own failure shape
        return {}

    out = resolve(entry, "a/b", fetch)
    assert out["human_edits"] is None


def test_zero_non_forge_commits_makes_no_per_commit_fetches():
    """When every commit on the PR is the Forge's own, there is nothing to
    price — and, just as importantly, nothing to fetch. Asserting on the
    recorded calls (not just the result) is what would catch a fix that
    fetches per-commit stats unconditionally.
    """
    entry = new_entry("a", outcome="pr_opened", pr=13)
    calls = []

    def fetch(path):
        calls.append(path)
        if path.endswith("/pulls/13"):
            return {"merged": True, "state": "closed"}
        if path.endswith("/pulls/13/commits"):
            return {"items": [
                {"sha": "c1", "commit": {"author": {"name": "the Forge"},
                                          "message": "forge: do the task"}},
                {"sha": "c2", "commit": {"author": {"name": "the Forge"},
                                          "message": "forge: fixup"}},
            ]}
        raise AssertionError(f"unexpected fetch: {path}")

    out = resolve(entry, "a/b", fetch)
    assert out["human_edits"] == 0
    assert calls == ["/repos/a/b/pulls/13", "/repos/a/b/pulls/13/commits"]


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


def test_backfill_survives_valid_json_lines_that_are_not_objects(tmp_path):
    """Finding 3: the ``isinstance(record, dict)`` guard in backfill keeps a
    line that is valid JSON but not an object (a bare list, string, number,
    or null — all legal JSON, none of them a ledger entry) from crashing the
    rewrite. Before this test, only the not-JSON-at-all path (a line that
    fails ``json.loads`` outright) was pinned; a regression in the
    isinstance guard itself would have gone uncaught. Every non-object line
    must survive byte-for-byte, in its original position, and no entry may
    be lost or reordered.
    """
    cfg = ForgeConfig()
    _seed(tmp_path, cfg, outcome="pr_opened", pr=1)
    _seed(tmp_path, cfg, outcome="no_task", run_id="2026-09-11")
    _seed(tmp_path, cfg, outcome="pr_opened", pr=2, run_id="2026-09-12")

    month_file = cfg.ledger_dir(tmp_path) / "2026-09.jsonl"
    lines = month_file.read_text(encoding="utf-8").splitlines()
    # Splice in valid-JSON-but-non-object lines at several positions,
    # interleaved with the real entries.
    corrupt_lines = ["[1, 2, 3]", '"a string"', "42", "null"]
    lines = [corrupt_lines[0], lines[0], corrupt_lines[1], lines[1],
             corrupt_lines[2], lines[2], corrupt_lines[3]]
    month_file.write_text("\n".join(lines) + "\n", encoding="utf-8")
    before = month_file.read_text(encoding="utf-8")

    def fetch(path):
        if path.endswith("/pulls/1"):
            return {"merged": True, "state": "closed"}
        if path.endswith("/pulls/2"):
            return {"merged": False, "state": "closed"}
        return {"items": []}

    assert backfill(tmp_path, cfg, slug="a/b", fetch=fetch) == 2

    out_text = month_file.read_text(encoding="utf-8")
    out_lines = out_text.splitlines()
    # Every non-object line survives byte-for-byte, in its original slot.
    assert out_lines[0] == "[1, 2, 3]"
    assert out_lines[2] == '"a string"'
    assert out_lines[4] == "42"
    assert out_lines[6] == "null"
    # Nothing lost: same number of lines in, same number out.
    assert len(out_lines) == len(lines)

    entries = read_all(tmp_path, cfg)
    assert [e["run_id"] for e in entries] == ["2026-09-10", "2026-09-11", "2026-09-12"]
    assert entries[0]["merged"] is True
    assert entries[1]["merged"] is None
    assert entries[2]["merged"] is False
