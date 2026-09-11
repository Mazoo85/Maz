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
