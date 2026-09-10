"""Config precedence: defaults < forge.json. Unknown keys are ignored."""

import json

from forge.config import (
    CONFIG_FILENAME,
    DEFAULT_WEIGHTS,
    ForgeConfig,
    load_config,
    write_starter_config,
)


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


def test_scalar_string_safe_zones_falls_back_to_default(tmp_path):
    """A typo of {"safe_zones": "docs/"} must not iterate into per-character
    zones ('d', 'o', 'c', 's', '/') — that would widen the allowlist to match
    almost every path in the repo instead of narrowing it."""
    _write(tmp_path, {"safe_zones": "docs/"})
    cfg = load_config(root=tmp_path)
    assert cfg.safe_zones == ForgeConfig().safe_zones
    assert cfg.safe_zones != tuple("docs/")


def test_scalar_string_no_touch_falls_back_to_default(tmp_path):
    _write(tmp_path, {"no_touch": "docs/"})
    cfg = load_config(root=tmp_path)
    # Falls back to the default no_touch (still welded with the hard paths),
    # never to a per-character tuple.
    assert cfg.no_touch == ForgeConfig().no_touch
    assert "d" not in cfg.no_touch


def test_bad_budget_usd_falls_back_other_keys_still_apply(tmp_path):
    _write(tmp_path, {"budget_usd": "five", "max_files_touched": 3})
    cfg = load_config(root=tmp_path)
    assert cfg.budget_usd == 5.0
    assert cfg.max_files_touched == 3


def test_bad_max_files_touched_falls_back(tmp_path):
    _write(tmp_path, {"max_files_touched": "lots"})
    cfg = load_config(root=tmp_path)
    assert cfg.max_files_touched == 12


def test_bad_weights_type_falls_back_to_default(tmp_path):
    _write(tmp_path, {"weights": ["oops"]})
    cfg = load_config(root=tmp_path)
    assert cfg.weights == DEFAULT_WEIGHTS


def test_unknown_weight_key_ignored_known_key_applied(tmp_path):
    _write(tmp_path, {"weights": {"value_ci_red": 99.0, "bogus_key": 1.0}})
    cfg = load_config(root=tmp_path)
    assert cfg.weights["value_ci_red"] == 99.0
    assert "bogus_key" not in cfg.weights


def test_bad_field_never_bypasses_hard_no_touch_weld(tmp_path):
    """Even when no_touch is malformed, the hard-welded paths must survive."""
    _write(tmp_path, {"no_touch": "docs/"})
    cfg = load_config(root=tmp_path)
    assert "forge/" in cfg.no_touch
    assert ".github/workflows/" in cfg.no_touch
