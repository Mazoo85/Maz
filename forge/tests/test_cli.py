"""The CLI surface, exercised through Typer's runner."""

import json

from typer.testing import CliRunner

from forge.cli import app, dry_run
from forge.config import ForgeConfig
from forge.ledger import read_all

runner = CliRunner()


def _snapshot(root):
    """Map every file under root (relative path -> bytes), for before/after diffs."""
    return {
        str(p.relative_to(root)).replace("\\", "/"): p.read_bytes()
        for p in sorted(root.rglob("*"))
        if p.is_file()
    }


def test_version_flag():
    result = runner.invoke(app, ["--version"])
    assert result.exit_code == 0
    assert "forge" in result.stdout


def test_init_writes_a_starter_config(tmp_path):
    result = runner.invoke(app, ["init", "--root", str(tmp_path)])
    assert result.exit_code == 0
    written = json.loads((tmp_path / "forge.json").read_text())
    assert "safe_zones" in written


def test_init_does_not_clobber(tmp_path):
    (tmp_path / "forge.json").write_text('{"budget_usd": 1.0}')
    runner.invoke(app, ["init", "--root", str(tmp_path)])
    assert json.loads((tmp_path / "forge.json").read_text()) == {"budget_usd": 1.0}


def test_config_command_prints_the_leash(tmp_path):
    result = runner.invoke(app, ["config", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert "safe_zones" in result.stdout


def test_sense_writes_a_pulse(tmp_path):
    result = runner.invoke(app, ["sense", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert (tmp_path / "forge" / "state" / "pulse.json").exists()


def test_dry_run_writes_a_ledger_line_and_touches_nothing_else(tmp_path):
    # Seed the tree with things a careless dry run could plausibly damage:
    # a doc, the config file itself, and something at the repo root.
    (tmp_path / "docs").mkdir()
    (tmp_path / "docs" / "ROADMAP.md").write_text("# Roadmap\n\nSome docs.\n")
    (tmp_path / "forge.json").write_text(json.dumps({"budget_usd": 1.0}))
    (tmp_path / "README.md").write_text("root readme\n")

    before = _snapshot(tmp_path)
    record = dry_run(tmp_path)
    after = _snapshot(tmp_path)

    entries = read_all(tmp_path, ForgeConfig())
    assert len(entries) == 1
    assert entries[0]["outcome"] == "dry_run"
    # No branch, no PR, no source file written.
    assert entries[0]["pr"] is None
    assert record["considered"] >= 0

    # The inertness guarantee: nothing created, modified, or deleted outside
    # forge/state/ and forge/ledger/. Compare content, not just paths, so an
    # in-place rewrite of an existing file (e.g. forge.json, docs/ROADMAP.md)
    # is caught too, not just new files appearing.
    created = set(after) - set(before)
    deleted = set(before) - set(after)
    modified = {p for p in set(before) & set(after) if before[p] != after[p]}
    changed = created | deleted | modified

    allowed_prefixes = ("forge/state/", "forge/ledger/")
    offenders = {p for p in changed if not p.startswith(allowed_prefixes)}
    assert not offenders, (
        f"dry_run touched files outside forge/state/ and forge/ledger/: {sorted(offenders)}"
    )


def test_dry_run_records_the_pick_when_there_is_one(tmp_path):
    (tmp_path / "docs").mkdir()
    (tmp_path / "docs" / "notes.md").write_text("<!-- TODO: write the loot table docs -->\n")
    shared = tmp_path / "shared"
    shared.mkdir()
    (shared / "exchange.json").write_text(json.dumps({"publishes": {}, "consumes": []}))

    def fake_todo(root):
        from forge.models import Candidate
        return [Candidate(task="Write the loot table docs", source="todo:docs/notes.md:1",
                          kind="todo", paths=("docs/notes.md",), detail="recent")]

    record = dry_run(tmp_path, collectors={"todo": fake_todo})
    assert record["chosen"]["candidate"]["task"] == "Write the loot table docs"
    entry = read_all(tmp_path, ForgeConfig())[0]
    assert entry["chose"] == "Write the loot table docs"
    assert entry["zone"] == "docs/"
    assert entry["why"]["score"] > 0


def test_dry_run_respects_a_broken_exchange_declaration(tmp_path):
    """Minor finding: `forge run --dry-run` used to skip the exchange gate
    entirely — a broken shared/exchange.json would still let a candidate be
    picked, unlike `forge decide` and a live run, which both call
    `decide_step` with `exchange_ok=is_loadable(root)`. A dry run is the
    first diagnostic an operator reaches for, so it must see the same
    picture the other two do.
    """
    (tmp_path / "docs").mkdir()
    (tmp_path / "docs" / "notes.md").write_text("<!-- TODO: write the loot table docs -->\n")
    shared = tmp_path / "shared"
    shared.mkdir()
    (shared / "exchange.json").write_text("{ not json", encoding="utf-8")

    def fake_todo(root):
        from forge.models import Candidate
        return [Candidate(task="Write the loot table docs", source="todo:docs/notes.md:1",
                          kind="todo", paths=("docs/notes.md",), detail="recent")]

    record = dry_run(tmp_path, collectors={"todo": fake_todo})
    assert record["chosen"] is None
    assert record["skipped"]["config_error"] == record["considered"] == 1


def test_run_command_defaults_to_dry_run(tmp_path):
    result = runner.invoke(app, ["run", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert read_all(tmp_path, ForgeConfig())[0]["outcome"] == "dry_run"


def test_decide_command_exits_1_without_a_pulse(tmp_path):
    result = runner.invoke(app, ["decide", "--root", str(tmp_path)])
    assert result.exit_code == 1


def test_decide_command_exits_0_after_sense(tmp_path):
    sense_result = runner.invoke(app, ["sense", "--root", str(tmp_path)])
    assert sense_result.exit_code == 0

    result = runner.invoke(app, ["decide", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert (tmp_path / "forge" / "state" / "tonight.json").exists()


def test_ledger_command_reads_back(tmp_path):
    dry_run(tmp_path)
    result = runner.invoke(app, ["ledger", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert "dry_run" in result.stdout


# ---------------------------------------------------------------------------
# `forge followup` must not report three different situations identically.
#
# `merged` is the only evidence in the ledger that a night's work was actually
# good — green checks only prove nothing broke. A run that updates nothing
# because it cannot reach GitHub looks exactly like one that had nothing to do,
# and confusing them means a ledger that quietly never learns anything.
# ---------------------------------------------------------------------------


def _ledger_with(root, entries):
    """A repo whose ledger holds the given entries."""
    d = root / "forge" / "ledger"
    d.mkdir(parents=True, exist_ok=True)
    (d / "2026-09.jsonl").write_text(
        "".join(json.dumps(e) + "\n" for e in entries), encoding="utf-8"
    )
    return root


_PENDING = {
    "at": "2026-09-12T02:00:00Z", "run_id": "2026-09-12", "outcome": "pr_opened",
    "chose": "something", "kind": "roadmap", "source": "roadmap:phase-14",
    "zone": "docs/", "candidate_key": "k1", "pr": 7, "merged": None,
    "human_edits": None, "checks": "green", "files_touched": 1,
    "cost_usd": 0.0, "duration_min": 0.0, "notes": "", "why": {},
}
_SETTLED = dict(_PENDING, pr=6, candidate_key="k0", merged=True, human_edits=0)


def test_followup_without_a_github_remote_says_so_and_fails(tmp_path, monkeypatch):
    # Silently reporting "nothing to backfill" here would hide the fact that
    # the quality signal can never be collected at all.
    monkeypatch.setattr("forge.github.repo_slug", lambda root: None)
    _ledger_with(tmp_path, [_PENDING])
    result = runner.invoke(app, ["followup", "--root", str(tmp_path)])
    assert result.exit_code == 1
    assert "No GitHub remote" in result.stdout


def test_followup_distinguishes_unresolved_from_nothing_to_do(tmp_path, monkeypatch):
    monkeypatch.setattr("forge.github.repo_slug", lambda root: "owner/repo")
    monkeypatch.setattr("forge.followup.backfill", lambda *a, **k: 0)
    _ledger_with(tmp_path, [_PENDING])
    result = runner.invoke(app, ["followup", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert "still unresolved" in result.stdout
    assert "GITHUB_TOKEN" in result.stdout
    assert "Nothing to backfill" not in result.stdout


def test_followup_with_no_pending_entries_says_nothing_is_waiting(tmp_path, monkeypatch):
    monkeypatch.setattr("forge.github.repo_slug", lambda root: "owner/repo")
    monkeypatch.setattr("forge.followup.backfill", lambda *a, **k: 0)
    _ledger_with(tmp_path, [_SETTLED])
    result = runner.invoke(app, ["followup", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert "Nothing to backfill" in result.stdout
    assert "still unresolved" not in result.stdout


def test_followup_reports_what_it_updated_and_what_is_left(tmp_path, monkeypatch):
    monkeypatch.setattr("forge.github.repo_slug", lambda root: "owner/repo")
    monkeypatch.setattr("forge.followup.backfill", lambda *a, **k: 1)
    _ledger_with(tmp_path, [_PENDING, dict(_PENDING, pr=8, candidate_key="k2")])
    result = runner.invoke(app, ["followup", "--root", str(tmp_path)])
    assert result.exit_code == 0
    assert "Updated" in result.stdout and "1 ledger entry" in result.stdout
    assert "1 still open or unreachable" in result.stdout
