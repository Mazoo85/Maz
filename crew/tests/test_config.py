"""Config honours env overrides for the loop bounds."""

from crew.config import load_config


def test_defaults(monkeypatch):
    monkeypatch.delenv("CREW_MAX_FIX_ROUNDS", raising=False)
    monkeypatch.delenv("CREW_MAX_TURNS", raising=False)
    cfg = load_config()
    assert cfg.max_fix_rounds == 3
    assert cfg.max_turns == 40


def test_env_overrides(monkeypatch):
    monkeypatch.setenv("CREW_MAX_FIX_ROUNDS", "7")
    monkeypatch.setenv("CREW_MAX_TURNS", "99")
    cfg = load_config()
    assert cfg.max_fix_rounds == 7
    assert cfg.max_turns == 99
