"""Config precedence: defaults < forge.json. Unknown keys are ignored."""

import json

from forge.config import CONFIG_FILENAME, ForgeConfig, load_config, write_starter_config


def _write(root, data):
    (root / CONFIG_FILENAME).write_text(json.dumps(data))


def test_defaults(tmp_path):
    cfg = load_config(root=tmp_path)
    assert cfg.budget_usd == 5.0
    assert cfg.max_files_touched == 12
    assert cfg.score_floor == 4.0
    assert cfg.strike_limit == 3


def test_no_touch_always_includes_forge_and_workflows(tmp_path):
    _write(tmp_path, {"no_touch": ["docs/"]})
    cfg = load_config(root=tmp_path)
    assert "forge/" in cfg.no_touch
    assert ".github/workflows/" in cfg.no_touch
    assert "docs/" in cfg.no_touch


def test_file_sets_values(tmp_path):
    _write(tmp_path, {"budget_usd": 1.5, "safe_zones": ["docs/"]})
    cfg = load_config(root=tmp_path)
    assert cfg.budget_usd == 1.5
    assert cfg.safe_zones == ("docs/",)
    assert cfg.max_files_touched == 12  # untouched keys keep defaults


def test_unknown_keys_ignored(tmp_path):
    _write(tmp_path, {"budget_usd": 2.0, "not_a_field": "nope", "state_dirname": "hax"})
    cfg = load_config(root=tmp_path)
    assert cfg.budget_usd == 2.0
    assert cfg.state_dirname == "forge/state"


def test_malformed_file_falls_back(tmp_path):
    (tmp_path / CONFIG_FILENAME).write_text("{ not json")
    assert load_config(root=tmp_path).budget_usd == 5.0


def test_state_and_ledger_dirs(tmp_path):
    cfg = ForgeConfig()
    assert cfg.state_dir(tmp_path) == tmp_path / "forge" / "state"
    assert cfg.ledger_dir(tmp_path) == tmp_path / "forge" / "ledger"


def test_write_starter_config_does_not_clobber(tmp_path):
    written, path = write_starter_config(root=tmp_path)
    assert written is True
    again, _ = write_starter_config(root=tmp_path)
    assert again is False
    assert json.loads(path.read_text())["budget_usd"] == 5.0
