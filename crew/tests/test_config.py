"""Config precedence: defaults < crew.json < CREW_MAX_* env vars."""

import json

from crew.config import CONFIG_FILENAME, effective_values, load_config


def _write(root, data):
    (root / CONFIG_FILENAME).write_text(json.dumps(data))


def test_defaults(monkeypatch, tmp_path):
    monkeypatch.delenv("CREW_MAX_FIX_ROUNDS", raising=False)
    monkeypatch.delenv("CREW_MAX_TURNS", raising=False)
    cfg = load_config(root=tmp_path)
    assert cfg.max_fix_rounds == 3
    assert cfg.max_turns == 40


def test_env_overrides(monkeypatch, tmp_path):
    monkeypatch.setenv("CREW_MAX_FIX_ROUNDS", "7")
    monkeypatch.setenv("CREW_MAX_TURNS", "99")
    cfg = load_config(root=tmp_path)
    assert cfg.max_fix_rounds == 7
    assert cfg.max_turns == 99


def test_file_sets_values(monkeypatch, tmp_path):
    monkeypatch.delenv("CREW_MAX_FIX_ROUNDS", raising=False)
    _write(tmp_path, {"max_fix_rounds": 5, "reviewer_model": "opus-custom"})
    cfg = load_config(root=tmp_path)
    assert cfg.max_fix_rounds == 5
    assert cfg.reviewer_model == "opus-custom"
    assert cfg.max_turns == 40  # untouched keys keep their defaults


def test_env_beats_file(monkeypatch, tmp_path):
    _write(tmp_path, {"max_fix_rounds": 5})
    monkeypatch.setenv("CREW_MAX_FIX_ROUNDS", "9")
    assert load_config(root=tmp_path).max_fix_rounds == 9


def test_unknown_keys_ignored(tmp_path):
    _write(tmp_path, {"max_fix_rounds": 2, "not_a_field": "nope", "state_dirname": "hax"})
    cfg = load_config(root=tmp_path)
    assert cfg.max_fix_rounds == 2
    assert cfg.state_dirname == ".crew"  # non-allowlisted field can't be set from file


def test_malformed_file_falls_back(tmp_path):
    (tmp_path / CONFIG_FILENAME).write_text("{ this is not json")
    cfg = load_config(root=tmp_path)  # must not raise
    assert cfg.max_fix_rounds == 3


def test_effective_values_shape(tmp_path):
    vals = effective_values(load_config(root=tmp_path))
    assert set(vals) == {
        "planner_model",
        "coder_model",
        "reviewer_model",
        "tester_model",
        "max_fix_rounds",
        "max_turns",
    }
